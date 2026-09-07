#include "player_service.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <cstdlib>
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
        args.push_back("--msg-level=all=fatal,statusline=status");

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
