#pragma once

#include "models/ClipboardModel.h"
#include "models/EmojiModel.h"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class QClipboard;
class HistoryStore;

class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by main.cpp")
    Q_PROPERTY(EmojiModel *emojiModel READ emojiModel CONSTANT)
    Q_PROPERTY(ClipboardModel *clipboardModel READ clipboardModel CONSTANT)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString initialView READ initialView CONSTANT)
    Q_PROPERTY(bool backgroundCaptureLimited READ backgroundCaptureLimited CONSTANT)

public:
    enum class ClipboardRestoreResult { Restored, Unavailable, Changed };

    explicit AppController(HistoryStore *store, QString initialView, QObject *parent = nullptr);

    EmojiModel *emojiModel();
    ClipboardModel *clipboardModel();
    bool paused() const;
    void setPaused(bool paused);
    QString status() const;
    QString initialView() const;
    bool backgroundCaptureLimited() const;

    Q_INVOKABLE void captureNow();
    Q_INVOKABLE void copyEmoji(const QString &symbol);
    Q_INVOKABLE void copyEntry(qint64 id);
    Q_INVOKABLE void pinEntry(qint64 id, bool pinned);
    Q_INVOKABLE void deleteEntry(qint64 id);
    Q_INVOKABLE void clearRecent();
    void reportPasteResult(const QString &result, const QString &shortcut);
    void reportShortcutFailure(const QString &shortcut);
    void reportClipboardRestoreResult(ClipboardRestoreResult result);
    bool hasEmojiClipboardSnapshot() const;
    quint64 emojiClipboardSnapshotGeneration() const;
    bool emojiClipboardSnapshot(QString *temporaryText, QString *mimeType,
                                QByteArray *content) const;
    void discardEmojiClipboardSnapshot();

signals:
    void itemCopied();
    void pausedChanged();
    void statusChanged();

private:
    void captureCurrentClipboard();
    void snapshotClipboardBeforeEmoji(const QString &symbol);
    void setStatus(const QString &message);
    bool isOwnClipboardContent(const QString &kind, const QByteArray &content);
    static QByteArray contentHash(const QString &kind, const QByteArray &content);

    HistoryStore *m_store;
    QClipboard *m_clipboard;
    EmojiModel m_emojiModel;
    ClipboardModel m_clipboardModel;
    QString m_initialView;
    QString m_status;
    QByteArray m_ownContentHash;
    enum class SnapshotKind { None, Text, Image };
    SnapshotKind m_snapshotKind = SnapshotKind::None;
    QString m_snapshotText;
    QImage m_snapshotImage;
    QString m_temporaryEmoji;
    quint64 m_snapshotGeneration = 0;
    bool m_paused = false;
};
