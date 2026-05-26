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

### 4.1 Feature 2: Combined Explorer Sidebar ✅

**Goal:** Create a new sidebar type "explorer" that combines both the places (bookmarks, devices) and the tree sidebar into a single scrollable panel, like Windows Explorer's navigation pane.

**Architecture:** A single GtkScrolledWindow contains both the full NemoPlacesSidebar (top, fixed height) and the full FMTreeView (bottom, expands to fill space), separated by a GtkSeparator. Both child widgets have their internal scrolling disabled (GTK_POLICY_NEVER) so the sidebar scrolls as one unified panel. Both widgets retain their full signal connectivity — the tree responds to window navigation regardless of origin.

```
NemoExplorerSidebar (GtkBox, VERTICAL)
└── GtkScrolledWindow (POLICY_NEVER, POLICY_AUTOMATIC)
    └── GtkBox (VERTICAL)
        ├── NemoPlacesSidebar — POLICY_NEVER, FALSE/FALSE pack
        ├── GtkSeparator
        └── FMTreeView — POLICY_NEVER, TRUE/TRUE pack
```

This approach reuses 100% of the proven NemoPlacesSidebar and FMTreeView widgets rather than building a bespoke places list and tree from scratch. The shared scrolled window gives the unified-scroll behavior of Windows Explorer's navigation pane.

#### 4.1.1 Files

**`src/src/nemo-explorer-sidebar.c`** — 155-line GObject widget (GtkBox, VERTICAL):
- `_init()`: creates the outer GtkScrolledWindow and content GtkBox
- `_new(window)`: creates places sidebar, separator, tree sidebar; disables internal scrolling on both; packs into content box
- `_dispose()`: clears child widget references

**`src/src/nemo-explorer-sidebar.h`** — Public header (unchanged)

#### 4.1.2 Implementation Tasks (all complete)

1. ✅ Create the widget skeleton — G_DEFINE_TYPE_WITH_PRIVATE, dispose, class_init, init
2. ✅ Create shared GtkScrolledWindow + content GtkBox in _init()
3. ✅ In _new(): create places sidebar, disable internal scrolling, pack FALSE/FALSE
4. ✅ Add GtkSeparator between places and tree
5. ✅ In _new(): create tree sidebar (FMTreeView), disable internal scrolling, pack TRUE/TRUE
6. ✅ Wire into sidebar system: sidebar_id_is_valid, set_up_sidebar, menus, GSettings, XML
7. ✅ Add to nemoCommon_sources in meson.build

#### 4.1.3 Wiring Checklist

| Step | File | Change | Status |
|------|------|--------|--------|
| 1 | nemo-explorer-sidebar.c | Composite widget implementation | ✅ |
| 2 | nemo-explorer-sidebar.h | Public header | ✅ |
| 3 | src/meson.build | Add to nemoCommon_sources | ✅ |
| 4 | nemo-window.h | Add SIDEBAR_EXPLORER constant | ✅ |
| 5 | nemo-window.c | Extend sidebar_id_is_valid(), add set_up case | ✅ |
| 6 | nemo-window-menus.c | Add enum, menu entry | ✅ |
| 7 | nemo-shell-ui.xml | Add `<menuitem>` | ✅ |
| 8 | org.nemo.gschema.xml | Add 'explorer' to choices | ✅ |

---

### 4.2 Feature 3: Windows CSS Theme ❌ (dropped)

**Status:** Dropped per user decision (2026-05-25). Focus is on Features 1 and 2 only.

---

## 5. FILES TO CREATE (exact paths, in order)

```
Feature 2 — Combined Explorer Sidebar:
  1.  src/src/nemo-explorer-sidebar.h           ← Header (macros, typedef, public API)
  2.  src/src/nemo-explorer-sidebar.c           ← Composite widget implementation
  3.  [modify] src/src/nemo-window.h            ← Add SIDEBAR_EXPLORER constant
  4.  [modify] src/src/nemo-window.c            ← Extend sidebar_id_is_valid + set_up
  5.  [modify] src/src/nemo-window-menus.c      ← Add enum, menu entry
  6.  [modify] src/gresources/nemo-shell-ui.xml ← Add <menuitem>
  7.  [modify] src/libnemo-private/org.nemo.gschema.xml ← Add to choices
  8.  [modify] src/src/meson.build              ← Add to nemoCommon_sources
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
|[ ] ninja -j4            — 0 errors, 0 warnings
|[ ] ./compile-schema.sh  — schema compiles without errors
|[ ] Binary starts        — GSETTINGS_SCHEMA_DIR=$(pwd)/build/schemas ./build/src/nemo ~
|[ ] Preview panel        — Alt+P toggles, file selection updates preview
|[ ] Explorer sidebar     — View → Explorer shows combined places+tree
|[ ] Sidebar navigation   — Click bookmark → navigates content area
|[ ] Tree navigation      — Expand/collapse, click → navigate
|[ ] Sidebar persistence  — Selection remembered after closing and reopening window
|[ ] No regressions       — Places sidebar still works, tree sidebar still works
|[ ] Preview panel + sidebar coexist — Both features work simultaneously
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
