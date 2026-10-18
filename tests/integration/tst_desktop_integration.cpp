#include "app/AppController.h"
#include "platform/DesktopIntegration.h"
#include "platform/gnome/GnomeBackend.h"
#include "platform/gnome/PickerService.h"
#include "storage/HistoryStore.h"

#include <QClipboard>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class RecordingBackend final : public DesktopIntegration
{
public:
    RecordingBackend(QQuickWindow *window, AppController *controller)
        : DesktopIntegration(window, controller) {}

    bool isAvailable() const override { return true; }
    QString errorString() const override { return {}; }
    void captureClipboard(const QString &) override {}
    void openView(const QString &view) override { showWindow(view); }
    bool rememberTarget() override { ++rememberCount; targetArmed = true; return true; }
    void pasteSelection(bool, quint64) override {}
    void cancel() override { ++cancelCount; targetArmed = false; }

    int rememberCount = 0;
    int cancelCount = 0;
    bool targetArmed = false;
};

class ProbeStub final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.emojikeyboard.ClipboardProbe")

public:
    QString lastMode;
    int pasteCount = 0;
    int cancelCount = 0;

public slots:
    Q_SCRIPTABLE QString PasteSelection(const QString &mode, QString &shortcut,
                                        uint &clipboardRevision)
    {
        lastMode = mode;
        ++pasteCount;
        shortcut = QStringLiteral("Ctrl+V");
        clipboardRevision = 0;
        return QStringLiteral("sent");
    }

    Q_SCRIPTABLE void CancelPaste() { ++cancelCount; }
};

class DesktopIntegrationTest final : public QObject
{
    Q_OBJECT

private slots:
    void gnomeUnavailableCopiesOnly();
    void gnomeAvailabilityChanges();
    void copyOnlyFallback();
    void shortcutFailurePersistsAfterCopy();
    void repeatedActivationTogglesAndSwitches();
    void gnomePasteModeAndCancellation();
};

void DesktopIntegrationTest::gnomeUnavailableCopiesOnly()
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
    controller.setIntegrationAvailability(integration->isAvailable(), integration->errorString());
    PickerService service(&window, &controller, integration);
    auto bus = QDBusConnection::sessionBus();
    QVERIFY(bus.registerObject(QStringLiteral("/org/emojikeyboard/App"), &service,
                               QDBusConnection::ExportScriptableSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.emojikeyboard.App")));
    QVERIFY(!integration->isAvailable());
    QVERIFY(integration->errorString().contains(QStringLiteral("GNOME Shell extension")));
    QVERIFY(controller.copyOnly());
    QVERIFY(controller.integrationWarning().contains(QStringLiteral("GNOME Shell extension")));

    QCOMPARE(service.ShowFromShortcut(QStringLiteral("invalid"), false), 0u);
    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard"), false), 0u);
    QCOMPARE(service.CurrentView(), QStringLiteral("clipboard"));
    QVERIFY(service.IsVisible());

    service.ShowEmoji();
    QCOMPARE(service.CurrentView(), QStringLiteral("emoji"));
    QGuiApplication::clipboard()->setText(QStringLiteral("previous text"));
    controller.copyEmoji(QStringLiteral("😀"));
    QCOMPARE(controller.status(), QStringLiteral("Emoji copied. Paste it in your app."));
    QVERIFY(!controller.hasEmojiClipboardSnapshot());
    QVERIFY(!controller.integrationWarning().isEmpty());

    QDBusInterface remote(QStringLiteral("org.emojikeyboard.App"),
                          QStringLiteral("/org/emojikeyboard/App"),
                          QStringLiteral("org.emojikeyboard.App"), bus);
    QVERIFY(remote.isValid());
    QDBusPendingCallWatcher call(remote.asyncCall(QStringLiteral("ShowFromShortcut"),
                                                   QStringLiteral("emoji"), false));
    QTRY_VERIFY_WITH_TIMEOUT(call.isFinished(), 3000);
    const QDBusPendingReply<uint> reply = call;
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    QCOMPARE(reply.value(), 0u);
    controller.copyEmoji(QStringLiteral("🧪"));
    QCOMPARE(controller.status(), QStringLiteral("Emoji copied. Paste it in your app."));
    QVERIFY(!controller.hasEmojiClipboardSnapshot());
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("🧪"));
    bus.unregisterObject(QStringLiteral("/org/emojikeyboard/App"));
    bus.unregisterService(QStringLiteral("org.emojikeyboard.App"));
}

void DesktopIntegrationTest::gnomeAvailabilityChanges()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    AppController controller(&store, QStringLiteral("emoji"));
    QQuickWindow window;
    GnomeBackend backend(&window, &controller);
    connect(&backend, &DesktopIntegration::availabilityChanged, &controller, [&] {
        controller.setIntegrationAvailability(backend.isAvailable(), backend.errorString());
    });
    controller.setIntegrationAvailability(backend.isAvailable(), backend.errorString());
    QVERIFY(controller.copyOnly());

    auto bus = QDBusConnection::sessionBus();
    QVERIFY(bus.registerService(QStringLiteral("org.emojikeyboard.ClipboardProbe")));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.copyOnly(), 3000);
    QVERIFY(controller.integrationWarning().isEmpty());

    QVERIFY(bus.unregisterService(QStringLiteral("org.emojikeyboard.ClipboardProbe")));
    QTRY_VERIFY_WITH_TIMEOUT(controller.copyOnly(), 3000);
    QVERIFY(controller.integrationWarning().contains(QStringLiteral("GNOME Shell extension")));
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
    controller.setIntegrationAvailability(integration->isAvailable(), integration->errorString());
    PickerService service(&window, &controller, integration);

    QVERIFY(!integration->isAvailable());
    QCOMPARE(integration->errorString(), QStringLiteral("Native backend unavailable"));
    QVERIFY(controller.copyOnly());
    QCOMPARE(controller.integrationWarning(), QStringLiteral("Native backend unavailable"));
    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard"), false), 0u);
    QCOMPARE(service.CurrentView(), QStringLiteral("clipboard"));
    controller.copyEmoji(QStringLiteral("🎉"));
    QVERIFY(!controller.hasEmojiClipboardSnapshot());
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("🎉"));
    QVERIFY(store.addText(QStringLiteral("Saved history item")));
    controller.copyEntry(store.entries().first().id);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("Saved history item"));
    QCOMPARE(controller.integrationWarning(), QStringLiteral("Native backend unavailable"));
    service.Hide();
    QVERIFY(!service.IsVisible());
}

