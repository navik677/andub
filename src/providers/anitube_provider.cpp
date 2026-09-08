#include "anitube_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include <iostream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace anime {

static std::string get_anitube_cookie_file() {
    return "/tmp/andub_anitube_cookies.txt";
}

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

static std::string clean_html_tags(const std::string& input) {
    std::string text = std::regex_replace(input, std::regex(R"raw(<br\s*/?>)raw"), "\n");
    text = std::regex_replace(text, std::regex(R"raw(<[^>]+>)raw"), "");
    text = std::regex_replace(text, std::regex(R"raw(&quot;)raw"), "\"");
    text = std::regex_replace(text, std::regex(R"raw(&amp;)raw"), "&");
    text = std::regex_replace(text, std::regex(R"raw(&lt;)raw"), "<");
    text = std::regex_replace(text, std::regex(R"raw(&gt;)raw"), ">");
    text = std::regex_replace(text, std::regex(R"raw(&#39;)raw"), "'");
    text = std::regex_replace(text, std::regex(R"raw(&nbsp;)raw"), " ");
    return utils::trim(text);
}

static std::string get_genre_slug(const std::string& genre) {
    if (genre.empty()) return "";
    std::string lower = utils::utf8_tolower(genre);

    static const std::unordered_map<std::string, std::string> mapping = {
        {"комедия", "comedy"}, {"комедія", "comedy"},
        {"боевик", "action"}, {"бойовик", "action"}, {"экшен", "action"},
        {"боевые искусства", "fight"}, {"бойове мистецтво", "fight"},
        {"романтика", "romance"},
        {"фэнтези", "fantasy"}, {"фентезі", "fantasy"},
        {"фантастика", "fantaskyka"},
        {"драма", "drama"},
        {"школа", "school-life"},
        {"сёнен", "shounen"}, {"сьонен", "shounen"}, {"шьонен", "shounen"},
        {"исекай", "isekai"}, {"ісекай", "isekai"},
        {"детектив", "detective"},
        {"ужасы", "horror"}, {"жахи", "horror"},
        {"приключения", "prigodi"}, {"пригоди", "prigodi"},
        {"спорт", "sport"},
        {"повседневность", "routine"}, {"буденність", "routine"},
        {"мистика", "mystic"}, {"містика", "mystic"},
        {"киберпанк", "cyberpunk"}, {"кіберпанк", "cyberpunk"},
        {"сейнен", "seinen"},
        {"сёдзе", "shoujo"}, {"сьодзе", "shoujo"}, {"шьоджьо", "shoujo"},
        {"меха", "mecha"},
        {"этти", "echi"}, {"еччі", "echi"},
        {"музыка", "music"}, {"музичний", "music"},
        {"пародия", "parodya"}, {"пародія", "parodya"},
        {"сверхъестественное", "supernatural"}, {"надприродне", "supernatural"},
        {"антиутопия", "dystopia"}, {"антиутопія", "dystopia"},
        {"война", "war"}, {"війна", "war"},
        {"исторический", "story"}, {"історія", "story"}
    };

    auto it = mapping.find(lower);
    if (it != mapping.end()) {
        return it->second;
    }
    return "";
}

