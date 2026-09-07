#include "application.hpp"
#include "ui/main_window.hpp"
#include "services/theme_manager.hpp"
#include <iostream>

namespace anime {

static void on_activate(GtkApplication* app, gpointer) {
    ThemeManager::apply_theme(ThemeManager::get_current_theme_id());
    GtkWidget* window = ui::MainWindow::create(app);
    gtk_window_present(GTK_WINDOW(window));
}

GtkApplication* Application::create() {
    GtkApplication* app = gtk_application_new(
        "io.github.navik677.anime-gui",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    return app;
}

} // namespace anime
