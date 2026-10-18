#include "AppController.h"

#include "storage/HistoryStore.h"

#include <QBuffer>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMimeData>
#include <QSet>
#include <QSettings>
#include <QTimer>

#include <atomic>
#include <utility>

namespace {
std::atomic<QLoggingCategory::CategoryFilter> previousCategoryFilter = nullptr;

void filterImageDecoderLogs(QLoggingCategory *category)
{
    if (auto previous = previousCategoryFilter.load(std::memory_order_acquire))
        previous(category);
    if (!QByteArrayView(category->categoryName()).startsWith("qt.gui.imageio."))
        return;
    // Decoders may include clipboard bytes in their diagnostic messages.
    category->setEnabled(QtDebugMsg, false);
    category->setEnabled(QtInfoMsg, false);
    category->setEnabled(QtWarningMsg, false);
    category->setEnabled(QtCriticalMsg, false);
}

void protectClipboardDecoderLogs()
{
    static const bool installed = [] {
        previousCategoryFilter.store(QLoggingCategory::installFilter(filterImageDecoderLogs),
                                     std::memory_order_release);
        return true;
    }();
    Q_UNUSED(installed)
}

constexpr qsizetype kMaxTextBytes = 1024 * 1024;
constexpr qsizetype kMaxImageBytes = 10 * 1024 * 1024;
constexpr qint64 kMaxImagePixels = 32 * 1024 * 1024;

enum class ImageIssue { None, NoImage, Empty, Unsupported, Invalid, TooLarge };

struct NormalizedImage
{
    QImage image;
    QByteArray png;
    ImageIssue issue = ImageIssue::NoImage;
};

bool imageTooLarge(const QSize &size)
{
    return !size.isValid() || size.width() > 8192 || size.height() > 8192
        || qint64(size.width()) * size.height() > kMaxImagePixels;
}

bool hasBitmapSignature(const QString &format, const QByteArray &data)
{
    if (format == QLatin1String("image/png"))
        return data.startsWith(QByteArray::fromHex("89504e470d0a1a0a"));
    if (format == QLatin1String("image/jpeg"))
        return data.size() >= 3 && uchar(data[0]) == 0xff
            && uchar(data[1]) == 0xd8 && uchar(data[2]) == 0xff;
    if (format == QLatin1String("image/bmp")
        || format == QLatin1String("image/x-ms-bmp")) {
        return data.startsWith("BM");
    }
    if (format == QLatin1String("image/gif"))
        return data.startsWith("GIF87a") || data.startsWith("GIF89a");
    if (format == QLatin1String("image/webp"))
        return data.size() >= 12 && data.startsWith("RIFF")
            && data.mid(8, 4) == QByteArray("WEBP");
    if (format == QLatin1String("image/tiff"))
        return data.startsWith(QByteArray::fromHex("49492a00"))
            || data.startsWith(QByteArray::fromHex("4d4d002a"));
    return false;
}

NormalizedImage normalizeImage(const QMimeData *mime)
{
    NormalizedImage result;
    const QStringList formats = mime->formats();
    static const QSet<QString> bitmapTypes = {
        QStringLiteral("image/png"), QStringLiteral("image/jpeg"),
        QStringLiteral("image/bmp"), QStringLiteral("image/x-ms-bmp"),
        QStringLiteral("image/gif"), QStringLiteral("image/webp"),
        QStringLiteral("image/tiff")
    };
    bool offeredImage = mime->hasImage();
    bool offeredSupported = false;
    for (const QString &format : formats) {
        if (!format.startsWith(QLatin1String("image/")))
            continue;
        offeredImage = true;
        if (!bitmapTypes.contains(format.toLower()))
            continue;
        offeredSupported = true;
        QByteArray encoded = mime->data(format);
        if (encoded.isEmpty()) {
            result.issue = ImageIssue::Empty;
            continue;
        }
        if (encoded.size() > kMaxImageBytes) {
            result.issue = ImageIssue::TooLarge;
            continue;
        }
        if (!hasBitmapSignature(format.toLower(), encoded)) {
            result.issue = ImageIssue::Invalid;
            continue;
        }
        QBuffer buffer(&encoded);
        if (!buffer.open(QIODevice::ReadOnly)) {
            result.issue = ImageIssue::Invalid;
            continue;
        }
        const QByteArray imageFormat = format == QLatin1String("image/x-ms-bmp")
            ? QByteArray("BMP") : format.mid(6).toLatin1();
        QImageReader reader(&buffer, imageFormat);
        reader.setAutoDetectImageFormat(false);
        const QSize size = reader.size();
        if (!size.isValid()) {
            result.issue = ImageIssue::Invalid;
            continue;
        }
        if (imageTooLarge(size)) {
            result.issue = ImageIssue::TooLarge;
            continue;
        }
        result.image = reader.read();
        if (result.image.isNull()) {
            result.issue = ImageIssue::Invalid;
            continue;
        }
        break;
    }
    if (result.image.isNull() && mime->hasFormat(QStringLiteral("application/x-qt-image"))) {
        offeredImage = true;
        offeredSupported = true;
        result.image = qvariant_cast<QImage>(mime->imageData());
        if (result.image.isNull())
            result.issue = ImageIssue::Invalid;
        else if (imageTooLarge(result.image.size()))
            result.issue = ImageIssue::TooLarge;
    }
    if (result.image.isNull()) {
        if (!offeredImage)
            result.issue = ImageIssue::NoImage;
        else if (!offeredSupported)
            result.issue = ImageIssue::Unsupported;
        return result;
    }
    if (imageTooLarge(result.image.size())) {
        result.issue = ImageIssue::TooLarge;
        return result;
    }
    QBuffer output(&result.png);
    output.open(QIODevice::WriteOnly);
    if (!result.image.save(&output, "PNG")) {
        result.issue = ImageIssue::Invalid;
    } else if (result.png.size() > kMaxImageBytes) {
        result.issue = ImageIssue::TooLarge;
    } else {
        result.issue = ImageIssue::None;
    }
    return result;
}

bool privateClipboard(const QMimeData *mime)
{
    return mime->hasFormat(QStringLiteral("application/x-kde-passwordManagerHint"))
        || mime->hasFormat(QStringLiteral("application/x-keepassxc-secret"))
        || mime->hasFormat(QStringLiteral("x-special/gnome-copied-files"));
}
}

