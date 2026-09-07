#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <vector>
#include "../providers/base_provider.hpp"
#include "../models/anime.hpp"

namespace anime::ui {

class MainWindow {
public:
    static GtkWidget* create(GtkApplication* app);

private:
    MainWindow() = default;
};

} // namespace anime::ui
