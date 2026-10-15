#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

namespace msi::db {

/**
 * @brief Gestionnaire de base de données SQLite.
 *
 * Initialise et maintient la connexion à la base de données,
 * crée les tables au premier démarrage, et fournit des méthodes
 * pour stocker et récupérer l'historique des températures.
 */
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    bool initialize();
    bool isInitialized() const { return m_initialized; }

    bool insertTemperature(int cpuTemp, int gpuTemp, const QString &mode);
    bool cleanupOldEntries(int maxAgeDays);

    QString databasePath() const;

signals:
    void errorOccurred(QString message);
    void initializedSuccessfully();

private:
    bool createTables();
    bool openDatabase();

    QSqlDatabase m_db;
    QString m_dbPath;
    bool m_initialized;
    int m_insertsSinceCleanup = 0;
};

} // namespace msi::db
