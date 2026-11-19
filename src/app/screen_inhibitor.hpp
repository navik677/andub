#pragma once

#include <QObject>

namespace anime::app {

// Keeps the display and the system awake during playback (SetThreadExecutionState).
class ScreenInhibitor : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~ScreenInhibitor() override { set(false); }
    void set(bool on);

private:
    bool active_ = false;
};

} // namespace anime::app
