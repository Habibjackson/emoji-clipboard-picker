#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

class AppController;
class QQuickWindow;

// The picker and D-Bus service use this contract regardless of the desktop session.
// A backend owns shortcut target state and decides how to focus and paste there.
class DesktopIntegration : public QObject
{
    Q_OBJECT

public:
    DesktopIntegration(QQuickWindow *window, AppController *controller,
                       QObject *parent = nullptr);
    ~DesktopIntegration() override = default;

    virtual bool isAvailable() const = 0;
    virtual QString errorString() const = 0;
    virtual void captureClipboard(const QString &kind) = 0;
    virtual void captureClipboardPayload(const QString &kind, const QString &mimeType,
                                         const QByteArray &content);
    virtual void openView(const QString &view) = 0;
    virtual bool rememberTarget() = 0;
    virtual void pasteSelection(bool emoji, quint64 snapshotGeneration) = 0;
    virtual void cancel() = 0;

    void reportFailure(const QString &reason);

signals:
    void availabilityChanged();

protected:
    void showWindow(const QString &view);

    QPointer<QQuickWindow> m_window;
    QPointer<AppController> m_controller;
};

// Keeps manual copy usable when a native integration cannot start.
class CopyOnlyBackend final : public DesktopIntegration
{
public:
    CopyOnlyBackend(QQuickWindow *window, AppController *controller, QString reason,
                    QObject *parent = nullptr);

    bool isAvailable() const override;
    QString errorString() const override;
    void captureClipboard(const QString &kind) override;
    void openView(const QString &view) override;
    bool rememberTarget() override;
    void pasteSelection(bool emoji, quint64 snapshotGeneration) override;
    void cancel() override;

private:
    QString m_reason;
};
