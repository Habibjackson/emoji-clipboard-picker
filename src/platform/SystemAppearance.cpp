#include "SystemAppearance.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QStyleHints>

namespace {
constexpr auto portalService = "org.freedesktop.portal.Desktop";
constexpr auto portalPath = "/org/freedesktop/portal/desktop";
constexpr auto portalInterface = "org.freedesktop.portal.Settings";
constexpr auto appearanceGroup = "org.freedesktop.appearance";
constexpr auto colorSchemeKey = "color-scheme";

QVariant unwrapVariant(QVariant value)
{
    while (value.metaType() == QMetaType::fromType<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}
}

SystemAppearance::SystemAppearance(QObject *parent)
    : QObject(parent)
{
    connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged, this,
            &SystemAppearance::refresh);
    refresh();

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    bus.connect(QString::fromLatin1(portalService), QString::fromLatin1(portalPath),
                QString::fromLatin1(portalInterface), QStringLiteral("SettingChanged"),
                this, SLOT(onPortalSettingChanged(QString,QString,QDBusVariant)));

    QDBusMessage request = QDBusMessage::createMethodCall(
        QString::fromLatin1(portalService), QString::fromLatin1(portalPath),
        QString::fromLatin1(portalInterface), QStringLiteral("Read"));
    request.setArguments({ QString::fromLatin1(appearanceGroup),
                           QString::fromLatin1(colorSchemeKey) });
    const QDBusMessage reply = bus.call(request, QDBus::BlockWithGui, 700);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
        setPortalScheme(reply.arguments().first());
}

bool SystemAppearance::dark() const
{
    return m_dark;
}

void SystemAppearance::onPortalSettingChanged(const QString &group, const QString &key,
                                              const QDBusVariant &value)
{
    if (group == QString::fromLatin1(appearanceGroup)
        && key == QString::fromLatin1(colorSchemeKey))
        setPortalScheme(value.variant());
}

void SystemAppearance::setPortalScheme(const QVariant &value)
{
    bool valid = false;
    const uint scheme = unwrapVariant(value).toUInt(&valid);
    if (!valid)
        return;
    m_portalScheme = scheme == 1 || scheme == 2 ? int(scheme) : 0;
    refresh();
}

void SystemAppearance::refresh()
{
    // The portal is authoritative when it expresses a preference. Qt is the fallback.
    const bool nextDark = m_portalScheme == 1
        || (m_portalScheme == 0
            && qGuiApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    if (m_dark == nextDark)
        return;
    m_dark = nextDark;
    emit darkChanged();
}
