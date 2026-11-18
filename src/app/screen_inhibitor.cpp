#include "screen_inhibitor.hpp"

#if defined(_WIN32)
#include <windows.h>
#elif defined(ANDUB_HAVE_DBUS)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

namespace anime::app {

void ScreenInhibitor::set(bool on) {
    if (on == active_) return;
    active_ = on;
#if defined(_WIN32)
    SetThreadExecutionState(on ? (ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED) : ES_CONTINUOUS);
#elif defined(ANDUB_HAVE_DBUS)
    QDBusInterface iface(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"),
                         QStringLiteral("org.freedesktop.ScreenSaver"), QDBusConnection::sessionBus());
    if (!iface.isValid()) return;
    if (on) {
        if (pending_) return;
        pending_ = true;
        auto* w = new QDBusPendingCallWatcher(
            iface.asyncCall(QStringLiteral("Inhibit"), QStringLiteral("Andub"), QStringLiteral("Відтворення відео")), this);
        connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* call) {
            QDBusPendingReply<quint32> reply = *call;
            pending_ = false;
            if (!reply.isError()) {
                cookie_ = reply.value();
                if (!active_ && cookie_) { // playback stopped while the call was in flight
                    QDBusInterface i(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"),
                                     QStringLiteral("org.freedesktop.ScreenSaver"), QDBusConnection::sessionBus());
                    i.asyncCall(QStringLiteral("UnInhibit"), cookie_);
                    cookie_ = 0;
                }
            }
            call->deleteLater();
        });
    } else if (cookie_) {
        iface.asyncCall(QStringLiteral("UnInhibit"), cookie_);
        cookie_ = 0;
    }
#endif
}

} // namespace anime::app
