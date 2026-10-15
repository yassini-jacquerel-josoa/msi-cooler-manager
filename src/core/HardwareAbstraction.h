#pragma once

#include <QString>
#include <QFile>
#include <QProcess>
#include <QTextStream>

namespace msi::core {

/**
 * @brief Interface d'abstraction matérielle.
 *
 * Fournit un accès uniforme aux capteurs et actionnaires matériels,
 * permettant le mocking pour les tests unitaires.
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
 * Lit les températures depuis /sys/class/thermal et nvidia-smi,
 * contrôle le cooler boost via /sys/devices/platform/msi-ec/cooler_boost.
 */
class SysfsHardware : public HardwareAbstraction
{
public:
    SysfsHardware() = default;
    ~SysfsHardware() override = default;

    int cpuTemperature() const override
    {
        // Lit /sys/class/thermal/thermal_zone0/temp (millidegrés)
        QFile file(QStringLiteral("/sys/class/thermal/thermal_zone0/temp"));
        if (!file.open(QIODevice::ReadOnly))
            return -1;

        QByteArray data = file.readAll().trimmed();
        file.close();

        bool ok = false;
        int raw = data.toInt(&ok);
        return ok ? raw / 1000 : -1;
    }

    int gpuTemperature() const override
    {
        QProcess proc;
        proc.start(QStringLiteral("nvidia-smi"),
                   QStringList{QStringLiteral("--query-gpu=temperature.gpu"),
                               QStringLiteral("--format=csv,noheader,nounits")});
        if (!proc.waitForFinished(2000))
            return -1;

        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        bool ok = false;
        int temp = out.toInt(&ok);
        return ok ? temp : -1;
    }

    bool coolerBoostState() const override
    {
        QFile file(QStringLiteral("/sys/devices/platform/msi-ec/cooler_boost"));
        if (!file.open(QIODevice::ReadOnly))
            return false;

        QByteArray data = file.readAll().trimmed();
        file.close();
        return data == "on" || data == "1";
    }

    bool setCoolerBoost(bool enabled) override
    {
        QFile file(QStringLiteral("/sys/devices/platform/msi-ec/cooler_boost"));
        if (!file.open(QIODevice::WriteOnly))
            return false;

        QByteArray value = enabled ? "1" : "0";
        qint64 written = file.write(value);
        file.close();
        return written == value.size();
    }
};

} // namespace msi::core
