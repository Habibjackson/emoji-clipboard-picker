#include "ClipboardModel.h"

#include <QDateTime>
#include <QLocale>

ClipboardModel::ClipboardModel(HistoryStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    reload();
}

int ClipboardModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant ClipboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size())
        return {};
    const ClipboardEntry &entry = m_entries.at(m_visible.at(index.row()));
    switch (role) {
    case EntryIdRole: return entry.id;
    case KindRole: return entry.kind;
    case PreviewTextRole:
        return entry.kind == QStringLiteral("text")
            ? entry.text.left(240).replace(QLatin1Char('\n'), QStringLiteral(" ↵ "))
            : tr("Image");
    case ImageSourceRole:
        return entry.kind == QStringLiteral("image")
            ? QStringLiteral("image://clipboard/%1/%2").arg(entry.id).arg(entry.usedAt)
            : QString();
    case PinnedRole: return entry.pinned;
    case TimeRole: return QLocale().toString(QDateTime::fromMSecsSinceEpoch(entry.usedAt),
                                            QLocale::ShortFormat);
    case SectionRole: return entry.pinned ? tr("Pinned") : tr("Recent");
    default: return {};
    }
}

QHash<int, QByteArray> ClipboardModel::roleNames() const
{
    return { { EntryIdRole, "entryId" }, { KindRole, "kind" },
             { PreviewTextRole, "previewText" }, { ImageSourceRole, "imageSource" },
             { PinnedRole, "pinned" }, { TimeRole, "timeText" },
             { SectionRole, "section" } };
}

QString ClipboardModel::query() const
{
    return m_query;
}

void ClipboardModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    emit queryChanged();
    rebuild();
}

int ClipboardModel::resultCount() const
{
    return m_visible.size();
}

int ClipboardModel::pinnedCount() const
{
    return m_pinnedCount;
}

int ClipboardModel::totalPinnedCount() const
{
    return m_totalPinnedCount;
}

int ClipboardModel::recentCount() const
{
    return m_visible.size() - m_pinnedCount;
}

void ClipboardModel::reload()
{
    m_entries = m_store->entries();
    rebuild();
}

void ClipboardModel::rebuild()
{
    beginResetModel();
    m_visible.clear();
    m_pinnedCount = 0;
    m_totalPinnedCount = 0;
    const QString search = m_query.trimmed();
    for (int i = 0; i < m_entries.size(); ++i) {
        const ClipboardEntry &entry = m_entries.at(i);
        if (entry.pinned)
            ++m_totalPinnedCount;
        if (!search.isEmpty() && !entry.text.contains(search, Qt::CaseInsensitive))
            continue;
        m_visible.append(i);
        if (entry.pinned)
            ++m_pinnedCount;
    }
    endResetModel();
    emit countsChanged();
}
