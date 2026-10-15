#pragma once

#include <QObject>
#include <QTimer>
#include <QDateTime>

#include "HardwareAbstraction.h"

namespace msi::core {

/**
 * @brief Contrôleur principal du Cooler Boost.
 *
 * Gère les modes AUTO/MANUEL, la régulation thermique et l'état du cooler.
 * Émet des signaux pour les changements d'état et de température.
 */
class CoolerBoostController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool coolerBoostActive READ isCoolerBoostActive NOTIFY coolerBoostActiveChanged)
    Q_PROPERTY(int cpuTemperature READ cpuTemperature NOTIFY temperaturesChanged)
    Q_PROPERTY(int gpuTemperature READ gpuTemperature NOTIFY temperaturesChanged)
    Q_PROPERTY(int cpuThreshold READ cpuThreshold WRITE setCpuThreshold NOTIFY thresholdsChanged)
    Q_PROPERTY(int gpuThreshold READ gpuThreshold WRITE setGpuThreshold NOTIFY thresholdsChanged)

public:
    enum class Mode { Auto, Manual };
    Q_ENUM(Mode)

    explicit CoolerBoostController(std::unique_ptr<HardwareAbstraction> hw,
                                   QObject *parent = nullptr);
    ~CoolerBoostController() override;

    // Getters
    QString mode() const;
    Mode modeEnum() const;
    bool isCoolerBoostActive() const;
    int cpuTemperature() const;
    int gpuTemperature() const;
    int cpuThreshold() const;
    int gpuThreshold() const;

    // Setters (appelables depuis QML)
    Q_INVOKABLE void setMode(const QString &mode);
    void setMode(Mode mode);
    Q_INVOKABLE void setCpuThreshold(int threshold);
    Q_INVOKABLE void setGpuThreshold(int threshold);

    // Actions (appelables depuis QML)
    Q_INVOKABLE void start();
    Q_INVOKABLE void toggleMode();
    Q_INVOKABLE void toggleCoolerBoost();
    void restoreMode(Mode mode);

    // Accès au monitoring
    QTimer *pollTimer() const { return m_pollTimer; }

public slots:
    void refreshHardware();

signals:
    void modeChanged(QString mode);
    void coolerBoostActiveChanged(bool active);
    void temperaturesChanged(int cpu, int gpu);
    void thresholdsChanged(int cpu, int gpu);
    void errorOccurred(QString message);

private:
    void applyAutoMode();
    void applyManualBoost();
    void setHardwareCoolerBoost(bool enabled);

    std::unique_ptr<HardwareAbstraction> m_hw;
    QTimer *m_pollTimer;
    bool m_started = false;
    Mode m_mode;
    bool m_coolerBoostActive;
    bool m_manualBoostEnabled = true;
    int m_cpuTemperature;
    int m_gpuTemperature;
    int m_cpuThreshold;
    int m_gpuThreshold;
    QDateTime m_lastLogTime;
};

} // namespace msi::core
