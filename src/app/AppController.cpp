#include "AppController.h"

#include "storage/HistoryStore.h"

#include <QBuffer>
#include <QClipboard>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QTimer>

#include <utility>

AppController::AppController(HistoryStore *store, QString initialView, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_clipboard(QGuiApplication::clipboard())
    , m_emojiModel(this)
    , m_clipboardModel(store, this)
    , m_initialView(std::move(initialView))
{
    if (!m_emojiModel.load(QStringLiteral(":/data/emoji-test.txt")))
        setStatus(tr("Emoji catalog could not be loaded"));
    m_emojiModel.setRecent(m_store->recentEmoji());
    if (!m_store->isOpen())
        setStatus(tr("History is unavailable: %1").arg(m_store->errorString()));

    connect(m_clipboard, &QClipboard::dataChanged, this, &AppController::captureCurrentClipboard);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            QTimer::singleShot(0, this, &AppController::captureCurrentClipboard);
    });
    QTimer::singleShot(0, this, &AppController::captureCurrentClipboard);
}

EmojiModel *AppController::emojiModel()
{
    return &m_emojiModel;
}

ClipboardModel *AppController::clipboardModel()
{
    return &m_clipboardModel;
}

bool AppController::paused() const
{
    return m_paused;
}

void AppController::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    emit pausedChanged();
    setStatus(paused ? tr("Clipboard capture paused") : tr("Clipboard capture resumed"));
    if (!paused)
        captureCurrentClipboard();
}

QString AppController::status() const
{
    return m_status;
}

QString AppController::integrationWarning() const
{
    return m_integrationWarning;
}

bool AppController::copyOnly() const
{
    return m_copyOnly;
}

QString AppController::initialView() const
{
    return m_initialView;
}

bool AppController::backgroundCaptureLimited() const
{
    return qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland");
}

void AppController::captureNow()
{
    captureCurrentClipboard();
}

void AppController::copyEmoji(const QString &symbol)
{
    if (symbol.isEmpty())
        return;
    snapshotClipboardBeforeEmoji(symbol);
    m_ownContentHash = contentHash(QStringLiteral("text"), symbol.toUtf8());
    m_clipboard->setText(symbol);
    if (m_store->recordEmoji(symbol)) {
        m_emojiModel.setRecent(m_store->recentEmoji());
        setStatus(tr("Emoji copied. Paste it in your app."));
    } else {
        setStatus(tr("Emoji copied, but its recent entry could not be saved."));
    }
    emit itemCopied();
}

void AppController::copyEntry(qint64 id)
{
    discardEmojiClipboardSnapshot();
    QString kind;
    const QByteArray data = m_store->payload(id, &kind);
    if (data.isEmpty()) {
        setStatus(tr("This clipboard item is no longer available"));
        return;
    }
    if (kind == QStringLiteral("image")) {
        const QImage image = QImage::fromData(data, "PNG");
        if (image.isNull()) {
            setStatus(tr("This image could not be opened"));
            return;
        }
        m_ownContentHash = contentHash(kind, data);
        m_clipboard->setImage(image);
    } else {
        m_ownContentHash = contentHash(kind, data);
        m_clipboard->setText(QString::fromUtf8(data));
    }
    m_store->touch(id);
    m_clipboardModel.reload();
    setStatus(tr("Item copied. Paste it in your app."));
    emit itemCopied();
}

void AppController::pinEntry(qint64 id, bool pinned)
{
    if (!m_store->setPinned(id, pinned)) {
        setStatus(m_store->errorString());
        return;
    }
    m_clipboardModel.reload();
    setStatus(pinned ? tr("Item pinned") : tr("Item unpinned"));
}

void AppController::deleteEntry(qint64 id)
{
    if (!m_store->remove(id)) {
        setStatus(m_store->errorString());
        return;
    }
    m_clipboardModel.reload();
    setStatus(tr("Item deleted"));
}

void AppController::clearRecent()
{
    if (!m_store->clearRecent()) {
        setStatus(m_store->errorString());
        return;
    }
    m_clipboardModel.reload();
    setStatus(tr("Recent clipboard items cleared"));
}

void AppController::reportPasteResult(const QString &result, const QString &shortcut)
{
    if (result == QStringLiteral("sent")) {
        setStatus(tr("Paste sent with %1").arg(shortcut));
    } else if (result == QStringLiteral("target-missing")) {
        setStatus(tr("Target window is gone. Item copied; paste manually."));
    } else if (result == QStringLiteral("focus-failed")) {
        setStatus(tr("Could not focus the target. Item copied; paste manually."));
    } else {
        setStatus(tr("Automatic paste unavailable. Item copied; paste manually."));
    }
}

void AppController::reportShortcutFailure(const QString &shortcut)
{
    const QString message = tr("%1 is unavailable because another application owns the shortcut.")
                                .arg(shortcut);
    if (!m_shortcutFailures.contains(message)) {
        m_shortcutFailures.append(message);
        refreshIntegrationWarning();
    }
    setStatus(message);
}

void AppController::setIntegrationAvailability(bool available, const QString &reason)
{
    const QString failure = available ? QString()
                                      : reason.isEmpty() ? tr("Desktop integration is unavailable")
                                                         : reason;
    if (m_copyOnly == !available && m_backendFailure == failure)
        return;
    m_copyOnly = !available;
    m_backendFailure = failure;
    refreshIntegrationWarning();
}

void AppController::refreshIntegrationWarning()
{
    QStringList messages;
    if (!m_backendFailure.isEmpty())
        messages.append(m_backendFailure);
    messages.append(m_shortcutFailures);
    const QString warning = messages.join(QLatin1Char('\n'));
    if (m_integrationWarning == warning)
        return;
    m_integrationWarning = warning;
    emit integrationWarningChanged();
}

