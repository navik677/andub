#include "anime_card.hpp"
#include "../services/image_cache.hpp"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <thread>

namespace anime::ui {

struct CardCallbackData {
    Anime anime;
    std::function<void(const Anime&)> callback;
};

struct PosterAsyncData {
    GtkWidget* picture;
    std::string path;
};

static gboolean on_poster_ready(gpointer user_data) {
    auto* data = static_cast<PosterAsyncData*>(user_data);
    if (GTK_IS_PICTURE(data->picture) && !data->path.empty()) {
        GError* err = nullptr;
        // Standard anime portrait poster ratio (188x265)
        GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(data->path.c_str(), 188, 265, FALSE, &err);
        if (pb) {
            GdkTexture* texture = gdk_texture_new_for_pixbuf(pb);
            if (texture) {
                gtk_picture_set_paintable(GTK_PICTURE(data->picture), GDK_PAINTABLE(texture));
                g_object_unref(texture);
            }
            g_object_unref(pb);
        } else {
            if (err) g_error_free(err);
            GdkTexture* texture = gdk_texture_new_from_filename(data->path.c_str(), nullptr);
            if (texture) {
                gtk_picture_set_paintable(GTK_PICTURE(data->picture), GDK_PAINTABLE(texture));
                g_object_unref(texture);
            }
        }
    }
    delete data;
    return G_SOURCE_REMOVE;
}

GtkWidget* AnimeCard::create(const Anime& anime, std::function<void(const Anime&)> on_clicked, std::function<void(const Anime&)> on_hover) {
    // Root container: vertical box (poster above, info below)
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_add_css_class(card, "anime-card");
    gtk_widget_set_size_request(card, 184, 322);
    gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(card, GTK_ALIGN_START);
    gtk_widget_set_hexpand(card, FALSE);
    gtk_widget_set_vexpand(card, FALSE);
    gtk_widget_set_cursor_from_name(card, "pointer");

    // Poster frame overlay (picture + floating badges)
    GtkWidget* poster_frame = gtk_overlay_new();
    gtk_widget_add_css_class(poster_frame, "anime-poster-frame");
    gtk_widget_set_size_request(poster_frame, 184, 258);
    gtk_widget_set_halign(poster_frame, GTK_ALIGN_CENTER);

    // Poster picture
    GtkWidget* picture = gtk_picture_new();
    gtk_widget_set_size_request(picture, 184, 258);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
    gtk_widget_set_hexpand(picture, FALSE);
    gtk_widget_set_vexpand(picture, FALSE);
    gtk_overlay_set_child(GTK_OVERLAY(poster_frame), picture);

    // Rating / Popularity badge — top-left of poster
    if (!anime.rating.empty()) {
        std::string r_str = anime.rating;
        size_t star_pos = r_str.find("★");
        if (star_pos != std::string::npos) {
            r_str.erase(star_pos, 3);
            while (!r_str.empty() && r_str.front() == ' ') r_str.erase(0, 1);
        }
        try {
            long val = std::stol(r_str);
            if (val >= 1000) {
                char buf[32];
                snprintf(buf, sizeof(buf), "★ %.1fk", val / 1000.0);
                r_str = buf;
            } else if (val > 0) {
                r_str = "★ " + std::to_string(val);
            }
        } catch (...) {
            if (!r_str.empty()) {
                r_str = "★ " + r_str;
            }
        }

        if (!r_str.empty()) {
            GtkWidget* rating_lbl = gtk_label_new(r_str.c_str());
            gtk_widget_add_css_class(rating_lbl, "poster-rating");
            gtk_widget_set_halign(rating_lbl, GTK_ALIGN_START);
            gtk_widget_set_valign(rating_lbl, GTK_ALIGN_START);
            gtk_overlay_add_overlay(GTK_OVERLAY(poster_frame), rating_lbl);
        }
    }

    // Age rating badge — top-right of poster
    if (!anime.age_rating.empty()) {
        GtkWidget* age_lbl = gtk_label_new(anime.age_rating.c_str());
        gtk_widget_add_css_class(age_lbl, "poster-age-badge");
        gtk_widget_set_halign(age_lbl, GTK_ALIGN_END);
        gtk_widget_set_valign(age_lbl, GTK_ALIGN_START);
        gtk_overlay_add_overlay(GTK_OVERLAY(poster_frame), age_lbl);
    }

    gtk_box_append(GTK_BOX(card), poster_frame);

    // Info box (Title + Meta) below poster
    GtkWidget* info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_add_css_class(info_box, "card-info-box");
    gtk_widget_set_halign(info_box, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(info_box, TRUE);

    // Title label (2 lines max, uniform height)
    GtkWidget* title_lbl = gtk_label_new(anime.title_ru.c_str());
    gtk_widget_add_css_class(title_lbl, "card-title");
    gtk_label_set_wrap(GTK_LABEL(title_lbl), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(title_lbl), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(title_lbl), 20);
    gtk_label_set_lines(GTK_LABEL(title_lbl), 2);
    gtk_label_set_ellipsize(GTK_LABEL(title_lbl), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_widget_set_size_request(title_lbl, 184, 34);
    gtk_box_append(GTK_BOX(info_box), title_lbl);

    // Meta line (year + provider)
    GtkWidget* meta_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(meta_box, "card-meta-line");

    if (anime.year > 0) {
        GtkWidget* year_lbl = gtk_label_new(std::to_string(anime.year).c_str());
        gtk_widget_add_css_class(year_lbl, "card-year-chip");
        gtk_box_append(GTK_BOX(meta_box), year_lbl);
    }

    if (!anime.provider.empty()) {
        std::string prov_name = anime.provider;
        if (prov_name == "anilibria") prov_name = "АніЛібрія";
        else if (prov_name == "animevost") prov_name = "AnimeVost";
        else if (prov_name == "shizaproject") prov_name = "Shiza Project";
        else if (prov_name == "anibaza") prov_name = "AniBaza";
        else if (prov_name == "anidub") prov_name = "AniDub";
        else if (prov_name == "anitube") prov_name = "AniTube UA";
        else if (prov_name == "dreamcast") prov_name = "Dream Cast";
        else if (prov_name == "anistar") prov_name = "AniStar";
        GtkWidget* prov_lbl = gtk_label_new(prov_name.c_str());
        gtk_widget_add_css_class(prov_lbl, "card-prov-chip");
        gtk_box_append(GTK_BOX(meta_box), prov_lbl);
    }

    gtk_box_append(GTK_BOX(info_box), meta_box);
    gtk_box_append(GTK_BOX(card), info_box);

    // Asynchronously load poster
    if (!anime.poster_url.empty()) {
        std::thread([picture_ptr = picture, url = anime.poster_url]() {
            std::string local_path = ImageCache::ensure_image(url);
            if (!local_path.empty()) {
                auto* d = new PosterAsyncData{picture_ptr, local_path};
                g_idle_add(on_poster_ready, d);
            }
        }).detach();
    }

    // Click gesture
    auto* click_gesture = gtk_gesture_click_new();
    auto* cb_data = new CardCallbackData{anime, std::move(on_clicked)};

    g_signal_connect_data(
        click_gesture, "released",
        G_CALLBACK(+[](GtkGestureClick*, int, double, double, gpointer user_data) {
            auto* d = static_cast<CardCallbackData*>(user_data);
            if (d && d->callback) {
                d->callback(d->anime);
            }
        }),
        cb_data,
        [](gpointer data, GClosure*) {
            delete static_cast<CardCallbackData*>(data);
        },
        static_cast<GConnectFlags>(0)
    );

    gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(click_gesture));

    // Motion controller for ambient hover background
    if (on_hover) {
        auto* motion_ctrl = gtk_event_controller_motion_new();
        auto* hover_data = new CardCallbackData{anime, std::move(on_hover)};

        g_signal_connect_data(
            motion_ctrl, "enter",
            G_CALLBACK(+[](GtkEventControllerMotion*, double, double, gpointer user_data) {
                auto* d = static_cast<CardCallbackData*>(user_data);
                if (d && d->callback) {
                    d->callback(d->anime);
                }
            }),
            hover_data,
            [](gpointer data, GClosure*) {
                delete static_cast<CardCallbackData*>(data);
            },
            static_cast<GConnectFlags>(0)
        );

        gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(motion_ctrl));
    }

    return card;
}

} // namespace anime::ui
