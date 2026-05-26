# Project Technical Audit — nemo-hack

**Date:** 2026-05-25
**Branch:** dev (dirty working tree — 8 modified, 2 untracked)
**Scope:** Full codebase audit — all 3 features assessed

---

## Executive Summary

Overall health: **GOOD** (risk: LOW-MED). Feature 1 (preview panel) is complete and solid. Feature 3 (CSS theme) is complete and well-implemented. Feature 2 (explorer sidebar) is in a **working but architecturally risky state** — it compiles and runs by stacking two full sidebar widgets in a paned, but this creates signal duplication, potential double-navigation bugs, and doesn't match the BUILD_SPEC's composite-widget design. The build compiles with 0 warnings and 0 errors. Six issues from the previous audit (2026-05-25) remain unaddressed.

**Decision needed:** Ship the current explorer sidebar as-is (risk: subtle signal bugs at runtime), rework it as a proper composite widget per the BUILD_SPEC, or split Feature 2 into its own branch and merge Features 1+3 now.

---

## 1. Project Structure & Architecture (7/10)

| Metric | Value |
|---|---|
| Total source files | 365 .c/.h files (upstream Nemo) |
| Modified files (Feature 1) | 7 (nemo-window.c/h, nemo-window-private.h, nemo-window-menus.c, meson.build, gschema.xml, nemo-global-preferences.h) |
| New files (Feature 1) | 2 (nemo-preview-panel.c, nemo-preview-panel.h) |
| Modified files (Feature 2) | 6 (nemo-window.c/h, nemo-window-menus.c, nemo-shell-ui.xml, gschema.xml, meson.build) |
| New files (Feature 2) | 2 (nemo-explorer-sidebar.c, nemo-explorer-sidebar.h) |
| Modified files (Feature 3) | 2 (nemo-application.c, data/meson.build) |
| New files (Feature 3) | 1 (nemo-windows.css) |
| Architecture pattern | GObject subclass in separate .c/.h — correct |
| Widget hierarchy | content_paned → vbox → preview_paned → (split_view_hpane | preview_scrolled) |

**Observations:**
- Feature 1's widget is properly separated from window code. Clean GObject implementation.
- Feature 2's explorer sidebar header uses `G_DECLARE_FINAL_TYPE` (GLib 2.44+) while the .c uses `G_DEFINE_TYPE_WITH_PRIVATE`. This is correct — `G_DECLARE_FINAL_TYPE` is the modern header macro and doesn't affect the .c implementation choice.
- Build is 278 targets, 0 warnings, 0 errors.

---

## 2. Dependencies Health (9/10)

| Metric | Value |
|---|---|
| Build system | Meson 1.7.0 + Ninja 1.12.1 |
| Compiler | GCC |
| Key deps | GTK3, GLib 2.84, GDK-Pixbuf, GSettings/dconf |
| Missing deps | None |

**Observations:**
- All system packages. No vendored deps. Good for LMDE stability.
- No new dependencies introduced by any of the 3 features.

---

## 3. Configuration & Build (8/10)

| Metric | Value |
|---|---|
| Build output | ✓ 278/278 targets, 0 errors, 0 warnings |
| Binary location | build/src/nemo (ELF 64-bit, debug info, not stripped, 6.5 MB) |
| Schema compilation | ✓ compile-schema.sh works, executable |
| GSettings keys | 2 new: start-with-preview-panel, preview-panel-width |
| Schema choices | 1 new: side-pane-view 'explorer' choice added |
| CSS installation | ✓ meson.build installs nemo-windows.css to pkgdatadir |
| File permissions | ✓ compile-schema.sh is now executable |

**Observations:**
- NEMO_DATADIR fallback in `load_windows_theme()` points to `/usr/share/nemo` — will never find the CSS file since we never install system-wide. The multi-path fallback (`nemo-windows.css`, `src/data/nemo-windows.css`, `data/nemo-windows.css`) compensates for this.
- Build from May 2026 audit (stale paths) was already fixed.

---

## 4. Feature Status — Detailed Assessment

