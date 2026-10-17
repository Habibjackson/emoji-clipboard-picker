#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QQuickWindow;
class AppController;

class PickerService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.emojikeyboard.App")

public:
    explicit PickerService(QQuickWindow *window, AppController *controller,
                           QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void ShowEmoji();
    Q_SCRIPTABLE void ShowClipboard();
    Q_SCRIPTABLE uint ShowFromShortcut(const QString &view);
    Q_SCRIPTABLE void NotifyClipboardChanged(const QString &kind);
    Q_SCRIPTABLE void Hide();
    Q_SCRIPTABLE void Quit();
    Q_SCRIPTABLE QString CurrentView() const;
    Q_SCRIPTABLE QString CurrentStatus() const;
    Q_SCRIPTABLE bool IsVisible() const;

private:
    void showView(const QString &view);
    void pasteSelection(bool emoji, quint64 snapshotGeneration);
    void restoreClipboard(quint64 snapshotGeneration, uint clipboardRevision);

    QPointer<QQuickWindow> m_window;
    QPointer<AppController> m_controller;
    bool m_shortcutTargetArmed = false;
};
