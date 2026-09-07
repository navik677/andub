#pragma once

#include <string>
#include <cstring>
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

} // namespace anime::utils
