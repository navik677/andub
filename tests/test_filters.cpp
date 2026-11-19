// Client-side catalog filtering: normalizers, matches() and sort_items().
#include <cstdio>
#include <cstdlib>
#include "models/catalog_match.hpp"

using namespace anime;

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            ++failures;                                                             \
        }                                                                           \
    } while (0)

static Anime make(const char* title, int year, Kind kind, Airing airing, int age, double score,
                  std::vector<std::string> genres = {}) {
    Anime a;
    a.id = title;
    a.title_ru = title;
    a.year = year;
    a.kind = kind;
    a.airing = airing;
    a.age = age;
    a.score = score;
    a.genres = std::move(genres);
    return a;
}

int main() {
    // --- normalizers
    CHECK(parse_kind("ТВ") == Kind::TV);
    CHECK(parse_kind("TV Сериал") == Kind::TV);
    CHECK(parse_kind("Фильм") == Kind::Movie);
    CHECK(parse_kind("OVA") == Kind::OVA);
    CHECK(parse_kind("ONA") == Kind::ONA);
    CHECK(parse_kind("TV_SPECIAL") == Kind::Special);
    CHECK(parse_kind("ТВ-спэшл") == Kind::Special);
    CHECK(parse_kind("") == Kind::Unknown);

    CHECK(parse_airing("Аниме Ongoing") == Airing::Ongoing);
    CHECK(parse_airing("В эфире") == Airing::Ongoing);
    CHECK(parse_airing("released") == Airing::Finished);
    CHECK(parse_airing("anons") == Airing::Announced);
    CHECK(airing_from_episode_counts(9, 12) == Airing::Ongoing);
    CHECK(airing_from_episode_counts(12, 12) == Airing::Finished);

    CHECK(parse_age("PG-13") == 12);
    CHECK(parse_age("R+") == 18);
    CHECK(parse_age("16+") == 16);
    CHECK(parse_age("R16_PLUS") == 16);
    CHECK(parse_age("Rx") == 18);
    CHECK(parse_age("") == -1);

    CHECK(season_from_month(12) == Season::Winter);
    CHECK(season_from_month(4) == Season::Spring);
    CHECK(season_from_month(10) == Season::Autumn);
    CHECK(parse_season("FALL") == Season::Autumn);
    CHECK(parse_season("Весна") == Season::Spring);

    CHECK(parse_score("★ 7,5") > 7.4 && parse_score("★ 7,5") < 7.6);
    CHECK(parse_score("4.6", 5.0) > 9.1);
    CHECK(parse_score("12500") < 0); // popularity count, not a score

    // --- genres across languages and spellings
    CHECK(genre_matches("Комедія", "Комедия"));
    CHECK(genre_matches("комедия", "Комедия"));
    CHECK(genre_matches("Сьонен", "Сёнен"));
    CHECK(genre_matches("Сенен", "Сёнен"));
    CHECK(!genre_matches("Сёдзе-ай", "Сёдзе"));
    CHECK(!genre_matches("Драма", "Комедия"));

    // --- matches(): client checks only what the server didn't do
    FilterSupport none;
    MetaSupport all{true, true, true, true, true, true, true};
    Anime tv2020 = make("A", 2020, Kind::TV, Airing::Finished, 16, 8.0, {"Комедия", "Школа"});
    Anime movie2015 = make("B", 2015, Kind::Movie, Airing::Finished, 12, 7.0, {"Драма"});
    Anime unknown = make("C", 0, Kind::Unknown, Airing::Unknown, -1, -1);

    CatalogFilter f;
    f.year_from = 2018;
    CHECK(matches(tv2020, f, none, all));
    CHECK(!matches(movie2015, f, none, all));
    CHECK(!matches(unknown, f, none, all)); // unknown year never matches an active filter
    FilterSupport years_on_server;
    years_on_server.years = true;
    CHECK(matches(movie2015, f, years_on_server, all)); // trust the server

    f = {};
    f.kinds = {Kind::Movie, Kind::OVA};
    CHECK(matches(movie2015, f, none, all));
    CHECK(!matches(tv2020, f, none, all));

    f = {};
    f.max_age = 12;
    CHECK(matches(movie2015, f, none, all));
    CHECK(!matches(tv2020, f, none, all));

    f = {};
    f.genres = {"Комедия", "Школа"};
    CHECK(matches(tv2020, f, none, all));
    f.genres = {"Комедия", "Драма"};
    CHECK(!matches(tv2020, f, none, all)); // every genre must match
    MetaSupport no_genres = all;
    no_genres.genres = false;
    CHECK(matches(unknown, f, none, no_genres)); // site doesn't list genres: server searched by it

    // --- sorting
    std::vector<Anime> list{unknown, movie2015, tv2020};
    sort_items(list, SortOrder::Rating);
    CHECK(list[0].id == "A" && list[1].id == "B" && list[2].id == "C");
    sort_items(list, SortOrder::YearAsc);
    CHECK(list[0].id == "B" && list[1].id == "A" && list[2].id == "C"); // unknown year last
    sort_items(list, SortOrder::YearDesc);
    CHECK(list[0].id == "A" && list[1].id == "B");
    sort_items(list, SortOrder::Title);
    CHECK(list[0].id == "A" && list[1].id == "B" && list[2].id == "C");

    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("All filter checks passed\n");
    return 0;
}
