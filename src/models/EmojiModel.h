#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class EmojiModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(QStringList categories READ categories CONSTANT)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)

public:
    enum Role { SymbolRole = Qt::UserRole + 1, NameRole, GroupRole };

    explicit EmojiModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool load(const QString &resourcePath);
    QString errorString() const;
    QStringList categories() const;
    QString category() const;
    void setCategory(const QString &category);
    QString query() const;
    void setQuery(const QString &query);
    int resultCount() const;
    void setRecent(const QStringList &symbols);

signals:
    void categoryChanged();
    void queryChanged();
    void resultCountChanged();

private:
    struct Emoji {
        QString symbol;
        QString name;
        QString group;
        QString searchable;
    };

    void rebuild();

    QVector<Emoji> m_catalog;
    QVector<int> m_visible;
    QHash<QString, int> m_indexBySymbol;
    QStringList m_categories { QStringLiteral("All"), QStringLiteral("Recent") };
    QStringList m_recent;
    QString m_category = QStringLiteral("All");
    QString m_query;
    QString m_error;
};
