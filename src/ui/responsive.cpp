#include "responsive.hpp"

namespace anime::ui {

LayoutSize layout_size_for_width(int width) {
    if (width < 860) return LayoutSize::Narrow;
    if (width < 1200) return LayoutSize::Compact;
    return LayoutSize::Wide;
}

namespace {

struct LayoutWatch {
    std::function<void(LayoutSize)> cb;
    GdkSurface* surface = nullptr;
    gulong handler_id = 0;
    int last = -1;
};

void deliver(LayoutWatch* w, int width) {
    if (width <= 0) return;
    int size = static_cast<int>(layout_size_for_width(width));
    if (size == w->last) return;
    w->last = size;
    w->cb(static_cast<LayoutSize>(size));
}

void detach(LayoutWatch* w) {
    if (w->surface && w->handler_id) {
        g_signal_handler_disconnect(w->surface, w->handler_id);
    }
    if (w->surface) g_object_remove_weak_pointer(G_OBJECT(w->surface), reinterpret_cast<gpointer*>(&w->surface));
    w->surface = nullptr;
    w->handler_id = 0;
}

} // namespace

void on_layout_size_changed(GtkWidget* widget, std::function<void(LayoutSize)> cb) {
    auto* watch = new LayoutWatch{std::move(cb)};

    // Owned by the widget: freed (and detached from the surface) on destroy.
    g_object_set_data_full(G_OBJECT(widget), "anime-layout-watch", watch, [](gpointer data) {
        auto* w = static_cast<LayoutWatch*>(data);
        detach(w);
        delete w;
    });

    g_signal_connect(widget, "map", G_CALLBACK(+[](GtkWidget* self, gpointer user_data) {
        auto* w = static_cast<LayoutWatch*>(user_data);
        detach(w);
        GtkNative* native = gtk_widget_get_native(self);
        GdkSurface* surface = native ? gtk_native_get_surface(native) : nullptr;
        if (!surface) return;

        w->surface = surface;
        g_object_add_weak_pointer(G_OBJECT(surface), reinterpret_cast<gpointer*>(&w->surface));
        w->handler_id = g_signal_connect(surface, "notify::width", G_CALLBACK(+[](GdkSurface* s, GParamSpec*, gpointer d) {
            deliver(static_cast<LayoutWatch*>(d), gdk_surface_get_width(s));
        }), w);
        deliver(w, gdk_surface_get_width(surface));
    }), watch);

    g_signal_connect(widget, "unmap", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        detach(static_cast<LayoutWatch*>(user_data));
    }), watch);
}

} // namespace anime::ui
