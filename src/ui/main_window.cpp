#include "main_window.hpp"
#include "anime_card.hpp"
#include "details_view.hpp"
#include "player_view.hpp"
#include "downloads_view.hpp"
#include "responsive.hpp"
#include "../providers/anilibria_provider.hpp"
#include "../providers/shizaproject_provider.hpp"
#include "../providers/anibaza_provider.hpp"
#include "../providers/anidub_provider.hpp"
#include "../providers/anitube_provider.hpp"
#include "../providers/animevost_provider.hpp"
#include "../providers/anistar_provider.hpp"
#include "../providers/dreamcast_provider.hpp"
#include "../services/favorites_manager.hpp"
#include "../services/player_service.hpp"
#include "../services/theme_manager.hpp"
#include "../services/image_cache.hpp"
#include "../utils/str_utils.hpp"
#include <thread>
#include <fstream>
#include <vector>
#include <string>

namespace anime::ui {

static const std::vector<std::string> RUSSIAN_GENRES = {
    "Все жанры",
    "Боевые искусства",
    "Вампиры",
    "Гарем",
    "Демоны",
    "Детектив",
    "Дзёсей",
    "Драма",
    "Игры",
    "Исекай",
    "Исторический",
    "Киберпанк",
    "Комедия",
    "Магия",
    "Меха",
    "Мистика",
    "Музыка",
    "Пародия",
    "Повседневность",
    "Приключения",
    "Психологическое",
    "Романтика",
    "Сверхъестественное",
    "Сейнен",
    "Спорт",
    "Супер сила",
    "Сёдзе",
    "Сёдзе-ай",
    "Сёнен",
    "Триллер",
    "Ужасы",
    "Фантастика",
    "Фэнтези",
    "Школа",
    "Экшен",
    "Этти",
    "Хентай"
};

struct AppState {
    GtkWindow* window = nullptr;
    GtkWidget* stack = nullptr;
    GtkWidget* flow_box = nullptr;
    GtkWidget* scrolled_window = nullptr;
    GtkWidget* status_box = nullptr;
    GtkWidget* spinner = nullptr;
    GtkWidget* status_label = nullptr;
    GtkWidget* search_entry = nullptr;
    GtkWidget* genre_drop = nullptr;
    GtkWidget* details_container = nullptr;
    GtkWidget* player_container = nullptr;
    GtkWidget* pagination_box = nullptr;
    GtkWidget* header = nullptr;

    GtkWidget* home_btn = nullptr;
    GtkWidget* fav_btn = nullptr;
    GtkWidget* favorites_bar = nullptr; // GtkRevealer wrapping the banner
    GtkWidget* empty_fav_btn = nullptr;

    std::vector<std::shared_ptr<BaseProvider>> providers;
    size_t current_provider_idx = 0;
    bool is_favorites_mode = false;

    guint search_debounce_id = 0;
    std::string current_query;
    std::string current_genre;
    int current_page = 1;
    uint64_t search_sequence = 0;

    // Ambient backdrop for catalog view
    GtkWidget* catalog_overlay = nullptr;
    GtkWidget* backdrop_stack = nullptr;
    GtkWidget* backdrop_pic_1 = nullptr;
    GtkWidget* backdrop_pic_2 = nullptr;
    int current_backdrop_idx = 0;
    std::string current_backdrop_url;
    guint hover_debounce_id = 0;
};

struct SearchResultData {
    AppState* state = nullptr;
    std::vector<Anime> items;
    std::string query;
    std::string genre;
    int page = 1;
    uint64_t seq = 0;
};

// Wraps a header section so its size doesn't set the window's minimum width.
// The responsive layout collapses the section before it would be clipped.
static GtkWidget* shrinkable(GtkWidget* child) {
    GtkWidget* sw = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(sw), TRUE);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(sw), TRUE);
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(sw), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), child);
    gtk_widget_add_css_class(sw, "header-section");
    return sw;
}

static void do_search(AppState* state);
static void update_nav_ui(AppState* state);
static void switch_to_home(AppState* state);
static void switch_to_favorites(AppState* state);

struct BgUpdateData {
    AppState* state;
    std::string blur_path;
    std::string url;
};

static void update_ambient_backdrop(AppState* state, const std::string& poster_url) {
    if (poster_url.empty() || !state || !state->backdrop_stack) return;
    state->current_backdrop_url = poster_url;

    std::thread([state, url = poster_url]() {
        std::string blur_path = ImageCache::ensure_blurred_backdrop(url);
        if (blur_path.empty()) return;

        auto* d = new BgUpdateData{state, blur_path, url};
        g_idle_add(+[](gpointer p_data) -> gboolean {
            auto* data = static_cast<BgUpdateData*>(p_data);
            AppState* s = data->state;
            if (s && s->backdrop_stack && data->url == s->current_backdrop_url) {
                GError* err = nullptr;
                GdkTexture* texture = gdk_texture_new_from_filename(data->blur_path.c_str(), &err);
                if (texture) {
                    int next_idx = (s->current_backdrop_idx == 0) ? 1 : 0;
                    GtkWidget* next_pic = (next_idx == 0) ? s->backdrop_pic_1 : s->backdrop_pic_2;
                    const char* child_name = (next_idx == 0) ? "bg1" : "bg2";

                    gtk_picture_set_paintable(GTK_PICTURE(next_pic), GDK_PAINTABLE(texture));
                    gtk_stack_set_visible_child_name(GTK_STACK(s->backdrop_stack), child_name);
                    s->current_backdrop_idx = next_idx;
                    g_object_unref(texture);
                } else if (err) {
                    g_error_free(err);
                }
            }
            delete data;
            return G_SOURCE_REMOVE;
        }, d);
    }).detach();
}

