#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace anime::app {

// Persistent user preferences, stored in settings.json next to favorites/history.
class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged)
    Q_PROPERTY(bool externalPlayer READ externalPlayer WRITE setExternalPlayer NOTIFY externalPlayerChanged)
    Q_PROPERTY(QString upscaler READ upscaler WRITE setUpscaler NOTIFY upscalerChanged)
    Q_PROPERTY(QVariantList upscalerProfiles READ upscalerProfiles CONSTANT)
    Q_PROPERTY(bool shadersAvailable READ shadersAvailable CONSTANT)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString downloadsPath READ downloadsPath CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    static Settings* instance();
    static Settings* create(QQmlEngine*, QJSEngine*);

    QString theme() const { return theme_; }
    void setTheme(const QString& v);
    QString accent() const { return accent_; }
    void setAccent(const QString& v);
    bool externalPlayer() const { return external_; }
    void setExternalPlayer(bool v);
    QString upscaler() const { return upscaler_; }
    void setUpscaler(const QString& v);
    QVariantList upscalerProfiles() const;
    bool shadersAvailable() const;
    double volume() const { return volume_; }
    void setVolume(double v);
    QString downloadsPath() const;
    QString version() const;

    QString provider() const;
    void setProvider(const QString& name);

    Q_INVOKABLE QString upscalerName(const QString& id) const;
    Q_INVOKABLE void openFolder(const QString& path) const;

signals:
    void themeChanged();
    void accentChanged();
    void externalPlayerChanged();
    void upscalerChanged();
    void volumeChanged();

private:
    Settings();
    QString theme_;
    QString accent_;
    bool external_ = false;
    QString upscaler_;
    double volume_ = 100.0;
};

} // namespace anime::app
