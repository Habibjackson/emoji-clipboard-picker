#include "HistoryStore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <utility>

namespace {
constexpr int kSchemaVersion = 1;
}

HistoryStore::HistoryStore(QString databasePath)
    : m_databasePath(std::move(databasePath))
    , m_connectionName(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

HistoryStore::~HistoryStore()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool HistoryStore::open()
{
    if (m_database.isOpen())
        return true;

    if (m_databasePath.isEmpty()) {
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_databasePath = directory + QStringLiteral("/history.sqlite");
    }

    const QString directory = QFileInfo(m_databasePath).absolutePath();
    if (!QDir().mkpath(directory)
        || !QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
        setError(QStringLiteral("Could not create a private data directory"));
        return false;
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        setError(m_database.lastError().text());
        return false;
    }
    if (!QFile::setPermissions(m_databasePath, QFile::ReadOwner | QFile::WriteOwner)) {
        setError(QStringLiteral("Could not protect the history database"));
        m_database.close();
        return false;
    }

    if (!exec(QStringLiteral("PRAGMA secure_delete = ON"))
        || !exec(QStringLiteral("PRAGMA busy_timeout = 2000"))) {
        m_database.close();
        return false;
    }

    QSqlQuery version(m_database);
    if (!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next()) {
        setError(version.lastError().text());
        m_database.close();
        return false;
    }
    const int schemaVersion = version.value(0).toInt();
    if (schemaVersion < 0 || schemaVersion > kSchemaVersion) {
        setError(QStringLiteral("History database uses a newer schema"));
        m_database.close();
        return false;
    }

    if (!migrateSchema(schemaVersion) || !validateSchema()) {
        m_database.close();
        return false;
    }

    m_error.clear();
    return true;
}

bool HistoryStore::migrateSchema(int version)
{
    if (version == kSchemaVersion)
        return true;
    if (!m_database.transaction()) {
        setError(m_database.lastError().text());
        return false;
    }
    if (!exec(QStringLiteral("CREATE TABLE IF NOT EXISTS clipboard_entries ("
                                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                "kind TEXT NOT NULL CHECK(kind IN ('text', 'image')) ,"
                                "mime TEXT NOT NULL, payload BLOB NOT NULL, preview BLOB,"
                                "hash BLOB NOT NULL, pinned INTEGER NOT NULL DEFAULT 0,"
                                "captured_at INTEGER NOT NULL, used_at INTEGER NOT NULL,"
                                "UNIQUE(kind, hash))"))
        || !exec(QStringLiteral("CREATE TABLE IF NOT EXISTS emoji_recents ("
                                "symbol TEXT PRIMARY KEY, used_at INTEGER NOT NULL)"))
        || !validateSchema()
        || !exec(QStringLiteral("CREATE INDEX IF NOT EXISTS clipboard_order "
                                "ON clipboard_entries(pinned, used_at DESC)"))
        || !exec(QStringLiteral("PRAGMA user_version = 1"))) {
        m_database.rollback();
        return false;
    }
    if (!m_database.commit()) {
        setError(m_database.lastError().text());
        m_database.rollback();
        return false;
    }
    return true;
}

bool HistoryStore::validateSchema()
{
    return tableHasColumns(QStringLiteral("clipboard_entries"),
                           { QStringLiteral("id"), QStringLiteral("kind"),
                             QStringLiteral("mime"), QStringLiteral("payload"),
                             QStringLiteral("preview"), QStringLiteral("hash"),
                             QStringLiteral("pinned"), QStringLiteral("captured_at"),
                             QStringLiteral("used_at") })
        && tableHasColumns(QStringLiteral("emoji_recents"),
                           { QStringLiteral("symbol"), QStringLiteral("used_at") });
}

bool HistoryStore::tableHasColumns(const QString &table, const QStringList &columns)
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        setError(query.lastError().text());
        return false;
    }
    QSet<QString> actual;
    while (query.next())
        actual.insert(query.value(1).toString());
    for (const QString &column : columns) {
        if (!actual.contains(column)) {
            setError(QStringLiteral("History database has an incompatible schema"));
            return false;
        }
    }
    return true;
}

bool HistoryStore::isOpen() const
{
    return m_database.isOpen();
}

QString HistoryStore::errorString() const
{
    return m_error;
}

QString HistoryStore::databasePath() const
{
    return m_databasePath;
}

