#pragma once

#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <glib.h>

namespace anime::utils {

inline bool utf8_contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.empty()) return false;

    gchar* h_fold = g_utf8_casefold(haystack.c_str(), -1);
    gchar* n_fold = g_utf8_casefold(needle.c_str(), -1);
    bool match = false;
    if (h_fold && n_fold) {
        match = (strstr(h_fold, n_fold) != nullptr);
    }
    if (h_fold) g_free(h_fold);
    if (n_fold) g_free(n_fold);
    return match;
}

inline std::string utf8_tolower(const std::string& str) {
    if (str.empty()) return "";
    gchar* lower = g_utf8_strdown(str.c_str(), -1);
    std::string res = lower ? lower : "";
    if (lower) g_free(lower);
    return res;
}

inline std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

inline std::string cp1251_to_utf8(const std::string& input) {
    if (input.empty()) return "";
    GError* err = nullptr;
    gsize bytes_written = 0;
    gchar* conv = g_convert(input.data(), static_cast<gssize>(input.size()),
                            "UTF-8", "WINDOWS-1251", nullptr, &bytes_written, &err);
    if (err) {
        g_error_free(err);
        return input;
    }
    std::string res(conv, bytes_written);
    g_free(conv);
    return res;
}

inline std::string utf8_to_cp1251(const std::string& input) {
    if (input.empty()) return "";
    GError* err = nullptr;
    gsize bytes_written = 0;
    gchar* conv = g_convert(input.data(), static_cast<gssize>(input.size()),
                            "WINDOWS-1251", "UTF-8", nullptr, &bytes_written, &err);
    if (err) {
        g_error_free(err);
        return input;
    }
    std::string res(conv, bytes_written);
    g_free(conv);
    return res;
}

inline std::string cp1251_url_encode(const std::string& utf8_str) {
    std::string cp1251 = utf8_to_cp1251(utf8_str);
    std::ostringstream escaped;
    for (unsigned char c : cp1251) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    return escaped.str();
}

} // namespace anime::utils
