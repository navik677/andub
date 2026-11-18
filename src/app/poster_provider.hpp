#pragma once

#include <QQuickAsyncImageProvider>
#include <QThreadPool>

namespace anime::app {

// image://poster/<percent-encoded url> : downloads through ImageCache and
// decodes at the requested size on a worker thread.
class PosterProvider : public QQuickAsyncImageProvider {
public:
    PosterProvider();
    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

private:
    QThreadPool pool_;
};

} // namespace anime::app
