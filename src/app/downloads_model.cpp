#include "downloads_model.hpp"
#include "notifier.hpp"
#include <QDesktopServices>
#include <QFileInfo>
#include <QQmlEngine>
#include <QUrl>

namespace anime::app {

Downloads* Downloads::instance() {
    static auto* d = new Downloads();
    return d;
}

Downloads* Downloads::create(QQmlEngine*, QJSEngine*) {
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Downloads::Downloads() {
    timer_.setInterval(500);
    connect(&timer_, &QTimer::timeout, this, &Downloads::poll);
    timer_.start();
}

int Downloads::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : count();
}

QHash<int, QByteArray> Downloads::roleNames() const {
    return {{AnimeRole, "animeTitle"}, {EpisodeRole, "episodeTitle"}, {QualityRole, "quality"},
            {ProgressRole, "progress"}, {SpeedRole, "speed"}, {EtaRole, "eta"},
            {ActiveRole, "active"}, {DoneRole, "done"}, {ErrorRole, "failed"},
            {ErrorTextRole, "errorText"}, {PathRole, "path"}};
}

QVariant Downloads::data(const QModelIndex& index, int role) const {
    const int r = index.row();
    if (r < 0 || r >= count()) return {};
    // Newest first
    const DownloadJob& j = jobs_[jobs_.size() - 1 - static_cast<size_t>(r)];
    switch (role) {
    case AnimeRole: return QString::fromStdString(j.anime_title);
    case EpisodeRole: return QString::fromStdString(j.episode_title);
    case QualityRole: return QString::fromStdString(j.quality);
    case ProgressRole: return j.progress;
    case SpeedRole: return QString::fromStdString(j.speed);
    case EtaRole: return QString::fromStdString(j.eta);
    case ActiveRole: return j.is_active;
    case DoneRole: return j.is_done;
    case ErrorRole: return j.has_error;
    case ErrorTextRole: return QString::fromStdString(j.error);
    case PathRole: return QString::fromStdString(j.output_path);
    default: return {};
    }
}

int Downloads::activeCount() const {
    int n = 0;
    for (const auto& j : jobs_) n += j.is_active ? 1 : 0;
    return n;
}

void Downloads::poll() {
    auto fresh = DownloadService::instance().get_jobs();

    // Announce jobs that just finished
    for (const auto& j : fresh) {
        for (const auto& old : jobs_) {
            if (old.id != j.id || !old.is_active || j.is_active) continue;
            const QString name = QString::fromStdString(j.anime_title + ", " + j.episode_title);
            if (j.is_done) Notifier::post(QStringLiteral("Завантажено: ") + name);
            else Notifier::post(QStringLiteral("Помилка завантаження: ") + name, true);
        }
    }

    if (fresh.size() != jobs_.size()) {
        beginResetModel();
        jobs_ = std::move(fresh);
        endResetModel();
        emit countChanged();
        return;
    }
    bool active_changed = false;
    for (size_t i = 0; i < fresh.size(); ++i) active_changed |= fresh[i].is_active != jobs_[i].is_active;
    jobs_ = std::move(fresh);
    if (!jobs_.empty()) emit dataChanged(index(0), index(count() - 1));
    if (active_changed) emit countChanged();
}

void Downloads::clearFinished() {
    DownloadService::instance().clear_finished();
    poll();
}

void Downloads::openFolder(int row) const {
    const QString path = data(index(row), PathRole).toString();
    if (path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

} // namespace anime::app
