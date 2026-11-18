#include "provider_registry.hpp"
#include "../providers/anilibria_provider.hpp"
#include "../providers/dreamcast_provider.hpp"
#include "../providers/shizaproject_provider.hpp"
#include "../providers/anibaza_provider.hpp"
#include "../providers/anidub_provider.hpp"
#include "../providers/anitube_provider.hpp"
#include "../providers/animevost_provider.hpp"
#include "../providers/anistar_provider.hpp"

namespace anime::app {

const std::vector<std::shared_ptr<BaseProvider>>& ProviderRegistry::all() {
    static const std::vector<std::shared_ptr<BaseProvider>> list = {
        std::make_shared<AnilibriaProvider>(),
        std::make_shared<DreamCastProvider>(),
        std::make_shared<ShizaProjectProvider>(),
        std::make_shared<AniBazaProvider>(),
        std::make_shared<AniDubProvider>(),
        std::make_shared<AniTubeProvider>(),
        std::make_shared<AnimeVostProvider>(),
        std::make_shared<AniStarProvider>(),
    };
    return list;
}

std::shared_ptr<BaseProvider> ProviderRegistry::by_name(const std::string& name) {
    for (const auto& p : all()) {
        if (p->name() == name) return p;
    }
    return all().front();
}

std::string ProviderRegistry::display_name(const std::string& name) {
    for (const auto& p : all()) {
        if (p->name() == name) return p->display_name();
    }
    return name;
}

} // namespace anime::app
