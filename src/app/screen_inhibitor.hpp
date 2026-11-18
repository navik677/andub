#pragma once

#include <QObject>

namespace anime::app {

// Keeps the display awake during playback (freedesktop ScreenSaver on Linux,
// SetThreadExecutionState on Windows).
class ScreenInhibitor : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~ScreenInhibitor() override { set(false); }
    void set(bool on);

private:
    bool active_ = false;
    quint32 cookie_ = 0;
    bool pending_ = false;
};

} // namespace anime::app
