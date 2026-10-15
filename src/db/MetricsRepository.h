#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QVector>

namespace msi::db {

/**
 * @brief Dépôt de métriques thermiques.
 *
 * Encapsule les requêtes SQL pour l'historique des températures.
 */
class MetricsRepository : public QObject
{
    Q_OBJECT

public:
    struct MetricEntry {
        QDateTime timestamp;
        int cpuTemp;
        int gpuTemp;
        QString mode;
    };

    explicit MetricsRepository(const QSqlDatabase &db, QObject *parent = nullptr);

    QVector<MetricEntry> getHistory(int limit = 300);
    QVector<MetricEntry> getHistoryRange(const QDateTime &from, const QDateTime &to);
    MetricEntry getLatest();

    int totalRecords() const;

private:
    QSqlDatabase m_db;
};

} // namespace msi::db
