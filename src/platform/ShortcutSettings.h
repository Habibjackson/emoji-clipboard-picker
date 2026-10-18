#pragma once

#include "platform/ShortcutSpec.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <functional>

class ShortcutSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by main.cpp")
    Q_PROPERTY(QString emojiLabel READ emojiLabel NOTIFY changed)
    Q_PROPERTY(QString clipboardLabel READ clipboardLabel NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)

public:
    explicit ShortcutSettings(QObject *parent = nullptr);

    QString emojiLabel() const;
    QString clipboardLabel() const;
    QString errorMessage() const;
    bool available() const;
    ShortcutSpec spec(const QString &view) const;
    void setX11Apply(std::function<bool(const QString &, const ShortcutSpec &)> apply);

    Q_INVOKABLE bool assignKey(const QString &view, int key, int modifiers);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void clearError();

signals:
    void changed();

private:
    enum class Backend { Unavailable, Gnome, X11 };
    bool readGnome(const QString &view, ShortcutSpec *spec);
    bool writeGnome(const QString &view, const ShortcutSpec &spec);
    void setError(const QString &error);
    QString schemaDirectory() const;

    Backend m_backend = Backend::Unavailable;
    ShortcutSpec m_emoji = ShortcutSpec::emojiDefault();
    ShortcutSpec m_clipboard = ShortcutSpec::clipboardDefault();
    QString m_error;
    std::function<bool(const QString &, const ShortcutSpec &)> m_x11Apply;
};
