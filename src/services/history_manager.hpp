#pragma once

#include <map>
#include <set>
#include <string>

namespace anime {

// Where the viewer stopped inside an episode
struct EpisodeProgress {
    double position = 0;   // seconds
    double duration = 0;   // seconds, 0 if unknown
    long long updated = 0; // unix time of the last save
};

// Watched episodes (history.json) and positions inside unfinished episodes
// (positions.json), per "provider:anime_id".
class HistoryManager {
public:
    static void mark_watched(const std::string& provider, const std::string& anime_id, const std::string& episode_num);
    static bool is_watched(const std::string& provider, const std::string& anime_id, const std::string& episode_num);
    static std::set<std::string> get_watched_episodes(const std::string& provider, const std::string& anime_id);

    static void save_progress(const std::string& provider, const std::string& anime_id, const std::string& episode_num,
                              double position, double duration);
    static void clear_progress(const std::string& provider, const std::string& anime_id, const std::string& episode_num);
    // episode number -> progress
    static std::map<std::string, EpisodeProgress> get_progress(const std::string& provider, const std::string& anime_id);

    // "Переглянути знову": forget watched marks and positions of one title
    static void reset_title(const std::string& provider, const std::string& anime_id);

private:
    static std::string get_history_file();
    static std::string get_positions_file();
};

} // namespace anime
