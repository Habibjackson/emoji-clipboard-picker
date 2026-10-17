#pragma once

#include "platform/DesktopIntegration.h"

class GnomeBackend final : public DesktopIntegration
{
    Q_OBJECT

public:
    GnomeBackend(QQuickWindow *window, AppController *controller, QObject *parent = nullptr);

    bool isAvailable() const override;
    QString errorString() const override;
    void captureClipboard(const QString &kind) override;
    void captureClipboardPayload(const QString &kind, const QString &mimeType,
                                 const QByteArray &content) override;
    void openView(const QString &view) override;
    bool rememberTarget() override;
    void pasteSelection(bool emoji, quint64 snapshotGeneration) override;
    void cancel() override;

private:
    void restoreClipboard(quint64 snapshotGeneration, uint clipboardRevision);

    bool m_shortcutTargetArmed = false;
    bool m_pastePending = false;
    bool m_pasteDispatched = false;
    quint64 m_pasteGeneration = 0;
};
