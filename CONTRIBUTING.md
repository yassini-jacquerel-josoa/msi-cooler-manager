# Contribuer à MSI Cooler Manager

Merci de votre intérêt ! Ce projet cible la version **v1.0.0** et fonctionne uniquement sur Linux/X11, avec exécution en root. Avant toute contribution, lisez le [README.md](README.md) et [docs/HARDWARE.md](docs/HARDWARE.md).

## Prérequis

- CMake 3.21+, compilateur C++20 (GCC 11+ / Clang 14+), SQLite3.
- Qt 6.4 minimum (6.5+ recommandé) avec les modules `Core`, `Gui`, `Widgets`, `Qml`, `Quick`, `QuickControls2`, `Sql`, et `Test` pour les tests.
- Paquets de développement X11 : `libxcb*`, `libx11-dev`, `libxkbcommon-dev` (X11/xcb est lié de façon inconditionnelle dans `CMakeLists.txt:91-92`).
- Sur la machine de test : module noyau `msi-ec` exposant `/sys/devices/platform/msi-ec/cooler_boost`, zone thermique CPU dans `/sys/class/thermal/thermal_zone*` et `nvidia-smi` ou un capteur GPU `hwmon` dans `/sys/class/hwmon/`.

## Compiler et tester

```bash
# Configuration (les tests sont activés par défaut)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

# Compilation
cmake --build build -j$(nproc)

# Tests unitaires
ctest --test-dir build --output-on-failure

# Installation (root nécessaire)
sudo cmake --install build
```

Pour désactiver les tests : `-DBUILD_TESTS=OFF`. Les avertissements stricts sont activés par défaut (`-DENABLE_WARNINGS=ON` ; `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` etc.) : le code doit compiler sans nouvel avertissement.

Pour essayer l'application sur une machine compatible :

```bash
sudo ./build/msi-cooler-manager            # interface graphique
sudo ./build/msi-cooler-manager --toggle   # transmise à l'instance en cours si elle tourne déjà
```

## Conventions de code

- **C++20**, style Qt : types et méthodes en `PascalCase`, variables et paramètres en `camelCase`, membres privés préfixés de `m_` (voir `src/core/CoolerBoostController.h`).
- Indentation de 4 espaces, accolades sur les lignes propres, un seul `namespace` par domaine : `msi::core`, `msi::db`, `msi::ui`. Les fichiers QML suivent la même indentation (4 espaces).
- Commentaires et messages utilisateur en français ; le code et les identifiants restent en anglais, comme dans les fichiers existants.
- Utiliser `QStringLiteral` / `QLatin1String` pour les chaînes constantes, éviter les casts de style C (`-Wold-style-cast` est actif) et les conversions implicites rétrécissantes (`-Wconversion` est actif).
- Toute nouvelle propriété QML passe par `Q_PROPERTY` / `Q_INVOKABLE` et expose des signaux de notification (`NOTIFY`) ; garder la logique dans `src/core/`, l'accès aux données dans `src/db/` et l'interface/X11 dans `src/ui/`.
- L'accès matériel reste derrière `HardwareAbstraction` (`src/core/HardwareAbstraction.h`) pour permettre les tests ; n'ajoutez pas de lecture sysfs directe dans le contrôleur.
- Ne modifiez pas `CMakeLists.txt`, `resources/` ou les fichiers QML sans expliquer le changement dans la pull request.

## Signaler un problème

Ouvrez une issue avec le maximum d'éléments ci-dessous (une sortie copiée vaut mieux qu'une description vague) :

1. **Modèle MSI exact** : `cat /sys/class/dmi/id/product_name` et `cat /sys/class/dmi/id/product_version`, ainsi que le nom commercial si connu.
2. **Version du noyau** : `uname -r`, ainsi que la distribution (`cat /etc/os-release`).
3. **Sortie de `nvidia-smi`** : au minimum `nvidia-smi`, et idéalement `nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits`.
4. **Contenu de `/sys/devices/platform/msi-ec/`** : `ls -la /sys/devices/platform/msi-ec/` puis `cat /sys/devices/platform/msi-ec/cooler_boost` (et la liste des fichiers utiles tels que les modes/ventilateurs présents sur votre machine).
5. **Capteur CPU** : `cat /sys/class/thermal/thermal_zone0/temp`.
6. **Environnement graphique** : version de Qt (`qmake6 -query QT_VERSION` ou `pkg-config --modversion Qt6Core`), session X11 ou Wayland/XWayland, présence de GNOME (le raccourci de repli et la détection de thème en dépendent).
7. **Reproduction** : commande lancée (avec `sudo` ou `pkexec`), comportement attendu, comportement observé, et extraits pertinents de `journalctl --user` ou de la sortie console.

N'hésitez pas à préciser si le problème concerne la lecture des températures, l'écriture de `cooler_boost`, le raccourci F7, l'IPC/CLI ou le thème.

## Proposer un patch matériel

1. Ouvrez d'abord une issue avec les informations de la section précédente, en indiquant le modèle et la valeur exacte retournée par chaque chemin sysfs.
2. Décrivez la différence observée par rapport à l'implémentation actuelle : chemin sysfs alternatif, format de valeur (`on`/`1`/`0`), présence ou absence de `nvidia-smi`, pertinence du capteur `hwmon` sélectionné.
3. Proposez le changement dans `HardwareAbstraction` ou une nouvelle implémentation, en conservant l'interface `cpuTemperature()`, `gpuTemperature()`, `coolerBoostState()` et `setCoolerBoost(bool)` (y compris le contrat `-1`/`false` en cas d'échec et les accesseurs de diagnostic) ; ajustez la détection de la zone CPU ou du repli `hwmon` si nécessaire, ajoutez un test correspondant dans `tests/` et documentez le modèle dans `docs/HARDWARE.md`.
4. Validez sur la machine concernée : compilation sans avertissement, `ctest --test-dir build --output-on-failure`, relevé CPU/GPU correct dans le panneau, bascule F7 et écriture effective de `/sys/devices/platform/msi-ec/cooler_boost`.
5. Dans la pull request, indiquez le modèle testé, les valeurs « avant/après », la version du noyau et les commandes de validation exécutées. Ne présentez pas un modèle comme pris en charge sans l'avoir réellement testé.

## Licence

En contribuant, vous acceptez que vos modifications soient distribuées sous licence [MIT](LICENSE).
