#include "app/AppController.h"
#include "app/ClipboardImageProvider.h"
#include "platform/DesktopIntegration.h"
#include "platform/gnome/GnomeBackend.h"
#include "platform/gnome/PickerService.h"
#include "storage/HistoryStore.h"
#ifdef EMOJI_HAS_X11
#include "platform/x11/X11Backend.h"
#endif

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>

#include <memory>

#include <sys/stat.h>

namespace {
constexpr auto serviceName = "org.emojikeyboard.App";
constexpr auto objectPath = "/org/emojikeyboard/App";

int callRunningPicker(const QDBusConnection &bus, const QString &method)
{
    QDBusInterface picker(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                          QString::fromLatin1(serviceName), bus);
    const QDBusReply<void> reply = picker.call(method);
    if (reply.isValid())
        return 0;
    qWarning() << "Could not contact the running picker:" << reply.error().message();
    return 1;
}
}

int main(int argc, char *argv[])
{
    umask(0077);
#if defined(Q_OS_LINUX)
    // The custom QML UI does not need GTK's platform theme. Inherited GTK/GSettings
    // paths from Snap-hosted IDEs can otherwise abort startup before the UI loads.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "generic");
#endif
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("EmojiKeyboard"));
    QCoreApplication::setOrganizationName(QStringLiteral("EmojiKeyboard"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Emoji Keyboard"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.emojikeyboard.App"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/org.emojikeyboard.App.svg")));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Emoji picker and local clipboard history"));
    parser.addHelpOption();
    const QCommandLineOption clipboardOption(QStringLiteral("clipboard"),
                                            QStringLiteral("Open the clipboard view"));
    const QCommandLineOption backgroundOption(QStringLiteral("background"),
                                             QStringLiteral("Run without showing the picker"));
    const QCommandLineOption quitOption(QStringLiteral("quit"),
                                       QStringLiteral("Stop the running picker"));
    const QCommandLineOption smokeOption(QStringLiteral("smoke-test"),
                                        QStringLiteral("Load the interface, then exit"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                             QStringLiteral("Save the window as a PNG and exit"),
                                             QStringLiteral("path"));
    const QCommandLineOption dataPathsOption(QStringLiteral("print-data-paths"),
                                             QStringLiteral("Print local history and settings paths as JSON"));
    parser.addOption(clipboardOption);
    parser.addOption(backgroundOption);
    parser.addOption(quitOption);
    parser.addOption(smokeOption);
    parser.addOption(screenshotOption);
    parser.addOption(dataPathsOption);
    parser.process(app);

    if (parser.isSet(dataPathsOption)) {
        const QJsonObject paths {
            { QStringLiteral("historyDirectory"),
              QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) },
            { QStringLiteral("settingsFile"), QSettings().fileName() }
        };
        const QByteArray output = QJsonDocument(paths).toJson(QJsonDocument::Compact);
        std::fwrite(output.constData(), 1, size_t(output.size()), stdout);
        std::fputc('\n', stdout);
        return 0;
    }

    auto bus = QDBusConnection::sessionBus();
    const bool busAvailable = bus.isConnected() && bus.interface();
    const bool diagnostic = parser.isSet(smokeOption) || parser.isSet(screenshotOption);
    if (parser.isSet(quitOption)) {
        if (!busAvailable)
            return 1;
        const QDBusReply<bool> running = bus.interface()->isServiceRegistered(
            QString::fromLatin1(serviceName));
        return !running.isValid() ? 1
               : running.value() ? callRunningPicker(bus, QStringLiteral("Quit")) : 0;
    }
    if (parser.isSet(backgroundOption) && !busAvailable && !diagnostic) {
        qWarning() << "A session D-Bus is required for background mode";
        return 1;
    }
    if (busAvailable && !diagnostic) {
        const QDBusReply<bool> running = bus.interface()->isServiceRegistered(
            QString::fromLatin1(serviceName));
        if (running.isValid() && running.value()) {
            if (parser.isSet(backgroundOption))
                return 0;
            return callRunningPicker(bus, parser.isSet(clipboardOption)
                                              ? QStringLiteral("ShowClipboard")
                                              : QStringLiteral("ShowEmoji"));
        }
    }

    HistoryStore store;
    store.open();
    AppController controller(&store, parser.isSet(clipboardOption)
                                ? QStringLiteral("clipboard") : QStringLiteral("emoji"));

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("clipboard"), new ClipboardImageProvider(&store));
    engine.setInitialProperties({ { QStringLiteral("controller"), QVariant::fromValue(&controller) } });
    engine.loadFromModule("EmojiKeyboard", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    std::unique_ptr<DesktopIntegration> desktop;
#ifdef EMOJI_HAS_X11
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        auto x11 = std::make_unique<X11Backend>(window, &controller);
        if (!x11->isAvailable()) {
            qWarning() << "X11 integration unavailable:" << x11->errorString();
            desktop = std::make_unique<CopyOnlyBackend>(window, &controller,
                                                        x11->errorString());
        } else {
            desktop = std::move(x11);
        }
    }
#endif
    if (!desktop && QGuiApplication::platformName() == QStringLiteral("xcb"))
        desktop = std::make_unique<CopyOnlyBackend>(
            window, &controller, QStringLiteral("X11 integration was not built"));
    if (!desktop)
        desktop = std::make_unique<GnomeBackend>(window, &controller);
    const auto updateIntegrationAvailability = [&controller, &desktop] {
        controller.setIntegrationAvailability(desktop->isAvailable(), desktop->errorString());
    };
    QObject::connect(desktop.get(), &DesktopIntegration::availabilityChanged,
                     &controller, updateIntegrationAvailability);
    updateIntegrationAvailability();
    PickerService picker(window, &controller, desktop.get());

    if (busAvailable && !diagnostic) {
        if (!bus.registerObject(QString::fromLatin1(objectPath), &picker,
                                QDBusConnection::ExportScriptableSlots)) {
            qWarning() << "Could not export the picker D-Bus object:" << bus.lastError().message();
            if (parser.isSet(backgroundOption))
                return 1;
        } else if (!bus.registerService(QString::fromLatin1(serviceName))) {
            bus.unregisterObject(QString::fromLatin1(objectPath));
            const QDBusReply<bool> running = bus.interface()->isServiceRegistered(
                QString::fromLatin1(serviceName));
            if (running.isValid() && running.value()) {
                if (parser.isSet(backgroundOption))
                    return 0;
                return callRunningPicker(bus, parser.isSet(clipboardOption)
                                                  ? QStringLiteral("ShowClipboard")
                                                  : QStringLiteral("ShowEmoji"));
            }
            qWarning() << "Could not register the picker D-Bus name:" << bus.lastError().message();
            if (parser.isSet(backgroundOption))
                return 1;
        }
    }

    if (!parser.isSet(backgroundOption) || diagnostic) {
        if (parser.isSet(clipboardOption))
            picker.ShowClipboard();
        else
            picker.ShowEmoji();
    }

    if (parser.isSet(screenshotOption)) {
        QTimer::singleShot(700, &app, [&app, window, &parser, &screenshotOption] {
            const bool saved = window->grabWindow().save(parser.value(screenshotOption));
            app.exit(saved ? 0 : 1);
        });
    } else if (parser.isSet(smokeOption)) {
        QTimer::singleShot(500, &app, [&app] { app.exit(0); });
    }

    return app.exec();
}
