#include "TrayManager.h"

#include <QApplication>
#include <QIcon>

namespace msi::ui {

TrayManager::TrayManager(QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    m_trayIcon->setIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    m_trayIcon->setToolTip(QStringLiteral("MSI Cooler Manager"));

    setupMenu();

    m_trayIcon->setContextMenu(m_menu);
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &TrayManager::onActivated);
}

TrayManager::~TrayManager() = default;

void TrayManager::setupMenu()
{
    m_modeAction = new QAction(QStringLiteral("Mode: AUTO"), this);
    connect(m_modeAction, &QAction::triggered, this, &TrayManager::toggleModeRequested);
    m_menu->addAction(m_modeAction);

    m_menu->addSeparator();

    m_showAction = new QAction(QStringLiteral("Afficher"), this);
    connect(m_showAction, &QAction::triggered, this, &TrayManager::showWindowRequested);
    m_menu->addAction(m_showAction);

    m_quitAction = new QAction(QStringLiteral("Quitter"), this);
    connect(m_quitAction, &QAction::triggered, this, &TrayManager::quitRequested);
    m_menu->addAction(m_quitAction);
}

void TrayManager::show()
{
    m_trayIcon->show();
}

void TrayManager::notify(const QString &message)
{
    m_trayIcon->showMessage(QStringLiteral("MSI Cooler Manager"),
                            message,
                            QSystemTrayIcon::Information,
                            6000);
}

void TrayManager::hide()
{
    m_trayIcon->hide();
}

void TrayManager::updateTooltip(const QString &mode, int cpu, int gpu)
{
    m_trayIcon->setToolTip(QStringLiteral("MSI Cooler Manager\n"
                                         "Mode: %1\n"
                                         "CPU: %2°C | GPU: %3°C")
                               .arg(mode)
                               .arg(cpu)
                               .arg(gpu));
}

void TrayManager::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        emit toggleModeRequested();
    } else if (reason == QSystemTrayIcon::DoubleClick) {
        emit showWindowRequested();
    }
}

} // namespace msi::ui
