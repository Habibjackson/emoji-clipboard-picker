#include "app/AppController.h"
#include "storage/HistoryStore.h"

#include <QClipboard>
#include <QBuffer>
#include <QGuiApplication>
#include <QImage>
#include <QLoggingCategory>
#include <QMimeData>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

namespace {
QStringList *capturedMessages = nullptr;

void captureMessage(QtMsgType, const QMessageLogContext &, const QString &message)
{
    if (capturedMessages)
        capturedMessages->append(message);
}

struct MessageCapture
{
    explicit MessageCapture(QStringList &messages)
        : previous(qInstallMessageHandler(captureMessage))
    {
        capturedMessages = &messages;
    }

    ~MessageCapture()
    {
        capturedMessages = nullptr;
        qInstallMessageHandler(previous);
    }

    QtMessageHandler previous;
};
}

class AppControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void capturesAndCopies();
    void selectingHistoryKeepsItsOrder();
    void capturesCommonMimeFormatsAndPrefersImage();
    void capturesShellClipboardPayload();
    void skipsUnsafeOrUnsupportedClipboardItems();
    void pauseResumeCapturesCurrentClipboard();
    void keepsPasteFailuresVisibleUntilTheNextAction();
    void restoredClipboardIsNotRecaptured();
    void laterCopyOfTemporaryEmojiIsCaptured();
    void newerIdenticalEmojiCopyIsCaptured();
    void snapshotsSupportedClipboardBeforeEmoji();
    void preferencesPersist();
    void doesNotLogClipboardPayload();
};

void AppControllerTest::capturesAndCopies()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY2(store.open(), qPrintable(store.errorString()));
    QGuiApplication::clipboard()->clear();
    AppController controller(&store, QStringLiteral("emoji"));
    QVERIFY(controller.emojiModel()->resultCount() > 1900);
    QVERIFY(controller.emojiModel()->categories().contains(QStringLiteral("Smileys & Emotion")));
    controller.emojiModel()->setQuery(QStringLiteral("grinning face"));
    QVERIFY(controller.emojiModel()->resultCount() > 0);
    QVERIFY(controller.emojiModel()->resultCount() < 100);
    controller.emojiModel()->setQuery(QString());
    controller.emojiModel()->setCategory(QStringLiteral("Recent"));
    QCOMPARE(controller.emojiModel()->resultCount(), 0);

    QGuiApplication::clipboard()->setText(QStringLiteral("copied from another app"));
    QTRY_COMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().first().text, QStringLiteral("copied from another app"));
    controller.clipboardModel()->setQuery(QStringLiteral("another"));
    QCOMPARE(controller.clipboardModel()->resultCount(), 1);
    controller.clipboardModel()->setQuery(QStringLiteral("missing"));
    QCOMPARE(controller.clipboardModel()->resultCount(), 0);
    controller.clipboardModel()->setQuery(QString());
    controller.copyEntry(store.entries().first().id);
    controller.captureNow();
    QCOMPARE(store.entries().size(), 1);

    controller.copyEmoji(QString::fromUtf8("😀"));
    QCOMPARE(store.recentEmoji().first(), QString::fromUtf8("😀"));
    QCOMPARE(controller.emojiModel()->resultCount(), 1);
    controller.captureNow();
    QCOMPARE(store.entries().size(), 1);

    QImage image(64, 48, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    QGuiApplication::clipboard()->setImage(image);
    QTRY_COMPARE(store.entries().size(), 2);
    QCOMPARE(store.entries().first().kind, QStringLiteral("image"));
    QVERIFY(!store.preview(store.entries().first().id).isEmpty());
    controller.copyEntry(store.entries().first().id);
    QCOMPARE(QGuiApplication::clipboard()->image().size(), image.size());
    controller.captureNow();
    QCOMPARE(store.entries().size(), 2);

    auto *files = new QMimeData;
    files->setUrls({ QUrl(QStringLiteral("file:///tmp/example.txt")) });
    QGuiApplication::clipboard()->setMimeData(files);
    controller.captureNow();
    QCOMPARE(store.entries().size(), 2);
    QCOMPARE(controller.status(), QStringLiteral("File copy was skipped"));

    auto *secret = new QMimeData;
    secret->setText(QStringLiteral("synthetic password"));
    secret->setData(QStringLiteral("application/x-kde-passwordManagerHint"), QByteArray("secret"));
    QGuiApplication::clipboard()->setMimeData(secret);
    controller.captureNow();
    QCOMPARE(store.entries().size(), 2);
    QCOMPARE(controller.status(), QStringLiteral("Private clipboard item was skipped"));

    controller.setPaused(true);
    QGuiApplication::clipboard()->setText(QStringLiteral("do not save this"));
    controller.captureNow();
    QCOMPARE(store.entries().size(), 2);
    controller.clearAll();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.clipboardModel()->resultCount(), 0);
    QCOMPARE(controller.status(), QStringLiteral("All clipboard history cleared"));
}