void AppController::reportClipboardRestoreResult(ClipboardRestoreResult result)
{
    if (result == ClipboardRestoreResult::Restored)
        setStatus(tr("Paste sent. Previous clipboard restored."));
    else if (result == ClipboardRestoreResult::Changed)
        setStatus(tr("Paste sent. A newer clipboard copy was kept."));
    else
        setStatus(tr("Paste sent. Previous clipboard could not be safely restored."));
}

bool AppController::hasEmojiClipboardSnapshot() const
{
    return m_snapshotKind != SnapshotKind::None;
}

quint64 AppController::emojiClipboardSnapshotGeneration() const
{
    return m_snapshotGeneration;
}

bool AppController::emojiClipboardSnapshot(QString *temporaryText, QString *mimeType,
                                           QByteArray *content) const
{
    if (m_snapshotKind == SnapshotKind::None)
        return false;
    *temporaryText = m_temporaryEmoji;
    if (m_snapshotKind == SnapshotKind::Text) {
        *mimeType = QStringLiteral("text/plain;charset=utf-8");
        *content = m_snapshotText.toUtf8();
        return true;
    }
    *mimeType = QStringLiteral("image/png");
    content->clear();
    QBuffer buffer(content);
    buffer.open(QIODevice::WriteOnly);
    return m_snapshotImage.save(&buffer, "PNG") && content->size() <= 10 * 1024 * 1024;
}

void AppController::discardEmojiClipboardSnapshot()
{
    ++m_snapshotGeneration;
    m_snapshotKind = SnapshotKind::None;
    m_snapshotText.clear();
    m_snapshotImage = QImage();
    m_temporaryEmoji.clear();
}

void AppController::snapshotClipboardBeforeEmoji(const QString &symbol)
{
    discardEmojiClipboardSnapshot();
    const QMimeData *mime = m_clipboard->mimeData();
    if (!mime || mime->hasUrls()
        || mime->hasFormat(QStringLiteral("application/x-kde-passwordManagerHint"))
        || mime->hasFormat(QStringLiteral("application/x-keepassxc-secret"))
        || mime->hasFormat(QStringLiteral("x-special/gnome-copied-files"))) {
        return;
    }

    if (mime->hasImage()) {
        const QImage image = m_clipboard->image();
        const qint64 pixels = qint64(image.width()) * image.height();
        if (image.isNull() || image.width() > 8192 || image.height() > 8192
            || pixels > 32 * 1024 * 1024) {
            return;
        }
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG") || png.size() > 10 * 1024 * 1024)
            return;
        m_snapshotImage = image;
        m_snapshotKind = SnapshotKind::Image;
    } else if (mime->hasText()) {
        const QString text = mime->text();
        const QByteArray utf8 = text.toUtf8();
        if (utf8.isEmpty() || utf8.size() > 1024 * 1024)
            return;
        m_snapshotText = text;
        m_snapshotKind = SnapshotKind::Text;
    } else {
        return;
    }
    m_temporaryEmoji = symbol;
}

void AppController::captureCurrentClipboard()
{
    if (m_paused || !m_store->isOpen())
        return;
    const QMimeData *mime = m_clipboard->mimeData();
    if (!mime || mime->hasUrls()
        || mime->hasFormat(QStringLiteral("application/x-kde-passwordManagerHint"))
        || mime->hasFormat(QStringLiteral("application/x-keepassxc-secret"))
        || mime->hasFormat(QStringLiteral("x-special/gnome-copied-files"))) {
        return;
    }

    if (mime->hasImage()) {
        const QImage image = m_clipboard->image();
        const qint64 pixels = qint64(image.width()) * image.height();
        if (image.isNull() || image.width() > 8192 || image.height() > 8192
            || pixels > 32 * 1024 * 1024) {
            setStatus(tr("Image is too large for history"));
            return;
        }
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG") || png.size() > 10 * 1024 * 1024) {
            setStatus(tr("Image is too large for history"));
            return;
        }
        if (isOwnClipboardContent(QStringLiteral("image"), png))
            return;
        QByteArray thumbnail;
        QBuffer thumbnailBuffer(&thumbnail);
        thumbnailBuffer.open(QIODevice::WriteOnly);
        image.scaled(QSize(320, 180), Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .save(&thumbnailBuffer, "PNG");
        if (m_store->addImage(png, thumbnail))
            m_clipboardModel.reload();
        else
            setStatus(m_store->errorString());
        return;
    }

    if (!mime->hasText())
        return;
    const QString text = mime->text();
    const QByteArray utf8 = text.toUtf8();
    if (utf8.isEmpty())
        return;
    if (isOwnClipboardContent(QStringLiteral("text"), utf8))
        return;
    if (utf8.size() > 1024 * 1024) {
        setStatus(tr("Text is too large for history"));
        return;
    }
    if (m_store->addText(text))
        m_clipboardModel.reload();
    else
        setStatus(m_store->errorString());
}

void AppController::setStatus(const QString &message)
{
    if (m_status == message)
        return;
    m_status = message;
    emit statusChanged();
}

bool AppController::isOwnClipboardContent(const QString &kind, const QByteArray &content)
{
    if (m_ownContentHash.isEmpty())
        return false;
    if (m_ownContentHash == contentHash(kind, content))
        return true;
    m_ownContentHash.clear();
    return false;
}

QByteArray AppController::contentHash(const QString &kind, const QByteArray &content)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(kind.toUtf8());
    hash.addData(QByteArrayView("\0", 1));
    hash.addData(content);
    return hash.result();
}
