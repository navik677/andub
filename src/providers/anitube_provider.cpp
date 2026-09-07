#include "anitube_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include <iostream>
#include <regex>
#include <sstream>

namespace anime {

static std::string anitube_url_encode(const std::string& value) {
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

std::vector<Anime> AniTubeProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    std::string search_query = query.empty() ? (genre.empty() ? "аніме" : genre) : query;
    std::string post_data = "do=search&subaction=search&story=" + anitube_url_encode(search_query);
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
        std::cerr << "[AniTube] Search error: " << resp.error << "\n";
        return {};
    }

    std::vector<Anime> results;

    // Pattern: <article class="story">...</article>
    std::regex story_re(R"raw(<article\s+class="story"[\s\S]*?</article>)raw");
    auto words_begin = std::sregex_iterator(resp.body.begin(), resp.body.end(), story_re);
    auto words_end = std::sregex_iterator();

    std::regex title_re(R"raw(<h2[^>]*itemprop="name"><a\s+href="([^"]+)">([^<]+)</a>)raw");
    std::regex poster_re(R"raw(<img[^>]+src="([^"]+)")raw");

    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        std::string story_html = it->str();
        std::smatch tmatch;
        if (!std::regex_search(story_html, tmatch, title_re)) continue;

        Anime a;
        a.id = tmatch[1].str();
        a.meta["url"] = a.id;
        a.title_ru = utils::trim(tmatch[2].str());
        a.provider = name();

        std::smatch pmatch;
        if (std::regex_search(story_html, pmatch, poster_re)) {
            std::string p = pmatch[1].str();
            if (p.rfind("http", 0) != 0) {
                a.poster_url = base_url + p;
            } else {
                a.poster_url = p;
            }
        }

        results.push_back(std::move(a));
        if (static_cast<int>(results.size()) >= limit) break;
    }

    return results;
}

std::vector<Episode> AniTubeProvider::get_episodes(const Anime& anime) {
    std::string page_url = anime.meta.count("url") ? anime.meta.at("url") : anime.id;

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::get(page_url, headers);
    if (!resp.success()) {
        std::cerr << "[AniTube] Failed to load page: " << resp.error << "\n";
        return {};
    }

    // Extract news_id
    std::regex id_re(R"raw(data-news_id="(\d+)")raw");
    std::smatch id_match;
    std::string news_id;
    if (std::regex_search(resp.body, id_match, id_re)) {
        news_id = id_match[1].str();
    } else {
        std::regex url_id_re(R"raw(anitube\.in\.ua/(\d+)-)raw");
        if (std::regex_search(page_url, id_match, url_id_re)) {
            news_id = id_match[1].str();
        }
    }

    // Extract dle_login_hash
    std::regex hash_re(R"raw(dle_login_hash\s*=\s*['"]([a-f0-9]+)['"])raw");
    std::smatch hash_match;
    std::string user_hash;
    if (std::regex_search(resp.body, hash_match, hash_re)) {
        user_hash = hash_match[1].str();
    }

    if (news_id.empty()) {
        // Fallback single episode
        Episode ep;
        ep.number = "1";
        ep.title = "Серія 1";
        ep.meta["page_url"] = page_url;
        return {ep};
    }

    // Fetch playlist via ajax
    std::string playlist_url = base_url + "/engine/ajax/playlists.php";
    std::string post_data = "news_id=" + news_id + "&xfield=playlist";
    if (!user_hash.empty()) {
        post_data += "&user_hash=" + user_hash;
    }

    std::map<std::string, std::string> ajax_headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"X-Requested-With", "XMLHttpRequest"},
        {"Referer", page_url}
    };

    auto pl_resp = http::Client::post(playlist_url, post_data, ajax_headers);
    if (!pl_resp.success()) {
        Episode ep;
        ep.number = "1";
        ep.title = "Серія 1";
        return {ep};
    }

    auto root = json::Value::parse(pl_resp.body);
    std::string html_resp = root["response"].get_str();
    if (html_resp.empty()) html_resp = pl_resp.body;

    std::vector<Episode> episodes;
    // Look for <li data-file="URL" ...>TITLE</li>
    std::regex li_re(R"raw(<li[^>]+data-file="([^"]+)"[^>]*>([^<]+)</li>)raw");
    auto words_begin = std::sregex_iterator(html_resp.begin(), html_resp.end(), li_re);
    auto words_end = std::sregex_iterator();

    int ep_idx = 1;
    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        std::smatch match = *it;
        Episode ep;
        ep.number = std::to_string(ep_idx++);
        ep.title = utils::trim(match[2].str());
        ep.meta["vod_url"] = match[1].str();
        ep.stream_urls["ashdi"] = match[1].str();
        episodes.push_back(std::move(ep));
    }

    if (episodes.empty()) {
        Episode ep;
        ep.number = "1";
        ep.title = "Серія 1";
        episodes.push_back(std::move(ep));
    }

    return episodes;
}

Stream AniTubeProvider::get_stream(const Anime& /*anime*/, const Episode& episode) {
    Stream stream;

    std::string vod_url;
    if (episode.meta.count("vod_url")) {
        vod_url = episode.meta.at("vod_url");
    } else if (episode.stream_urls.count("ashdi")) {
        vod_url = episode.stream_urls.at("ashdi");
    }

    if (vod_url.empty()) return stream;

    // Resolve m3u8 directly from ashdi.vip player page
    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::get(vod_url, headers);
    std::string m3u8_url;
    if (resp.success()) {
        std::regex file_re(R"raw(file:\s*['"](https?://[^'"]+\.m3u8[^'"]*)['"])raw");
        std::smatch m;
        if (std::regex_search(resp.body, m, file_re)) {
            m3u8_url = m[1].str();
        } else {
            std::regex any_m3u8(R"raw((https?://[^\s"'<>]+\.m3u8[^\s"'<>]*))raw");
            if (std::regex_search(resp.body, m, any_m3u8)) {
                m3u8_url = m[1].str();
            }
        }
    }

    if (!m3u8_url.empty()) {
        Quality q;
        q.label = "1080p";
        q.url = m3u8_url;
        q.headers["Referer"] = "https://ashdi.vip/";
        q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
        stream.qualities.push_back(std::move(q));
    }

    // Always keep VOD URL as fallback
    Quality fallback;
    fallback.label = m3u8_url.empty() ? "auto" : "fallback";
    fallback.url = vod_url;
    fallback.headers["Referer"] = base_url + "/";
    fallback.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    stream.qualities.push_back(std::move(fallback));

    return stream;
}

} // namespace anime
