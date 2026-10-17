#pragma once

#include <QQuickImageProvider>

class HistoryStore;

class ClipboardImageProvider : public QQuickImageProvider
{
public:
    explicit ClipboardImageProvider(HistoryStore *store);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    HistoryStore *m_store;
};
