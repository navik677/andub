#include "episode_list_model.hpp"
#include <QDateTime>

namespace anime::app {

int EpisodeListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : count();
}

QHash<int, QByteArray> EpisodeListModel::roleNames() const {
    return {{NumberRole, "number"}, {TitleRole, "title"}, {LabelRole, "label"},
            {WatchedRole, "watched"}, {BusyRole, "busy"}, {ProgressRole, "progress"}};
}

QVariant EpisodeListModel::data(const QModelIndex& index, int role) const {
    const int row = index.row();
    if (row < 0 || row >= count()) return {};
    const Episode& ep = items_[static_cast<size_t>(row)];
    switch (role) {
    case NumberRole: return QString::fromStdString(ep.number);
    case TitleRole:
        // Many providers repeat the number as the title; hide that duplicate
        return (ep.title == ep.number) ? QString() : QString::fromStdString(ep.title);
    case LabelRole: return QString::fromStdString(ep.display_title());
    case WatchedRole: return isWatched(row);
    case BusyRole: return busy_.count(row) > 0;
    case ProgressRole: return isWatched(row) ? 0.0 : progress(row).fraction;
    default: return {};
    }
}

bool EpisodeListModel::isWatched(int row) const {
    if (row < 0 || row >= count()) return false;
    return watched_.count(items_[static_cast<size_t>(row)].number) > 0;
}

int EpisodeListModel::watchedCount() const {
    int n = 0;
    for (int i = 0; i < count(); ++i) n += isWatched(i) ? 1 : 0;
    return n;
}

void EpisodeListModel::reset(std::vector<Episode> items, std::set<std::string> watched,
                             std::map<std::string, Progress> progress) {
    beginResetModel();
    items_ = std::move(items);
    watched_ = std::move(watched);
    progress_ = std::move(progress);
    busy_.clear();
    endResetModel();
    emit countChanged();
}

void EpisodeListModel::setWatched(int row) {
    if (row < 0 || row >= count()) return;
    const std::string& num = items_[static_cast<size_t>(row)].number;
    const bool had_progress = progress_.erase(num) > 0;
    if (isWatched(row) && !had_progress) return;
    watched_.insert(num);
    emit dataChanged(index(row), index(row), {WatchedRole, ProgressRole});
}

void EpisodeListModel::setProgress(int row, double fraction, double position) {
    if (row < 0 || row >= count()) return;
    Progress& p = progress_[items_[static_cast<size_t>(row)].number];
    p.fraction = fraction;
    p.position = position;
    p.updated = QDateTime::currentSecsSinceEpoch();
    emit dataChanged(index(row), index(row), {ProgressRole});
}

EpisodeListModel::Progress EpisodeListModel::progress(int row) const {
    if (row < 0 || row >= count()) return {};
    auto it = progress_.find(items_[static_cast<size_t>(row)].number);
    return it != progress_.end() ? it->second : Progress{};
}

int EpisodeListModel::lastInProgress() const {
    int best = -1;
    long long best_time = -1;
    for (int i = 0; i < count(); ++i) {
        auto it = progress_.find(items_[static_cast<size_t>(i)].number);
        if (it != progress_.end() && it->second.updated > best_time) {
            best = i;
            best_time = it->second.updated;
        }
    }
    return best;
}

void EpisodeListModel::setBusy(int row, bool busy) {
    if (row < 0 || row >= count()) return;
    if (busy) busy_.insert(row);
    else busy_.erase(row);
    emit dataChanged(index(row), index(row), {BusyRole});
}

} // namespace anime::app
