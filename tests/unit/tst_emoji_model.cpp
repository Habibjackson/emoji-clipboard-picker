#include "models/EmojiModel.h"

#include <QGuiApplication>
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

QString nameForVariant(const QVariantList &variants, const QString &symbol)
{
    for (const QVariant &value : variants) {
        const QVariantMap variant = value.toMap();
        if (variant.value(QStringLiteral("symbol")).toString() == symbol)
            return variant.value(QStringLiteral("emojiName")).toString();
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
    void groupsSkinToneVariants();
};

void EmojiModelTest::cldrNamesAndKeywords()
{
    EmojiModel model;
    QVERIFY(model.load(QStringLiteral(":/data/emoji-test.txt")));
    QCOMPARE(model.resultCount(), 1923);

    model.setQuery(QStringLiteral("awesome"));
    QCOMPARE(nameForSymbol(model, QString::fromUtf8("😎")),
             QStringLiteral("smiling face with sunglasses"));

    model.setQuery(QStringLiteral("coder"));
    QCOMPARE(nameForSymbol(model, QString::fromUtf8("👩‍💻")),
             QStringLiteral("woman technologist"));
    QCOMPARE(nameForVariant(model.variantsFor(QString::fromUtf8("👩‍💻")),
                            QString::fromUtf8("👩🏽‍💻")),
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

void EmojiModelTest::groupsSkinToneVariants()
{
    EmojiModel model;
    QVERIFY(model.load(QStringLiteral(":/data/emoji-test.txt")));
    QCOMPARE(model.resultCount(), 1923);

    const QString wave = QString::fromUtf8("👋");
    const QVariantList waveVariants = model.variantsFor(wave);
    QCOMPARE(waveVariants.size(), 6);
    QCOMPARE(waveVariants.first().toMap().value(QStringLiteral("symbol")).toString(), wave);
    QCOMPARE(model.variantsFor(QString::fromUtf8("🤝")).size(), 26);
    QCOMPARE(model.variantsFor(QString::fromUtf8("👯")).size(), 26);
    QVERIFY(model.variantsFor(wave).first().toMap().value(QStringLiteral("renderable")).isValid());

    const QString selectedTone = QString::fromUtf8("👩🏽‍💻");
    model.setQuery(selectedTone);
    QCOMPARE(model.resultCount(), 1);
    QCOMPARE(model.data(model.index(0), EmojiModel::SymbolRole).toString(),
             QString::fromUtf8("👩‍💻"));
    QCOMPARE(model.data(model.index(0), EmojiModel::VariantCountRole).toInt(), 5);

    model.setQuery(QString());
    model.setRecent({ selectedTone });
    model.setCategory(QStringLiteral("Recent"));
    QCOMPARE(model.resultCount(), 1);
    QCOMPARE(model.data(model.index(0), EmojiModel::SymbolRole).toString(), selectedTone);
    QCOMPARE(model.data(model.index(0), EmojiModel::VariantCountRole).toInt(), 0);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    EmojiModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_emoji_model.moc"