void AppControllerTest::selectingHistoryKeepsItsOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    QGuiApplication::clipboard()->clear();
    QVERIFY(store.addText(QStringLiteral("older")));
    QVERIFY(store.addText(QStringLiteral("newer")));
    const auto before = store.entries();
    QCOMPARE(before.size(), 2);
    QCOMPARE(before.at(0).text, QStringLiteral("newer"));
    QCOMPARE(before.at(1).text, QStringLiteral("older"));

    AppController controller(&store, QStringLiteral("clipboard"));
    controller.copyEntry(before.at(1).id);
    controller.captureNow();
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("older"));
    const auto after = store.entries();
    QCOMPARE(after.size(), 2);
    for (int i = 0; i < before.size(); ++i) {
        QCOMPARE(after.at(i).id, before.at(i).id);
        QCOMPARE(after.at(i).usedAt, before.at(i).usedAt);
    }
    QCOMPARE(controller.clipboardModel()->data(controller.clipboardModel()->index(0),
                                               ClipboardModel::EntryIdRole).toLongLong(),
             before.at(0).id);

    QVERIFY(store.addText(QStringLiteral("older")));
    QCOMPARE(store.entries().first().id, before.at(1).id);
}

void AppControllerTest::capturesCommonMimeFormatsAndPrefersImage()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->clear();
    AppController controller(&store, QStringLiteral("clipboard"));

    const QStringList formats = { QStringLiteral("PNG"), QStringLiteral("JPEG"),
                                  QStringLiteral("BMP") };
    for (int i = 0; i < formats.size(); ++i) {
        QImage image(40, 30, QImage::Format_RGB32);
        image.fill(QColor::fromHsv(i * 100, 230, 220));
        QByteArray encoded;
        QBuffer buffer(&encoded);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, formats.at(i).toLatin1().constData()));

        auto *mime = new QMimeData;
        mime->setData(QStringLiteral("image/%1").arg(formats.at(i).toLower()), encoded);
        mime->setText(QStringLiteral("text alternative %1").arg(i));
        clipboard->setMimeData(mime);
        controller.captureNow();

        QCOMPARE(store.entries().size(), i + 1);
        const ClipboardEntry entry = store.entries().first();
        QCOMPARE(entry.kind, QStringLiteral("image"));
        QCOMPARE(entry.mime, QStringLiteral("image/png"));
        QVERIFY(entry.text.isEmpty());
        QCOMPARE(QImage::fromData(store.payload(entry.id), "PNG").size(), image.size());
        QVERIFY(!store.preview(entry.id).isEmpty());
    }
}

void AppControllerTest::capturesShellClipboardPayload()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    QGuiApplication::clipboard()->clear();
    AppController controller(&store, QStringLiteral("clipboard"));

    const QByteArray text = QString::fromUtf8("Wayland 😀\nsecond line").toUtf8();
    controller.capturePayload(QStringLiteral("text"),
                              QStringLiteral("text/plain;charset=utf-8"), text);
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().first().text, QString::fromUtf8(text));
    controller.capturePayload(QStringLiteral("text"), QStringLiteral("text/plain"), text);
    QCOMPARE(store.entries().size(), 1);

    QImage image(3, 2, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray encoded;
    QBuffer buffer(&encoded);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&buffer, "PNG"));
    controller.capturePayload(QStringLiteral("image"), QStringLiteral("image/png"), encoded);
    QCOMPARE(store.entries().size(), 2);
    const ClipboardEntry entry = store.entries().first();
    QCOMPARE(entry.kind, QStringLiteral("image"));
    QCOMPARE(QImage::fromData(store.payload(entry.id), "PNG").size(), image.size());
    QVERIFY(!store.preview(entry.id).isEmpty());

    controller.capturePayload(QStringLiteral("image"), QStringLiteral("image/avif"), {});
    QCOMPARE(store.entries().size(), 2);
    QCOMPARE(controller.status(), QStringLiteral("Unsupported image format was skipped"));

    controller.setPaused(true);
    controller.capturePayload(QStringLiteral("text"), QStringLiteral("text/plain"),
                              QByteArray("paused"));
    QCOMPARE(store.entries().size(), 2);
    controller.setPaused(false);
    controller.capturePayload(QStringLiteral("text"), QStringLiteral("application/x-keepassxc-secret"),
                              QByteArray("synthetic secret"));
    QCOMPARE(store.entries().size(), 2);
}

