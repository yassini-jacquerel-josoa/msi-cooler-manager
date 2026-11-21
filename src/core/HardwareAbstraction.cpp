#include "HardwareAbstraction.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

constexpr int kNvidiaTimeoutMs = 2000;
constexpr qint64 kDiscoveryRetryMs = 60000;
constexpr int kMaxTempMilliC = 200000;   // 200 °C : au-delà, la valeur est aberrante
constexpr int kMaxNvidiaTempC = 150;     // nvidia-smi renvoie déjà des degrés entiers

QString readFileText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QByteArray data = file.readAll().trimmed();
    file.close();
    return QString::fromUtf8(data);
}

bool readMilliCelsius(const QString &path, int &celsius, QString &error)
{
    celsius = -1;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("lecture de %1 impossible (%2)").arg(path, file.errorString());
        return false;
    }
    const QByteArray data = file.readAll().trimmed();
    file.close();

    bool ok = false;
    const int raw = data.toInt(&ok);
    if (!ok || raw <= 0 || raw > kMaxTempMilliC) {
        error = QStringLiteral("valeur invalide lue dans %1 : « %2 »")
                    .arg(path, QString::fromUtf8(data));
        return false;
    }

    celsius = raw / 1000; // sysfs : millidegrés Celsius
    return true;
}

bool looksLikeGpuHwmon(const QString &dir)
{
    const QString name = readFileText(dir + QStringLiteral("/name")).toLower();

    // k10temp/coretemp sont des capteurs CPU, même si le bus PCI est celui d'un GPU.
    if (name.contains(QStringLiteral("k10temp")) ||
        name.contains(QStringLiteral("coretemp")) ||
        name.contains(QStringLiteral("fam15h_power")))
        return false;

    QString driver;
    const QString uevent = readFileText(dir + QStringLiteral("/device/uevent"));
    const QStringList lines = uevent.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("DRIVER="))) {
            driver = line.mid(7).trimmed().toLower();
            break;
        }
    }

    static const QStringList kGpuDrivers{
        QStringLiteral("amdgpu"), QStringLiteral("radeon"), QStringLiteral("nouveau"),
        QStringLiteral("nvidia"), QStringLiteral("i915"),   QStringLiteral("xe")
    };
    for (const QString &key : kGpuDrivers) {
        if (name.contains(key) || driver.contains(key))
            return true;
    }

    // Repli prudent pour AMD/NVIDIA : fournisseur PCI GPU et capteur non CPU.
    const QString vendor = readFileText(dir + QStringLiteral("/device/vendor")).toLower();
    if (vendor == QLatin1String("0x1002") || vendor == QLatin1String("0x10de"))
        return name.isEmpty() || (!name.contains(QStringLiteral("cpu")) &&
                                  !name.contains(QStringLiteral("k10")));
    return false;
}

QStringList hwmonTemperatureInputs(const QString &dir)
{
    QMap<int, QString> ordered; // temp1_input avant temp2_input, même au-delà de 9
    const QDir directory(dir);
    static const QRegularExpression pattern(QStringLiteral("^temp([0-9]+)_input$"));

    const QStringList names = directory.entryList({QStringLiteral("temp*_input")},
                                                  QDir::Files, QDir::Name);
    for (const QString &name : names) {
        const QRegularExpressionMatch match = pattern.match(name);
        if (!match.hasMatch())
            continue;
        bool ok = false;
        const int index = match.captured(1).toInt(&ok);
        if (ok)
            ordered.insert(index, directory.filePath(name));
    }
    return ordered.values();
}

void stopProcess(QProcess &process)
{
    if (process.state() == QProcess::NotRunning)
        return;
    process.kill();
    if (!process.waitForFinished(1000)) {
        process.kill();
        process.waitForFinished(1000);
    }
}

} // namespace

