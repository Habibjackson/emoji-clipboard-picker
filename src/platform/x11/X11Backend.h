#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

struct _XDisplay;
class AppController;
class QQuickWindow;
class QSocketNotifier;

class X11Backend : public QObject
{
    Q_OBJECT

public:
    explicit X11Backend(QQuickWindow *window, AppController *controller,
                        QObject *parent = nullptr);
    ~X11Backend() override;

    bool isAvailable() const;
    QString errorString() const;
    quint64 clipboardEventCount() const;
    void disarm();

signals:
    void clipboardOwnerChanged();
    void shortcutTriggered(const QString &view);
    void pasteSent(const QString &shortcut);

private:
    void processEvents();
    void openView(const QString &view);
    void pasteSelection();
    bool grabShortcut(unsigned int keycode);
    void releaseShortcut(unsigned int keycode);
    bool windowExists(unsigned long window) const;
    bool targetIsTerminal(unsigned long window) const;
    unsigned int numLockMask() const;

    struct _XDisplay *m_display = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    QPointer<QQuickWindow> m_window;
    QPointer<AppController> m_controller;
    QString m_error;
    unsigned long m_root = 0;
    unsigned long m_target = 0;
    unsigned int m_emojiKeycode = 0;
    unsigned int m_clipboardKeycode = 0;
    unsigned int m_numLockMask = 0;
    int m_xfixesEventBase = 0;
    quint64 m_clipboardEvents = 0;
    bool m_emojiGrabbed = false;
    bool m_clipboardGrabbed = false;
    bool m_shortcutArmed = false;
    bool m_pastePending = false;
};
