#include "anime_list_model.hpp"
#include "provider_registry.hpp"
#include "../models/catalog_match.hpp"
#include <QStringList>

namespace anime::app {

int AnimeListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : count();
}

QHash<int, QByteArray> AnimeListModel::roleNames() const {
    return {
        {KeyRole, "key"},
        {TitleRole, "title"},
        {TitleEnRole, "titleEn"},
        {YearRole, "year"},
        {PosterRole, "poster"},
        {RatingRole, "rating"},
        {AgeRatingRole, "ageRating"},
        {StatusRole, "status"},
        {ProviderRole, "provider"},
        {ProviderNameRole, "providerName"},
        {GenresRole, "genres"},
    };
}

QString AnimeListModel::formatRating(const std::string& raw) {
    // Providers report either a score ("8.4", "★ 8") or a popularity count ("12500")
    QString r = QString::fromStdString(raw).remove(QStringLiteral("★")).trimmed();
    if (r.isEmpty()) return {};
    bool ok = false;
    const double v = r.toDouble(&ok);
    if (!ok) return r;
    if (v <= 0) return {};
    if (v >= 1000) return QString::number(v / 1000.0, 'f', 1) + QStringLiteral("k");
    if (v == static_cast<long long>(v)) return QString::number(static_cast<long long>(v));
    return QString::number(v, 'f', 1);
}

QVariant AnimeListModel::data(const QModelIndex& index, int role) const {
    const Anime* a = at(index.row());
    if (!a) return {};
    switch (role) {
    case KeyRole: return QString::fromStdString(a->provider + ":" + a->id);
    case TitleRole: return QString::fromStdString(a->title_ru.empty() ? a->title_en : a->title_ru);
    case TitleEnRole: return QString::fromStdString(a->title_en != a->title_ru ? a->title_en : "");
    case YearRole: return a->year;
    case PosterRole: return QString::fromStdString(a->poster_url);
    case RatingRole: return formatRating(a->rating);
    case AgeRatingRole: return QString::fromStdString(a->age_rating);
    case StatusRole: return QString::fromStdString(a->status);
    case ProviderRole: return QString::fromStdString(a->provider);
    case ProviderNameRole: return QString::fromStdString(ProviderRegistry::display_name(a->provider));
    case GenresRole: {
        QStringList g;
        for (const auto& s : a->genres) g << QString::fromStdString(s);
        return g;
    }
    default: return {};
    }
}

const Anime* AnimeListModel::at(int row) const {
    if (row < 0 || row >= count()) return nullptr;
    return &items_[static_cast<size_t>(row)];
}

bool AnimeListModel::contains(const Anime& a) const {
    for (const auto& it : items_) {
        if (it.id == a.id && it.provider == a.provider) return true;
    }
    return false;
}

void AnimeListModel::reset(std::vector<Anime> items) {
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
    emit countChanged();
}

void AnimeListModel::append(std::vector<Anime> items) {
    if (items.empty()) return;
    const int first = count();
    beginInsertRows({}, first, first + static_cast<int>(items.size()) - 1);
    for (auto& a : items) items_.push_back(std::move(a));
    endInsertRows();
    emit countChanged();
}

void AnimeListModel::sortBy(SortOrder order) {
    if (order == SortOrder::Default || order == SortOrder::Updated || items_.size() < 2) return;
    emit layoutAboutToBeChanged();
    sort_items(items_, order);
    emit layoutChanged();
}

} // namespace anime::app
