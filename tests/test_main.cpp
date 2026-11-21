#include <QtTest>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/CoolerBoostController.h"
#include "core/SettingsManager.h"
#include "core/TemperatureMonitor.h"
#include "MockHardware.h"

using msi::core::CoolerBoostController;
using msi::core::SettingsManager;
using msi::core::TemperatureMonitor;

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void autoThresholdCrossingActivatesBoost();
    void manualModeTogglesBoost();
    void toggleModeSwitchesBetweenModes();
    void restoreModeRestoresRequestedMode();
    void hardwareWriteFailureReportsError();
    void temperatureHistoryTrimsAt300Readings();
    void settingsManagerDefaults();
    void settingsManagerPersistsChanges();

private:
    QTemporaryDir m_settingsDir;
};

void TestCore::initTestCase()
{
    // Isoler QSettings pour ne pas modifier la configuration de l'utilisateur.
    QVERIFY(m_settingsDir.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    // SettingsManager utilise le constructeur (organisation, application),
    // qui force NativeFormat : rediriger également ce format.
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
}

void TestCore::autoThresholdCrossingActivatesBoost()
{
    auto hardware = std::make_unique<MockHardware>();
    MockHardware *mock = hardware.get();
    mock->setTemperatures(50, 40);
    mock->setCoolerBoostState(false);

    CoolerBoostController controller(std::move(hardware));
    controller.setCpuThreshold(80);
    controller.setGpuThreshold(70);

    QSignalSpy boostSpy(&controller, &CoolerBoostController::coolerBoostActiveChanged);
    QVERIFY(boostSpy.isValid());

    controller.refreshHardware();
    QCOMPARE(controller.cpuTemperature(), 50);
    QCOMPARE(controller.gpuTemperature(), 40);
    QVERIFY(!controller.isCoolerBoostActive());
    QCOMPARE(boostSpy.count(), 0);

    // Franchissement du seuil CPU.
    mock->setTemperatures(85, 40);
    controller.refreshHardware();
    QVERIFY(controller.isCoolerBoostActive());
    QVERIFY(mock->coolerBoostState());
    QCOMPARE(boostSpy.count(), 1);
    QCOMPARE(boostSpy.constFirst().constFirst().toBool(), true);

    // Retour sous le seuil : le boost doit être coupé.
    mock->setTemperatures(50, 40);
    controller.refreshHardware();
    QVERIFY(!controller.isCoolerBoostActive());
    QVERIFY(!mock->coolerBoostState());
    QCOMPARE(boostSpy.count(), 2);

    // Franchissement du seuil GPU seul.
    mock->setTemperatures(50, 75);
    controller.refreshHardware();
    QVERIFY(controller.isCoolerBoostActive());
    QVERIFY(mock->coolerBoostState());
    QCOMPARE(boostSpy.count(), 3);
}

void TestCore::manualModeTogglesBoost()
{
    auto hardware = std::make_unique<MockHardware>();
    MockHardware *mock = hardware.get();
    mock->setTemperatures(45, 35);

    CoolerBoostController controller(std::move(hardware));
    QSignalSpy modeSpy(&controller, &CoolerBoostController::modeChanged);
    QSignalSpy boostSpy(&controller, &CoolerBoostController::coolerBoostActiveChanged);

    controller.setMode(QStringLiteral("MANUEL"));
    QCOMPARE(controller.mode(), QStringLiteral("MANUEL"));
    QCOMPARE(controller.modeEnum(), CoolerBoostController::Mode::Manual);
    QVERIFY(controller.isCoolerBoostActive());
    QVERIFY(mock->coolerBoostState());
    QCOMPARE(modeSpy.count(), 1);

    controller.toggleCoolerBoost();
    QVERIFY(!controller.isCoolerBoostActive());
    QVERIFY(!mock->coolerBoostState());

    controller.toggleCoolerBoost();
    QVERIFY(controller.isCoolerBoostActive());
    QVERIFY(mock->coolerBoostState());

    // Le mode manuel ignore la température : un refresh ne doit rien changer.
    mock->setTemperatures(99, 99);
    controller.toggleCoolerBoost(); // coupe le boost
    controller.refreshHardware();
    QVERIFY(!controller.isCoolerBoostActive());
    QVERIFY(!mock->coolerBoostState());

    QVERIFY(boostSpy.count() >= 3);
}

void TestCore::toggleModeSwitchesBetweenModes()
{
    auto hardware = std::make_unique<MockHardware>();
    CoolerBoostController controller(std::move(hardware));

    QCOMPARE(controller.mode(), QStringLiteral("AUTO"));
    QSignalSpy modeSpy(&controller, &CoolerBoostController::modeChanged);

    controller.toggleMode();
    QCOMPARE(controller.mode(), QStringLiteral("MANUEL"));
    QCOMPARE(controller.modeEnum(), CoolerBoostController::Mode::Manual);

    controller.toggleMode();
    QCOMPARE(controller.mode(), QStringLiteral("AUTO"));
    QCOMPARE(controller.modeEnum(), CoolerBoostController::Mode::Auto);

    QCOMPARE(modeSpy.count(), 2);
    QCOMPARE(modeSpy.at(0).constFirst().toString(), QStringLiteral("MANUEL"));
    QCOMPARE(modeSpy.at(1).constFirst().toString(), QStringLiteral("AUTO"));
}

void TestCore::restoreModeRestoresRequestedMode()
{
    auto hardware = std::make_unique<MockHardware>();
    CoolerBoostController controller(std::move(hardware));
    QSignalSpy modeSpy(&controller, &CoolerBoostController::modeChanged);

    controller.setMode(CoolerBoostController::Mode::Manual);
    QCOMPARE(controller.mode(), QStringLiteral("MANUEL"));

    controller.restoreMode(CoolerBoostController::Mode::Auto);
    QCOMPARE(controller.mode(), QStringLiteral("AUTO"));
    QCOMPARE(controller.modeEnum(), CoolerBoostController::Mode::Auto);
    QCOMPARE(modeSpy.count(), 2);

    // Restaurer le mode courant ne doit pas émettre de signal.
    controller.restoreMode(CoolerBoostController::Mode::Auto);
    QCOMPARE(modeSpy.count(), 2);
}

void TestCore::hardwareWriteFailureReportsError()
{
    auto hardware = std::make_unique<MockHardware>();
    MockHardware *mock = hardware.get();
    mock->setWritesSucceed(false);
    mock->setTemperatures(95, 90);

    CoolerBoostController controller(std::move(hardware));
    QSignalSpy errorSpy(&controller, &CoolerBoostController::errorOccurred);

    controller.refreshHardware();
    QVERIFY(!controller.isCoolerBoostActive());
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(mock->writeCount() > 0);
}

void TestCore::temperatureHistoryTrimsAt300Readings()
{
    TemperatureMonitor monitor;
    QCOMPARE(monitor.historySize(), 300);
    QVERIFY(monitor.history().isEmpty());

    QSignalSpy addedSpy(&monitor, &TemperatureMonitor::readingAdded);
    for (int i = 0; i < 305; ++i)
        monitor.addReading(50 + i, 40 + i);

    // La fenêtre glissante conserve exactement les 300 dernières lectures.
    QCOMPARE(monitor.history().size(), 300);
    QCOMPARE(monitor.history().constFirst().cpuTemp, 55);
    QCOMPARE(monitor.history().constLast().cpuTemp, 354);
    QCOMPARE(monitor.maxCpuTemp(), 354);
    QCOMPARE(monitor.maxGpuTemp(), 344);
    QCOMPARE(addedSpy.count(), 305);

    monitor.clear();
    QVERIFY(monitor.history().isEmpty());

    monitor.setHistorySize(20);
    for (int i = 0; i < 25; ++i)
        monitor.addReading(60 + i, 50 + i);
    QCOMPARE(monitor.historySize(), 20);
    QCOMPARE(monitor.history().size(), 20);
    QCOMPARE(monitor.history().constFirst().cpuTemp, 65);
}

void TestCore::settingsManagerDefaults()
{
    SettingsManager settings;
    QVERIFY(settings.isDarkTheme());
    QCOMPARE(settings.defaultMode(), QStringLiteral("AUTO"));
    QCOMPARE(settings.pollIntervalMs(), 1000);
    QCOMPARE(settings.cpuThreshold(), 80);
    QCOMPARE(settings.gpuThreshold(), 70);
    QCOMPARE(settings.historySize(), 300);
}

void TestCore::settingsManagerPersistsChanges()
{
    {
        SettingsManager settings;
        settings.setDarkTheme(false);
        settings.setDefaultMode(QStringLiteral("MANUEL"));
        settings.setPollIntervalMs(250);
        settings.setCpuThreshold(75);
        settings.setGpuThreshold(65);
        settings.setHistorySize(120);
        settings.sync();
    }

    SettingsManager reloaded;
    QVERIFY(!reloaded.isDarkTheme());
    QCOMPARE(reloaded.defaultMode(), QStringLiteral("MANUEL"));
    QCOMPARE(reloaded.pollIntervalMs(), 250);
    QCOMPARE(reloaded.cpuThreshold(), 75);
    QCOMPARE(reloaded.gpuThreshold(), 65);
    QCOMPARE(reloaded.historySize(), 120);
}

QTEST_MAIN(TestCore)
#include "test_main.moc"