std::vector<Anime> AniTubeProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    std::string target_url;
    std::string post_data;
    bool is_post = false;

    std::string genre_slug = get_genre_slug(genre);

    if (query.empty()) {
        // Direct GET requests avoid DLE search flood protection ("працює через раз") completely!
        if (!genre_slug.empty()) {
            if (page > 1) {
                target_url = base_url + "/anime/" + genre_slug + "/page/" + std::to_string(page) + "/";
            } else {
                target_url = base_url + "/anime/" + genre_slug + "/";
            }
        } else if (genre.empty() || genre == "Все жанры") {
            if (page > 1) {
                target_url = base_url + "/anime/page/" + std::to_string(page) + "/";
            } else {
                target_url = base_url + "/anime/";
            }
        } else {
            // Genre not in slug mapping, use POST search with genre name
            target_url = base_url + "/index.php?do=search";
            post_data = "do=search&subaction=search&story=" + anitube_url_encode(genre);
            if (page > 1) {
                post_data += "&search_start=" + std::to_string(page);
            }
            is_post = true;
        }
    } else {
        // Search query provided
        target_url = base_url + "/index.php?do=search";
        post_data = "do=search&subaction=search&story=" + anitube_url_encode(query);
        if (page > 1) {
            post_data += "&search_start=" + std::to_string(page);
        }
        is_post = true;
    }

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"},
        {"Referer", base_url + "/"}
    };
    if (is_post) {
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    }

    std::string cookie_file = get_anitube_cookie_file();
    http::Response resp;
    if (is_post) {
        resp = http::Client::post(target_url, post_data, headers, 15, cookie_file);
    } else {
        resp = http::Client::get(target_url, headers, 15, cookie_file);
    }

    if (!resp.success()) {
        std::cerr << "[AniTube] Search error: " << resp.error << " (status " << resp.status_code << ")\n";
        return {};
    }

    std::vector<Anime> results;

    // Pattern: <article class="story">...</article>
    std::regex story_re(R"raw(<article\s+class="story"[\s\S]*?</article>)raw");
    auto words_begin = std::sregex_iterator(resp.body.begin(), resp.body.end(), story_re);
    auto words_end = std::sregex_iterator();

    std::regex title_re(R"raw(<h2[^>]*itemprop="name"><a\s+href="([^"]+)">([^<]+)</a>)raw");
    // Match data-src first (AniTube uses lazy load with spacer.gif in src)
    std::regex data_src_re(R"raw(data-src="([^"]+)")raw");
    std::regex src_re(R"raw(src="([^"]+)")raw");
    std::regex year_re(R"raw((?:Рік виходу[^<]*:|year/)\s*(?:<[^>]+>)?\s*(\d{4}))raw");
    std::regex rating_re(R"raw(<span>([\d.]+)</span>/<span>10</span>)raw");
    std::regex cat_re(R"raw(<dt>Категорія:</dt>([^<]+))raw");

    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        std::string story_html = it->str();
        std::smatch tmatch;
        if (!std::regex_search(story_html, tmatch, title_re)) continue;

        Anime a;
        a.id = tmatch[1].str();
        a.meta["url"] = a.id;
        a.title_ru = utils::trim(tmatch[2].str());
        a.provider = name();

        // Extract poster URL
        std::smatch pmatch;
        std::string poster;
        if (std::regex_search(story_html, pmatch, data_src_re)) {
            poster = pmatch[1].str();
        } else if (std::regex_search(story_html, pmatch, src_re)) {
            std::string candidate = pmatch[1].str();
            if (candidate.find("spacer.gif") == std::string::npos) {
                poster = candidate;
            }
        }

        if (!poster.empty()) {
            if (poster.rfind("http", 0) != 0) {
                a.poster_url = base_url + poster;
            } else {
                a.poster_url = poster;
            }
        }

        // Year
        std::smatch ymatch;
        if (std::regex_search(story_html, ymatch, year_re)) {
            try { a.year = std::stoi(ymatch[1].str()); } catch (...) {}
        }

        // Rating
        std::smatch rmatch;
        if (std::regex_search(story_html, rmatch, rating_re)) {
            a.rating = rmatch[1].str();
        }

        // Genres
        std::smatch cmatch;
        if (std::regex_search(story_html, cmatch, cat_re)) {
            std::string cat_str = utils::trim(cmatch[1].str());
            std::stringstream ss(cat_str);
            std::string g;
            while (std::getline(ss, g, ',')) {
                std::string trimmed = utils::trim(g);
                if (!trimmed.empty()) a.genres.push_back(trimmed);
            }
        }

        results.push_back(std::move(a));
        if (static_cast<int>(results.size()) >= limit) break;
    }

    return results;
}

Anime AniTubeProvider::get_details(const Anime& anime) {
    Anime result = anime;
    std::string page_url = result.meta.count("url") ? result.meta.at("url") : result.id;
    if (page_url.empty()) return result;

    std::string cookie_file = get_anitube_cookie_file();
    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::get(page_url, headers, 15, cookie_file);
    if (!resp.success()) return result;

    // Parse description
    std::regex desc_re(R"raw(<div[^>]*class="story_c_text"[^>]*>([\s\S]*?)</div>)raw");
    std::smatch dmatch;
    if (std::regex_search(resp.body, dmatch, desc_re)) {
        result.description = clean_html_tags(dmatch[1].str());
    }

    // Parse poster if missing
    if (result.poster_url.empty()) {
        std::regex p_re(R"raw(<div class="story_c_l">[\s\S]*?data-src="([^"]+)")raw");
        std::smatch pmatch;
        if (std::regex_search(resp.body, pmatch, p_re)) {
            std::string p = pmatch[1].str();
            result.poster_url = (p.rfind("http", 0) != 0) ? (base_url + p) : p;
        }
    }

    return result;
}

std::vector<Episode> AniTubeProvider::get_episodes(const Anime& anime) {
    std::string page_url = anime.meta.count("url") ? anime.meta.at("url") : anime.id;
    std::string cookie_file = get_anitube_cookie_file();

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"},
        {"Referer", base_url + "/"}
    };

    // GET page_url using cookie_file: sets up PHPSESSID required by DLE playlists.php
    auto resp = http::Client::get(page_url, headers, 15, cookie_file);
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
        Episode ep;
        ep.number = "1";
        ep.title = "Серія 1";
        ep.meta["page_url"] = page_url;
        return {ep};
    }

    // Fetch playlist via ajax with session cookie
    std::string playlist_url = base_url + "/engine/ajax/playlists.php";
    std::string post_data = "news_id=" + news_id + "&xfield=playlist";
    if (!user_hash.empty()) {
        post_data += "&user_hash=" + user_hash;
    }

    std::map<std::string, std::string> ajax_headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"},
        {"X-Requested-With", "XMLHttpRequest"},
        {"Referer", page_url}
    };

    auto pl_resp = http::Client::post(playlist_url, post_data, ajax_headers, 15, cookie_file);
    if (!pl_resp.success()) {
        Episode ep;
        ep.number = "1";
        ep.title = "Серія 1";
        return {ep};
    }

    std::string html_resp;
    try {
        auto root = json::Value::parse(pl_resp.body);
        html_resp = root["response"].get_str();
    } catch (...) {}
    if (html_resp.empty()) html_resp = pl_resp.body;

    std::vector<Episode> episodes;

    // Map data-id prefixes to categories (e.g., 0_1 = Озвучення, 0_0 = Субтитри, 0_1_0 = Студія)
    std::unordered_map<std::string, std::string> cat_names;
    std::regex cat_item_re(R"raw(<li\s+data-id="([^"]+)">([^<]+)</li>)raw");
    auto c_begin = std::sregex_iterator(html_resp.begin(), html_resp.end(), cat_item_re);
    auto c_end = std::sregex_iterator();
    for (auto it = c_begin; it != c_end; ++it) {
        cat_names[(*it)[1].str()] = utils::trim((*it)[2].str());
    }

    // Parse all video entries: <li data-file="URL" data-id="ID" ...>TITLE</li>
    struct RawEntry {
        std::string url;
        std::string data_id;
        std::string raw_title;
    };
    std::vector<RawEntry> raw_vids;

    std::regex li_re(R"raw(<li[^>]+data-file="([^"]+)"[^>]*data-id="([^"]+)"[^>]*>([^<]+)</li>)raw");
    auto words_begin = std::sregex_iterator(html_resp.begin(), html_resp.end(), li_re);
    auto words_end = std::sregex_iterator();
    for (auto it = words_begin; it != words_end; ++it) {
        raw_vids.push_back({(*it)[1].str(), (*it)[2].str(), utils::trim((*it)[3].str())});
    }

    // Fallback if data-id attribute was before data-file
    if (raw_vids.empty()) {
        std::regex alt_li_re(R"raw(<li[^>]+data-id="([^"]+)"[^>]*data-file="([^"]+)"[^>]*>([^<]+)</li>)raw");
        auto a_begin = std::sregex_iterator(html_resp.begin(), html_resp.end(), alt_li_re);
        auto a_end = std::sregex_iterator();
        for (auto it = a_begin; it != a_end; ++it) {
            raw_vids.push_back({(*it)[2].str(), (*it)[1].str(), utils::trim((*it)[3].str())});
        }
    }

    // Check if ashdi.vip is available
    bool has_ashdi = false;
    for (const auto& v : raw_vids) {
        if (v.url.find("ashdi.vip") != std::string::npos) {
            has_ashdi = true;
            break;
        }
    }

    // Deduplicate: key = (is_voiceover, studio_name, raw_title)
    struct GroupedEp {
        bool is_voiceover = false;
        std::string studio_name;
        std::string raw_title;
        std::string url;
        std::string data_id;
        int ep_num = 0;
    };
    std::vector<GroupedEp> grouped;
    std::unordered_map<std::string, size_t> seen_index;
    std::unordered_set<std::string> unique_studios;

    for (const auto& v : raw_vids) {
        // Skip moonanime if ashdi exists because moonanime currently errors with 400
        if (has_ashdi && v.url.find("moonanime.art") != std::string::npos) {
            continue;
        }

        // Split data_id by '_'
        std::vector<std::string> parts;
        std::stringstream ss(v.data_id);
        std::string segment;
        while (std::getline(ss, segment, '_')) {
            parts.push_back(segment);
        }

        std::string type_id = (parts.size() >= 2) ? (parts[0] + "_" + parts[1]) : "";
        std::string studio_id = (parts.size() >= 3) ? (parts[0] + "_" + parts[1] + "_" + parts[2]) : "";

        bool is_voice = (type_id == "0_1");
        if (cat_names.count(type_id)) {
            std::string tname = utils::utf8_tolower(cat_names[type_id]);
            if (tname.find("озвуч") != std::string::npos) is_voice = true;
            else if (tname.find("субтит") != std::string::npos) is_voice = false;
        }

        std::string studio_name;
        if (cat_names.count(studio_id)) {
            studio_name = cat_names[studio_id];
        }
        if (!studio_name.empty()) {
            unique_studios.insert(studio_name);
        }

        // Extract numeric episode number
        int ep_num = 0;
        std::smatch nm;
        std::regex num_re(R"raw(\d+)raw");
        if (std::regex_search(v.raw_title, nm, num_re)) {
            try { ep_num = std::stoi(nm[0].str()); } catch (...) {}
        }

        std::string group_key = std::string(is_voice ? "1|" : "0|") + studio_name + "|" + v.raw_title;
        auto sit = seen_index.find(group_key);
        if (sit != seen_index.end()) {
            // Already seen: if current candidate is ashdi.vip and existing was not, replace
            if (v.url.find("ashdi.vip") != std::string::npos && grouped[sit->second].url.find("ashdi.vip") == std::string::npos) {
                grouped[sit->second].url = v.url;
                grouped[sit->second].data_id = v.data_id;
            }
        } else {
            seen_index[group_key] = grouped.size();
            grouped.push_back({is_voice, studio_name, v.raw_title, v.url, v.data_id, ep_num});
        }
    }

    // Sort: voiceover first (true > false), then studio_name, then ep_num
    std::stable_sort(grouped.begin(), grouped.end(), [](const GroupedEp& a, const GroupedEp& b) {
        if (a.is_voiceover != b.is_voiceover) return a.is_voiceover > b.is_voiceover;
        if (a.studio_name != b.studio_name) return a.studio_name < b.studio_name;
        return a.ep_num < b.ep_num;
    });

    for (size_t i = 0; i < grouped.size(); ++i) {
        const auto& g = grouped[i];
        Episode ep;
        ep.number = (g.ep_num > 0) ? std::to_string(g.ep_num) : std::to_string(i + 1);

        std::string title = g.raw_title;
        if (!g.studio_name.empty() && unique_studios.size() > 1) {
            title += " — " + g.studio_name;
        }
        title += g.is_voiceover ? " (Озвучення)" : " (Субтитри)";

        ep.title = title;
        ep.meta["vod_url"] = g.url;
        ep.meta["page_url"] = page_url;
        ep.stream_urls["ashdi"] = g.url;
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
        {"User-Agent", "Mozilla/5.0"},
        {"Referer", base_url + "/"}
    };

    auto resp = http::Client::get(vod_url, headers, 15);
    std::string master_m3u8;
    if (resp.success()) {
        std::regex file_re(R"raw(file:\s*['"](https?://[^'"]+\.m3u8[^'"]*)['"])raw");
        std::smatch m;
        if (std::regex_search(resp.body, m, file_re)) {
            master_m3u8 = m[1].str();
        } else {
            std::regex any_m3u8(R"raw((https?://[^\s"'<>]+\.m3u8[^\s"'<>]*))raw");
            if (std::regex_search(resp.body, m, any_m3u8)) {
                master_m3u8 = m[1].str();
            }
        }
    }

    if (!master_m3u8.empty()) {
        // Fetch master playlist to parse individual qualities (1080p, 720p, 480p)
        std::map<std::string, std::string> ashdi_headers = {
            {"Referer", "https://ashdi.vip/"},
            {"User-Agent", "Mozilla/5.0"}
        };
        auto m3u8_resp = http::Client::get(master_m3u8, ashdi_headers, 10);
        bool parsed_substreams = false;
        if (m3u8_resp.success()) {
            std::stringstream mss(m3u8_resp.body);
            std::string line;
            std::string current_res;
            while (std::getline(mss, line)) {
                line = utils::trim(line);
                if (line.rfind("#EXT-X-STREAM-INF:", 0) == 0) {
                    std::smatch rm;
                    std::regex res_re(R"raw(RESOLUTION=\d+x(\d+))raw");
                    if (std::regex_search(line, rm, res_re)) {
                        current_res = rm[1].str() + "p";
                    }
                } else if (!line.empty() && line[0] != '#') {
                    std::string stream_url = line;
                    if (stream_url.rfind("http", 0) != 0) {
                        auto slash = master_m3u8.rfind('/');
                        if (slash != std::string::npos) {
                            stream_url = master_m3u8.substr(0, slash + 1) + stream_url;
                        }
                    }
                    Quality q;
                    q.label = current_res.empty() ? "HD" : current_res;
                    q.url = stream_url;
                    q.headers["Referer"] = "https://ashdi.vip/";
                    stream.qualities.push_back(std::move(q));
                    parsed_substreams = true;
                    current_res.clear();
                }
            }
        }

        // Add master playlist
        Quality master_q;
        master_q.label = parsed_substreams ? "Auto" : "1080p";
        master_q.url = master_m3u8;
        master_q.headers["Referer"] = "https://ashdi.vip/";
        if (!parsed_substreams) {
            stream.qualities.push_back(std::move(master_q));
        } else {
            stream.qualities.insert(stream.qualities.begin(), std::move(master_q));
        }
    }

    // Keep VOD URL as fallback
    Quality fallback;
    fallback.label = stream.qualities.empty() ? "auto" : "fallback";
    fallback.url = vod_url;
    fallback.headers["Referer"] = base_url + "/";
    stream.qualities.push_back(std::move(fallback));

    return stream;
}

} // namespace anime
