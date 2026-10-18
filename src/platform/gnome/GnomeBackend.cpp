#include "GnomeBackend.h"

#include "app/AppController.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QQuickWindow>
#include <QTimer>

GnomeBackend::GnomeBackend(QQuickWindow *window, AppController *controller, QObject *parent)
    : DesktopIntegration(window, controller, parent)
{
    auto *watcher = new QDBusServiceWatcher(
        QStringLiteral("org.emojikeyboard.ClipboardProbe"), QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, [this] { emit availabilityChanged(); });

    connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (!visible) {
            QTimer::singleShot(0, this, [this] {
                if (m_window && !m_window->isVisible()
                    && (!m_pastePending || m_window->property("currentView").toString()
                                              == QStringLiteral("clipboard")))
                    cancel();
            });
        }
    });

    connect(m_controller, &AppController::itemCopied, this, [this] {
        if (!m_shortcutTargetArmed) {
            if (m_pastePending)
                cancel();
            if (m_controller)
                m_controller->discardEmojiClipboardSnapshot();
            return;
        }
        m_shortcutTargetArmed = m_window
            && m_window->property("currentView").toString() == QStringLiteral("clipboard");
        m_pastePending = true;
        m_pasteDispatched = false;
        const quint64 pasteGeneration = ++m_pasteGeneration;
        const bool emoji = m_controller && m_controller->hasEmojiClipboardSnapshot();
        const quint64 generation = m_controller
            ? m_controller->emojiClipboardSnapshotGeneration() : 0;
        QTimer::singleShot(0, this, [this, emoji, generation, pasteGeneration] {
            if (pasteGeneration == m_pasteGeneration)
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

void GnomeBackend::captureClipboardPayload(const QString &kind, const QString &mimeType,
                                           const QByteArray &content)
{
    if (m_controller)
        m_controller->capturePayload(kind, mimeType, content);
}

void GnomeBackend::openView(const QString &view)
{
    const bool opening = m_window && !m_window->isVisible();
    showWindow(view);
    if (!opening)
        return;
    const auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.emojikeyboard.ClipboardProbe"),
        QStringLiteral("/org/emojikeyboard/ClipboardProbe"),
        QStringLiteral("org.emojikeyboard.ClipboardProbe"), QStringLiteral("PositionPicker"));
    message.setArguments({uint(QCoreApplication::applicationPid())});
    bus.asyncCall(message);
}

bool GnomeBackend::rememberTarget()
{
    if (!isAvailable()) {
        m_shortcutTargetArmed = false;
        reportFailure(QStringLiteral("bridge-unavailable"));
        return false;
    }
    // GNOME Shell captured the focused window before invoking ShowFromShortcut.
    m_shortcutTargetArmed = true;
    return true;
}

void GnomeBackend::cancel()
{
    ++m_pasteGeneration;
    if (m_pastePending && !m_pasteDispatched && m_controller)
        m_controller->discardEmojiClipboardSnapshot();
    m_pastePending = false;
    m_pasteDispatched = false;
    m_shortcutTargetArmed = false;
    const auto bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        const auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.emojikeyboard.ClipboardProbe"),
            QStringLiteral("/org/emojikeyboard/ClipboardProbe"),
            QStringLiteral("org.emojikeyboard.ClipboardProbe"), QStringLiteral("CancelPaste"));
        bus.asyncCall(message);
    }
}

void GnomeBackend::pasteSelection(bool emoji, quint64 snapshotGeneration)
{
    if (!m_controller || m_controller->emojiClipboardSnapshotGeneration()
        != snapshotGeneration) {
        m_pastePending = false;
        return;
    }
    const auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_pastePending = false;
        m_controller->discardEmojiClipboardSnapshot();
        reportFailure(QStringLiteral("bridge-unavailable"));
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.emojikeyboard.ClipboardProbe"),
        QStringLiteral("/org/emojikeyboard/ClipboardProbe"),
        QStringLiteral("org.emojikeyboard.ClipboardProbe"), QStringLiteral("PasteSelection"));
    message.setArguments({m_controller->pasteShortcutMode()});
    m_pasteDispatched = true;
    const quint64 pasteGeneration = m_pasteGeneration;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this, watcher, emoji, snapshotGeneration, pasteGeneration] {
        if (pasteGeneration != m_pasteGeneration) {
            watcher->deleteLater();
            return;
        }
        m_pastePending = false;
        m_pasteDispatched = false;
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
        if (m_window && !m_window->isVisible())
            cancel();
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
    m_controller->expectClipboardRestore(mimeType, content);

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
