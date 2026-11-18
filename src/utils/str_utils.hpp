#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdint>
#include <QString>

namespace anime::utils {

inline bool utf8_contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.empty()) return false;
    return QString::fromStdString(haystack).toCaseFolded()
        .contains(QString::fromStdString(needle).toCaseFolded());
}

inline std::string utf8_tolower(const std::string& str) {
    if (str.empty()) return "";
    return QString::fromStdString(str).toLower().toStdString();
}

inline std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

namespace detail {
// Unicode code points for Windows-1251 bytes 0x80..0xFF (0 = undefined byte).
inline const char16_t* cp1251_high() {
    static const char16_t table[128] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
        0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x0000, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
        0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
        0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
        0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
        0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
        0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
        0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
        0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
        0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
        0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
    };
    return table;
}
} // namespace detail

inline std::string cp1251_to_utf8(const std::string& input) {
    if (input.empty()) return "";
    QString out;
    out.reserve(static_cast<qsizetype>(input.size()));
    const char16_t* high = detail::cp1251_high();
    for (unsigned char c : input) {
        if (c < 0x80) {
            out.append(QChar(c));
        } else {
            char16_t u = high[c - 0x80];
            out.append(QChar(u ? u : 0xFFFD));
        }
    }
    return out.toStdString();
}

inline std::string utf8_to_cp1251(const std::string& input) {
    if (input.empty()) return "";
    const QString s = QString::fromStdString(input);
    const char16_t* high = detail::cp1251_high();
    std::string out;
    out.reserve(static_cast<size_t>(s.size()));
    for (QChar ch : s) {
        char16_t u = ch.unicode();
        if (u < 0x80) {
            out += static_cast<char>(u);
            continue;
        }
        char mapped = '?';
        for (int i = 0; i < 128; ++i) {
            if (high[i] == u) {
                mapped = static_cast<char>(0x80 + i);
                break;
            }
        }
        out += mapped;
    }
    return out;
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
