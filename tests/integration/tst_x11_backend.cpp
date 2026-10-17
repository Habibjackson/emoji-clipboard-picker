#include "app/AppController.h"
#include "platform/x11/X11Backend.h"
#include "storage/HistoryStore.h"

#include <QFile>
#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

class X11BackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void clipboardShortcutsAndPaste();
    void restoresEmojiClipboardAndCancelsSafely();
};

namespace {
void sendShortcut(Display *display, KeySym symbol)
{
    const KeyCode super = XKeysymToKeycode(display, XK_Super_L);
    const KeyCode key = XKeysymToKeycode(display, symbol);
    QVERIFY(super != 0);
    QVERIFY(key != 0);
    QVERIFY(XTestFakeKeyEvent(display, super, True, CurrentTime));
    QVERIFY(XTestFakeKeyEvent(display, key, True, CurrentTime));
    QVERIFY(XTestFakeKeyEvent(display, key, False, CurrentTime));
    QVERIFY(XTestFakeKeyEvent(display, super, False, CurrentTime));
    XFlush(display);
}

QString pastedText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

bool hasKind(const HistoryStore &store, const QString &kind)
{
    for (const auto &entry : store.entries()) {
        if (entry.kind == kind)
            return true;
    }
    return false;
}
}

void X11BackendTest::clipboardShortcutsAndPaste()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY2(store.open(), qPrintable(store.errorString()));
    AppController controller(&store, QStringLiteral("emoji"));
    QQuickWindow picker;
    picker.setProperty("currentView", QStringLiteral("emoji"));
    picker.resize(400, 300);
    X11Backend backend(&picker, &controller);
    QVERIFY2(backend.isAvailable(), qPrintable(backend.errorString()));
    {
        X11Backend conflictingBackend(&picker, &controller);
        QVERIFY(conflictingBackend.isAvailable());
        QVERIFY(controller.status().contains(QStringLiteral("Super+V")));
        QVERIFY(controller.status().contains(QStringLiteral("unavailable")));
    }
    QSignalSpy ownerSpy(&backend, &X11Backend::clipboardOwnerChanged);
    QSignalSpy shortcutSpy(&backend, &X11Backend::shortcutTriggered);
    QSignalSpy pasteSpy(&backend, &X11Backend::pasteSent);
    QVERIFY(ownerSpy.isValid());
    QVERIFY(shortcutSpy.isValid());
    QVERIFY(pasteSpy.isValid());

    Display *display = XOpenDisplay(nullptr);
    QVERIFY(display);
    const QString resultPath = directory.filePath(QStringLiteral("paste-result"));
    QProcess target;
    target.start(QStringLiteral(X11_TEST_TARGET), { resultPath });
    QVERIFY(target.waitForStarted());
    QVERIFY(target.waitForReadyRead(3000));
    const QByteArray ready = target.readLine();
    QVERIFY2(ready.startsWith("READY "), ready.constData());
    const Window targetWindow = ready.mid(6).trimmed().toULong();
    QVERIFY(targetWindow);
    Window focused = None;
    int revert = 0;
    QTRY_VERIFY_WITH_TIMEOUT((XGetInputFocus(display, &focused, &revert), focused == targetWindow), 3000);

    QTRY_VERIFY_WITH_TIMEOUT(ownerSpy.size() >= 2, 4000);
    QTRY_VERIFY_WITH_TIMEOUT(hasKind(store, QStringLiteral("text")), 4000);
    QTRY_VERIFY_WITH_TIMEOUT(hasKind(store, QStringLiteral("image")), 4000);

    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 1, 3000);
    QCOMPARE(shortcutSpy.at(0).at(0).toString(), QStringLiteral("emoji"));
    QCOMPARE(picker.property("currentView").toString(), QStringLiteral("emoji"));
    QVERIFY(picker.isVisible());

    QTRY_VERIFY_WITH_TIMEOUT((XGetInputFocus(display, &focused, &revert),
                              focused == Window(picker.winId())), 3000);
    sendShortcut(display, XK_v);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 2, 3000);
    QCOMPARE(picker.property("currentView").toString(), QStringLiteral("clipboard"));
    QVERIFY(picker.isVisible());
    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 3, 3000);
    QCOMPARE(picker.property("currentView").toString(), QStringLiteral("emoji"));
    QVERIFY(picker.isVisible());

    controller.copyEmoji(QStringLiteral("😀"));
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(pastedText(resultPath), QStringLiteral("😀"), 3000);

    sendShortcut(display, XK_v);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 4, 3000);
    QCOMPARE(shortcutSpy.at(3).at(0).toString(), QStringLiteral("clipboard"));
    QCOMPARE(picker.property("currentView").toString(), QStringLiteral("clipboard"));
    qint64 textId = 0;
    for (const auto &entry : store.entries()) {
        if (entry.kind == QStringLiteral("text") && entry.text == QStringLiteral("X11 source text")) {
            textId = entry.id;
            break;
        }
    }
    QVERIFY(textId);
    QVERIFY(store.setPinned(textId, true));
    qint64 previousUse = 0;
    for (const auto &entry : store.entries()) {
        if (entry.id == textId)
            previousUse = entry.usedAt;
    }
    QVERIFY(previousUse);
    QTest::qWait(5);
    controller.copyEntry(textId);
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 2, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(pastedText(resultPath), QStringLiteral("X11 source text"), 3000);
    for (const auto &entry : store.entries()) {
        if (entry.id == textId) {
            QVERIFY(entry.pinned);
            QVERIFY(entry.usedAt > previousUse);
        }
    }

    sendShortcut(display, XK_v);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 5, 3000);
    qint64 imageId = 0;
    for (const auto &entry : store.entries()) {
        if (entry.kind == QStringLiteral("image")) {
            imageId = entry.id;
            break;
        }
    }
    QVERIFY(imageId);
    controller.copyEntry(imageId);
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 3, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(pastedText(resultPath), QStringLiteral("IMAGE 2x2"), 3000);

    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 6, 3000);
    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 7, 3000);
    QTRY_VERIFY(!picker.isVisible());
    QTest::qWait(30);
    controller.copyEmoji(QStringLiteral("🪐"));
    QTest::qWait(100);
    QCOMPARE(pasteSpy.size(), 3);
    QCOMPARE(pastedText(resultPath), QStringLiteral("IMAGE 2x2"));

    target.terminate();
    QVERIFY(target.waitForFinished(3000));
    QFile::remove(resultPath);

    target.start(QStringLiteral(X11_TEST_TARGET), { resultPath, QStringLiteral("--terminal") });
    QVERIFY(target.waitForStarted());
    QVERIFY(target.waitForReadyRead(3000));
    const QByteArray terminalReady = target.readLine();
    QVERIFY2(terminalReady.startsWith("READY "), terminalReady.constData());
    const Window terminalWindow = terminalReady.mid(6).trimmed().toULong();
    QTRY_VERIFY_WITH_TIMEOUT((XGetInputFocus(display, &focused, &revert), focused == terminalWindow), 3000);
    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 8, 3000);
    controller.copyEmoji(QStringLiteral("🧪"));
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 4, 3000);
    QCOMPARE(pasteSpy.at(3).at(0).toString(), QStringLiteral("Ctrl+Shift+V"));
    QTRY_COMPARE_WITH_TIMEOUT(pastedText(resultPath), QStringLiteral("🧪"), 3000);

    controller.setPasteShortcutMode(QStringLiteral("ctrl-v"));
    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 9, 3000);
    controller.copyEmoji(QStringLiteral("👀"));
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 5, 3000);
    QCOMPARE(pasteSpy.at(4).at(0).toString(), QStringLiteral("Ctrl+V"));
    QCOMPARE(pastedText(resultPath), QStringLiteral("🧪"));
    controller.setPasteShortcutMode(QStringLiteral("auto"));

    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 10, 3000);
    target.terminate();
    QVERIFY(target.waitForFinished(3000));
    controller.copyEmoji(QStringLiteral("🥑"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.status().contains(QStringLiteral("Target window is gone")), 3000);
    QCOMPARE(pasteSpy.size(), 5);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("🥑"));
    XCloseDisplay(display);
}

