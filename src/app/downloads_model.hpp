#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <vector>
#include "../services/download_service.hpp"

class QQmlEngine;
class QJSEngine;

namespace anime::app {

// Live view of DownloadService jobs, polled while anything is in progress.
class Downloads : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY countChanged)

public:
    enum Role { AnimeRole = Qt::UserRole + 1, EpisodeRole, QualityRole, ProgressRole, SpeedRole,
                EtaRole, ActiveRole, DoneRole, ErrorRole, ErrorTextRole, PathRole };

    static Downloads* instance();
    static Downloads* create(QQmlEngine*, QJSEngine*);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(jobs_.size()); }
    int activeCount() const;

    Q_INVOKABLE void clearFinished();
    Q_INVOKABLE void openFolder(int row) const;

signals:
    void countChanged();

private:
    Downloads();
    void poll();

    std::vector<DownloadJob> jobs_;
    QTimer timer_;
};

} // namespace anime::app
