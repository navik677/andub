#include "details_view.hpp"
#include "../services/image_cache.hpp"
#include "../services/player_service.hpp"
#include "../services/history_manager.hpp"
#include "../services/favorites_manager.hpp"
#include "../services/download_service.hpp"
#include <thread>
#include <vector>

namespace anime::ui {

struct DetailsContext {
    Anime anime;
    std::shared_ptr<BaseProvider> provider;
    GtkWidget* episodes_box;
    GtkWidget* spinner;
    GtkWidget* progress_lbl;
    GtkWidget* fav_btn;
};

struct EpisodesLoadedData {
    DetailsContext* ctx;
    std::vector<Episode> episodes;
};

static void populate_episodes(DetailsContext* ctx, const std::vector<Episode>& episodes) {
    if (!ctx || !GTK_IS_BOX(ctx->episodes_box)) return;

    auto watched = HistoryManager::get_watched_episodes(ctx->anime.provider, ctx->anime.id);
    int watched_count = 0;

    for (const auto& ep : episodes) {
        bool is_watched = watched.find(ep.number) != watched.end();
        if (is_watched) watched_count++;

        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "episode-row");

        // Watched icon
        GtkWidget* mark_lbl = gtk_label_new(is_watched ? "✓" : "  ");
        gtk_widget_add_css_class(mark_lbl, "watched-badge");
        gtk_box_append(GTK_BOX(row), mark_lbl);

        // Title
        GtkWidget* title_lbl = gtk_label_new(ep.display_title().c_str());
        gtk_widget_set_hexpand(title_lbl, TRUE);
        gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), title_lbl);

        // Play Button
        GtkWidget* play_btn = gtk_button_new_with_label("▶ Дивитись");
        gtk_widget_add_css_class(play_btn, "suggested-action");

        struct PlayData {
            Anime anime;
            Episode ep;
            std::shared_ptr<BaseProvider> provider;
            GtkWidget* mark_lbl;
        };
        auto* pd = new PlayData{ctx->anime, ep, ctx->provider, mark_lbl};

        g_signal_connect_data(
            play_btn, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer user_data) {
                auto* d = static_cast<PlayData*>(user_data);
                std::thread([d]() {
                    Stream s = d->provider->get_stream(d->anime, d->ep);
                    const Quality* q = s.best();
                    if (q) {
                        PlayerService::play(*q, d->anime.title_ru, &d->ep);
                        HistoryManager::mark_watched(d->anime.provider, d->anime.id, d->ep.number);
                        g_idle_add(+[](gpointer p) -> gboolean {
                            auto* lbl = static_cast<GtkWidget*>(p);
                            if (GTK_IS_LABEL(lbl)) gtk_label_set_text(GTK_LABEL(lbl), "✓");
                            return G_SOURCE_REMOVE;
                        }, d->mark_lbl);
                    }
                }).detach();
            }),
            pd,
            [](gpointer data, GClosure*) { delete static_cast<PlayData*>(data); },
            static_cast<GConnectFlags>(0)
        );

        gtk_box_append(GTK_BOX(row), play_btn);

        // Download Button
        GtkWidget* dl_btn = gtk_button_new_with_label("⬇");
        struct DlData {
            Anime anime;
            Episode ep;
            std::shared_ptr<BaseProvider> provider;
        };
        auto* dd = new DlData{ctx->anime, ep, ctx->provider};

        g_signal_connect_data(
            dl_btn, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer user_data) {
                auto* d = static_cast<DlData*>(user_data);
                std::thread([d]() {
                    Stream s = d->provider->get_stream(d->anime, d->ep);
                    const Quality* q = s.best();
                    if (q) {
                        DownloadService::instance().start_download(d->anime, d->ep, q->label, q->url);
                    }
                }).detach();
            }),
            dd,
            [](gpointer data, GClosure*) { delete static_cast<DlData*>(data); },
            static_cast<GConnectFlags>(0)
        );

        gtk_box_append(GTK_BOX(row), dl_btn);

        gtk_box_append(GTK_BOX(ctx->episodes_box), row);
    }

    if (ctx->progress_lbl && !episodes.empty()) {
        std::string prog = "Переглянуто: " + std::to_string(watched_count) + " / " + std::to_string(episodes.size());
        gtk_label_set_text(GTK_LABEL(ctx->progress_lbl), prog.c_str());
    }
}

