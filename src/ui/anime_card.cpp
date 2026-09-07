#include "anime_card.hpp"
#include "../services/image_cache.hpp"
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
        gtk_picture_set_filename(GTK_PICTURE(data->picture), data->path.c_str());
    }
    delete data;
    return G_SOURCE_REMOVE;
}

GtkWidget* AnimeCard::create(const Anime& anime, std::function<void(const Anime&)> on_clicked) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card, "anime-card");
    gtk_widget_set_size_request(card, 210, 360);
    gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(card, GTK_ALIGN_START);
    gtk_widget_set_hexpand(card, FALSE);
    gtk_widget_set_vexpand(card, FALSE);
    gtk_widget_set_cursor_from_name(card, "pointer");

    // Poster (Portrait aspect ratio ~2:3)
    GtkWidget* picture = gtk_picture_new();
    gtk_widget_set_size_request(picture, 194, 275);
    gtk_widget_set_halign(picture, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(picture, FALSE);
    gtk_widget_set_vexpand(picture, FALSE);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), FALSE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
    gtk_box_append(GTK_BOX(card), picture);

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

    // Title label
    GtkWidget* title_lbl = gtk_label_new(anime.title_ru.c_str());
    gtk_widget_add_css_class(title_lbl, "card-title");
    gtk_label_set_wrap(GTK_LABEL(title_lbl), TRUE);
    gtk_label_set_lines(GTK_LABEL(title_lbl), 2);
    gtk_label_set_ellipsize(GTK_LABEL(title_lbl), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(card), title_lbl);

    // Meta box (Year + Rating)
    GtkWidget* meta_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    if (anime.year > 0) {
        GtkWidget* year_lbl = gtk_label_new(std::to_string(anime.year).c_str());
        gtk_widget_add_css_class(year_lbl, "card-meta");
        gtk_box_append(GTK_BOX(meta_box), year_lbl);
    }

    if (!anime.rating.empty()) {
        GtkWidget* rating_lbl = gtk_label_new(anime.rating.c_str());
        gtk_widget_add_css_class(rating_lbl, "rating-badge");
        gtk_box_append(GTK_BOX(meta_box), rating_lbl);
    }

    gtk_box_append(GTK_BOX(card), meta_box);

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

    return card;
}

} // namespace anime::ui
