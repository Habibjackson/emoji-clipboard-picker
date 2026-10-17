#include "PickerService.h"

#include "app/AppController.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

PickerService::PickerService(QQuickWindow *window, AppController *controller, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_controller(controller)
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

void PickerService::ShowEmoji()
{
    m_shortcutTargetArmed = false;
    showView(QStringLiteral("emoji"));
}

void PickerService::ShowClipboard()
{
    m_shortcutTargetArmed = false;
    showView(QStringLiteral("clipboard"));
}

uint PickerService::ShowFromShortcut(const QString &view)
{
    if (view != QStringLiteral("emoji") && view != QStringLiteral("clipboard"))
        return 0;
    m_shortcutTargetArmed = true;
    showView(view);
    return uint(QCoreApplication::applicationPid());
}

void PickerService::NotifyClipboardChanged(const QString &kind)
{
    if (m_controller && (kind == QStringLiteral("text") || kind == QStringLiteral("image")))
        m_controller->captureNow();
}

void PickerService::Hide()
{
    m_shortcutTargetArmed = false;
    if (m_window)
        m_window->hide();
}

void PickerService::Quit()
{
    QCoreApplication::exit(0);
}

QString PickerService::CurrentView() const
{
    return m_window ? m_window->property("currentView").toString() : QString();
}

QString PickerService::CurrentStatus() const
{
    return m_controller ? m_controller->status() : QString();
}

bool PickerService::IsVisible() const
{
    return m_window && m_window->isVisible();
}

void PickerService::pasteSelection(bool emoji, quint64 snapshotGeneration)
{
    if (!m_controller || m_controller->emojiClipboardSnapshotGeneration()
        != snapshotGeneration) {
        return;
    }
    const auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_controller->discardEmojiClipboardSnapshot();
        m_controller->reportPasteResult(QStringLiteral("bridge-unavailable"), QString());
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
                m_controller->reportPasteResult(QStringLiteral("bridge-unavailable"), QString());
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

void PickerService::restoreClipboard(quint64 snapshotGeneration, uint clipboardRevision)
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

void PickerService::showView(const QString &view)
{
    if (!m_window)
        return;
    m_window->setProperty("currentView", view);
    m_window->show();
    if (QGuiApplication::platformName() == QStringLiteral("xcb"))
        m_window->raise();
    m_window->requestActivate();
}