### 4.1 Feature 1: Preview Panel ✅ COMPLETE

**nemo-preview-panel.c** (649 lines):

| Aspect | Status |
|---|---|
| GObject boilerplate | ✓ Clean — G_DEFINE_TYPE_WITH_PRIVATE, dispose, class_init, init |
| Image preview | ✓ Full-resolution loading + dynamic scale via size-allocate callback |
| Text preview | ✓ 16KB cap, null-byte replacement, read-only, word-wrap |
| Metadata display | ✓ Type, size, location URI, description |
| Placeholder state | ✓ "Select a file to preview" |
| Multi-select handling | ✓ Clears on multi-select, shows on single select |
| Error handling | ✓ Warns on load failure, falls back to large icon |
| Memory management | ✓ g_clear_object, g_object_unref on current_file, g_free on strings |

**nemo-window.c integration:**

| Aspect | Status |
|---|---|
| Preview paned nesting | ✓ Clean block-scoped construction, correct pack1/pack2 |
| Toggle function | ✓ Handles show (restore width) and hide (save width, push to max) |
| Startup restore | ✓ nemo_window_show reads GSettings and toggles preview |
| Selection tracking | ✓ Connected in nemo_window_connect_content_view, disconnected on teardown |
| Width persistence | ✓ preview_paned_position_changed_cb saves on handle drag |

### 4.2 Feature 2: Explorer Sidebar 🟡 PARTIAL

**nemo-explorer-sidebar.c** (88 lines):

| Aspect | Status |
|---|---|
| Header boilerplate | ✓ G_DECLARE_FINAL_TYPE + copyright headers |
| dispose() | ✓ properly clears places_sidebar and tree_sidebar |
| class_init/init | ✓ sets vertical orientation, adds CSS class |
| _new() constructor | ✓ Creates VPaned, packs places in pack1, tree in pack2 |
| Wiring (7 files) | ✓ All 7 wiring points in BUILD_SPEC implemented correctly |
| Build compilation | ✓ Compiles, links, 0 warnings |

**Architectural concern — 🔴 CRITICAL:**

The current implementation creates both `nemo_places_sidebar_new(window)` and `nemo_tree_sidebar_new(window)` simultaneously inside a GtkPaned. This means:

1. **Double signal subscriptions**: Both sidebar widgets connect to the same NemoWindow signals (navigation, selection, volume changes). The tree sidebar's `fm_tree_model_get_default()` subscribes to the global tree model. The places sidebar's volume monitor subscribes to mount/unmount events. Both are active simultaneously.

