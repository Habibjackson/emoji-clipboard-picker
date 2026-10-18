#pragma once

#include <QString>
#include <Qt>

#include <optional>

struct ShortcutSpec
{
    QString accelerator;
    QString label;
    QString keyName;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;

    bool isValid() const { return !accelerator.isEmpty(); }

    static std::optional<ShortcutSpec> fromKey(int key, Qt::KeyboardModifiers modifiers);
    static std::optional<ShortcutSpec> fromAccelerator(const QString &accelerator);
    static ShortcutSpec emojiDefault();
    static ShortcutSpec clipboardDefault();
};
