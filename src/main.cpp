#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <clocale>

#include "app/poster_provider.hpp"

int main(int argc, char* argv[]) {
    // The embedded player renders through mpv's OpenGL API, so the scene graph
    // has to use OpenGL (Qt would pick Direct3D 11 on Windows otherwise).
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Andub"));
    QGuiApplication::setApplicationVersion(QStringLiteral(ANDUB_VERSION));
    QGuiApplication::setOrganizationName(QStringLiteral("Andub"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Andub/resources/andub.png")));
    std::setlocale(LC_NUMERIC, "C");

    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Andub/resources/fonts/Manrope.ttf"));
    QFont font(QStringLiteral("Manrope"));
    font.setPixelSize(14);
    font.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(font);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("poster"), new anime::app::PosterProvider());
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    // Development aid for screenshots: ANDUB_DEV_ACTION=details:<row>|play:<row>|settings|downloads|favorites
    engine.rootContext()->setContextProperty(QStringLiteral("devAction"), qEnvironmentVariable("ANDUB_DEV_ACTION"));
    engine.loadFromModule("Andub", "Main");

    // Development aid: ANDUB_SNAPSHOT=out.png [ANDUB_SNAPSHOT_DELAY=ms] renders
    // the window to a file and exits (works with QT_QPA_PLATFORM=offscreen).
    if (const QString out = qEnvironmentVariable("ANDUB_SNAPSHOT"); !out.isEmpty()) {
        const int delay = qEnvironmentVariableIntValue("ANDUB_SNAPSHOT_DELAY");
        QTimer::singleShot(delay > 0 ? delay : 4000, &app, [&engine, out] {
            if (auto* w = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                w->grabWindow().save(out);
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
