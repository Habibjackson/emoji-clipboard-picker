#include "app/AppController.h"
#include "storage/HistoryStore.h"

#include <QClipboard>
#include <QBuffer>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

class AppControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void capturesAndCopies();
    void capturesCommonMimeFormatsAndPrefersImage();
    void skipsUnsafeOrUnsupportedClipboardItems();
    void snapshotsSupportedClipboardBeforeEmoji();
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

void AppControllerTest::skipsUnsafeOrUnsupportedClipboardItems()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->clear();
    AppController controller(&store, QStringLiteral("clipboard"));

    clipboard->setText(QString(1024 * 1024 + 1, QLatin1Char('a')));
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Text is too large for history"));

    QImage tooWide(8193, 1, QImage::Format_RGB32);
    tooWide.fill(Qt::red);
    clipboard->setImage(tooWide);
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Image is too large for history"));

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

    auto *corrupt = new QMimeData;
    corrupt->setData(QStringLiteral("image/jpeg"), QByteArray("not an image"));
    clipboard->setMimeData(corrupt);
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Image could not be decoded for history"));

    clipboard->setText(QString());
    controller.captureNow();
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(controller.status(), QStringLiteral("Empty clipboard item was skipped"));
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

    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    clipboard->setImage(image);
    controller.copyEmoji(QString::fromUtf8("🎉"));
    QVERIFY(controller.emojiClipboardSnapshot(&temporaryText, &mimeType, &content));
    QCOMPARE(temporaryText, QString::fromUtf8("🎉"));
    QCOMPARE(mimeType, QStringLiteral("image/png"));
    QCOMPARE(QImage::fromData(content, "PNG"), image);
}

QTEST_MAIN(AppControllerTest)

#include "tst_app_controller.moc"
