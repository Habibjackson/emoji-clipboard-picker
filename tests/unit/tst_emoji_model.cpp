#include "models/EmojiModel.h"

#include <QtTest>

namespace {
QString nameForSymbol(const EmojiModel &model, const QString &symbol)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row);
        if (model.data(index, EmojiModel::SymbolRole).toString() == symbol)
            return model.data(index, EmojiModel::NameRole).toString();
    }
    return {};
}
}

class EmojiModelTest : public QObject
{
    Q_OBJECT

private slots:
    void cldrNamesAndKeywords();
    void unicodeFallbackForNewEmoji();
};

void EmojiModelTest::cldrNamesAndKeywords()
{
    EmojiModel model;
    QVERIFY(model.load(QStringLiteral(":/data/emoji-test.txt")));
    QCOMPARE(model.resultCount(), 3963);

    model.setQuery(QStringLiteral("awesome"));
    QCOMPARE(nameForSymbol(model, QString::fromUtf8("😎")),
             QStringLiteral("smiling face with sunglasses"));

    model.setQuery(QStringLiteral("coder"));
    QCOMPARE(nameForSymbol(model, QString::fromUtf8("👩🏽‍💻")),
             QStringLiteral("woman technologist: medium skin tone"));

    model.setQuery(QStringLiteral("person walking facing right"));
    QCOMPARE(nameForSymbol(model, QString::fromUtf8("🚶‍➡️")),
             QStringLiteral("person walking: facing right"));
}

void EmojiModelTest::unicodeFallbackForNewEmoji()
{
    EmojiModel model;
    QVERIFY(model.load(QStringLiteral(":/data/emoji-test.txt")));
    model.setQuery(QStringLiteral("cracking face"));
    QCOMPARE(nameForSymbol(model, QString::fromUtf8("🫫")),
             QStringLiteral("cracking face"));
}

QTEST_GUILESS_MAIN(EmojiModelTest)

#include "tst_emoji_model.moc"