struct HoverDebounceData {
    AppState* state;
    std::string poster_url;
};

static void on_card_hovered(AppState* state, const Anime& anime) {
    if (anime.poster_url.empty() || anime.poster_url == state->current_backdrop_url) return;

    if (state->hover_debounce_id > 0) {
        g_source_remove(state->hover_debounce_id);
        state->hover_debounce_id = 0;
    }

    auto* d = new HoverDebounceData{state, anime.poster_url};
    state->hover_debounce_id = g_timeout_add(85, +[](gpointer p_data) -> gboolean {
        auto* data = static_cast<HoverDebounceData*>(p_data);
        data->state->hover_debounce_id = 0;
        update_ambient_backdrop(data->state, data->poster_url);
        delete data;
        return G_SOURCE_REMOVE;
    }, d);
}

static void update_pagination_ui(AppState* state, bool has_results, bool has_more) {
    if (!state->pagination_box) return;

    GtkWidget* child = gtk_widget_get_first_child(state->pagination_box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(state->pagination_box), child);
        child = next;
    }

    int cur = state->current_page;
    if (cur < 1) cur = 1;

    if (state->is_favorites_mode || (!has_results && cur == 1)) {
        gtk_widget_set_visible(state->pagination_box, FALSE);
        return;
    }

    gtk_widget_set_visible(state->pagination_box, TRUE);

    // « Prev button
    GtkWidget* prev_btn = gtk_button_new_with_label("«");
    gtk_widget_add_css_class(prev_btn, "page-btn");
    gtk_widget_set_size_request(prev_btn, 36, 36);
    gtk_widget_set_sensitive(prev_btn, cur > 1);
    g_signal_connect_data(
        prev_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            if (s->current_page > 1) {
                s->current_page--;
                do_search(s);
                if (s->scrolled_window) {
                    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(s->scrolled_window));
                    if (vadj) gtk_adjustment_set_value(vadj, 0.0);
                }
            }
        }),
        state, nullptr, static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(state->pagination_box), prev_btn);

    // Numbered buttons (sliding window of 5 pages centered around current)
    int start_p = std::max(1, cur - 2);
    int end_p = start_p + 4;

    for (int p = start_p; p <= end_p; ++p) {
        GtkWidget* p_btn = gtk_button_new_with_label(std::to_string(p).c_str());
        gtk_widget_add_css_class(p_btn, "page-btn");
        if (p == cur) {
            gtk_widget_add_css_class(p_btn, "active");
        }
        gtk_widget_set_size_request(p_btn, 36, 36);
        g_object_set_data(G_OBJECT(p_btn), "target_page", GINT_TO_POINTER(p));

        g_signal_connect_data(
            p_btn, "clicked",
            G_CALLBACK(+[](GtkButton* btn, gpointer user_data) {
                auto* s = static_cast<AppState*>(user_data);
                int target = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "target_page"));
                if (target > 0 && target != s->current_page) {
                    s->current_page = target;
                    do_search(s);
                    if (s->scrolled_window) {
                        GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(s->scrolled_window));
                        if (vadj) gtk_adjustment_set_value(vadj, 0.0);
                    }
                }
            }),
            state, nullptr, static_cast<GConnectFlags>(0)
        );
        gtk_box_append(GTK_BOX(state->pagination_box), p_btn);
    }

    // » Next button
    GtkWidget* next_btn = gtk_button_new_with_label("»");
    gtk_widget_add_css_class(next_btn, "page-btn");
    gtk_widget_set_size_request(next_btn, 36, 36);
    gtk_widget_set_sensitive(next_btn, has_more);
    g_signal_connect_data(
        next_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            s->current_page++;
            do_search(s);
            if (s->scrolled_window) {
                GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(s->scrolled_window));
                if (vadj) gtk_adjustment_set_value(vadj, 0.0);
            }
        }),
        state, nullptr, static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(state->pagination_box), next_btn);
}

