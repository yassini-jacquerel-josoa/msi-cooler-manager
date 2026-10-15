cmake_minimum_required(VERSION 3.21)

project(msi-cooler-manager
    VERSION 1.0.0
    DESCRIPTION "Gestionnaire de Cooler Boost pour laptops MSI"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Options ──────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Compiler les tests unitaires" ON)
option(ENABLE_WARNINGS "Activer les avertissements stricts" ON)

# ── Dépendances ──────────────────────────────────────────────────────────────
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Sql)
find_package(SQLite3 REQUIRED)

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
    xcb
    X11
)

# ── Module QML ───────────────────────────────────────────────────────────────
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

# ── Tests ────────────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_core
        tests/test_main.cpp
    )

    target_link_libraries(test_core PRIVATE msi-core Qt6::Test)

    add_test(NAME CoreTests COMMAND test_core)
endif()

# ── Installation ─────────────────────────────────────────────────────────────
install(TARGETS msi-cooler-manager DESTINATION bin)
install(FILES docs/msi-cooler-manager.desktop DESTINATION share/applications)
