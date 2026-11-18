#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>
#include <vector>
#include "../models/anime.hpp"

namespace anime::app {

class AnimeListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        TitleRole,
        TitleEnRole,
        YearRole,
        PosterRole,
        RatingRole,
        AgeRatingRole,
        StatusRole,
        ProviderRole,
        ProviderNameRole,
        GenresRole,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(items_.size()); }
    const Anime* at(int row) const;
    bool contains(const Anime& a) const;

    void reset(std::vector<Anime> items);
    void append(std::vector<Anime> items);

    static QString formatRating(const std::string& raw);

signals:
    void countChanged();

private:
    std::vector<Anime> items_;
};

} // namespace anime::app
