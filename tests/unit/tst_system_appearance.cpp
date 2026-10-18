#include "platform/SystemAppearance.h"

#include <QDBusConnection>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QStyleHints>
#include <QTest>

class TestPortalSettings : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Settings")

public:
    uint scheme = 1;

public slots:
    QDBusVariant Read(const QString &group, const QString &key)
    {
        if (group != QStringLiteral("org.freedesktop.appearance")
            || key != QStringLiteral("color-scheme"))
            return {};
        return QDBusVariant(QVariant::fromValue(scheme));
    }

signals:
    void SettingChanged(const QString &group, const QString &key,
                        const QDBusVariant &value);
};

class SystemAppearanceTest : public QObject
{
    Q_OBJECT

private slots:
    void followsPortalPreference()
    {
        qGuiApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        TestPortalSettings portal;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"),
                                   &portal, QDBusConnection::ExportAllSlots
                                               | QDBusConnection::ExportAllSignals));

        SystemAppearance appearance;
        QCOMPARE(appearance.dark(), true);
        QSignalSpy darkChanged(&appearance, &SystemAppearance::darkChanged);

        portal.scheme = 2;
        emit portal.SettingChanged(QStringLiteral("org.freedesktop.appearance"),
                                   QStringLiteral("color-scheme"),
                                   QDBusVariant(QVariant::fromValue(uint(2))));
        QTRY_COMPARE(appearance.dark(), false);
        QCOMPARE(darkChanged.size(), 1);

        portal.scheme = 1;
        emit portal.SettingChanged(QStringLiteral("org.freedesktop.appearance"),
                                   QStringLiteral("color-scheme"),
                                   QDBusVariant(QVariant::fromValue(uint(1))));
        QTRY_COMPARE(appearance.dark(), true);
        QCOMPARE(darkChanged.size(), 2);
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
    QGuiApplication app(argc, argv);
    SystemAppearanceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_system_appearance.moc"
