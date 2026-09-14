#include "kodik_resolver.hpp"
#include "http_client.hpp"
#include "json.hpp"
#include <regex>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace anime {

static std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << std::setw(2) << (int)(unsigned char)c;
        }
    }
    return escaped.str();
}

static std::string url_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            auto hex_val = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int v1 = hex_val(in[i + 1]);
            int v2 = hex_val(in[i + 2]);
            if (v1 != -1 && v2 != -1) {
                out.push_back(static_cast<char>((v1 << 4) | v2));
                i += 2;
                continue;
            }
        } else if (in[i] == '+') {
            out.push_back(' ');
            continue;
        }
        out.push_back(in[i]);
    }
    return out;
}

static char rot18_char(char c) {
    if (c >= 'a' && c <= 'z') {
        return static_cast<char>((c - 'a' + 18) % 26 + 'a');
    } else if (c >= 'A' && c <= 'Z') {
        return static_cast<char>((c - 'A' + 18) % 26 + 'A');
    }
    return c;
}

static std::string rot18(const std::string& input) {
    std::string result = input;
    for (char& c : result) {
        c = rot18_char(c);
    }
    return result;
}

static std::string base64_decode(const std::string& in) {
    static const std::string b64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[static_cast<unsigned char>(b64_chars[i])] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static std::string decode_kodik_link(const std::string& raw) {
    std::string rot = rot18(raw);
    size_t rem = rot.size() % 4;
    if (rem > 0) {
        rot.append(4 - rem, '=');
    }
    std::string decoded = base64_decode(rot);
    if (decoded.rfind("//", 0) == 0) {
        decoded = "https:" + decoded;
    }
    return decoded;
}

std::vector<int> KodikResolver::get_episodes(const std::string& raw_kodik_url, const std::string& referer) {
    std::string url = raw_kodik_url;
    if (url.rfind("//", 0) == 0) url = "https:" + url;

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", referer.empty() ? "https://kodikplayer.com/" : referer}
    };

    auto resp = http::Client::get(url, headers);
    if (!resp.success()) return {};

    std::vector<int> episodes;
    std::regex opt_re(R"raw(<option\s+[^>]*value=[\"\'](\d+)[\"\'][^>]*data-id=[\"\'](\d+)[\"\'][^>]*data-hash=[\"\']([a-zA-Z0-9]+)[\"\'])raw");
    auto begin = std::sregex_iterator(resp.body.begin(), resp.body.end(), opt_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        try {
            int ep = std::stoi((*it)[1].str());
            episodes.push_back(ep);
        } catch (...) {}
    }

    if (episodes.empty()) {
        std::regex count_re(R"raw(data-episode-count="(\d+)")raw");
        std::smatch cmatch;
        if (std::regex_search(resp.body, cmatch, count_re)) {
            try {
                int total = std::stoi(cmatch[1].str());
                for (int i = 1; i <= total; ++i) episodes.push_back(i);
            } catch (...) {}
        }
    }

    return episodes;
}

