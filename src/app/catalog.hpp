#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include "anime_list_model.hpp"
#include "details.hpp"
#include "../models/catalog_filter.hpp"

class QQmlEngine;
class QJSEngine;

namespace anime::app {

// Browsing state: provider, search query, filters/sorting, favorites mode and
// the paginated result list (pages are appended as the grid scrolls).
//
// Filters the provider applies on its server go into the request; the rest are
// checked here against the normalized metadata, fetching further pages until a
// screenful of matches is collected (or the source runs out).
class Catalog : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(anime::app::AnimeListModel* results READ results CONSTANT)
    Q_PROPERTY(QStringList providers READ providers CONSTANT)
    Q_PROPERTY(int providerIndex READ providerIndex WRITE setProviderIndex NOTIFY providerIndexChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool favoritesMode READ favoritesMode WRITE setFavoritesMode NOTIFY favoritesModeChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool loadingMore READ loadingMore NOTIFY stateChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY stateChanged)
    Q_PROPERTY(int generation READ generation NOTIFY stateChanged)

    // Filters. Enum values: see SortOrder / Kind / Airing / Season in catalog_filter.hpp
    Q_PROPERTY(QVariantList genres READ genres CONSTANT)            // [{label, value}]
    Q_PROPERTY(QStringList selectedGenres READ selectedGenres WRITE setSelectedGenres NOTIFY filterChanged)
    Q_PROPERTY(int sort READ sort WRITE setSort NOTIFY filterChanged)
    Q_PROPERTY(int yearFrom READ yearFrom WRITE setYearFrom NOTIFY filterChanged)
    Q_PROPERTY(int yearTo READ yearTo WRITE setYearTo NOTIFY filterChanged)
    Q_PROPERTY(int season READ season WRITE setSeason NOTIFY filterChanged)
    Q_PROPERTY(QVariantList kinds READ kinds WRITE setKinds NOTIFY filterChanged)
    Q_PROPERTY(int airing READ airing WRITE setAiring NOTIFY filterChanged)
    Q_PROPERTY(int maxAge READ maxAge WRITE setMaxAge NOTIFY filterChanged)
    Q_PROPERTY(int activeFilterCount READ activeFilterCount NOTIFY filterChanged)
    // What the current source can do, per filter: 2 = on the server, 1 = on
    // loaded results, 0 = not available. Keys: sort, years, season, kinds,
    // airing, genres ("genres" = 1 also means one genre only), age, plus
    // "sortUpdated"/"sortRating"/... for individual sort options.
    Q_PROPERTY(QVariantMap support READ support NOTIFY supportChanged)
    Q_PROPERTY(int minYear READ minYear CONSTANT)
    Q_PROPERTY(int maxYear READ maxYear CONSTANT)

public:
    static Catalog* instance();
    static Catalog* create(QQmlEngine*, QJSEngine*);

    AnimeListModel* results() { return &results_; }
    QStringList providers() const;
    int providerIndex() const { return provider_index_; }
    void setProviderIndex(int i);
    QString query() const { return query_; }
    void setQuery(const QString& q);
    bool favoritesMode() const { return favorites_; }
    void setFavoritesMode(bool on);
    bool loading() const { return loading_; }
    bool loadingMore() const { return loading_more_; }
    bool hasMore() const { return has_more_; }
    // Bumped on every fresh (page 1) result set so the grid can replay its entrance
    int generation() const { return generation_; }

    QVariantList genres() const;
    QStringList selectedGenres() const;
    void setSelectedGenres(const QStringList& g);
    int sort() const { return static_cast<int>(filter_.sort); }
    void setSort(int s);
    int yearFrom() const { return filter_.year_from; }
    void setYearFrom(int y);
    int yearTo() const { return filter_.year_to; }
    void setYearTo(int y);
    int season() const { return static_cast<int>(filter_.season); }
    void setSeason(int s);
    QVariantList kinds() const;
    void setKinds(const QVariantList& k);
    int airing() const { return static_cast<int>(filter_.airing); }
    void setAiring(int a);
    int maxAge() const { return filter_.max_age; }
    void setMaxAge(int a);
    int activeFilterCount() const;
    QVariantMap support() const;
    int minYear() const { return 1970; }
    int maxYear() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void resetFilters();
    // "Каталог" in the navigation rail: the provider's front page, i.e. no
    // search, no filters, default order, not in favorites. One reload at most.
    Q_INVOKABLE void goHome();
    // Back/forward history: everything that defines what the catalog shows
    // (source, search, favorites mode, filters, sort). restoreState() reloads
    // once, and only if something differs.
    Q_INVOKABLE QVariantMap saveState() const;
    Q_INVOKABLE void restoreState(const QVariantMap& state);
    Q_INVOKABLE void toggleGenre(const QString& value);
    Q_INVOKABLE void toggleKind(int kind);
    Q_INVOKABLE anime::app::Details* openDetails(int row);

    void favoritesEdited();

signals:
    void providerIndexChanged();
    void queryChanged();
    void favoritesModeChanged();
    void filterChanged();
    void supportChanged();
    void stateChanged();

private:
    Catalog();
    void fetch(int page);
    void filterEdited();
    // The filter as the current source can honour it (unavailable parts dropped)
    CatalogFilter effectiveFilter() const;

    AnimeListModel results_;
    QTimer filter_debounce_;
    CatalogFilter filter_;
    int provider_index_ = 0;
    QString query_;
    bool favorites_ = false;
    bool loading_ = false;
    bool loading_more_ = false;
    bool has_more_ = false;
    bool favorites_dirty_ = false;
    int page_ = 1;           // last server page fetched
    int generation_ = 0;
    quint64 seq_ = 0;
};

} // namespace anime::app