void AppControllerTest::skipsUnsafeOrUnsupportedClipboardItems()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->clear();
    AppController controller(&store, QStringLiteral("clipboard"));
    QTest::failOnWarning();

    clipboard->setText(QString(1024 * 1024 + 1, QLatin1Char('a')));
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Text is too large for history"));
    QCOMPARE(controller.attention(), controller.status());

    QImage tooWide(8193, 1, QImage::Format_RGB32);
    tooWide.fill(Qt::red);
    clipboard->setImage(tooWide);
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Image is too large for history"));
    QCOMPARE(controller.attention(), controller.status());

    auto *oversized = new QMimeData;
    oversized->setData(QStringLiteral("image/png"), QByteArray(10 * 1024 * 1024 + 1, 'x'));
    clipboard->setMimeData(oversized);
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Image is too large for history"));

    auto *unsupported = new QMimeData;
    unsupported->setData(QStringLiteral("image/avif"), QByteArray("unsupported"));
    unsupported->setText(QStringLiteral("should not save the text alternative"));
    clipboard->setMimeData(unsupported);
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Unsupported image format was skipped"));
    QCOMPARE(controller.attention(), QStringLiteral("Image is too large for history"));

    auto *corrupt = new QMimeData;
    corrupt->setData(QStringLiteral("image/jpeg"), QByteArray("not an image"));
    clipboard->setMimeData(corrupt);
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Image could not be decoded for history"));

    auto *truncatedJpeg = new QMimeData;
    truncatedJpeg->setData(QStringLiteral("image/jpeg"),
                           QByteArray::fromHex("ffd8ff") + QByteArray("private"));
    clipboard->setMimeData(truncatedJpeg);
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Image could not be decoded for history"));

    clipboard->setText(QString());
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Empty clipboard item was skipped"));

    clipboard->setText(QStringLiteral("valid text clears the oversized warning"));
    controller.captureNow();
    QCOMPARE(controller.attention(), QString());
}

void AppControllerTest::pauseResumeCapturesCurrentClipboard()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->clear();
    AppController controller(&store, QStringLiteral("clipboard"));
    controller.setPaused(true);
    clipboard->setText(QStringLiteral("copied while paused"));
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    controller.setPaused(false);
    QTRY_COMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().first().text, QStringLiteral("copied while paused"));
}

void AppControllerTest::keepsPasteFailuresVisibleUntilTheNextAction()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    QGuiApplication::clipboard()->clear();
    AppController controller(&store, QStringLiteral("emoji"));

    controller.reportPasteResult(QStringLiteral("target-missing"), QString());
    QCOMPARE(controller.attention(),
             QStringLiteral("Target window is gone. Item copied; paste manually."));
    controller.setPaused(true);
    QCOMPARE(controller.attention(),
             QStringLiteral("Target window is gone. Item copied; paste manually."));

    controller.reportPasteResult(QStringLiteral("sent"), QStringLiteral("Ctrl+V"));
    QCOMPARE(controller.attention(), QString());
    controller.reportPasteResult(QStringLiteral("focus-failed"), QString());
    QVERIFY(!controller.attention().isEmpty());
    controller.copyEmoji(QString::fromUtf8("😀"));
    QCOMPARE(controller.attention(), QString());
}

