#include "app/AppController.h"
#include "storage/HistoryStore.h"

#include <QClipboard>
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
};

void AppControllerTest::capturesAndCopies()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY2(store.open(), qPrintable(store.errorString()));
    QGuiApplication::clipboard()->clear();
    AppController controller(&store, QStringLiteral("emoji"));
    QVERIFY(controller.emojiModel()->resultCount() > 3000);
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

    auto *secret = new QMimeData;
    secret->setText(QStringLiteral("synthetic password"));
    secret->setData(QStringLiteral("application/x-kde-passwordManagerHint"), QByteArray("secret"));
    QGuiApplication::clipboard()->setMimeData(secret);
    controller.captureNow();
    QCOMPARE(store.entries().size(), 2);

    controller.setPaused(true);
    QGuiApplication::clipboard()->setText(QStringLiteral("do not save this"));
    controller.captureNow();
    QCOMPARE(store.entries().size(), 2);
}

QTEST_MAIN(AppControllerTest)

#include "tst_app_controller.moc"
