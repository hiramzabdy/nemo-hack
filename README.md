# nemo-hack

> Nemo File Manager with Windows Explorer-style preview panel and combined places+tree sidebar.

A modified build of the [Nemo file manager](https://github.com/linuxmint/nemo) (Cinnamon desktop, Linux Mint) that adds a right-side preview panel (Alt+P) and a combined explorer sidebar — like Windows File Explorer's navigation pane. Runs locally without replacing your system Nemo.

![screenshot](https://img.shields.io/badge/LMDE7-ready-brightgreen)
![build](https://img.shields.io/badge/build-0%20errors%2C%200%20warnings-brightgreen)

---

## Features

### 1. Preview Panel (Alt+P)

A right-side panel that shows file previews inline, like Windows Explorer's preview pane.

| File type | What it shows |
|-----------|--------------|
| **Images** (JPEG, PNG, etc.) | Full-resolution preview with dynamic scaling. Drag the divider to resize in real-time. Capped at 50 MB to prevent OOM. |
| **Text files** (code, markup, logs) | Read-only text view, word-wrapped, capped at 16 KB. |
| **Directories** | Folder icon + metadata (type, size, location). |
| **Everything else** | Large file icon + metadata grid (Type, Size, Location, Info). |

- Toggle: **Alt+P** or View → Preview Pane
- Width persists between sessions (GSettings)
- Smooth rescaling while dragging the divider

### 2. Explorer Sidebar

A combined sidebar that merges the Places section (bookmarks, devices, network) with the directory tree in one scrollable panel — like Windows Explorer's navigation pane.

- **Places** on top: Favorites, mounted volumes, network locations
- **Tree** below: Full directory tree with expand/collapse
- Single unified scrollbar (no double scrolling)
- **Files are visible in the tree** — click a photo or document to show it in the preview panel
- Switch between Places / Tree / Explorer via View → Sidebar

---

## Quick Install (LMDE7 / Debian 13)

Copy-paste the whole block:

```bash
# 1. Install build dependencies
sudo apt update
sudo apt install -y git meson ninja-build gcc \
    libgtk-3-dev libglib2.0-dev libcinnamon-desktop-dev \
    libxapp-dev libexif-dev libexempi-dev libgsf-1-dev \
    libjson-glib-dev gobject-introspection libgirepository1.0-dev

# 2. Clone and build
git clone https://github.com/<your-username>/nemo-hack.git
cd nemo-hack
rm -rf build/*
cd build && meson setup ../src --prefix=/usr \
    -Dempty_view=false -Dselinux=false -Dgtk_doc=false \
    -Dtracker=false -Dexif=true -Dxmp=true
cd ..
ninja -C build -j4

# 3. Compile local GSettings schema
./compile-schema.sh

# 4. Run (does NOT replace system Nemo)
./run.sh ~
```

To verify it compiled correctly:

```bash
ninja -C build -j4 2>&1 | grep -E 'error|warning' || echo "Build clean ✓"
```

---

## Project Structure

```
nemo-hack/
├── src/                         ← Nemo source (from linuxmint/nemo)
│   └── src/
│       ├── nemo-preview-panel.c/h  ← Feature 1: Preview panel widget
│       ├── nemo-explorer-sidebar.c/h ← Feature 2: Composite sidebar
│       ├── nemo-window.c           ← Window layout + integration
│       ├── nemo-window-menus.c     ← Menu entries for both features
│       ├── nemo-application.c      ← App startup
│       └── nemo-tree-sidebar.c     ← FMTreeView (shows files now)
├── build/                        ← Build output (gitignored)
├── compile-schema.sh             ← Compile GSettings schema locally
├── run.sh                        ← Run with GSETTINGS_SCHEMA_DIR set
├── BUILD_SPEC.md                 ← Full implementation spec
├── AUDIT.md                      ← Technical audit & scorecard
└── PLAN.md                       ← Original planning doc
```

---

## Build system

| Tool | Version |
|------|---------|
| Language | C (C99/GNU) |
| UI Toolkit | GTK3 + GLib + GObject |
| Build | Meson 1.7+ / Ninja 1.12+ |
| Preferences | GSettings (dconf) |
| Desktop | Cinnamon (LMDE7) |

---

## Running

Always use `./run.sh` — it sets `GSETTINGS_SCHEMA_DIR` so Nemo picks up our custom schema keys:

```bash
./run.sh ~          # Open home folder
./run.sh /path      # Open specific folder
```

Without the env var you'll crash with a GSettings error.

---

## License

GNU General Public License v2. Same as Nemo upstream.

This project is a fork of [linuxmint/nemo](https://github.com/linuxmint/nemo). All modifications are GPLv2.
