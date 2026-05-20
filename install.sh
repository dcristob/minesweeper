#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ICON_SIZES="48 64 128 256"

SYSTEM=false
UNINSTALL=false
for arg in "$@"; do
    case "$arg" in
        --system)   SYSTEM=true ;;
        --uninstall) UNINSTALL=true ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --system     Install system-wide to /usr (requires sudo)"
            echo "  --uninstall  Remove the installed files"
            echo "  --help       Show this help message"
            echo ""
            echo "Default: install to ~/.local (no root required)"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 1
            ;;
    esac
done

if $SYSTEM; then
    PREFIX="/usr"
    DESTDIR=""
else
    PREFIX="$HOME/.local"
    DESTDIR=""
fi

BIN_DIR="${DESTDIR}${PREFIX}/bin"
ICONS_DIR="${DESTDIR}${PREFIX}/share/icons/hicolor"
APPS_DIR="${DESTDIR}${PREFIX}/share/applications"

uninstall() {
    echo "Uninstalling Minesweeper..."
    rm -f "${BIN_DIR}/minesweeper"
    rm -f "${ICONS_DIR}/scalable/apps/minesweeper.svg"
    for s in $ICON_SIZES; do
        rm -f "${ICONS_DIR}/${s}x${s}/apps/minesweeper.png"
    done
    rm -f "${APPS_DIR}/minesweeper.desktop"
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t "${ICONS_DIR}" 2>/dev/null || true
    fi
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${APPS_DIR}" 2>/dev/null || true
    fi
    echo "Done."
}

if $UNINSTALL; then
    uninstall
    exit 0
fi

echo "Building Minesweeper..."
make -C "$SCRIPT_DIR" clean
make -C "$SCRIPT_DIR"

echo "Installing to ${PREFIX}..."

install -d "$BIN_DIR"
install -m755 "${SCRIPT_DIR}/minesweeper" "${BIN_DIR}/minesweeper"

install -d "${ICONS_DIR}/scalable/apps"
install -m644 "${SCRIPT_DIR}/minesweeper.svg" "${ICONS_DIR}/scalable/apps/minesweeper.svg"

for s in $ICON_SIZES; do
    install -d "${ICONS_DIR}/${s}x${s}/apps"
    if [ -f "${SCRIPT_DIR}/minesweeper-${s}.png" ]; then
        install -m644 "${SCRIPT_DIR}/minesweeper-${s}.png" "${ICONS_DIR}/${s}x${s}/apps/minesweeper.png"
    fi
done

install -d "$APPS_DIR"
install -m644 "${SCRIPT_DIR}/minesweeper.desktop" "${APPS_DIR}/minesweeper.desktop"

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "${ICONS_DIR}" 2>/dev/null || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${APPS_DIR}" 2>/dev/null || true
fi

echo ""
echo "Installation complete!"
echo "  Binary:   ${BIN_DIR}/minesweeper"
echo "  Desktop:  ${APPS_DIR}/minesweeper.desktop"
if ! $SYSTEM; then
    echo ""
    echo "Make sure ~/.local/bin is in your PATH."
    echo "Add this to your shell profile if needed:"
    echo '  export PATH="$HOME/.local/bin:$PATH"'
fi
