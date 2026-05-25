# Nemo → Windows Explorer Mod Project

**Goal**: Modify Nemo (Cinnamon file manager) GPLv2 source to behave
more like Windows File Explorer without replacing the system Nemo.

## Architecture Overview

Current window layout (nemo-window.c `nemo_window_constructed`):

```
GtkGrid (vertical)
 ├── GtkMenuBar
 ├── GtkBox (toolbar_holder)
 ├── GtkPaned *content_paned (HORIZONTAL)      ← main split
 │    ├── PACK1: GtkBox *sidebar (places or tree)
 │    └── PACK2: GtkBox *vbox
 │         └── GtkPaned *split_view_hpane (HORIZONTAL)
 │              └── NemoWindowPane(s) via notebook
 ├── GtkSeparator
 └── GtkEventBox > NemoStatusBar
```

## Feature 1: Preview Panel (right side)

Add a right-side preview pane like Windows Explorer (Alt+P).

### Layout Change

Insert a new GtkPaned inside `vbox`: split the space between
the existing split_view_hpane and the new preview panel:

```
GtkPaned *preview_paned (HORIZONTAL)  ← new, inside vbox
 ├── PACK1: [existing split_view_hpane]  (content listing)
 └── PACK2: NemoPreviewPanel widget       (file preview)
```

- Default: position at max (preview hidden)
- Toggle: Alt+P or View menu → Preview Pane → sets position
- Width persisted in GSettings

### Preview Panel Widget (NemoPreviewPanel)

New files: `src/nemo-preview-panel.c` + `src/nemo-preview-panel.h`

A GtkBox (VERTICAL) inside a GtkScrolledWindow that displays:
- **Image files** → GtkImage with pixbuf scaled to fit
- **Text files** → GtkTextView (plain text)
- **Directories** → nemo_file_get size/item count info
- **Fallback** → file icon + metadata (type, size, modified, permissions)

Selection tracking:
- Connect to `nemo_view` selection-changed signal
- On single file selected → update preview
- On nothing selected → show "No file selected" placeholder

### Files to modify:
- `src/nemo-window.c` / `.h` — layout changes, new GtkPaned, toggle action
- `src/nemo-window-private.h` — store preview_paned, preview_panel widgets
- `src/nemo-window-menus.c` — View menu toggle for Preview Pane
- `src/meson.build` — add `nemo-preview-panel.c`
- `libnemo-private/org.nemo.gschema.xml` — preview-pane-width GSettings key
- `src/nemo-view.c` — connect selection-changed → update preview panel

## Feature 2: Windows Explorer Navigation Pane

Windows navigation pane combines favorites/bookmarks (top) with
a directory tree (bottom). Currently Nemo has two separate sidebar modes
("places" and "tree") that exclude each other.

### Combined Sidebar Approach

Option chosen: **Create a new sidebar type "explorer"** that embeds
both the Places sidebar's bookmark/favorites section AND the
Tree sidebar's directory tree in a single scrollable panel.

New files: `src/nemo-explorer-sidebar.c` + `src/nemo-explorer-sidebar.h`

Layout:
```
NemoExplorerSidebar (GtkBox, VERTICAL)
 ├── GtkScrolledWindow
 │    ├── Places section (bookmarks, devices, network)
 │    │     — reuse NemoBookmarkList / volume monitor logic
 │    └── GtkSeparator
 │         Tree section — embed FMTreeView for full directory tree
 └── (scroll)
```

### Files to modify:
- `src/nemo-explorer-sidebar.c` / `.h` — NEW: combined sidebar
- `src/nemo-window.c` → add "explorer" as valid sidebar_id
- `src/nemo-window-menus.c` → add SIDEBAR_EXPLORER enum, menu entry
- `src/meson.build` → add source
- `libnemo-private/org.nemo.gschema.xml` → add "explorer" choice to side-pane-view

## Feature 3: Appearance (GTK CSS)

A CSS stylesheet that makes Nemo look more like Windows Explorer.

### `data/nemo-windows.css` — injected programmatically at startup

Targets:
- Sidebar background, selection colors, tree expander arrows
- Pathbar styling (breadcrumb → Windows-style path segments)
- Toolbar icon spacing and sizing
- Status bar styling
- File list row heights and hover colors
- Preview panel styling

### Files to modify:
- `src/nemo-application.c` — load CSS at startup via `gtk_css_provider_new()`
- `data/nemo-windows.css` — NEW: the Windows 11/10 inspired theme
- `data/meson.build` — install CSS file

## Build & Test Workflow

```bash
cd ~/Projects/nemo-hack/build
ninja -j4                    # compile
./src/nemo                   # run dev build (does NOT replace system Nemo)
```

Run from build directory to test without installing. The binary
in `build/src/nemo` is self-contained for testing.
