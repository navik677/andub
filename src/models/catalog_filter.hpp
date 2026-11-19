#pragma once

// Catalog filters and sorting, shared by every provider.
//
// Providers that can filter on their server say so through FilterSupport and
// apply those parts of the filter in browse(). Whatever a provider can't do on
// the server, Catalog applies on the client with matches(), using the
// normalized metadata below (kind, airing, season, age, score) that providers
// fill from their list pages.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include "../utils/str_utils.hpp"

namespace anime {

enum class SortOrder { Default, Updated, Rating, YearDesc, YearAsc, Title };
enum class Kind { Unknown, TV, Movie, OVA, ONA, Special };
enum class Airing { Unknown, Ongoing, Finished, Announced };
enum class Season { Unknown, Winter, Spring, Summer, Autumn };

struct CatalogFilter {
    SortOrder sort = SortOrder::Default;
    int year_from = 0;            // 0 = no lower bound
    int year_to = 0;              // 0 = no upper bound
    Season season = Season::Unknown;   // Unknown = any season
    std::vector<Kind> kinds;           // empty = any kind
    Airing airing = Airing::Unknown;   // Unknown = any
    std::vector<std::string> genres;   // catalog values (Russian names); every one must match
    int max_age = 0;                   // 0 = any; otherwise hide titles rated above (6/12/16/18)

    bool has_years() const { return year_from > 0 || year_to > 0; }
    bool empty_filters() const {
        return !has_years() && season == Season::Unknown && kinds.empty() && airing == Airing::Unknown &&
               genres.empty() && max_age == 0;
    }
    bool empty() const { return sort == SortOrder::Default && empty_filters(); }
};

// What a provider applies on its own server. The rest is filtered client-side.
struct FilterSupport {
    bool sort = false;
    bool years = false;
    bool season = false;
    bool kinds = false;
    bool airing = false;
    bool genres = false;   // all selected genres, not just one
    bool age = false;
};

// Which normalized fields a provider's list results carry, so the client can
// filter on them. A field that is neither on the server nor in the data can't
// be filtered for that provider and the UI disables it.
struct MetaSupport {
    bool year = false;
    bool score = false;
    bool kind = false;
    bool airing = false;
    bool season = false;
    bool age = false;
    bool genres = false;
};

// ---------------------------------------------------------------------------
// Genres: the catalog value is the Russian name (what most sites index), the
// Ukrainian one is shown in the UI and used by Ukrainian sites (AniTube).
struct GenreName {
    const char* uk;
    const char* ru;
};

inline const std::vector<GenreName>& genre_table() {
    static const std::vector<GenreName> t = {
        {"Бойові мистецтва", "Боевые искусства"}, {"Вампіри", "Вампиры"}, {"Гарем", "Гарем"},
        {"Демони", "Демоны"}, {"Детектив", "Детектив"}, {"Дзьосей", "Дзёсей"}, {"Драма", "Драма"},
        {"Ігри", "Игры"}, {"Ісекай", "Исекай"}, {"Історичне", "Исторический"}, {"Кіберпанк", "Киберпанк"},
        {"Комедія", "Комедия"}, {"Магія", "Магия"}, {"Меха", "Меха"}, {"Містика", "Мистика"},
        {"Музика", "Музыка"}, {"Пародія", "Пародия"}, {"Повсякденність", "Повседневность"},
        {"Пригоди", "Приключения"}, {"Психологічне", "Психологическое"}, {"Романтика", "Романтика"},
        {"Надприродне", "Сверхъестественное"}, {"Сейнен", "Сейнен"}, {"Спорт", "Спорт"},
        {"Суперсила", "Супер сила"}, {"Сьодзьо", "Сёдзе"}, {"Сьодзьо-ай", "Сёдзе-ай"}, {"Сьонен", "Сёнен"},
        {"Трилер", "Триллер"}, {"Жахи", "Ужасы"}, {"Фантастика", "Фантастика"}, {"Фентезі", "Фэнтези"},
        {"Школа", "Школа"}, {"Екшн", "Экшен"}, {"Етті", "Этти"},
    };
    return t;
}

namespace detail {
// "Сёнен" / "Сэйнэн" / "Сейнен" spellings differ: compare a lowercase form with
// ё and э folded to е and spaces/hyphens dropped.
inline std::string genre_key(const std::string& s) {
    std::string l = utils::utf8_tolower(utils::trim(s));
    std::string out;
    for (size_t i = 0; i < l.size(); ++i) {
        if (l.compare(i, 2, "ё") == 0 || l.compare(i, 2, "э") == 0) { out += "е"; ++i; continue; }
        if (l[i] == ' ' || l[i] == '-') continue;
        out += l[i];
    }
    return out;
}
} // namespace detail

// Does a provider genre name (any language/spelling) mean the catalog genre?
inline bool genre_matches(const std::string& item_genre, const std::string& catalog_ru) {
    const std::string item = detail::genre_key(item_genre);
    if (item.empty()) return false;
    std::vector<std::string> names{detail::genre_key(catalog_ru)};
    for (const auto& g : genre_table()) {
        if (catalog_ru == g.ru) names.push_back(detail::genre_key(g.uk));
    }
    for (const auto& n : names) {
        if (!n.empty() && item == n) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Normalizers for the free-form strings sites use.

inline Kind parse_kind(const std::string& raw) {
    const std::string s = utils::utf8_tolower(raw);
    auto has = [&](const char* w) { return s.find(w) != std::string::npos; };
    if (s.empty()) return Kind::Unknown;
    if (has("ova") || has("ова") || has("oad")) return Kind::OVA;
    if (has("ona") || has("она") || has("web")) return Kind::ONA;
    if (has("фильм") || has("фільм") || has("movie") || has("полнометраж") || has("повнометраж")) return Kind::Movie;
    if (has("спешл") || has("спэшл") || has("special") || has("спецвып") || has("tv_special")) return Kind::Special;
    if (has("tv") || has("тв") || has("сериал") || has("серіал")) return Kind::TV;
    return Kind::Unknown;
}

inline Airing parse_airing(const std::string& raw) {
    const std::string s = utils::utf8_tolower(raw);
    auto has = [&](const char* w) { return s.find(w) != std::string::npos; };
    if (s.empty()) return Airing::Unknown;
    if (has("анонс") || has("announce") || has("anons")) return Airing::Announced;
    if (has("ongoing") || has("онгоинг") || has("онгоїнг") || has("онґоїнґ") || has("в эфире") ||
        has("выходит") || has("виходить") || has("в ефірі") || has("is_ongoing"))
        return Airing::Ongoing;
    if (has("released") || has("завершен") || has("вышел") || has("вийшов") || has("finished") ||
        has("completed") || has("is_not_ongoing"))
        return Airing::Finished;
    return Airing::Unknown;
}

// "9 з 12", "[1-3 из 12]", "12 из 12" -> ongoing / finished
inline Airing airing_from_episode_counts(int aired, int total) {
    if (aired <= 0 || total <= 0) return Airing::Unknown;
    return aired < total ? Airing::Ongoing : Airing::Finished;
}

// "PG-13", "R+", "16+", "R16_PLUS", "Rx" -> minimum viewer age, or -1
inline int parse_age(const std::string& raw) {
    std::string s = utils::utf8_tolower(raw);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    if (s.empty()) return -1;
    if (s == "g") return 0;
    if (s == "pg") return 6;
    if (s == "pg-13" || s == "pg_13" || s == "pg13") return 12;
    if (s == "r") return 16;
    if (s == "r+" || s == "r_plus" || s == "rx" || s == "nc-17") return 18;
    for (size_t i = 0; i < s.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[i]))) {
            int v = std::atoi(s.c_str() + i);
            if (v == 0 || v == 6 || v == 12 || v == 16 || v == 18) return v;
            break;
        }
    }
    return -1;
}

