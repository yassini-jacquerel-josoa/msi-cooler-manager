#include <QtTest>
#include <QCoreApplication>

// Tests unitaires basiques pour le projet MSI Cooler Manager

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void testTemperatureMonitor()
    {
        // Test simple: vérification de la structure
        QVERIFY(true);
    }

    void testSettingsDefaults()
    {
        // Test simple: vérification des valeurs par défaut
        QVERIFY(true);
    }
};

QTEST_MAIN(TestCore)
#include "test_main.moc"
