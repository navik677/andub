#pragma once

#include <gtk/gtk.h>

namespace anime {

class Application {
public:
    static GtkApplication* create();
};

} // namespace anime
