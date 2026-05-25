# Project Technical Audit — nemo-hack

**Date:** 2026-05-25
**Branch:** dev
**Scope:** Full codebase audit (C/GTK3 desktop application)

---

## Executive Summary

Overall health: **GOOD** (risk: LOW). The preview panel feature (Feature 1) is well-implemented, compiles clean (0 warnings, 0 errors), and runs. The project structure follows Nemo conventions. Main gaps: no tests, duplicate code in image scaling, no directory preview, and no CI. Ready to proceed with Feature 2 (Combined Explorer Sidebar) and Feature 3 (CSS Theme).

---

## 1. Project Structure & Architecture (7/10)

| Metric | Value |
|---|---|
| Total source files | 365 .c/.h files |
| Modified files | 7 (nemo-window.c/h, nemo-window-private.h, nemo-window-menus.c, meson.build, gschema.xml, nemo-global-preferences.h) |
| New files | 2 (nemo-preview-panel.c, nemo-preview-panel.h) |
| Architecture pattern | Nemo standard: GObject subclass in separate .c/.h |
| Layout strategy | Nested GtkPaned — correct pattern for 3-panel layout |
| Widget hierarchy | content_paned → vbox → preview_paned → (split_view_hpane | preview_scrolled) |

**Observations:**
- New widget properly separated from window code. Clean GObject implementation.
- GtkScrolledWindow correctly wraps the preview panel for vertical overflow.
- `G_DEFINE_TYPE_WITH_PRIVATE` used correctly — no segfault risk.
- Build is at 278 targets, 100% clean (0 warnings).

---

## 2. Dependencies Health (9/10)

| Metric | Value |
|---|---|
| Build system | Meson 1.7.0 + Ninja 1.12.1 |
| Compiler | GCC |
| Key deps | GTK3, GLib 2.84, GDK-Pixbuf, GSettings/dconf |
| Missing deps | None — apt build-deps satisfied |

**Observations:**
- All system packages, no vendored deps. Good for LMDE stability.
- GdkPixbuf handles JPEG/PNG without extra libs.

---

## 3. Configuration & Build (7/10)

| Metric | Value |
|---|---|
| Build output | ✓ 278/278 targets, 0 errors, 0 warnings |
| Binary location | build/src/nemo (ELF 64-bit, debug info, not stripped) |
| Schema compilation | ✓ compile-schema.sh works |
| GSettings keys | 2 new keys: start-with-preview-panel (bool), preview-panel-width (int) |
| Path portability | ⚠️ Schema dir is `build/schemas` — must pass GSETTINGS_SCHEMA_DIR at runtime |
| File permissions | ⚠️ compile-schema.sh was not executable after git commit |

**Fix applied during audit:** Build reconfigured from stale `~/Projects/` paths.

---

## 4. Source Code Quality (7/10)

**Preview panel widget (nemo-preview-panel.c, 649 lines):**

| Aspect | Status |
|---|---|
| GObject boilerplate | ✓ Clean — G_DEFINE_TYPE_WITH_PRIVATE, dispose, class_init, init |
| Image preview | ✓ Full-resolution loading + dynamic scale (size-allocate callback) |
| Text preview | ✓ 16KB cap, null-byte replacement, read-only text view |
| Metadata display | ✓ Type, size, URI, file description |
| placeholder state | ✓ "Select a file to preview" |
| Multi-select handling | ✓ Clears on multi-select, shows on single select |
| Directory preview | ⚠️ Falls through to icon fallback — no directory-specific content |
| Error handling | ✓ Warns on load failure, falls back to large icon |
| Memory management | ✓ g_clear_object, g_object_unref on current_file, g_free on strings |
| Image size limit | ⚠️ No cap — large images load entire file into memory |

**Window integration (nemo-window.c):**

| Aspect | Status |
|---|---|
| Preview paned nesting | ✓ Clean block-scoped construction with proper pack1/pack2 |
| Toggle function | ✓ Handles both show (restore width) and hide (save width, push to max) |
| Startup restore | ✓ nemo_window_show reads GSettings and toggles preview on |
| Selection tracking | ✓ Connected in nemo_window_connect_content_view, disconnected in disconnect |
| Width persistence | ✓ preview_paned_position_changed_cb saves on paned handle drag |

**Code quality issues:**

1. 🟡 **Duplicate image scaling logic** — `show_image_preview()` and `preview_image_size_allocate_cb()` have nearly identical scale code (~30 lines duplicated). Extract into a single `scale_pixbuf_to_width()` helper.

2. 🟡 **No image file size cap** — `gdk_pixbuf_new_from_file()` loads entire file. A 200MB TIFF would allocate 200MB+ of memory. Cap at 50MB or check file size before loading.

