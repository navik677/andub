#include "shizaproject_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include "../utils/kodik_resolver.hpp"
#include <iostream>

namespace anime {

std::vector<Anime> ShizaProjectProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    json::Value req_body;
    std::string q;

    int fetch_count = (page > 1) ? (limit * page) : limit;

    if (!query.empty()) {
        q = "query { releases(query: \"" + query + "\", first: " + std::to_string(fetch_count) + ") { edges { node { id name originalName slug description posters { preview: resize(width: 360, height: 500) { url } original { url } } genres { name } } } } }";
    } else {
        q = "query { releases(first: " + std::to_string(fetch_count) + ") { edges { node { id name originalName slug description posters { preview: resize(width: 360, height: 500) { url } original { url } } genres { name } } } } }";
    }

    req_body["query"] = q;

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
    };

    auto resp = http::Client::post(graphql_url, req_body.dump(), headers);
    if (!resp.success()) {
        std::cerr << "[ShizaProject] Search request failed: " << resp.error << " (status: " << resp.status_code << ")\n";
        return {};
    }

    auto root = json::Value::parse(resp.body);
    auto edges = root["data"]["releases"]["edges"];
    if (!edges.is_array()) return {};

    std::vector<Anime> results;
    std::string lower_genre = utils::utf8_tolower(genre);
    size_t skip_count = (page > 1) ? static_cast<size_t>((page - 1) * limit) : 0;
    size_t matched_count = 0;

    for (size_t i = 0; i < edges.size(); ++i) {
        auto node = edges[i]["node"];
        Anime a;
        a.id = node["slug"].get_str();
        if (a.id.empty()) a.id = node["id"].get_str();
        a.title_ru = node["name"].get_str();
        a.title_en = node["originalName"].get_str();
        a.description = node["description"].get_str();
        a.provider = name();
        a.meta["slug"] = node["slug"].get_str();

        auto posters = node["posters"];
        if (posters.is_array() && posters.size() > 0) {
            std::string preview_url = posters[0]["preview"]["url"].get_str();
            if (!preview_url.empty()) {
                a.poster_url = preview_url;
            } else {
                a.poster_url = posters[0]["original"]["url"].get_str();
            }
        }

        auto genres = node["genres"];
        bool matches_genre = genre.empty();
        if (genres.is_array()) {
            for (size_t g = 0; g < genres.size(); ++g) {
                std::string gname = genres[g]["name"].get_str();
                if (!gname.empty()) {
                    a.genres.push_back(gname);
                    if (!matches_genre && utils::utf8_tolower(gname).find(lower_genre) != std::string::npos) {
                        matches_genre = true;
                    }
                }
            }
        }

        if (!matches_genre) continue;

        matched_count++;
        if (matched_count <= skip_count) continue;

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
        auto resolved = KodikResolver::resolve(stream_url, ep_num);
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
