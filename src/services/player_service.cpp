#include "player_service.hpp"
#include "../utils/json.hpp"
#include "../utils/paths.hpp"
#include "../utils/str_utils.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <QProcess>
#include <QStringList>

namespace anime {

static PlayerMode s_player_mode = PlayerMode::Embedded;
static bool s_mode_loaded = false;
// settings.json is read-modify-written from the UI and from background threads
static std::mutex s_settings_mutex;

std::string PlayerService::get_config_path() {
    return paths::config_file("settings.json");
}

PlayerMode PlayerService::get_player_mode() {
    if (!s_mode_loaded) {
        s_mode_loaded = true;
        std::string cfg_path = get_config_path();
        if (std::filesystem::exists(cfg_path)) {
            try {
                std::ifstream f(cfg_path);
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                auto root = json::Value::parse(content);
                if (root.is_object() && root.contains("player_mode")) {
                    std::string m = root["player_mode"].get_str();
                    if (m == "external") {
                        s_player_mode = PlayerMode::ExternalMpv;
                    } else {
                        s_player_mode = PlayerMode::Embedded;
                    }
                }
            } catch (...) {}
        }
    }
    return s_player_mode;
}

void PlayerService::set_player_mode(PlayerMode mode) {
    s_player_mode = mode;
    s_mode_loaded = true;
    set_setting("player_mode", mode == PlayerMode::ExternalMpv ? "external" : "embedded");
}

std::string PlayerService::get_setting(const std::string& key, const std::string& fallback) {
    std::lock_guard lock(s_settings_mutex);
    try {
        std::string cfg_path = get_config_path();
        if (!std::filesystem::exists(cfg_path)) return fallback;
        std::ifstream f(cfg_path);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto root = json::Value::parse(content);
        if (root.is_object() && root.contains(key)) {
            std::string v = root[key].get_str();
            if (!v.empty()) return v;
        }
    } catch (...) {}
    return fallback;
}

void PlayerService::set_setting(const std::string& key, const std::string& value) {
    std::lock_guard lock(s_settings_mutex);
    try {
        std::string cfg_path = get_config_path();
        json::Value root;
        if (std::filesystem::exists(cfg_path)) {
            std::ifstream f(cfg_path);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            root = json::Value::parse(content);
        }
        root[key] = value;

        std::ofstream out(cfg_path);
        out << root.dump(2);
    } catch (...) {}
}

bool PlayerService::play(const Quality& quality, const std::string& anime_title, const Episode* episode) {
    return play_external(quality, anime_title, episode);
}

bool PlayerService::play_external(const Quality& quality, const std::string& anime_title, const Episode* episode, double start_pos) {
    if (quality.url.empty()) return false;

    QStringList args;
    std::string win_title = anime_title;
    if (episode && !episode->number.empty()) {
        win_title += " - Серія " + episode->number;
    }
    args << QString::fromStdString("--title=" + win_title)
         << QString::fromStdString("--force-media-title=" + win_title)
         << "--msg-level=all=warn"
         << "--hwdec=auto-safe"
         << "--force-window=yes"
         << "--keep-open=yes";

    if (start_pos > 1.0) {
        args << QString("--start=%1").arg(static_cast<int>(start_pos));
    }

    // Anime-friendly key bindings (s skips the opening, like in the embedded player)
    try {
        std::string input_conf = paths::config_file("mpv_input.conf");
        std::ofstream f(input_conf);
        f << "# Anime shortcuts\n"
          << "s show-text \"Пропуск опенінгу (+85с)\" ; seek 85\n"
          << "S show-text \"Повернення (-85с)\" ; seek -85 ; set vid no ; set vid auto\n"
          << "RIGHT seek 10\n"
          << "LEFT seek -10 ; set vid no ; set vid auto\n"
          << "UP add volume 5\n"
          << "DOWN add volume -5\n"
          << "SPACE cycle pause\n"
          << "f cycle fullscreen\n";
        args << QString::fromStdString("--input-conf=" + input_conf);
    } catch (...) {}

    std::string stream_url = quality.url;
    auto hls_pos = stream_url.find(":hls:manifest.m3u8");
    if (hls_pos != std::string::npos) {
        stream_url = stream_url.substr(0, hls_pos);
    }

    args << "--ytdl=no" << "--load-scripts=no" << "--cache=yes"
         << "--demuxer-max-bytes=128MiB" << "--demuxer-readahead-secs=60";

    std::string headers_str;
    for (const auto& [k, v] : quality.headers) {
        std::string lower_k = utils::utf8_tolower(k);
        if (lower_k == "user-agent") {
            args << QString::fromStdString("--user-agent=" + v);
        } else if (lower_k == "referer") {
            args << QString::fromStdString("--referrer=" + v);
        } else {
            if (!headers_str.empty()) headers_str += ",";
            headers_str += k + ": " + v;
        }
    }
    if (!headers_str.empty()) {
        args << QString::fromStdString("--http-header-fields=" + headers_str);
    }
    args << QString::fromStdString(stream_url);

    // Arguments go straight to the process (no shell), so URLs and titles need no escaping.
    bool ok = QProcess::startDetached(QStringLiteral("mpv"), args);
    if (!ok) std::cerr << "[PlayerService] failed to start mpv\n";
    return ok;
}

} // namespace anime
