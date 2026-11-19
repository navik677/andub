#include "shizaproject_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include "../utils/kodik_resolver.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>

namespace anime {

namespace {

const std::map<std::string, std::string>& shiza_headers() {
    static const std::map<std::string, std::string> h = {
        {"Content-Type", "application/json"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
    };
    return h;
}

// Genre ids (opaque GraphQL ids), fetched once per run: {name -> id}
std::vector<std::pair<std::string, std::string>> shiza_genres(const std::string& graphql_url) {
    static std::mutex m;
    static std::vector<std::pair<std::string, std::string>> cache;
    std::lock_guard lock(m);
    if (!cache.empty()) return cache;
    json::Value body;
    body["query"] = "{ genres(first: 200) { edges { node { id name } } } }";
    auto resp = http::Client::post(graphql_url, body.dump(), shiza_headers());
    if (!resp.success()) return {};
    auto edges = json::Value::parse(resp.body)["data"]["genres"]["edges"];
    for (size_t i = 0; i < edges.size(); ++i) {
        auto node = edges[i]["node"];
        cache.emplace_back(node["name"].get_str(), node["id"].get_str());
    }
    return cache;
}

} // namespace

std::vector<Anime> ShizaProjectProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    CatalogFilter f;
    if (!genre.empty()) f.genres.push_back(genre);
    return browse(query, limit, f, page);
}

std::vector<Anime> ShizaProjectProvider::browse(const std::string& query, int limit, const CatalogFilter& filter, int page) {
    // Relay-style connection without page numbers: ask for everything up to
    // this page and keep the tail
    const int fetch_count = limit * std::max(1, page);

    // Only enum literals, numbers and ids from the API go into the query text;
    // the user's search string is passed as a variable.
    std::string args = "first: " + std::to_string(fetch_count) + ", query: $q";

    std::string genre_ids;
    if (!filter.genres.empty()) {
        const auto all = shiza_genres(graphql_url);
        for (const auto& want : filter.genres) {
            bool found = false;
            for (const auto& [gname, gid] : all) {
                if (genre_matches(gname, want)) {
                    genre_ids += (genre_ids.empty() ? "\"" : ", \"") + gid + "\"";
                    found = true;
                    break;
                }
            }
            // A genre this site doesn't have (e.g. "Школа"): nothing can match
            if (!found) return {};
        }
        if (!genre_ids.empty()) args += ", genre: {include: [" + genre_ids + "]}";
    }

    std::string types;
    for (Kind k : filter.kinds) {
        const char* v = nullptr;
        switch (k) {
        case Kind::TV: v = "TV"; break;
        case Kind::Movie: v = "MOVIE"; break;
        case Kind::OVA: v = "OVA"; break;
        case Kind::ONA: v = "ONA"; break;
        case Kind::Special: v = "TV_SPECIAL"; break;
        default: break;
        }
        if (v) types += (types.empty() ? "" : ", ") + std::string(v);
    }
    if (!types.empty()) args += ", type: {include: [" + types + "]}";

    if (filter.airing == Airing::Ongoing) args += ", status: {include: [ONGOING]}";
    else if (filter.airing == Airing::Finished) args += ", status: {include: [RELEASED]}";
    else if (filter.airing == Airing::Announced) args += ", status: {include: [ANNOUNCE]}";

    static const char* seasons[] = {"", "WINTER", "SPRING", "SUMMER", "FALL"};
    if (filter.season != Season::Unknown)
        args += std::string(", season: {include: [") + seasons[static_cast<int>(filter.season)] + "]}";

    if (filter.has_years()) {
        args += ", airedOn: {";
        if (filter.year_from > 0) args += "startYear: " + std::to_string(filter.year_from);
        if (filter.year_to > 0) args += std::string(filter.year_from > 0 ? ", " : "") + "endYear: " + std::to_string(filter.year_to);
        args += "}";
    }

    if (filter.max_age > 0) {
        std::string ratings;
        const std::pair<int, const char*> scale[] = {{0, "G"}, {6, "PG"}, {12, "PG_13"}, {16, "R"}, {18, "R_PLUS"}, {18, "RX"}};
        for (const auto& [age, v] : scale) {
            if (age <= filter.max_age) ratings += (ratings.empty() ? "" : ", ") + std::string(v);
        }
        args += ", rating: {include: [" + ratings + "]}";
    }

    switch (filter.sort) {
    case SortOrder::Updated: args += ", orderBy: {field: UPDATED_AT, direction: DESC}"; break;
    case SortOrder::Rating: args += ", orderBy: {field: SCORE, direction: DESC}"; break;
    // No release-date order in the API: newest published first is the closest
    case SortOrder::YearDesc: args += ", orderBy: {field: PUBLISHED_AT, direction: DESC}"; break;
    case SortOrder::YearAsc: args += ", orderBy: {field: PUBLISHED_AT, direction: ASC}"; break;
    default: break;
    }

    json::Value req_body;
    req_body["query"] = "query($q: String) { releases(" + args +
                        ") { edges { node { id name originalName slug description type status season airedOn rating score "
                        "posters { preview: resize(width: 360, height: 500) { url } original { url } } genres { name } } } } }";
    json::Value vars;
    if (query.empty()) vars["q"] = json::Value(nullptr);
    else vars["q"] = query;
    req_body["variables"] = vars;

    auto resp = http::Client::post(graphql_url, req_body.dump(), shiza_headers());
    if (!resp.success()) {
        std::cerr << "[ShizaProject] Search request failed: " << resp.error << " (status: " << resp.status_code << ")\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    auto edges = root["data"]["releases"]["edges"];
    if (!edges.is_array()) {
        std::cerr << "[ShizaProject] Unexpected response: " << resp.body.substr(0, 300) << "\n";
        return {};
    }

    std::vector<Anime> results;
    const size_t skip_count = static_cast<size_t>((std::max(1, page) - 1) * limit);
    for (size_t i = skip_count; i < edges.size(); ++i) {
        auto node = edges[i]["node"];
        Anime a;
        a.id = node["slug"].get_str();
        if (a.id.empty()) a.id = node["id"].get_str();
        a.title_ru = node["name"].get_str();
        a.title_en = node["originalName"].get_str();
        a.description = node["description"].get_str();
        a.provider = name();
        a.meta["slug"] = node["slug"].get_str();

        a.kind = parse_kind(node["type"].get_str());
        a.airing = parse_airing(node["status"].get_str());
        a.season = parse_season(node["season"].get_str());
        a.age = parse_age(node["rating"].get_str());
        const std::string aired = node["airedOn"].get_str(); // "2026-10-13"
        if (aired.size() >= 4) a.year = std::atoi(aired.substr(0, 4).c_str());
        const double score = node["score"].get_double();
        if (score > 0) {
            a.score = score;
            char buf[16];
            std::snprintf(buf, sizeof buf, "%.1f", a.score);
            a.rating = buf;
        }

        auto posters = node["posters"];
        if (posters.is_array() && posters.size() > 0) {
            std::string preview_url = posters[0]["preview"]["url"].get_str();
            a.poster_url = !preview_url.empty() ? preview_url : posters[0]["original"]["url"].get_str();
        }

        auto genres = node["genres"];
        for (size_t g = 0; g < genres.size(); ++g) {
            std::string gname = genres[g]["name"].get_str();
            if (!gname.empty()) a.genres.push_back(gname);
        }

        results.push_back(std::move(a));
        if (static_cast<int>(results.size()) >= limit) break;
    }
    return results;
}

std::vector<Episode> ShizaProjectProvider::get_episodes(const Anime& anime) {
    std::string slug = anime.meta.count("slug") ? anime.meta.at("slug") : anime.id;
    if (slug.empty()) slug = anime.id;

    json::Value req_body;
    req_body["query"] = "query { release(slug: \"" + slug + "\") { episodes { id number name videos { id embedSource embedUrl } } } }";

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
    };

    auto resp = http::Client::post(graphql_url, req_body.dump(), headers);
    if (!resp.success()) {
        std::cerr << "[ShizaProject] get_episodes failed: " << resp.error << "\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    auto eps = root["data"]["release"]["episodes"];
    if (!eps.is_array()) return {};

    std::vector<Episode> episodes;
    for (size_t i = 0; i < eps.size(); ++i) {
        auto item = eps[i];
        Episode ep;
        int num = item["number"].get_int();
        ep.number = num > 0 ? std::to_string(num) : std::to_string(i + 1);
        ep.title = item["name"].get_str();

        auto vids = item["videos"];
        if (vids.is_array()) {
            for (size_t v = 0; v < vids.size(); ++v) {
                std::string src = vids[v]["embedSource"].get_str();
                std::string url = vids[v]["embedUrl"].get_str();
                if (!url.empty()) {
                    if (src.empty()) src = "video_" + std::to_string(v);
                    ep.stream_urls[src] = url;
                    // Prefer Kodik if available as default_url
                    if (!ep.meta.count("default_url") || src == "KODIK") {
                        ep.meta["default_url"] = url;
                    }
                }
            }
        }

        episodes.push_back(std::move(ep));
    }

    return episodes;
}

Stream ShizaProjectProvider::get_stream(const Anime& /*anime*/, const Episode& episode) {
    std::string stream_url;
    if (episode.meta.count("default_url")) {
        stream_url = episode.meta.at("default_url");
    } else if (!episode.stream_urls.empty()) {
        stream_url = episode.stream_urls.begin()->second;
    }

    if (stream_url.empty()) return {};

    // Decode Kodik iframe URL into direct HLS .m3u8 stream
    if (stream_url.find("kodik") != std::string::npos) {
        int ep_num = 1;
        try { ep_num = std::stoi(episode.number); } catch (...) {}
        auto resolved = KodikResolver::resolve(stream_url, ep_num, "https://shizaproject.com/");
        if (!resolved.qualities.empty()) {
            return resolved;
        }
    }

    Stream stream;
    Quality q;
    q.label = "auto";
    q.url = stream_url;
    q.headers["Referer"] = "https://shizaproject.com/";
    q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    stream.qualities.push_back(std::move(q));

    return stream;
}

} // namespace anime
