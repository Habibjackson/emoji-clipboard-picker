#pragma once

#include "storage/HistoryStore.h"

#include <QAbstractListModel>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class ClipboardModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY countsChanged)
    Q_PROPERTY(int pinnedCount READ pinnedCount NOTIFY countsChanged)
    Q_PROPERTY(int totalPinnedCount READ totalPinnedCount NOTIFY countsChanged)
    Q_PROPERTY(int recentCount READ recentCount NOTIFY countsChanged)

public:
    enum Role { EntryIdRole = Qt::UserRole + 1, KindRole, PreviewTextRole,
                ImageSourceRole, PinnedRole, TimeRole, SectionRole };

    explicit ClipboardModel(HistoryStore *store, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const;
    void setQuery(const QString &query);
    int resultCount() const;
    int pinnedCount() const;
    int totalPinnedCount() const;
    int recentCount() const;
    void reload();

signals:
    void queryChanged();
    void countsChanged();

private:
    void rebuild();

    HistoryStore *m_store;
    QList<ClipboardEntry> m_entries;
    QVector<int> m_visible;
    QString m_query;
    int m_pinnedCount = 0;
    int m_totalPinnedCount = 0;
};
