#include "player_service.hpp"
#include "../utils/json.hpp"
#include "../utils/str_utils.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <glib.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace anime {

static PlayerMode s_player_mode = PlayerMode::Embedded;
static bool s_mode_loaded = false;

std::string PlayerService::get_config_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::string base;
    if (xdg && *xdg) base = xdg;
    else if (home && *home) base = std::string(home) + "/.config";
    else base = "/tmp";

    std::string dir = base + "/anime-gui";
    try {
        std::filesystem::create_directories(dir);
    } catch (...) {
        dir = "/tmp/anime-gui";
        try { std::filesystem::create_directories(dir); } catch (...) {}
    }
    return dir + "/settings.json";
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

    try {
        std::string cfg_path = get_config_path();
        json::Value root;
        if (std::filesystem::exists(cfg_path)) {
            std::ifstream f(cfg_path);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            root = json::Value::parse(content);
        }
        root["player_mode"] = (mode == PlayerMode::ExternalMpv ? "external" : "embedded");

        std::ofstream out(cfg_path);
        out << root.dump(2);
    } catch (...) {}
}

bool PlayerService::play(const Quality& quality, const std::string& anime_title, const Episode* episode) {
    return play_external(quality, anime_title, episode);
}

bool PlayerService::play_external(const Quality& quality, const std::string& anime_title, const Episode* episode, double start_pos) {
    if (quality.url.empty()) return false;

    std::thread([quality, anime_title, ep_copy = (episode ? *episode : Episode()), start_pos]() {
        std::vector<std::string> args = {"mpv"};

        std::string win_title = anime_title;
        if (!ep_copy.number.empty()) {
            win_title += " — Серія " + ep_copy.number;
        }
        args.push_back("--title=" + win_title);
        args.push_back("--force-media-title=" + win_title);
        args.push_back("--msg-level=all=warn,statusline=status");
        args.push_back("--hwdec=auto-safe");
        args.push_back("--force-window=yes");
        args.push_back("--keep-open=yes");

        if (start_pos > 1.0) {
            args.push_back("--start=" + std::to_string(static_cast<int>(start_pos)));
        }

        // Ensure anime input config with opening skip (+85s on 's')
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        std::string conf_dir;
        if (xdg && *xdg) conf_dir = std::string(xdg) + "/anime-gui";
        else if (home && *home) conf_dir = std::string(home) + "/.config/anime-gui";
        else conf_dir = "/tmp/anime-gui";

        try {
            std::filesystem::create_directories(conf_dir);
            std::string input_conf = conf_dir + "/mpv_input.conf";
            std::ofstream f(input_conf);
            f << "# Anime shortcuts\n"
              << "s show-text \"Пропуск опенінгу (+85с)\" ; seek 85\n"
              << "S show-text \"Повернення (-85с)\" ; seek -85\n"
              << "RIGHT seek 10\n"
              << "LEFT seek -10\n"
              << "UP add volume 5\n"
              << "DOWN add volume -5\n"
              << "SPACE cycle pause\n"
              << "f cycle fullscreen\n";
            args.push_back("--input-conf=" + input_conf);
        } catch (...) {}

        if (quality.url.find(".m3u8") != std::string::npos || !quality.headers.empty()) {
            args.push_back("--ytdl=no");
            args.push_back("--cache=yes");
            args.push_back("--demuxer-max-bytes=500MiB");
            args.push_back("--demuxer-readahead-secs=300");
        }

        std::string headers_str;
        std::string user_agent;
        for (const auto& [k, v] : quality.headers) {
            std::string lower_k = utils::utf8_tolower(k);
            if (lower_k == "user-agent") {
                user_agent = v;
            } else {
                if (!headers_str.empty()) headers_str += ",";
                headers_str += k + ": " + v;
            }
        }
        if (!user_agent.empty()) {
            args.push_back("--user-agent=" + user_agent);
        }
        if (!headers_str.empty()) {
            args.push_back("--http-header-fields=" + headers_str);
        }

        args.push_back(quality.url);

        std::vector<char*> c_args;
        for (auto& s : args) c_args.push_back(s.data());
        c_args.push_back(nullptr);

#ifdef _WIN32
        GError* gerr = nullptr;
        g_spawn_async(nullptr, c_args.data(), nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &gerr);
        if (gerr) {
            std::cerr << "[PlayerService] g_spawn_async error: " << gerr->message << "\n";
            g_error_free(gerr);
        }
#else
        pid_t pid = fork();
        if (pid == 0) {
            execvp("mpv", c_args.data());
            _exit(127);
        } else if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
        }
#endif
    }).detach();

    return true;
}

} // namespace anime
