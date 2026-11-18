#include "poster_provider.hpp"
#include "../services/image_cache.hpp"
#include <QImage>
#include <QImageReader>
#include <QRunnable>
#include <QUrl>

namespace anime::app {

namespace {

class PosterResponse : public QQuickImageResponse {
public:
    void deliver(QImage img) {
        image_ = std::move(img);
        emit finished();
    }
    QQuickTextureFactory* textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(image_);
    }
    QString errorString() const override {
        return image_.isNull() ? QStringLiteral("poster unavailable") : QString();
    }

private:
    QImage image_;
};

QImage load_scaled(const QString& path, const QSize& req) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);
    const QSize src = reader.size();
    if (src.isValid() && (req.width() > 0 || req.height() > 0)) {
        QSize target = src;
        if (req.width() > 0 && req.height() > 0) {
            target = src.scaled(req, Qt::KeepAspectRatioByExpanding);
        } else if (req.width() > 0) {
            target = QSize(req.width(), src.height() * req.width() / std::max(1, src.width()));
        } else {
            target = QSize(src.width() * req.height() / std::max(1, src.height()), req.height());
        }
        if (target.width() < src.width()) reader.setScaledSize(target);
    }
    return reader.read();
}

} // namespace

PosterProvider::PosterProvider() : QQuickAsyncImageProvider() {
    pool_.setMaxThreadCount(6); // network bound
}

QQuickImageResponse* PosterProvider::requestImageResponse(const QString& id, const QSize& requestedSize) {
    auto* response = new PosterResponse();
    const QString url = QUrl::fromPercentEncoding(id.toUtf8());
    pool_.start([response, url, requestedSize] {
        const std::string path = anime::ImageCache::ensure_image(url.toStdString());
        QImage img = path.empty() ? QImage() : load_scaled(QString::fromStdString(path), requestedSize);
        QMetaObject::invokeMethod(response, [response, img = std::move(img)]() mutable {
            response->deliver(std::move(img));
        }, Qt::QueuedConnection);
    });
    return response;
}

} // namespace anime::app
