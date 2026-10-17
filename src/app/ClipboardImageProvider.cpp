#include "ClipboardImageProvider.h"

#include "storage/HistoryStore.h"

#include <QImage>

ClipboardImageProvider::ClipboardImageProvider(HistoryStore *store)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_store(store)
{
}

QImage ClipboardImageProvider::requestImage(const QString &id, QSize *size,
                                           const QSize &requestedSize)
{
    bool ok = false;
    const qint64 entryId = id.section(QLatin1Char('/'), 0, 0).toLongLong(&ok);
    if (!ok)
        return {};
    QImage image = QImage::fromData(m_store->preview(entryId), "PNG");
    if (size)
        *size = image.size();
    if (!image.isNull() && requestedSize.isValid())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}
