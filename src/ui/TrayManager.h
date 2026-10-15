#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

namespace msi::ui {

/**
 * @brief Gestionnaire de zone de notification.
 *
 * Affiche une icône dans le systray avec un menu contextuel
 * pour contrôler le cooler boost sans ouvrir la fenêtre.
 */
class TrayManager : public QObject
{
    Q_OBJECT

public:
    explicit TrayManager(QObject *parent = nullptr);
    ~TrayManager() override;

    void show();
    void hide();
    void notify(const QString &message);
    void updateTooltip(const QString &mode, int cpu, int gpu);

signals:
    void toggleModeRequested();
    void showWindowRequested();
    void quitRequested();

private:
    void setupMenu();
    void onActivated(QSystemTrayIcon::ActivationReason reason);

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QAction *m_modeAction;
    QAction *m_showAction;
    QAction *m_quitAction;
};

} // namespace msi::ui
