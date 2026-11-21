# Changelog

Toutes les modifications notables de ce projet sont documentées dans ce fichier.

Le format suit les recommandations de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/)
et le projet adhère au [versionnage sémantique](https://semver.org/lang/fr/).

## [Unreleased]

## [1.0.1] - 2026-11-21

### Added

- Script d'installation `install.sh` exécutable en une ligne (`curl -fsSL .../install.sh | sh`) : installation des dépendances via `apt`/`dnf`/`pacman`/`zypper`, téléchargement des sources de la release **`v1.0.1`** par défaut (option `--ref` pour une autre branche ou un tag), compilation, tests et installation, avec options `--ref`, `--prefix`, `--jobs`, `--skip-tests`, `--no-deps`, `--no-install`, `--keep-build` et `MSI_SHA256` (contrôle de l'archive).
- Vérification des modules QML requis à l'exécution (`check_qml_modules`), indépendante du gestionnaire de paquets.

### Fixed

- Installateur : les modules QML **`QtQuick.Templates`** et **`QtQml`** n'étaient pas installés (`--no-install-recommends` ne tire pas `qml6-module-qtquick-templates`, et `qml6-module-qtquick-controls` ne dépend que des bibliothèques C++) ; les types `QtQuick.Controls` étaient introuvables au chargement de l'interface (`module "QtQuick.Templates" plugin "qtquicktemplates2plugin" not found`).
- Compilation : les avertissements issus du code généré par `qmlcachegen` (Qt 6.4, `-Wshadow`/`-Wconversion`) ne masquent plus ceux du projet — 416 avertissements → 0, les flags stricts restant actifs sur `src/**` et `tests/**`.

### Known limitations

- `qml6-module-qtquick-templates` et `qml6-module-qtqml` ne sont installés explicitement que pour `apt` ; sur les autres gestionnaires de paquets, `check_qml_modules` avertit si le module manque.

## [1.0.0] - 2026-11-21

### Added

- Gestion des deux modes **AUTO** (seuils CPU/GPU configurables, 80 °C et 70 °C par défaut, bornes 30–100 °C) et **MANUEL** (forçage du Cooler Boost), avec persistance du mode et des seuils via `QSettings` (`src/core/CoolerBoostController.cpp`, `src/core/SettingsManager.cpp`).
- Surveillance des températures CPU (zone détectée dans `/sys/class/thermal/thermal_zone*`, repli `thermal_zone0`) et GPU (`nvidia-smi`, puis capteur `hwmon` présomptif) à une fréquence d'un relevé par seconde (`src/core/HardwareAbstraction.cpp:124-259`, `src/core/CoolerBoostController.cpp:19`).
- Historique glissant de 5 minutes (300 lectures), restauré depuis la base de données au démarrage (`src/core/TemperatureMonitor.cpp:9`, `src/main.cpp:145-157`).
- Persistance des mesures dans une base **SQLite** (`QStandardPaths::AppDataLocation`), avec rétention de 7 jours (`src/db/DatabaseManager.cpp:14-15`, `:40`, `:114-117`).
- Interface QML en vue compacte et vue étendue, thème clair/sombre avec détection du thème système GNOME et effet de flou d'arrière-plan (`src/ui/Main.qml`, `src/ui/ThemeManager.cpp`, `src/ui/BackdropBlur.cpp`).
- **Systray** avec menu « Mode », « Afficher » et « Quitter » (`src/ui/TrayManager.cpp:61-76`).
- **Raccourci global F7** via X11/xcb : appui court pour basculer de mode, maintien d'environ 2 s pour déployer la vue étendue ; installation d'un raccourci `Alt+F7` de repli sous GNOME (`src/ui/GlobalHotkey.cpp:191`, `src/ui/DesktopShortcuts.cpp:206-211`).
- **IPC** par `QLocalServer`/`QLocalSocket` garantissant une instance unique et acceptant les commandes `--toggle` (ou `-t`), `--auto`, `--manual`, `--expand` et `--quit` (`src/main.cpp:44-64`, `:171-192`).
- Lancement en root avec `sudo`, ou via `pkexec` avec le lanceur `.desktop` installé ; les commandes CLI sont transmises par IPC avant le contrôle root (`src/main.cpp:61-88`, `docs/msi-cooler-manager.desktop:5`).
- Suite de tests **Qt Test** pilotée par l'option CMake `BUILD_TESTS` (activée par défaut) et exécutable avec `ctest` (`CMakeLists.txt:118-129`, `tests/test_main.cpp`).
- Documentation de publication : `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md` et `LICENSE` (MIT).

### Known limitations

- Linux/X11 uniquement (XWayland pour les sessions Wayland) ; Windows n'est pas pris en charge (`src/main.cpp:12`, `:86`, `src/ui/GlobalHotkey.cpp:8`).
- L'interface et le contrôle matériel nécessitent les droits root ; seules les commandes IPC peuvent être envoyées par un utilisateur non root.
- Accès matériel spécifique à Linux : `/sys/devices/platform/msi-ec/cooler_boost` est le seul point de contrôle ; le capteur GPU `hwmon` est détecté par présomption et n'est pas validé sur toutes les machines (`src/core/HardwareAbstraction.cpp:52-87`, `:212-227`).
- Aucune courbe d'historique n'est affichée dans l'interface ; la base conserve 7 jours de mesures mais le tampon affiché reste limité à 5 minutes.
- La détection de thème dépend de `gsettings` (GNOME) ; le réglage manuel est écrasé au démarrage.
- Les tests unitaires fournis sont encore des squelettes (`tests/test_main.cpp:11-21`) ; la compatibilité matérielle documentée est décrite dans `docs/HARDWARE.md`.