static gboolean on_search_results_ready(gpointer user_data) {
    auto* data = static_cast<SearchResultData*>(user_data);
    AppState* state = data->state;

    // Discard stale search requests
    if (data->seq != state->search_sequence) {
        delete data;
        return G_SOURCE_REMOVE;
    }

    if (GTK_IS_SPINNER(state->spinner)) {
        gtk_spinner_stop(GTK_SPINNER(state->spinner));
        gtk_widget_set_visible(state->spinner, FALSE);
    }
    gtk_widget_remove_css_class(state->flow_box, "refreshing");

    // Clear flow box
    GtkWidget* child = gtk_widget_get_first_child(state->flow_box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_flow_box_remove(GTK_FLOW_BOX(state->flow_box), child);
        child = next;
    }

    if (data->items.empty()) {
        gtk_widget_set_visible(state->status_box, TRUE);
        gtk_widget_set_visible(state->status_label, TRUE);
        std::string empty_msg = "Нічого не знайдено.";
        if (state->is_favorites_mode) {
            if (!data->query.empty()) {
                empty_msg = "В улюблених нічого не знайдено за запитом \"" + data->query + "\"";
            } else if (!data->genre.empty()) {
                empty_msg = "В улюблених нічого не знайдено за жанром \"" + data->genre + "\"";
            } else {
                empty_msg = "У вас поки немає збережених улюблених аніме.\nДодайте тайтли до улюблених на сторінці опису!";
            }
            if (state->empty_fav_btn) {
                gtk_widget_set_visible(state->empty_fav_btn, TRUE);
            }
        } else {
            if (state->empty_fav_btn) {
                gtk_widget_set_visible(state->empty_fav_btn, FALSE);
            }
            if (!data->genre.empty() && !data->query.empty()) {
                empty_msg = "Нічого не знайдено за запитом \"" + data->query + "\" та жанром \"" + data->genre + "\"";
            } else if (!data->genre.empty()) {
                empty_msg = "Нічого не знайдено за жанром: \"" + data->genre + "\"";
            }
        }
        gtk_label_set_text(GTK_LABEL(state->status_label), empty_msg.c_str());
    } else {
        gtk_widget_set_visible(state->status_box, FALSE);
        gtk_widget_set_visible(state->status_label, FALSE);
        if (state->empty_fav_btn) {
            gtk_widget_set_visible(state->empty_fav_btn, FALSE);
        }
        for (size_t i = 0; i < data->items.size(); ++i) {
            const auto& item = data->items[i];
            GtkWidget* card = AnimeCard::create(
                item,
                [state](const Anime& anime) {
                    // Open details view
                    std::shared_ptr<BaseProvider> prov = state->providers[state->current_provider_idx];
                    for (const auto& p : state->providers) {
                        if (p->name() == anime.provider) {
                            prov = p;
                            break;
                        }
                    }

                    // Remove previous details view child
                    GtkWidget* old_child = gtk_widget_get_first_child(state->details_container);
                    if (old_child) gtk_box_remove(GTK_BOX(state->details_container), old_child);

                    GtkWidget* details = DetailsView::create(
                        anime,
                        prov,
                        [state]() {
                            gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "catalog");
                        },
                        [state](const Anime& a, const std::vector<Episode>& eps, size_t ep_idx, std::shared_ptr<BaseProvider> p) {
                            GtkWidget* old_player = gtk_widget_get_first_child(state->player_container);
                            if (old_player) gtk_box_remove(GTK_BOX(state->player_container), old_player);

                            GtkWidget* player = PlayerView::create(
                                a,
                                eps,
                                ep_idx,
                                p,
                                [state]() {
                                    if (state->header) gtk_widget_set_visible(state->header, TRUE);
                                    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "details");
                                    g_idle_add(+[](gpointer p_data) -> gboolean {
                                        auto* c = static_cast<GtkWidget*>(p_data);
                                        if (c && GTK_IS_BOX(c)) {
                                            GtkWidget* ch = gtk_widget_get_first_child(c);
                                            if (ch) gtk_box_remove(GTK_BOX(c), ch);
                                        }
                                        return G_SOURCE_REMOVE;
                                    }, state->player_container);
                                }
                            );
                            gtk_box_append(GTK_BOX(state->player_container), player);
                            if (state->header) gtk_widget_set_visible(state->header, FALSE);
                            gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "player");
                        }
                    );
                    gtk_box_append(GTK_BOX(state->details_container), details);
                    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "details");
                },
                [state](const Anime& anime) {
                    on_card_hovered(state, anime);
                }
            );

            // Staggered rise-in as the new page appears
            gtk_widget_add_css_class(card, "enter");
            gtk_widget_add_css_class(card, ThemeManager::stagger_class(i).c_str());

            gtk_flow_box_append(GTK_FLOW_BOX(state->flow_box), card);
            GtkWidget* child_widget = gtk_widget_get_parent(card);
            if (child_widget) {
                gtk_widget_set_halign(child_widget, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(child_widget, GTK_ALIGN_START);
                gtk_widget_set_focusable(child_widget, FALSE);
            }
        }

        if (state->current_backdrop_url.empty() && !data->items.empty()) {
            update_ambient_backdrop(state, data->items[0].poster_url);
        }
    }

    bool has_more = data->items.size() >= 30;
    update_pagination_ui(state, !data->items.empty(), has_more);

    delete data;
    return G_SOURCE_REMOVE;
}

