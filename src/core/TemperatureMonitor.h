#pragma once

#include <QObject>
#include <QVector>
#include <QDateTime>

namespace msi::core {

/**
 * @brief Moniteur de températures avec historique.
 *
 * Conserve un historique glissant des températures CPU et GPU
 * pour l'affichage graphique et les statistiques.
 */
class TemperatureMonitor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int historySize READ historySize WRITE setHistorySize NOTIFY historySizeChanged)

public:
    struct Reading {
        QDateTime timestamp;
        int cpuTemp;
        int gpuTemp;
    };

    explicit TemperatureMonitor(QObject *parent = nullptr);

    QVector<Reading> history() const { return m_history; }
    QVector<Reading> cpuHistory() const;
    QVector<Reading> gpuHistory() const;

    int maxCpuTemp() const;
    int maxGpuTemp() const;
    double avgCpuTemp() const;
    double avgGpuTemp() const;

    int historySize() const { return m_historySize; }
    void setHistorySize(int size);

public slots:
    void addReading(int cpuTemp, int gpuTemp);
    void clear();

signals:
    void historySizeChanged(int size);
    void readingAdded(int cpuTemp, int gpuTemp);

private:
    QVector<Reading> m_history;
    int m_historySize;
};

} // namespace msi::core