void AppControllerTest::restoredClipboardIsNotRecaptured()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->clear();
    AppController controller(&store, QStringLiteral("emoji"));

    clipboard->setText(QStringLiteral("previous text"));
    QTRY_COMPARE(store.entries().size(), 1);
    const qint64 originalUse = store.entries().first().usedAt;
    controller.copyEmoji(QString::fromUtf8("😀"));
    QString temporaryText;
    QString mimeType;
    QByteArray content;
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    controller.expectClipboardRestore(mimeType, content);
    clipboard->setText(QStringLiteral("previous text"));
    controller.captureNow();
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().first().usedAt, originalUse);
    controller.reportClipboardRestoreResult(AppController::ClipboardRestoreResult::Restored);
    controller.discardEmojiClipboardSnapshot();

    controller.copyEmoji(QString::fromUtf8("🧪"));
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    controller.expectClipboardRestore(mimeType, content);
    clipboard->setText(QStringLiteral("previous text"));
    controller.captureNow();
    QCOMPARE(store.entries().first().usedAt, originalUse);
    controller.reportClipboardRestoreResult(AppController::ClipboardRestoreResult::Changed);
    controller.discardEmojiClipboardSnapshot();
    QVERIFY(store.entries().first().usedAt > originalUse);

    clipboard->setText(QStringLiteral("new user copy"));
    QTRY_COMPARE(store.entries().size(), 2);
    QCOMPARE(store.entries().first().text, QStringLiteral("new user copy"));

    QImage image(24, 24, QImage::Format_ARGB32);
    image.fill(Qt::cyan);
    clipboard->setImage(image);
    QTRY_COMPARE(store.entries().size(), 3);
    const qint64 imageId = store.entries().first().id;
    const qint64 imageUse = store.entries().first().usedAt;
    controller.copyEmoji(QString::fromUtf8("🎉"));
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    QCOMPARE(mimeType, QStringLiteral("image/png"));
    controller.expectClipboardRestore(mimeType, content);
    clipboard->setImage(image);
    controller.captureNow();
    QCOMPARE(store.entries().size(), 3);
    QCOMPARE(store.entries().first().id, imageId);
    QCOMPARE(store.entries().first().usedAt, imageUse);
    controller.reportClipboardRestoreResult(AppController::ClipboardRestoreResult::Restored);
    controller.discardEmojiClipboardSnapshot();
}

void AppControllerTest::laterCopyOfTemporaryEmojiIsCaptured()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->clear();
    AppController controller(&store, QStringLiteral("emoji"));
    clipboard->setText(QStringLiteral("prior"));
    QTRY_COMPARE(store.entries().size(), 1);
    controller.copyEmoji(QString::fromUtf8("😀"));

    QString temporaryText;
    QString mimeType;
    QByteArray content;
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    controller.expectClipboardRestore(mimeType, content);
    clipboard->setText(QStringLiteral("prior"));
    controller.captureNow();
    controller.reportClipboardRestoreResult(AppController::ClipboardRestoreResult::Restored);
    controller.discardEmojiClipboardSnapshot();

    clipboard->setText(QString::fromUtf8("😀"));
    QTRY_COMPARE(store.entries().size(), 2);
    QCOMPARE(store.entries().first().text, QString::fromUtf8("😀"));
}

void AppControllerTest::newerIdenticalEmojiCopyIsCaptured()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->clear();
    AppController controller(&store, QStringLiteral("emoji"));
    clipboard->setText(QStringLiteral("prior"));
    QTRY_COMPARE(store.entries().size(), 1);
    controller.copyEmoji(QString::fromUtf8("😀"));

    QString temporaryText;
    QString mimeType;
    QByteArray content;
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    controller.expectClipboardRestore(mimeType, content);
    clipboard->setText(QString::fromUtf8("😀"));
    controller.captureNow();
    QCOMPARE(store.entries().size(), 1);
    controller.reportClipboardRestoreResult(AppController::ClipboardRestoreResult::Changed);
    controller.discardEmojiClipboardSnapshot();
    QTRY_COMPARE(store.entries().size(), 2);
    QCOMPARE(store.entries().first().text, QString::fromUtf8("😀"));
}

