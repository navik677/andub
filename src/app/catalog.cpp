#include "catalog.hpp"
#include "details.hpp"
#include "provider_registry.hpp"
#include "settings.hpp"
#include "../services/favorites_manager.hpp"
#include "../utils/str_utils.hpp"
#include "../models/catalog_match.hpp"
#include <QDate>
#include <unordered_set>
#include <QQmlEngine>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QRegularExpression>

namespace anime::app {

static constexpr int PAGE_SIZE = 30;

// Pages scanned per request when results are filtered on the client: enough
// to fill a screen for most filters without hammering the site.
static constexpr int MAX_SCAN_PAGES = 6;

Catalog* Catalog::instance() {
    static auto* c = new Catalog();
    return c;
}

Catalog* Catalog::create(QQmlEngine*, QJSEngine*) {
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Catalog::Catalog() : results_(this) {
    // Several controls usually change together (reset, dragging the year range)
    filter_debounce_.setSingleShot(true);
    filter_debounce_.setInterval(250);
    connect(&filter_debounce_, &QTimer::timeout, this, &Catalog::refresh);

    const QString saved = Settings::instance()->provider();
    const auto& all = ProviderRegistry::all();
    for (size_t i = 0; i < all.size(); ++i) {
        if (QString::fromStdString(all[i]->name()) == saved) provider_index_ = static_cast<int>(i);
    }
}

QStringList Catalog::providers() const {
    QStringList names;
    for (const auto& p : ProviderRegistry::all()) names << QString::fromStdString(p->display_name());
    return names;
}

QVariantList Catalog::genres() const {
    QVariantList out;
    for (const auto& g : genre_table())
        out << QVariantMap{{"label", QString::fromUtf8(g.uk)}, {"value", QString::fromUtf8(g.ru)}};
    return out;
}

QStringList Catalog::selectedGenres() const {
    QStringList out;
    for (const auto& g : filter_.genres) out << QString::fromStdString(g);
    return out;
}

void Catalog::setSelectedGenres(const QStringList& g) {
    std::vector<std::string> v;
    for (const auto& s : g) v.push_back(s.toStdString());
    if (v == filter_.genres) return;
    filter_.genres = std::move(v);
    filterEdited();
}

void Catalog::toggleGenre(const QString& value) {
    QStringList g = selectedGenres();
    if (g.contains(value)) g.removeAll(value);
    else if (!support().value(QStringLiteral("genresMulti")).toBool()) g = {value}; // one genre only
    else g << value;
    setSelectedGenres(g);
}

void Catalog::setSort(int s) {
    if (s == sort() || s < 0 || s > static_cast<int>(SortOrder::Title)) return;
    filter_.sort = static_cast<SortOrder>(s);
    filterEdited();
}

int Catalog::maxYear() const {
    return QDate::currentDate().year() + 1;
}

void Catalog::setYearFrom(int y) {
    if (y <= minYear()) y = 0;
    if (y == filter_.year_from) return;
    filter_.year_from = y;
    filterEdited();
}

void Catalog::setYearTo(int y) {
    if (y >= maxYear()) y = 0;
    if (y == filter_.year_to) return;
    filter_.year_to = y;
    filterEdited();
}

void Catalog::setSeason(int s) {
    if (s == season() || s < 0 || s > static_cast<int>(Season::Autumn)) return;
    filter_.season = static_cast<Season>(s);
    filterEdited();
}

QVariantList Catalog::kinds() const {
    QVariantList out;
    for (Kind k : filter_.kinds) out << static_cast<int>(k);
    return out;
}

void Catalog::setKinds(const QVariantList& k) {
    std::vector<Kind> v;
    for (const auto& x : k) {
        const int i = x.toInt();
        if (i > 0 && i <= static_cast<int>(Kind::Special)) v.push_back(static_cast<Kind>(i));
    }
    if (v == filter_.kinds) return;
    filter_.kinds = std::move(v);
    filterEdited();
}

void Catalog::toggleKind(int kind) {
    QVariantList k = kinds();
    if (k.contains(kind)) k.removeAll(kind);
    else k << kind;
    setKinds(k);
}

void Catalog::setAiring(int a) {
    if (a == airing() || a < 0 || a > static_cast<int>(Airing::Announced)) return;
    filter_.airing = static_cast<Airing>(a);
    filterEdited();
}

void Catalog::setMaxAge(int a) {
    if (a == filter_.max_age) return;
    filter_.max_age = a;
    filterEdited();
}

int Catalog::activeFilterCount() const {
    const CatalogFilter f = effectiveFilter();
    int n = 0;
    if (f.has_years()) ++n;
    if (f.season != Season::Unknown) ++n;
    if (!f.kinds.empty()) ++n;
    if (f.airing != Airing::Unknown) ++n;
    if (f.max_age > 0) ++n;
    n += static_cast<int>(f.genres.size());
    return n;
}

void Catalog::resetFilters() {
    if (filter_.empty_filters()) return;
    const SortOrder keep = filter_.sort;
    filter_ = CatalogFilter{};
    filter_.sort = keep;
    filterEdited();
}

void Catalog::filterEdited() {
    emit filterChanged();
    filter_debounce_.start();
}

static void source_support(bool favorites, const BaseProvider& p, FilterSupport& server, MetaSupport& meta) {
    if (favorites) {
        // Saved titles carry whatever their source provided; filter them locally
        server = FilterSupport{};
        meta = MetaSupport{true, true, true, true, true, true, true};
    } else {
        server = p.server_filters();
        meta = p.list_meta();
    }
}

QVariantMap Catalog::support() const {
    FilterSupport server;
    MetaSupport meta;
    source_support(favorites_, *ProviderRegistry::all()[static_cast<size_t>(provider_index_)], server, meta);
    return {
        {"sort", 1},
        {"sortUpdated", server.sort ? 2 : 0},
        {"sortRating", filter_state(server.sort, meta.score)},
        {"sortYear", filter_state(server.sort, meta.year)},
        {"sortTitle", filter_state(server.sort, true)},
        {"years", filter_state(server.years, meta.year)},
        {"season", filter_state(server.season, meta.season)},
        {"kinds", filter_state(server.kinds, meta.kind)},
        {"airing", filter_state(server.airing, meta.airing)},
        // Every source can search by at least one genre (the base browse() passes it on)
        {"genres", server.genres ? 2 : 1},
        {"genresMulti", server.genres || meta.genres},
        {"age", filter_state(server.age, meta.age)},
    };
}

CatalogFilter Catalog::effectiveFilter() const {
    CatalogFilter f = filter_;
    const QVariantMap s = support();
    auto on = [&](const char* k) { return s.value(QString::fromLatin1(k)).toInt() > 0; };
    if (!on("years")) f.year_from = f.year_to = 0;
    if (!on("season")) f.season = Season::Unknown;
    if (!on("kinds")) f.kinds.clear();
    if (!on("airing")) f.airing = Airing::Unknown;
    if (!on("age")) f.max_age = 0;
    if (!s.value(QStringLiteral("genresMulti")).toBool() && f.genres.size() > 1) f.genres.resize(1);
    bool sort_ok = true;
    switch (f.sort) {
    case SortOrder::Updated: sort_ok = on("sortUpdated"); break;
    case SortOrder::Rating: sort_ok = on("sortRating"); break;
    case SortOrder::YearAsc:
    case SortOrder::YearDesc: sort_ok = on("sortYear"); break;
    default: break;
    }
    if (!sort_ok) f.sort = SortOrder::Default;
    return f;
}

void Catalog::setProviderIndex(int i) {
    const auto& all = ProviderRegistry::all();
    if (i < 0 || i >= static_cast<int>(all.size()) || i == provider_index_) return;
    provider_index_ = i;
    Settings::instance()->setProvider(QString::fromStdString(all[static_cast<size_t>(i)]->name()));
    emit providerIndexChanged();
    emit supportChanged();
    emit filterChanged(); // activeFilterCount depends on what the source supports
    if (!favorites_) refresh();
}

void Catalog::setQuery(const QString& q) {
    if (q == query_) return;
    query_ = q;
    emit queryChanged();
    refresh();
}

void Catalog::setFavoritesMode(bool on) {
    if (on == favorites_) {
        if (on && favorites_dirty_) refresh();
        return;
    }
    favorites_ = on;
    emit favoritesModeChanged();
    emit supportChanged();
    emit filterChanged();
    refresh();
}

void Catalog::favoritesEdited() {
    favorites_dirty_ = true;
}

void Catalog::refresh() {
    filter_debounce_.stop();
    fetch(1);
}

void Catalog::loadMore() {
    if (loading_ || loading_more_ || !has_more_ || favorites_) return;
    fetch(page_ + 1);
}

namespace {
struct FetchResult {
    std::vector<Anime> items;
    int last_page = 1;
    bool exhausted = false;
};
} // namespace

void Catalog::fetch(int page) {
    favorites_dirty_ = false;
    const quint64 seq = ++seq_;
    if (page == 1) loading_ = true;
    else loading_more_ = true;
    emit stateChanged();

    // "жанр: X" / "genre: X" typed into the search box adds a genre filter
    std::string text = query_.trimmed().toStdString();
    CatalogFilter filter = effectiveFilter();
    {
        const QString q = query_.trimmed();
        static const QRegularExpression prefix(QStringLiteral(R"((?:жанр|genre):\s*(\S+))"),
                                               QRegularExpression::CaseInsensitiveOption);
        auto m = prefix.match(q);
        if (m.hasMatch()) {
            filter.genres.insert(filter.genres.begin(), m.captured(1).toStdString());
            text = QString(q).remove(m.capturedStart(), m.capturedLength()).trimmed().toStdString();
        }
    }

    const bool favs = favorites_;
    auto provider = ProviderRegistry::all()[static_cast<size_t>(provider_index_)];
    FilterSupport server;
    MetaSupport meta;
    source_support(favs, *provider, server, meta);

    // Anything left for the client to check? Then a server page may hold only
    // a few matches, and further pages are scanned to fill the screen.
    CatalogFilter client = filter;
    if (server.years) client.year_from = client.year_to = 0;
    if (server.season) client.season = Season::Unknown;
    if (server.kinds) client.kinds.clear();
    if (server.airing) client.airing = Airing::Unknown;
    if (server.age) client.max_age = 0;
    if (server.genres || !meta.genres) client.genres.clear();
    const bool client_filtering = !client.empty_filters();

    QtConcurrent::run([=]() -> FetchResult {
        FetchResult r;
        if (favs) {
            for (auto& a : FavoritesManager::get_favorites()) {
                const bool q_ok = text.empty() || utils::utf8_contains_ci(a.title_ru, text) ||
                                  utils::utf8_contains_ci(a.title_en, text);
                if (q_ok && matches(a, filter, server, meta)) r.items.push_back(std::move(a));
            }
            r.exhausted = true;
            return r;
        }
        std::unordered_set<std::string> seen;
        int p = page;
        for (int scanned = 0; scanned < MAX_SCAN_PAGES; ++scanned, ++p) {
            std::vector<Anime> batch;
            try {
                batch = provider->browse(text, PAGE_SIZE, filter, p);
            } catch (...) {
            }
            r.last_page = p;
            int fresh = 0;
            for (auto& a : batch) {
                if (!seen.insert(a.id).second) continue;
                ++fresh;
                if (!client_filtering || matches(a, filter, server, meta)) r.items.push_back(std::move(a));
            }
            // A short page, or a provider that ignores the page number
            if (static_cast<int>(batch.size()) < PAGE_SIZE || fresh == 0) {
                r.exhausted = true;
                break;
            }
            if (!client_filtering || static_cast<int>(r.items.size()) >= PAGE_SIZE) break;
        }
        return r;
    }).then(this, [this, seq, page, filter, server](FetchResult r) {
        if (seq != seq_) return; // superseded by a newer request
        loading_ = false;
        loading_more_ = false;
        const bool client_sort = filter.sort != SortOrder::Default && !server.sort;
        if (page == 1) {
            if (client_sort) sort_items(r.items, filter.sort);
            results_.reset(std::move(r.items));
            ++generation_;
        } else {
            std::vector<Anime> fresh;
            for (auto& a : r.items) {
                if (!results_.contains(a)) fresh.push_back(std::move(a));
            }
            results_.append(std::move(fresh));
            if (client_sort) results_.sortBy(filter.sort);
        }
        has_more_ = !r.exhausted;
        page_ = r.last_page;
        emit stateChanged();
    });
}

Details* Catalog::openDetails(int row) {
    const Anime* a = results_.at(row);
    if (!a) return nullptr;
    auto prov = favorites_ ? ProviderRegistry::by_name(a->provider)
                           : ProviderRegistry::all()[static_cast<size_t>(provider_index_)];
    if (!a->provider.empty()) prov = ProviderRegistry::by_name(a->provider);
    auto* d = new Details(*a, prov);
    QQmlEngine::setObjectOwnership(d, QQmlEngine::JavaScriptOwnership);
    return d;
}

} // namespace anime::app
