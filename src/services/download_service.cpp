#include "download_service.hpp"
#include <iostream>
#include <thread>
#include <filesystem>
#include <cstdlib>
#include <array>

namespace anime {

static void send_notification(const std::string& title, const std::string& msg) {
    std::string cmd = "notify-send -a \"Anime GUI\" \"" + title + "\" \"" + msg + "\" 2>/dev/null";
    std::system(cmd.c_str());
}

static std::string sanitize_filename(const std::string& name) {
    std::string res;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_' || c == '(' || c == ')') {
            res += c;
        } else {
            res += '_';
        }
    }
    return res;
}

DownloadService& DownloadService::instance() {
    static DownloadService s;
    return s;
}

void DownloadService::start_download(const Anime& anime, const Episode& episode, const std::string& quality, const std::string& stream_url) {
    DownloadJob job;
    job.anime_title = anime.title_ru.empty() ? "Аніме" : anime.title_ru;
    job.episode_title = episode.display_title();
    job.quality = quality;
    job.url = stream_url;
    job.is_active = true;

    size_t job_idx = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push_back(job);
        job_idx = jobs_.size() - 1;
    }

    send_notification("Початок завантаження", job.anime_title + " — " + job.episode_title);

    std::thread([this, job_idx, job]() {
        const char* home = std::getenv("HOME");
        std::string dl_base = home ? std::string(home) + "/Downloads/Anime" : "/tmp";
        std::string safe_dir = dl_base + "/" + sanitize_filename(job.anime_title);
        std::filesystem::create_directories(safe_dir);

        std::string out_path = safe_dir + "/" + sanitize_filename(job.episode_title) + ".mp4";

        std::string cmd = "yt-dlp --newline -o \"" + out_path + "\" \"" + job.url + "\" 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_[job_idx].is_active = false;
            jobs_[job_idx].has_error = true;
            send_notification("Помилка завантаження", job.anime_title + " — " + job.episode_title);
            return;
        }

        std::array<char, 512> buf;
        while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
            std::string line = buf.data();
            if (line.find("[download]") != std::string::npos && line.find('%') != std::string::npos) {
                // Parse percentage
                size_t p_pos = line.find('%');
                size_t sp = line.rfind(' ', p_pos);
                if (sp != std::string::npos) {
                    std::string pct = line.substr(sp + 1, p_pos - sp);
                    std::lock_guard<std::mutex> lock(mutex_);
                    jobs_[job_idx].progress_percent = pct;
                }
            }
        }

        int ret = pclose(pipe);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_[job_idx].is_active = false;
            if (ret == 0) {
                jobs_[job_idx].is_done = true;
                jobs_[job_idx].progress_percent = "100%";
            } else {
                jobs_[job_idx].has_error = true;
            }
        }

        if (ret == 0) {
            send_notification("Завершено", job.anime_title + " — " + job.episode_title);
        } else {
            send_notification("Помилка", "Не вдалося завантажити " + job.episode_title);
        }
    }).detach();
}

std::vector<DownloadJob> DownloadService::get_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_;
}

} // namespace anime
