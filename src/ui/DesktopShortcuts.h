#pragma once

#include <QString>

namespace msi::ui {

/**
 * @brief Réassigne le raccourci Ubuntu qui entre en conflit avec F7.
 *
 * Alt+F7 (« Déplacer la fenêtre » dans Paramètres) est repris par
 * l'application pour changer le mode du Cooler Boost.
 */
class DesktopShortcuts
{
public:
    static QString claimAltF7(const QString &executablePath);
    static bool installToggleCommand(const QString &executablePath);
};

} // namespace msi::ui
