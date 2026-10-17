#include "app/AppController.h"
#include "platform/DesktopIntegration.h"
#include "platform/gnome/GnomeBackend.h"
#include "platform/gnome/PickerService.h"
#include "storage/HistoryStore.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

class DesktopIntegrationTest final : public QObject
{
    Q_OBJECT

private slots:
    void gnomeShortcutAndManualCancellation();
    void copyOnlyFallback();
};

void DesktopIntegrationTest::gnomeShortcutAndManualCancellation()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    AppController controller(&store, QStringLiteral("emoji"));
    QQuickWindow window;
    window.setProperty("currentView", QStringLiteral("emoji"));
    GnomeBackend backend(&window, &controller);
    DesktopIntegration *integration = &backend;
    PickerService service(&window, &controller, integration);
    auto bus = QDBusConnection::sessionBus();
    QVERIFY(bus.registerObject(QStringLiteral("/org/emojikeyboard/App"), &service,
                               QDBusConnection::ExportScriptableSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.emojikeyboard.App")));
    QVERIFY(!integration->isAvailable());
    QVERIFY(integration->errorString().contains(QStringLiteral("GNOME Shell extension")));

    QCOMPARE(service.ShowFromShortcut(QStringLiteral("invalid")), 0u);
    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard")),
             uint(QCoreApplication::applicationPid()));
    QCOMPARE(service.CurrentView(), QStringLiteral("clipboard"));
    QVERIFY(service.IsVisible());

    service.ShowEmoji();
    QCOMPARE(service.CurrentView(), QStringLiteral("emoji"));
    QGuiApplication::clipboard()->setText(QStringLiteral("previous text"));
    controller.copyEmoji(QStringLiteral("😀"));
    QCOMPARE(controller.status(), QStringLiteral("Emoji copied. Paste it in your app."));
    QVERIFY(!controller.hasEmojiClipboardSnapshot());

    QDBusInterface remote(QStringLiteral("org.emojikeyboard.App"),
                          QStringLiteral("/org/emojikeyboard/App"),
                          QStringLiteral("org.emojikeyboard.App"), bus);
    QVERIFY(remote.isValid());
    QDBusPendingCallWatcher call(remote.asyncCall(QStringLiteral("ShowFromShortcut"),
                                                   QStringLiteral("emoji")));
    QTRY_VERIFY_WITH_TIMEOUT(call.isFinished(), 3000);
    const QDBusPendingReply<uint> reply = call;
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    QCOMPARE(reply.value(), uint(QCoreApplication::applicationPid()));
    controller.copyEmoji(QStringLiteral("🧪"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.status().contains(
        QStringLiteral("Automatic paste unavailable")), 3000);
    QVERIFY(!controller.hasEmojiClipboardSnapshot());
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("🧪"));
    bus.unregisterObject(QStringLiteral("/org/emojikeyboard/App"));
    bus.unregisterService(QStringLiteral("org.emojikeyboard.App"));
}

void DesktopIntegrationTest::copyOnlyFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    AppController controller(&store, QStringLiteral("emoji"));
    QQuickWindow window;
    window.setProperty("currentView", QStringLiteral("emoji"));
    CopyOnlyBackend backend(&window, &controller, QStringLiteral("Native backend unavailable"));
    DesktopIntegration *integration = &backend;
    PickerService service(&window, &controller, integration);

    QVERIFY(!integration->isAvailable());
    QCOMPARE(integration->errorString(), QStringLiteral("Native backend unavailable"));
    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard")), 0u);
    QCOMPARE(service.CurrentView(), QStringLiteral("clipboard"));
    controller.copyEmoji(QStringLiteral("🎉"));
    QVERIFY(!controller.hasEmojiClipboardSnapshot());
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("🎉"));
    service.Hide();
    QVERIFY(!service.IsVisible());
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    DesktopIntegrationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_desktop_integration.moc"
