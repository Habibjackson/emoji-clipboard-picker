#include "ShortcutSpec.h"

#include <QChar>
#include <QStringList>

namespace {
struct NamedKey {
    int key;
    const char *name;
    const char *label;
};

constexpr NamedKey namedKeys[] = {
    {Qt::Key_Period, "period", "."},
    {Qt::Key_Comma, "comma", ","},
    {Qt::Key_Semicolon, "semicolon", ";"},
    {Qt::Key_Slash, "slash", "/"},
    {Qt::Key_Minus, "minus", "-"},
    {Qt::Key_Equal, "equal", "="},
    {Qt::Key_BracketLeft, "bracketleft", "["},
    {Qt::Key_BracketRight, "bracketright", "]"},
    {Qt::Key_Backslash, "backslash", "\\"},
};

std::optional<NamedKey> keyByQt(int key)
{
    for (const auto &item : namedKeys) {
        if (item.key == key)
            return item;
    }
    return std::nullopt;
}

std::optional<int> keyByName(const QString &name)
{
    for (const auto &item : namedKeys) {
        if (name.compare(QLatin1String(item.name), Qt::CaseInsensitive) == 0)
            return item.key;
    }
    if (name.size() == 1) {
        const QChar letter = name.at(0).toUpper();
        if (letter >= QLatin1Char('A') && letter <= QLatin1Char('Z'))
            return Qt::Key_A + letter.unicode() - 'A';
        if (letter >= QLatin1Char('0') && letter <= QLatin1Char('9'))
            return Qt::Key_0 + letter.unicode() - '0';
    }
    if (name.size() >= 2 && name.at(0).toUpper() == QLatin1Char('F')) {
        bool ok = false;
        const int function = name.mid(1).toInt(&ok);
        if (ok && function >= 1 && function <= 12)
            return Qt::Key_F1 + function - 1;
    }
    return std::nullopt;
}
}

std::optional<ShortcutSpec> ShortcutSpec::fromKey(int key, Qt::KeyboardModifiers modifiers)
{
    constexpr auto allowed = Qt::MetaModifier | Qt::ControlModifier
        | Qt::AltModifier | Qt::ShiftModifier;
    if (modifiers & ~allowed)
        return std::nullopt;
    if (!(modifiers & Qt::MetaModifier)
        && (modifiers & (Qt::ControlModifier | Qt::AltModifier))
            != (Qt::ControlModifier | Qt::AltModifier))
        return std::nullopt;

    QString keyName;
    QString keyLabel;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        const QChar letter = QLatin1Char('a' + key - Qt::Key_A);
        keyName = letter;
        keyLabel = letter.toUpper();
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        keyName = QLatin1Char('0' + key - Qt::Key_0);
        keyLabel = keyName;
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        keyName = QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);
        keyLabel = keyName;
    } else if (const auto named = keyByQt(key)) {
        keyName = QLatin1String(named->name);
        keyLabel = QLatin1String(named->label);
    } else if (modifiers & Qt::ShiftModifier) {
        const QString shifted = QStringLiteral("><:?_+{}|");
        const QString base = QStringLiteral(".,;/-=[]\\");
        const int index = shifted.indexOf(QChar(key));
        if (index < 0)
            return std::nullopt;
        return fromKey(base.at(index).unicode(), modifiers);
    } else {
        return std::nullopt;
    }

    QString accelerator;
    QStringList labels;
    if (modifiers & Qt::MetaModifier) {
        accelerator += QStringLiteral("<Super>");
        labels.append(QStringLiteral("Super"));
    }
    if (modifiers & Qt::ControlModifier) {
        accelerator += QStringLiteral("<Control>");
        labels.append(QStringLiteral("Ctrl"));
    }
    if (modifiers & Qt::AltModifier) {
        accelerator += QStringLiteral("<Alt>");
        labels.append(QStringLiteral("Alt"));
    }
    if (modifiers & Qt::ShiftModifier) {
        accelerator += QStringLiteral("<Shift>");
        labels.append(QStringLiteral("Shift"));
    }
    labels.append(keyLabel);
    return ShortcutSpec{accelerator + keyName, labels.join(QLatin1Char('+')),
                        keyName, modifiers};
}

std::optional<ShortcutSpec> ShortcutSpec::fromAccelerator(const QString &accelerator)
{
    QString remaining = accelerator.trimmed();
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    while (remaining.startsWith(QLatin1Char('<'))) {
        const int end = remaining.indexOf(QLatin1Char('>'));
        if (end < 2)
            return std::nullopt;
        const QString name = remaining.mid(1, end - 1).toLower();
        Qt::KeyboardModifier modifier;
        if (name == QStringLiteral("super") || name == QStringLiteral("mod4"))
            modifier = Qt::MetaModifier;
        else if (name == QStringLiteral("control") || name == QStringLiteral("ctrl")
                 || name == QStringLiteral("ctl"))
            modifier = Qt::ControlModifier;
        else if (name == QStringLiteral("alt"))
            modifier = Qt::AltModifier;
        else if (name == QStringLiteral("shift"))
            modifier = Qt::ShiftModifier;
        else
            return std::nullopt;
        if (modifiers & modifier)
            return std::nullopt;
        modifiers |= modifier;
        remaining.remove(0, end + 1);
    }
    const auto key = keyByName(remaining);
    return key ? fromKey(*key, modifiers) : std::nullopt;
}

ShortcutSpec ShortcutSpec::emojiDefault()
{
    return *fromKey(Qt::Key_Period, Qt::MetaModifier);
}

ShortcutSpec ShortcutSpec::clipboardDefault()
{
    return *fromKey(Qt::Key_V, Qt::MetaModifier);
}