static void do_search(AppState* state) {
    if (state->status_box) {
        gtk_widget_set_visible(state->status_box, TRUE);
    }
    if (GTK_IS_SPINNER(state->spinner)) {
        gtk_widget_set_visible(state->spinner, TRUE);
        gtk_spinner_start(GTK_SPINNER(state->spinner));
    }
    gtk_widget_set_visible(state->status_label, FALSE);
    if (state->empty_fav_btn) gtk_widget_set_visible(state->empty_fav_btn, FALSE);
    // Dim the current results until the new ones replace them
    gtk_widget_add_css_class(state->flow_box, "refreshing");

    std::string q = state->current_query;
    std::string genre = state->current_genre;
    bool favs = state->is_favorites_mode;
    auto prov = state->providers[state->current_provider_idx];

    // Extract "жанр:..." or "genre:..." prefix if user typed it into search bar
    std::string query_text = q;
    std::string query_genre = genre;
    {
        std::string lower_q = utils::utf8_tolower(q);
        size_t pos = lower_q.find("жанр:");
        size_t pfx_len = 0;
        if (pos != std::string::npos) {
            // "жанр:" is 9 bytes in UTF-8
            pfx_len = std::string("жанр:").length();
        } else {
            pos = lower_q.find("genre:");
            if (pos != std::string::npos) pfx_len = 6;
        }

        if (pos != std::string::npos) {
            size_t val_start = pos + pfx_len;
            while (val_start < q.size() && (q[val_start] == ' ' || q[val_start] == '\t')) val_start++;
            size_t val_end = val_start;
            while (val_end < q.size() && q[val_end] != ' ' && q[val_end] != '\t') val_end++;
            if (val_end > val_start) {
                query_genre = q.substr(val_start, val_end - val_start);
                query_text = q.substr(0, pos) + q.substr(val_end);
                while (!query_text.empty() && std::isspace(static_cast<unsigned char>(query_text.front()))) query_text.erase(query_text.begin());
                while (!query_text.empty() && std::isspace(static_cast<unsigned char>(query_text.back()))) query_text.pop_back();
            }
        }
    }

    uint64_t seq = ++state->search_sequence;
    int page = state->current_page;
    std::thread([state, query_text, query_genre, favs, prov, page, seq]() {
        std::vector<Anime> results;
        if (favs) {
            auto all_favs = FavoritesManager::get_favorites();
            for (const auto& a : all_favs) {
                bool matches_query = query_text.empty() ||
                    utils::utf8_contains_ci(a.title_ru, query_text) ||
                    utils::utf8_contains_ci(a.title_en, query_text);

                bool matches_genre = query_genre.empty();
                if (!matches_genre) {
                    for (const auto& g : a.genres) {
                        if (utils::utf8_contains_ci(g, query_genre)) {
                            matches_genre = true;
                            break;
                        }
                    }
                }

                if (matches_query && matches_genre) {
                    results.push_back(a);
                }
            }
        } else {
            results = prov->search(query_text, 30, query_genre, page);
        }

        auto* res_data = new SearchResultData{state, std::move(results), query_text, query_genre, page, seq};
        g_idle_add(on_search_results_ready, res_data);
    }).detach();
}

static void update_nav_ui(AppState* state) {
    if (!state) return;
    if (state->home_btn && state->fav_btn) {
        if (state->is_favorites_mode) {
            gtk_widget_remove_css_class(state->home_btn, "active");
            gtk_widget_add_css_class(state->fav_btn, "active");
        } else {
            gtk_widget_add_css_class(state->home_btn, "active");
            gtk_widget_remove_css_class(state->fav_btn, "active");
        }
    }
    if (state->favorites_bar) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(state->favorites_bar), state->is_favorites_mode);
    }
}

static void switch_to_home(AppState* state) {
    if (!state) return;

    if (state->stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "catalog");
    }
    if (state->header) {
        gtk_widget_set_visible(state->header, TRUE);
    }

    state->is_favorites_mode = false;
    state->current_page = 1;
    update_nav_ui(state);

    if (state->search_debounce_id > 0) {
        g_source_remove(state->search_debounce_id);
        state->search_debounce_id = 0;
    }

    if (state->search_entry) {
        const char* text = gtk_editable_get_text(GTK_EDITABLE(state->search_entry));
        if (text && strlen(text) > 0) {
            gtk_editable_set_text(GTK_EDITABLE(state->search_entry), "");
            return;
        }
    }

    do_search(state);
}

static void switch_to_favorites(AppState* state) {
    if (!state) return;

    if (state->stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "catalog");
    }
    if (state->header) {
        gtk_widget_set_visible(state->header, TRUE);
    }

    state->is_favorites_mode = true;
    state->current_page = 1;
    update_nav_ui(state);

    if (state->search_debounce_id > 0) {
        g_source_remove(state->search_debounce_id);
        state->search_debounce_id = 0;
    }

    do_search(state);
}

