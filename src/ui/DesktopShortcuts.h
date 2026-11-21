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

    /**
     * @brief Restaure les raccourcis modifiés par claimAltF7()/installToggleCommand().
     *
     * Remet le raccourci GNOME « déplacer la fenêtre » tel qu'il était avant la
     * modification et retire uniquement le raccourci personnalisé créé par MSI
     * Cooler Manager. Les raccourcis des autres applications sont préservés.
     * Appelable plusieurs fois sans effet supplémentaire.
     */
    static void restore();
};

} // namespace msi::ui