static gboolean on_episodes_loaded(gpointer user_data) {
    auto* data = static_cast<EpisodesLoadedData*>(user_data);
    if (data->ctx && GTK_IS_SPINNER(data->ctx->spinner)) {
        gtk_spinner_stop(GTK_SPINNER(data->ctx->spinner));
        gtk_widget_set_visible(data->ctx->spinner, FALSE);
        populate_episodes(data->ctx, data->episodes);
    }
    delete data;
    return G_SOURCE_REMOVE;
}

GtkWidget* DetailsView::create(
    const Anime& anime,
    std::shared_ptr<BaseProvider> provider,
    std::function<void()> on_back
) {
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 32);

    // Top action bar
    GtkWidget* top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    GtkWidget* back_btn = gtk_button_new_with_label("← Назад до каталогу");
    auto* back_cb = new std::function<void()>(std::move(on_back));
    g_signal_connect_data(
        back_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* cb = static_cast<std::function<void()>*>(user_data);
            if (cb && *cb) (*cb)();
        }),
        back_cb,
        [](gpointer data, GClosure*) { delete static_cast<std::function<void()>*>(data); },
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(top_bar), back_btn);

    // Favorite button
    bool is_fav = FavoritesManager::is_favorite(anime.provider, anime.id);
    GtkWidget* fav_btn = gtk_button_new_with_label(is_fav ? "★ В улюблених" : "☆ Додати в улюблені");
    struct FavData { Anime anime; GtkWidget* btn; };
    auto* fd = new FavData{anime, fav_btn};
    g_signal_connect_data(
        fav_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* d = static_cast<FavData*>(user_data);
            bool added = FavoritesManager::toggle_favorite(d->anime);
            gtk_button_set_label(GTK_BUTTON(d->btn), added ? "★ В улюблених" : "☆ Додати в улюблені");
        }),
        fd,
        [](gpointer data, GClosure*) { delete static_cast<FavData*>(data); },
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(top_bar), fav_btn);

    gtk_box_append(GTK_BOX(main_box), top_bar);

    // Scrolled body
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrolled), FALSE);

    GtkWidget* content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 28);
    gtk_widget_set_margin_bottom(content_box, 80);

    // Left column: Poster & Meta
    GtkWidget* left_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_size_request(left_col, 220, -1);

    GtkWidget* poster = gtk_picture_new();
    gtk_widget_set_size_request(poster, 220, 315);
    gtk_picture_set_can_shrink(GTK_PICTURE(poster), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(poster), GTK_CONTENT_FIT_COVER);
    gtk_box_append(GTK_BOX(left_col), poster);

    if (!anime.poster_url.empty()) {
        std::thread([poster_ptr = poster, url = anime.poster_url]() {
            std::string path = ImageCache::ensure_image(url);
            if (!path.empty()) {
                g_idle_add(+[](gpointer p) -> gboolean {
                    auto* d = static_cast<std::pair<GtkWidget*, std::string>*>(p);
                    if (GTK_IS_PICTURE(d->first)) {
                        gtk_picture_set_filename(GTK_PICTURE(d->first), d->second.c_str());
                    }
                    delete d;
                    return G_SOURCE_REMOVE;
                }, new std::pair<GtkWidget*, std::string>(poster_ptr, path));
            }
        }).detach();
    }

    if (!anime.genres.empty()) {
        GtkWidget* genres_flow = gtk_flow_box_new();
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(genres_flow), GTK_SELECTION_NONE);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(genres_flow), 6);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(genres_flow), 6);
        for (const auto& g : anime.genres) {
            GtkWidget* chip = gtk_label_new(g.c_str());
            gtk_widget_add_css_class(chip, "genre-chip");
            gtk_flow_box_append(GTK_FLOW_BOX(genres_flow), chip);
        }
        gtk_box_append(GTK_BOX(left_col), genres_flow);
    }

    gtk_box_append(GTK_BOX(content_box), left_col);

    // Right column: Info & Episodes
    GtkWidget* right_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_hexpand(right_col, TRUE);

    GtkWidget* title_lbl = gtk_label_new(anime.title_ru.c_str());
    gtk_widget_add_css_class(title_lbl, "details-title");
    gtk_label_set_wrap(GTK_LABEL(title_lbl), TRUE);
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(right_col), title_lbl);

    if (!anime.title_en.empty() && anime.title_en != anime.title_ru) {
        GtkWidget* en_lbl = gtk_label_new(anime.title_en.c_str());
        gtk_label_set_xalign(GTK_LABEL(en_lbl), 0.0f);
        gtk_widget_add_css_class(en_lbl, "card-meta");
        gtk_box_append(GTK_BOX(right_col), en_lbl);
    }

    // Metadata row (Year, Age rating, Status)
    GtkWidget* meta_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    if (anime.year > 0) {
        GtkWidget* y_lbl = gtk_label_new(std::to_string(anime.year).c_str());
        gtk_widget_add_css_class(y_lbl, "chip-tag");
        gtk_box_append(GTK_BOX(meta_row), y_lbl);
    }
    if (!anime.age_rating.empty()) {
        GtkWidget* ar_lbl = gtk_label_new(anime.age_rating.c_str());
        gtk_widget_add_css_class(ar_lbl, "chip-tag");
        gtk_box_append(GTK_BOX(meta_row), ar_lbl);
    }
    if (!anime.status.empty()) {
        GtkWidget* st_lbl = gtk_label_new(anime.status.c_str());
        gtk_widget_add_css_class(st_lbl, "chip-tag");
        gtk_box_append(GTK_BOX(meta_row), st_lbl);
    }
    gtk_box_append(GTK_BOX(right_col), meta_row);

    if (!anime.description.empty()) {
        GtkWidget* desc_lbl = gtk_label_new(anime.description.c_str());
        gtk_widget_add_css_class(desc_lbl, "details-desc");
        gtk_label_set_wrap(GTK_LABEL(desc_lbl), TRUE);
        gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0f);
        gtk_box_append(GTK_BOX(right_col), desc_lbl);
    }

    // Separator
    gtk_box_append(GTK_BOX(right_col), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    // Episodes header with Progress
    GtkWidget* eps_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* eps_title = gtk_label_new("Список серій:");
    gtk_widget_add_css_class(eps_title, "card-title");
    gtk_box_append(GTK_BOX(eps_header), eps_title);

    GtkWidget* prog_lbl = gtk_label_new("");
    gtk_widget_add_css_class(prog_lbl, "card-meta");
    gtk_box_append(GTK_BOX(eps_header), prog_lbl);

    GtkWidget* spinner = gtk_spinner_new();
    gtk_spinner_start(GTK_SPINNER(spinner));
    gtk_box_append(GTK_BOX(eps_header), spinner);

    gtk_box_append(GTK_BOX(right_col), eps_header);

    // Episodes Box
    GtkWidget* eps_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(right_col), eps_box);

    gtk_box_append(GTK_BOX(content_box), right_col);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), content_box);
    gtk_box_append(GTK_BOX(main_box), scrolled);

    // Fetch episodes in thread
    auto* ctx = new DetailsContext{anime, provider, eps_box, spinner, prog_lbl, fav_btn};
    std::thread([ctx]() {
        auto eps = ctx->provider->get_episodes(ctx->anime);
        auto* data = new EpisodesLoadedData{ctx, std::move(eps)};
        g_idle_add(on_episodes_loaded, data);
    }).detach();

    return main_box;
}

} // namespace anime::ui
