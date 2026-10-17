#include "EmojiModel.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <utility>

EmojiModel::EmojiModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EmojiModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant EmojiModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size())
        return {};
    const Emoji &emoji = m_catalog.at(m_visible.at(index.row()));
    switch (role) {
    case SymbolRole: return emoji.symbol;
    case NameRole: return emoji.name;
    case GroupRole: return emoji.group;
    default: return {};
    }
}

QHash<int, QByteArray> EmojiModel::roleNames() const
{
    return { { SymbolRole, "symbol" }, { NameRole, "emojiName" }, { GroupRole, "groupName" } };
}

bool EmojiModel::load(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = file.errorString();
        return false;
    }

    beginResetModel();
    m_catalog.clear();
    m_visible.clear();
    m_indexBySymbol.clear();
    m_categories = { QStringLiteral("All"), QStringLiteral("Recent") };
    QString group;
    QString subgroup;
    static const QRegularExpression namePattern(QStringLiteral("^\\S+\\s+E[0-9.]+\\s+(.+)$"));
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.startsWith(QStringLiteral("# group: "))) {
            group = line.mid(9).trimmed();
            if (!m_categories.contains(group))
                m_categories.append(group);
            continue;
        }
        if (line.startsWith(QStringLiteral("# subgroup: "))) {
            subgroup = line.mid(12).trimmed().replace(QLatin1Char('-'), QLatin1Char(' '));
            continue;
        }
        if (!line.contains(QStringLiteral("; fully-qualified")))
            continue;

        const int separator = line.indexOf(QLatin1Char(';'));
        const int comment = line.indexOf(QLatin1Char('#'), separator);
        if (separator < 0 || comment < 0)
            continue;
        QString symbol;
        bool valid = true;
        const QStringList codePoints = line.left(separator).trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &hex : codePoints) {
            bool ok = false;
            const char32_t point = hex.toUInt(&ok, 16);
            if (!ok) {
                valid = false;
                break;
            }
            symbol += QString::fromUcs4(&point, 1);
        }
        if (!valid || symbol.isEmpty())
            continue;
        const QRegularExpressionMatch match = namePattern.match(line.mid(comment + 1).trimmed());
        if (!match.hasMatch())
            continue;

        const QString name = match.captured(1);
        Emoji emoji { symbol, name, group, name + QLatin1Char(' ') + group + QLatin1Char(' ') + subgroup };
        m_indexBySymbol.insert(symbol, m_catalog.size());
        m_catalog.append(std::move(emoji));
    }
    endResetModel();
    m_error.clear();
    rebuild();
    return !m_catalog.isEmpty();
}

QString EmojiModel::errorString() const
{
    return m_error;
}

QStringList EmojiModel::categories() const
{
    return m_categories;
}

QString EmojiModel::category() const
{
    return m_category;
}

void EmojiModel::setCategory(const QString &category)
{
    if (m_category == category || !m_categories.contains(category))
        return;
    m_category = category;
    emit categoryChanged();
    rebuild();
}

QString EmojiModel::query() const
{
    return m_query;
}

void EmojiModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    emit queryChanged();
    rebuild();
}

int EmojiModel::resultCount() const
{
    return m_visible.size();
}

void EmojiModel::setRecent(const QStringList &symbols)
{
    m_recent = symbols;
    if (m_category == QStringLiteral("Recent") && m_query.isEmpty())
        rebuild();
}

void EmojiModel::rebuild()
{
    beginResetModel();
    m_visible.clear();
    const QString search = m_query.trimmed();
    if (!search.isEmpty()) {
        for (int i = 0; i < m_catalog.size(); ++i) {
            if (m_catalog.at(i).searchable.contains(search, Qt::CaseInsensitive)
                || m_catalog.at(i).symbol == search) {
                m_visible.append(i);
            }
        }
    } else if (m_category == QStringLiteral("Recent")) {
        for (const QString &symbol : std::as_const(m_recent)) {
            const auto found = m_indexBySymbol.constFind(symbol);
            if (found != m_indexBySymbol.cend())
                m_visible.append(*found);
        }
    } else {
        for (int i = 0; i < m_catalog.size(); ++i) {
            if (m_category == QStringLiteral("All") || m_catalog.at(i).group == m_category)
                m_visible.append(i);
        }
    }
    endResetModel();
    emit resultCountChanged();
}