namespace msi::core {

QString SysfsHardware::discoverCpuZone() const
{
    const QDir thermalRoot(QStringLiteral("/sys/class/thermal"));
    const QStringList zones = thermalRoot.entryList({QStringLiteral("thermal_zone*")},
                                                    QDir::Dirs | QDir::NoDotAndDotDot,
                                                    QDir::Name);

    // Noms de zones rencontrés sur les plateformes x86 MSI (comparaison insensible à la casse).
    static const QStringList kCpuTypes{
        QStringLiteral("x86_pkg_temp"), QStringLiteral("cpu_thermal"),
        QStringLiteral("k10temp"),      QStringLiteral("TCPU")
    };

    for (const QString &zone : zones) {
        const QString dir = thermalRoot.filePath(zone);
        const QString type = readFileText(dir + QStringLiteral("/type"));
        if (type.isEmpty())
            continue;

        bool cpuType = false;
        for (const QString &candidate : kCpuTypes) {
            if (type.compare(candidate, Qt::CaseInsensitive) == 0) {
                cpuType = true;
                break;
            }
        }
        if (cpuType && QFileInfo::exists(dir + QStringLiteral("/temp")))
            return dir;
    }

    // Compatibilité avec l'ancien comportement : thermal_zone0 reste le repli.
    const QString fallback = thermalRoot.filePath(QStringLiteral("thermal_zone0"));
    return QFileInfo::exists(fallback + QStringLiteral("/temp")) ? fallback : QString();
}

bool SysfsHardware::queryNvidiaSmi(int &temperatureC, QString &error) const
{
    temperatureC = -1;
    error.clear();

    const QString program = QStandardPaths::findExecutable(QStringLiteral("nvidia-smi"));
    if (program.isEmpty()) {
        error = QStringLiteral("nvidia-smi introuvable dans PATH");
        return false;
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments({QStringLiteral("--query-gpu=temperature.gpu"),
                          QStringLiteral("--format=csv,noheader,nounits")});
    process.start();

    if (!process.waitForStarted(kNvidiaTimeoutMs)) {
        error = QStringLiteral("nvidia-smi n'a pas démarré (%1)").arg(process.errorString());
        stopProcess(process); // évite tout processus résiduel
        return false;
    }
    if (!process.waitForFinished(kNvidiaTimeoutMs)) {
        stopProcess(process); // timeout : on tue avant de détruire le QProcess
        error = QStringLiteral("nvidia-smi a dépassé le délai de 2 s (processus terminé)");
        return false;
    }

    const QByteArray stdoutData = process.readAllStandardOutput();
    const QByteArray stderrData = process.readAllStandardError();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString detail = QString::fromUtf8(stderrData).trimmed();
        error = QStringLiteral("nvidia-smi a quitté avec le code %1").arg(process.exitCode());
        if (!detail.isEmpty())
            error += QStringLiteral(" : ") + detail;
        return false;
    }

    const QString output = QString::fromUtf8(stdoutData).trimmed();
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) { // première valeur valide = premier GPU
        bool ok = false;
        const int value = line.trimmed().toInt(&ok);
        if (ok && value > 0 && value <= kMaxNvidiaTempC) {
            temperatureC = value;
            return true;
        }
    }

    error = QStringLiteral("sortie nvidia-smi inexploitable : « %1 »").arg(output);
    return false;
}

QString SysfsHardware::findGpuHwmonInput() const
{
    const QDir hwmonRoot(QStringLiteral("/sys/class/hwmon"));
    const QStringList entries = hwmonRoot.entryList({QStringLiteral("hwmon*")},
                                                    QDir::Dirs | QDir::NoDotAndDotDot,
                                                    QDir::Name);
    for (const QString &entry : entries) {
        const QString dir = hwmonRoot.filePath(entry);
        if (!looksLikeGpuHwmon(dir))
            continue;
        const QStringList inputs = hwmonTemperatureInputs(dir);
        if (!inputs.isEmpty())
            return inputs.constFirst();
    }
    return {};
}

void SysfsHardware::detectGpuBackend(int &initialTemperature) const
{
    initialTemperature = -1;
    m_gpuScanTimer.restart();

    QString nvidiaError;
    if (queryNvidiaSmi(initialTemperature, nvidiaError)) {
        // Un seul test d'existence/validité : le backend est mémorisé pour la session.
        m_gpuBackend = GpuBackend::NvidiaSmi;
        m_gpuStatus = QStringLiteral("GPU : température disponible via nvidia-smi.");
        recordError(m_gpuError, QString());
        return;
    }

    initialTemperature = -1; // le test nvidia-smi ne doit pas fournir de mesure
    const QString input = findGpuHwmonInput();
    if (!input.isEmpty()) {
        m_gpuBackend = GpuBackend::Hwmon;
        m_gpuInputPath = input;
        m_gpuStatus = QStringLiteral("GPU : capteur hwmon présomptif retenu (%1).").arg(input);
        recordError(m_gpuError, QString());
        return;
    }

    m_gpuBackend = GpuBackend::Unavailable;
    const QString message = QStringLiteral(
                                "GPU : nvidia-smi indisponible (%1) et aucun capteur hwmon GPU présomptif.")
                                .arg(nvidiaError);
    m_gpuStatus = message;
    recordError(m_gpuError, message);
}

void SysfsHardware::recordError(QString &field, const QString &message) const
{
    if (field == message)
        return;
    field = message;
    if (!message.isEmpty())
        qWarning().noquote() << message; // un seul avertissement par changement d'état
}

