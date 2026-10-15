#include "CoolerBoostController.h"

#include <QDebug>

namespace msi::core {

CoolerBoostController::CoolerBoostController(std::unique_ptr<HardwareAbstraction> hw,
                                             QObject *parent)
    : QObject(parent)
    , m_hw(std::move(hw))
    , m_pollTimer(new QTimer(this))
    , m_mode(Mode::Auto)
    , m_coolerBoostActive(false)
    , m_cpuTemperature(0)
    , m_gpuTemperature(0)
    , m_cpuThreshold(80)
    , m_gpuThreshold(70)
{
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, &CoolerBoostController::refreshHardware);
}

CoolerBoostController::~CoolerBoostController() = default;

QString CoolerBoostController::mode() const
{
    return m_mode == Mode::Auto ? QStringLiteral("AUTO") : QStringLiteral("MANUEL");
}

CoolerBoostController::Mode CoolerBoostController::modeEnum() const
{
    return m_mode;
}

bool CoolerBoostController::isCoolerBoostActive() const
{
    return m_coolerBoostActive;
}

int CoolerBoostController::cpuTemperature() const
{
    return m_cpuTemperature;
}

int CoolerBoostController::gpuTemperature() const
{
    return m_gpuTemperature;
}

int CoolerBoostController::cpuThreshold() const
{
    return m_cpuThreshold;
}

int CoolerBoostController::gpuThreshold() const
{
    return m_gpuThreshold;
}

void CoolerBoostController::setMode(const QString &mode)
{
    const Mode newMode = (mode.compare(QStringLiteral("AUTO"), Qt::CaseInsensitive) == 0)
                             ? Mode::Auto
                             : Mode::Manual;
    setMode(newMode);
}

void CoolerBoostController::setMode(Mode newMode)
{
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    emit modeChanged(mode());

    if (m_mode == Mode::Manual) {
        m_manualBoostEnabled = true;
        applyManualBoost();
    } else {
        refreshHardware();
    }
}

void CoolerBoostController::setCpuThreshold(int threshold)
{
    if (m_cpuThreshold == threshold)
        return;
    m_cpuThreshold = qBound(30, threshold, 100);
    emit thresholdsChanged(m_cpuThreshold, m_gpuThreshold);
}

void CoolerBoostController::setGpuThreshold(int threshold)
{
    if (m_gpuThreshold == threshold)
        return;
    m_gpuThreshold = qBound(30, threshold, 100);
    emit thresholdsChanged(m_cpuThreshold, m_gpuThreshold);
}

void CoolerBoostController::start()
{
    if (m_started) {
        refreshHardware();
        return;
    }

    m_started = true;
    m_pollTimer->start();
    refreshHardware();
}

void CoolerBoostController::restoreMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    emit modeChanged(this->mode());
}

void CoolerBoostController::toggleMode()
{
    setMode(m_mode == Mode::Auto ? Mode::Manual : Mode::Auto);
}

void CoolerBoostController::toggleCoolerBoost()
{
    if (m_mode != Mode::Manual) {
        setHardwareCoolerBoost(!m_coolerBoostActive);
        return;
    }

    m_manualBoostEnabled = !m_manualBoostEnabled;
    applyManualBoost();
}

void CoolerBoostController::refreshHardware()
{
    if (!m_hw) {
        emit errorOccurred(QStringLiteral("Hardware abstraction non disponible"));
        return;
    }

    int cpu = m_hw->cpuTemperature();
    int gpu = m_hw->gpuTemperature();
    bool active = m_hw->coolerBoostState();

    bool changed = false;
    if (cpu > 0) { m_cpuTemperature = cpu; changed = true; }
    if (gpu > 0) { m_gpuTemperature = gpu; changed = true; }

    if (changed)
        emit temperaturesChanged(m_cpuTemperature, m_gpuTemperature);

    if (m_mode == Mode::Manual) {
        if (m_coolerBoostActive != m_manualBoostEnabled) {
            m_coolerBoostActive = m_manualBoostEnabled;
            emit coolerBoostActiveChanged(m_coolerBoostActive);
        }
        return;
    }

    if (m_coolerBoostActive != active) {
        m_coolerBoostActive = active;
        emit coolerBoostActiveChanged(active);
    }

    applyAutoMode();
}

void CoolerBoostController::applyAutoMode()
{
    bool shouldActivate = (m_cpuTemperature >= m_cpuThreshold) ||
                          (m_gpuTemperature >= m_gpuThreshold);

    if (shouldActivate != m_coolerBoostActive) {
        setHardwareCoolerBoost(shouldActivate);
    }
}

void CoolerBoostController::applyManualBoost()
{
    setHardwareCoolerBoost(m_manualBoostEnabled);
    if (m_coolerBoostActive == m_manualBoostEnabled)
        return;

    m_coolerBoostActive = m_manualBoostEnabled;
    emit coolerBoostActiveChanged(m_coolerBoostActive);
}

void CoolerBoostController::setHardwareCoolerBoost(bool enabled)
{
    if (!m_hw) return;

    if (m_hw->setCoolerBoost(enabled)) {
        m_coolerBoostActive = enabled;
        emit coolerBoostActiveChanged(enabled);
    } else {
        emit errorOccurred(QStringLiteral("Impossible d'écrire dans sysfs. "
                                         "Vérifiez les permissions sudo."));
    }
}

} // namespace msi::core
