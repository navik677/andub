#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../models/anime.hpp"
#include "../models/episode.hpp"
#include "../providers/base_provider.hpp"

namespace anime::ui {

class PlayerView {
public:
    static GtkWidget* create(
        const Anime& anime,
        const std::vector<Episode>& all_episodes,
        size_t initial_episode_idx,
        std::shared_ptr<BaseProvider> provider,
        std::function<void()> on_close
    );
};

} // namespace anime::ui
