#pragma once

#include "core/HardwareAbstraction.h"

/**
 * @brief Faux matériel utilisé par les tests unitaires du contrôleur.
 *
 * Toutes les lectures et écritures sont deterministes : aucun accès à sysfs
 * ou à nvidia-smi n'est effectué.
 */
class MockHardware final : public msi::core::HardwareAbstraction
{
public:
    int cpuTemperature() const override { return m_cpuTemperature; }
    int gpuTemperature() const override { return m_gpuTemperature; }
    bool coolerBoostState() const override { return m_coolerBoostActive; }

    bool setCoolerBoost(bool enabled) override
    {
        ++m_writeCount;
        if (!m_writesSucceed)
            return false;
        m_coolerBoostActive = enabled;
        return true;
    }

    void setTemperatures(int cpu, int gpu)
    {
        m_cpuTemperature = cpu;
        m_gpuTemperature = gpu;
    }

    void setCoolerBoostState(bool active) { m_coolerBoostActive = active; }
    void setWritesSucceed(bool succeed) { m_writesSucceed = succeed; }

    int writeCount() const { return m_writeCount; }

private:
    int m_cpuTemperature = 0;
    int m_gpuTemperature = 0;
    bool m_coolerBoostActive = false;
    bool m_writesSucceed = true;
    int m_writeCount = 0;
};
