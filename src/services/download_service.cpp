#include "download_service.hpp"
#include <filesystem>
#include <functional>
#include <thread>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QRegularExpression>

namespace anime {

namespace fs = std::filesystem;

static std::string sanitize_filename(const std::string& name) {
    std::string res;
    for (char ch : name) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|' || c < 32) {
            res += '_';
        } else {
            res += ch;
        }
    }
    while (!res.empty() && (res.back() == ' ' || res.back() == '_' || res.back() == '.')) res.pop_back();
    if (res.empty()) res = "anime";
    return res;
}

DownloadService& DownloadService::instance() {
    static DownloadService s;
    return s;
}

std::string DownloadService::download_root() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (base.isEmpty()) base = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    fs::path root = fs::path(base.toStdString()) / "Anime";
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) root = fs::temp_directory_path(ec) / "AnimeDownloads";
    fs::create_directories(root, ec);
    return root.string();
}

void DownloadService::update(int id, const std::function<void(DownloadJob&)>& fn) {
    std::lock_guard lock(mutex_);
    for (auto& j : jobs_) {
        if (j.id == id) {
            fn(j);
            return;
        }
    }
}

void DownloadService::start_download(const Anime& anime, const Episode& episode, const std::string& quality,
                                     const std::string& stream_url, const std::map<std::string, std::string>& headers) {
    DownloadJob job;
    job.anime_title = anime.title_ru.empty() ? "Аніме" : anime.title_ru;
    job.episode_title = episode.display_title();
    job.quality = quality;
    job.url = stream_url;
    job.is_active = true;

    fs::path dir = fs::path(download_root()) / sanitize_filename(job.anime_title);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) dir = download_root();
    job.output_path = (dir / (sanitize_filename(job.episode_title) + ".mp4")).string();

    {
        std::lock_guard lock(mutex_);
        job.id = next_id_++;
        jobs_.push_back(job);
    }

    std::thread([this, job, headers]() {
        QStringList args{"--newline", "--no-part", "-o", QString::fromStdString(job.output_path)};
        for (const auto& [k, v] : headers) {
            if (k == "Referer" || k == "referer") {
                args << "--referer" << QString::fromStdString(v);
            } else {
                args << "--add-header" << QString::fromStdString(k + ": " + v);
            }
        }
        args << QString::fromStdString(job.url);

        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(QStringLiteral("yt-dlp"), args);
        if (!proc.waitForStarted(10000)) {
            update(job.id, [](DownloadJob& j) {
                j.is_active = false;
                j.has_error = true;
                j.error = "yt-dlp не знайдено";
            });
            return;
        }

        // "[download]  42.3% of ~120.00MiB at  2.10MiB/s ETA 00:41"
        static const QRegularExpression re(
            R"(\[download\]\s+([\d.]+)%.*?(?:at\s+(\S+))?.*?(?:ETA\s+(\S+))?\s*$)");
        QByteArray buffer;
        while (proc.state() != QProcess::NotRunning) {
            proc.waitForReadyRead(500);
            buffer += proc.readAll();
            int nl;
            while ((nl = buffer.indexOf('\n')) >= 0) {
                QString line = QString::fromUtf8(buffer.left(nl)).trimmed();
                buffer.remove(0, nl + 1);
                auto m = re.match(line);
                if (!m.hasMatch()) continue;
                double pct = m.captured(1).toDouble() / 100.0;
                std::string speed = m.captured(2).toStdString();
                std::string eta = m.captured(3).toStdString();
                update(job.id, [&](DownloadJob& j) {
                    j.progress = pct;
                    if (!speed.empty() && speed != "Unknown") j.speed = speed;
                    if (!eta.empty() && eta != "Unknown") j.eta = eta;
                });
            }
        }
        proc.waitForFinished(-1);
        const bool ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
        update(job.id, [ok](DownloadJob& j) {
            j.is_active = false;
            j.speed.clear();
            j.eta.clear();
            if (ok) {
                j.is_done = true;
                j.progress = 1.0;
            } else {
                j.has_error = true;
                j.error = "Не вдалося завантажити";
            }
        });
    }).detach();
}

std::vector<DownloadJob> DownloadService::get_jobs() {
    std::lock_guard lock(mutex_);
    return jobs_;
}

void DownloadService::clear_finished() {
    std::lock_guard lock(mutex_);
    std::erase_if(jobs_, [](const DownloadJob& j) { return !j.is_active; });
}

} // namespace anime
