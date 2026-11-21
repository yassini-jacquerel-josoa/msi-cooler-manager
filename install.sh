#!/bin/sh
#
# install.sh — Installation de MSI Cooler Manager
#
# Usage (one-liner) :
#   curl -fsSL https://raw.githubusercontent.com/yassini-jacquerel-josoa/msi-cooler-manager/main/install.sh | sh
#
# Avec des options (le script est lu depuis stdin, on passe les arguments
# après un "--") :
#   curl -fsSL .../install.sh | sh -s -- --ref main
#   curl -fsSL .../install.sh | sh -s -- --skip-tests
#
# Depuis un checkout local (le téléchargement est alors ignoré) :
#   ./install.sh
#
# Le script est en POSIX sh : il fonctionne sous dash, bash et ash.
set -eu

REPO="yassini-jacquerel-josoa/msi-cooler-manager"
REPO_URL="https://github.com/${REPO}.git"
ARCHIVE_URL="https://github.com/${REPO}/archive"

# ── Défauts, surchargeables par variables d'environnement ─────────────────────
#   MSI_REF=v1.0.0      référence Git à installer (branche ou tag)
#   MSI_PREFIX=         préfixe d'installation (défaut : préfixe CMake, /usr/local)
#   MSI_JOBS=n          nombre de cœurs de compilation parallèle
#   MSI_SKIP_TESTS=1    ne pas exécuter ctest
#   MSI_NO_DEPS=1       ne pas installer les paquets système
#   MSI_NO_INSTALL=1    compiler sans installer (pas de sudo nécessaire)
#   MSI_KEEP_BUILD=1    conserver le répertoire de compilation
#   MSI_SHA256=hex      somme de contrôle attendue de l'archive téléchargée
# MSI_REF permet de choisir une autre référence (branche ou tag).
MSI_REF="${MSI_REF:-v1.0.0}"
MSI_PREFIX="${MSI_PREFIX:-}"
MSI_JOBS="${MSI_JOBS:-}"
MSI_SKIP_TESTS="${MSI_SKIP_TESTS:-0}"
MSI_NO_DEPS="${MSI_NO_DEPS:-0}"
MSI_NO_INSTALL="${MSI_NO_INSTALL:-0}"
MSI_KEEP_BUILD="${MSI_KEEP_BUILD:-0}"
MSI_SHA256="${MSI_SHA256:-}"

# ── Sorties ───────────────────────────────────────────────────────────────────
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_HEAD="$(printf '\033[1;34m')"
    C_OK="$(printf '\033[1;32m')"
    C_WARN="$(printf '\033[1;33m')"
    C_ERR="$(printf '\033[1;31m')"
    C_RST="$(printf '\033[0m')"
else
    C_HEAD=""; C_OK=""; C_WARN=""; C_ERR=""; C_RST=""
fi

