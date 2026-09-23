#pragma once

#include <gtk/gtk.h>
#include <functional>

namespace anime::ui {

// Width-based layout breakpoints for windowed mode.
enum class LayoutSize {
    Wide,     // >= 1200px
    Compact,  // 860..1199px
    Narrow    // < 860px
};

LayoutSize layout_size_for_width(int width);

// Invokes `cb` once when `widget` is mapped and again whenever the layout size
// of its toplevel window changes. The subscription is dropped on unmap/destroy.
void on_layout_size_changed(GtkWidget* widget, std::function<void(LayoutSize)> cb);

} // namespace anime::ui