3. 🟡 **metadata_grid destroyed/recreated** — `show_file_metadata()` calls `gtk_widget_destroy` and rebuilds the grid every time. For performance, update labels in-place instead.

4. 🟠 **No directory-specific preview** — `update_preview_content()` doesn't check `nemo_file_is_directory()`. Directories show only metadata. Should show item count and summary.

5. 🟠 **Text preview blocks main thread** — `g_file_get_contents()` is synchronous I/O. At 16KB this is negligible, but a concern at scale.

6. 🔵 **Missing icon size constant** — `128` used for large icon fallback is magic number. Define as `#define PREVIEW_ICON_LARGE_SIZE 128`.

---

## 5. Testing (2/10)

| Metric | Value |
|---|---|
| Test infrastructure | Meson test targets exist (upstream Nemo tests) |
| Preview panel tests | None |
| Build-time tests | Pass (test-nemo-search-engine, test-nemo-directory-async, test-nemo-copy) |

**Observation:** No tests for preview panel widget, window integration, or GSettings persistence. Mitigating factor: desktop GUI apps are inherently harder to unit test.

---

## 6. CI/CD (0/10)

No CI pipeline. For local-only hacking, this is acceptable.

---

## 7. Git Health (6/10)

| Metric | Value |
|---|---|
| Branches | 1 (dev) |
| Commits | 1 (initial project setup) |
| Remote | None |
| Git workflow | dev → main convention documented in AGENTS.md |
| .gitignore | ✓ Present, covers build/, schemas/, editor files |
| History loss | 5 upstream patches flattened into single commit (backup at /tmp/nemo-hack-patches.patch) |

---

## 8. Build Output (8/10)

| Metric | Value |
|---|---|
| Binary size | build/src/nemo: ELF 64-bit |
| Warnings | 0 |
| Errors | 0 |
| Runtime | Starts clean (only expected Samba warning) |
| Compile time | ~30s on ninja -j4 |

---

## 9. Issues Found (Prioritized)

### 🟡 Medium — Duplicate image scaling logic
`show_image_preview()` (lines 260-287) and `preview_image_size_allocate_cb()` (lines 176-201) duplicate the same pixbuf scaling algorithm. Extract into a single function to avoid divergence.

### 🟡 Medium — No image size limit
`gdk_pixbuf_new_from_file()` loads the entire image into memory with no file size check. Should check `g_file_query_info()` for file size before loading, or cap allocation.

### 🟡 Medium — metadata_grid rebuilt every selection
`show_file_metadata()` destroys and recreates the grid and all its children on every file change. Should update existing labels in-place.

### 🟠 Low — No directory preview
Directories display icon + metadata, but no child count or summary. Windows Explorer shows item count and file type summary for directories.

### 🟠 Low — Text I/O on main thread
`g_file_get_contents()` blocks the UI thread. At 16KB this is fast enough, but worth noting for future refactoring.

### 🔵 Info — Magic numbers
`128` (icon size), `280` (default preview width), and `16` (internal padding) are magic numbers scattered through the code. Define as constants.

### 🔵 Info — Stale build after directory move
When the project directory moves, `build/` must be wiped and reconfigured. Consider documenting this in PLAN.md or adding a `rebuild.sh` script.

---

## 10. Recommendations

### Actionable Now (5 min fixes)
- [x] Fix compile-schema.sh permissions: `chmod +x compile-schema.sh`
- [x] Reconfigure build after directory move
- [ ] Extract duplicate scale code into `scale_pixbuf_to_width()` helper
- [ ] Define magic numbers as #define constants

### This Week
- [ ] Add directory preview (child count, file type breakdown)
- [ ] Add image file size check before loading
- [ ] Add menuitem XML reference documentation
- [ ] Git: create main branch, add remote if desired

### Next Iteration
- [ ] Feature 2: Combined Explorer Sidebar
- [ ] Feature 3: Windows CSS Theme
- [ ] Add unit tests for preview panel (if test framework permits GUI tests)
- [ ] Consider async I/O for text preview (GTask/GIO)

---

## Score Summary

| Category | Score | Notes |
|---|---|---|
| Architecture | 8/10 | Clean GObject widget, correct layout nesting |
| Dependencies | 9/10 | All system packages, no vendored deps |
| Configuration | 7/10 | GSettings correct, build needed reconfigure |
| Source Quality | 7/10 | Good patterns, minor duplication |
| Testing | 2/10 | No preview panel tests |
| CI/CD | 0/10 | No CI (acceptable for local hacking) |
| Git Health | 6/10 | Clean start, history flattened |
| Build Output | 8/10 | 0 warnings, 0 errors, binary runs |
| **Overall** | **6.5/10** | Solid foundation. Feature 1 complete and working. Ready for Features 2 & 3. |
