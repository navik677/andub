#pragma once

#include <string>
#include <set>
#include <map>

namespace anime {

class HistoryManager {
public:
    static void mark_watched(const std::string& provider, const std::string& anime_id, const std::string& episode_num);
    static bool is_watched(const std::string& provider, const std::string& anime_id, const std::string& episode_num);
    static std::set<std::string> get_watched_episodes(const std::string& provider, const std::string& anime_id);

private:
    static std::string get_history_file();
};

} // namespace anime
