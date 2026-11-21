#pragma once

#include <QElapsedTimer>
#include <QString>

namespace msi::core {

/**
 * @brief Interface d'abstraction matérielle.
 *
 * Fournit un accès uniforme aux capteurs et actionnaires matériels,
 * permettant le mocking pour les tests unitaires.
 *
 * Cette interface publique est volontairement inchangée : les contrôleurs
 * et les mocks existants ne dépendent pas des détails d'implémentation sysfs.
 */
class HardwareAbstraction
{
public:
    virtual ~HardwareAbstraction() = default;

    virtual int cpuTemperature() const = 0;
    virtual int gpuTemperature() const = 0;

    virtual bool coolerBoostState() const = 0;
    virtual bool setCoolerBoost(bool enabled) = 0;
};

/**
 * @brief Implémentation sysfs pour Linux.
 *
 * Sources utilisées (voir docs/HARDWARE.md) :
 *  - CPU : zone thermique détectée dans /sys/class/thermal/thermal_zone* ;
 *  - GPU : nvidia-smi si disponible, sinon capteur hwmon présomptif ;
 *  - Cooler Boost : /sys/devices/platform/msi-ec/cooler_boost.
 *
 * Les méthodes virtuelles conservent leur contrat historique (-1/false en cas
 * d'échec) pour ne pas perturber CoolerBoostController. Les accesseurs de
 * diagnostic ci-dessous (hors de l'interface virtuelle) expliquent l'échec.
 */
class SysfsHardware : public HardwareAbstraction
{
public:
    SysfsHardware() = default;
    ~SysfsHardware() override = default;

    int cpuTemperature() const override;
    int gpuTemperature() const override;
    bool coolerBoostState() const override;
    bool setCoolerBoost(bool enabled) override;

    // ── Diagnostics (spécifiques à SysfsHardware, hors interface virtuelle) ─
    /** Résumé lisible des trois sous-systèmes, séparés par « | ». */
    QString status() const;
    /** Erreurs courantes concaténées ; message explicite si aucune erreur. */
    QString lastError() const;

    QString cpuStatus() const { return m_cpuStatus; }
    QString gpuStatus() const { return m_gpuStatus; }
    QString coolerBoostStatus() const { return m_coolerStatus; }

private:
    enum class GpuBackend { Unknown, NvidiaSmi, Hwmon, Unavailable };

    QString discoverCpuZone() const;
    void detectGpuBackend(int &initialTemperature) const;
    QString findGpuHwmonInput() const;
    bool queryNvidiaSmi(int &temperatureC, QString &error) const;
    void recordError(QString &field, const QString &message) const;

    mutable QString m_cpuZone;
    mutable QString m_cpuStatus = QStringLiteral("CPU : non interrogé.");
    mutable QString m_cpuError;
    mutable QElapsedTimer m_cpuScanTimer;

    mutable GpuBackend m_gpuBackend = GpuBackend::Unknown;
    mutable QString m_gpuInputPath;
    mutable QString m_gpuStatus = QStringLiteral("GPU : non interrogé.");
    mutable QString m_gpuError;
    mutable QElapsedTimer m_gpuScanTimer;

    mutable QString m_coolerStatus = QStringLiteral("Cooler Boost : non interrogé.");
    mutable QString m_coolerError;
};

} // namespace msi::core
