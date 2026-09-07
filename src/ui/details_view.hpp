#pragma once

#include <gtk/gtk.h>
#include <functional>
#include <memory>
#include "../models/anime.hpp"
#include "../providers/base_provider.hpp"

namespace anime::ui {

class DetailsView {
public:
    static GtkWidget* create(
        const Anime& anime,
        std::shared_ptr<BaseProvider> provider,
        std::function<void()> on_back
    );
};

} // namespace anime::ui
