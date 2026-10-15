#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

namespace msi::core {

/**
 * @brief Gestionnaire de paramètres persistants.
 *
 * Encapsule QSettings pour stocker les préférences utilisateur :
 * thème, seuils, mode par défaut, taille de l'historique.
 */
class SettingsManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool darkTheme READ isDarkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(QString defaultMode READ defaultMode WRITE setDefaultMode NOTIFY defaultModeChanged)
    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY pollIntervalChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() override;

    bool isDarkTheme() const;
    void setDarkTheme(bool dark);

    QString defaultMode() const;
    void setDefaultMode(const QString &mode);

    int pollIntervalMs() const;
    void setPollIntervalMs(int ms);

    int cpuThreshold() const;
    void setCpuThreshold(int t);

    int gpuThreshold() const;
    void setGpuThreshold(int t);

    int historySize() const;
    void setHistorySize(int size);

    void sync();

signals:
    void darkThemeChanged(bool dark);
    void defaultModeChanged(QString mode);
    void pollIntervalChanged(int ms);

private:
    QSettings m_settings;
};

} // namespace msi::core
