#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

#include "Fonts.h"

Q_IMPORT_QML_PLUGIN(Reckon_CorePlugin)
Q_IMPORT_QML_PLUGIN(ReckonPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("reckon"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Reckon"));
    QGuiApplication::setOrganizationName(QStringLiteral("reckon"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("reckon.local"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Reckon/logo/reckon.svg")));
    // Wayland shows the icon of the desktop file this names.
    QGuiApplication::setDesktopFileName(QStringLiteral("reckon"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    Fonts::registerBundled();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A calculator with a history, graphs and scripts"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("session"), QStringLiteral("Session file to open"),
                                 QStringLiteral("[session]"));
    parser.process(app);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({
        {QStringLiteral("initialSession"), parser.positionalArguments().value(0)},
    });
    engine.loadFromModule("Reckon", "Main");
    return app.exec();
}
