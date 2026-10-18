#include "platform/ShortcutSpec.h"

#include <QTest>

class ShortcutSpecTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsRoundTrip();
    void acceptsSupportedCombinations();
    void rejectsUnsafeOrInvalidCombinations();
};

void ShortcutSpecTest::defaultsRoundTrip()
{
    const auto emoji = ShortcutSpec::emojiDefault();
    QCOMPARE(emoji.accelerator, QStringLiteral("<Super>period"));
    QCOMPARE(emoji.label, QStringLiteral("Super+."));
    const auto clipboard = ShortcutSpec::clipboardDefault();
    QCOMPARE(clipboard.accelerator, QStringLiteral("<Super>v"));
    QCOMPARE(clipboard.label, QStringLiteral("Super+V"));
    QCOMPARE(ShortcutSpec::fromAccelerator(emoji.accelerator)->accelerator,
             emoji.accelerator);
    QCOMPARE(ShortcutSpec::fromAccelerator(clipboard.accelerator)->accelerator,
             clipboard.accelerator);
}

void ShortcutSpecTest::acceptsSupportedCombinations()
{
    const auto combo = ShortcutSpec::fromKey(Qt::Key_E,
        Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
    QVERIFY(combo);
    QCOMPARE(combo->accelerator, QStringLiteral("<Control><Alt><Shift>e"));
    QCOMPARE(combo->label, QStringLiteral("Ctrl+Alt+Shift+E"));
    QCOMPARE(ShortcutSpec::fromAccelerator(QStringLiteral("<Mod4>F8"))->accelerator,
             QStringLiteral("<Super>F8"));
    QCOMPARE(ShortcutSpec::fromKey(Qt::Key_Greater,
                                  Qt::MetaModifier | Qt::ShiftModifier)->accelerator,
             QStringLiteral("<Super><Shift>period"));
}

void ShortcutSpecTest::rejectsUnsafeOrInvalidCombinations()
{
    QVERIFY(!ShortcutSpec::fromKey(Qt::Key_V, Qt::ControlModifier));
    QVERIFY(!ShortcutSpec::fromKey(Qt::Key_V, Qt::NoModifier));
    QVERIFY(!ShortcutSpec::fromKey(Qt::Key_Escape, Qt::MetaModifier));
    QVERIFY(!ShortcutSpec::fromAccelerator(QStringLiteral("<Super><Super>v")));
    QVERIFY(!ShortcutSpec::fromAccelerator(QStringLiteral("<Alt>v")));
    QVERIFY(!ShortcutSpec::fromAccelerator(QStringLiteral("<Super>Escape")));
}

QTEST_GUILESS_MAIN(ShortcutSpecTest)
#include "tst_shortcut_spec.moc"
