#include "player_service.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <glib.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace anime {

bool PlayerService::play(const Quality& quality, const std::string& anime_title, const Episode* episode) {
    if (quality.url.empty()) return false;

    // Run mpv in background thread
    std::thread([quality, anime_title, ep_copy = (episode ? *episode : Episode())]() {
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
            if (!std::filesystem::exists(input_conf)) {
                std::ofstream f(input_conf);
                f << "# Anime shortcuts\n"
                  << "s show-text \"⏩ Пропуск опенінгу (+85с)\" ; seek 85\n"
                  << "S show-text \"⏪ Повернення (-85с)\" ; seek -85\n"
                  << "RIGHT seek 10\n"
                  << "LEFT seek -10\n"
                  << "UP add volume 5\n"
                  << "DOWN add volume -5\n"
                  << "SPACE cycle pause\n"
                  << "f cycle fullscreen\n";
            }
            args.push_back("--input-conf=" + input_conf);
        } catch (...) {}

        if (quality.url.find(".m3u8") != std::string::npos) {
            args.push_back("--cache=yes");
            args.push_back("--demuxer-max-bytes=500MiB");
            args.push_back("--demuxer-readahead-secs=300");
            args.push_back("--stream-lavf-o=reconnect=1");
            args.push_back("--stream-lavf-o=reconnect_streamed=1");
            args.push_back("--stream-lavf-o=reconnect_delay_max=30");
        }

        std::string headers_str;
        for (const auto& [k, v] : quality.headers) {
            if (!headers_str.empty()) headers_str += ",";
            headers_str += k + ": " + v;
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
            // Child process
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
