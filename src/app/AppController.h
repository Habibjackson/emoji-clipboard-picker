#pragma once

#include "models/ClipboardModel.h"
#include "models/EmojiModel.h"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QClipboard;
class QMimeData;
class HistoryStore;

class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by main.cpp")
    Q_PROPERTY(EmojiModel *emojiModel READ emojiModel CONSTANT)
    Q_PROPERTY(ClipboardModel *clipboardModel READ clipboardModel CONSTANT)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(QString pasteShortcutMode READ pasteShortcutMode WRITE setPasteShortcutMode
                   NOTIFY pasteShortcutModeChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString attention READ attention NOTIFY attentionChanged)
    Q_PROPERTY(QString integrationWarning READ integrationWarning NOTIFY integrationWarningChanged)
    Q_PROPERTY(bool copyOnly READ copyOnly NOTIFY integrationWarningChanged)
    Q_PROPERTY(QString initialView READ initialView CONSTANT)
    Q_PROPERTY(bool backgroundCaptureLimited READ backgroundCaptureLimited
                   NOTIFY integrationWarningChanged)

public:
    enum class ClipboardRestoreResult { Restored, Unavailable, Changed };

    explicit AppController(HistoryStore *store, QString initialView, QObject *parent = nullptr);

    EmojiModel *emojiModel();
    ClipboardModel *clipboardModel();
    bool paused() const;
    void setPaused(bool paused);
    QString pasteShortcutMode() const;
    void setPasteShortcutMode(const QString &mode);
    QString pasteShortcutFor(bool terminal) const;
    QString status() const;
    QString attention() const;
    QString integrationWarning() const;
    bool copyOnly() const;
    QString initialView() const;
    bool backgroundCaptureLimited() const;

    Q_INVOKABLE void captureNow();
    void capturePayload(const QString &kind, const QString &mimeType, const QByteArray &content);
    Q_INVOKABLE void copyEmoji(const QString &symbol);
    Q_INVOKABLE void copyEntry(qint64 id);
    Q_INVOKABLE void pinEntry(qint64 id, bool pinned);
    Q_INVOKABLE void deleteEntry(qint64 id);
    Q_INVOKABLE void clearRecent();
    Q_INVOKABLE void clearAll();
    void reportPasteResult(const QString &result, const QString &shortcut);
    void reportShortcutFailure(const QString &shortcut);
    void setIntegrationAvailability(bool available, const QString &reason);
    void reportClipboardRestoreResult(ClipboardRestoreResult result);
    bool hasEmojiClipboardSnapshot() const;
    quint64 emojiClipboardSnapshotGeneration() const;
    bool emojiClipboardSnapshot(QString *temporaryText, QString *mimeType,
                                QByteArray *content) const;
    void expectClipboardRestore(const QString &mimeType, const QByteArray &content);
    void discardEmojiClipboardSnapshot();

signals:
    void itemCopied();
    void pausedChanged();
    void pasteShortcutModeChanged();
    void statusChanged();
    void attentionChanged();
    void integrationWarningChanged();

private:
    void captureCurrentClipboard();
    void captureMimeData(const QMimeData *mime);
    void snapshotClipboardBeforeEmoji(const QString &symbol);
    void setStatus(const QString &message);
    void setAttention(const QString &message);
    void refreshIntegrationWarning();
    bool isOwnClipboardContent(const QString &kind, const QByteArray &content);
    static QByteArray contentHash(const QString &kind, const QByteArray &content);

    HistoryStore *m_store;
    QClipboard *m_clipboard;
    EmojiModel m_emojiModel;
    ClipboardModel m_clipboardModel;
    QString m_initialView;
    QString m_status;
    QString m_attention;
    QString m_backendFailure;
    QStringList m_shortcutFailures;
    QString m_integrationWarning;
    bool m_copyOnly = false;
    QByteArray m_ownContentHash;
    QByteArray m_expectedRestoreHash;
    qint64 m_restoreSuppressionExpiresAt = 0;
    bool m_restoreObserved = false;
    enum class SnapshotKind { None, Text, Image };
    SnapshotKind m_snapshotKind = SnapshotKind::None;
    QString m_snapshotText;
    QImage m_snapshotImage;
    QString m_temporaryEmoji;
    quint64 m_snapshotGeneration = 0;
    bool m_paused = false;
    QString m_pasteShortcutMode;
};
