#include "downloads_view.hpp"
#include "../services/download_service.hpp"

namespace anime::ui {

struct DialogTimerData {
    GtkWidget* list_box;
    guint timer_id;
};

static void update_downloads_list(GtkWidget* list_box) {
    if (!GTK_IS_BOX(list_box)) return;

    // Clear existing
    GtkWidget* child = gtk_widget_get_first_child(list_box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(list_box), child);
        child = next;
    }

    auto jobs = DownloadService::instance().get_jobs();
    if (jobs.empty()) {
        GtkWidget* empty_lbl = gtk_label_new("Немає активних завантажень");
        gtk_widget_add_css_class(empty_lbl, "card-meta");
        gtk_box_append(GTK_BOX(list_box), empty_lbl);
        return;
    }

    for (const auto& job : jobs) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_add_css_class(row, "episode-row");

        GtkWidget* top_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget* title = gtk_label_new((job.anime_title + " — " + job.episode_title).c_str());
        gtk_widget_set_hexpand(title, TRUE);
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_box_append(GTK_BOX(top_box), title);

        std::string status_text = job.progress_percent;
        if (job.is_done) status_text = "Завершено";
        else if (job.has_error) status_text = "Помилка";
        GtkWidget* status_lbl = gtk_label_new(status_text.c_str());
        gtk_widget_add_css_class(status_lbl, "card-meta");
        gtk_box_append(GTK_BOX(top_box), status_lbl);

        gtk_box_append(GTK_BOX(row), top_box);

        GtkWidget* progress = gtk_progress_bar_new();
        try {
            double pct = std::stod(job.progress_percent) / 100.0;
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), pct);
        } catch (...) {
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), job.is_done ? 1.0 : 0.0);
        }
        gtk_box_append(GTK_BOX(row), progress);

        gtk_box_append(GTK_BOX(list_box), row);
    }
}

void DownloadsView::show_dialog(GtkWindow* parent) {
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Фонові завантаження");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);

    GtkWidget* list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    update_downloads_list(list_box);

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list_box);
    gtk_box_append(GTK_BOX(vbox), scrolled);

    auto* timer_data = new DialogTimerData{list_box, 0};
    timer_data->timer_id = g_timeout_add(1000, +[](gpointer data) -> gboolean {
        auto* td = static_cast<DialogTimerData*>(data);
        if (GTK_IS_BOX(td->list_box)) {
            update_downloads_list(td->list_box);
            return G_SOURCE_CONTINUE;
        }
        return G_SOURCE_REMOVE;
    }, timer_data);

    g_signal_connect_data(
        dialog, "destroy",
        G_CALLBACK(+[](GtkWidget*, gpointer data) {
            auto* td = static_cast<DialogTimerData*>(data);
            if (td->timer_id > 0) g_source_remove(td->timer_id);
            delete td;
        }),
        timer_data,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    GtkWidget* close_btn = gtk_button_new_with_label("Закрити");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(vbox), close_btn);

    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    gtk_window_present(GTK_WINDOW(dialog));
}

} // namespace anime::ui
