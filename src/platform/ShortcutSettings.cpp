#include "ShortcutSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include <utility>

namespace {
constexpr auto schema = "org.gnome.shell.extensions.emoji-keyboard";

QString settingsKey(const QString &view)
{
    return view == QStringLiteral("emoji") ? QStringLiteral("shortcuts/emoji")
                                            : QStringLiteral("shortcuts/clipboard");
}

QString schemaKey(const QString &view)
{
    return view == QStringLiteral("emoji") ? QStringLiteral("show-emoji")
                                            : QStringLiteral("show-clipboard");
}

bool runGsettings(const QStringList &arguments, QString *output)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("GSETTINGS_SCHEMA_DIR"));
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral("gsettings"), arguments);
    if (!process.waitForStarted(1500) || !process.waitForFinished(1500)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return false;
    if (output)
        *output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return true;
}
}

ShortcutSettings::ShortcutSettings(QObject *parent)
    : QObject(parent)
{
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        m_backend = Backend::X11;
    } else if (qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'))
                   .contains(QStringLiteral("GNOME"), Qt::CaseInsensitive)
               && QFileInfo::exists(schemaDirectory() + QStringLiteral("/gschemas.compiled"))) {
        m_backend = Backend::Gnome;
    }
    reload();
}

QString ShortcutSettings::emojiLabel() const { return m_emoji.label; }
QString ShortcutSettings::clipboardLabel() const { return m_clipboard.label; }
QString ShortcutSettings::errorMessage() const { return m_error; }
bool ShortcutSettings::available() const
{
    return m_backend == Backend::Gnome
        || (m_backend == Backend::X11 && bool(m_x11Apply));
}

ShortcutSpec ShortcutSettings::spec(const QString &view) const
{
    return view == QStringLiteral("clipboard") ? m_clipboard : m_emoji;
}

void ShortcutSettings::setX11Apply(
    std::function<bool(const QString &, const ShortcutSpec &)> apply)
{
    m_x11Apply = std::move(apply);
    emit changed();
}

bool ShortcutSettings::assignKey(const QString &view, int key, int modifiers)
{
    if (view != QStringLiteral("emoji") && view != QStringLiteral("clipboard"))
        return false;
    if (!available()) {
        setError(tr("Shortcut editing requires GNOME Wayland or X11 integration."));
        return false;
    }
    const auto candidate = ShortcutSpec::fromKey(key, Qt::KeyboardModifiers(modifiers));
    if (!candidate) {
        setError(tr("Use Super+key or Ctrl+Alt+key. Letters, digits, F1–F12, and common punctuation are supported."));
        return false;
    }
    const ShortcutSpec other = spec(view == QStringLiteral("emoji")
                                        ? QStringLiteral("clipboard") : QStringLiteral("emoji"));
    if (candidate->accelerator == other.accelerator) {
        setError(tr("Emoji and clipboard need different shortcuts."));
        return false;
    }
    if (candidate->accelerator == spec(view).accelerator) {
        if (m_backend == Backend::X11 && !m_x11Apply(view, *candidate)) {
            setError(tr("This shortcut is unavailable. Another application may already use it."));
            return false;
        }
        clearError();
        return true;
    }
    if (m_backend == Backend::Gnome && !writeGnome(view, *candidate)) {
        setError(tr("Could not save the GNOME shortcut. Check the extension and session settings."));
        return false;
    }
    if (m_backend == Backend::X11) {
        if (!m_x11Apply(view, *candidate)) {
            setError(tr("This shortcut is unavailable. Another application may already use it."));
            return false;
        }
        QSettings settings;
        settings.setValue(settingsKey(view), candidate->accelerator);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            m_x11Apply(view, spec(view));
            setError(tr("Could not save the shortcut to local settings."));
            return false;
        }
    }
    (view == QStringLiteral("emoji") ? m_emoji : m_clipboard) = *candidate;
    m_error.clear();
    emit changed();
    return true;
}

void ShortcutSettings::reload()
{
    if (m_backend == Backend::X11) {
        // Once X11 is running, its live grabs are authoritative. Reading a changed
        // settings file here would show a key that has not actually been grabbed.
        if (m_x11Apply) {
            emit changed();
            return;
        }
        const auto emoji = ShortcutSpec::fromAccelerator(
            QSettings().value(settingsKey(QStringLiteral("emoji")),
                              m_emoji.accelerator).toString());
        const auto clipboard = ShortcutSpec::fromAccelerator(
            QSettings().value(settingsKey(QStringLiteral("clipboard")),
                              m_clipboard.accelerator).toString());
        if (emoji)
            m_emoji = *emoji;
        if (clipboard)
            m_clipboard = *clipboard;
    } else if (m_backend == Backend::Gnome) {
        ShortcutSpec emoji;
        ShortcutSpec clipboard;
        if (readGnome(QStringLiteral("emoji"), &emoji))
            m_emoji = emoji;
        if (readGnome(QStringLiteral("clipboard"), &clipboard))
            m_clipboard = clipboard;
    }
    emit changed();
}

void ShortcutSettings::clearError()
{
    setError(QString());
}

void ShortcutSettings::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit changed();
}

QString ShortcutSettings::schemaDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/gnome-shell/extensions/emoji-keyboard@emojikeyboard.local/schemas");
}

bool ShortcutSettings::readGnome(const QString &view, ShortcutSpec *spec)
{
    QString output;
    if (!runGsettings({QStringLiteral("--schemadir"), schemaDirectory(),
                       QStringLiteral("get"), QString::fromLatin1(schema), schemaKey(view)},
                      &output))
        return false;
    static const QRegularExpression firstString(QStringLiteral(R"(['"]([^'"]+)['"])"));
    const auto match = firstString.match(output);
    if (!match.hasMatch()) {
        spec->label = tr("Not set");
        return true;
    }
    const auto parsed = ShortcutSpec::fromAccelerator(match.captured(1));
    if (parsed)
        *spec = *parsed;
    else {
        spec->accelerator = match.captured(1);
        spec->label = match.captured(1);
    }
    return true;
}

bool ShortcutSettings::writeGnome(const QString &view, const ShortcutSpec &spec)
{
    const QString value = QStringLiteral("['%1']").arg(spec.accelerator);
    return runGsettings({QStringLiteral("--schemadir"), schemaDirectory(),
                         QStringLiteral("set"), QString::fromLatin1(schema),
                         schemaKey(view), value}, nullptr);
}
