#include "MetricsRepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

namespace msi::db {

MetricsRepository::MetricsRepository(const QSqlDatabase &db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{}

QVector<MetricsRepository::MetricEntry> MetricsRepository::getHistory(int limit)
{
    QVector<MetricEntry> results;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT timestamp, cpu_temp, gpu_temp, mode "
        "FROM temperature_log ORDER BY timestamp DESC LIMIT :limit"
    ));
    query.bindValue(QStringLiteral(":limit"), limit);

    if (!query.exec())
        return results;

    while (query.next()) {
        MetricEntry e;
        e.timestamp = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
        e.cpuTemp = query.value(1).toInt();
        e.gpuTemp = query.value(2).toInt();
        e.mode = query.value(3).toString();
        results.prepend(e); // ordre chronologique
    }
    return results;
}

QVector<MetricsRepository::MetricEntry> MetricsRepository::getHistoryRange(
    const QDateTime &from, const QDateTime &to)
{
    QVector<MetricEntry> results;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT timestamp, cpu_temp, gpu_temp, mode "
        "FROM temperature_log WHERE timestamp BETWEEN :from AND :to "
        "ORDER BY timestamp ASC"
    ));
    query.bindValue(QStringLiteral(":from"), from.toSecsSinceEpoch());
    query.bindValue(QStringLiteral(":to"), to.toSecsSinceEpoch());

    if (!query.exec())
        return results;

    while (query.next()) {
        MetricEntry e;
        e.timestamp = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
        e.cpuTemp = query.value(1).toInt();
        e.gpuTemp = query.value(2).toInt();
        e.mode = query.value(3).toString();
        results.append(e);
    }
    return results;
}

MetricsRepository::MetricEntry MetricsRepository::getLatest()
{
    MetricEntry entry;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT timestamp, cpu_temp, gpu_temp, mode "
        "FROM temperature_log ORDER BY timestamp DESC LIMIT 1"
    ));

    if (query.exec() && query.next()) {
        entry.timestamp = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
        entry.cpuTemp = query.value(1).toInt();
        entry.gpuTemp = query.value(2).toInt();
        entry.mode = query.value(3).toString();
    }
    return entry;
}

int MetricsRepository::totalRecords() const
{
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM temperature_log")) && query.next())
        return query.value(0).toInt();
    return 0;
}

} // namespace msi::db
