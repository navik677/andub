#include "application.hpp"
#include "ui/main_window.hpp"
#include <iostream>
#include <filesystem>

namespace anime {

static void load_css() {
    GtkCssProvider* provider = gtk_css_provider_new();

    // Check multiple possible locations for style.css
    std::vector<std::string> paths = {
        "resources/style.css",
        "../resources/style.css",
        "/usr/share/anime-gui/style.css"
    };

    for (const auto& path : paths) {
        if (std::filesystem::exists(path)) {
            gtk_css_provider_load_from_path(provider, path.c_str());
            break;
        }
    }

    GdkDisplay* display = gdk_display_get_default();
    if (display) {
        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }
    g_object_unref(provider);
}

static void on_activate(GtkApplication* app, gpointer) {
    load_css();
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
