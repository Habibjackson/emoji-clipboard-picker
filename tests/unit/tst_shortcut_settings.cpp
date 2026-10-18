#include "platform/ShortcutSettings.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class ShortcutSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void gnomeSettingsPersistAndReload();
};

void ShortcutSettingsTest::gnomeSettingsPersistAndReload()
{
    const QString schemaDirectory = qEnvironmentVariable("XDG_DATA_HOME")
        + QStringLiteral("/gnome-shell/extensions/emoji-keyboard@emojikeyboard.local/schemas");
    QVERIFY(QDir().mkpath(schemaDirectory));
    QVERIFY(QFile::copy(QString::fromUtf8(TEST_SCHEMA_SOURCE),
                        schemaDirectory + QStringLiteral("/org.gnome.shell.extensions.emoji-keyboard.gschema.xml")));
    QCOMPARE(QProcess::execute(QStringLiteral("glib-compile-schemas"), {schemaDirectory}), 0);

    ShortcutSettings settings;
    QVERIFY(settings.available());
    QCOMPARE(settings.emojiLabel(), QStringLiteral("Not set"));
    QVERIFY(settings.assignKey(QStringLiteral("emoji"), Qt::Key_E, Qt::MetaModifier));
    QVERIFY(settings.assignKey(QStringLiteral("clipboard"), Qt::Key_B,
                               Qt::ControlModifier | Qt::AltModifier));
    QCOMPARE(settings.emojiLabel(), QStringLiteral("Super+E"));
    QCOMPARE(settings.clipboardLabel(), QStringLiteral("Ctrl+Alt+B"));
    QVERIFY(!settings.assignKey(QStringLiteral("emoji"), Qt::Key_B,
                                Qt::ControlModifier | Qt::AltModifier));
    QVERIFY(!settings.assignKey(QStringLiteral("clipboard"), Qt::Key_B,
                                Qt::ControlModifier));

    ShortcutSettings restarted;
    QCOMPARE(restarted.emojiLabel(), QStringLiteral("Super+E"));
    QCOMPARE(restarted.clipboardLabel(), QStringLiteral("Ctrl+Alt+B"));

    QProcess external;
    external.start(QStringLiteral("gsettings"),
                   {QStringLiteral("--schemadir"), schemaDirectory, QStringLiteral("set"),
                    QStringLiteral("org.gnome.shell.extensions.emoji-keyboard"),
                    QStringLiteral("show-emoji"), QStringLiteral("['<Super>f']")});
    QVERIFY(external.waitForFinished(3000));
    QCOMPARE(external.exitCode(), 0);
    restarted.reload();
    QCOMPARE(restarted.emojiLabel(), QStringLiteral("Super+F"));
}

int main(int argc, char *argv[])
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("XDG_DATA_HOME", QFile::encodeName(home.filePath(QStringLiteral("data"))));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(home.filePath(QStringLiteral("config"))));
    qputenv("XDG_CURRENT_DESKTOP", "GNOME");
    qputenv("GSETTINGS_BACKEND", "keyfile");
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ShortcutSettingsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_shortcut_settings.moc"
