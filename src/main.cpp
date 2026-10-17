#include "app/AppController.h"
#include "app/ClipboardImageProvider.h"
#include "storage/HistoryStore.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QTimer>

#include <sys/stat.h>

int main(int argc, char *argv[])
{
    umask(0077);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("EmojiKeyboard"));
    QCoreApplication::setOrganizationName(QStringLiteral("EmojiKeyboard"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Emoji Keyboard"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Emoji picker and local clipboard history"));
    parser.addHelpOption();
    const QCommandLineOption clipboardOption(QStringLiteral("clipboard"),
                                            QStringLiteral("Open the clipboard view"));
    const QCommandLineOption smokeOption(QStringLiteral("smoke-test"),
                                        QStringLiteral("Load the interface, then exit"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                             QStringLiteral("Save the window as a PNG and exit"),
                                             QStringLiteral("path"));
    parser.addOption(clipboardOption);
    parser.addOption(smokeOption);
    parser.addOption(screenshotOption);
    parser.process(app);

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

    if (parser.isSet(screenshotOption)) {
        QTimer::singleShot(700, &app, [&app, &engine, &parser, &screenshotOption] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            const bool saved = window && window->grabWindow().save(parser.value(screenshotOption));
            app.exit(saved ? 0 : 1);
        });
    } else if (parser.isSet(smokeOption)) {
        QTimer::singleShot(500, &app, &QGuiApplication::quit);
    }

    return app.exec();
}
