#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QDateTime>

#include <memory>

#include "db/DatabaseManager.h"
#include "db/MetricsRepository.h"

using msi::db::DatabaseManager;
using msi::db::MetricsRepository;

class TestDb : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void databaseInitializeAndInsert();
    void databaseCleanupRemovesOldEntries();

private:
    static std::unique_ptr<QTemporaryDir> createDataDir();

    std::unique_ptr<QTemporaryDir> m_dataDir;
    QByteArray m_previousDataHome;
    bool m_hadDataHome = false;
};

std::unique_ptr<QTemporaryDir> TestDb::createDataDir()
{
    // La base de test réside dans /dev/shm (tmpfs : stockage en mémoire)
    // lorsque c'est possible, avec un repli sur un répertoire temporaire.
    const QString shm = QStringLiteral("/dev/shm");
    const QFileInfo shmInfo(shm);
    if (shmInfo.exists() && shmInfo.isDir() && shmInfo.isWritable()) {
        auto dir = std::make_unique<QTemporaryDir>(
            shm + QStringLiteral("/msi-cooler-manager-XXXXXX"));
        if (dir->isValid())
            return dir;
    }

    auto dir = std::make_unique<QTemporaryDir>();
    Q_ASSERT(dir->isValid());
    return dir;
}

void TestDb::initTestCase()
{
    m_hadDataHome = !qgetenv("XDG_DATA_HOME").isNull();
    m_previousDataHome = qgetenv("XDG_DATA_HOME");
}

void TestDb::init()
{
    // Une base neuve (et un nom d'application distinct) pour chaque cas de test.
    m_dataDir = createDataDir();
    QVERIFY(m_dataDir->isValid());
    QCoreApplication::setApplicationName(QStringLiteral("test_db_")
                                         + QString::fromUtf8(QTest::currentTestFunction()));
    qputenv("XDG_DATA_HOME", m_dataDir->path().toUtf8());
}

void TestDb::cleanup()
{
    // Chaque test utilise une connexion SQLite neuve (nom de connexion par défaut).
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection))
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    m_dataDir.reset();
}

void TestDb::cleanupTestCase()
{
    if (m_hadDataHome)
        qputenv("XDG_DATA_HOME", m_previousDataHome);
    else
        qunsetenv("XDG_DATA_HOME");
}

void TestDb::databaseInitializeAndInsert()
{
    DatabaseManager db;
    QVERIFY(!db.isInitialized());
    QVERIFY(!db.insertTemperature(60, 50, QStringLiteral("AUTO")));

    QVERIFY(db.initialize());
    QVERIFY(db.isInitialized());
    QVERIFY2(db.databasePath().startsWith(m_dataDir->path()),
             qPrintable(QStringLiteral("Base créée hors du répertoire de test : ")
                        + db.databasePath()));

    QVERIFY(db.insertTemperature(72, 61, QStringLiteral("AUTO")));
    QVERIFY(db.insertTemperature(73, 62, QStringLiteral("MANUEL")));

    QSqlDatabase handle = QSqlDatabase::database();
    QVERIFY(handle.isValid());
    QVERIFY(handle.isOpen());

    QSqlQuery query(handle);
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM temperature_log")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);
    query.finish();

    MetricsRepository repository(handle);
    QCOMPARE(repository.totalRecords(), 2);
    const QVector<MetricsRepository::MetricEntry> rows = repository.getHistory();
    QCOMPARE(rows.size(), 2);

    bool hasAuto = false;
    bool hasManual = false;
    for (const MetricsRepository::MetricEntry &row : rows) {
        hasAuto = hasAuto || row.mode == QStringLiteral("AUTO");
        hasManual = hasManual || row.mode == QStringLiteral("MANUEL");
    }
    QVERIFY(hasAuto);
    QVERIFY(hasManual);
}

void TestDb::databaseCleanupRemovesOldEntries()
{
    DatabaseManager db;
    QVERIFY(db.initialize());

    const qint64 oldTimestamp = QDateTime::currentSecsSinceEpoch() - (30 * 86400);
    {
        QSqlDatabase handle = QSqlDatabase::database();
        QSqlQuery query(handle);
        QVERIFY(query.prepare(QStringLiteral(
            "INSERT INTO temperature_log (timestamp, cpu_temp, gpu_temp, mode) "
            "VALUES (:ts, :cpu, :gpu, :mode)")));
        query.bindValue(QStringLiteral(":ts"), oldTimestamp);
        query.bindValue(QStringLiteral(":cpu"), 55);
        query.bindValue(QStringLiteral(":gpu"), 45);
        query.bindValue(QStringLiteral(":mode"), QStringLiteral("AUTO"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    }
    QVERIFY(db.insertTemperature(70, 60, QStringLiteral("AUTO")));

    const auto rowCount = []() -> int {
        QSqlDatabase handle = QSqlDatabase::database();
        QSqlQuery query(handle);
        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM temperature_log")) || !query.next())
            return -1;
        return query.value(0).toInt();
    };

    QCOMPARE(rowCount(), 2);
    QVERIFY(db.cleanupOldEntries(7));
    QCOMPARE(rowCount(), 1);

    QVERIFY(db.cleanupOldEntries(-1));
    QCOMPARE(rowCount(), 0);
}

QTEST_MAIN(TestDb)
#include "test_db.moc"
