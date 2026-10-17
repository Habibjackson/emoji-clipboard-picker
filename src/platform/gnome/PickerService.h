#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

class AppController;
class DesktopIntegration;
class QQuickWindow;

// Stable D-Bus surface used by the GNOME extension and repeated app launches.
class PickerService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.emojikeyboard.App")

public:
    PickerService(QQuickWindow *window, AppController *controller,
                  DesktopIntegration *backend, QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void ShowEmoji();
    Q_SCRIPTABLE void ShowClipboard();
    Q_SCRIPTABLE uint ShowFromShortcut(const QString &view, bool pickerFocused);
    Q_SCRIPTABLE void NotifyClipboardChanged(const QString &kind);
    Q_SCRIPTABLE void NotifyClipboardPayload(const QString &kind, const QString &mimeType,
                                             const QByteArray &content);
    Q_SCRIPTABLE void Hide();
    Q_SCRIPTABLE void Quit();
    Q_SCRIPTABLE QString CurrentView() const;
    Q_SCRIPTABLE QString CurrentStatus() const;
    Q_SCRIPTABLE bool IsVisible() const;

private:
    QPointer<QQuickWindow> m_window;
    QPointer<AppController> m_controller;
    QPointer<DesktopIntegration> m_backend;
};
