#include "catalog.hpp"
#include "details.hpp"
#include "provider_registry.hpp"
#include "settings.hpp"
#include "../services/favorites_manager.hpp"
#include "../utils/str_utils.hpp"
#include <QQmlEngine>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QRegularExpression>

namespace anime::app {

static constexpr int PAGE_SIZE = 30;

static const QStringList& genre_list() {
    static const QStringList list = {
        "Бойові мистецтва|Боевые искусства", "Вампіри|Вампиры", "Гарем|Гарем", "Демони|Демоны",
        "Детектив|Детектив", "Дзьосей|Дзёсей", "Драма|Драма", "Ігри|Игры", "Ісекай|Исекай",
        "Історичне|Исторический", "Кіберпанк|Киберпанк", "Комедія|Комедия", "Магія|Магия",
        "Меха|Меха", "Містика|Мистика", "Музика|Музыка", "Пародія|Пародия",
        "Повсякденність|Повседневность", "Пригоди|Приключения", "Психологічне|Психологическое",
        "Романтика|Романтика", "Надприродне|Сверхъестественное", "Сейнен|Сейнен", "Спорт|Спорт",
        "Суперсила|Супер сила", "Сьодзьо|Сёдзе", "Сьодзьо-ай|Сёдзе-ай", "Сьонен|Сёнен",
        "Трилер|Триллер", "Жахи|Ужасы", "Фантастика|Фантастика", "Фентезі|Фэнтези",
        "Школа|Школа", "Екшн|Экшен", "Етті|Этти",
    };
    return list;
}

Catalog* Catalog::instance() {
    static auto* c = new Catalog();
    return c;
}

Catalog* Catalog::create(QQmlEngine*, QJSEngine*) {
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Catalog::Catalog() : results_(this) {
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

// Each entry is "Ukrainian label|catalog value": providers index genres in Russian.
QStringList Catalog::genres() const {
    return genre_list();
}

void Catalog::setProviderIndex(int i) {
    const auto& all = ProviderRegistry::all();
    if (i < 0 || i >= static_cast<int>(all.size()) || i == provider_index_) return;
    provider_index_ = i;
    Settings::instance()->setProvider(QString::fromStdString(all[static_cast<size_t>(i)]->name()));
    emit providerIndexChanged();
    if (!favorites_) refresh();
}

void Catalog::setQuery(const QString& q) {
    if (q == query_) return;
    query_ = q;
    emit queryChanged();
    refresh();
}

void Catalog::setGenre(const QString& g) {
    if (g == genre_) return;
    genre_ = g;
    emit genreChanged();
    refresh();
}

void Catalog::setFavoritesMode(bool on) {
    if (on == favorites_) {
        if (on && favorites_dirty_) refresh();
        return;
    }
    favorites_ = on;
    emit favoritesModeChanged();
    refresh();
}

void Catalog::favoritesEdited() {
    favorites_dirty_ = true;
}

void Catalog::refresh() {
    fetch(1);
}

void Catalog::loadMore() {
    if (loading_ || loading_more_ || !has_more_ || favorites_) return;
    fetch(page_ + 1);
}

void Catalog::fetch(int page) {
    favorites_dirty_ = false;
    const quint64 seq = ++seq_;
    if (page == 1) loading_ = true;
    else loading_more_ = true;
    emit stateChanged();

    // "жанр: X" / "genre: X" typed into the search box acts as a genre filter
    std::string text = query_.trimmed().toStdString();
    std::string genre = genre_.toStdString();
    {
        const QString q = query_.trimmed();
        static const QRegularExpression prefix(QStringLiteral(R"((?:жанр|genre):\s*(\S+))"),
                                               QRegularExpression::CaseInsensitiveOption);
        auto m = prefix.match(q);
        if (m.hasMatch()) {
            genre = m.captured(1).toStdString();
            text = QString(q).remove(m.capturedStart(), m.capturedLength()).trimmed().toStdString();
        }
    }

    const bool favs = favorites_;
    auto provider = ProviderRegistry::all()[static_cast<size_t>(provider_index_)];

    QtConcurrent::run([=]() -> std::vector<Anime> {
        if (favs) {
            std::vector<Anime> out;
            for (auto& a : FavoritesManager::get_favorites()) {
                bool q_ok = text.empty() || utils::utf8_contains_ci(a.title_ru, text) ||
                            utils::utf8_contains_ci(a.title_en, text);
                bool g_ok = genre.empty();
                for (const auto& g : a.genres) {
                    if (!g_ok && utils::utf8_contains_ci(g, genre)) g_ok = true;
                }
                if (q_ok && g_ok) out.push_back(std::move(a));
            }
            return out;
        }
        try {
            return provider->search(text, PAGE_SIZE, genre, page);
        } catch (...) {
            return {};
        }
    }).then(this, [this, seq, page, favs](std::vector<Anime> items) {
        if (seq != seq_) return; // superseded by a newer request
        loading_ = false;
        loading_more_ = false;
        if (page == 1) {
            has_more_ = !favs && static_cast<int>(items.size()) >= PAGE_SIZE;
            results_.reset(std::move(items));
            ++generation_;
        } else {
            // Some providers ignore the page number; stop once a page brings nothing new
            std::vector<Anime> fresh;
            for (auto& a : items) {
                if (!results_.contains(a)) fresh.push_back(std::move(a));
            }
            has_more_ = !fresh.empty() && static_cast<int>(items.size()) >= PAGE_SIZE;
            results_.append(std::move(fresh));
        }
        page_ = page;
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