info() { printf '%s==>%s %s\n' "$C_HEAD" "$C_RST" "$*"; }
good() { printf '%s  ✓%s %s\n' "$C_OK" "$C_RST" "$*"; }
warn() { printf '%sWARN%s %s\n' "$C_WARN" "$C_RST" "$*" >&2; }
die()  { printf '%sERROR%s %s\n' "$C_ERR" "$C_RST" "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

usage() {
    cat <<EOF
install.sh — installe MSI Cooler Manager depuis les sources.

Usage :
  ./install.sh [options]
  curl -fsSL https://raw.githubusercontent.com/${REPO}/main/install.sh | sh -s -- [options]

Options :
  -h, --help        afficher cette aide
  --ref REF         branche ou tag à installer (défaut : ${MSI_REF} ; --ref main pour la dernière version de développement)
  --prefix DIR      préfixe d'installation (défaut : /usr/local)
  --jobs N          cœurs de compilation parallèle (défaut : tous)
  --skip-tests      ne pas exécuter la suite de tests
  --no-deps         ne pas installer les paquets système (dépendances supposées présentes)
  --no-install      compiler uniquement, sans installer (aucun droit root)
  --keep-build      conserver le répertoire de compilation et afficher son chemin

Variables d'environnement équivalentes :
  MSI_REF, MSI_PREFIX, MSI_JOBS, MSI_SKIP_TESTS, MSI_NO_DEPS, MSI_NO_INSTALL,
  MSI_KEEP_BUILD (valeur 1 pour activer), MSI_SHA256 (somme de contrôle de
  l'archive, ex. publiée dans les notes de version).

Après installation :
  sudo msi-cooler-manager          # nouvelle instance (droits root requis)
  pkexec msi-cooler-manager        # via le lanceur .desktop
EOF
}

# ── Arguments ─────────────────────────────────────────────────────────────────
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help)      usage; exit 0 ;;
        --ref)          [ $# -ge 2 ] || die "--ref attend une valeur."; MSI_REF=$2; shift 2 ;;
        --prefix)       [ $# -ge 2 ] || die "--prefix attend une valeur."; MSI_PREFIX=$2; shift 2 ;;
        --jobs)         [ $# -ge 2 ] || die "--jobs attend une valeur."; MSI_JOBS=$2; shift 2 ;;
        --skip-tests)   MSI_SKIP_TESTS=1; shift ;;
        --no-deps)      MSI_NO_DEPS=1; shift ;;
        --no-install)   MSI_NO_INSTALL=1; shift ;;
        --keep-build)   MSI_KEEP_BUILD=1; shift ;;
        --)             shift; break ;;
        *)              usage >&2; die "Option inconnue : $1" ;;
    esac
done

# ── Nettoyage ─────────────────────────────────────────────────────────────────
WORK_DIR=""
cleanup() {
    if [ -n "$WORK_DIR" ] && [ -d "$WORK_DIR" ] && [ "$MSI_KEEP_BUILD" != "1" ]; then
        rm -rf "$WORK_DIR"
    fi
    return 0
}
trap cleanup 0
trap 'exit 130' INT
trap 'exit 143' TERM

# ── Prérequis de base ─────────────────────────────────────────────────────────
[ "$(uname -s)" = "Linux" ] || die "MSI Cooler Manager ne prend en charge que Linux."

if [ "$MSI_NO_DEPS" != "1" ]; then
    have apt-get || have dnf || have pacman || have zypper || \
        warn "Gestionnaire de paquets inconnu : les dépendances devront être installées manuellement (voir README.md)."
fi
have curl || have wget || \
    die "curl ou wget est nécessaire pour télécharger les sources (sinon clonez le dépôt et lancez ./install.sh)."
have tar || die "tar est nécessaire pour extraire les sources."
have cmake || [ "$MSI_NO_DEPS" != "1" ] || die "cmake introuvable (installez-le ou retirez --no-deps)."

# ── Droits root (sudo) ────────────────────────────────────────────────────────
SUDO=""
sudo_ready=""
require_sudo() {
    if [ "$sudo_ready" = "1" ]; then return 0; fi
    if [ "$(id -u)" -eq 0 ]; then
        SUDO=""; sudo_ready="1"; return 0
    fi
    if have sudo; then
        printf 'Mot de passe root requis pour cette étape.\n'
        sudo -v || die "Authentification sudo refusée."
        SUDO="sudo"; sudo_ready="1"; return 0
    fi
    die "Droits root requis. Relancez avec :
  curl -fsSL https://raw.githubusercontent.com/${REPO}/main/install.sh | sudo sh"
}

# ── Paquets par distribution ──────────────────────────────────────────────────
# Alignés sur .github/workflows/ci.yml (liste validée sur Ubuntu).
# qml6-module-qtquick-templates : QtQuick.Controls charge QtQuick.Templates au
#   chargement de l'UI. --no-install-recommends ne l'installe pas d'office et
#   qml6-module-qtquick-controls ne dépend que des bibliothèques C++ : sans ce
#   paquet, l'application échoue au démarrage de l'interface.
# qml6-module-qtqml : fournit le qmldir racine de QtQml (import « auto » de
#   QtQuick). Son absence provoque un avertissement de qmlimportscanner.
DEPS_APT="build-essential cmake ninja-build pkg-config \
qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools \
libqt6sql6-sqlite libsqlite3-dev \
libx11-dev libxcb1-dev libxcb-keysyms1-dev libxkbcommon-x11-dev \
qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-window \
qml6-module-qtquick-templates qml6-module-qtqml"

