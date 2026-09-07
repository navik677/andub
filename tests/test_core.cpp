#include "utils/json.hpp"
#include "models/anime.hpp"
#include "models/episode.hpp"
#include "services/history_manager.hpp"
#include "services/favorites_manager.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "Testing JSON parser...\n";
    std::string test_json = R"({
        "title": "Naruto",
        "cyrillic": "\u0410\u0442\u0430\u043a\u0430 \u0422\u0438\u0442\u0430\u043d\u0456\u0432",
        "year": 2002,
        "genres": ["Action", "Shounen"],
        "active": true,
        "rating": null
    })";

    auto root = anime::json::Value::parse(test_json);
    assert(root.is_object());
    assert(root["title"].get_str() == "Naruto");
    assert(root["cyrillic"].get_str() == "Атака Титанів");
    assert(root["year"].get_int() == 2002);
    assert(root["genres"].is_array());
    assert(root["genres"].size() == 2);
    assert(root["genres"][0].get_str() == "Action");
    assert(root["active"].get_bool() == true);
    assert(root["rating"].is_null());

    std::cout << "JSON parser test passed!\n";

    std::cout << "Testing FavoritesManager...\n";
    anime::Anime a;
    a.id = "test_123";
    a.title_ru = "Тестове аніме";
    a.provider = "anilibria";
    a.year = 2024;

    bool added = anime::FavoritesManager::toggle_favorite(a);
    assert(added);
    assert(anime::FavoritesManager::is_favorite("anilibria", "test_123"));

    bool removed = !anime::FavoritesManager::toggle_favorite(a);
    assert(removed);
    assert(!anime::FavoritesManager::is_favorite("anilibria", "test_123"));
    std::cout << "FavoritesManager test passed!\n";

    std::cout << "Testing HistoryManager...\n";
    anime::HistoryManager::mark_watched("anilibria", "999", "1");
    assert(anime::HistoryManager::is_watched("anilibria", "999", "1"));
    assert(!anime::HistoryManager::is_watched("anilibria", "999", "2"));
    std::cout << "HistoryManager test passed!\n";

    std::cout << "\nAll core C++ components verified successfully!\n";
    return 0;
}