AppController::AppController(HistoryStore *store, QString initialView, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_clipboard(QGuiApplication::clipboard())
    , m_emojiModel(this)
    , m_clipboardModel(store, this)
    , m_initialView(std::move(initialView))
{
    const QString savedMode = QSettings().value(QStringLiteral("pasteShortcutMode"),
                                                QStringLiteral("auto")).toString();
    m_pasteShortcutMode = savedMode == QStringLiteral("ctrl-v")
            || savedMode == QStringLiteral("ctrl-shift-v") ? savedMode : QStringLiteral("auto");
    const QString savedTheme = QSettings().value(QStringLiteral("themeMode"),
                                                 QStringLiteral("system")).toString();
    m_themeMode = savedTheme == QStringLiteral("light") || savedTheme == QStringLiteral("dark")
            ? savedTheme : QStringLiteral("system");
    protectClipboardDecoderLogs();
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

QString AppController::pasteShortcutMode() const
{
    return m_pasteShortcutMode;
}

void AppController::setPasteShortcutMode(const QString &mode)
{
    if (mode != QStringLiteral("auto") && mode != QStringLiteral("ctrl-v")
        && mode != QStringLiteral("ctrl-shift-v"))
        return;
    if (m_pasteShortcutMode == mode)
        return;
    m_pasteShortcutMode = mode;
    QSettings().setValue(QStringLiteral("pasteShortcutMode"), mode);
    emit pasteShortcutModeChanged();
}

QString AppController::themeMode() const
{
    return m_themeMode;
}

void AppController::setThemeMode(const QString &mode)
{
    if (mode != QStringLiteral("system") && mode != QStringLiteral("light")
        && mode != QStringLiteral("dark"))
        return;
    if (m_themeMode == mode)
        return;
    m_themeMode = mode;
    QSettings().setValue(QStringLiteral("themeMode"), mode);
    emit themeModeChanged();
}

QString AppController::pasteShortcutFor(bool terminal) const
{
    if (m_pasteShortcutMode == QStringLiteral("ctrl-shift-v")
        || (m_pasteShortcutMode == QStringLiteral("auto") && terminal))
        return QStringLiteral("Ctrl+Shift+V");
    return QStringLiteral("Ctrl+V");
}

QString AppController::status() const
{
    return m_status;
}

QString AppController::attention() const
{
    return m_attention;
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
    return qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland")
        && m_copyOnly;
}

void AppController::captureNow()
{
    captureCurrentClipboard();
}

void AppController::capturePayload(const QString &kind, const QString &mimeType,
                                   const QByteArray &content)
{
    QMimeData mime;
    if (kind == QStringLiteral("text")
        && (mimeType == QStringLiteral("text/plain")
            || mimeType == QStringLiteral("text/plain;charset=utf-8"))) {
        mime.setText(QString::fromUtf8(content));
    } else if (kind == QStringLiteral("image") && mimeType.startsWith(QLatin1String("image/"))) {
        mime.setData(mimeType, content);
    } else {
        return;
    }
    captureMimeData(&mime);
}

void AppController::copyEmoji(const QString &symbol)
{
    if (symbol.isEmpty())
        return;
    setAttention(QString());
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
        setAttention(m_status);
        return;
    }
    if (kind == QStringLiteral("image")) {
        const QImage image = QImage::fromData(data, "PNG");
        if (image.isNull()) {
            setStatus(tr("This image could not be opened"));
            setAttention(m_status);
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
    setAttention(QString());
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

void AppController::clearAll()
{
    if (!m_store->clearAll()) {
        setStatus(m_store->errorString());
        return;
    }
    m_clipboardModel.reload();
    setStatus(tr("All clipboard history cleared"));
}

void AppController::reportPasteResult(const QString &result, const QString &shortcut)
{
    if (result == QStringLiteral("sent")) {
        setStatus(tr("Paste sent with %1").arg(shortcut));
        setAttention(QString());
    } else if (result == QStringLiteral("target-missing")) {
        setStatus(tr("Target window is gone. Item copied; paste manually."));
    } else if (result == QStringLiteral("focus-failed")) {
        setStatus(tr("Could not focus the target. Item copied; paste manually."));
    } else if (result == QStringLiteral("clipboard-changed")) {
        setStatus(tr("Clipboard changed before paste. Paste manually if needed."));
    } else if (result == QStringLiteral("cancelled")) {
        setStatus(tr("Paste cancelled. Item copied; paste manually."));
    } else {
        setStatus(tr("Automatic paste unavailable. Item copied; paste manually."));
    }
    if (result != QStringLiteral("sent"))
        setAttention(m_status);
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
    if (result == ClipboardRestoreResult::Restored) {
        m_ownContentHash.clear();
        if (!m_restoreObserved)
            m_restoreSuppressionExpiresAt = QDateTime::currentMSecsSinceEpoch() + 3000;
        setStatus(tr("Paste sent. Previous clipboard restored."));
    } else if (result == ClipboardRestoreResult::Changed) {
        m_expectedRestoreHash.clear();
        m_restoreSuppressionExpiresAt = 0;
        m_restoreObserved = false;
        m_ownContentHash.clear();
        captureCurrentClipboard();
        setStatus(tr("Paste sent. A newer clipboard copy was kept."));
    } else {
        m_expectedRestoreHash.clear();
        m_restoreSuppressionExpiresAt = 0;
        m_restoreObserved = false;
        setStatus(tr("Paste sent. Previous clipboard could not be safely restored."));
    }
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

void AppController::expectClipboardRestore(const QString &mimeType, const QByteArray &content)
{
    m_expectedRestoreHash.clear();
    m_restoreSuppressionExpiresAt = 0;
    m_restoreObserved = false;
    if (content.isEmpty())
        return;
    const QString kind = mimeType == QStringLiteral("image/png") ? QStringLiteral("image")
        : mimeType == QStringLiteral("text/plain;charset=utf-8") ? QStringLiteral("text")
                                                              : QString();
    if (kind.isEmpty())
        return;
    m_expectedRestoreHash = contentHash(kind, content);
    m_restoreSuppressionExpiresAt = QDateTime::currentMSecsSinceEpoch() + 5000;
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
    m_expectedRestoreHash.clear();
    m_restoreSuppressionExpiresAt = 0;
    m_restoreObserved = false;
    const QMimeData *mime = m_clipboard->mimeData();
    if (!mime || mime->hasUrls() || privateClipboard(mime)) {
        return;
    }

    const NormalizedImage normalized = normalizeImage(mime);
    if (normalized.issue != ImageIssue::NoImage) {
        if (normalized.issue != ImageIssue::None)
            return;
        m_snapshotImage = normalized.image;
        m_snapshotKind = SnapshotKind::Image;
    } else if (mime->hasText()) {
        const QString text = mime->text();
        const QByteArray utf8 = text.toUtf8();
        if (utf8.isEmpty() || utf8.size() > kMaxTextBytes)
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
    captureMimeData(m_clipboard->mimeData());
}

void AppController::captureMimeData(const QMimeData *mime)
{
    if (m_paused || !m_store->isOpen())
        return;
    if (!mime)
        return;
    if (mime->formats().isEmpty())
        return;
    if (privateClipboard(mime)) {
        setStatus(tr("Private clipboard item was skipped"));
        return;
    }
    if (mime->hasUrls()) {
        setStatus(tr("File copy was skipped"));
        return;
    }

    const NormalizedImage normalized = normalizeImage(mime);
    if (normalized.issue != ImageIssue::NoImage) {
        if (normalized.issue == ImageIssue::TooLarge) {
            setStatus(tr("Image is too large for history"));
            setAttention(m_status);
        } else if (normalized.issue == ImageIssue::Unsupported)
            setStatus(tr("Unsupported image format was skipped"));
        else if (normalized.issue == ImageIssue::Empty)
            setStatus(tr("Empty image was skipped"));
        else if (normalized.issue == ImageIssue::Invalid)
            setStatus(tr("Image could not be decoded for history"));
        if (normalized.issue != ImageIssue::None)
            return;
        if (isOwnClipboardContent(QStringLiteral("image"), normalized.png))
            return;
        QByteArray thumbnail;
        QBuffer thumbnailBuffer(&thumbnail);
        thumbnailBuffer.open(QIODevice::WriteOnly);
        normalized.image.scaled(QSize(320, 180), Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .save(&thumbnailBuffer, "PNG");
        if (m_store->addImage(normalized.png, thumbnail)) {
            m_clipboardModel.reload();
            setAttention(QString());
        } else
            setStatus(m_store->errorString());
        return;
    }

    if (!mime->hasText()) {
        setStatus(tr("Unsupported clipboard format was skipped"));
        return;
    }
    const QString text = mime->text();
    const QByteArray utf8 = text.toUtf8();
    if (utf8.isEmpty()) {
        setStatus(tr("Empty clipboard item was skipped"));
        return;
    }
    if (isOwnClipboardContent(QStringLiteral("text"), utf8))
        return;
    if (utf8.size() > kMaxTextBytes) {
        setStatus(tr("Text is too large for history"));
        setAttention(m_status);
        return;
    }
    if (m_store->addText(text)) {
        m_clipboardModel.reload();
        setAttention(QString());
    } else
        setStatus(m_store->errorString());
}

void AppController::setStatus(const QString &message)
{
    if (m_status == message)
        return;
    m_status = message;
    emit statusChanged();
}

void AppController::setAttention(const QString &message)
{
    if (m_attention == message)
        return;
    m_attention = message;
    emit attentionChanged();
}

bool AppController::isOwnClipboardContent(const QString &kind, const QByteArray &content)
{
    const QByteArray hash = contentHash(kind, content);
    if (!m_expectedRestoreHash.isEmpty()) {
        if (QDateTime::currentMSecsSinceEpoch() > m_restoreSuppressionExpiresAt) {
            m_expectedRestoreHash.clear();
            m_restoreSuppressionExpiresAt = 0;
            m_restoreObserved = false;
        } else if (m_expectedRestoreHash == hash) {
            m_restoreObserved = true;
            m_restoreSuppressionExpiresAt = QDateTime::currentMSecsSinceEpoch() + 500;
            m_ownContentHash.clear();
            return true;
        }
    }
    if (m_ownContentHash.isEmpty())
        return false;
    if (m_ownContentHash == hash)
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
