# MSI Cooler Manager

Gestionnaire de Cooler Boost pour laptops MSI — Qt6/QML, C++20.

## Fonctionnalités

- **Mode AUTO** : régulation thermique automatique (seuils CPU/GPU configurables)
- **Mode MANUEL** : activation forcée du Cooler Boost
- **Surveillance temps réel** avec historique glissant (5 min)
- **Thème clair/sombre** avec détection automatique du thème système
- **Systray** avec menu contextuel
- **Raccourcis clavier** : F7 (toggle mode), maintien 3s (expand)
- **Base de données SQLite** pour l'historique des températures
- **IPC** : communication entre instances via QLocalServer

## Dépendances

- Qt 6.5+ (Core, Gui, Qml, Quick, Sql, Test)
- SQLite3
- CMake 3.21+
- GCC 11+ / Clang 14+ / MSVC 2022

## Compilation

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## Installation

```bash
sudo cmake --install .
```

## Utilisation

```bash
# Mode GUI (par défaut)
./msi-cooler-manager

# Commandes CLI (instance unique)
./msi-cooler-manager --toggle   # toggle mode AUTO/MANUEL
./msi-cooler-manager --auto     # passer en mode AUTO
./msi-cooler-manager --manual   # passer en mode MANUEL
```

## Structure

```
src/
  core/     # Logique métier (CoolerBoostController, TemperatureMonitor, SettingsManager)
  db/       # Base de données (DatabaseManager, MetricsRepository)
  ui/       # Interface (ThemeManager, TrayManager, ShortcutManager)
tests/      # Tests unitaires
resources/  # SVG, icônes
docs/       # Documentation
```

## Licence

MIT
