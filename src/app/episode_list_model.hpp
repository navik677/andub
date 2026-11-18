#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>
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
    enum Role { NumberRole = Qt::UserRole + 1, TitleRole, LabelRole, WatchedRole, BusyRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(items_.size()); }
    const std::vector<Episode>& items() const { return items_; }
    bool isWatched(int row) const;
    int watchedCount() const;

    void reset(std::vector<Episode> items, std::set<std::string> watched);
    void setWatched(int row);
    void setBusy(int row, bool busy);

signals:
    void countChanged();

private:
    std::vector<Episode> items_;
    std::set<std::string> watched_;
    std::set<int> busy_;
};

} // namespace anime::app