DEPS_DNF="gcc-c++ cmake ninja-build pkgconf-pkg-config \
qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtbase-sqlite \
sqlite-devel libx11-devel libxcb-devel libxkbcommon-x11-devel \
qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-window"

DEPS_PACMAN="base-devel cmake ninja \
qt6-base qt6-declarative qt6-quickcontrols2 sqlite \
libx11 xcb-util-keysyms libxkbcommon-x11"

DEPS_ZYPPER="devel_basis cmake ninja pkg-config \
qt6-base-devel qt6-declarative-devel libsqlite3-devel \
libX11-devel libxcb-devel libxkbcommon-devel"

install_deps() {
    if have apt-get; then
        info "Installation des dépendances (apt)"
        require_sudo
        $SUDO apt-get update </dev/null
        # shellcheck disable=SC2086
        $SUDO apt-get install -y --no-install-recommends $DEPS_APT </dev/null
    elif have dnf; then
        info "Installation des dépendances (dnf)"
        require_sudo
        # shellcheck disable=SC2086
        $SUDO dnf install -y $DEPS_DNF </dev/null
    elif have pacman; then
        info "Installation des dépendances (pacman)"
        require_sudo
        # shellcheck disable=SC2086
        $SUDO pacman -Sy --needed --noconfirm $DEPS_PACMAN </dev/null
    elif have zypper; then
        info "Installation des dépendances (zypper)"
        require_sudo
        # shellcheck disable=SC2086
        $SUDO zypper --non-interactive install $DEPS_ZYPPER </dev/null
    else
        warn "Gestionnaire de paquets inconnu : vérification des outils présents."
        missing=""
        for tool in cmake c++; do
            have "$tool" || missing="$missing $tool"
        done
        if [ -n "$missing" ]; then
            die "Outils manquants :$missing — installez les prérequis du README.md puis relancez avec --no-deps."
        fi
        warn "Relancez avec --no-deps si la configuration CMake échoue faute de Qt 6."
    fi
}

