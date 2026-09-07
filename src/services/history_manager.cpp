#include "history_manager.hpp"
#include "../utils/json.hpp"
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace anime {

std::string HistoryManager::get_history_file() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::string base;
    if (xdg && *xdg) base = xdg;
    else if (home && *home) base = std::string(home) + "/.config";
    else base = "/tmp";

    std::string dir = base + "/anime-gui";
    try {
        std::filesystem::create_directories(dir);
    } catch (...) {
        dir = "/tmp/anime-gui";
        try { std::filesystem::create_directories(dir); } catch (...) {}
    }
    return dir + "/history.json";
}

void HistoryManager::mark_watched(const std::string& provider, const std::string& anime_id, const std::string& episode_num) {
    std::string file_path = get_history_file();
    json::Value root;
    if (std::filesystem::exists(file_path)) {
        std::ifstream f(file_path);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        root = json::Value::parse(content);
    }

    std::string key = provider + ":" + anime_id;
    auto& eps = root[key];
    bool already = false;
    for (size_t i = 0; i < eps.size(); ++i) {
        if (eps[i].get_str() == episode_num) {
            already = true;
            break;
        }
    }
    if (!already) {
        eps.push_back(episode_num);
        std::ofstream out(file_path);
        out << root.dump(2);
    }
}

bool HistoryManager::is_watched(const std::string& provider, const std::string& anime_id, const std::string& episode_num) {
    auto eps = get_watched_episodes(provider, anime_id);
    return eps.find(episode_num) != eps.end();
}

std::set<std::string> HistoryManager::get_watched_episodes(const std::string& provider, const std::string& anime_id) {
    std::string file_path = get_history_file();
    if (!std::filesystem::exists(file_path)) return {};

    std::ifstream f(file_path);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto root = json::Value::parse(content);

    std::string key = provider + ":" + anime_id;
    auto eps = root[key];
    std::set<std::string> res;
    for (size_t i = 0; i < eps.size(); ++i) {
        res.insert(eps[i].get_str());
    }
    return res;
}

} // namespace anime
