#include "SettingsManager.h"

namespace msi::core {

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("MSI"), QStringLiteral("CoolerManager"))
{}

SettingsManager::~SettingsManager() = default;

bool SettingsManager::isDarkTheme() const
{
    return m_settings.value(QStringLiteral("theme/dark"), true).toBool();
}

void SettingsManager::setDarkTheme(bool dark)
{
    if (isDarkTheme() == dark)
        return;
    m_settings.setValue(QStringLiteral("theme/dark"), dark);
    emit darkThemeChanged(dark);
}

QString SettingsManager::defaultMode() const
{
    return m_settings.value(QStringLiteral("mode/default"), QStringLiteral("AUTO")).toString();
}

void SettingsManager::setDefaultMode(const QString &mode)
{
    if (defaultMode() == mode)
        return;
    m_settings.setValue(QStringLiteral("mode/default"), mode);
    emit defaultModeChanged(mode);
}

int SettingsManager::pollIntervalMs() const
{
    return m_settings.value(QStringLiteral("monitor/intervalMs"), 1000).toInt();
}

void SettingsManager::setPollIntervalMs(int ms)
{
    if (pollIntervalMs() == ms)
        return;
    m_settings.setValue(QStringLiteral("monitor/intervalMs"), ms);
    emit pollIntervalChanged(ms);
}

int SettingsManager::cpuThreshold() const
{
    return m_settings.value(QStringLiteral("thresholds/cpu"), 80).toInt();
}

void SettingsManager::setCpuThreshold(int t)
{
    m_settings.setValue(QStringLiteral("thresholds/cpu"), t);
}

int SettingsManager::gpuThreshold() const
{
    return m_settings.value(QStringLiteral("thresholds/gpu"), 70).toInt();
}

void SettingsManager::setGpuThreshold(int t)
{
    m_settings.setValue(QStringLiteral("thresholds/gpu"), t);
}

int SettingsManager::historySize() const
{
    return m_settings.value(QStringLiteral("monitor/historySize"), 300).toInt();
}

void SettingsManager::setHistorySize(int size)
{
    m_settings.setValue(QStringLiteral("monitor/historySize"), size);
}

void SettingsManager::sync()
{
    m_settings.sync();
}

} // namespace msi::core
