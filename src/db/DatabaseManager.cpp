#include "DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>

namespace msi::db {

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , m_dbPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
               + QStringLiteral("/metrics.db"))
    , m_initialized(false)
{
    // Créer le répertoire parent si nécessaire
    const QString dirPath = QFileInfo(m_dbPath).absolutePath();
    if (!QDir(dirPath).exists()) {
        QDir().mkpath(dirPath);
    }
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::initialize()
{
    if (!openDatabase())
        return false;

    if (!createTables())
        return false;

    m_initialized = true;
    cleanupOldEntries(7);
    emit initializedSuccessfully();
    return true;
}

bool DatabaseManager::openDatabase()
{
    const QString dirPath = QFileInfo(m_dbPath).absolutePath();
    if (!QDir(dirPath).exists()) {
        if (!QDir().mkpath(dirPath)) {
            emit errorOccurred(QStringLiteral("Impossible de créer le répertoire: ") + dirPath);
            return false;
        }
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        emit errorOccurred(m_db.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_db);
    const QString createTable = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS temperature_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  cpu_temp INTEGER NOT NULL,"
        "  gpu_temp INTEGER NOT NULL,"
        "  mode TEXT NOT NULL"
        ")"
    );

    if (!query.exec(createTable)) {
        emit errorOccurred(query.lastError().text());
        return false;
    }

    const QString createIndex = QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_timestamp ON temperature_log(timestamp)"
    );

    if (!query.exec(createIndex)) {
        emit errorOccurred(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::insertTemperature(int cpuTemp, int gpuTemp, const QString &mode)
{
    if (!m_initialized) return false;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO temperature_log (timestamp, cpu_temp, gpu_temp, mode) "
        "VALUES (:ts, :cpu, :gpu, :mode)"
    ));
    query.bindValue(QStringLiteral(":ts"), QDateTime::currentSecsSinceEpoch());
    query.bindValue(QStringLiteral(":cpu"), cpuTemp);
    query.bindValue(QStringLiteral(":gpu"), gpuTemp);
    query.bindValue(QStringLiteral(":mode"), mode);

    if (!query.exec()) {
        emit errorOccurred(query.lastError().text());
        return false;
    }

    ++m_insertsSinceCleanup;
    if (m_insertsSinceCleanup >= 3600) {
        m_insertsSinceCleanup = 0;
        cleanupOldEntries(7);
    }
    return true;
}

bool DatabaseManager::cleanupOldEntries(int maxAgeDays)
{
    if (!m_initialized) return false;

    const qint64 cutoff = QDateTime::currentSecsSinceEpoch()
                          - static_cast<qint64>(maxAgeDays) * 86400;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM temperature_log WHERE timestamp < :cutoff"));
    query.bindValue(QStringLiteral(":cutoff"), cutoff);

    if (!query.exec()) {
        emit errorOccurred(query.lastError().text());
        return false;
    }
    return true;
}

QString DatabaseManager::databasePath() const
{
    return m_dbPath;
}

} // namespace msi::db
