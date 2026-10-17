#include "storage/HistoryStore.h"

#include <QBuffer>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

class HistoryStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void limitsAndPersistence();
    void deduplication();
    void emojiAndImage();
    void privatePermissions();
};

void HistoryStoreTest::limitsAndPersistence()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("private/history.sqlite"));
    QList<qint64> pinnedIds;

    {
        HistoryStore store(path);
        QVERIFY2(store.open(), qPrintable(store.errorString()));
        for (int i = 0; i < 20; ++i)
            QVERIFY(store.addText(QStringLiteral("item-%1").arg(i)));
        QCOMPARE(store.entries().size(), 20);

        const QList<ClipboardEntry> initial = store.entries();
        for (int i = 0; i < 5; ++i) {
            pinnedIds.append(initial.at(i).id);
            QVERIFY(store.setPinned(initial.at(i).id, true));
        }
        QVERIFY(!store.setPinned(initial.at(5).id, true));
        QVERIFY(store.errorString().contains(QStringLiteral("five")));

        for (int i = 20; i < 52; ++i)
            QVERIFY(store.addText(QStringLiteral("item-%1").arg(i)));
        const QList<ClipboardEntry> capped = store.entries();
        QCOMPARE(capped.size(), 25);
        QCOMPARE(std::count_if(capped.cbegin(), capped.cend(),
                               [](const ClipboardEntry &item) { return item.pinned; }), 5);
        for (qint64 id : pinnedIds)
            QVERIFY(std::any_of(capped.cbegin(), capped.cend(),
                                [id](const ClipboardEntry &item) { return item.id == id; }));

        QVERIFY(store.setPinned(pinnedIds.first(), false));
        QCOMPARE(store.entries().size(), 24);
        QVERIFY(store.clearRecent());
        QCOMPARE(store.entries().size(), 4);
    }

    HistoryStore reopened(path);
    QVERIFY2(reopened.open(), qPrintable(reopened.errorString()));
    QCOMPARE(reopened.entries().size(), 4);
    for (const ClipboardEntry &entry : reopened.entries())
        QVERIFY(entry.pinned);
}

void HistoryStoreTest::deduplication()
{
    QTemporaryDir directory;
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QVERIFY(store.open());
    QVERIFY(store.addText(QStringLiteral("hello")));
    const ClipboardEntry first = store.entries().first();
    QVERIFY(store.setPinned(first.id, true));
    QVERIFY(store.addText(QStringLiteral("hello")));
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().first().id, first.id);
    QVERIFY(store.entries().first().pinned);
    QVERIFY(store.entries().first().usedAt > first.usedAt);
}

void HistoryStoreTest::emojiAndImage()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("history.sqlite"));
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray png;
    QBuffer buffer(&png);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&buffer, "PNG"));

    {
        HistoryStore store(path);
        QVERIFY(store.open());
        for (int i = 0; i < 26; ++i)
            QVERIFY(store.recordEmoji(QStringLiteral("emoji-%1").arg(i)));
        QCOMPARE(store.recentEmoji().size(), 24);
        QVERIFY(!store.recentEmoji().contains(QStringLiteral("emoji-0")));
        QVERIFY(store.recordEmoji(QStringLiteral("emoji-2")));
        QCOMPARE(store.recentEmoji().first(), QStringLiteral("emoji-2"));
        QVERIFY(store.addImage(png, png));
    }

    HistoryStore reopened(path);
    QVERIFY(reopened.open());
    QCOMPARE(reopened.recentEmoji().first(), QStringLiteral("emoji-2"));
    QCOMPARE(reopened.entries().size(), 1);
    QCOMPARE(reopened.entries().first().kind, QStringLiteral("image"));
    QCOMPARE(reopened.payload(reopened.entries().first().id), png);
    QCOMPARE(reopened.preview(reopened.entries().first().id), png);
}

void HistoryStoreTest::privatePermissions()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("private/history.sqlite"));
    HistoryStore store(path);
    QVERIFY(store.open());
    const QFile::Permissions forbidden = QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup
        | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther;
    QCOMPARE(QFileInfo(path).permissions() & forbidden, QFile::Permissions());
    QCOMPARE(QFileInfo(QFileInfo(path).absolutePath()).permissions() & forbidden,
             QFile::Permissions());
}

QTEST_GUILESS_MAIN(HistoryStoreTest)

#include "tst_history_store.moc"