# ── Modules QML requis à l'exécution ──────────────────────────────────────────
# Les distributions découpent les modules QML en paquets séparés et
# --no-install-recommends ne tire pas QtQuick.Templates, pourtant indispensable
# au chargement des contrôles QtQuick.Controls (Slider, boutons...). On vérifie
# la présence des modules attendus, quelle que soit la manière dont les
# dépendances ont été obtenues (--no-deps, autre distribution).
check_qml_modules() {
    import_path=""
    if have qmake6; then
        import_path=$(qmake6 -query QT_INSTALL_QML 2>/dev/null || true)
    elif have qtpaths6; then
        import_path=$(qtpaths6 --qt-query QT_INSTALL_QML 2>/dev/null || true)
    fi
    if [ -z "$import_path" ]; then
        for candidate in /usr/lib/qt6/qml /usr/lib64/qt6/qml /usr/lib/*/qt6/qml; do
            if [ -f "$candidate/QtQuick/qmldir" ]; then
                import_path=$candidate
                break
            fi
        done
    fi
    # Chemin d'import introuvable : Qt n'est pas installé, CMake signalera l'erreur.
    [ -f "$import_path/QtQuick/qmldir" ] || return 0

    if [ ! -f "$import_path/QtQuick/Templates/qmldir" ]; then
        warn "Module QML manquant : QtQuick.Templates ($import_path)."
        warn "  Les contrôles QtQuick.Controls échoueront au chargement de l'interface."
        warn "  Debian/Ubuntu : sudo apt-get install qml6-module-qtquick-templates"
        warn "  Autres : installez le module QML « QtQuick.Templates » de votre distribution."
    fi
    if [ ! -f "$import_path/QtQml/qmldir" ]; then
        warn "Module QML manquant : QtQml (non bloquant, mais qmlimportscanner avertira au build)."
        warn "  Debian/Ubuntu : sudo apt-get install qml6-module-qtqml"
    fi
    return 0
}

# ── Sources ───────────────────────────────────────────────────────────────────
SRC_DIR=""
BUILD_DIR=""

detect_local_source() {
    # $0 vaut "sh" lorsque le script est pipé : dirname renvoie alors ".".
    script_dir=$(dirname "$0")
    if [ -f "$script_dir/CMakeLists.txt" ] && [ -f "$script_dir/src/main.cpp" ]; then
        SRC_DIR=$(cd "$script_dir" && pwd)
        good "Source locale détectée : $SRC_DIR"
    fi
}

download() { # $1 = url, $2 = destination
    if have curl; then
        curl -fsSL "$1" -o "$2"
    elif have wget; then
        wget -qO "$2" "$1"
    else
        die "curl ou wget est nécessaire pour télécharger $1"
    fi
}

# Contrôle optionnel de l'archive : HTTPS protège le transport, la somme de
# contrôle protège contre un dépôt ou un compte GitHub compromis.
verify_archive() { # $1 = chemin de l'archive
    [ -n "$MSI_SHA256" ] || return 0
    if ! have sha256sum; then
        warn "sha256sum introuvable : somme de contrôle non vérifiée."
        return 0
    fi
    printf '%s  %s\n' "$MSI_SHA256" "$1" | sha256sum -c - >/dev/null 2>&1 || \
        die "Somme de contrôle SHA-256 incorrecte.
  Attendu : $MSI_SHA256
  Obtenu  : $(sha256sum "$1" 2>/dev/null | cut -d' ' -f1)
Refusez l'installation ou vérifiez la valeur publiée par le mainteneur."
    good "Somme de contrôle SHA-256 vérifiée"
}

fetch_source() {
    WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/msi-cooler-manager.XXXXXX") \
        || die "Impossible de créer un répertoire temporaire."
    info "Téléchargement des sources (${REPO} @ ${MSI_REF})"
    archive="$WORK_DIR/src.tar.gz"
    extracted=0
    # La référence peut être une branche (refs/heads) ou un tag (refs/tags).
    for kind in heads tags; do
        if download "$ARCHIVE_URL/refs/$kind/$MSI_REF.tar.gz" "$archive" >/dev/null 2>&1; then
            verify_archive "$archive"
            mkdir -p "$WORK_DIR/src"
            if tar -xzf "$archive" -C "$WORK_DIR/src" --strip-components=1 2>/dev/null; then
                extracted=1
                break
            fi
        fi
    done
    if [ "$extracted" -ne 1 ] && have git; then
        info "Archive indisponible, tentative via git clone"
        if [ -n "$MSI_SHA256" ]; then
            warn "Clone git utilisé : la somme de contrôle MSI_SHA256 ne s'applique pas (contenu authentifié par le commit Git)."
        fi
        rm -rf "$WORK_DIR/src"
        if git clone --depth 1 --branch "$MSI_REF" "$REPO_URL" "$WORK_DIR/src" >/dev/null 2>&1; then
            extracted=1
        fi
    fi
    [ "$extracted" -eq 1 ] || \
        die "Téléchargement impossible pour la référence '$MSI_REF' (vérifiez le nom : branche ou tag)."
    [ -f "$WORK_DIR/src/CMakeLists.txt" ] || die "Archive invalide : CMakeLists.txt introuvable."
    SRC_DIR="$WORK_DIR/src"
    BUILD_DIR="$WORK_DIR/build"
}

# ── Compilation ───────────────────────────────────────────────────────────────
configure_build() {
    if [ -z "$SRC_DIR" ]; then
        fetch_source
    else
        # Checkout local : on utilise le répertoire build/ habituel du README.
        BUILD_DIR="$SRC_DIR/build"
    fi

    if [ -z "$MSI_JOBS" ]; then
        MSI_JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
    fi

    generator=""
    # On ne force le générateur Ninja que pour un build neuf : un répertoire
    # existant garde le générateur de sa configuration initiale.
    if have ninja && [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        generator="-G Ninja"
    fi

    info "Configuration (Release, tests: $([ "$MSI_SKIP_TESTS" = "1" ] && echo non || echo oui))"
    # shellcheck disable=SC2086
    cmake -S "$SRC_DIR" -B "$BUILD_DIR" $generator \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=ON \
        -DENABLE_WARNINGS=ON \
        || die "Échec de la configuration CMake (Qt 6.4+ et les en-têtes X11/xcb sont-ils installés ? voir README.md)"

    info "Compilation (-j${MSI_JOBS})"
    cmake --build "$BUILD_DIR" --parallel "$MSI_JOBS" \
        || die "Échec de la compilation (voir les erreurs ci-dessus)."
    good "Compilation terminée"
}

run_tests() {
    if [ "$MSI_SKIP_TESTS" = "1" ]; then
        info "Tests ignorés (--skip-tests)"
        return 0
    fi
    info "Exécution des tests"
    QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD_DIR" --output-on-failure \
        || die "Des tests ont échoué (rapport ci-dessus). Relancez avec --skip-tests pour passer outre."
    good "Tests réussis"
}

install_project() {
    if [ "$MSI_NO_INSTALL" = "1" ]; then
        info "Installation ignorée (--no-install)"
        return 0
    fi
    require_sudo
    info "Installation"
    if [ -n "$MSI_PREFIX" ]; then
        $SUDO cmake --install "$BUILD_DIR" --prefix "$MSI_PREFIX" \
            || die "Échec de l'installation."
    else
        $SUDO cmake --install "$BUILD_DIR" \
            || die "Échec de l'installation."
    fi
    if have update-desktop-database; then
        $SUDO update-desktop-database "${MSI_PREFIX:-/usr/local}/share/applications" \
            >/dev/null 2>&1 || true
    fi
    good "Installation terminée"
}

# ── Vérifications post-installation ───────────────────────────────────────────
check_hardware() {
    if [ -e /sys/devices/platform/msi-ec/cooler_boost ]; then
        good "msi-ec détecté : /sys/devices/platform/msi-ec/cooler_boost"
    else
        warn "Module msi-ec introuvable : le Cooler Boost restera inactif."
        if have modinfo && modinfo msi-ec >/dev/null 2>&1; then
            warn "Le module est disponible sur ce système : sudo modprobe msi-ec"
        fi
        warn "Voir docs/HARDWARE.md : https://github.com/${REPO}#readme"
    fi
    if [ -n "${XDG_SESSION_TYPE:-}" ] && [ "$XDG_SESSION_TYPE" = "wayland" ]; then
        warn "Session Wayland détectée : le raccourci global F7 nécessite XWayland (repli Alt+F7 sous GNOME)."
    fi
}

summary() {
    echo ""
    printf '%s==> Installation de MSI Cooler Manager terminée%s\n' "$C_OK" "$C_RST"
    echo ""
    echo "  Lancement :  sudo msi-cooler-manager"
    echo "               pkexec msi-cooler-manager   (lanceur du menu d'applications)"
    echo "  CLI :        sudo msi-cooler-manager --toggle | --auto | --manual | --quit"
    echo "  Raccourci :  F7 (court : mode, maintenu ~2 s : vue étendue)"
    echo ""
    if [ "$MSI_NO_INSTALL" = "1" ]; then
        echo "  Binaire compilé dans : $BUILD_DIR"
        if [ -n "$WORK_DIR" ] && [ "$MSI_KEEP_BUILD" != "1" ]; then
            echo "  (répertoire temporaire supprimé à la fin : relancez avec --keep-build pour le conserver)"
        fi
        echo ""
    fi
}

# ── Enchaînement ──────────────────────────────────────────────────────────────
detect_local_source
if [ "$MSI_NO_DEPS" != "1" ]; then
    install_deps
else
    info "Installation des dépendances ignorée (--no-deps)"
fi
check_qml_modules
configure_build
run_tests
install_project
check_hardware
summary