Stream KodikResolver::resolve(const std::string& raw_kodik_url, int episode_num, const std::string& referer) {
    Stream stream;
    std::string url = raw_kodik_url;
    if (url.rfind("//", 0) == 0) url = "https:" + url;

    std::map<std::string, std::string> headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", referer.empty() ? "https://kodikplayer.com/" : referer}
    };

    auto resp = http::Client::get(url, headers);
    if (!resp.success()) {
        std::cerr << "[KodikResolver] Failed to fetch player page: " << url << " (error: " << resp.error << ")\n";
        return stream;
    }

    const std::string& html = resp.body;

    std::smatch m;
    std::string domain, d_sign, pd = "kodikplayer.com", pd_sign, ref, ref_sign;

    // 1. Check for urlParams JSON object
    std::regex url_params_re(R"raw(urlParams\s*=\s*['"]([^'"]+)['"])raw");
    if (std::regex_search(html, m, url_params_re)) {
        try {
            auto params_json = json::Value::parse(m[1].str());
            domain = params_json["d"].get_str();
            d_sign = params_json["d_sign"].get_str();
            std::string parsed_pd = params_json["pd"].get_str();
            if (!parsed_pd.empty()) pd = parsed_pd;
            pd_sign = params_json["pd_sign"].get_str();
            ref = params_json["ref"].get_str();
            ref_sign = params_json["ref_sign"].get_str();
        } catch (...) {}
    }

    // 2. Fallback regexes if urlParams was incomplete
    if (domain.empty()) {
        std::regex d_re(R"raw(var\s+domain\s*=\s*[\"\']([^\"\']+)[\"\'])raw");
        if (std::regex_search(html, m, d_re)) domain = m[1].str();
    }
    if (d_sign.empty()) {
        std::regex ds_re(R"raw(var\s+d_sign\s*=\s*[\"\']([^\"\']+)[\"\'])raw");
        if (std::regex_search(html, m, ds_re)) d_sign = m[1].str();
    }
    if (pd_sign.empty()) {
        std::regex pds_re(R"raw(var\s+pd_sign\s*=\s*[\"\']([^\"\']+)[\"\'])raw");
        if (std::regex_search(html, m, pds_re)) pd_sign = m[1].str();
    }
    if (ref.empty()) {
        std::regex ref_re(R"raw(var\s+ref\s*=\s*[\"\']([^\"\']*)[\"\'])raw");
        if (std::regex_search(html, m, ref_re)) ref = m[1].str();
    }
    if (ref_sign.empty()) {
        std::regex refs_re(R"raw(var\s+ref_sign\s*=\s*[\"\']([^\"\']*)[\"\'])raw");
        if (std::regex_search(html, m, refs_re)) ref_sign = m[1].str();
    }

    // Decode URL-encoded ref if needed (e.g. https%3A%2F%2Fshizaproject.com%2F)
    if (ref.find('%') != std::string::npos) {
        ref = url_decode(ref);
    }

    std::string vid_id, vid_hash, vid_type = "seria";

    // Try finding episode in options first
    std::string ep_str = std::to_string(episode_num);
    std::regex target_opt(R"raw(<option\s+[^>]*value=[\"\'])raw" + ep_str + R"raw([\"\'][^>]*data-id=[\"\'](\d+)[\"\'][^>]*data-hash=[\"\']([a-zA-Z0-9]+)[\"\'])raw");
    if (std::regex_search(html, m, target_opt)) {
        vid_id = m[1].str();
        vid_hash = m[2].str();
    } else {
        // Look for any option fallback
        std::regex any_opt(R"raw(<option\s+[^>]*data-id=[\"\'](\d+)[\"\'][^>]*data-hash=[\"\']([a-zA-Z0-9]+)[\"\'])raw");
        if (std::regex_search(html, m, any_opt)) {
            vid_id = m[1].str();
            vid_hash = m[2].str();
        }
    }

    // Fallback to vInfo if options not found (e.g. single video /uv/ or /seria/)
    if (vid_id.empty() || vid_hash.empty()) {
        std::regex vi_id(R"raw(vInfo\.id\s*=\s*[\"\']([^\"\']+)[\"\'])raw");
        if (std::regex_search(html, m, vi_id)) vid_id = m[1].str();

        std::regex vi_hash(R"raw(vInfo\.hash\s*=\s*[\"\']([^\"\']+)[\"\'])raw");
        if (std::regex_search(html, m, vi_hash)) vid_hash = m[1].str();

        std::regex vi_type(R"raw(vInfo\.type\s*=\s*[\"\']([^\"\']+)[\"\'])raw");
        if (std::regex_search(html, m, vi_type)) vid_type = m[1].str();
    }

    // Fallback to serialId / serialHash
    if (vid_id.empty() || vid_hash.empty()) {
        std::regex s_id(R"raw(serialId\s*=\s*Number\((\d+)\))raw");
        if (std::regex_search(html, m, s_id)) vid_id = m[1].str();

        std::regex s_hash(R"raw(serialHash\s*=\s*[\"\']([^\"\']+)[\"\'])raw");
        if (std::regex_search(html, m, s_hash)) vid_hash = m[1].str();
    }

    if (vid_id.empty() || vid_hash.empty()) {
        std::cerr << "[KodikResolver] Could not find video id/hash in: " << url << "\n";
        return stream;
    }

    std::ostringstream post_body;
    post_body << "d=" << url_encode(domain)
              << "&d_sign=" << url_encode(d_sign)
              << "&pd=" << url_encode(pd)
              << "&pd_sign=" << url_encode(pd_sign)
              << "&ref=" << url_encode(ref)
              << "&ref_sign=" << url_encode(ref_sign)
              << "&bad_user=false&cdn_is_working=true"
              << "&type=" << url_encode(vid_type)
              << "&hash=" << url_encode(vid_hash)
              << "&id=" << url_encode(vid_id);

    std::string ftor_url = "https://" + pd + "/ftor";
    std::map<std::string, std::string> post_headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"},
        {"Referer", url},
        {"Origin", "https://" + pd},
        {"X-Requested-With", "XMLHttpRequest"},
        {"Content-Type", "application/x-www-form-urlencoded"}
    };

    auto post_resp = http::Client::post(ftor_url, post_body.str(), post_headers);
    if (!post_resp.success()) {
        std::cerr << "[KodikResolver] ftor request failed: " << post_resp.error << " (status: " << post_resp.status_code << ")\n";
        return stream;
    }

    auto root = json::Value::parse(post_resp.body);
    auto links = root["links"];
    if (!links.is_object()) {
        std::cerr << "[KodikResolver] No 'links' object in ftor response\n";
        return stream;
    }

    // Process available qualities in descending order
    std::vector<std::string> quality_order = {"1080", "720", "480", "360"};
    for (const auto& q_label : quality_order) {
        auto q_arr = links[q_label];
        if (q_arr.is_array() && q_arr.size() > 0) {
            std::string raw_src = q_arr[0]["src"].get_str();
            if (!raw_src.empty()) {
                std::string direct_url = decode_kodik_link(raw_src);
                auto hls_pos = direct_url.find(":hls:manifest.m3u8");
                if (hls_pos != std::string::npos) {
                    direct_url = direct_url.substr(0, hls_pos);
                }
                Quality q;
                q.label = q_label + "p";
                q.url = direct_url;
                q.headers["Referer"] = "https://" + pd + "/";
                q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
                stream.qualities.push_back(std::move(q));
            }
        }
    }

    // Any other qualities present
    const auto& obj = links.obj_val;
    for (const auto& [k, v] : obj) {
        bool already = false;
        for (const auto& existing : quality_order) {
            if (existing == k) { already = true; break; }
        }
        if (!already && v.is_array() && v.size() > 0) {
            std::string raw_src = v[0]["src"].get_str();
            if (!raw_src.empty()) {
                std::string direct_url = decode_kodik_link(raw_src);
                auto hls_pos = direct_url.find(":hls:manifest.m3u8");
                if (hls_pos != std::string::npos) {
                    direct_url = direct_url.substr(0, hls_pos);
                }
                Quality q;
                q.label = k + "p";
                q.url = direct_url;
                q.headers["Referer"] = "https://" + pd + "/";
                q.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
                stream.qualities.push_back(std::move(q));
            }
        }
    }

    return stream;
}

} // namespace anime