QList<ClipboardEntry> HistoryStore::entries() const
{
    QList<ClipboardEntry> result;
    if (!isOpen())
        return result;

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT id, kind, mime, "
                                   "CASE WHEN kind = 'text' THEN payload END, "
                                   "hash, pinned, captured_at, used_at "
                                   "FROM clipboard_entries ORDER BY pinned DESC, used_at DESC, id DESC"))) {
        return result;
    }
    while (query.next()) {
        ClipboardEntry entry;
        entry.id = query.value(0).toLongLong();
        entry.kind = query.value(1).toString();
        entry.mime = query.value(2).toString();
        if (entry.kind == QStringLiteral("text"))
            entry.text = QString::fromUtf8(query.value(3).toByteArray());
        entry.hash = query.value(4).toByteArray();
        entry.pinned = query.value(5).toBool();
        entry.capturedAt = query.value(6).toLongLong();
        entry.usedAt = query.value(7).toLongLong();
        result.append(std::move(entry));
    }
    return result;
}

QByteArray HistoryStore::payload(qint64 id, QString *kind) const
{
    if (!isOpen())
        return {};

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT kind, payload FROM clipboard_entries WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return {};
    if (kind)
        *kind = query.value(0).toString();
    return query.value(1).toByteArray();
}

QByteArray HistoryStore::preview(qint64 id) const
{
    if (!isOpen())
        return {};

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT preview FROM clipboard_entries WHERE id = ? AND kind = 'image'"));
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return {};
    return query.value(0).toByteArray();
}

bool HistoryStore::addText(const QString &text)
{
    const QByteArray data = text.toUtf8();
    if (data.isEmpty() || data.size() > 1024 * 1024) {
        setError(QStringLiteral("Text must be between 1 byte and 1 MiB"));
        return false;
    }
    return addEntry(QStringLiteral("text"), QStringLiteral("text/plain;charset=utf-8"), data, {});
}

bool HistoryStore::addImage(const QByteArray &png, const QByteArray &thumbnail)
{
    if (png.isEmpty() || png.size() > 10 * 1024 * 1024 || thumbnail.isEmpty()) {
        setError(QStringLiteral("Image must be a valid PNG no larger than 10 MiB"));
        return false;
    }
    return addEntry(QStringLiteral("image"), QStringLiteral("image/png"), png, thumbnail);
}

bool HistoryStore::addEntry(const QString &kind, const QString &mime, const QByteArray &data,
                            const QByteArray &thumbnail)
{
    if (!isOpen()) {
        setError(QStringLiteral("History database is unavailable"));
        return false;
    }
    const QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    if (!m_database.transaction()) {
        setError(m_database.lastError().text());
        return false;
    }

    QSqlQuery find(m_database);
    find.prepare(QStringLiteral("SELECT id FROM clipboard_entries WHERE kind = ? AND hash = ?"));
    find.addBindValue(kind);
    find.addBindValue(hash);
    if (!find.exec()) {
        setError(find.lastError().text());
        m_database.rollback();
        return false;
    }

    const qint64 stamp = nextTimestamp();
    QSqlQuery change(m_database);
    if (find.next()) {
        change.prepare(QStringLiteral("UPDATE clipboard_entries SET used_at = ? WHERE id = ?"));
        change.addBindValue(stamp);
        change.addBindValue(find.value(0).toLongLong());
    } else {
        change.prepare(QStringLiteral("INSERT INTO clipboard_entries "
                                      "(kind, mime, payload, preview, hash, pinned, captured_at, used_at) "
                                      "VALUES (?, ?, ?, ?, ?, 0, ?, ?)"));
        change.addBindValue(kind);
        change.addBindValue(mime);
        change.addBindValue(data);
        change.addBindValue(thumbnail);
        change.addBindValue(hash);
        change.addBindValue(stamp);
        change.addBindValue(stamp);
    }
    if (!change.exec()) {
        setError(change.lastError().text());
        m_database.rollback();
        return false;
    }
    if (!trimUnpinned()) {
        m_database.rollback();
        return false;
    }
    if (!m_database.commit()) {
        setError(m_database.lastError().text());
        m_database.rollback();
        return false;
    }
    m_error.clear();
    return true;
}

