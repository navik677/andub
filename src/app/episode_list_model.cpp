#include "episode_list_model.hpp"

namespace anime::app {

int EpisodeListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : count();
}

QHash<int, QByteArray> EpisodeListModel::roleNames() const {
    return {{NumberRole, "number"}, {TitleRole, "title"}, {LabelRole, "label"},
            {WatchedRole, "watched"}, {BusyRole, "busy"}};
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

void EpisodeListModel::reset(std::vector<Episode> items, std::set<std::string> watched) {
    beginResetModel();
    items_ = std::move(items);
    watched_ = std::move(watched);
    busy_.clear();
    endResetModel();
    emit countChanged();
}

void EpisodeListModel::setWatched(int row) {
    if (row < 0 || row >= count() || isWatched(row)) return;
    watched_.insert(items_[static_cast<size_t>(row)].number);
    emit dataChanged(index(row), index(row), {WatchedRole});
}

void EpisodeListModel::setBusy(int row, bool busy) {
    if (row < 0 || row >= count()) return;
    if (busy) busy_.insert(row);
    else busy_.erase(row);
    emit dataChanged(index(row), index(row), {BusyRole});
}

} // namespace anime::app
