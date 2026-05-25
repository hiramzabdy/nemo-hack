# BUILD_SPEC: Nemo File Manager → Windows Explorer Mod

> Single-page executable build spec for AI coding agents.
> Read once. All context is here. Build in order. Quality gates at the end.

---

## 0. CONTEXT

**What this is:** A modified build of the Nemo file manager (Cinnamon desktop, Linux Mint) that adds Windows Explorer-style features: a right-side preview panel (Alt+P), a combined places+tree sidebar, and a Windows 11-like CSS theme. The modified binary runs locally without replacing the system Nemo.

**Strategic context:** This is a showcase/hobby project — an exploration of hacking on GTK3 C desktop apps. The goal is to learn how a major GTK application works and make meaningful UI modifications. Desktop Linux users coming from Windows find Nemo's sidebar and lack of preview panel limiting.

**Who uses it:** Developer testing locally. Eventually, any Linux user who wants a file manager that feels familiar coming from Windows Explorer.

**What this replaces:** Nope — this is an experimental fork, built and run locally. The system Nemo is never replaced. The dev binary lives in `build/src/nemo` and runs with custom GSettings schemas.

---

## 1. STACK

| Layer | Tech | Version |
|-------|------|---------|
| Language | C (C99/GNU) | GCC compiler |
| UI Toolkit | GTK3 + GLib + GObject | System installed |
| Build System | Meson + Ninja | meson 1.7, ninja 1.12 |
| Image Loading | GdkPixbuf | System installed |
| Preferences | GSettings (dconf) | System installed |
| Desktop | Cinnamon (Linux Mint) | LMDE7 |

**Rules:**
- No system install — binary runs from `build/src/nemo`
- GSettings schema compiled locally, not installed system-wide
- GObject widgets use `G_DEFINE_TYPE_WITH_PRIVATE` (never plain `G_DEFINE_TYPE`)
- GNOME code style: tabs, 8-space indent, braces on same line

---

## 2. PROJECT STRUCTURE

```
~/showcase/nemo-hack/               ← Project root (git repo)
├── .gitignore                       ← Ignores build/, schemas/
├── PLAN.md                          ← Original feature plan
├── AUDIT.md                         ← Technical audit (6.5/10)
├── compile-schema.sh                ← Script to compile GSettings locally
├── build/                           ← Build output (gitignored)
│   ├── src/nemo                     ← Compiled binary
│   └── schemas/
│       └── gschemas.compiled        ← Local GSettings schema
└── src/                             ← Nemo source (from github.com/linuxmint/nemo)
    ├── src/                         ← Main application code
    │   ├── nemo-window.c            ← Main window (2903 lines)
    │   ├── nemo-window.h            ← Public window API
    │   ├── nemo-window-private.h    ← WindowDetails struct
    │   ├── nemo-window-menus.c      ← Menu entries and callbacks (2178 lines)
    │   ├── nemo-preview-panel.c     ← Preview panel widget (649 lines) [NEW]
    │   ├── nemo-preview-panel.h     ← Preview panel API [NEW]
    │   ├── nemo-places-sidebar.c    ← Places sidebar (bookmarks)
    │   ├── nemo-tree-sidebar.c      ← Tree sidebar
    │   ├── nemo-view.c              ← File list view, selection signal
    │   ├── nemo-application.c       ← App startup
    │   └── meson.build              ← Source registration
    ├── libnemo-private/
    │   ├── nemo-global-preferences.h ← GSettings key #defines
    │   ├── nemo-file.h              ← NemoFile API
    │   └── org.nemo.gschema.xml     ← GSettings schema
    └── gresources/
        └── nemo-shell-ui.xml        ← Menu bar UI layout
```

---

## 3. WHAT'S ALREADY BUILT (Feature 1: Preview Panel) ✅

### 3.1 Layout Change

The window now has a nested paned layout:

```
GtkGrid (vertical)
├── GtkMenuBar
├── GtkBox (toolbar_holder)
├── GtkPaned content_paned (HORIZONTAL)    ← main sidebar/content split
│   ├── PACK1: GtkBox sidebar
│   └── PACK2: GtkBox vbox
│       └── GtkPaned preview_paned (HORIZONTAL)  ← NEW
│           ├── PACK1: split_view_hpane (file listing)
│           └── PACK2: GtkScrolledWindow
│               └── NemoPreviewPanel widget
├── GtkSeparator
└── NemoStatusBar
```

