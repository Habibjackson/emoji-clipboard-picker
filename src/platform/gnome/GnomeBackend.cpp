#include "GnomeBackend.h"

#include "app/AppController.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QTimer>

GnomeBackend::GnomeBackend(QQuickWindow *window, AppController *controller, QObject *parent)
    : DesktopIntegration(window, controller, parent)
{
    connect(m_controller, &AppController::itemCopied, this, [this] {
        if (!m_shortcutTargetArmed) {
            if (m_controller)
                m_controller->discardEmojiClipboardSnapshot();
            return;
        }
        m_shortcutTargetArmed = false;
        const bool emoji = m_controller && m_controller->hasEmojiClipboardSnapshot();
        const quint64 generation = m_controller
            ? m_controller->emojiClipboardSnapshotGeneration() : 0;
        QTimer::singleShot(0, this, [this, emoji, generation] {
            pasteSelection(emoji, generation);
        });
    });
}

bool GnomeBackend::isAvailable() const
{
    const auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return false;
    const QDBusReply<bool> registered = bus.interface()->isServiceRegistered(
        QStringLiteral("org.emojikeyboard.ClipboardProbe"));
    return registered.isValid() && registered.value();
}

QString GnomeBackend::errorString() const
{
    if (!QDBusConnection::sessionBus().isConnected())
        return tr("A session D-Bus is required for GNOME integration");
    return isAvailable() ? QString() : tr("The GNOME Shell extension is not active");
}

void GnomeBackend::captureClipboard(const QString &kind)
{
    if (m_controller && (kind == QStringLiteral("text") || kind == QStringLiteral("image")))
        m_controller->captureNow();
}

void GnomeBackend::openView(const QString &view)
{
    showWindow(view);
}

bool GnomeBackend::rememberTarget()
{
    // GNOME Shell captured the focused window before invoking ShowFromShortcut.
    m_shortcutTargetArmed = true;
    return true;
}

void GnomeBackend::cancel()
{
    m_shortcutTargetArmed = false;
}

void GnomeBackend::pasteSelection(bool emoji, quint64 snapshotGeneration)
{
    if (!m_controller || m_controller->emojiClipboardSnapshotGeneration()
        != snapshotGeneration) {
        return;
    }
    const auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_controller->discardEmojiClipboardSnapshot();
        reportFailure(QStringLiteral("bridge-unavailable"));
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.emojikeyboard.ClipboardProbe"),
        QStringLiteral("/org/emojikeyboard/ClipboardProbe"),
        QStringLiteral("org.emojikeyboard.ClipboardProbe"), QStringLiteral("PasteSelection"));
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this, watcher, emoji, snapshotGeneration] {
        const QDBusPendingReply<QString, QString, uint> reply = *watcher;
        if (m_controller && m_controller->emojiClipboardSnapshotGeneration()
            == snapshotGeneration) {
            if (reply.isError()) {
                m_controller->discardEmojiClipboardSnapshot();
                reportFailure(QStringLiteral("bridge-unavailable"));
            } else {
                m_controller->reportPasteResult(reply.argumentAt<0>(), reply.argumentAt<1>());
                if (reply.argumentAt<0>() == QStringLiteral("sent") && emoji) {
                    if (reply.argumentAt<2>() != 0) {
                        const uint revision = reply.argumentAt<2>();
                        QTimer::singleShot(750, this, [this, snapshotGeneration, revision] {
                            restoreClipboard(snapshotGeneration, revision);
                        });
                    } else {
                        m_controller->discardEmojiClipboardSnapshot();
                        m_controller->reportClipboardRestoreResult(
                            AppController::ClipboardRestoreResult::Unavailable);
                    }
                } else {
                    m_controller->discardEmojiClipboardSnapshot();
                }
            }
        }
        watcher->deleteLater();
    });
}

void GnomeBackend::restoreClipboard(quint64 snapshotGeneration, uint clipboardRevision)
{
    if (!m_controller || m_controller->emojiClipboardSnapshotGeneration()
        != snapshotGeneration) {
        return;
    }

    QString temporaryText;
    QString mimeType;
    QByteArray content;
    if (!m_controller->emojiClipboardSnapshot(&temporaryText, &mimeType, &content)) {
        m_controller->discardEmojiClipboardSnapshot();
        m_controller->reportClipboardRestoreResult(
            AppController::ClipboardRestoreResult::Unavailable);
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.emojikeyboard.ClipboardProbe"),
        QStringLiteral("/org/emojikeyboard/ClipboardProbe"),
        QStringLiteral("org.emojikeyboard.ClipboardProbe"),
        QStringLiteral("RestoreClipboardIfCurrent"));
    message.setArguments({QVariant::fromValue(clipboardRevision), temporaryText,
                          mimeType, content});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message),
                                                this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, snapshotGeneration] {
        const QDBusPendingReply<QString> reply = *watcher;
        if (m_controller && m_controller->emojiClipboardSnapshotGeneration()
            == snapshotGeneration) {
            const auto result = reply.isError()
                ? AppController::ClipboardRestoreResult::Unavailable
                : reply.value() == QStringLiteral("restored")
                    ? AppController::ClipboardRestoreResult::Restored
                    : reply.value() == QStringLiteral("changed")
                        ? AppController::ClipboardRestoreResult::Changed
                        : AppController::ClipboardRestoreResult::Unavailable;
            m_controller->discardEmojiClipboardSnapshot();
            m_controller->reportClipboardRestoreResult(result);
        }
        watcher->deleteLater();
    });
}
