# MSI Cooler Manager

Gestionnaire de Cooler Boost pour laptops MSI — Qt6/QML, C++20.

> **v1.0.1** : Linux uniquement (X11, ou XWayland) ; le démarrage d'une nouvelle instance exige les droits root. Voir [Limitations connues](#limitations-connues) et [docs/HARDWARE.md](docs/HARDWARE.md) pour la compatibilité matérielle documentée.

## Démonstration

![Démonstration de l'interface : panneau compact, déploiement de la vue étendue avec températures et seuils CPU/GPU, puis passage en mode MANUEL](resources/demo.gif)

Capture réelle de l'interface (24 s, 1920 × 1080) : [voir la vidéo WebM](resources/demo.webm).

## Fonctionnalités

- **Mode AUTO** : régulation thermique automatique selon des seuils CPU/GPU configurables (80 °C / 70 °C par défaut, bornes 30–100 °C)
- **Mode MANUEL** : activation ou coupure forcée du Cooler Boost
- **Surveillance temps réel** : températures lues chaque seconde, avec historique glissant de 5 minutes (300 lectures) restauré depuis la base de données
- **Thème clair/sombre** avec détection automatique du thème système (via `gsettings` GNOME, repli sur la palette Qt)
- **Systray** avec menu contextuel (mode, affichage, quitter)
- **Raccourci global F7** : appui court pour changer de mode, maintien d'environ 2 s pour déployer la vue étendue ; repli sur `Alt+F7` sous GNOME si la capture X11 échoue
- **Base de données SQLite** pour l'historique des températures (rétention de 7 jours)
- **IPC** : une seule instance, via `QLocalServer`/`QLocalSocket` ; les commandes CLI sont transmises à l'instance en cours

## Prérequis

### Système et matériel

- **Linux avec session X11** (ou XWayland) : l'éditeur de liens rajoute `xcb` et `X11` de façon inconditionnelle (`CMakeLists.txt:91-92`) et le raccourci global utilise X11/xcb (`src/ui/GlobalHotkey.cpp:8`, `:58`). **Windows n'est pas pris en charge** : `src/main.cpp:12` inclut `<unistd.h>` et `src/main.cpp:86` appelle `geteuid()`.
- **Droits root** pour démarrer une nouvelle instance : le contrôle de l'UID effectif intervient après la gestion de l'IPC. Le socket local est créé en 0600 et chaque connexion est authentifiée : seuls root et l'utilisateur de la session qui a lancé l'instance (UID transmis par `sudo`/`pkexec` via `SUDO_UID` ou `PKEXEC_UID`) peuvent envoyer des commandes ; `quit` est réservé à root. Le fichier `.desktop` fourni utilise `pkexec` (`docs/msi-cooler-manager.desktop:5`).
- **Module noyau `msi-ec`** exposant `/sys/devices/platform/msi-ec/cooler_boost` en lecture et en écriture ; c'est le seul point d'accès au Cooler Boost (`src/core/HardwareAbstraction.cpp`).
- **Capteur CPU** : une zone thermique est détectée dans `/sys/class/thermal/thermal_zone*` (types `x86_pkg_temp`, `cpu_thermal`, `k10temp`, `TCPU`), avec repli sur `thermal_zone0` (`src/core/HardwareAbstraction.cpp:124-157`) ; les valeurs sysfs sont en millidegrés.
- **Température GPU** : `nvidia-smi` est essayé en premier, puis un capteur `hwmon` présomptif de GPU est recherché dans `/sys/class/hwmon/` (`src/core/HardwareAbstraction.cpp:159-259`). Si aucune source n'est disponible, les lectures GPU restent indisponibles et n'alimentent pas l'historique.
- **GNOME** (optionnel) : `gsettings` est utilisé pour détecter le thème (`src/ui/ThemeManager.cpp:131-137`) et pour le raccourci de repli `Alt+F7` (`src/ui/DesktopShortcuts.cpp:196-211`).

La liste des configurations et des retours matériels documentés est centralisée dans [docs/HARDWARE.md](docs/HARDWARE.md).

### Outils de compilation

- CMake **3.21 minimum** (`CMakeLists.txt:1`)
- Compilateur **C++20** : GCC 11+ / Clang 14+ (le support MSVC de `CMakeLists.txt:31-33` ne concerne que les options d'avertissement et ne rend pas le projet compilable sous Windows)
- SQLite3 (`CMakeLists.txt:23`)
- Qt **6.4 minimum imposé** par CMake (`CMakeLists.txt:22`) ; **Qt 6.5+ recommandé** pour profiter des correctifs des versions récentes, sans être requis
- Modules Qt requis : `Core`, `Gui`, `Widgets`, `Qml`, `Quick`, `QuickControls2`, `Sql` (`CMakeLists.txt:22`) et `Test` pour les tests (`CMakeLists.txt:120`)
- Paquets de développement X11 : `libxcb*` (au minimum `libxcb1-dev`), `libx11-dev`, `libxkbcommon-dev` ; le plugin xcb de Qt peut également réclamer des paquets d'exécution xcb supplémentaires

Exemple sur Debian/Ubuntu :

```bash
sudo apt install cmake build-essential qt6-base-dev qt6-declarative-dev \
                 libsqlite3-dev libxcb1-dev libx11-dev libxkbcommon-dev
```

## Compilation

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

Les tests sont activés par défaut (`-DBUILD_TESTS=ON`) ; pour les désactiver : `-DBUILD_TESTS=OFF`.

## Tests

Toujours depuis le répertoire `build` :

```bash
ctest --output-on-failure
```

## Installation

### Télécharger, inspecter, exécuter

Le script `install.sh` installe les dépendances, récupère les sources de la **release `v1.0.1`** (référence par défaut), compile, exécute les tests et installe. Téléchargez-le, lisez-le, puis exécutez-le :

```bash
curl -fsSL https://raw.githubusercontent.com/yassini-jacquerel-josoa/msi-cooler-manager/main/install.sh -o install.sh
less install.sh          # inspecter le script avant de l'exécuter
sh install.sh
```

Le script étant versionné, il peut aussi être épinglé sur la release :

```bash
curl -fsSL https://raw.githubusercontent.com/yassini-jacquerel-josoa/msi-cooler-manager/v1.0.1/install.sh -o install.sh
sh install.sh
```

Options utiles (à passer à `sh install.sh`) :

```bash
sh install.sh --ref main      # dernière version de développement
sh install.sh --skip-tests    # ne pas exécuter ctest
sh install.sh --no-install    # compiler sans installer (aucun sudo)
sh install.sh --help          # toutes les options
```

Le script télécharge aussi le fichier `SHA256SUMS` publié avec la release et vérifie l'archive avant extraction ; une somme fournie explicitement via `MSI_SHA256` reste prioritaire. Le repli `git clone` reste utilisable mais n'offre pas cette vérification : un avertissement est alors affiché.

Le script est en POSIX sh (dash/bash/ash), détecte le gestionnaire de paquets (`apt`, `dnf`, `pacman`, `zypper`), télécharge les sources dans un répertoire temporaire (nettoyé à la fin) et vérifie la présence de `msi-ec`. Depuis un clone local, `./install.sh` utilise directement l'arbre de travail. Sans gestionnaire de paquets reconnu ou sans `sudo`, il affiche les prérequis du README et s'arrête.

### Manuellement

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
sudo cmake --install .
```

Dans les deux cas, le lanceur `msi-cooler-manager.desktop` est installé dans `share/applications` (`CMakeLists.txt:132-133`).

## Utilisation

### Démarrage de l'interface

Le démarrage d'une nouvelle instance exige les droits root (`src/main.cpp:81-88`) :

```bash
sudo msi-cooler-manager
# équivalent au lanceur .desktop :
pkexec msi-cooler-manager
```

### Commandes CLI

Une seule instance peut tourner (`msi_cooler_manager_ipc`, `src/main.cpp`). Chaque connexion au socket local est authentifiée via `SO_PEERCRED` : l'identité du pair doit être root ou l'UID de la session qui a lancé l'instance (`SUDO_UID` / `PKEXEC_UID`) ; toute autre connexion est refusée et journalisée. Le socket est créé directement en 0600 (`umask 0177`) et appartient à l'utilisateur de session, auquel root conserve toujours l'accès. La commande `quit` est réservée à root. Sans instance active, `sudo` est obligatoire et la commande est appliquée au démarrage de l'interface :

```bash
sudo msi-cooler-manager --toggle   # ou -t : bascule AUTO / MANUEL
sudo msi-cooler-manager --auto     # passer en mode AUTO
sudo msi-cooler-manager --manual   # passer en mode MANUEL
sudo msi-cooler-manager --expand   # déployer la vue étendue
sudo msi-cooler-manager --quit     # quitter l'instance en cours
```

### Raccourcis et systray

- **F7 court** : affiche le panneau et change de mode ; **F7 maintenu ~2 s** : affiche la vue étendue (`src/ui/GlobalHotkey.cpp:191`, `src/ui/Main.qml:286`) ; un appui long non disponible via X11 peut être remplacé par `Alt+F7` sous GNOME.
- **Échap** : replie la vue étendue puis masque le panneau (`src/ui/Main.qml:313-318`) ; le panneau compact se masque aussi après 3 s d'inactivité (`src/ui/Main.qml:197-205`).
- **Systray** : « Mode » bascule le mode, « Afficher » rouvre le panneau, « Quitter » arrête l'application (`src/ui/TrayManager.cpp:61-76`).

## Données et paramètres

- **Paramètres** : `QSettings` avec l'organisation « MSI » et l'application « CoolerManager » (`src/core/SettingsManager.cpp:7`) — mode par défaut, seuils, thème et taille d'historique ; emplacement fourni par Qt via `QStandardPaths` (typiquement `~/.config/MSI/CoolerManager.conf` sous Linux).
- **Historique** : `QStandardPaths::AppDataLocation` + `/metrics.db` (`src/db/DatabaseManager.cpp:14-15`), soit un répertoire de données utilisateur Qt (généralement sous `~/.local/share/` sous Linux). Les entrées plus anciennes que 7 jours sont supprimées (`src/db/DatabaseManager.cpp:40`, `:114-117`).

## Structure

```
src/
  core/     # Logique métier (CoolerBoostController, TemperatureMonitor, SettingsManager, HardwareAbstraction)
  db/       # Base de données (DatabaseManager, MetricsRepository)
  ui/       # Interface QML et services (ThemeManager, TrayManager, ShortcutManager, GlobalHotkey, ...)
tests/      # Tests unitaires (Qt Test)
resources/  # SVG, icônes et démonstration (demo.gif, demo.webm)
docs/       # Documentation, dont le lanceur .desktop et HARDWARE.md
```

## Limitations connues

- Plateforme : **Linux/X11 uniquement** ; pas de support Wayland natif ni de support Windows. Sous XWayland, la capture globale de F7 peut échouer et le repli `Alt+F7` n'est installé que sur GNOME.
- Privilèges : l'interface et le contrôle matériel tournent en root, y compris le moteur QML et les processus `gsettings` ; l'IPC n'accepte que root et l'utilisateur de session qui a lancé l'instance (socket 0600, authentification `SO_PEERCRED`), et `quit` reste réservé à root.
- Matériel : le Cooler Boost est piloté uniquement via `/sys/devices/platform/msi-ec/cooler_boost`. La zone CPU est détectée dynamiquement (repli `thermal_zone0`) et le GPU utilise `nvidia-smi` puis un capteur `hwmon` présomptif ; sans module `msi-ec`, la régulation est inactive. Le repli `hwmon` repose sur une présomption de capteur GPU et n'a pas de validation matérielle universelle.
- Mesures : les seuils sont limités à 30–100 °C (`src/core/CoolerBoostController.cpp:88`, `:96`) ; l'historique affiché est un tampon glissant de 5 minutes (`src/core/TemperatureMonitor.cpp:9`) et aucune courbe n'est tracée dans l'interface.
- Thème : détection fondée sur `gsettings` (GNOME) avec repli sur la palette Qt ; la préférence `theme/dark` est stockée par `SettingsManager` (`src/core/SettingsManager.cpp:12-23`) mais la détection système l'écrase au démarrage (`src/ui/ThemeManager.cpp:129-163`).
- Tests : la suite `Qt Test` actuellement fournie ne contient que deux tests squelettes (`tests/test_main.cpp:11-21`).

Voir [docs/HARDWARE.md](docs/HARDWARE.md) pour le détail de la compatibilité matérielle connue.

## Contribuer

Les règles de compilation, de test et de signalement sont décrites dans [CONTRIBUTING.md](CONTRIBUTING.md).

## Release

La version **v1.0.1** est la version courante (`project(... VERSION 1.0.1)`, `CMakeLists.txt:4`) ; **v1.0.0** est la première version publique du projet. Le détail des changements figure dans [CHANGELOG.md](CHANGELOG.md).

## Licence

MIT — voir [LICENSE](LICENSE).
