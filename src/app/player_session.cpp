#include "player_session.hpp"
#include "notifier.hpp"
#include "../services/history_manager.hpp"
#include "../services/player_service.hpp"
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QThreadPool>

namespace anime::app {

PlayerSession::PlayerSession(Anime anime, std::vector<Episode> episodes, int index,
                             std::shared_ptr<BaseProvider> provider, QObject* parent)
    : QObject(parent), anime_(std::move(anime)), episodes_(std::move(episodes)),
      provider_(std::move(provider)), index_(index) {}

QString PlayerSession::animeTitle() const {
    return QString::fromStdString(anime_.title_ru.empty() ? anime_.title_en : anime_.title_ru);
}

QString PlayerSession::episodeTitle() const {
    if (index_ < 0 || index_ >= count()) return {};
    const Episode& ep = episodes_[static_cast<size_t>(index_)];
    QString s = QStringLiteral("Серія ") + QString::fromStdString(ep.number);
    if (!ep.title.empty() && ep.title != ep.number) s += QStringLiteral(". ") + QString::fromStdString(ep.title);
    return s;
}

QVariantList PlayerSession::episodes() const {
    QVariantList list;
    for (const auto& ep : episodes_) {
        list.append(QVariantMap{
            {"number", QString::fromStdString(ep.number)},
            {"title", ep.title == ep.number ? QString() : QString::fromStdString(ep.title)},
        });
    }
    return list;
}

void PlayerSession::start(int index) {
    if (index < 0 || index >= count()) return;
    const bool changed = index != index_;
    index_ = index;
    resolving_ = true;
    error_.clear();
    quality_.clear();
    stream_ = {};
    if (changed) emit indexChanged();
    emit stateChanged();

    const Episode ep = episodes_[static_cast<size_t>(index)];
    HistoryManager::mark_watched(anime_.provider, anime_.id, ep.number);
    emit episodeWatched(index);

    const int req = ++request_;
    QtConcurrent::run([anime = anime_, ep, prov = provider_]() -> Stream {
        try {
            return prov->get_stream(anime, ep);
        } catch (...) {
            return {};
        }
    }).then(this, [this, req](Stream s) {
        if (req != request_) return; // user already switched episodes
        resolving_ = false;
        const Quality* q = s.best();
        if (!q || q->url.empty()) {
            error_ = QStringLiteral("Не вдалося отримати відео для цієї серії");
            emit stateChanged();
            return;
        }
        std::string url = q->url;
        if (auto p = url.find(":hls:manifest.m3u8"); p != std::string::npos) url = url.substr(0, p);
        QVariantMap headers;
        for (const auto& [k, v] : q->headers) headers.insert(QString::fromStdString(k), QString::fromStdString(v));
        quality_ = QString::fromStdString(q->label);
        stream_ = std::move(s);
        emit stateChanged();
        emit streamReady(QString::fromStdString(url), headers);
    });
}

void PlayerSession::openExternal(double position) {
    if (index_ < 0 || index_ >= count()) return;
    const Episode ep = episodes_[static_cast<size_t>(index_)];
    if (const Quality* q = stream_.best(); q && !q->url.empty()) {
        if (!PlayerService::play_external(*q, anime_.title_ru, &ep, position))
            Notifier::post(QStringLiteral("Не вдалося запустити mpv. Перевірте, що його встановлено."), true);
        return;
    }
    QThreadPool::globalInstance()->start([anime = anime_, ep, prov = provider_, position] {
        try {
            Stream s = prov->get_stream(anime, ep);
            if (const Quality* q = s.best(); q && !q->url.empty()) {
                if (!PlayerService::play_external(*q, anime.title_ru, &ep, position))
                    Notifier::post(QStringLiteral("Не вдалося запустити mpv. Перевірте, що його встановлено."), true);
                return;
            }
        } catch (...) {}
        Notifier::post(QStringLiteral("Не вдалося отримати відео для цієї серії"), true);
    });
}

} // namespace anime::app