2. **Navigation conflict risk**: When the user clicks a bookmark in the places section, that triggers `nemo_window_slot_go_to()`. The tree sidebar is also listening and will try to expand/select the same path. While likely benign in practice (GTK's main loop serializes this), it's wasteful and could cause race conditions on slow filesystems.

3. **Resource waste**: Two full sidebar widgets with duplicate GSettings watchers, signal handlers, and tree model subscriptions. Memory and CPU overhead.

4. **Teardown correctness**: `nemo_window_tear_down_sidebar()` calls `gtk_widget_destroy()` on the outer explorer sidebar, which should cascade-destroy both child sidebars. The places and tree sidebars themselves have their own dispose handlers. This should work correctly through GTK's parent-child destroy propagation, but it's untested.

**Comparison to BUILD_SPEC:**

The spec (Section 4.1.1) calls for a **bespoke composite widget** with:
- Custom places list (NemoBookmarkList + volume monitor, NOT the full NemoPlacesSidebar)
- Custom FMTreeView (just the tree, NOT the full NemoTreeSidebar)
- Both sharing a single GtkScrolledWindow

The current implementation uses complete, off-the-shelf sidebar widgets instead. This is a **simpler approach that may actually be preferable** — less code, less maintenance, proven widgets. But the spec should be updated to reflect this design decision, and the signal duplication should be explicitly documented as a known tradeoff.

**Verdict:** Compiles and likely works for basic navigation, but the double-sidebar architecture needs either validation (test that click-in-places doesn't trigger tree misbehavior) or a spec update documenting the tradeoff.

### 4.3 Feature 3: Windows CSS Theme ✅ COMPLETE

**nemo-windows.css** (190 lines):

| Aspect | Status |
|---|---|
| Design tokens | ✓ 16 CSS variables matching Windows 11 palette |
| Sidebar styling | ✓ Places, tree, explorer sidebars all targeted |
| Toolbar styling | ✓ Background, buttons, hover states |
| Pathbar styling | ✓ Breadcrumb-style pathbar with rounded border |
| File list styling | ✓ Row hover/selection, min-height |
| Preview panel styling | ✓ Background, labels, border-left |
| Menu bar styling | ✓ Background, border, item hover |
| Scrollbar styling | ✓ Slim rounded scrollbar (Windows 11 style) |
| Separator styling | ✓ Color via CSS variable |
| Window styling | ✓ nemo-window, nemo-window-pane backgrounds |

**nemo-application.c loader:**

| Aspect | Status |
|---|---|
| Multi-path fallback | ✓ Tries 3 relative paths before compile-time NEMO_DATADIR |
| gdk_screen_get_default | ✓ Correct for GTK3 (not GTK4's gdk_display) |
| Provider priority | ✓ GTK_STYLE_PROVIDER_PRIORITY_APPLICATION — overrides theme defaults |
| Memory management | ✓ Proper unref of provider and file |
| Silent failure | ✓ If CSS file not found, app continues without theme (no crash) |
| Startup position | ✓ Called after init_icons_and_styles(), before init_gtk_accels() |

---

## 5. Source Code Quality (7/10)

### Preview panel strengths:
- Clean separation: each file type has its own `show_*_preview()` function
- Null safety: proper g_return_if_fail guards on public API
- Memory: pixbuf lifecycle managed via g_clear_object
- Text: null-byte sanitization, 16KB cap, read-only mode

### Preview panel issues (from previous audit, still present):

1. 🟡 **Duplicate image scaling logic** — `show_image_preview()` (lines 260-287) and `preview_image_size_allocate_cb()` (lines 176-201) contain ~25 lines of identical scaling math. Should be extracted to `scale_pixbuf_to_width()`.

2. 🟡 **No image file size cap** — `gdk_pixbuf_new_from_file()` loads entire file into memory. A 200MB TIFF → 200MB+ allocation. Should check file size via `g_file_query_info()` before loading.

3. 🟡 **metadata_grid rebuilt every selection** — `show_file_metadata()` calls `gtk_widget_destroy()` + rebuilds grid. For files browsed rapidly, this is wasteful. Should update labels in-place.

4. 🟠 **No directory-specific preview** — Directories fall through to icon+metadata fallback. No item count, no summary.

5. 🟠 **Text I/O on main thread** — `g_file_get_contents()` blocks UI. At 16KB it's negligible but worth noting.

6. 🔵 **Magic numbers** — `128` (icon), `280` (default width), `16` (padding) scattered without #defines.

### Explorer sidebar issues:

7. 🟡 **Private struct access inconsistency** — The .c file defines `NemoExplorerSidebarPrivate` as a standalone typedef but uses `G_DEFINE_TYPE_WITH_PRIVATE`. Private is accessed via `nemo_explorer_sidebar_get_instance_private()`. However, `priv` is assigned in `_new()` but never in `_init()`. If `_init()` ever needs private data (e.g., for signal handlers set up at init time), it would crash. Currently safe because `_init()` only sets orientation and CSS class, which don't need private data.

8. 🔵 **Header uses G_DECLARE_FINAL_TYPE** — The .c file, however, has the struct definition as `struct _NemoExplorerSidebar { GtkBox parent_instance; };` which is the pattern for `G_DECLARE_FINAL_TYPE`. The `G_DEFINE_TYPE_WITH_PRIVATE` in the .c works with this because GLib generates the private offset regardless. Not a bug, but inconsistent with other Nemo widgets that use `G_DEFINE_TYPE_WITH_PRIVATE` in the .h too.

### CSS theme issues:

9. 🔵 **Uses deprecated @define-color** — GTK3 CSS supports `@define-color` but it's GTK-specific (not standard CSS). Works fine but doesn't validate in standard CSS linters.

10. 🔵 **No dark mode** — BUILD_SPEC section 10 explicitly excludes dark mode. Fine for now.

---

## 6. Testing (2/10)

| Metric | Value |
|---|---|
| Upstream tests | test-nemo-search-engine, test-nemo-directory-async, test-nemo-copy (pass) |
| Preview panel tests | None |
| Explorer sidebar tests | None |
| CSS theme tests | None (visual-only) |
| Runtime testing | Not performed during this audit (no display) |

**Observation:** Desktop GUI apps are hard to unit test, but manual verification is essential for Feature 2 given the signal duplication concern.

---

## 7. CI/CD (0/10)

No CI pipeline. Acceptable for local-only hacking project.

---

## 8. Git Health (5/10)

| Metric | Value |
|---|---|
| Branches | 1 (dev) |
| Commits | 3 |
| Remote | None |
| Working tree | **DIRTY** — 8 modified + 2 untracked files |
| Git workflow | dev → main convention per AGENTS.md |
| .gitignore | ✓ Covers build/, schemas/, editor files |
| History | 5 patches flattened into initial commit |

**Issues:**
- 🔴 **No main branch** — Per AGENTS.md convention, `main` branch should exist. Currently only `dev`.
- 🟡 **No intermediate commits** — Features 2 and 3 are entirely in the working tree with no git history. If a build breaks, there's no granular rollback. The working tree has 69 insertions across 8 modified files plus 2 new files (278 lines total of new code).

---

## 9. Build Output (9/10)

| Metric | Value |
|---|---|
| Binary | build/src/nemo, ELF 64-bit, 6.5 MB, debug info, not stripped |
| Warnings | 0 |
| Errors | 0 |
| Compile time | < 5s incremental (ninja: no work to do) |
| Clean build | ~30s on ninja -j4 |

---

## 10. Issues Found (Prioritized)

### 🔴 Critical — Explorer sidebar double-navigation risk
**File:** `src/src/nemo-explorer-sidebar.c` (entire approach)
**Impact:** Creating two full sidebar widgets simultaneously means two sets of signal handlers, two tree model subscriptions, two volume monitors. While GTK's main loop serializes callbacks, the resource waste and potential for subtle navigation bugs (race conditions on slow mounts, double-history entries) is real.
**Recommendation:** Either (a) test thoroughly and document the tradeoff, or (b) rework as a composite widget per BUILD_SPEC. If keeping the current approach, add a comment block explaining that both sidebars are fully active and that signal deduplication is handled by GTK's main loop.

### 🔴 Critical — No git commits for Features 2+3
**Impact:** All work since the initial setup is uncommitted. 10 files, 347 lines of new code, no version history.
**Recommendation:** Commit immediately. Consider splitting into two commits: Feature 2 (explorer sidebar wiring) and Feature 3 (CSS theme).

### 🟡 Medium — Duplicate image scaling logic (Feature 1)
**File:** `src/src/nemo-preview-panel.c` lines 260-287, 176-201
**Impact:** Bugs fixed in one block won't propagate to the other. ~25 lines of near-identical code.
**Recommendation:** Extract `static GdkPixbuf *scale_pixbuf_to_width(GdkPixbuf *src, gint target_w)`.

### 🟡 Medium — No image file size cap (Feature 1)
**File:** `src/src/nemo-preview-panel.c` line 229
**Impact:** Loading a large image allocates unlimited memory. Can crash Nemo.
**Recommendation:** Call `g_file_query_info(file, "standard::size", ...)` before `gdk_pixbuf_new_from_file()`. Cap at 50 MB.

### 🟡 Medium — metadata_grid rebuilt every selection (Feature 1)
**File:** `src/src/nemo-preview-panel.c` lines 106-109
**Impact:** Widget destroy/create on every file selection change. Performance hit when rapidly browsing.
**Recommendation:** Update existing label text in-place instead of destroying/recreating.

### 🟡 Medium — Spec mismatch for Feature 2
**File:** BUILD_SPEC.md vs actual implementation
**Impact:** BUILD_SPEC specifies a composite widget with bespoke places list and FMTreeView. Implementation stacks two complete sidebar widgets. Either spec or code should be updated.
**Recommendation:** Update BUILD_SPEC section 4.1 to document the "stacked sidebar" approach and its tradeoffs, OR rework the implementation.

### 🟠 Low — No directory preview (Feature 1)
**File:** `src/src/nemo-preview-panel.c` update_preview_content()
**Impact:** Directories show only icon + metadata. Missing item count.
**Recommendation:** Add `if (nemo_file_is_directory(file))` branch showing child count.

### 🟠 Low — Text I/O on main thread (Feature 1)
**File:** `src/src/nemo-preview-panel.c` line 349
**Impact:** Synchronous file read blocks UI thread. Negligible at 16KB.
**Recommendation:** Acceptable for now. Note for future async refactor.

### 🟠 Low — No main branch
**Impact:** Violates AGENTS.md convention (dev → main).
**Recommendation:** `git branch main dev` after committing current work.

### 🔵 Info — Magic numbers
**Files:** `nemo-preview-panel.c` lines 128, 260, 265
**Recommendation:** Define `PREVIEW_ICON_LARGE 128`, `PREVIEW_DEFAULT_WIDTH 280`, `PREVIEW_INTERNAL_PADDING 16`.

### 🔵 Info — NEMO_DATADIR CSS fallback unreachable
**File:** `src/src/nemo-application.c` line 610
**Impact:** The NEMO_DATADIR path will never exist since we don't install system-wide. Harmless — the multi-path fallback handles this.
**Recommendation:** Consider removing the NEMO_DATADIR fallback or replacing with a build-dir-relative path.

### 🔵 Info — CSS uses @define-color
**File:** `src/data/nemo-windows.css`
**Impact:** GTK3-specific syntax. No functional impact. Would need rewriting if ever porting to GTK4.
**Recommendation:** Document in CSS file header that it's GTK3 CSS.

---

## 11. Recommendations

### Actionable Now (5 min fixes)
- [ ] **Commit working tree** — `git add -A && git commit -m "feat: explorer sidebar wiring + Windows CSS theme"`
- [ ] **Create main branch** — `git branch main dev`
- [ ] Extract duplicate scale code into helper function
- [ ] Define magic numbers as #defines

### This Week
- [ ] **Test Feature 2 runtime behavior** — verify places click doesn't cause tree double-navigation
- [ ] **Decide Feature 2 architecture** — keep stacked-sidebars approach (update spec) or rework as composite widget
- [ ] Add image file size check (50 MB cap)
- [ ] Add directory preview (child count)

### Next Iteration
- [ ] Fix metadata grid rebuild perf issue
- [ ] Add manual test checklist for all 3 features
- [ ] Consider adding a remote (GitHub/GitLab) for backup
- [ ] If Feature 2 is kept as-is, add signal deduplication comment in explorer-sidebar.c

---

## 12. Score Summary

| Category | Score | Change from prev | Notes |
|---|---|---|---|
| Architecture | 7/10 | -1 | Explorer sidebar stacking is pragmatic but not spec-compliant |
| Dependencies | 9/10 | — | All system packages, clean |
| Configuration | 8/10 | +1 | compile-schema.sh fixed, CSS build integration added |
| Source Quality | 7/10 | — | Same strengths, same issues from previous audit |
| Testing | 2/10 | — | No new tests added |
| CI/CD | 0/10 | — | No CI (acceptable) |
| Git Health | 5/10 | -1 | Dirty working tree, no main branch, no commits for Features 2+3 |
| Build Output | 9/10 | +1 | Still 0w/0e, now includes Features 2+3 code |
| **Overall** | **6.5/10** | — | Feature scope expanded (1→3 features), quality maintained. Explorer sidebar needs runtime validation. |
