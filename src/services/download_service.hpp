#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <functional>
#include "../models/episode.hpp"
#include "../models/anime.hpp"

namespace anime {

struct DownloadJob {
    int id = 0;
    std::string anime_title;
    std::string episode_title;
    std::string quality;
    std::string url;
    std::string output_path;
    double progress = 0.0;      // 0..1
    std::string speed;
    std::string eta;
    bool is_active = false;
    bool is_done = false;
    bool has_error = false;
    std::string error;
};

// Background episode downloads through yt-dlp.
class DownloadService {
public:
    static DownloadService& instance();

    void start_download(const Anime& anime, const Episode& episode, const std::string& quality,
                        const std::string& stream_url, const std::map<std::string, std::string>& headers = {});
    std::vector<DownloadJob> get_jobs();
    void clear_finished();
    static std::string download_root();

private:
    DownloadService() = default;
    void update(int id, const std::function<void(DownloadJob&)>& fn);

    std::vector<DownloadJob> jobs_;
    std::mutex mutex_;
    int next_id_ = 1;
};

} // namespace anime
