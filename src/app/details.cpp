#include "details.hpp"
#include "anime_list_model.hpp"
#include "catalog.hpp"
#include "notifier.hpp"
#include "player_session.hpp"
#include "provider_registry.hpp"
#include "../services/download_service.hpp"
#include "../services/favorites_manager.hpp"
#include "../services/history_manager.hpp"
#include "../services/player_service.hpp"
#include <QQmlEngine>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>

namespace anime::app {

Details::Details(Anime anime, std::shared_ptr<BaseProvider> provider, QObject* parent)
    : QObject(parent), anime_(std::move(anime)), provider_(std::move(provider)), episodes_(this) {
    favorite_ = FavoritesManager::is_favorite(anime_.provider, anime_.id);
    connect(&episodes_, &QAbstractItemModel::dataChanged, this, &Details::progressChanged);
    connect(&episodes_, &QAbstractItemModel::modelReset, this, &Details::progressChanged);
    reload();

    // Some providers only return the full synopsis from the title page
    if (anime_.description.empty()) {
        QtConcurrent::run([a = anime_, p = provider_]() -> Anime {
            try { return p->get_details(a); } catch (...) { return a; }
        }).then(this, [this](Anime full) {
            bool changed = false;
            if (anime_.description.empty() && !full.description.empty()) {
                anime_.description = full.description;
                changed = true;
            }
            if (anime_.poster_url.empty() && !full.poster_url.empty()) {
                anime_.poster_url = full.poster_url;
                changed = true;
            }
            if (changed) emit infoChanged();
        });
    }
}

QString Details::title() const {
    return QString::fromStdString(anime_.title_ru.empty() ? anime_.title_en : anime_.title_ru);
}

QString Details::titleEn() const {
    return anime_.title_en == anime_.title_ru ? QString() : QString::fromStdString(anime_.title_en);
}

QString Details::rating() const {
    return AnimeListModel::formatRating(anime_.rating);
}

QStringList Details::genres() const {
    QStringList g;
    for (const auto& s : anime_.genres) g << QString::fromStdString(s);
    return g;
}

QString Details::providerName() const {
    return QString::fromStdString(ProviderRegistry::display_name(anime_.provider));
}

int Details::resumeIndex() const {
    // An episode stopped midway wins, the most recently watched one first...
    const int in_progress = episodes_.lastInProgress();
    if (in_progress >= 0) return in_progress;
    // ...otherwise the episode after the last watched one
    int last = -1;
    for (int i = 0; i < episodes_.count(); ++i) {
        if (episodes_.isWatched(i)) last = i;
    }
    if (last < 0) return 0;
    return std::min(last + 1, episodes_.count() - 1);
}

double Details::resumePosition() const {
    const int i = episodes_.lastInProgress();
    return i >= 0 && i == resumeIndex() ? episodes_.progress(i).position : 0;
}

QString Details::resumeNumber() const {
    const int i = resumeIndex();
    if (i < 0 || i >= episodes_.count()) return {};
    return QString::fromStdString(episodes_.items()[static_cast<size_t>(i)].number);
}

void Details::reload() {
    loading_ = true;
    emit loadingChanged();
    QtConcurrent::run([a = anime_, p = provider_]() -> std::vector<Episode> {
        try { return p->get_episodes(a); } catch (...) { return {}; }
    }).then(this, [this](std::vector<Episode> eps) {
        std::map<std::string, EpisodeListModel::Progress> progress;
        for (const auto& [num, p] : HistoryManager::get_progress(anime_.provider, anime_.id)) {
            progress[num] = {p.duration > 0 ? p.position / p.duration : 0, p.position, p.updated};
        }
        episodes_.reset(std::move(eps), HistoryManager::get_watched_episodes(anime_.provider, anime_.id),
                        std::move(progress));
        loading_ = false;
        emit loadingChanged();
    });
}

void Details::toggleFavorite() {
    favorite_ = FavoritesManager::toggle_favorite(anime_);
    emit favoriteChanged();
    Catalog::instance()->favoritesEdited();
}

void Details::markWatched(int index) {
    episodes_.setWatched(index);
}

void Details::progressSaved(int index, double fraction) {
    // Mirror what the session just saved (it stores position - 3 s)
    const auto all = HistoryManager::get_progress(anime_.provider, anime_.id);
    if (index < 0 || index >= episodes_.count()) return;
    auto it = all.find(episodes_.items()[static_cast<size_t>(index)].number);
    episodes_.setProgress(index, fraction, it != all.end() ? it->second.position : 0);
}

void Details::resetProgress() {
    HistoryManager::reset_title(anime_.provider, anime_.id);
    episodes_.reset(episodes_.items(), {}, {});
}

PlayerSession* Details::createSession(int index) {
    if (index < 0 || index >= episodes_.count()) return nullptr;
    auto* s = new PlayerSession(anime_, episodes_.items(), index, provider_);
    QQmlEngine::setObjectOwnership(s, QQmlEngine::JavaScriptOwnership);
    connect(s, &PlayerSession::episodeWatched, this, &Details::markWatched);
    connect(s, &PlayerSession::progressSaved, this, &Details::progressSaved);
    return s;
}

void Details::playExternal(int index) {
    if (index < 0 || index >= episodes_.count()) return;
    const Episode ep = episodes_.items()[static_cast<size_t>(index)];
    episodes_.setBusy(index, true);
    QtConcurrent::run([a = anime_, ep, p = provider_]() -> bool {
        try {
            Stream s = p->get_stream(a, ep);
            const Quality* q = s.best();
            if (!q || q->url.empty()) return false;
            if (!PlayerService::play_external(*q, a.title_ru, &ep)) {
                Notifier::post(QStringLiteral("Не вдалося запустити mpv. Перевірте, що його встановлено."), true);
                return true;
            }
            HistoryManager::mark_watched(a.provider, a.id, ep.number);
            return true;
        } catch (...) {
            return false;
        }
    }).then(this, [this, index](bool ok) {
        episodes_.setBusy(index, false);
        if (ok) episodes_.setWatched(index);
        else Notifier::post(QStringLiteral("Не вдалося отримати відео для цієї серії"), true);
    });
}

void Details::download(int index) {
    if (index < 0 || index >= episodes_.count()) return;
    const Episode ep = episodes_.items()[static_cast<size_t>(index)];
    episodes_.setBusy(index, true);
    QtConcurrent::run([a = anime_, ep, p = provider_]() -> bool {
        try {
            Stream s = p->get_stream(a, ep);
            const Quality* q = s.best();
            if (!q || q->url.empty()) return false;
            DownloadService::instance().start_download(a, ep, q->label, q->url, q->headers);
            return true;
        } catch (...) {
            return false;
        }
    }).then(this, [this, index, label = QString::fromStdString(ep.display_title())](bool ok) {
        episodes_.setBusy(index, false);
        if (ok) Notifier::post(label + QStringLiteral(": завантаження почалося"));
        else Notifier::post(QStringLiteral("Не вдалося отримати відео для завантаження"), true);
    });
}

} // namespace anime::app