void AppControllerTest::snapshotsSupportedClipboardBeforeEmoji()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY2(store.open(), qPrintable(store.errorString()));
    AppController controller(&store, QStringLiteral("emoji"));
    auto *clipboard = QGuiApplication::clipboard();
    QString temporaryText;
    QString mimeType;
    QByteArray content;

    clipboard->setText(QStringLiteral("previous text"));
    controller.copyEmoji(QString::fromUtf8("😀"));
    QCOMPARE(clipboard->text(), QString::fromUtf8("😀"));
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    QCOMPARE(temporaryText, QString::fromUtf8("😀"));
    QCOMPARE(mimeType, QStringLiteral("text/plain;charset=utf-8"));
    QCOMPARE(QString::fromUtf8(content), QStringLiteral("previous text"));

    controller.copyEmoji(QString::fromUtf8("🧪"));
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    QCOMPARE(temporaryText, QString::fromUtf8("🧪"));
    QCOMPARE(mimeType, QStringLiteral("text/plain;charset=utf-8"));
    QCOMPARE(QString::fromUtf8(content), QStringLiteral("previous text"));

    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    clipboard->setImage(image);
    controller.copyEmoji(QString::fromUtf8("🎉"));
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    QCOMPARE(temporaryText, QString::fromUtf8("🎉"));
    QCOMPARE(mimeType, QStringLiteral("image/png"));
    QCOMPARE(QImage::fromData(content, "PNG"), image);
}

void AppControllerTest::preferencesPersist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString previousOrganization = QCoreApplication::organizationName();
    const QString previousApplication = QCoreApplication::applicationName();
    QCoreApplication::setOrganizationName(QStringLiteral("EmojiKeyboardTests"));
    QCoreApplication::setApplicationName(QStringLiteral("PasteShortcutOverride"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());

    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    {
        AppController controller(&store, QStringLiteral("emoji"));
        QCOMPARE(controller.pasteShortcutMode(), QStringLiteral("auto"));
        QCOMPARE(controller.themeMode(), QStringLiteral("system"));
        QCOMPARE(controller.pasteShortcutFor(false), QStringLiteral("Ctrl+V"));
        QCOMPARE(controller.pasteShortcutFor(true), QStringLiteral("Ctrl+Shift+V"));
        QSignalSpy changed(&controller, &AppController::pasteShortcutModeChanged);
        controller.setPasteShortcutMode(QStringLiteral("ctrl-shift-v"));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(controller.pasteShortcutFor(false), QStringLiteral("Ctrl+Shift+V"));
        controller.setPasteShortcutMode(QStringLiteral("invalid"));
        QCOMPARE(changed.size(), 1);
        QSignalSpy themeChanged(&controller, &AppController::themeModeChanged);
        controller.setThemeMode(QStringLiteral("dark"));
        QCOMPARE(themeChanged.size(), 1);
        controller.setThemeMode(QStringLiteral("invalid"));
        QCOMPARE(themeChanged.size(), 1);
    }
    {
        AppController controller(&store, QStringLiteral("emoji"));
        QCOMPARE(controller.pasteShortcutMode(), QStringLiteral("ctrl-shift-v"));
        QCOMPARE(controller.themeMode(), QStringLiteral("dark"));
        controller.setPasteShortcutMode(QStringLiteral("ctrl-v"));
        QCOMPARE(controller.pasteShortcutFor(true), QStringLiteral("Ctrl+V"));
        controller.setThemeMode(QStringLiteral("light"));
        QCOMPARE(controller.themeMode(), QStringLiteral("light"));
        controller.setThemeMode(QStringLiteral("system"));
        QCOMPARE(controller.themeMode(), QStringLiteral("system"));
    }
    QCoreApplication::setOrganizationName(previousOrganization);
    QCoreApplication::setApplicationName(previousApplication);
}

void AppControllerTest::doesNotLogClipboardPayload()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->clear();
    AppController controller(&store, QStringLiteral("clipboard"));
    const QString sentinel = QStringLiteral("synthetic-private-payload-6f9a21");
    QStringList messages;
    {
        MessageCapture capture(messages);
        clipboard->setText(sentinel);
        controller.captureNow();
        QCOMPARE(store.entries().first().text, sentinel);

        auto *invalidImage = new QMimeData;
        invalidImage->setData(QStringLiteral("image/jpeg"),
                              QByteArray::fromHex("ffd8ff") + sentinel.toUtf8());
        clipboard->setMimeData(invalidImage);
        controller.captureNow();
        QCOMPARE(controller.status(), QStringLiteral("Image could not be decoded for history"));
    }
    QVERIFY2(!messages.join(QLatin1Char('\n')).contains(sentinel),
             "Clipboard payload appeared in a diagnostic message");
}

QTEST_MAIN(AppControllerTest)

#include "tst_app_controller.moc"
