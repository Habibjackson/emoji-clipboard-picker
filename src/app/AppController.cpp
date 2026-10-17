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
    m_ownContentHash = contentHash(QStringLiteral("text"), symbol.toUtf8());
    m_clipboard->setText(symbol);
    if (m_store->recordEmoji(symbol)) {
        m_emojiModel.setRecent(m_store->recentEmoji());
        setStatus(tr("Emoji copied. Paste it in your app."));
    } else {
        setStatus(tr("Emoji copied, but its recent entry could not be saved."));
    }
}

void AppController::copyEntry(qint64 id)
{
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