static gboolean on_debounce_timeout(gpointer user_data) {
    auto* state = static_cast<AppState*>(user_data);
    state->search_debounce_id = 0;
    do_search(state);
    return G_SOURCE_REMOVE;
}

GtkWidget* MainWindow::create(GtkApplication* app) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Andub");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 850);
    if (g_getenv("ANIME_MAXIMIZE")) {
        gtk_window_maximize(GTK_WINDOW(window));
    }

    auto* state = new AppState();
    state->window = GTK_WINDOW(window);
    state->providers = {
        std::make_shared<AnilibriaProvider>(),
        std::make_shared<DreamCastProvider>(),
        std::make_shared<ShizaProjectProvider>(),
        std::make_shared<AniBazaProvider>(),
        std::make_shared<AniDubProvider>(),
        std::make_shared<AniTubeProvider>(),
        std::make_shared<AnimeVostProvider>(),
        std::make_shared<AniStarProvider>()
    };

    // HeaderBar
    GtkWidget* header = gtk_header_bar_new();
    state->header = header;
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);

    // Left container: Brand logo, Provider switch, Russian Genre dropdown, Home & Favorites buttons
    GtkWidget* left_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    // Brand Logo (Clickable Home link)
    GtkWidget* brand_btn = gtk_button_new_with_label("ANDUB");
    gtk_widget_add_css_class(brand_btn, "brand-logo-btn");
    gtk_widget_set_tooltip_text(brand_btn, "На головну сторінку");
    gtk_widget_set_cursor_from_name(brand_btn, "pointer");
    g_signal_connect_data(
        brand_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            switch_to_home(static_cast<AppState*>(user_data));
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(left_box), brand_btn);

    std::vector<std::string> prov_name_strings;
    for (const auto& p : state->providers) {
        prov_name_strings.push_back(p->display_name());
    }
    std::vector<const char*> provider_names;
    for (const auto& name_str : prov_name_strings) {
        provider_names.push_back(name_str.c_str());
    }
    provider_names.push_back(nullptr);
    GtkWidget* prov_drop = gtk_drop_down_new_from_strings(provider_names.data());
    gtk_widget_add_css_class(prov_drop, "header-dropdown");
    gtk_box_append(GTK_BOX(left_box), prov_drop);

    std::vector<const char*> genre_cstrs;
    for (const auto& g : RUSSIAN_GENRES) {
        genre_cstrs.push_back(g.c_str());
    }
    genre_cstrs.push_back(nullptr);
    GtkWidget* genre_drop = gtk_drop_down_new_from_strings(genre_cstrs.data());
    gtk_widget_add_css_class(genre_drop, "header-dropdown");
    state->genre_drop = genre_drop;
    gtk_box_append(GTK_BOX(left_box), genre_drop);

    // Home Button
    GtkWidget* home_btn = gtk_button_new_with_label("Головна");
    gtk_widget_add_css_class(home_btn, "nav-tab-btn");
    gtk_widget_add_css_class(home_btn, "active");
    gtk_widget_set_tooltip_text(home_btn, "Головний каталог аніме");
    gtk_widget_set_cursor_from_name(home_btn, "pointer");
    state->home_btn = home_btn;
    g_signal_connect_data(
        home_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            switch_to_home(static_cast<AppState*>(user_data));
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(left_box), home_btn);

    // Favorites Button
    GtkWidget* fav_btn = gtk_button_new_with_label("Улюблені");
    gtk_widget_add_css_class(fav_btn, "nav-tab-btn");
    gtk_widget_set_tooltip_text(fav_btn, "Збережені улюблені аніме");
    gtk_widget_set_cursor_from_name(fav_btn, "pointer");
    state->fav_btn = fav_btn;
    g_signal_connect_data(
        fav_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            if (s->is_favorites_mode) {
                switch_to_home(s);
            } else {
                switch_to_favorites(s);
            }
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(left_box), fav_btn);

    // Check saved provider from config
    std::string saved_prov;
    try {
        std::string cfg_path = ThemeManager::get_config_file();
        if (std::filesystem::exists(cfg_path)) {
            std::ifstream f(cfg_path);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            auto root = json::Value::parse(content);
            saved_prov = root["provider"].get_str();
        }
    } catch (...) {}

    if (!saved_prov.empty()) {
        for (guint i = 0; i < state->providers.size(); ++i) {
            if (state->providers[i]->name() == saved_prov) {
                state->current_provider_idx = i;
                gtk_drop_down_set_selected(GTK_DROP_DOWN(prov_drop), i);
                break;
            }
        }
    }

    g_signal_connect_data(
        prov_drop, "notify::selected",
        G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            s->current_provider_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
            s->current_page = 1;

            // Save selected provider
            try {
                std::string cfg_path = ThemeManager::get_config_file();
                json::Value root;
                if (std::filesystem::exists(cfg_path)) {
                    std::ifstream f(cfg_path);
                    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    root = json::Value::parse(content);
                }
                root["provider"] = s->providers[s->current_provider_idx]->name();
                std::ofstream out(cfg_path);
                out << root.dump(2);
            } catch (...) {}

            if (!s->is_favorites_mode) {
                do_search(s);
            }
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    g_signal_connect_data(
        genre_drop, "notify::selected",
        G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
            if (sel == 0 || sel >= RUSSIAN_GENRES.size()) {
                s->current_genre.clear();
            } else {
                s->current_genre = RUSSIAN_GENRES[sel];
            }
            s->current_page = 1;
            do_search(s);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), shrinkable(left_box));

    // Search Entry
    GtkWidget* search_entry = gtk_search_entry_new();
    gtk_widget_add_css_class(search_entry, "header-search");
    gtk_widget_set_size_request(search_entry, 240, -1);
    gtk_search_entry_set_key_capture_widget(GTK_SEARCH_ENTRY(search_entry), window);
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search_entry), "Пошук аніме...");
    state->search_entry = search_entry;
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), search_entry);

    g_signal_connect_data(
        search_entry, "search-changed",
        G_CALLBACK(+[](GtkSearchEntry* entry, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            s->current_query = gtk_editable_get_text(GTK_EDITABLE(entry));
            s->current_page = 1;
            if (s->search_debounce_id > 0) {
                g_source_remove(s->search_debounce_id);
            }
            s->search_debounce_id = g_timeout_add(350, on_debounce_timeout, s);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    // Right container: Theme selector & Downloads button
    GtkWidget* right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    const auto& themes = ThemeManager::get_themes();
    std::vector<const char*> theme_labels;
    for (const auto& th : themes) {
        theme_labels.push_back(th.name.c_str());
    }
    theme_labels.push_back(nullptr);

    GtkWidget* theme_drop = gtk_drop_down_new_from_strings(theme_labels.data());
    gtk_widget_add_css_class(theme_drop, "header-dropdown");
    std::string curr_th = ThemeManager::get_current_theme_id();
    for (guint i = 0; i < themes.size(); ++i) {
        if (themes[i].id == curr_th) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(theme_drop), i);
            break;
        }
    }

    g_signal_connect(
        theme_drop, "notify::selected",
        G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer) {
            guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
            const auto& ths = ThemeManager::get_themes();
            if (sel < ths.size()) {
                ThemeManager::apply_theme(ths[sel].id);
            }
        }),
        nullptr
    );
    gtk_box_append(GTK_BOX(right_box), theme_drop);

    GtkWidget* dl_btn = gtk_button_new_with_label("Завантаження");
    gtk_widget_add_css_class(dl_btn, "header-action-btn");
    g_signal_connect_data(
        dl_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            DownloadsView::show_dialog(s->window);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(right_box), dl_btn);

    GtkWidget* player_btn = gtk_button_new();
    gtk_widget_add_css_class(player_btn, "header-action-btn");
    bool is_emb = (PlayerService::get_player_mode() == PlayerMode::Embedded);
    gtk_button_set_label(GTK_BUTTON(player_btn), is_emb ? "Плеєр: Вбудований" : "Плеєр: MPV");
    gtk_widget_set_tooltip_text(player_btn, is_emb ? "Вбудований плеєр (клікніть щоб обрати MPV)" : "Зовнішній MPV (клікніть щоб обрати вбудований)");
    g_signal_connect_data(
        player_btn, "clicked",
        G_CALLBACK(+[](GtkButton* btn, gpointer) {
            PlayerMode cur = PlayerService::get_player_mode();
            PlayerMode next = (cur == PlayerMode::Embedded ? PlayerMode::ExternalMpv : PlayerMode::Embedded);
            PlayerService::set_player_mode(next);
            bool emb = (next == PlayerMode::Embedded);
            gtk_button_set_label(btn, emb ? "Плеєр: Вбудований" : "Плеєр: MPV");
            gtk_widget_set_tooltip_text(GTK_WIDGET(btn), emb ? "Вбудований плеєр (клікніть щоб обрати MPV)" : "Зовнішній MPV (клікніть щоб обрати вбудований)");
        }),
        nullptr,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(right_box), player_btn);

    // Overflow menu: holds secondary header controls when the window is too narrow
    GtkWidget* more_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(more_box, 6);
    gtk_widget_set_margin_end(more_box, 6);
    gtk_widget_set_margin_top(more_box, 6);
    gtk_widget_set_margin_bottom(more_box, 6);
    GtkWidget* more_popover = gtk_popover_new();
    gtk_popover_set_child(GTK_POPOVER(more_popover), more_box);

    GtkWidget* more_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(more_btn), "open-menu-symbolic");
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(more_btn), more_popover);
    gtk_widget_set_tooltip_text(more_btn, "Більше");
    gtk_widget_set_visible(more_btn, FALSE);
    gtk_box_prepend(GTK_BOX(right_box), more_btn);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), shrinkable(right_box));

    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    // Main Stack
    GtkWidget* stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 260);
    state->stack = stack;

    // --- Catalog View Overlay (Backdrop + Scrim + Content) ---
    GtkWidget* catalog_overlay = gtk_overlay_new();
    state->catalog_overlay = catalog_overlay;
    gtk_widget_add_css_class(catalog_overlay, "catalog-overlay");

    // 1. Ambient Backdrop Stack with crossfade transition
    GtkWidget* backdrop_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(backdrop_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(backdrop_stack), 420);
    gtk_widget_set_can_target(backdrop_stack, FALSE);
    state->backdrop_stack = backdrop_stack;

    GtkWidget* bg1 = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(bg1), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(bg1), GTK_CONTENT_FIT_COVER);
    state->backdrop_pic_1 = bg1;
    gtk_stack_add_named(GTK_STACK(backdrop_stack), bg1, "bg1");

    GtkWidget* bg2 = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(bg2), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(bg2), GTK_CONTENT_FIT_COVER);
    state->backdrop_pic_2 = bg2;
    gtk_stack_add_named(GTK_STACK(backdrop_stack), bg2, "bg2");

    gtk_overlay_set_child(GTK_OVERLAY(catalog_overlay), backdrop_stack);

    // 2. Ambient Scrim / Tint Layer to keep high contrast & readability
    GtkWidget* scrim = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(scrim, "catalog-backdrop-scrim");
    gtk_widget_set_can_target(scrim, FALSE);
    gtk_widget_set_hexpand(scrim, TRUE);
    gtk_widget_set_vexpand(scrim, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(catalog_overlay), scrim);

    // 3. Foreground Catalog Box
    GtkWidget* catalog_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(catalog_box, "catalog-content-box");
    gtk_widget_set_hexpand(catalog_box, TRUE);
    gtk_widget_set_vexpand(catalog_box, TRUE);

    // Favorites banner bar (only shown in favorites mode)
    GtkWidget* fav_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(fav_bar, "favorites-banner");
    gtk_widget_set_margin_start(fav_bar, 40);
    gtk_widget_set_margin_end(fav_bar, 40);
    gtk_widget_set_margin_top(fav_bar, 14);
    gtk_widget_set_margin_bottom(fav_bar, 4);

    GtkWidget* fav_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(fav_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(fav_revealer), 240);
    gtk_revealer_set_child(GTK_REVEALER(fav_revealer), fav_bar);
    state->favorites_bar = fav_revealer;

    GtkWidget* fav_title = gtk_label_new("⭐ Улюблені аніме");
    gtk_widget_add_css_class(fav_title, "favorites-banner-title");
    gtk_box_append(GTK_BOX(fav_bar), fav_title);

    GtkWidget* fav_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(fav_spacer, TRUE);
    gtk_box_append(GTK_BOX(fav_bar), fav_spacer);

    GtkWidget* fav_back_btn = gtk_button_new_with_label("← На головну");
    gtk_widget_add_css_class(fav_back_btn, "fav-exit-btn");
    gtk_widget_set_tooltip_text(fav_back_btn, "Повернутися до головного каталогу");
    gtk_widget_set_cursor_from_name(fav_back_btn, "pointer");
    g_signal_connect_data(
        fav_back_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            switch_to_home(static_cast<AppState*>(user_data));
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(fav_bar), fav_back_btn);

    gtk_box_append(GTK_BOX(catalog_box), fav_revealer);

    // Spinner and Status
    GtkWidget* status_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(status_box, 32);
    gtk_widget_set_margin_bottom(status_box, 16);
    state->status_box = status_box;

    GtkWidget* spinner = gtk_spinner_new();
    state->spinner = spinner;
    gtk_box_append(GTK_BOX(status_box), spinner);

    GtkWidget* status_lbl = gtk_label_new("");
    gtk_widget_add_css_class(status_lbl, "card-meta");
    gtk_label_set_justify(GTK_LABEL(status_lbl), GTK_JUSTIFY_CENTER);
    state->status_label = status_lbl;
    gtk_box_append(GTK_BOX(status_box), status_lbl);

    GtkWidget* empty_fav_btn = gtk_button_new_with_label("← Перейти до каталогу");
    gtk_widget_add_css_class(empty_fav_btn, "fav-empty-btn");
    gtk_widget_set_halign(empty_fav_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_cursor_from_name(empty_fav_btn, "pointer");
    gtk_widget_set_visible(empty_fav_btn, FALSE);
    g_signal_connect_data(
        empty_fav_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            switch_to_home(static_cast<AppState*>(user_data));
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(status_box), empty_fav_btn);
    state->empty_fav_btn = empty_fav_btn;

    gtk_box_append(GTK_BOX(catalog_box), status_box);

    // Scrolled window for Grid
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(scrolled, "catalog-scrolled");
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    state->scrolled_window = scrolled;

    GtkWidget* flow = gtk_flow_box_new();
    gtk_widget_add_css_class(flow, "catalog-grid");
    gtk_widget_set_valign(flow, GTK_ALIGN_START);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 10);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 24);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 28);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_widget_set_margin_start(flow, 40);
    gtk_widget_set_margin_end(flow, 40);
    gtk_widget_set_margin_top(flow, 20);
    gtk_widget_set_margin_bottom(flow, 20);
    state->flow_box = flow;

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), flow);
    gtk_box_append(GTK_BOX(catalog_box), scrolled);

    // Bottom Pagination Bar
    GtkWidget* pagination_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(pagination_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(pagination_box, 16);
    gtk_widget_set_margin_bottom(pagination_box, 22);
    state->pagination_box = pagination_box;
    gtk_box_append(GTK_BOX(catalog_box), pagination_box);

    gtk_overlay_add_overlay(GTK_OVERLAY(catalog_overlay), catalog_box);

    gtk_stack_add_named(GTK_STACK(stack), catalog_overlay, "catalog");

    // --- Details Container ---
    GtkWidget* details_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->details_container = details_container;
    gtk_stack_add_named(GTK_STACK(stack), details_container, "details");

    // --- Player Container ---
    GtkWidget* player_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->player_container = player_container;
    gtk_stack_add_named(GTK_STACK(stack), player_container, "player");

    gtk_window_set_child(GTK_WINDOW(window), stack);

    // Responsive layout: collapse header controls and tighten spacing as the window narrows
    struct HeaderLayout {
        GtkWidget *window, *brand, *prov, *genre, *home, *fav, *search;
        GtkWidget *left_box, *right_box, *more_btn, *more_box;
        GtkWidget *theme, *downloads, *player;
        GtkWidget *flow, *fav_bar;
    };
    auto* hl = new HeaderLayout{
        window, brand_btn, prov_drop, genre_drop, home_btn, fav_btn, search_entry,
        left_box, right_box, more_btn, more_box,
        theme_drop, dl_btn, player_btn,
        flow, fav_bar
    };
    g_object_set_data_full(G_OBJECT(window), "anime-header-layout", hl,
                           [](gpointer d) { delete static_cast<HeaderLayout*>(d); });

    on_layout_size_changed(window, [hl](LayoutSize size) {
        const bool wide = size == LayoutSize::Wide;
        const bool narrow = size == LayoutSize::Narrow;

        auto place = [](GtkWidget* w, GtkWidget* box, GtkWidget* after) {
            g_object_ref(w);
            if (GtkWidget* parent = gtk_widget_get_parent(w)) gtk_box_remove(GTK_BOX(parent), w);
            gtk_box_insert_child_after(GTK_BOX(box), w, after);
            g_object_unref(w);
        };

        // Genre filter stays in the bar unless the window is narrow
        if (narrow) place(hl->genre, hl->more_box, nullptr);
        else place(hl->genre, hl->left_box, hl->prov);

        // Theme / downloads / player mode move into the overflow menu below the wide breakpoint
        GtkWidget* secondary_box = wide ? hl->right_box : hl->more_box;
        GtkWidget* anchor = wide ? hl->more_btn : (narrow ? hl->genre : nullptr);
        place(hl->theme, secondary_box, anchor);
        place(hl->downloads, secondary_box, hl->theme);
        place(hl->player, secondary_box, hl->downloads);
        gtk_widget_set_visible(hl->more_btn, !wide);

        gtk_widget_set_visible(hl->brand, !narrow);
        if (narrow) {
            gtk_button_set_icon_name(GTK_BUTTON(hl->home), "go-home-symbolic");
            gtk_button_set_icon_name(GTK_BUTTON(hl->fav), "starred-symbolic");
        } else {
            gtk_button_set_label(GTK_BUTTON(hl->home), "Головна");
            gtk_button_set_label(GTK_BUTTON(hl->fav), "Улюблені");
        }
        gtk_widget_set_size_request(hl->search, wide ? 240 : (narrow ? 140 : 200), -1);

        // Catalog grid spacing
        const int margin = wide ? 40 : (narrow ? 12 : 24);
        gtk_widget_set_margin_start(hl->flow, margin);
        gtk_widget_set_margin_end(hl->flow, margin);
        gtk_widget_set_margin_start(hl->fav_bar, margin);
        gtk_widget_set_margin_end(hl->fav_bar, margin);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(hl->flow), wide ? 24 : (narrow ? 12 : 18));
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(hl->flow), wide ? 28 : (narrow ? 16 : 22));

        if (narrow) gtk_widget_add_css_class(hl->window, "layout-narrow");
        else gtk_widget_remove_css_class(hl->window, "layout-narrow");
        if (!wide) gtk_widget_add_css_class(hl->window, "layout-compact");
        else gtk_widget_remove_css_class(hl->window, "layout-compact");
    });

    // Cleanup on destroy
    g_signal_connect_data(
        window, "destroy",
        G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            if (s->search_debounce_id > 0) g_source_remove(s->search_debounce_id);
            if (s->hover_debounce_id > 0) g_source_remove(s->hover_debounce_id);
            delete s;
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    // Initial search or favorites mode
    if (g_getenv("ANIME_START_FAVORITES")) {
        switch_to_favorites(state);
    } else {
        do_search(state);
    }

    return window;
}

} // namespace anime::ui
