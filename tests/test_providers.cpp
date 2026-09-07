#include "providers/anilibria_provider.hpp"
#include "providers/dreamcast_provider.hpp"
#include "providers/shizaproject_provider.hpp"
#include "providers/anibaza_provider.hpp"
#include "providers/anidub_provider.hpp"
#include "providers/anitube_provider.hpp"
#include "providers/animevost_provider.hpp"
#include <iostream>

int main() {
    std::cout << "Testing all anime providers...\n";

    std::vector<std::shared_ptr<anime::BaseProvider>> providers = {
        std::make_shared<anime::AnilibriaProvider>(),
        std::make_shared<anime::DreamCastProvider>(),
        std::make_shared<anime::ShizaProjectProvider>(),
        std::make_shared<anime::AniBazaProvider>(),
        std::make_shared<anime::AniDubProvider>(),
        std::make_shared<anime::AniTubeProvider>(),
        std::make_shared<anime::AnimeVostProvider>()
    };

    for (const auto& p : providers) {
        std::cout << "\n----------------------------------------\n";
        std::cout << "Provider: " << p->display_name() << " (" << p->name() << ")\n";
        auto results = p->search("Наруто", 3);
        if (results.empty()) {
            // For providers without Naruto (like newer AniBaza catalog), test catalog/empty query
            results = p->search("", 3);
        }
        std::cout << "Search results found: " << results.size() << "\n";
        for (const auto& a : results) {
            std::cout << "  - " << a.title_ru << " | ID: " << a.id
                      << " | Poster: " << (!a.poster_url.empty() ? a.poster_url.substr(0, 50) + "..." : "NONE") << "\n";
        }
        if (!results.empty()) {
            auto eps = p->get_episodes(results[0]);
            std::cout << "  Episodes count for first title: " << eps.size() << "\n";
            if (!eps.empty()) {
                auto stream = p->get_stream(results[0], eps[0]);
                std::cout << "  Stream qualities count: " << stream.qualities.size() << "\n";
                for (const auto& q : stream.qualities) {
                    std::cout << "    [" << q.label << "] -> " << q.url.substr(0, 70) << "...\n";
                }
            }
        }
    }

    std::cout << "\nAll providers tested successfully!\n";
    return 0;
}
