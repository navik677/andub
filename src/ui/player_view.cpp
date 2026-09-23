#include "player_view.hpp"
#include "responsive.hpp"
#include "../services/player_service.hpp"
#include "../services/history_manager.hpp"
#include "../services/upscaler.hpp"
#include "../utils/str_utils.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <locale.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <thread>
#include <atomic>

#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace anime::ui {

static void* get_proc_address_mpv(void*, const char* name) {
#ifdef _WIN32
    static HMODULE hEGL = LoadLibraryA("libEGL.dll");
    if (hEGL) {
        auto egl_gpa = (void*(*)(const char*))GetProcAddress(hEGL, "eglGetProcAddress");
        if (egl_gpa) {
            void* p = egl_gpa(name);
            if (p) return p;
        }
    }
    static HMODULE hGL = LoadLibraryA("opengl32.dll");
    if (hGL) {
        auto wgl_gpa = (void*(*)(const char*))GetProcAddress(hGL, "wglGetProcAddress");
        if (wgl_gpa) {
            void* p = wgl_gpa(name);
            if (p) return p;
        }
        void* p = (void*)GetProcAddress(hGL, name);
        if (p) return p;
    }
    return nullptr;
#else
    static void* egl_handle = dlopen("libEGL.so.1", RTLD_LAZY);
    static auto egl_gpa = egl_handle ? (void*(*)(const char*))dlsym(egl_handle, "eglGetProcAddress") : nullptr;
    if (egl_gpa) {
        void* p = egl_gpa(name);
        if (p) return p;
    }
    return dlsym(RTLD_DEFAULT, name);
#endif
}

static std::string format_time(double seconds) {
    if (seconds < 0 || std::isnan(seconds)) seconds = 0;
    int total = static_cast<int>(seconds);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;

    std::ostringstream ss;
    if (h > 0) {
        ss << std::setfill('0') << std::setw(2) << h << ":";
    }
    ss << std::setfill('0') << std::setw(2) << m << ":"
       << std::setfill('0') << std::setw(2) << s;
    return ss.str();
}

struct PlayerState {
    Anime anime;
    std::vector<Episode> episodes;
    size_t current_idx = 0;
    std::shared_ptr<BaseProvider> provider;
    std::function<void()> on_close;

    std::atomic<bool> render_queued{false};

    // GTK Widgets
    GtkWidget* root_overlay = nullptr;
    GtkWidget* gl_area = nullptr;
    GtkWidget* controls_overlay = nullptr;
    GtkWidget* top_bar = nullptr;
    GtkWidget* bottom_bar = nullptr;
    GtkWidget* title_label = nullptr;
    GtkWidget* quality_label = nullptr;
    GtkWidget* spinner = nullptr;
    GtkWidget* osd_label = nullptr;
    GtkWidget* play_btn = nullptr;
    GtkWidget* prev_btn = nullptr;
    GtkWidget* next_btn = nullptr;
    GtkWidget* seek_scale = nullptr;
    GtkWidget* time_cur_lbl = nullptr;
    GtkWidget* time_dur_lbl = nullptr;
    GtkWidget* vol_btn = nullptr;
    GtkWidget* vol_scale = nullptr;
    GtkWidget* fs_btn = nullptr;
    GtkWidget* upscale_btn = nullptr;

    // mpv
    mpv_handle* mpv = nullptr;
    mpv_render_context* mpv_gl = nullptr;
    bool is_gl_ready = false;
    bool is_seeking = false;
    double seek_target = 0;
    guint seek_debounce_id = 0;
    guint click_timer_id = 0;
    guint inhibit_cookie = 0;
    double last_mouse_x = -1.0;
    double last_mouse_y = -1.0;
    bool is_fullscreen = false;
    bool auto_advance_triggered = false;
    bool controls_hovered = false;
    guint timer_id = 0;
    guint osd_timeout_id = 0;
    guint autohide_id = 0;
    size_t upscale_idx = 0;

    std::shared_ptr<std::atomic<bool>> is_alive = std::make_shared<std::atomic<bool>>(true);

    Stream current_stream;
    std::string current_stream_url;
};

static void show_controls(PlayerState* state);

static void update_inhibit(PlayerState* state, bool should_inhibit) {
    if (!state || !state->root_overlay) return;
    GtkRoot* root = gtk_widget_get_root(state->root_overlay);
    if (!root || !GTK_IS_WINDOW(root)) return;
    GtkWindow* win = GTK_WINDOW(root);
    GtkApplication* app = GTK_APPLICATION(g_application_get_default());
    if (!app) return;

    if (should_inhibit && state->inhibit_cookie == 0) {
        state->inhibit_cookie = gtk_application_inhibit(
            app,
            win,
            GTK_APPLICATION_INHIBIT_IDLE,
            "Anime playback"
        );
    } else if (!should_inhibit && state->inhibit_cookie > 0) {
        gtk_application_uninhibit(app, state->inhibit_cookie);
        state->inhibit_cookie = 0;
    }
}

// Duration of the .player-osd opacity/scale transition in the theme CSS
static constexpr int OSD_FADE_MS = 180;

static void show_osd(PlayerState* state, const std::string& text, int duration_ms = 1200) {
    if (!state->osd_label) return;
    gtk_label_set_text(GTK_LABEL(state->osd_label), text.c_str());

    if (state->osd_timeout_id > 0) {
        g_source_remove(state->osd_timeout_id);
        state->osd_timeout_id = 0;
    }

    if (!gtk_widget_get_visible(state->osd_label)) {
        // Show it in the hidden state first, then drop the class on the next frame so the fade-in runs
        gtk_widget_add_css_class(state->osd_label, "osd-hidden");
        gtk_widget_set_visible(state->osd_label, TRUE);
        gtk_widget_add_tick_callback(state->osd_label, +[](GtkWidget* w, GdkFrameClock*, gpointer) -> gboolean {
            gtk_widget_remove_css_class(w, "osd-hidden");
            return G_SOURCE_REMOVE;
        }, nullptr, nullptr);
    } else {
        gtk_widget_remove_css_class(state->osd_label, "osd-hidden");
    }

    state->osd_timeout_id = g_timeout_add(duration_ms, +[](gpointer data) -> gboolean {
        auto* s = static_cast<PlayerState*>(data);
        if (!s || !s->osd_label) return G_SOURCE_REMOVE;
        // Fade out, then hide once the transition has finished
        gtk_widget_add_css_class(s->osd_label, "osd-hidden");
        s->osd_timeout_id = g_timeout_add(OSD_FADE_MS, +[](gpointer d) -> gboolean {
            auto* st = static_cast<PlayerState*>(d);
            if (st && st->osd_label) gtk_widget_set_visible(st->osd_label, FALSE);
            st->osd_timeout_id = 0;
            return G_SOURCE_REMOVE;
        }, s);
        return G_SOURCE_REMOVE;
    }, state);
}

