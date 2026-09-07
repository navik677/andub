#pragma once

#include "base_provider.hpp"
#include "../utils/json.hpp"

namespace anime {

class DreamCastProvider : public BaseProvider {
public:
    std::string name() const override { return "dreamcast"; }
    std::string display_name() const override { return "Dream Cast"; }

    std::vector<Anime> search(const std::string& query, int limit = 30, const std::string& genre = "", int page = 1) override;
    std::vector<Episode> get_episodes(const Anime& anime) override;
    Stream get_stream(const Anime& anime, const Episode& episode) override;

private:
    std::string api_base = "https://kodik-api.com";
    std::string api_token = "56a768d08f43091901c44b54fe970049";
    int translation_id = 1978;
};

} // namespace anime
