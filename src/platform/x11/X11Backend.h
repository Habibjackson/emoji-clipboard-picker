#pragma once

#include "platform/DesktopIntegration.h"

#include <QString>

struct _XDisplay;
class AppController;
class QQuickWindow;
class QSocketNotifier;
class QTimer;

class X11Backend : public DesktopIntegration
{
    Q_OBJECT

public:
    explicit X11Backend(QQuickWindow *window, AppController *controller,
                        QObject *parent = nullptr);
    ~X11Backend() override;

    bool isAvailable() const override;
    QString errorString() const override;
    quint64 clipboardEventCount() const;
    void captureClipboard(const QString &kind) override;
    void openView(const QString &view) override;
    bool rememberTarget() override;
    void pasteSelection(bool emoji, quint64 snapshotGeneration) override;
    void cancel() override;

signals:
    void clipboardOwnerChanged();
    void shortcutTriggered(const QString &view);
    void pasteSent(const QString &shortcut);

private:
    void processEvents();
    void activateShortcutView(const QString &view);
    bool grabShortcut(unsigned int keycode);
    void releaseShortcut(unsigned int keycode);
    bool windowExists(unsigned long window) const;
    bool targetHasFocus(unsigned long target) const;
    bool targetIsTerminal(unsigned long window) const;
    bool hasEwmhWindowManager() const;
    void sendPaste(unsigned long target, bool emoji, quint64 snapshotGeneration,
                   const QString &shortcut);
    void restoreClipboard(quint64 snapshotGeneration, unsigned long temporaryOwner,
                          quint64 clipboardRevision);
    unsigned int numLockMask() const;

    struct _XDisplay *m_display = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    QString m_error;
    unsigned long m_root = 0;
    unsigned long m_target = 0;
    unsigned long m_clipboardSelection = 0;
    unsigned long m_lastShortcutTime = 0;
    unsigned long m_selectedClipboardOwner = 0;
    quint64 m_selectedClipboardRevision = 0;
    unsigned int m_emojiKeycode = 0;
    unsigned int m_clipboardKeycode = 0;
    unsigned int m_numLockMask = 0;
    int m_xfixesEventBase = 0;
    quint64 m_clipboardEvents = 0;
    bool m_emojiGrabbed = false;
    bool m_clipboardGrabbed = false;
    bool m_shortcutArmed = false;
    bool m_pastePending = false;
    quint64 m_pasteGeneration = 0;
    QTimer *m_focusTimer = nullptr;
};
