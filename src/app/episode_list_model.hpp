#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "../models/episode.hpp"

namespace anime::app {

class EpisodeListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { NumberRole = Qt::UserRole + 1, TitleRole, LabelRole, WatchedRole, BusyRole, ProgressRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(items_.size()); }
    const std::vector<Episode>& items() const { return items_; }
    bool isWatched(int row) const;
    int watchedCount() const;

    // Where the viewer stopped in an unfinished episode
    struct Progress {
        double fraction = 0;   // 0..1, 0 if the duration is unknown
        double position = 0;   // seconds
        long long updated = 0; // unix time
    };

    void reset(std::vector<Episode> items, std::set<std::string> watched, std::map<std::string, Progress> progress = {});
    void setWatched(int row);
    void setProgress(int row, double fraction, double position);
    Progress progress(int row) const;
    // Row with the most recently saved position, -1 if none
    int lastInProgress() const;
    void setBusy(int row, bool busy);

signals:
    void countChanged();

private:
    std::vector<Episode> items_;
    std::set<std::string> watched_;
    std::map<std::string, Progress> progress_;
    std::set<int> busy_;
};

} // namespace anime::app
