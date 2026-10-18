#pragma once

#include <QDBusVariant>
#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class SystemAppearance : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by main.cpp")
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)

public:
    explicit SystemAppearance(QObject *parent = nullptr);

    bool dark() const;

signals:
    void darkChanged();

private slots:
    void onPortalSettingChanged(const QString &group, const QString &key,
                                const QDBusVariant &value);

private:
    void setPortalScheme(const QVariant &value);
    void refresh();

    int m_portalScheme = 0;
    bool m_dark = false;
};