void X11BackendTest::restoresEmojiClipboardAndCancelsSafely()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    QGuiApplication::clipboard()->clear();
    AppController controller(&store, QStringLiteral("emoji"));
    QQuickWindow picker;
    picker.setProperty("currentView", QStringLiteral("emoji"));
    picker.resize(400, 300);
    X11Backend backend(&picker, &controller);
    QVERIFY2(backend.isAvailable(), qPrintable(backend.errorString()));
    QSignalSpy pasteSpy(&backend, &X11Backend::pasteSent);
    QSignalSpy shortcutSpy(&backend, &X11Backend::shortcutTriggered);
    Display *display = XOpenDisplay(nullptr);
    QVERIFY(display);
    const QString resultPath = directory.filePath(QStringLiteral("paste-result"));
    QProcess target;
    target.start(QStringLiteral(X11_TEST_TARGET), {resultPath});
    QVERIFY(target.waitForStarted());
    QVERIFY(target.waitForReadyRead(3000));
    const QByteArray ready = target.readLine();
    QVERIFY2(ready.startsWith("READY "), ready.constData());
    const Window targetWindow = ready.mid(6).trimmed().toULong();
    QVERIFY(targetWindow);
    QTRY_COMPARE_WITH_TIMEOUT(QGuiApplication::clipboard()->image().size(), QSize(2, 2), 3000);

    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 1, 3000);
    controller.copyEmoji(QStringLiteral("😀"));
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(pastedText(resultPath), QStringLiteral("😀"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(QGuiApplication::clipboard()->image().size(), QSize(2, 2), 3000);
    QVERIFY(controller.status().contains(QStringLiteral("Previous clipboard restored")));

    QGuiApplication::clipboard()->setText(QStringLiteral("prior text"));
    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 2, 3000);
    controller.copyEmoji(QStringLiteral("🧪"));
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 2, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(pastedText(resultPath), QStringLiteral("🧪"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(QGuiApplication::clipboard()->text(), QStringLiteral("prior text"),
                              3000);
    QVERIFY(controller.status().contains(QStringLiteral("Previous clipboard restored")));

    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 3, 3000);
    controller.copyEmoji(QStringLiteral("🪁"));
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 3, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(pastedText(resultPath), QStringLiteral("🪁"), 3000);
    QGuiApplication::clipboard()->setText(QStringLiteral("newer user copy"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.status().contains(QStringLiteral("newer clipboard copy")),
                             3000);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("newer user copy"));

    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 4, 3000);
    controller.copyEmoji(QStringLiteral("🪁"));
    QTRY_COMPARE_WITH_TIMEOUT(pasteSpy.size(), 4, 3000);
    const quint64 priorEvents = backend.clipboardEventCount();
    QGuiApplication::clipboard()->setText(QStringLiteral("🪁"));
    QTRY_VERIFY_WITH_TIMEOUT(backend.clipboardEventCount() > priorEvents, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.status().contains(QStringLiteral("newer clipboard copy")),
                             3000);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("🪁"));

    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 5, 3000);
    controller.copyEmoji(QStringLiteral("🐱"));
    backend.cancel();
    QTest::qWait(100);
    QCOMPARE(pasteSpy.size(), 4);
    QCOMPARE(pastedText(resultPath), QStringLiteral("🪁"));
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("🐱"));
    Window focused = None;
    int revert = 0;
    XGetInputFocus(display, &focused, &revert);
    QCOMPARE(focused, Window(picker.winId()));

    picker.hide();
    XSetInputFocus(display, targetWindow, RevertToPointerRoot, CurrentTime);
    XFlush(display);
    sendShortcut(display, XK_period);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutSpy.size(), 6, 3000);
    target.terminate();
    QVERIFY(target.waitForFinished(3000));
    controller.copyEmoji(QStringLiteral("🥑"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.status().contains(QStringLiteral("Target window is gone")),
                             3000);
    QCOMPARE(pasteSpy.size(), 4);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("🥑"));
    XCloseDisplay(display);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid())
        return 1;
    QCoreApplication::setOrganizationName(QStringLiteral("EmojiKeyboardTests"));
    QCoreApplication::setApplicationName(QStringLiteral("X11Backend"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    X11BackendTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_x11_backend.moc"
