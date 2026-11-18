#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace anime::app {

// App-wide transient messages ("toasts"). post() is safe from any thread.
class Notifier : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static Notifier* instance();
    static Notifier* create(QQmlEngine*, QJSEngine*);
    static void post(const QString& text, bool error = false);

signals:
    void toast(const QString& text, bool error);

private:
    using QObject::QObject;
};

} // namespace anime::app
