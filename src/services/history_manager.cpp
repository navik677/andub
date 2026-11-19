#include "history_manager.hpp"
#include "../utils/json.hpp"
#include "../utils/paths.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace anime {

namespace {

// Saves come from the GUI thread and from download/external-player workers
std::mutex g_mutex;

json::Value load(const std::string& path) {
    if (!std::filesystem::exists(path)) return {};
    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return json::Value::parse(content);
}

// Write to a temp file and rename, so a crash mid-write can't wipe the history
void store(const std::string& path, const json::Value& root) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << root.dump(2);
        if (!out) return;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
}

std::string key_of(const std::string& provider, const std::string& anime_id) {
    return provider + ":" + anime_id;
}

} // namespace

std::string HistoryManager::get_history_file() {
    return paths::config_file("history.json");
}

std::string HistoryManager::get_positions_file() {
    return paths::config_file("positions.json");
}

void HistoryManager::mark_watched(const std::string& provider, const std::string& anime_id, const std::string& episode_num) {
    std::lock_guard lock(g_mutex);
    const std::string file_path = get_history_file();
    json::Value root = load(file_path);
    auto& eps = root[key_of(provider, anime_id)];
    for (size_t i = 0; i < eps.size(); ++i) {
        if (eps[i].get_str() == episode_num) return;
    }
    eps.push_back(episode_num);
    store(file_path, root);
}

bool HistoryManager::is_watched(const std::string& provider, const std::string& anime_id, const std::string& episode_num) {
    auto eps = get_watched_episodes(provider, anime_id);
    return eps.find(episode_num) != eps.end();
}

std::set<std::string> HistoryManager::get_watched_episodes(const std::string& provider, const std::string& anime_id) {
    std::lock_guard lock(g_mutex);
    auto eps = load(get_history_file())[key_of(provider, anime_id)];
    std::set<std::string> res;
    for (size_t i = 0; i < eps.size(); ++i) res.insert(eps[i].get_str());
    return res;
}

void HistoryManager::save_progress(const std::string& provider, const std::string& anime_id, const std::string& episode_num,
                                   double position, double duration) {
    std::lock_guard lock(g_mutex);
    const std::string file_path = get_positions_file();
    json::Value root = load(file_path);
    json::Value entry;
    entry["position"] = position;
    entry["duration"] = duration;
    entry["updated"] = static_cast<double>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    root[key_of(provider, anime_id)][episode_num] = entry;
    store(file_path, root);
}

void HistoryManager::clear_progress(const std::string& provider, const std::string& anime_id, const std::string& episode_num) {
    std::lock_guard lock(g_mutex);
    const std::string file_path = get_positions_file();
    json::Value root = load(file_path);
    auto& title = root[key_of(provider, anime_id)];
    if (!title.is_object() || !title.obj_val.count(episode_num)) return;
    title.obj_val.erase(episode_num);
    if (title.obj_val.empty()) root.obj_val.erase(key_of(provider, anime_id));
    store(file_path, root);
}

std::map<std::string, EpisodeProgress> HistoryManager::get_progress(const std::string& provider, const std::string& anime_id) {
    std::lock_guard lock(g_mutex);
    const auto title = load(get_positions_file())[key_of(provider, anime_id)];
    std::map<std::string, EpisodeProgress> out;
    if (!title.is_object()) return out;
    for (const auto& [ep, v] : title.obj_val) {
        EpisodeProgress p;
        p.position = v["position"].get_double();
        p.duration = v["duration"].get_double();
        p.updated = static_cast<long long>(v["updated"].get_double());
        if (p.position > 0) out.emplace(ep, p);
    }
    return out;
}

void HistoryManager::reset_title(const std::string& provider, const std::string& anime_id) {
    std::lock_guard lock(g_mutex);
    const std::string key = key_of(provider, anime_id);
    for (const std::string& path : {get_history_file(), get_positions_file()}) {
        json::Value root = load(path);
        if (root.is_object() && root.obj_val.erase(key)) store(path, root);
    }
}

} // namespace anime
