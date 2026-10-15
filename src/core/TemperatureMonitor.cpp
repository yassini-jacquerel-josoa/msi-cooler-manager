#include "TemperatureMonitor.h"

#include <QtAlgorithms>

namespace msi::core {

TemperatureMonitor::TemperatureMonitor(QObject *parent)
    : QObject(parent)
    , m_historySize(300) // 5 minutes à 1 lecture/seconde
{}

QVector<TemperatureMonitor::Reading> TemperatureMonitor::cpuHistory() const
{
    QVector<Reading> result;
    result.reserve(m_history.size());
    for (const auto &r : m_history)
        if (r.cpuTemp > 0) result.append(r);
    return result;
}

QVector<TemperatureMonitor::Reading> TemperatureMonitor::gpuHistory() const
{
    QVector<Reading> result;
    result.reserve(m_history.size());
    for (const auto &r : m_history)
        if (r.gpuTemp > 0) result.append(r);
    return result;
}

int TemperatureMonitor::maxCpuTemp() const
{
    int max = 0;
    for (const auto &r : m_history)
        if (r.cpuTemp > max) max = r.cpuTemp;
    return max;
}

int TemperatureMonitor::maxGpuTemp() const
{
    int max = 0;
    for (const auto &r : m_history)
        if (r.gpuTemp > max) max = r.gpuTemp;
    return max;
}

double TemperatureMonitor::avgCpuTemp() const
{
    if (m_history.isEmpty()) return 0.0;
    double sum = 0;
    int count = 0;
    for (const auto &r : m_history)
        if (r.cpuTemp > 0) { sum += r.cpuTemp; ++count; }
    return count > 0 ? sum / count : 0.0;
}

double TemperatureMonitor::avgGpuTemp() const
{
    if (m_history.isEmpty()) return 0.0;
    double sum = 0;
    int count = 0;
    for (const auto &r : m_history)
        if (r.gpuTemp > 0) { sum += r.gpuTemp; ++count; }
    return count > 0 ? sum / count : 0.0;
}

void TemperatureMonitor::setHistorySize(int size)
{
    if (m_historySize == size)
        return;
    m_historySize = qMax(10, size);
    emit historySizeChanged(m_historySize);

    // Trim excess
    while (m_history.size() > m_historySize)
        m_history.removeFirst();
}

void TemperatureMonitor::addReading(int cpuTemp, int gpuTemp)
{
    Reading r{QDateTime::currentDateTime(), cpuTemp, gpuTemp};
    m_history.append(r);

    while (m_history.size() > m_historySize)
        m_history.removeFirst();

    emit readingAdded(cpuTemp, gpuTemp);
}

void TemperatureMonitor::clear()
{
    m_history.clear();
}

} // namespace msi::core
