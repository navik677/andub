#pragma once

#include <QQuickFramebufferObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>

namespace anime::app {

struct MpvShared;
class ScreenInhibitor;

// libmpv rendered into a Qt Quick item through mpv's OpenGL render API.
// Requires the scene graph to run on OpenGL (set in main()).
class MpvPlayer : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double buffered READ buffered NOTIFY bufferedChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY hasMediaChanged)
    Q_PROPERTY(QString upscaler READ upscaler WRITE setUpscaler NOTIFY upscalerChanged)

public:
    explicit MpvPlayer(QQuickItem* parent = nullptr);
    ~MpvPlayer() override;

    Renderer* createRenderer() const override;

    double position() const { return position_; }
    double duration() const { return duration_; }
    double buffered() const { return buffered_; }
    bool paused() const { return paused_; }
    void setPaused(bool p);
    double volume() const { return volume_; }
    void setVolume(double v);
    bool muted() const { return muted_; }
    void setMuted(bool m);
    bool buffering() const { return buffering_; }
    bool hasMedia() const { return has_media_; }
    QString upscaler() const { return upscaler_; }
    void setUpscaler(const QString& id);

    Q_INVOKABLE void load(const QString& url, const QVariantMap& headers);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePause() { setPaused(!paused_); }
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void seekRelative(double offset);
    // Returns false if some shader files of the profile were missing
    Q_INVOKABLE bool applyUpscaler();

signals:
    void positionChanged();
    void durationChanged();
    void bufferedChanged();
    void pausedChanged();
    void volumeChanged();
    void mutedChanged();
    void bufferingChanged();
    void hasMediaChanged();
    void upscalerChanged();
    void endReached();
    void loadFailed();

private:
    Q_INVOKABLE void processEvents();
    void handlePropertyChange(const char* name, int format, void* data);
    void updateBuffering();
    void updateInhibit();

    std::shared_ptr<MpvShared> shared_;
    ScreenInhibitor* inhibitor_ = nullptr;

    double position_ = 0;
    double duration_ = 0;
    double buffered_ = 0;
    bool paused_ = false;
    double volume_ = 100;
    bool muted_ = false;
    bool buffering_ = false;
    bool has_media_ = false;
    bool loading_ = false;
    bool cache_wait_ = false;
    bool seeking_ = false;
    bool eof_ = false;
    double seek_target_ = -1;
    QString upscaler_ = QStringLiteral("off");
};

} // namespace anime::app