void DesktopIntegrationTest::shortcutFailurePersistsAfterCopy()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    AppController controller(&store, QStringLiteral("emoji"));
    controller.setIntegrationAvailability(true, QString());
    controller.reportShortcutFailure(QStringLiteral("Super+."));
    controller.reportShortcutFailure(QStringLiteral("Super+V"));
    controller.copyEmoji(QStringLiteral("✅"));
    QVERIFY(!controller.copyOnly());
    QVERIFY(controller.integrationWarning().contains(QStringLiteral("Super+.")));
    QVERIFY(controller.integrationWarning().contains(QStringLiteral("Super+V")));
    QCOMPARE(controller.status(), QStringLiteral("Emoji copied. Paste it in your app."));
}

void DesktopIntegrationTest::repeatedActivationTogglesAndSwitches()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    AppController controller(&store, QStringLiteral("emoji"));
    QQuickWindow window;
    window.setProperty("currentView", QStringLiteral("emoji"));
    RecordingBackend backend(&window, &controller);
    PickerService service(&window, &controller, &backend);
    const uint pid = uint(QCoreApplication::applicationPid());

    QCOMPARE(service.ShowFromShortcut(QStringLiteral("emoji"), false), pid);
    QVERIFY(service.IsVisible());
    QCOMPARE(backend.rememberCount, 1);
    QVERIFY(backend.targetArmed);

    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard"), true), pid);
    QVERIFY(service.IsVisible());
    QCOMPARE(service.CurrentView(), QStringLiteral("clipboard"));
    QCOMPARE(backend.rememberCount, 1);
    QVERIFY(backend.targetArmed);

    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard"), false), pid);
    QVERIFY(service.IsVisible());
    QCOMPARE(backend.rememberCount, 2);
    QVERIFY(backend.targetArmed);

    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard"), true), 0u);
    QVERIFY(!service.IsVisible());
    QCOMPARE(backend.cancelCount, 1);
    QVERIFY(!backend.targetArmed);

    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard"), false), pid);
    QVERIFY(service.IsVisible());
    QCOMPARE(backend.rememberCount, 3);
    service.ShowEmoji();
    QCOMPARE(service.CurrentView(), QStringLiteral("emoji"));
    QCOMPARE(backend.cancelCount, 2);
    service.ShowEmoji();
    QVERIFY(!service.IsVisible());
}

void DesktopIntegrationTest::gnomePasteModeAndCancellation()
{
    auto bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    ProbeStub probe;
    QVERIFY(bus.registerObject(QStringLiteral("/org/emojikeyboard/ClipboardProbe"), &probe,
                               QDBusConnection::ExportScriptableSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.emojikeyboard.ClipboardProbe")));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    QVERIFY(store.addText(QStringLiteral("Saved item")));
    AppController controller(&store, QStringLiteral("clipboard"));
    QQuickWindow window;
    window.setProperty("currentView", QStringLiteral("clipboard"));
    GnomeBackend backend(&window, &controller);
    PickerService service(&window, &controller, &backend);
    QVERIFY(backend.isAvailable());

    controller.setPasteShortcutMode(QStringLiteral("ctrl-shift-v"));
    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard"), false),
             uint(QCoreApplication::applicationPid()));
    controller.copyEntry(store.entries().first().id);
    QTRY_COMPARE_WITH_TIMEOUT(probe.pasteCount, 1, 3000);
    QCOMPARE(probe.lastMode, QStringLiteral("ctrl-shift-v"));
    QVERIFY(window.isVisible());

    controller.copyEntry(store.entries().first().id);
    QTRY_COMPARE_WITH_TIMEOUT(probe.pasteCount, 2, 3000);
    QVERIFY(window.isVisible());

    window.hide();
    QTRY_VERIFY_WITH_TIMEOUT(probe.cancelCount > 0, 3000);
    controller.copyEntry(store.entries().first().id);
    QTest::qWait(100);
    QCOMPARE(probe.pasteCount, 2);
    const int cancelsAfterHide = probe.cancelCount;

    QCOMPARE(service.ShowFromShortcut(QStringLiteral("clipboard"), false),
             uint(QCoreApplication::applicationPid()));
    controller.copyEntry(store.entries().first().id);
    backend.cancel();
    QTest::qWait(100);
    QCOMPARE(probe.pasteCount, 2);
    QTRY_VERIFY_WITH_TIMEOUT(probe.cancelCount > cancelsAfterHide, 3000);

    bus.unregisterService(QStringLiteral("org.emojikeyboard.ClipboardProbe"));
    bus.unregisterObject(QStringLiteral("/org/emojikeyboard/ClipboardProbe"));
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid())
        return 1;
    QCoreApplication::setOrganizationName(QStringLiteral("EmojiKeyboardTests"));
    QCoreApplication::setApplicationName(QStringLiteral("DesktopIntegration"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    DesktopIntegrationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_desktop_integration.moc"
