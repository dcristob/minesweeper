#!/bin/sh
set -e

REPO="https://github.com/dcristob/minesweeper"
TMPDIR=$(mktemp -d)
SYSTEM=false
UNINSTALL=false

for arg in "$@"; do
    case "$arg" in
        --system)   SYSTEM=true ;;
        --uninstall) UNINSTALL=true ;;
        --help|-h)
            echo "Usage: curl -sL ${REPO}/raw/main/remote-install.sh | bash [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --system     Install system-wide to /usr (requires sudo)"
            echo "  --uninstall  Remove the installed files"
            echo "  --help       Show this help message"
            echo ""
            echo "Default: install to ~/.local (no root required)"
            exit 0
            ;;
    esac
done

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

if $UNINSTALL; then
    if $SYSTEM; then
        PREFIX="/usr"
    else
        PREFIX="$HOME/.local"
    fi
    ICON_SIZES="48 64 128 256"
    echo "Uninstalling Minesweeper..."
    rm -f "${PREFIX}/bin/minesweeper"
    rm -f "${PREFIX}/share/icons/hicolor/scalable/apps/minesweeper.svg"
    for s in $ICON_SIZES; do
        rm -f "${PREFIX}/share/icons/hicolor/${s}x${s}/apps/minesweeper.png"
    done
    rm -f "${PREFIX}/share/applications/minesweeper.desktop"
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t "${PREFIX}/share/icons/hicolor" 2>/dev/null || true
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "${PREFIX}/share/applications" 2>/dev/null || true
    echo "Done."
    exit 0
fi

if ! command -v make >/dev/null 2>&1; then
    echo "Error: 'make' is required but not installed." >&2
    exit 1
fi
if ! command -v gcc >/dev/null 2>&1; then
    echo "Error: 'gcc' is required but not installed." >&2
    exit 1
fi
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists raylib 2>/dev/null; then
    echo "Error: raylib is required but not installed." >&2
    echo "Install it with your package manager, e.g.:" >&2
    echo "  Arch:     sudo pacman -S raylib" >&2
    echo "  Debian:   sudo apt install libraylib-dev" >&2
    echo "  Fedora:   sudo dnf install raylib-devel" >&2
    exit 1
fi

echo "Cloning repository..."
git clone --depth 1 "$REPO" "$TMPDIR/minesweeper"

echo "Building..."
make -C "$TMPDIR/minesweeper"

cd "$TMPDIR/minesweeper"
./install.sh ${SYSTEM:+--system}

echo ""
echo "Minesweeper installed successfully!"
if ! $SYSTEM; then
    echo "Make sure ~/.local/bin is in your PATH."
    echo "Add this to your shell profile if needed:"
    echo '  export PATH="$HOME/.local/bin:$PATH"'
fi
