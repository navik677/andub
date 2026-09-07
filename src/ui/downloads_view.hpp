#pragma once

#include <gtk/gtk.h>

namespace anime::ui {

class DownloadsView {
public:
    static void show_dialog(GtkWindow* parent);
};

} // namespace anime::ui
