#include "favorites_manager.hpp"
#include "../utils/json.hpp"
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace anime {

std::string FavoritesManager::get_file_path() {
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
    return dir + "/favorites.json";
}

std::vector<Anime> FavoritesManager::get_favorites() {
    std::string file_path = get_file_path();
    if (!std::filesystem::exists(file_path)) return {};

    std::ifstream f(file_path);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto root = json::Value::parse(content);
    if (!root.is_array()) return {};

    std::vector<Anime> list;
    for (size_t i = 0; i < root.size(); ++i) {
        const auto& item = root[i];
        Anime a;
        a.id = item["id"].get_str();
        a.title_ru = item["title_ru"].get_str();
        a.title_en = item["title_en"].get_str();
        a.year = item["year"].get_int();
        a.provider = item["provider"].get_str();
        a.poster_url = item["poster_url"].get_str();
        a.description = item["description"].get_str();
        a.status = item["status"].get_str();

        auto genres_val = item["genres"];
        if (genres_val.is_array()) {
            for (size_t j = 0; j < genres_val.size(); ++j) {
                a.genres.push_back(genres_val[j].get_str());
            }
        }
        list.push_back(a);
    }
    return list;
}

bool FavoritesManager::is_favorite(const std::string& provider, const std::string& anime_id) {
    auto list = get_favorites();
    for (const auto& a : list) {
        if (a.provider == provider && a.id == anime_id) return true;
    }
    return false;
}

bool FavoritesManager::toggle_favorite(const Anime& anime) {
    auto list = get_favorites();
    bool found = false;
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (it->provider == anime.provider && it->id == anime.id) {
            list.erase(it);
            found = true;
            break;
        }
    }

    if (!found) {
        list.insert(list.begin(), anime);
    }

    json::Value root;
    for (const auto& a : list) {
        json::Value item;
        item["id"] = a.id;
        item["title_ru"] = a.title_ru;
        item["title_en"] = a.title_en;
        item["year"] = a.year;
        item["provider"] = a.provider;
        item["poster_url"] = a.poster_url;
        item["description"] = a.description;
        item["status"] = a.status;

        json::Value genres_arr;
        for (const auto& g : a.genres) genres_arr.push_back(g);
        item["genres"] = genres_arr;

        root.push_back(item);
    }

    std::ofstream out(get_file_path());
    out << root.dump(2);

    return !found;
}

} // namespace anime