bool HistoryStore::setPinned(qint64 id, bool pinned)
{
    if (!isOpen() || !m_database.transaction()) {
        setError(QStringLiteral("History database is unavailable"));
        return false;
    }

    QSqlQuery current(m_database);
    current.prepare(QStringLiteral("SELECT pinned FROM clipboard_entries WHERE id = ?"));
    current.addBindValue(id);
    if (!current.exec() || !current.next()) {
        setError(QStringLiteral("Clipboard item no longer exists"));
        m_database.rollback();
        return false;
    }
    if (current.value(0).toBool() == pinned) {
        m_database.commit();
        m_error.clear();
        return true;
    }
    if (pinned) {
        QSqlQuery count(m_database);
        if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM clipboard_entries WHERE pinned = 1"))
            || !count.next()) {
            setError(count.lastError().text());
            m_database.rollback();
            return false;
        }
        if (count.value(0).toInt() >= 5) {
            setError(QStringLiteral("Only five clipboard items can be pinned"));
            m_database.rollback();
            return false;
        }
    }

    QSqlQuery change(m_database);
    change.prepare(QStringLiteral("UPDATE clipboard_entries SET pinned = ?, used_at = ? WHERE id = ?"));
    change.addBindValue(pinned ? 1 : 0);
    change.addBindValue(nextTimestamp());
    change.addBindValue(id);
    if (!change.exec()) {
        setError(change.lastError().text());
        m_database.rollback();
        return false;
    }
    if (!trimUnpinned()) {
        m_database.rollback();
        return false;
    }
    if (!m_database.commit()) {
        setError(m_database.lastError().text());
        m_database.rollback();
        return false;
    }
    m_error.clear();
    return true;
}

bool HistoryStore::touch(qint64 id)
{
    if (!isOpen())
        return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE clipboard_entries SET used_at = ? WHERE id = ?"));
    query.addBindValue(nextTimestamp());
    query.addBindValue(id);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    m_error.clear();
    return query.numRowsAffected() > 0;
}

bool HistoryStore::remove(qint64 id)
{
    if (!isOpen())
        return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM clipboard_entries WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    m_error.clear();
    return true;
}

bool HistoryStore::clearRecent()
{
    if (!isOpen()) {
        setError(QStringLiteral("History database is unavailable"));
        return false;
    }
    if (!exec(QStringLiteral("DELETE FROM clipboard_entries WHERE pinned = 0")))
        return false;
    m_error.clear();
    return true;
}

bool HistoryStore::clearAll()
{
    if (!isOpen()) {
        setError(QStringLiteral("History database is unavailable"));
        return false;
    }
    if (!exec(QStringLiteral("DELETE FROM clipboard_entries")))
        return false;
    m_error.clear();
    return true;
}

QStringList HistoryStore::recentEmoji() const
{
    QStringList result;
    if (!isOpen())
        return result;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT symbol FROM emoji_recents "
                                   "ORDER BY used_at DESC LIMIT 24"))) {
        return result;
    }
    while (query.next())
        result.append(query.value(0).toString());
    return result;
}

bool HistoryStore::recordEmoji(const QString &symbol)
{
    if (!isOpen() || symbol.isEmpty())
        return false;
    if (!m_database.transaction()) {
        setError(m_database.lastError().text());
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO emoji_recents(symbol, used_at) VALUES(?, ?) "
                                 "ON CONFLICT(symbol) DO UPDATE SET used_at = excluded.used_at"));
    query.addBindValue(symbol);
    query.addBindValue(nextTimestamp());
    if (!query.exec()) {
        setError(query.lastError().text());
        m_database.rollback();
        return false;
    }
    if (!exec(QStringLiteral("DELETE FROM emoji_recents WHERE symbol NOT IN ("
                             "SELECT symbol FROM emoji_recents ORDER BY used_at DESC LIMIT 24)"))) {
        m_database.rollback();
        return false;
    }
    if (!m_database.commit()) {
        setError(m_database.lastError().text());
        m_database.rollback();
        return false;
    }
    m_error.clear();
    return true;
}

bool HistoryStore::trimUnpinned()
{
    return exec(QStringLiteral("DELETE FROM clipboard_entries WHERE pinned = 0 AND id NOT IN ("
                               "SELECT id FROM clipboard_entries WHERE pinned = 0 "
                               "ORDER BY used_at DESC, id DESC LIMIT 20)"));
}

bool HistoryStore::exec(const QString &statement)
{
    QSqlQuery query(m_database);
    if (!query.exec(statement)) {
        setError(query.lastError().text());
        return false;
    }
    return true;
}

qint64 HistoryStore::nextTimestamp() const
{
    qint64 latest = 0;
    QSqlQuery query(m_database);
    if (query.exec(QStringLiteral("SELECT MAX(value) FROM ("
                                  "SELECT MAX(used_at) AS value FROM clipboard_entries "
                                  "UNION ALL SELECT MAX(used_at) FROM emoji_recents)"))
        && query.next()) {
        latest = query.value(0).toLongLong();
    }
    return qMax(QDateTime::currentMSecsSinceEpoch(), latest + 1);
}

void HistoryStore::setError(const QString &message)
{
    m_error = message;
}