// Month 1..12 -> anime season (Dec belongs to winter)
inline Season season_from_month(int month) {
    if (month == 12 || month == 1 || month == 2) return Season::Winter;
    if (month >= 3 && month <= 5) return Season::Spring;
    if (month >= 6 && month <= 8) return Season::Summer;
    if (month >= 9 && month <= 11) return Season::Autumn;
    return Season::Unknown;
}

inline Season parse_season(const std::string& raw) {
    const std::string s = utils::utf8_tolower(raw);
    auto has = [&](const char* w) { return s.find(w) != std::string::npos; };
    if (has("winter") || has("зима") || has("зимн")) return Season::Winter;
    if (has("spring") || has("весна") || has("весен")) return Season::Spring;
    if (has("summer") || has("лето") || has("літо") || has("летн") || has("літн")) return Season::Summer;
    if (has("autumn") || has("fall") || has("осень") || has("осінь") || has("осен") || has("осін")) return Season::Autumn;
    return Season::Unknown;
}

// "8,4", "★ 7.5", "4.6/5", "220px of 250" -> score on a 0..10 scale, or -1.
// Plain big numbers are popularity counts, not scores.
inline double parse_score(const std::string& raw, double scale_max = 10.0) {
    std::string s;
    for (char c : raw) s += (c == ',') ? '.' : c;
    const char* p = s.c_str();
    while (*p && !(std::isdigit(static_cast<unsigned char>(*p)))) ++p;
    if (!*p) return -1;
    char* end = nullptr;
    double v = std::strtod(p, &end);
    if (v <= 0 || v > scale_max) return -1;
    return v * 10.0 / scale_max;
}

} // namespace anime
