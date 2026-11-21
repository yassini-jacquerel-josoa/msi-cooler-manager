cmake_minimum_required(VERSION 3.21)

project(msi-cooler-manager
    VERSION 1.0.0
    DESCRIPTION "Gestionnaire de Cooler Boost pour laptops MSI"
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Options ──────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Compiler les tests unitaires" ON)
option(ENABLE_WARNINGS "Activer les avertissements stricts" ON)
option(BUILD_GUI "Compiler l'interface graphique (nécessite X11/xcb)" ON)

# ── Plateforme ───────────────────────────────────────────────────────────────
# L'intégration matérielle repose sur sysfs/msi-ec : Linux reste la seule
# plateforme prise en charge.
if(NOT UNIX OR APPLE)
    message(FATAL_ERROR
        "MSI Cooler Manager n'est pris en charge que sous Linux "
        "(abstraction matérielle sysfs/msi-ec et intégration X11).")
endif()

# ── Dépendances ──────────────────────────────────────────────────────────────
# Le README documente Qt 6.5+ comme minimum. Les images de distribution
# (par exemple Ubuntu 24.04) fournissent encore Qt 6.4 : on accepte cette
# version avec un avertissement afin que la configuration et les tests
# restent possibles sur ces systèmes.
set(MSI_QT_COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Sql)
find_package(Qt6 6.5 QUIET COMPONENTS ${MSI_QT_COMPONENTS})
if(NOT Qt6_FOUND)
    find_package(Qt6 6.4 REQUIRED COMPONENTS ${MSI_QT_COMPONENTS})
    message(WARNING
        "Qt 6.5+ est le minimum documenté dans le README, mais seule la version "
        "${Qt6_VERSION} est disponible. Construction avec cette version plus "
        "ancienne pour permettre les tests sur les distributions fournissant "
        "encore Qt 6.4.")
endif()
find_package(SQLite3 REQUIRED)

# ── X11 / xcb : liaison conditionnelle ───────────────────────────────────────
set(MSI_HAVE_X11_XCB OFF)
find_package(X11 QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(XCB QUIET IMPORTED_TARGET xcb)
endif()
if(X11_FOUND AND TARGET PkgConfig::XCB)
    set(MSI_HAVE_X11_XCB ON)
endif()

if(BUILD_GUI AND NOT MSI_HAVE_X11_XCB)
    set(_msi_x11_hint "")
    if(QT_FEATURE_xcb)
        set(_msi_x11_hint " Qt a été compilé avec le support X11 (QT_FEATURE_xcb=ON).")
    endif()
    message(FATAL_ERROR
        "L'interface graphique nécessite les bibliothèques de développement X11 "
        "et xcb (GlobalHotkey utilise xcb et X11/XKBlib).${_msi_x11_hint}\n"
        "Installez libx11-dev, libxcb1-dev et libxcb-keysyms1-dev, et exécutez "
        "l'application sous un serveur X11 ou XWayland (session Wayland).\n"
        "Pour compiler uniquement les bibliothèques et les tests sans interface "
        "graphique, reconfigurez avec -DBUILD_GUI=OFF.")
endif()

# ── Avertissements stricts ───────────────────────────────────────────────────
if(ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
        add_compile_options(-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual)
        add_compile_options(-Wconversion -Wsign-conversion -Wnull-dereference)
    elseif(MSVC)
        add_compile_options(/W4 /permissive-)
    endif()
endif()

# ── Bibliothèque core ────────────────────────────────────────────────────────
add_library(msi-core STATIC
    src/core/CoolerBoostController.cpp
    src/core/CoolerBoostController.h
    src/core/TemperatureMonitor.cpp
    src/core/TemperatureMonitor.h
    src/core/SettingsManager.cpp
    src/core/SettingsManager.h
    src/core/HardwareAbstraction.cpp
    src/core/HardwareAbstraction.h
)

target_include_directories(msi-core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(msi-core PUBLIC Qt6::Core Qt6::Sql SQLite::SQLite3)

# ── Base de données ──────────────────────────────────────────────────────────
add_library(msi-db STATIC
    src/db/DatabaseManager.cpp
    src/db/DatabaseManager.h
    src/db/MetricsRepository.cpp
    src/db/MetricsRepository.h
)

target_include_directories(msi-db PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(msi-db PUBLIC Qt6::Sql SQLite::SQLite3 msi-core)

# ── Exécutable principal ─────────────────────────────────────────────────────
set(MSI_GUI_ENABLED OFF)
if(BUILD_GUI AND MSI_HAVE_X11_XCB)
    qt_add_executable(msi-cooler-manager
        src/main.cpp
        src/ui/ThemeManager.cpp
        src/ui/ThemeManager.h
        src/ui/TrayManager.cpp
        src/ui/TrayManager.h
        src/ui/ShortcutManager.cpp
        src/ui/ShortcutManager.h
        src/ui/DesktopShortcuts.cpp
        src/ui/DesktopShortcuts.h
        src/ui/InputMask.cpp
        src/ui/InputMask.h
        src/ui/GlobalHotkey.cpp
        src/ui/GlobalHotkey.h
        src/ui/BackdropBlur.cpp
        src/ui/BackdropBlur.h
    )

    target_link_libraries(msi-cooler-manager PRIVATE
        msi-core
        msi-db
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
    )

    # Liaison explicite uniquement lorsque les dépendances sont disponibles.
    # Les cibles importées fournissent les chemins d'inclusion et les bibliothèques.
    target_link_libraries(msi-cooler-manager PRIVATE X11::X11 PkgConfig::XCB)

    # ── Module QML ───────────────────────────────────────────────────────────
    qt_add_qml_module(msi-cooler-manager
        URI MSI.CoolerManager
        VERSION 1.0
        QML_FILES
            src/ui/Main.qml
            src/ui/CompactView.qml
            src/ui/ExpandedView.qml
            src/ui/TemperatureCard.qml
            src/ui/ModeCard.qml
            src/ui/ThresholdSlider.qml
        RESOURCES
            resources/svg/fire.svg
            resources/svg/balance.svg
            resources/svg/fire-dark.svg
            resources/svg/fire-light.svg
            resources/svg/fire-blue.svg
            resources/icons/fire-blue.png
            resources/svg/balance-dark.svg
            resources/svg/balance-light.svg
    )
    set(MSI_GUI_ENABLED ON)
endif()

# ── Tests ────────────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_core
        tests/test_main.cpp
        tests/MockHardware.h
    )
    target_link_libraries(test_core PRIVATE msi-core Qt6::Test)
    add_test(NAME CoreTests COMMAND test_core)
    set_tests_properties(CoreTests PROPERTIES LABELS "core")

    add_executable(test_db
        tests/test_db.cpp
    )
    target_link_libraries(test_db PRIVATE msi-db Qt6::Test)
    add_test(NAME DatabaseTests COMMAND test_db)
    set_tests_properties(DatabaseTests PROPERTIES LABELS "database")
endif()

# ── Code généré par Qt : avertissements neutralisés ──────────────────────────
# Les sources produites par qmlcachegen et AUTOMOC (.rcc/qmlcache/*,
# *_autogen/, mocs_compilation.cpp) ne relèvent pas du projet. Qt 6.4 y génère
# des lambdas qui redéclarent leurs paramètres (-Wshadow, ~414 occurrences)
# et des conversions qsizetype -> int (-Wconversion). On applique -w à ces
# seules sources : le jurnal de compilation reste lisible et seuls les
# avertissements du code maison subsistent.
if(ENABLE_WARNINGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    get_property(_msi_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_msi_target IN LISTS _msi_targets)
        get_target_property(_msi_sources ${_msi_target} SOURCES)
        if(_msi_sources)
            foreach(_msi_source IN LISTS _msi_sources)
                get_source_file_property(_msi_generated "${_msi_source}" GENERATED)
                if(_msi_generated OR _msi_source MATCHES "(^|/)[.]rcc/|qmlcache|_autogen|mocs_compilation")
                    set_property(SOURCE "${_msi_source}" APPEND PROPERTY COMPILE_OPTIONS -w)
                endif()
            endforeach()
        endif()
    endforeach()
    unset(_msi_targets)
    unset(_msi_sources)
    unset(_msi_source)
    unset(_msi_generated)
endif()

# ── Installation ─────────────────────────────────────────────────────────────
if(MSI_GUI_ENABLED)
    install(TARGETS msi-cooler-manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES docs/msi-cooler-manager.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    # resources/icons/fire-blue.png mesure 128 x 128 pixels (vérifié avec file).
    install(FILES resources/icons/fire-blue.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
            RENAME msi-cooler-manager.png)
endif()
