#pragma once

#include <QByteArray>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

struct ClipboardEntry
{
    qint64 id = 0;
    QString kind;
    QString mime;
    QString text;
    QByteArray hash;
    bool pinned = false;
    qint64 capturedAt = 0;
    qint64 usedAt = 0;
};

class HistoryStore
{
public:
    explicit HistoryStore(QString databasePath = {});
    ~HistoryStore();

    HistoryStore(const HistoryStore &) = delete;
    HistoryStore &operator=(const HistoryStore &) = delete;

    bool open();
    bool isOpen() const;
    QString errorString() const;
    QString databasePath() const;

    QList<ClipboardEntry> entries() const;
    QByteArray payload(qint64 id, QString *kind = nullptr) const;
    QByteArray preview(qint64 id) const;
    bool addText(const QString &text);
    bool addImage(const QByteArray &png, const QByteArray &thumbnail);
    bool setPinned(qint64 id, bool pinned);
    bool touch(qint64 id);
    bool remove(qint64 id);
    bool clearRecent();
    bool clearAll();

    QStringList recentEmoji() const;
    bool recordEmoji(const QString &symbol);

private:
    bool migrateSchema(int version);
    bool validateSchema();
    bool tableHasColumns(const QString &table, const QStringList &columns);
    bool addEntry(const QString &kind, const QString &mime, const QByteArray &data,
                  const QByteArray &thumbnail);
    bool trimUnpinned();
    bool exec(const QString &statement);
    qint64 nextTimestamp() const;
    void setError(const QString &message);

    QString m_databasePath;
    QString m_connectionName;
    QString m_error;
    QSqlDatabase m_database;
};