int SysfsHardware::cpuTemperature() const
{
    if (m_cpuZone.isEmpty()) {
        if (m_cpuScanTimer.isValid() && m_cpuScanTimer.elapsed() < kDiscoveryRetryMs) {
            const qint64 remaining = (kDiscoveryRetryMs - m_cpuScanTimer.elapsed()) / 1000;
            const QString message = QStringLiteral(
                                        "CPU : aucune zone valide ; nouvelle recherche dans %1 s.")
                                        .arg(remaining);
            m_cpuStatus = message;
            recordError(m_cpuError, message);
            return -1;
        }

        m_cpuScanTimer.restart();
        m_cpuZone = discoverCpuZone();
        if (m_cpuZone.isEmpty()) {
            const QString message = QStringLiteral(
                "CPU : aucune zone thermique exploitable dans /sys/class/thermal/thermal_zone* ; "
                "vérifiez les modules x86_pkg_temp, k10temp ou cpu_thermal.");
            m_cpuStatus = message;
            recordError(m_cpuError, message);
            return -1;
        }
    }

    int celsius = -1;
    QString error;
    const QString path = m_cpuZone + QStringLiteral("/temp");
    if (!readMilliCelsius(path, celsius, error)) {
        m_cpuZone.clear(); // la zone peut avoir disparu : nouvelle détection bornée
        const QString message = QStringLiteral("CPU : %1.").arg(error);
        m_cpuStatus = message;
        recordError(m_cpuError, message);
        return -1;
    }

    m_cpuStatus = QStringLiteral("CPU : température disponible (source : %1).").arg(path);
    recordError(m_cpuError, QString());
    return celsius;
}

int SysfsHardware::gpuTemperature() const
{
    int initialTemperature = -1;
    if (m_gpuBackend == GpuBackend::Unknown)
        detectGpuBackend(initialTemperature);
    if (initialTemperature > 0)
        return initialTemperature;

    int celsius = -1;
    QString error;

    switch (m_gpuBackend) {
    case GpuBackend::NvidiaSmi:
        if (queryNvidiaSmi(celsius, error)) {
            m_gpuStatus = QStringLiteral("GPU : température disponible via nvidia-smi.");
            recordError(m_gpuError, QString());
            return celsius;
        }
        {
            // nvidia-smi a déjà fonctionné : bascule unique vers hwmon, sans le relancer.
            const QString nvidiaFailure = error;
            const QString fallback = findGpuHwmonInput();
            if (!fallback.isEmpty()) {
                m_gpuInputPath = fallback;
                if (readMilliCelsius(fallback, celsius, error)) {
                    m_gpuBackend = GpuBackend::Hwmon;
                    m_gpuStatus = QStringLiteral(
                                      "GPU : température disponible via %1 (repli après échec nvidia-smi).")
                                      .arg(fallback);
                    recordError(m_gpuError, QString());
                    return celsius;
                }
                m_gpuInputPath.clear();
                const QString message = QStringLiteral(
                                            "GPU : nvidia-smi a échoué après détection (%1) ; repli hwmon illisible (%2).")
                                            .arg(nvidiaFailure, error);
                m_gpuBackend = GpuBackend::Unavailable;
                m_gpuScanTimer.restart();
                m_gpuStatus = message;
                recordError(m_gpuError, message);
                return -1;
            }
            m_gpuBackend = GpuBackend::Unavailable;
            m_gpuScanTimer.restart();
            const QString message = QStringLiteral(
                                        "GPU : nvidia-smi a échoué après détection (%1) ; "
                                        "aucun repli hwmon exploitable, nvidia-smi n'est pas relancé.")
                                        .arg(nvidiaFailure);
            m_gpuStatus = message;
            recordError(m_gpuError, message);
            return -1;
        }

    case GpuBackend::Hwmon:
        if (readMilliCelsius(m_gpuInputPath, celsius, error)) {
            m_gpuStatus = QStringLiteral(
                              "GPU : température disponible via %1 (capteur hwmon présomptif).")
                              .arg(m_gpuInputPath);
            recordError(m_gpuError, QString());
            return celsius;
        }
        m_gpuInputPath.clear();
        m_gpuBackend = GpuBackend::Unavailable;
        m_gpuScanTimer.restart();
        {
            const QString message = QStringLiteral("GPU : %1 ; capteur hwmon abandonné.").arg(error);
            m_gpuStatus = message;
            recordError(m_gpuError, message);
        }
        return -1;

    case GpuBackend::Unavailable:
        if (m_gpuScanTimer.isValid() && m_gpuScanTimer.elapsed() < kDiscoveryRetryMs) {
            const qint64 remaining = (kDiscoveryRetryMs - m_gpuScanTimer.elapsed()) / 1000;
            const QString message = QStringLiteral(
                                        "GPU : aucun capteur disponible ; réexamen hwmon dans %1 s "
                                        "(nvidia-smi n'est pas relancé).")
                                        .arg(remaining);
            m_gpuStatus = message;
            recordError(m_gpuError, message);
            return -1;
        }
        m_gpuScanTimer.restart();
        {
            const QString input = findGpuHwmonInput();
            if (input.isEmpty()) {
                const QString message = QStringLiteral(
                    "GPU : aucun capteur hwmon GPU présomptif ; nvidia-smi n'est pas relancé.");
                m_gpuStatus = message;
                recordError(m_gpuError, message);
                return -1;
            }
            m_gpuInputPath = input;
            if (!readMilliCelsius(input, celsius, error)) {
                const QString message = QStringLiteral("GPU : %1 ; aucun capteur exploitable.").arg(error);
                m_gpuStatus = message;
                recordError(m_gpuError, message);
                return -1;
            }
            m_gpuBackend = GpuBackend::Hwmon;
            m_gpuStatus = QStringLiteral(
                              "GPU : température disponible via %1 (capteur hwmon présomptif).")
                              .arg(input);
            recordError(m_gpuError, QString());
            return celsius;
        }

    case GpuBackend::Unknown: // déjà détecté au début de la méthode
        break;
    }

    return -1;
}

