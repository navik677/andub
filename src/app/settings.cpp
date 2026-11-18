#include "settings.hpp"
#include "../services/player_service.hpp"
#include "../services/upscaler.hpp"
#include "../services/download_service.hpp"
#include "../utils/json.hpp"
#include "../utils/paths.hpp"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantMap>
#include <filesystem>
#include <fstream>

namespace anime::app {

static QString get(const char* key, const char* fallback) {
    return QString::fromStdString(PlayerService::get_setting(key, fallback));
}

static void put(const char* key, const QString& value) {
    PlayerService::set_setting(key, value.toStdString());
}

Settings* Settings::instance() {
    static auto* s = new Settings();
    return s;
}

Settings* Settings::create(QQmlEngine*, QJSEngine*) {
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Settings::Settings() {
    theme_ = get("ui_theme", "dark");
    accent_ = get("ui_accent", "#ff6a4d");
    external_ = PlayerService::get_player_mode() == PlayerMode::ExternalMpv;
    upscaler_ = get("upscaler", "off");
    volume_ = get("volume", "100").toDouble();
    if (volume_ < 0 || volume_ > 100) volume_ = 100;
}

void Settings::setTheme(const QString& v) {
    if (v == theme_) return;
    theme_ = v;
    put("ui_theme", v);
    emit themeChanged();
}

void Settings::setAccent(const QString& v) {
    if (v == accent_) return;
    accent_ = v;
    put("ui_accent", v);
    emit accentChanged();
}

void Settings::setExternalPlayer(bool v) {
    if (v == external_) return;
    external_ = v;
    PlayerService::set_player_mode(v ? PlayerMode::ExternalMpv : PlayerMode::Embedded);
    emit externalPlayerChanged();
}

void Settings::setUpscaler(const QString& v) {
    if (v == upscaler_) return;
    upscaler_ = v;
    Upscaler::save_profile(v.toStdString());
    emit upscalerChanged();
}

void Settings::setVolume(double v) {
    v = std::clamp(v, 0.0, 100.0);
    if (qFuzzyCompare(v + 1.0, volume_ + 1.0)) return;
    volume_ = v;
    put("volume", QString::number(static_cast<int>(v)));
    emit volumeChanged();
}

QVariantList Settings::upscalerProfiles() const {
    QVariantList list;
    for (const auto& p : Upscaler::profiles()) {
        list.append(QVariantMap{{"id", QString::fromStdString(p.id)}, {"name", QString::fromStdString(p.name)}});
    }
    return list;
}

QString Settings::upscalerName(const QString& id) const {
    const auto& list = Upscaler::profiles();
    return QString::fromStdString(list[Upscaler::index_of(id.toStdString())].name);
}

bool Settings::shadersAvailable() const {
    return !Upscaler::shader_dir().empty();
}

QString Settings::downloadsPath() const {
    return QString::fromStdString(DownloadService::download_root());
}

QString Settings::version() const {
    return QCoreApplication::applicationVersion();
}

void Settings::openFolder(const QString& path) const {
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString Settings::provider() const {
    QString p = get("provider", "");
    if (!p.isEmpty()) return p;
    // Older builds kept the provider in config.json
    try {
        std::ifstream f(paths::config_file("config.json"));
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (!content.empty()) return QString::fromStdString(json::Value::parse(content)["provider"].get_str());
    } catch (...) {}
    return {};
}

void Settings::setProvider(const QString& name) {
    put("provider", name);
}

} // namespace anime::app
