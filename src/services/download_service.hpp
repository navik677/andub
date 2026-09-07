#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "../models/episode.hpp"
#include "../models/anime.hpp"

namespace anime {

struct DownloadJob {
    std::string anime_title;
    std::string episode_title;
    std::string quality;
    std::string url;
    std::string progress_percent = "0%";
    std::string speed;
    std::string eta;
    bool is_active = false;
    bool is_done = false;
    bool has_error = false;
};

class DownloadService {
public:
    static DownloadService& instance();

    void start_download(const Anime& anime, const Episode& episode, const std::string& quality, const std::string& stream_url);
    std::vector<DownloadJob> get_jobs();

private:
    DownloadService() = default;
    std::vector<DownloadJob> jobs_;
    std::mutex mutex_;
};

} // namespace anime