bool SysfsHardware::coolerBoostState() const
{
    const QString path = QStringLiteral("/sys/devices/platform/msi-ec/cooler_boost");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString message = QStringLiteral(
                                    "Cooler Boost : lecture de %1 impossible (%2) ; "
                                    "vérifiez le module msi-ec, l'attribut cooler_boost et les droits root.")
                                    .arg(path, file.errorString());
        m_coolerStatus = message;
        recordError(m_coolerError, message);
        return false;
    }
    const QByteArray data = file.readAll().trimmed();
    file.close();

    const QString value = QString::fromUtf8(data);
    if (value == QLatin1String("on") || value == QLatin1String("1")) {
        m_coolerStatus = QStringLiteral("Cooler Boost : état disponible (on, source : %1).").arg(path);
        recordError(m_coolerError, QString());
        return true;
    }
    if (value == QLatin1String("off") || value == QLatin1String("0")) {
        m_coolerStatus = QStringLiteral("Cooler Boost : état disponible (off, source : %1).").arg(path);
        recordError(m_coolerError, QString());
        return false;
    }

    const QString message = QStringLiteral(
                                "Cooler Boost : valeur inattendue « %1 » dans %2 (attendu : on/off/1/0).")
                                .arg(value, path);
    m_coolerStatus = message;
    recordError(m_coolerError, message);
    return false;
}

bool SysfsHardware::setCoolerBoost(bool enabled)
{
    const QString path = QStringLiteral("/sys/devices/platform/msi-ec/cooler_boost");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString message = QStringLiteral(
                                    "Cooler Boost : écriture de %1 impossible (%2) ; "
                                    "vérifiez msi-ec/cooler_boost et les droits root.")
                                    .arg(path, file.errorString());
        m_coolerStatus = message;
        recordError(m_coolerError, message);
        return false;
    }

    const QByteArray data = enabled ? "1" : "0";
    const qint64 written = file.write(data);
    if (written != static_cast<qint64>(data.size())) {
        const QString message = QStringLiteral(
                                    "Cooler Boost : écriture partielle dans %1 (%2/%3 octets).")
                                    .arg(path)
                                    .arg(written)
                                    .arg(data.size());
        m_coolerStatus = message;
        recordError(m_coolerError, message);
        return false;
    }
    if (!file.flush()) {
        const QString message = QStringLiteral(
                                    "Cooler Boost : vidage de l'écriture impossible sur %1 (%2).")
                                    .arg(path, file.errorString());
        m_coolerStatus = message;
        recordError(m_coolerError, message);
        return false;
    }
    file.close();

    m_coolerStatus = QStringLiteral("Cooler Boost : écriture de %1 réussie (source : %2).")
                         .arg(enabled ? QStringLiteral("on") : QStringLiteral("off"), path);
    recordError(m_coolerError, QString());
    return true;
}

QString SysfsHardware::status() const
{
    return QStringLiteral("%1 | %2 | %3")
        .arg(cpuStatus(), gpuStatus(), coolerBoostStatus());
}

QString SysfsHardware::lastError() const
{
    QStringList errors;
    if (!m_cpuError.isEmpty())
        errors << m_cpuError;
    if (!m_gpuError.isEmpty())
        errors << m_gpuError;
    if (!m_coolerError.isEmpty())
        errors << m_coolerError;
    return errors.isEmpty() ? QStringLiteral("Aucune erreur matérielle détectée.")
                            : errors.join(QStringLiteral(" | "));
}

} // namespace msi::core
