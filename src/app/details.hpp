#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <memory>
#include "episode_list_model.hpp"
#include "player_session.hpp"
#include "../models/anime.hpp"
#include "../providers/base_provider.hpp"

namespace anime::app {

// State behind the title page: metadata, episode list, watch progress, favorite flag.
class Details : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by Catalog.openDetails()")

    Q_PROPERTY(QString title READ title NOTIFY infoChanged)
    Q_PROPERTY(QString titleEn READ titleEn NOTIFY infoChanged)
    Q_PROPERTY(QString description READ description NOTIFY infoChanged)
    Q_PROPERTY(int year READ year NOTIFY infoChanged)
    Q_PROPERTY(QString status READ status NOTIFY infoChanged)
    Q_PROPERTY(QString ageRating READ ageRating NOTIFY infoChanged)
    Q_PROPERTY(QString rating READ rating NOTIFY infoChanged)
    Q_PROPERTY(QStringList genres READ genres NOTIFY infoChanged)
    Q_PROPERTY(QString poster READ poster NOTIFY infoChanged)
    Q_PROPERTY(QString providerName READ providerName CONSTANT)
    Q_PROPERTY(bool isFavorite READ isFavorite NOTIFY favoriteChanged)
    Q_PROPERTY(anime::app::EpisodeListModel* episodes READ episodes CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(int watchedCount READ watchedCount NOTIFY progressChanged)
    Q_PROPERTY(int resumeIndex READ resumeIndex NOTIFY progressChanged)
    Q_PROPERTY(QString resumeNumber READ resumeNumber NOTIFY progressChanged)

public:
    Details(Anime anime, std::shared_ptr<BaseProvider> provider, QObject* parent = nullptr);

    QString title() const;
    QString titleEn() const;
    QString description() const { return QString::fromStdString(anime_.description); }
    int year() const { return anime_.year; }
    QString status() const { return QString::fromStdString(anime_.status); }
    QString ageRating() const { return QString::fromStdString(anime_.age_rating); }
    QString rating() const;
    QStringList genres() const;
    QString poster() const { return QString::fromStdString(anime_.poster_url); }
    QString providerName() const;
    bool isFavorite() const { return favorite_; }
    EpisodeListModel* episodes() { return &episodes_; }
    bool loading() const { return loading_; }
    int watchedCount() const { return episodes_.watchedCount(); }
    int resumeIndex() const;
    QString resumeNumber() const;

    Q_INVOKABLE void toggleFavorite();
    Q_INVOKABLE void reload();
    Q_INVOKABLE anime::app::PlayerSession* createSession(int index);
    Q_INVOKABLE void playExternal(int index);
    Q_INVOKABLE void download(int index);

signals:
    void infoChanged();
    void favoriteChanged();
    void loadingChanged();
    void progressChanged();

private:
    void markWatched(int index);

    Anime anime_;
    std::shared_ptr<BaseProvider> provider_;
    EpisodeListModel episodes_;
    bool favorite_ = false;
    bool loading_ = false;
};

} // namespace anime::app