static void toggle_pause(PlayerState* state) {
    if (!state->mpv) return;
    int paused = 0;
    mpv_get_property(state->mpv, "pause", MPV_FORMAT_FLAG, &paused);
    int new_paused = !paused;
    mpv_set_property(state->mpv, "pause", MPV_FORMAT_FLAG, &new_paused);

    if (state->play_btn) {
        gtk_button_set_icon_name(GTK_BUTTON(state->play_btn), new_paused ? "media-playback-start-symbolic" : "media-playback-pause-symbolic");
    }
    update_inhibit(state, !new_paused);
    show_controls(state);
    show_osd(state, new_paused ? "Пауза" : "Відтворення");
}

static void execute_seek(PlayerState* state, double target, bool is_backward) {
    if (!state || !state->mpv) return;

    std::string tgt_str = std::to_string(target);
    const char* cmd[] = {"seek", tgt_str.c_str(), "absolute", nullptr};
    mpv_command_async(state->mpv, 0, cmd);

    if (is_backward) {
        // Workaround for libopenh264 delayed DPB buffer bug across flushes:
        // Cycling the video track forces libopenh264 teardown and re-creation at seek position,
        // preventing audio from remaining desynchronized at the pre-seek timestamp.
        const char* v_off[] = {"set", "vid", "no", nullptr};
        mpv_command_async(state->mpv, 0, v_off);
        const char* v_on[] = {"set", "vid", "auto", nullptr};
        mpv_command_async(state->mpv, 0, v_on);
    }
}

