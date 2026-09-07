#pragma once

#include <gtk/gtk.h>
#include <functional>
#include <memory>
#include "../models/anime.hpp"

namespace anime::ui {

class AnimeCard {
public:
    static GtkWidget* create(const Anime& anime, std::function<void(const Anime&)> on_clicked);
};

} // namespace anime::ui