### 3.2 Preview Panel Widget

`src/src/nemo-preview-panel.c` — 649-line GObject widget extending GtkBox:

- **Header row:** File icon (32px) + filename + mime type
- **Image files (image/*):** Full-resolution pixbuf with dynamic scaling. Uses `size-allocate` callback for responsive resize. No upscale beyond native resolution. 280px default width.
- **Text files (text/*, application/json, application/xml, etc.):** GtkTextView in GtkScrolledWindow, capped at 16KB, null-byte replacement, read-only.
- **Other files:** Large icon (128px) fallback + metadata grid (Type, Size, Location, Info).
- **No selection:** Placeholder label "Select a file to preview".
- **Multi-select:** Clears preview.

### 3.3 Toggle & Persistence

- **Keyboard:** Alt+P
- **Menu:** View → Preview Pane (toggle action in nemo-window-menus.c, XML entry in nemo-shell-ui.xml line 76)
- **GSettings:** `start-with-preview-panel` (bool, default false), `preview-panel-width` (int, default 280)
- **Persistence:** Width saved on paned handle drag. Visibility restored on window show via `nemo_window_show()`.

### 3.4 Selection Tracking

`preview_selection_changed_callback()` in nemo-window.c (line 1710):
- Connected in `nemo_window_connect_content_view()` — fires on view slot changes
- Disconnected in `nemo_window_disconnect_content_view()` — prevents stale handlers
- Only updates when panel is visible (`show_preview_panel == TRUE`)
- Calls `nemo_preview_panel_set_file()` for single selection, `_clear()` otherwise

---

## 4. REMAINING FEATURES

### 4.1 Feature 2: Combined Explorer Sidebar ❌

**Goal:** Create a new sidebar type "explorer" that combines both the places (bookmarks, devices) and the tree sidebar into a single scrollable panel, like Windows Explorer's navigation pane.

**Architecture decision:**
Option chosen: **New sidebar type "explorer"** — not modifying existing places or tree sidebars. This avoids breaking the original modes.

#### 4.1.1 New Files

**`src/src/nemo-explorer-sidebar.c`** — New GObject widget (GtkBox, VERTICAL):
```
NemoExplorerSidebar (GtkBox, VERTICAL)
├── GtkScrolledWindow
│   ├── Places section
│   │   ├── Favorites/Bookmarks (top)
│   │   │   — Use NemoBookmarkList + volume monitor
│   │   │   — Drag-and-drop reordering
│   │   ├── GtkSeparator
│   │   └── Devices/Network section
│   │       — Mounted volumes
│   │       — Network locations
│   ├── GtkSeparator
│   └── Tree section (bottom, expands to fill remaining space)
│       └── FMTreeView — populate from model via fm_tree_model_get_default()
```

**`src/src/nemo-explorer-sidebar.h`** — Public header:
```c
#define NEMO_TYPE_EXPLORER_SIDEBAR (nemo_explorer_sidebar_get_type())
G_DECLARE_FINAL_TYPE (NemoExplorerSidebar, nemo_explorer_sidebar,
                      NEMO, EXPLORER_SIDEBAR, GtkBox)
GtkWidget * nemo_explorer_sidebar_new (NemoWindow *window);
```

#### 4.1.2 Implementation Tasks

**Task 1: Create the widget skeleton**
- Create `nemo-explorer-sidebar.c` with `G_DEFINE_TYPE_WITH_PRIVATE`
- Create `nemo-explorer-sidebar.h` with proper macros
- Implement `_init()`: create vertical GtkBox, add GtkScrolledWindow
- Implement `_new(window)`: pass window pointer to private struct
- Add to `nemoCommon_sources` in `src/src/meson.build`

**Task 2: Add places section**
- In `_init()`: create the places GtkBox (VERTICAL) inside scrolled window
- Add header label "Quick access" (like Windows)
- Import `NemoBookmarkList` from `nemo-bookmark-list.h`
- Use `nemo_bookmark_list_new()` and populate
- Connect signals: bookmark click → navigate in window
- Add volume monitor: use `g_volume_monitor_get()` to list drives
- Reference: `nemo-places-sidebar.c` for bookmark list patterns

**Task 3: Add tree section**
- Create FMTreeView below a GtkSeparator
- Use `fm_tree_view_new()` from `nemo-tree-sidebar.h`
- Call `fm_tree_view_set_parent_window(tree_view, window)` to wire navigation
- Set `vexpand = TRUE` so tree fills remaining space
- Reference: `nemo-tree-sidebar.c` setup code

**Task 4: Wire into sidebar system**
In `src/src/nemo-window.c`:
- Add `#include "nemo-explorer-sidebar.h"`
- Extend `sidebar_id_is_valid()` to accept `NEMO_WINDOW_SIDEBAR_EXPLORER`
- Add `else if` branch in `nemo_window_set_up_sidebar()` to instantiate `nemo_explorer_sidebar_new(window)`

In `src/src/nemo-window.h`:
- Add constant: `#define NEMO_WINDOW_SIDEBAR_EXPLORER "explorer"`

In `src/src/nemo-window-menus.c`:
- Add `SIDEBAR_EXPLORER` to the `SidebarRadio` enum
- Add entry in `sidebar_radio_entries[]` array:
```c
{ "explorer", N_("E_xplorer"), N_("Show combined places and tree sidebar"),
  NEMO_WINDOW_SIDEBAR_EXPLORER, SIDEBAR_EXPLORER },
```

In `src/gresources/nemo-shell-ui.xml`:
- Add `<menuitem>` inside the `<placeholder name='Sidebar Radio Placeholder'>` block

In `src/libnemo-private/org.nemo.gschema.xml`:
- Add `'explorer'` to the `<choices>` for `side-pane-view` key

#### 4.1.3 Wiring Checklist for New Sidebar Type

| Step | File | Change |
|------|------|--------|
| 1 | nemo-explorer-sidebar.c | New widget implementation |
| 2 | nemo-explorer-sidebar.h | Public header |
| 3 | src/meson.build | Add to nemoCommon_sources |
| 4 | nemo-window.h | Add SIDEBAR_EXPLORER constant |
| 5 | nemo-window.c | Extend sidebar_id_is_valid(), add set_up case |
| 6 | nemo-window-menus.c | Add enum, menu entry |
| 7 | nemo-shell-ui.xml | Add `<menuitem>` |
| 8 | org.nemo.gschema.xml | Add 'explorer' to choices |

---

### 4.2 Feature 3: Windows CSS Theme ❌

**Goal:** Apply a GTK CSS stylesheet at startup that makes Nemo look like Windows 11 File Explorer.

#### 4.2.1 Design Tokens — Windows 11 Explorer

```css
/* Colors */
--explorer-bg:           #fafafa;      /* Main background - Windows light */
--explorer-sidebar-bg:   #f0f0f0;      /* Sidebar background - slightly darker */
--explorer-sidebar-hover:#e5e5e5;      /* Sidebar item hover */
--explorer-sidebar-active:#cccccc;     /* Sidebar item selected */
--explorer-toolbar-bg:   #f5f5f5;      /* Toolbar/ribbon background */
--explorer-text:         #1a1a1a;      /* Primary text */
--explorer-text-secondary:#666666;     /* Secondary text */
--explorer-accent:       #005fb8;      /* Windows accent blue */
--explorer-border:       #e0e0e0;      /* Panel borders */
--explorer-separator:    #d0d0d0;      /* Separator lines */
--explorer-path-bg:      #ffffff;      /* Breadcrumb pathbar background */
--explorer-path-border:  #d0d0d0;      /* Pathbar border */
--explorer-list-hover:   #e8f0fe;      /* File list row hover (light blue) */
--explorer-list-select:  #cce4f7;      /* File list row selected */
--explorer-preview-bg:   #fafafa;      /* Preview panel background */
--explorer-preview-label:#999999;      /* Preview panel labels */

/* Typography */
--font-ui: "Segoe UI", "Cantarell", "Noto Sans", sans-serif;
--font-mono: "Cascadia Code", "Consolas", monospace;
--font-size-ui: 12px;
--font-size-heading: 14px;

/* Spacing */
--sidebar-item-padding: 4px 8px;
--sidebar-item-height: 24px;
--toolbar-height: 40px;
--pathbar-height: 32px;
--panel-border-width: 1px;

/* Effects */
--transition-fast: 100ms ease;
--transition-normal: 200ms ease;
--sidebar-radius: 4px;
```

#### 4.2.2 New File

**`src/data/nemo-windows.css`** — CSS stylesheet (200-300 lines)

Key targets and CSS selectors:

```css
/* === SIDEBAR === */
.nemo-places-sidebar { background: @explorer-sidebar-bg; }
.nemo-places-sidebar row { padding: 4px 8px; min-height: 24px; border-radius: 4px; }
.nemo-places-sidebar row:hover { background: @explorer-sidebar-hover; }
.nemo-places-sidebar row:selected { background: @explorer-sidebar-active; }

/* === TREE SIDEBAR ARROWS === */
.nemo-tree-sidebar { background: @explorer-sidebar-bg; }
treeview.view { background: transparent; }
treeview.view:hover { background: @explorer-sidebar-hover; }
treeview.view:selected { background: @explorer-sidebar-active; color: @explorer-text; }
treeview expander {
    -gtk-icon-source: -gtk-icontheme("pan-end-symbolic");
    min-width: 16px; min-height: 16px;
}
treeview expander:checked {
    -gtk-icon-source: -gtk-icontheme("pan-down-symbolic");
}

/* === TOOLBAR === */
.nemo-toolbar { background: @explorer-toolbar-bg; border-bottom: 1px solid @explorer-border; min-height: 40px; }
.nemo-toolbar button { padding: 4px 10px; border-radius: 4px; }
.nemo-toolbar button:hover { background: @explorer-sidebar-hover; }

/* === PATHBAR (breadcrumb) === */
.nemo-pathbar { background: @explorer-path-bg; border: 1px solid @explorer-path-border; border-radius: 4px; }
.nemo-pathbar button { padding: 2px 8px; border-radius: 3px; }
.nemo-pathbar button:hover { background: @explorer-sidebar-hover; }

/* === STATUS BAR === */
.nemo-statusbar { background: @explorer-toolbar-bg; border-top: 1px solid @explorer-border; padding: 0 8px; }
.nemo-statusbar label { font-size: 11px; color: @explorer-text-secondary; }

/* === FILE LIST === */
.nemo-icon-view { background: @explorer-bg; }
view row:hover { background: @explorer-list-hover; }
view row:selected { background: @explorer-list-select; color: @explorer-text; }
view row { min-height: 24px; padding: 2px 4px; }

/* === PREVIEW PANEL === */
.nemo-preview-panel { background: @explorer-preview-bg; border-left: 1px solid @explorer-border; }
.nemo-preview-panel label { font-size: 12px; color: @explorer-text-secondary; }
.nemo-preview-panel GtkLabel:first-child { font-size: 14px; font-weight: bold; color: @explorer-text; }

/* === MENU BAR === */
menubar { background: @explorer-bg; border-bottom: 1px solid @explorer-border; padding: 0; }
menubar > menuitem { padding: 4px 8px; }
menubar > menuitem:hover { background: @explorer-sidebar-hover; border-radius: 4px; }

/* === SCROLLBAR === */
scrollbar slider { background: #c0c0c0; border-radius: 8px; min-width: 8px; }
scrollbar slider:hover { background: #a0a0a0; }

/* === SEPARATOR === */
separator { color: @explorer-separator; }
```

#### 4.2.3 Application Startup Injection

In `src/src/nemo-application.c`, add to the startup function:

```c
/* Apply Windows-style CSS theme */
static void
load_windows_theme (void)
{
    GtkCssProvider *provider;
    GFile *css_file;
    gchar *css_path;

    /* Look for CSS in data dir relative to build root */
    css_path = g_build_filename (DATADIR, "nemo-windows.css", NULL);

    css_file = g_file_new_for_path (css_path);
    if (g_file_query_exists (css_file, NULL)) {
        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_file (provider, css_file, NULL);
        gtk_style_context_add_provider_for_screen (
            gdk_screen_get_default (),
            GTK_STYLE_PROVIDER (provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);
    }

    g_object_unref (css_file);
    g_free (css_path);
}
```

Call `load_windows_theme()` in the `nemo_application_startup()` function.

#### 4.2.4 CSS Data Installation

In `src/data/meson.build`, add the CSS file to the data installation:

```meson
install_data(
    'nemo-windows.css',
    install_dir: pkgdatadir,
)
```

---

## 5. FILES TO CREATE (exact paths, in order)

```
Feature 2 — Combined Explorer Sidebar:
  1.  src/src/nemo-explorer-sidebar.h           ← Header (macros, typedef, public API)
  2.  src/src/nemo-explorer-sidebar.c           ← Implementation (widget, places, tree)
  3.  [modify] src/src/nemo-window.h            ← Add SIDEBAR_EXPLORER constant
  4.  [modify] src/src/nemo-window.c            ← Extend sidebar_id_is_valid + set_up
  5.  [modify] src/src/nemo-window-menus.c      ← Add enum, menu entry
  6.  [modify] src/gresources/nemo-shell-ui.xml ← Add <menuitem>
  7.  [modify] src/libnemo-private/org.nemo.gschema.xml ← Add to choices
  8.  [modify] src/src/meson.build              ← Add to nemoCommon_sources

Feature 3 — Windows CSS Theme:
  9.  src/data/nemo-windows.css                 ← Full CSS stylesheet
  10. [modify] src/src/nemo-application.c       ← Load CSS at startup
  11. [modify] src/data/meson.build             ← Install CSS to data dir
```

---

## 6. BUILD & TEST COMMANDS

### Build
```bash
cd ~/showcase/nemo-hack
# Full build
cd build && ninja -j4 && cd ..

# Compile GSettings schema (after modifying .gschema.xml)
./compile-schema.sh
```

### Run (binary runs locally, does NOT replace system Nemo)
```bash
cd ~/showcase/nemo-hack
GSETTINGS_SCHEMA_DIR=$(pwd)/build/schemas ./build/src/nemo ~
```

### Debug crash
```bash
cd ~/showcase/nemo-hack
gdb -batch -ex "run ~" -ex "bt" --args ./build/src/nemo ~
```

### Incremental build (after modifying C files)
```bash
cd build && ninja -j4
```

### Reconfigure build (after adding new source files to meson.build)
```bash
rm -rf build/* && cd build && meson setup ../src --prefix=/usr -Dempty_view=false -Dselinux=false -Dgtk_doc=false -Dtracker=false -Dexif=true -Dxmp=true && cd ..
```

---

## 7. ARCHITECTURE REFERENCE

### 7.1 How Sidebar Types Work

1. Sidebar switch: `nemo_window_set_sidebar_id(window, "explorer")` sets GSettings property
2. `notify::sidebar-view-id` signal → `side_pane_id_changed()` callback
3. Destroys old sidebar: `nemo_window_tear_down_sidebar()`
4. Creates new sidebar: `nemo_window_set_up_sidebar()` — this is where to add the `"explorer"` case
5. New widget is packed into `content_paned` pack1

### 7.2 GObject Widget Pattern

```c
// In .c file:
struct _NemoExplorerSidebarPrivate {
    NemoWindow *window;
    GtkWidget *places_section;
    GtkWidget *tree_view;
    // ... more widgets
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoExplorerSidebar, nemo_explorer_sidebar, GTK_TYPE_BOX);

static void
nemo_explorer_sidebar_init (NemoExplorerSidebar *sidebar) {
    sidebar->priv = nemo_explorer_sidebar_get_instance_private (sidebar);
    gtk_orientable_set_orientation (GTK_ORIENTABLE (sidebar), GTK_ORIENTATION_VERTICAL);
    // Build UI here
}
```

### 7.3 NemoFile API (key functions)

```c
#include <libnemo-private/nemo-file.h>

char*     nemo_file_get_uri           (NemoFile *file);
char*     nemo_file_get_display_name  (NemoFile *file);
char*     nemo_file_get_mime_type     (NemoFile *file);
goffset   nemo_file_get_size          (NemoFile *file);
GFile*    nemo_file_get_location      (NemoFile *file);
GIcon*    nemo_file_get_gicon         (NemoFile *file, gint size);
gboolean  nemo_file_is_directory      (NemoFile *file);
void      nemo_file_list_free         (GList *file_list);
```

---

## 8. KNOWN PITFALLS

| Pitfall | How to avoid |
|---------|-------------|
| G_DEFINE_TYPE segfault | ALWAYS use `G_DEFINE_TYPE_WITH_PRIVATE` when widget has private data |
| Private struct naming | `_TypeNamePrivate` → `TypeNamePrivate`. Generated accessor: `type_name_get_instance_private()` |
| GtkPaned position | Position is relative to handle, not panel edge. Account for ~6px handle width |
| GSettings not found | Must run `./compile-schema.sh` after schema changes. Binary MUST have `GSETTINGS_SCHEMA_DIR` set |
| Meson stale paths | After moving project directory, `rm -rf build/*` and reconfigure |
| Sidebar destruction | `set_up_sidebar` destroys old sidebar completely. New widget gets fresh instance |
| GTK run from build dir | Binary references resources relative to build directory. Always run from project root |
| CSS theme not applying | GTK3 CSS selectors use widget names + style classes. Use GTK Inspector (Ctrl+Shift+D) to debug |
| Missing meson.build source | New .c files must be added to `nemoCommon_sources` in `src/src/meson.build` |
| GResource XML changes | Resource file is compiled into C. Rebuild picks up changes automatically via ninja dependency tracking |

---

## 9. QUALITY GATES (all must pass before done)

```
[ ] ninja -j4            — 0 errors, 0 warnings
[ ] ./compile-schema.sh  — schema compiles without errors
[ ] Binary starts        — GSETTINGS_SCHEMA_DIR=$(pwd)/build/schemas ./build/src/nemo ~
[ ] Preview panel        — Alt+P toggles, file selection updates preview
[ ] Explorer sidebar     — View → Explorer shows combined places+tree
[ ] Sidebar navigation   — Click bookmark → navigates content area
[ ] Tree navigation      — Expand/collapse, click → navigate
[ ] Sidebar persistence  — Selection remembered after closing and reopening window
[ ] CSS theme visible    — Verify visually: menu bar, toolbar, sidebar, status bar match spec
[ ] No regressions       — Places sidebar still works, tree sidebar still works
[ ] Preview panel + CSS  — Preview panel styling matches theme
```

---

## 10. WHAT NOT TO BUILD (scope guard)

```
- ❌ Replace sidebar types — "explorer" is an ADDITIONAL option, not a replacement
- ❌ Rewrite places or tree sidebar — extend, don't modify existing code
- ❌ Dark mode — Windows 11 light theme only for now
- ❌ System-wide install — binary stays in build/, run locally
- ❌ CI/CD pipeline — local development only
- ❌ Unit tests — desktop GUI tests are out of scope for this phase
- ❌ Modify nemo-desktop — only nemo file browser windows
```

---

## 11. POST-BUILD

After quality gates pass:
1. `git add -A && git commit -m "feat: combined explorer sidebar and Windows CSS theme"`
2. Test multiple sessions: open, toggle, close, reopen — verify GSettings persistence
3. If satisfied, merge `dev` into `main` when user decides

---

## 12. REFERENCE: Window Layout (Current State)

```
nemo_window_constructed() — line 790, nemo-window.c

GtkGrid (vertical)
├── MenuBar (line 660)
├── toolbar_holder (GtkBox, line 670)
├── content_paned (GtkPaned HORIZONTAL, line 780)
│   ├── PACK1: sidebar GtkBox (line 785) — places/tree
│   └── PACK2: vbox GtkBox (line 793)
│       └── preview_paned (GtkPaned HORIZONTAL, line 803) ← added by us
│           ├── PACK1: hpaned → split_view_hpane (line 811)
│           │   └── NemoWindowPane(s) → NemoNotebook → NemoWindowSlot → NemoView
│           └── PACK2: GtkScrolledWindow (line 818) ← added by us
│               └── NemoPreviewPanel (line 824) ← added by us
├── GtkSeparator (line 856)
└── NemoStatusBar (line 853)
```

---

## 13. GSettings Schema (relevant keys)

```xml
<!-- In org.nemo.gschema.xml -->
<key name="side-pane-view" type="s">
  <default>'places'</default>
  <choices>
    <choice value='places'/>
    <choice value='tree'/>
    <choice value='explorer'/>   <!-- ADD THIS for Feature 2 -->
  </choices>
</key>
<key name="start-with-preview-panel" type="b">
  <default>false</default>
</key>
<key name="preview-panel-width" type="i">
  <default>280</default>
</key>
```
