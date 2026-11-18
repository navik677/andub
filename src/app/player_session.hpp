#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>
#include <vector>
#include "../models/anime.hpp"
#include "../models/episode.hpp"
#include "../models/stream.hpp"
#include "../providers/base_provider.hpp"

namespace anime::app {

// One viewing session in the embedded player: resolves streams for the
// current episode and moves between episodes. The video itself is owned by
// MpvPlayer; the page wires streamReady() into MpvPlayer.load().
class PlayerSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by Details.createSession()")

    Q_PROPERTY(QString animeTitle READ animeTitle CONSTANT)
    Q_PROPERTY(QString episodeTitle READ episodeTitle NOTIFY indexChanged)
    Q_PROPERTY(int index READ index NOTIFY indexChanged)
    Q_PROPERTY(int count READ count CONSTANT)
    Q_PROPERTY(bool hasPrev READ hasPrev NOTIFY indexChanged)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY indexChanged)
    Q_PROPERTY(bool resolving READ resolving NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString quality READ quality NOTIFY stateChanged)
    Q_PROPERTY(QVariantList episodes READ episodes CONSTANT)

public:
    PlayerSession(Anime anime, std::vector<Episode> episodes, int index,
                  std::shared_ptr<BaseProvider> provider, QObject* parent = nullptr);

    QString animeTitle() const;
    QString episodeTitle() const;
    int index() const { return index_; }
    int count() const { return static_cast<int>(episodes_.size()); }
    bool hasPrev() const { return index_ > 0; }
    bool hasNext() const { return index_ + 1 < count(); }
    bool resolving() const { return resolving_; }
    QString error() const { return error_; }
    QString quality() const { return quality_; }
    QVariantList episodes() const;

    Q_INVOKABLE void start(int index);
    Q_INVOKABLE void next() { if (hasNext()) start(index_ + 1); }
    Q_INVOKABLE void prev() { if (hasPrev()) start(index_ - 1); }
    Q_INVOKABLE void retry() { start(index_); }
    // Hands the current stream to an external mpv window, resuming at `position`.
    Q_INVOKABLE void openExternal(double position);

signals:
    void indexChanged();
    void stateChanged();
    void streamReady(const QString& url, const QVariantMap& headers);
    void episodeWatched(int index);

private:
    Anime anime_;
    std::vector<Episode> episodes_;
    std::shared_ptr<BaseProvider> provider_;
    int index_ = 0;
    int request_ = 0;
    bool resolving_ = false;
    QString error_;
    QString quality_;
    Stream stream_;
};

} // namespace anime::app
