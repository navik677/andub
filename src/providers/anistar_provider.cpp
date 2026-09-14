#include "anistar_provider.hpp"
#include "../utils/http_client.hpp"
#include "../utils/str_utils.hpp"
#include <iostream>
#include <regex>
#include <algorithm>
#include <set>

namespace anime {

std::string AniStarProvider::s_cookie = "l7_browser=fxgfCS8T5Ez-wqEvekGhiKULOD4PPUH7fDPe30yd-ww";

void AniStarProvider::update_cookie_from_html(const std::string& html) {
    std::regex re(R"raw(document\.cookie\s*=\s*"(l7_browser=[^;"]+))raw");
    std::smatch m;
    if (std::regex_search(html, m, re)) {
        s_cookie = m[1].str();
        std::cout << "[AniStar] Updated challenge cookie: " << s_cookie << std::endl;
        return;
    }
    std::regex re2(R"raw(l7_browser=([a-zA-Z0-9_\-]+))raw");
    if (std::regex_search(html, m, re2)) {
        s_cookie = "l7_browser=" + m[1].str();
        std::cout << "[AniStar] Extracted challenge cookie: " << s_cookie << std::endl;
    }
}

std::string AniStarProvider::fetch_page(const std::string& url, const std::string& referer) {
    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"},
        {"Referer", referer}
    };
    if (!s_cookie.empty()) {
        headers["Cookie"] = s_cookie;
    }

    auto resp = http::Client::get(url, headers);
    if (resp.body.find("Проверяем ваш браузер") != std::string::npos ||
        resp.body.find("document.cookie") != std::string::npos) {
        update_cookie_from_html(resp.body);
        if (!s_cookie.empty()) {
            headers["Cookie"] = s_cookie;
            resp = http::Client::get(url, headers);
        }
    }
    return resp.body;
}

static std::string clean_html_tags(const std::string& input) {
    std::string text = std::regex_replace(input, std::regex(R"raw(<p class="reason">[\s\S]*?</p>)raw"), "");
    text = std::regex_replace(text, std::regex(R"raw(<br\s*/?>)raw"), "\n");
    text = std::regex_replace(text, std::regex(R"raw(<[^>]+>)raw"), "");
    text = std::regex_replace(text, std::regex(R"raw(&quot;)raw"), "\"");
    text = std::regex_replace(text, std::regex(R"raw(&amp;)raw"), "&");
    text = std::regex_replace(text, std::regex(R"raw(&lt;)raw"), "<");
    text = std::regex_replace(text, std::regex(R"raw(&gt;)raw"), ">");
    text = std::regex_replace(text, std::regex(R"raw(&#39;)raw"), "'");
    text = std::regex_replace(text, std::regex(R"raw(&nbsp;)raw"), " ");
    return utils::trim(text);
}

std::vector<Anime> AniStarProvider::search(const std::string& query, int limit, const std::string& genre, int page) {
    std::string url;
    bool is_hentai = utils::utf8_contains_ci(genre, "хентай") || utils::utf8_contains_ci(genre, "hentai");

    if (is_hentai) {
        if (query.empty()) {
            url = (page > 1) ? ("https://v30.astar.bz/hentai/page/" + std::to_string(page) + "/") : "https://v30.astar.bz/hentai/";
        } else {
            url = "https://v30.astar.bz/index.php?do=search&subaction=search&story=" + utils::cp1251_url_encode(query);
            if (page > 1) {
                url += "&search_start=" + std::to_string(page);
            }
        }
    } else if (!query.empty()) {
        url = "https://v30.astar.bz/index.php?do=search&subaction=search&story=" + utils::cp1251_url_encode(query);
        if (page > 1) {
            url += "&search_start=" + std::to_string(page);
        }
    } else if (!genre.empty() && genre != "Все жанры") {
        url = "https://v30.astar.bz/filter/janrs/" + utils::cp1251_url_encode(genre) + "/";
        if (page > 1) {
            url += "page/" + std::to_string(page) + "/";
        }
    } else {
        url = (page > 1) ? ("https://v30.astar.bz/anime/page/" + std::to_string(page) + "/") : "https://v30.astar.bz/anime/";
    }

    std::string raw_html = fetch_page(url);
    std::string html = utils::cp1251_to_utf8(raw_html);

    std::vector<Anime> results;
    std::regex item_re(R"raw(<div\s+class="news"[\s\S]*?(?=<div\s+class="news"|<div\s+class="pagenav"|$))raw");
    auto it = std::sregex_iterator(html.begin(), html.end(), item_re);
    auto end = std::sregex_iterator();

    for (; it != end && (int)results.size() < limit; ++it) {
        std::string block = it->str();

        std::regex title_re(R"raw(<div\s+class="title_left"><a\s+href="([^"]+)"[^>]*>([\s\S]*?)</a>)raw");
        std::smatch tm;
        if (!std::regex_search(block, tm, title_re)) continue;

        std::string item_url = tm[1].str();
        std::string full_title = clean_html_tags(tm[2].str());

        // Skip ads or non-release posts
        if (item_url.find("igra") != std::string::npos) continue;
        std::regex id_re(R"raw(/(\d+)-)raw");
        std::smatch id_m;
        if (!std::regex_search(item_url, id_m, id_re)) continue;

        std::string news_id = id_m[1].str();

        Anime a;
        a.id = news_id;
        a.provider = name();
        a.meta["url"] = item_url;
        a.meta["news_id"] = news_id;

        auto slash_pos = full_title.find(" / ");
        if (slash_pos != std::string::npos) {
            a.title_ru = utils::trim(full_title.substr(0, slash_pos));
            a.title_en = utils::trim(full_title.substr(slash_pos + 3));
        } else {
            a.title_ru = full_title;
        }

        // Poster
        std::regex img_re(R"raw(<img[^>]+(?:class="main-img"[^>]+src="([^"]+)"|src="([^"]+)"[^>]+class="main-img"|itemprop="image"[^>]+src="([^"]+)"|src="([^"]+)"[^>]+itemprop="image"))raw");
        std::smatch im;
        if (std::regex_search(block, im, img_re)) {
            std::string p;
            for (size_t i = 1; i < im.size(); ++i) {
                if (im[i].matched) {
                    p = im[i].str();
                    break;
                }
            }
            if (!p.empty()) {
                if (p.rfind("http", 0) != 0) {
                    p = "https://v30.astar.bz" + p;
                }
                a.poster_url = p;
            }
        }

        // Year
        std::regex year_re(R"raw(Год выпуска:[\s\S]*?(\d{4}))raw");
        std::smatch ym;
        if (std::regex_search(block, ym, year_re)) {
            try {
                a.year = std::stoi(ym[1].str());
            } catch (...) {}
        }

        // Rating
        std::regex rat_re(R"raw(itemprop="ratingValue">([^<]+)<)raw");
        std::smatch rm;
        if (std::regex_search(block, rm, rat_re)) {
            a.rating = rm[1].str();
        }

        // Status / Episodes
        std::regex ep_re(R"raw(Серии:[\s\S]*?</b>([^<]+)<)raw");
        std::smatch em;
        if (std::regex_search(block, em, ep_re)) {
            a.status = clean_html_tags(em[1].str());
        }

        // Genres & Categories
        std::set<std::string> genres_set;
        std::regex tags_re(R"raw(<p class="tags">([\s\S]*?)</p>)raw");
        std::smatch tag_m;
        if (std::regex_search(block, tag_m, tags_re)) {
            std::string tags_content = tag_m[1].str();
            std::regex a_re(R"raw(>([^<]+)</a>)raw");
            auto ga_it = std::sregex_iterator(tags_content.begin(), tags_content.end(), a_re);
            for (; ga_it != std::sregex_iterator(); ++ga_it) {
                std::string g = clean_html_tags((*ga_it)[1].str());
                if (!g.empty() && g != "Аниме") genres_set.insert(g);
            }
        }

        std::regex janrs_re(R"raw(<b>Жанр:[\s\S]*?</b>([\s\S]*?)</li>)raw");
        std::smatch jm;
        if (std::regex_search(block, jm, janrs_re)) {
            std::string janrs_content = jm[1].str();
            std::regex a_re(R"raw(>([^<]+)</a>)raw");
            auto ga_it = std::sregex_iterator(janrs_content.begin(), janrs_content.end(), a_re);
            for (; ga_it != std::sregex_iterator(); ++ga_it) {
                std::string g = clean_html_tags((*ga_it)[1].str());
                if (!g.empty()) genres_set.insert(g);
            }
        }

        for (const auto& g : genres_set) {
            a.genres.push_back(g);
        }

        // If filtering by genre, ensure item matches
        if (!genre.empty() && genre != "Все жанры") {
            bool matches = false;
            for (const auto& g : a.genres) {
                if (utils::utf8_contains_ci(g, genre)) {
                    matches = true;
                    break;
                }
            }
            if (!matches) continue;
        }

        // Description
        std::regex desc_re(R"raw(<div\s+class="descripts">([\s\S]*?)</div>)raw");
        std::smatch dm;
        if (std::regex_search(block, dm, desc_re)) {
            a.description = clean_html_tags(dm[1].str());
        }

        results.push_back(a);
    }

    return results;
}

std::vector<Episode> AniStarProvider::get_episodes(const Anime& anime) {
    std::string news_id = anime.meta.count("news_id") ? anime.meta.at("news_id") : anime.id;
    if (news_id.find('/') != std::string::npos || news_id.find('-') != std::string::npos) {
        std::regex id_re(R"raw(/(\d+)-)raw");
        std::smatch id_m;
        if (std::regex_search(news_id, id_m, id_re)) {
            news_id = id_m[1].str();
        }
    }

    std::string referer = anime.meta.count("url") ? anime.meta.at("url") : "https://v30.astar.bz/";
    std::string player_url = "https://v30.astar.bz/test/player2/videoas_p2p_new.php?id=" + news_id;
    std::string p_html = fetch_page(player_url, referer);

    std::vector<Episode> episodes;
    std::regex ep_re(R"raw(\{\s*title:"([^"]+)"([\s\S]*?files_mp4:\s*\[[\s\S]*?\]))raw");
    auto ep_it = std::sregex_iterator(p_html.begin(), p_html.end(), ep_re);
    auto ep_end = std::sregex_iterator();

    int idx = 1;
    for (; ep_it != ep_end; ++ep_it, ++idx) {
        std::string ep_title = (*ep_it)[1].str();
        std::string ep_body = (*ep_it)[2].str();

        Episode ep;
        ep.title = ep_title;
        ep.meta["news_id"] = news_id;
        ep.meta["referer"] = "https://v30.astar.bz/";

        // Number extraction
        std::regex num_re(R"raw((\d+))raw");
        std::smatch nm;
        if (std::regex_search(ep_title, nm, num_re)) {
            ep.number = nm[1].str();
        } else {
            ep.number = std::to_string(idx);
        }

        // Skip ad opening
        std::regex ad_re(R"raw(to_skeep_ad:\s*\[\["(\d+)","(\d+)"\]\])raw");
        std::smatch adm;
        if (std::regex_search(ep_body, adm, ad_re)) {
            try {
                ep.opening_skip = {std::stoi(adm[1].str()), std::stoi(adm[2].str())};
            } catch (...) {}
        }

        // Direct MP4 streams (sfhd.an-media.org)
        std::regex mp4_block_re(R"raw(files_mp4:\s*\[([\s\S]*?)\])raw");
        std::smatch mp4_m;
        if (std::regex_search(ep_body, mp4_m, mp4_block_re)) {
            std::string mp4_content = mp4_m[1].str();
            std::regex file_re(R"raw(title:"([^"]+)",\s*file:"([^"]+)")raw");
            auto f_it = std::sregex_iterator(mp4_content.begin(), mp4_content.end(), file_re);
            for (; f_it != std::sregex_iterator(); ++f_it) {
                std::string q = (*f_it)[1].str();
                std::string f = (*f_it)[2].str();
                if (q == "720") ep.stream_urls["720p"] = f;
                else if (q == "360") ep.stream_urls["360p"] = f;
                else if (q == "1080") ep.stream_urls["1080p"] = f;
                else if (q == "480") ep.stream_urls["480p"] = f;
                else ep.stream_urls[q] = f;
            }
        }

        // HLS streams fallback (sf2.an-media.org)
        std::regex hls_block_re(R"raw(files:\s*\[([\s\S]*?)\])raw");
        std::smatch hls_m;
        if (std::regex_search(ep_body, hls_m, hls_block_re)) {
            std::string hls_content = hls_m[1].str();
            std::regex file_re(R"raw(title:"([^"]+)",\s*file:"([^"]+)")raw");
            auto f_it = std::sregex_iterator(hls_content.begin(), hls_content.end(), file_re);
            for (; f_it != std::sregex_iterator(); ++f_it) {
                std::string q = (*f_it)[1].str();
                std::string f = (*f_it)[2].str();
                std::string label = (q.rfind("p") == std::string::npos) ? (q + "p") : q;
                if (ep.stream_urls.find(label) == ep.stream_urls.end()) {
                    ep.stream_urls[label] = f;
                }
            }
        }

        episodes.push_back(ep);
    }

    return episodes;
}

Stream AniStarProvider::get_stream(const Anime& anime, const Episode& episode) {
    Stream s;
    std::map<std::string, std::string> urls = episode.stream_urls;

    if (urls.empty()) {
        auto all_episodes = get_episodes(anime);
        for (const auto& ep : all_episodes) {
            if (ep.number == episode.number) {
                urls = ep.stream_urls;
                break;
            }
        }
        if (urls.empty() && !all_episodes.empty()) {
            urls = all_episodes[0].stream_urls;
        }
    }

    const std::vector<std::string> order = {"1080p", "720p", "480p", "360p"};
    for (const auto& target : order) {
        auto it = urls.find(target);
        if (it != urls.end() && !it->second.empty()) {
            Quality q;
            q.label = it->first;
            q.url = it->second;
            q.headers["Referer"] = "https://v30.astar.bz/";
            q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
            s.qualities.push_back(q);
        }
    }

    for (const auto& [label, url] : urls) {
        if (url.empty()) continue;
        if (std::find(order.begin(), order.end(), label) != order.end()) continue;
        Quality q;
        q.label = label;
        q.url = url;
        q.headers["Referer"] = "https://v30.astar.bz/";
        q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
        s.qualities.push_back(q);
    }

    return s;
}

} // namespace anime
