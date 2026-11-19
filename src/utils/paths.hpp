#pragma once

#include <string>
#include <filesystem>
#include <QStandardPaths>
#include <QString>

namespace anime::paths {

// Per-user directories shared by all services: settings, favorites and history
// in %LOCALAPPDATA%\anime-gui, poster cache in %LOCALAPPDATA%\cache\anime-gui.
namespace detail {
inline std::string ensure(const QString& base, const char* sub) {
    std::filesystem::path dir = std::filesystem::path(base.toStdString()) / sub;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        dir = std::filesystem::temp_directory_path(ec) / sub;
        std::filesystem::create_directories(dir, ec);
    }
    return dir.string();
}
} // namespace detail

inline std::string config_dir() {
    static const std::string dir = detail::ensure(
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation), "anime-gui");
    return dir;
}

inline std::string cache_dir() {
    static const std::string dir = detail::ensure(
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation), "anime-gui");
    return dir;
}

inline std::string config_file(const char* name) {
    return (std::filesystem::path(config_dir()) / name).string();
}

} // namespace anime::paths
