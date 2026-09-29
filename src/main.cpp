#include "Backend.hpp"
#include "MacroBackend.hpp"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

int main(int argc, char* argv[]) {
    QQuickWindow::setDefaultAlphaBuffer(true);
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FlintAutoClicker"));
    app.setOrganizationName(QStringLiteral("FlintAutoClicker"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));

    Backend backend;
    MacroBackend macro(&backend);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.rootContext()->setContextProperty(QStringLiteral("macro"), &macro);
    engine.loadFromModule("Flint", "Main");
    if (engine.rootObjects().isEmpty()) return 1;

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (!window) return 1;
    backend.attachWindow(window);
    macro.attachWindow(window);
    window->show();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &backend, [&backend, &macro] {
        macro.stop();
        backend.stop();
        backend.save();
    });
    return app.exec();
}
