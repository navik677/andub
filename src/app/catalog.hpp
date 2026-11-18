#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include "anime_list_model.hpp"
#include "details.hpp"

class QQmlEngine;
class QJSEngine;

namespace anime::app {

// Browsing state: provider, search query, genre filter, favorites mode and
// the paginated result list (pages are appended as the grid scrolls).
class Catalog : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(anime::app::AnimeListModel* results READ results CONSTANT)
    Q_PROPERTY(QStringList providers READ providers CONSTANT)
    Q_PROPERTY(int providerIndex READ providerIndex WRITE setProviderIndex NOTIFY providerIndexChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QStringList genres READ genres CONSTANT)
    Q_PROPERTY(QString genre READ genre WRITE setGenre NOTIFY genreChanged)
    Q_PROPERTY(bool favoritesMode READ favoritesMode WRITE setFavoritesMode NOTIFY favoritesModeChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool loadingMore READ loadingMore NOTIFY stateChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY stateChanged)
    Q_PROPERTY(int generation READ generation NOTIFY stateChanged)

public:
    static Catalog* instance();
    static Catalog* create(QQmlEngine*, QJSEngine*);

    AnimeListModel* results() { return &results_; }
    QStringList providers() const;
    int providerIndex() const { return provider_index_; }
    void setProviderIndex(int i);
    QString query() const { return query_; }
    void setQuery(const QString& q);
    QStringList genres() const;
    QString genre() const { return genre_; }
    void setGenre(const QString& g);
    bool favoritesMode() const { return favorites_; }
    void setFavoritesMode(bool on);
    bool loading() const { return loading_; }
    bool loadingMore() const { return loading_more_; }
    bool hasMore() const { return has_more_; }
    // Bumped on every fresh (page 1) result set so the grid can replay its entrance
    int generation() const { return generation_; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE anime::app::Details* openDetails(int row);

    void favoritesEdited();

signals:
    void providerIndexChanged();
    void queryChanged();
    void genreChanged();
    void favoritesModeChanged();
    void stateChanged();

private:
    Catalog();
    void fetch(int page);

    AnimeListModel results_;
    int provider_index_ = 0;
    QString query_;
    QString genre_;
    bool favorites_ = false;
    bool loading_ = false;
    bool loading_more_ = false;
    bool has_more_ = false;
    bool favorites_dirty_ = false;
    int page_ = 1;
    int generation_ = 0;
    quint64 seq_ = 0;
};

} // namespace anime::app
