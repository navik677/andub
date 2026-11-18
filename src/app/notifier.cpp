#include "notifier.hpp"
#include <QMetaObject>
#include <QQmlEngine>

namespace anime::app {

Notifier* Notifier::instance() {
    static auto* n = new Notifier();
    return n;
}

Notifier* Notifier::create(QQmlEngine*, QJSEngine*) {
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

void Notifier::post(const QString& text, bool error) {
    auto* n = instance();
    QMetaObject::invokeMethod(n, [n, text, error] { emit n->toast(text, error); }, Qt::QueuedConnection);
}

} // namespace anime::app