static void adjust_volume(PlayerState* state, double delta) {
    if (!state || !state->mpv) return;
    double vol = 100.0;
    mpv_get_property(state->mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
    vol += delta;
    if (vol < 0.0) vol = 0.0;
    if (vol > 100.0) vol = 100.0;
    mpv_set_property(state->mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
    int unmute = 0;
    mpv_set_property(state->mpv, "mute", MPV_FORMAT_FLAG, &unmute);

    if (state->vol_scale) {
        gtk_range_set_value(GTK_RANGE(state->vol_scale), vol);
    }
    show_controls(state);
    show_osd(state, "Гучність: " + std::to_string(static_cast<int>(vol)) + "%", 1000);
}

static void toggle_mute(PlayerState* state) {
    if (!state || !state->mpv) return;
    int mute = 0;
    mpv_get_property(state->mpv, "mute", MPV_FORMAT_FLAG, &mute);
    int new_mute = !mute;
    mpv_set_property(state->mpv, "mute", MPV_FORMAT_FLAG, &new_mute);

    if (state->vol_btn) {
        gtk_button_set_icon_name(GTK_BUTTON(state->vol_btn), new_mute ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic");
    }
    show_controls(state);
    show_osd(state, new_mute ? "Звук вимкнено" : "Звук увімкнено", 1000);
}

static void seek_relative(PlayerState* state, double offset_seconds, const std::string& osd_msg = "") {
    if (!state->mpv) return;

    double base = 0;
    if (state->is_seeking) {
        base = state->seek_target;
    } else {
        if (mpv_get_property(state->mpv, "time-pos", MPV_FORMAT_DOUBLE, &base) != 0) {
            base = 0;
        }
    }

    double dur = 0;
    mpv_get_property(state->mpv, "duration", MPV_FORMAT_DOUBLE, &dur);

    double target = base + offset_seconds;
    if (target < 0.0) target = 0.0;
    if (dur > 0.0 && target > dur) target = dur;

    state->is_seeking = true;
    state->seek_target = target;

    if (state->time_cur_lbl) {
        gtk_label_set_text(GTK_LABEL(state->time_cur_lbl), format_time(target).c_str());
    }
    if (state->seek_scale) {
        gtk_range_set_value(GTK_RANGE(state->seek_scale), target);
    }

    bool is_backward = (offset_seconds < 0.0);
    execute_seek(state, target, is_backward);

    if (state->seek_debounce_id > 0) g_source_remove(state->seek_debounce_id);
    state->seek_debounce_id = g_timeout_add(400, +[](gpointer data) -> gboolean {
        auto* st = static_cast<PlayerState*>(data);
        if (st) {
            st->seek_debounce_id = 0;
            st->is_seeking = false;
        }
        return G_SOURCE_REMOVE;
    }, state);

    show_controls(state);
    if (!osd_msg.empty()) {
        show_osd(state, osd_msg);
    } else {
        std::string sign = (offset_seconds >= 0 ? "+" : "");
        show_osd(state, sign + std::to_string(static_cast<int>(offset_seconds)) + "с");
    }
}

static void toggle_fullscreen(PlayerState* state) {
    if (!state->root_overlay) return;
    GtkRoot* root = gtk_widget_get_root(state->root_overlay);
    if (!root || !GTK_IS_WINDOW(root)) return;
    GtkWindow* win = GTK_WINDOW(root);

    if (state->is_fullscreen) {
        gtk_window_unfullscreen(win);
        state->is_fullscreen = false;
        if (state->fs_btn) gtk_button_set_icon_name(GTK_BUTTON(state->fs_btn), "view-fullscreen-symbolic");
        show_osd(state, "Віконний режим");
    } else {
        gtk_window_fullscreen(win);
        state->is_fullscreen = true;
        if (state->fs_btn) gtk_button_set_icon_name(GTK_BUTTON(state->fs_btn), "view-restore-symbolic");
        show_osd(state, "Повний екран");
    }
    show_controls(state);
}

static void show_controls(PlayerState* state) {
    if (!state || !state->controls_overlay) return;

    if (state->top_bar) gtk_widget_set_visible(state->top_bar, TRUE);
    if (state->bottom_bar) gtk_widget_set_visible(state->bottom_bar, TRUE);
    gtk_widget_remove_css_class(state->controls_overlay, "controls-hidden");
    gtk_widget_set_can_target(state->controls_overlay, TRUE);
    if (state->root_overlay) gtk_widget_set_cursor_from_name(state->root_overlay, "default");

    if (state->autohide_id > 0) {
        g_source_remove(state->autohide_id);
        state->autohide_id = 0;
    }

    state->autohide_id = g_timeout_add_seconds(3, +[](gpointer data) -> gboolean {
        auto* s = static_cast<PlayerState*>(data);
        if (s && s->controls_overlay) {
            if (s->controls_hovered) {
                return G_SOURCE_CONTINUE;
            }
            int paused = 0;
            if (s->mpv) mpv_get_property(s->mpv, "pause", MPV_FORMAT_FLAG, &paused);
            if (!paused && !s->is_seeking) {
                if (s->top_bar) gtk_widget_set_visible(s->top_bar, FALSE);
                if (s->bottom_bar) gtk_widget_set_visible(s->bottom_bar, FALSE);
                gtk_widget_add_css_class(s->controls_overlay, "controls-hidden");
                gtk_widget_set_can_target(s->controls_overlay, FALSE);
                if (s->root_overlay) gtk_widget_set_cursor_from_name(s->root_overlay, "none");
            }
            s->autohide_id = 0;
        }
        return G_SOURCE_REMOVE;
    }, state);
}

static void set_upscaler(PlayerState* state, size_t idx, bool announce) {
    const auto& profiles = Upscaler::profiles();
    if (idx >= profiles.size()) idx = 0;
    const auto& profile = profiles[idx];
    state->upscale_idx = idx;

    bool ok = Upscaler::apply(state->mpv, profile);
    if (state->upscale_btn) {
        gtk_button_set_label(GTK_BUTTON(state->upscale_btn), ("Апскейл: " + profile.name).c_str());
    }
    if (announce) {
        Upscaler::save_profile(profile.id);
        show_controls(state);
        show_osd(state, ok ? "Апскейл: " + profile.name : "Шейдери апскейлу не знайдено", 1500);
    }
}

static void cycle_upscaler(PlayerState* state) {
    set_upscaler(state, (state->upscale_idx + 1) % Upscaler::profiles().size(), true);
}

static void load_episode(PlayerState* state, size_t idx);

static void on_mpv_update(void* ctx) {
    auto* s = static_cast<PlayerState*>(ctx);
    if (!s) return;
    if (s->render_queued.exchange(true)) return;

    g_idle_add_full(G_PRIORITY_HIGH_IDLE, +[](gpointer data) -> gboolean {
        auto* st = static_cast<PlayerState*>(data);
        if (st) {
            st->render_queued.store(false);
            if (st->spinner && gtk_widget_get_visible(st->spinner)) {
                gtk_spinner_stop(GTK_SPINNER(st->spinner));
                gtk_widget_set_visible(st->spinner, FALSE);
            }
            if (st->gl_area && GTK_IS_GL_AREA(st->gl_area)) {
                gtk_gl_area_queue_render(GTK_GL_AREA(st->gl_area));
            }
        }
        return G_SOURCE_REMOVE;
    }, s, nullptr);
}

static void on_gl_realize(GtkGLArea* area, gpointer user_data) {
    auto* state = static_cast<PlayerState*>(user_data);
    gtk_gl_area_make_current(area);

    mpv_opengl_init_params gl_params = {
        get_proc_address_mpv,
        nullptr
    };
    int advanced = 0; // CRITICAL: 0 prevents mpv core thread deadlock!
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_params},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    if (mpv_render_context_create(&state->mpv_gl, state->mpv, params) < 0) {
        std::cerr << "[PlayerView] Failed to create mpv render context\n";
        return;
    }

    state->is_gl_ready = true;
    mpv_render_context_set_update_callback(state->mpv_gl, on_mpv_update, state);

    load_episode(state, state->current_idx);
}

static gboolean on_gl_render(GtkGLArea* area, GdkGLContext*, gpointer user_data) {
    auto* state = static_cast<PlayerState*>(user_data);
    if (!state->mpv_gl) return FALSE;

    mpv_render_context_update(state->mpv_gl);

    int scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    int w = gtk_widget_get_width(GTK_WIDGET(area)) * scale;
    int h = gtk_widget_get_height(GTK_WIDGET(area)) * scale;

    if (w <= 0 || h <= 0) return FALSE;

    int fbo = 0;
    typedef void (*glGetIntegerv_fn)(unsigned int, int*);
    static auto fn_glGetIntegerv = (glGetIntegerv_fn)get_proc_address_mpv(nullptr, "glGetIntegerv");
    if (fn_glGetIntegerv) {
        fn_glGetIntegerv(0x8CA6 /* GL_FRAMEBUFFER_BINDING */, &fbo);
    }

    mpv_opengl_fbo mpv_fbo{fbo, w, h, 0};
    int flip_y = 1;
    int block_time = 0; // CRITICAL: NEVER block GTK main UI loop!
    mpv_render_param r_params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block_time},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    mpv_render_context_render(state->mpv_gl, r_params);
    return TRUE;
}

static void on_gl_unrealize(GtkGLArea*, gpointer user_data) {
    auto* state = static_cast<PlayerState*>(user_data);
    if (state->is_alive) *state->is_alive = false;
    update_inhibit(state, false);
    if (state->click_timer_id > 0) {
        g_source_remove(state->click_timer_id);
        state->click_timer_id = 0;
    }
    if (state->seek_debounce_id > 0) {
        g_source_remove(state->seek_debounce_id);
        state->seek_debounce_id = 0;
    }
    if (state->timer_id > 0) {
        g_source_remove(state->timer_id);
        state->timer_id = 0;
    }
    if (state->osd_timeout_id > 0) {
        g_source_remove(state->osd_timeout_id);
        state->osd_timeout_id = 0;
    }
    if (state->autohide_id > 0) {
        g_source_remove(state->autohide_id);
        state->autohide_id = 0;
    }
    if (state->mpv_gl) {
        mpv_render_context_free(state->mpv_gl);
        state->mpv_gl = nullptr;
    }
    if (state->mpv) {
        mpv_destroy(state->mpv);
        state->mpv = nullptr;
    }
    state->is_gl_ready = false;
}

static void load_episode(PlayerState* state, size_t idx) {
    if (idx >= state->episodes.size()) return;
    state->current_idx = idx;
    const auto& ep = state->episodes[idx];

    state->auto_advance_triggered = false;

    // Immediately stop playback of old episode so audio does not leak while resolving new stream
    if (state->mpv) {
        int paused = 1;
        mpv_set_property(state->mpv, "pause", MPV_FORMAT_FLAG, &paused);
        const char* stop_cmd[] = {"stop", nullptr};
        mpv_command_async(state->mpv, 0, stop_cmd);
    }

    std::string ep_title = state->anime.title_ru;
    if (!ep.number.empty()) {
        ep_title += " — Серія " + ep.number;
    }
    if (!ep.title.empty() && ep.title != ep.number) {
        ep_title += " (" + ep.title + ")";
    }

    if (state->title_label) {
        gtk_label_set_text(GTK_LABEL(state->title_label), ep_title.c_str());
    }
    if (state->quality_label) {
        gtk_label_set_text(GTK_LABEL(state->quality_label), "Завантаження...");
    }
    if (state->prev_btn) {
        gtk_widget_set_sensitive(state->prev_btn, idx > 0);
    }
    if (state->next_btn) {
        gtk_widget_set_sensitive(state->next_btn, idx + 1 < state->episodes.size());
    }
    if (state->spinner) {
        gtk_widget_set_visible(state->spinner, TRUE);
        gtk_spinner_start(GTK_SPINNER(state->spinner));
    }

    HistoryManager::mark_watched(state->anime.provider, state->anime.id, ep.number);

    std::thread([state, alive = state->is_alive, anime = state->anime, ep = ep, prov = state->provider]() {
        try {
            Stream s = prov->get_stream(anime, ep);

            g_idle_add(+[](gpointer data) -> gboolean {
                auto* tuple = static_cast<std::tuple<PlayerState*, std::shared_ptr<std::atomic<bool>>, Stream>*>(data);
                auto* s_ptr = std::get<0>(*tuple);
                auto alive_ptr = std::get<1>(*tuple);
                Stream st = std::move(std::get<2>(*tuple));
                delete tuple;

                if (!alive_ptr || !*alive_ptr || !s_ptr) return G_SOURCE_REMOVE;

                s_ptr->current_stream = std::move(st);

                const Quality* q = s_ptr->current_stream.best();
                if (!q || q->url.empty()) {
                    if (s_ptr->quality_label) {
                        gtk_label_set_text(GTK_LABEL(s_ptr->quality_label), "Помилка");
                    }
                    if (s_ptr->spinner) {
                        gtk_spinner_stop(GTK_SPINNER(s_ptr->spinner));
                        gtk_widget_set_visible(s_ptr->spinner, FALSE);
                    }
                    show_osd(s_ptr, "Не вдалося завантажити потік", 3000);
                    return G_SOURCE_REMOVE;
                }

                std::string stream_url = q->url;
                auto hls_pos = stream_url.find(":hls:manifest.m3u8");
                if (hls_pos != std::string::npos) {
                    stream_url = stream_url.substr(0, hls_pos);
                }

                s_ptr->current_stream_url = stream_url;
                if (s_ptr->quality_label) {
                    gtk_label_set_text(GTK_LABEL(s_ptr->quality_label), q->label.c_str());
                }

                if (s_ptr->mpv) {
                    std::string headers_str;
                    std::string user_agent;
                    std::string referrer_val;
                    for (const auto& [k, v] : q->headers) {
                        std::string lower_k = utils::utf8_tolower(k);
                        if (lower_k == "user-agent") {
                            user_agent = v;
                        } else if (lower_k == "referer") {
                            referrer_val = v;
                        } else {
                            if (!headers_str.empty()) headers_str += ",";
                            headers_str += k + ": " + v;
                        }
                    }

                    if (!user_agent.empty()) {
                        mpv_set_property_string(s_ptr->mpv, "user-agent", user_agent.c_str());
                    }
                    if (!referrer_val.empty()) {
                        mpv_set_property_string(s_ptr->mpv, "referrer", referrer_val.c_str());
                    }
                    if (!headers_str.empty()) {
                        mpv_set_property_string(s_ptr->mpv, "http-header-fields", headers_str.c_str());
                    } else {
                        mpv_set_property_string(s_ptr->mpv, "http-header-fields", "");
                    }

                    const char* cmd[] = {"loadfile", stream_url.c_str(), nullptr};
                    mpv_command_async(s_ptr->mpv, 0, cmd);

                    int unpause = 0;
                    mpv_set_property(s_ptr->mpv, "pause", MPV_FORMAT_FLAG, &unpause);
                    update_inhibit(s_ptr, true);

                    if (s_ptr->play_btn) {
                        gtk_button_set_icon_name(GTK_BUTTON(s_ptr->play_btn), "media-playback-pause-symbolic");
                    }
                }

                return G_SOURCE_REMOVE;
            }, new std::tuple<PlayerState*, std::shared_ptr<std::atomic<bool>>, Stream>(state, alive, std::move(s)));
        } catch (...) {}
    }).detach();
}

GtkWidget* PlayerView::create(
    const Anime& anime,
    const std::vector<Episode>& all_episodes,
    size_t initial_episode_idx,
    std::shared_ptr<BaseProvider> provider,
    std::function<void()> on_close
) {
    setlocale(LC_NUMERIC, "C");

    auto* state = new PlayerState();
    state->anime = anime;
    state->episodes = all_episodes;
    state->current_idx = initial_episode_idx;
    state->provider = provider;
    state->on_close = std::move(on_close);

    state->mpv = mpv_create();
    if (state->mpv) {
        mpv_set_option_string(state->mpv, "terminal", "no");
        mpv_set_option_string(state->mpv, "msg-level", "all=warn");
        mpv_set_option_string(state->mpv, "vo", "libmpv");
        // auto-copy (not auto-safe): zero-copy hwdec interop (vaapi-egl etc.) into our
        // GtkGLArea render context is driver-dependent and produces solid-color texture
        // corruption on some GPUs/drivers. auto-copy keeps HW-accelerated decode but always
        // copies the frame back before upload, avoiding the fragile EGL/DRM interop path.
        mpv_set_option_string(state->mpv, "hwdec", "auto-copy");
        mpv_set_option_string(state->mpv, "vd-lavc-threads", "4");
        // CRITICAL: Must be set BEFORE mpv_initialize() to prevent yt-dlp 20s timeout delay on video start
        mpv_set_option_string(state->mpv, "ytdl", "no");
        mpv_set_option_string(state->mpv, "load-scripts", "no");
        mpv_set_option_string(state->mpv, "cache", "yes");
        mpv_set_option_string(state->mpv, "cache-pause", "no");
        mpv_set_option_string(state->mpv, "cache-pause-wait", "1");
        mpv_set_option_string(state->mpv, "demuxer-readahead-secs", "60");
        mpv_set_option_string(state->mpv, "demuxer-max-bytes", "128MiB");
        mpv_set_option_string(state->mpv, "demuxer-max-back-bytes", "128MiB");
        mpv_set_option_string(state->mpv, "force-seekable", "yes");
        mpv_set_option_string(state->mpv, "hr-seek", "default");
        mpv_set_option_string(state->mpv, "hr-seek-framedrop", "yes");
        mpv_set_option_string(state->mpv, "stream-lavf-o", "reconnect=1,reconnect_streamed=1,reconnect_delay_max=5");
        mpv_set_option_string(state->mpv, "stop-screensaver", "yes");
        mpv_initialize(state->mpv);
    }

    GtkWidget* root_overlay = gtk_overlay_new();
    state->root_overlay = root_overlay;
    gtk_widget_set_vexpand(root_overlay, TRUE);
    gtk_widget_set_hexpand(root_overlay, TRUE);
    gtk_widget_add_css_class(root_overlay, "player-root");

    GtkWidget* gl_area = gtk_gl_area_new();
    state->gl_area = gl_area;
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl_area), FALSE);
    gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(gl_area), FALSE);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(gl_area), FALSE);
    gtk_widget_set_vexpand(gl_area, TRUE);
    gtk_widget_set_hexpand(gl_area, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(root_overlay), gl_area);

    g_signal_connect(gl_area, "realize", G_CALLBACK(on_gl_realize), state);
    g_signal_connect(gl_area, "render", G_CALLBACK(on_gl_render), state);
    g_signal_connect(gl_area, "unrealize", G_CALLBACK(on_gl_unrealize), state);

    // Gestures: single click pauses, double click toggles fullscreen without accidental pause
    GtkGesture* click_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), GDK_BUTTON_PRIMARY);
    g_signal_connect_data(
        click_gesture, "released",
        G_CALLBACK(+[](GtkGestureClick*, gint n_press, gdouble, gdouble, gpointer user_data) {
            auto* s = static_cast<PlayerState*>(user_data);
            if (!s) return;
            if (n_press == 1) {
                if (s->click_timer_id > 0) g_source_remove(s->click_timer_id);
                s->click_timer_id = g_timeout_add(220, +[](gpointer d) -> gboolean {
                    auto* st = static_cast<PlayerState*>(d);
                    if (st) {
                        st->click_timer_id = 0;
                        toggle_pause(st);
                    }
                    return G_SOURCE_REMOVE;
                }, s);
            } else if (n_press == 2) {
                if (s->click_timer_id > 0) {
                    g_source_remove(s->click_timer_id);
                    s->click_timer_id = 0;
                }
                toggle_fullscreen(s);
            }
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_widget_add_controller(gl_area, GTK_EVENT_CONTROLLER(click_gesture));

    // Mouse scroll on video area adjusts volume
    GtkEventController* scroll_ctrl = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect_data(
        scroll_ctrl, "scroll",
        G_CALLBACK(+[](GtkEventControllerScroll*, gdouble, gdouble dy, gpointer user_data) -> gboolean {
            auto* s = static_cast<PlayerState*>(user_data);
            if (dy < 0) {
                adjust_volume(s, 5.0);
            } else if (dy > 0) {
                adjust_volume(s, -5.0);
            }
            return TRUE;
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_widget_add_controller(gl_area, scroll_ctrl);

    // Motion controller for auto-hiding controls
    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect_data(
        motion, "motion",
        G_CALLBACK(+[](GtkEventControllerMotion*, gdouble x, gdouble y, gpointer user_data) {
            auto* s = static_cast<PlayerState*>(user_data);
            if (!s) return;
            if (std::abs(x - s->last_mouse_x) < 2.0 && std::abs(y - s->last_mouse_y) < 2.0) {
                return;
            }
            s->last_mouse_x = x;
            s->last_mouse_y = y;
            show_controls(s);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_widget_add_controller(root_overlay, motion);

    // Overlays Container
    GtkWidget* controls_overlay = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->controls_overlay = controls_overlay;
    gtk_widget_set_vexpand(controls_overlay, TRUE);
    gtk_widget_set_hexpand(controls_overlay, TRUE);
    gtk_widget_add_css_class(controls_overlay, "player-ui-overlay");

    // Top Bar
    GtkWidget* top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    state->top_bar = top_bar;
    gtk_widget_set_valign(top_bar, GTK_ALIGN_START);
    gtk_widget_set_hexpand(top_bar, TRUE);
    gtk_widget_add_css_class(top_bar, "player-bar-top");

    GtkEventController* top_hover = gtk_event_controller_motion_new();
    g_signal_connect_data(top_hover, "enter", G_CALLBACK(+[](GtkEventControllerMotion*, gdouble, gdouble, gpointer d) {
        auto* s = static_cast<PlayerState*>(d);
        if (s) s->controls_hovered = true;
    }), state, nullptr, static_cast<GConnectFlags>(0));
    g_signal_connect_data(top_hover, "leave", G_CALLBACK(+[](GtkEventControllerMotion*, gpointer d) {
        auto* s = static_cast<PlayerState*>(d);
        if (s) s->controls_hovered = false;
    }), state, nullptr, static_cast<GConnectFlags>(0));
    gtk_widget_add_controller(top_bar, top_hover);

    GtkWidget* back_btn = gtk_button_new_with_label("Назад");
    gtk_widget_add_css_class(back_btn, "player-btn");
    g_signal_connect_data(
        back_btn, "clicked",
        G_CALLBACK((+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<PlayerState*>(user_data);
            if (s->is_fullscreen) toggle_fullscreen(s);
            update_inhibit(s, false);
            if (s->mpv) {
                int paused = 1;
                mpv_set_property(s->mpv, "pause", MPV_FORMAT_FLAG, &paused);
                const char* stop_cmd[] = {"stop", nullptr};
                mpv_command(s->mpv, stop_cmd);
            }
            if (s->on_close) s->on_close();
        })),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(top_bar), back_btn);

    GtkWidget* title_lbl = gtk_label_new(anime.title_ru.c_str());
    state->title_label = title_lbl;
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(title_lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(title_lbl, "player-title");
    gtk_box_append(GTK_BOX(top_bar), title_lbl);

    GtkWidget* q_lbl = gtk_label_new("");
    state->quality_label = q_lbl;
    gtk_widget_add_css_class(q_lbl, "player-badge");
    gtk_box_append(GTK_BOX(top_bar), q_lbl);

    // Prominent "Відкрити в MPV" button
    GtkWidget* ext_btn = gtk_button_new_with_label("Відкрити в MPV");
    gtk_widget_add_css_class(ext_btn, "player-btn-mpv");
    gtk_widget_set_tooltip_text(ext_btn, "Перемкнути на зовнішнє вікно MPV");
    g_signal_connect_data(
        ext_btn, "clicked",
        G_CALLBACK((+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<PlayerState*>(user_data);
            update_inhibit(s, false);
            double cur_pos = 0;
            if (s->mpv) {
                mpv_get_property(s->mpv, "time-pos", MPV_FORMAT_DOUBLE, &cur_pos);
                int paused = 1;
                mpv_set_property(s->mpv, "pause", MPV_FORMAT_FLAG, &paused);
                const char* stop_cmd[] = {"stop", nullptr};
                mpv_command(s->mpv, stop_cmd);
            }
            const auto* q = s->current_stream.best();
            if (q && !q->url.empty()) {
                const Episode* ep_ptr = (s->current_idx < s->episodes.size() ? &s->episodes[s->current_idx] : nullptr);
                PlayerService::play_external(*q, s->anime.title_ru, ep_ptr, cur_pos);
                if (s->on_close) s->on_close();
            } else {
                Anime a = s->anime;
                Episode ep = (s->current_idx < s->episodes.size() ? s->episodes[s->current_idx] : Episode{});
                auto prov = s->provider;
                std::thread([a, ep, prov, cur_pos]() {
                    try {
                        Stream st = prov->get_stream(a, ep);
                        const Quality* q_ext = st.best();
                        if (q_ext && !q_ext->url.empty()) {
                            PlayerService::play_external(*q_ext, a.title_ru, &ep, cur_pos);
                        }
                    } catch (...) {}
                }).detach();
                if (s->on_close) s->on_close();
            }
        })),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(top_bar), ext_btn);

    gtk_box_append(GTK_BOX(controls_overlay), top_bar);

    // Center area with spinner and OSD
    GtkWidget* mid_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(mid_spacer, TRUE);
    gtk_widget_set_hexpand(mid_spacer, TRUE);
    gtk_widget_set_valign(mid_spacer, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(mid_spacer, GTK_ALIGN_CENTER);

    GtkWidget* spinner = gtk_spinner_new();
    state->spinner = spinner;
    gtk_widget_set_size_request(spinner, 42, 42);
    gtk_box_append(GTK_BOX(mid_spacer), spinner);

    GtkWidget* osd_lbl = gtk_label_new("");
    state->osd_label = osd_lbl;
    gtk_widget_add_css_class(osd_lbl, "player-osd");
    gtk_widget_set_visible(osd_lbl, FALSE);
    gtk_box_append(GTK_BOX(mid_spacer), osd_lbl);

    gtk_box_append(GTK_BOX(controls_overlay), mid_spacer);

    // Bottom Bar
    GtkWidget* bottom_bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    state->bottom_bar = bottom_bar;
    gtk_widget_set_valign(bottom_bar, GTK_ALIGN_END);
    gtk_widget_set_hexpand(bottom_bar, TRUE);
    gtk_widget_add_css_class(bottom_bar, "player-bar-bottom");

    GtkEventController* bot_hover = gtk_event_controller_motion_new();
    g_signal_connect_data(bot_hover, "enter", G_CALLBACK(+[](GtkEventControllerMotion*, gdouble, gdouble, gpointer d) {
        auto* s = static_cast<PlayerState*>(d);
        if (s) s->controls_hovered = true;
    }), state, nullptr, static_cast<GConnectFlags>(0));
    g_signal_connect_data(bot_hover, "leave", G_CALLBACK(+[](GtkEventControllerMotion*, gpointer d) {
        auto* s = static_cast<PlayerState*>(d);
        if (s) s->controls_hovered = false;
    }), state, nullptr, static_cast<GConnectFlags>(0));
    gtk_widget_add_controller(bottom_bar, bot_hover);

    // Seek row
    GtkWidget* seek_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* time_cur = gtk_label_new("00:00");
    state->time_cur_lbl = time_cur;
    gtk_widget_add_css_class(time_cur, "player-time");
    gtk_box_append(GTK_BOX(seek_row), time_cur);

    GtkWidget* seek_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    state->seek_scale = seek_scale;
    gtk_scale_set_draw_value(GTK_SCALE(seek_scale), FALSE);
    gtk_widget_set_hexpand(seek_scale, TRUE);
    gtk_widget_add_css_class(seek_scale, "player-slider");
    g_signal_connect_data(
        seek_scale, "change-value",
        G_CALLBACK(+[](GtkRange*, GtkScrollType, gdouble val, gpointer user_data) -> gboolean {
            auto* s = static_cast<PlayerState*>(user_data);
            if (!s || !s->mpv) return FALSE;

            s->is_seeking = true;
            s->seek_target = val;
            if (s->time_cur_lbl) {
                gtk_label_set_text(GTK_LABEL(s->time_cur_lbl), format_time(val).c_str());
            }

            if (s->seek_debounce_id > 0) g_source_remove(s->seek_debounce_id);
            s->seek_debounce_id = g_timeout_add(150, +[](gpointer data) -> gboolean {
                auto* st = static_cast<PlayerState*>(data);
                if (st && st->mpv) {
                    double cur = 0;
                    mpv_get_property(st->mpv, "time-pos", MPV_FORMAT_DOUBLE, &cur);
                    bool is_bwd = (st->seek_target < cur);
                    execute_seek(st, st->seek_target, is_bwd);
                }
                if (st) {
                    st->seek_debounce_id = 0;
                    st->is_seeking = false;
                }
                return G_SOURCE_REMOVE;
            }, s);

            return FALSE;
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(seek_row), seek_scale);

    GtkWidget* time_dur = gtk_label_new("00:00");
    state->time_dur_lbl = time_dur;
    gtk_widget_add_css_class(time_dur, "player-time");
    gtk_box_append(GTK_BOX(seek_row), time_dur);
    gtk_box_append(GTK_BOX(bottom_bar), seek_row);

    // Buttons row
    GtkWidget* btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget* prev_btn = gtk_button_new_from_icon_name("media-skip-backward-symbolic");
    state->prev_btn = prev_btn;
    gtk_widget_add_css_class(prev_btn, "player-btn-icon");
    gtk_widget_set_tooltip_text(prev_btn, "Попередня серія (P)");
    g_signal_connect_data(
        prev_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<PlayerState*>(user_data);
            if (s->current_idx > 0) load_episode(s, s->current_idx - 1);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), prev_btn);

    GtkWidget* rwd_btn = gtk_button_new_with_label("-10с");
    gtk_widget_add_css_class(rwd_btn, "player-btn-text");
    gtk_widget_set_tooltip_text(rwd_btn, "Назад на 10с (Вліво)");
    g_signal_connect_data(
        rwd_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) { seek_relative(static_cast<PlayerState*>(user_data), -10.0); }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), rwd_btn);

    GtkWidget* play_btn = gtk_button_new_from_icon_name("media-playback-pause-symbolic");
    state->play_btn = play_btn;
    gtk_widget_add_css_class(play_btn, "player-btn-play");
    gtk_widget_set_tooltip_text(play_btn, "Пауза / Відтворення (Пробіл)");
    g_signal_connect_data(
        play_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) { toggle_pause(static_cast<PlayerState*>(user_data)); }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), play_btn);

    GtkWidget* fwd_btn = gtk_button_new_with_label("+10с");
    gtk_widget_add_css_class(fwd_btn, "player-btn-text");
    gtk_widget_set_tooltip_text(fwd_btn, "Вперед на 10с (Вправо)");
    g_signal_connect_data(
        fwd_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) { seek_relative(static_cast<PlayerState*>(user_data), 10.0); }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), fwd_btn);

    GtkWidget* next_btn = gtk_button_new_from_icon_name("media-skip-forward-symbolic");
    state->next_btn = next_btn;
    gtk_widget_add_css_class(next_btn, "player-btn-icon");
    gtk_widget_set_tooltip_text(next_btn, "Наступна серія (N)");
    g_signal_connect_data(
        next_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<PlayerState*>(user_data);
            if (s->current_idx + 1 < s->episodes.size()) load_episode(s, s->current_idx + 1);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), next_btn);

    // Clean, minimalist skip opening button
    GtkWidget* skip_op = gtk_button_new_with_label("+85с Опенінг");
    gtk_widget_add_css_class(skip_op, "player-btn-chip");
    gtk_widget_set_tooltip_text(skip_op, "Пропустити опенінг (S)");
    g_signal_connect_data(
        skip_op, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            seek_relative(static_cast<PlayerState*>(user_data), 85.0, "Опенінг пропущено (+85с)");
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), skip_op);

    GtkWidget* upscale_btn = gtk_button_new_with_label("Апскейл: Вимк");
    state->upscale_btn = upscale_btn;
    gtk_widget_add_css_class(upscale_btn, "player-btn-chip");
    gtk_widget_set_tooltip_text(upscale_btn, "Апскейлер: Вимк → FSR → FSRCNNX → Anime4K → Anime4K HQ (Ctrl+U)");
    g_signal_connect_data(
        upscale_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) { cycle_upscaler(static_cast<PlayerState*>(user_data)); }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), upscale_btn);
    set_upscaler(state, Upscaler::index_of(Upscaler::get_saved_profile()), false);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(btn_row), spacer);

    GtkWidget* vol_btn = gtk_button_new_from_icon_name("audio-volume-high-symbolic");
    state->vol_btn = vol_btn;
    gtk_widget_add_css_class(vol_btn, "player-btn-icon");
    gtk_widget_set_tooltip_text(vol_btn, "Вимкнути / увімкнути звук (M)");
    g_signal_connect_data(
        vol_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            toggle_mute(static_cast<PlayerState*>(user_data));
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), vol_btn);

    GtkWidget* vol_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 5);
    state->vol_scale = vol_scale;
    gtk_range_set_value(GTK_RANGE(vol_scale), 100);
    gtk_scale_set_draw_value(GTK_SCALE(vol_scale), FALSE);
    gtk_widget_set_size_request(vol_scale, 80, -1);
    gtk_widget_add_css_class(vol_scale, "player-vol-slider");
    g_signal_connect_data(
        vol_scale, "value-changed",
        G_CALLBACK(+[](GtkRange* range, gpointer user_data) {
            auto* s = static_cast<PlayerState*>(user_data);
            double val = gtk_range_get_value(range);
            if (s && s->mpv) mpv_set_property(s->mpv, "volume", MPV_FORMAT_DOUBLE, &val);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), vol_scale);

    GtkWidget* fs_btn = gtk_button_new_from_icon_name("view-fullscreen-symbolic");
    state->fs_btn = fs_btn;
    gtk_widget_add_css_class(fs_btn, "player-btn-icon");
    gtk_widget_set_tooltip_text(fs_btn, "Повний екран (F)");
    g_signal_connect_data(
        fs_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) { toggle_fullscreen(static_cast<PlayerState*>(user_data)); }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(btn_row), fs_btn);

    gtk_box_append(GTK_BOX(bottom_bar), btn_row);
    gtk_box_append(GTK_BOX(controls_overlay), bottom_bar);

    gtk_overlay_add_overlay(GTK_OVERLAY(root_overlay), controls_overlay);

    // Drop secondary controls when the window is too narrow for the full button row
    on_layout_size_changed(root_overlay, [skip_op, vol_scale](LayoutSize size) {
        const bool narrow = size == LayoutSize::Narrow;
        gtk_widget_set_visible(skip_op, !narrow);
        gtk_widget_set_visible(vol_scale, !narrow);
    });

    // Periodic timer (250ms)
    state->timer_id = g_timeout_add(250, +[](gpointer data) -> gboolean {
        auto* s = static_cast<PlayerState*>(data);
        if (!s || !s->mpv) return G_SOURCE_REMOVE;

        double pos = 0;
        double dur = 0;
        mpv_get_property(s->mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos);
        mpv_get_property(s->mpv, "duration", MPV_FORMAT_DOUBLE, &dur);

        int paused = 0;
        mpv_get_property(s->mpv, "pause", MPV_FORMAT_FLAG, &paused);
        update_inhibit(s, !paused);

        if (s->play_btn) {
            gtk_button_set_icon_name(GTK_BUTTON(s->play_btn), paused ? "media-playback-start-symbolic" : "media-playback-pause-symbolic");
        }

        // Keep volume button icon synced
        if (s->vol_btn) {
            int mute = 0;
            mpv_get_property(s->mpv, "mute", MPV_FORMAT_FLAG, &mute);
            double cur_vol = 100.0;
            mpv_get_property(s->mpv, "volume", MPV_FORMAT_DOUBLE, &cur_vol);
            if (mute || cur_vol <= 0.0) {
                gtk_button_set_icon_name(GTK_BUTTON(s->vol_btn), "audio-volume-muted-symbolic");
            } else if (cur_vol < 35.0) {
                gtk_button_set_icon_name(GTK_BUTTON(s->vol_btn), "audio-volume-low-symbolic");
            } else if (cur_vol < 70.0) {
                gtk_button_set_icon_name(GTK_BUTTON(s->vol_btn), "audio-volume-medium-symbolic");
            } else {
                gtk_button_set_icon_name(GTK_BUTTON(s->vol_btn), "audio-volume-high-symbolic");
            }
        }

        if (!s->is_seeking && dur > 0) {
            if (s->spinner && gtk_widget_get_visible(s->spinner)) {
                gtk_spinner_stop(GTK_SPINNER(s->spinner));
                gtk_widget_set_visible(s->spinner, FALSE);
            }
            gtk_range_set_range(GTK_RANGE(s->seek_scale), 0, dur);
            gtk_range_set_value(GTK_RANGE(s->seek_scale), pos);

            if (s->time_cur_lbl) gtk_label_set_text(GTK_LABEL(s->time_cur_lbl), format_time(pos).c_str());
            if (s->time_dur_lbl) gtk_label_set_text(GTK_LABEL(s->time_dur_lbl), format_time(dur).c_str());

            int eof = 0;
            mpv_get_property(s->mpv, "eof-reached", MPV_FORMAT_FLAG, &eof);
            if ((eof || (dur > 30 && pos >= dur - 0.5)) && !s->auto_advance_triggered) {
                if (s->current_idx + 1 < s->episodes.size()) {
                    s->auto_advance_triggered = true;
                    show_osd(s, "Наступна серія...", 2000);
                    load_episode(s, s->current_idx + 1);
                }
            }
        }
        return G_SOURCE_CONTINUE;
    }, state);

    // Keyboard Shortcuts
    GtkEventController* key_ctrl = gtk_event_controller_key_new();
    g_signal_connect_data(
        key_ctrl, "key-pressed",
        G_CALLBACK((+[](GtkEventControllerKey*, guint keyval, guint, GdkModifierType mods, gpointer user_data) -> gboolean {
            auto* s = static_cast<PlayerState*>(user_data);
            // Ctrl+U; Cyrillic ghe is what the U key sends in Ukrainian/Russian layouts
            if ((mods & GDK_CONTROL_MASK) &&
                (keyval == GDK_KEY_u || keyval == GDK_KEY_U ||
                 keyval == GDK_KEY_Cyrillic_ghe || keyval == GDK_KEY_Cyrillic_GHE)) {
                cycle_upscaler(s);
                return TRUE;
            }
            switch (keyval) {
                case GDK_KEY_space:
                    toggle_pause(s);
                    return TRUE;
                case GDK_KEY_Left:
                    seek_relative(s, -10.0);
                    return TRUE;
                case GDK_KEY_Right:
                    seek_relative(s, 10.0);
                    return TRUE;
                case GDK_KEY_Up:
                    adjust_volume(s, 5.0);
                    return TRUE;
                case GDK_KEY_Down:
                    adjust_volume(s, -5.0);
                    return TRUE;
                case GDK_KEY_m:
                case GDK_KEY_M:
                case GDK_KEY_Cyrillic_softsign:
                case GDK_KEY_Cyrillic_SOFTSIGN:
                    toggle_mute(s);
                    return TRUE;
                case GDK_KEY_s:
                case GDK_KEY_S:
                case GDK_KEY_Cyrillic_es:
                case GDK_KEY_Cyrillic_ES:
                    seek_relative(s, 85.0, "Опенінг пропущено (+85с)");
                    return TRUE;
                case GDK_KEY_n:
                case GDK_KEY_N:
                    if (s->current_idx + 1 < s->episodes.size()) load_episode(s, s->current_idx + 1);
                    return TRUE;
                case GDK_KEY_p:
                case GDK_KEY_P:
                    if (s->current_idx > 0) load_episode(s, s->current_idx - 1);
                    return TRUE;
                case GDK_KEY_f:
                case GDK_KEY_F:
                case GDK_KEY_F11:
                    toggle_fullscreen(s);
                    return TRUE;
                case GDK_KEY_Escape:
                    if (s->is_fullscreen) {
                        toggle_fullscreen(s);
                    } else if (s->on_close) {
                        update_inhibit(s, false);
                        if (s->mpv) {
                            int paused = 1;
                            mpv_set_property(s->mpv, "pause", MPV_FORMAT_FLAG, &paused);
                            const char* stop_cmd[] = {"stop", nullptr};
                            mpv_command(s->mpv, stop_cmd);
                        }
                        s->on_close();
                    }
                    return TRUE;
                default:
                    break;
            }
            return FALSE;
        })),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_event_controller_set_propagation_phase(key_ctrl, GTK_PHASE_CAPTURE);
    gtk_widget_set_focusable(root_overlay, TRUE);
    gtk_widget_add_controller(root_overlay, key_ctrl);
    g_signal_connect(root_overlay, "map", G_CALLBACK(+[](GtkWidget* w, gpointer) {
        gtk_widget_grab_focus(w);
    }), nullptr);

    // Initial controls timer
    show_controls(state);

    g_signal_connect_data(
        root_overlay, "destroy",
        G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
            delete static_cast<PlayerState*>(user_data);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    return root_overlay;
}

} // namespace anime::ui
