# Minesweeper

A lightweight Minesweeper clone for Linux built with C and [Raylib](https://www.raylib.com/). Inspired by GNOME Mines light theme.

![C](https://img.shields.io/badge/C-C99-blue) ![Raylib](https://img.shields.io/badge/Raylib-5.5-green) ![License](https://img.shields.io/badge/License-MIT-yellow)

## Features

- Three difficulty levels: Beginner (9x9), Intermediate (16x16), Expert (30x16)
- Mouse and keyboard controls
- Chord click (middle-click or left+right)
- Auto-pause on window focus loss
- Local leaderboard (top 10 per difficulty)
- Safe first click with flood-fill reveal
- ~30KB binary, single source file

## Install

Requires [Raylib](https://www.raylib.com/) installed on your system.

```bash
# Arch Linux
sudo pacman -S raylib
```

### One-liner (recommended)

```bash
curl -sL https://raw.githubusercontent.com/dcristob/minesweeper/main/remote-install.sh | bash
```

System-wide:

```bash
curl -sL https://raw.githubusercontent.com/dcristob/minesweeper/main/remote-install.sh | sudo bash -s --system
```

### From a cloned repo

```bash
./install.sh              # user install to ~/.local
sudo ./install.sh --system  # system-wide to /usr
```

### Uninstall

```bash
./install.sh --uninstall
# Or for system-wide:
sudo ./install.sh --system --uninstall
# Or via curl:
curl -sL https://raw.githubusercontent.com/dcristob/minesweeper/main/remote-install.sh | bash -s --uninstall
```

### Build from source

```bash
make
./minesweeper
```

## Controls

### Mouse

| Action | Input |
|--------|-------|
| Reveal cell | Left click |
| Toggle flag | Right click |
| Chord | Middle click or Left+Right click |

### Keyboard

| Action | Input |
|--------|-------|
| Move cursor | Arrow keys |
| Reveal cell | Space / Enter |
| Toggle flag | F |
| Back / Menu | Escape |

## Leaderboard

Scores are saved to `~/.local/share/minesweeper/leaderboard.txt`.

## License

MIT
