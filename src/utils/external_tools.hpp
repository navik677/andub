#pragma once

// mpv.exe and yt-dlp.exe ship next to andub.exe; fall back to PATH so a
// user-installed (newer) copy still works with a development build.
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>

#include <windows.h>

namespace anime::tools {

inline QString find(const QString& name) {
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).filePath(name + QStringLiteral(".exe"));
    if (QFileInfo::exists(bundled)) return bundled;
    const QString in_path = QStandardPaths::findExecutable(name);
    return in_path.isEmpty() ? name : in_path;
}

// Console programs (yt-dlp) started from a GUI process would otherwise open
// their own console window.
inline void hide_console(QProcess& proc) {
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a) {
        a->flags |= CREATE_NO_WINDOW;
    });
}

} // namespace anime::tools
