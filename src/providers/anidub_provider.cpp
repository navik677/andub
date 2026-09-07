#include "anidub_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include <iostream>
#include <regex>
#include <sstream>

namespace anime {

static std::string anidub_url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << (int)(unsigned char)c;
        }
    }
    return escaped.str();
}

std::vector<Anime> AniDubProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    std::string search_query = query.empty() ? (genre.empty() ? "аниме" : genre) : query;
    std::string post_data = "do=search&subaction=search&story=" + anidub_url_encode(search_query);
    if (page > 1) {
        post_data += "&search_start=" + std::to_string(page);
    }

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::post(base_url + "/index.php?do=search", post_data, headers);
    if (!resp.success()) {
        std::cerr << "[AniDub] Search request error: " << resp.error << "\n";
        return {};
    }

    std::vector<Anime> results;

    // Matches <a class="th-in" href="LINK">...<img ... src="POSTER"...>...<div class="th-title">TITLE</div>
    std::regex item_re(R"raw(<a\s+class="th-in"[^>]*href="([^"]+)"[^>]*>([\s\S]*?)<div\s+class="th-title">([^<]+)</div>)raw");
    auto words_begin = std::sregex_iterator(resp.body.begin(), resp.body.end(), item_re);
    auto words_end = std::sregex_iterator();

    std::regex img_re(R"raw(<img[^>]+src="([^"]+)")raw");

    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        std::smatch match = *it;
        Anime a;
        a.id = match[1].str();
        a.meta["url"] = a.id;
        a.provider = name();

        std::string inner_html = match[2].str();
        std::string raw_title = match[3].str();

        // Extract poster
        std::smatch img_match;
        if (std::regex_search(inner_html, img_match, img_re)) {
            std::string poster = img_match[1].str();
            if (poster.rfind("http", 0) != 0) {
                a.poster_url = base_url + poster;
            } else {
                a.poster_url = poster;
            }
        }

        // Clean and split title: "Title RU / Title EN [episodes]"
        size_t slash_pos = raw_title.find('/');
        if (slash_pos != std::string::npos) {
            a.title_ru = raw_title.substr(0, slash_pos);
            a.title_en = raw_title.substr(slash_pos + 1);
        } else {
            a.title_ru = raw_title;
        }

        // Trim title strings
        a.title_ru = utils::trim(a.title_ru);
        a.title_en = utils::trim(a.title_en);

        results.push_back(std::move(a));
        if (static_cast<int>(results.size()) >= limit) break;
    }

    return results;
}

std::vector<Episode> AniDubProvider::get_episodes(const Anime& anime) {
    std::string page_url = anime.meta.count("url") ? anime.meta.at("url") : anime.id;

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::get(page_url, headers);
    if (!resp.success()) {
        std::cerr << "[AniDub] Failed to load episode page: " << resp.error << "\n";
        return {};
    }

    std::vector<Episode> episodes;

    // Pattern: <span data="(https?://video.sibnet.ru/[^"]+)">([^<]+)</span>
    std::regex tab_re(R"raw(<span\s+data="([^"]*video\.sibnet\.ru[^"]*)">([^<]+)</span>)raw");
    auto words_begin = std::sregex_iterator(resp.body.begin(), resp.body.end(), tab_re);
    auto words_end = std::sregex_iterator();

    int ep_idx = 1;
    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        std::smatch match = *it;
        Episode ep;
        std::string vid_url = match[1].str();
        if (vid_url.rfind("//", 0) == 0) vid_url = "https:" + vid_url;
        else if (vid_url.rfind("http", 0) != 0) vid_url = "https://" + vid_url;

        ep.number = std::to_string(ep_idx++);
        ep.title = utils::trim(match[2].str());
        ep.meta["sibnet_url"] = vid_url;
        ep.stream_urls["sibnet"] = vid_url;
        episodes.push_back(std::move(ep));
    }

    // Fallback if no series tabs found
    if (episodes.empty()) {
        std::regex single_sibnet_re(R"raw((?:https?:)?//(video\.sibnet\.ru/shell\.php\?videoid=\d+))raw");
        std::smatch sm;
        if (std::regex_search(resp.body, sm, single_sibnet_re)) {
            Episode ep;
            ep.number = "1";
            ep.title = "Серія 1";
            std::string vid_url = "https://" + sm[1].str();
            ep.meta["sibnet_url"] = vid_url;
            ep.stream_urls["sibnet"] = vid_url;
            episodes.push_back(std::move(ep));
        }
    }

    return episodes;
}

Stream AniDubProvider::get_stream(const Anime& /*anime*/, const Episode& episode) {
    Stream stream;

    std::string sibnet_url;
    if (episode.meta.count("sibnet_url")) {
        sibnet_url = episode.meta.at("sibnet_url");
    } else if (episode.stream_urls.count("sibnet")) {
        sibnet_url = episode.stream_urls.at("sibnet");
    }

    if (sibnet_url.empty()) return stream;

    // Resolve direct mp4 link from Sibnet
    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::get(sibnet_url, headers);
    std::string direct_mp4;
    if (resp.success()) {
        std::regex mp4_re(R"raw((/v/[a-zA-Z0-9_/]+\.mp4))raw");
        std::smatch m;
        if (std::regex_search(resp.body, m, mp4_re)) {
            direct_mp4 = "https://video.sibnet.ru" + m[1].str();
        }
    }

    if (!direct_mp4.empty()) {
        Quality q;
        q.label = "720p";
        q.url = direct_mp4;
        q.headers["Referer"] = "https://video.sibnet.ru/";
        q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
        stream.qualities.push_back(std::move(q));
    }

    // Always provide Sibnet shell URL as fallback
    Quality fallback;
    fallback.label = direct_mp4.empty() ? "auto" : "fallback";
    fallback.url = sibnet_url;
    fallback.headers["Referer"] = base_url + "/";
    fallback.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    stream.qualities.push_back(std::move(fallback));

    return stream;
}

} // namespace anime
