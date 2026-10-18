# Emoji Keyboard for Linux — Implementation TODO

## Product contract

- [x] Build the picker UI with Qt 6.11 and QML; use C++ for persistence and desktop integration.
- [x] Support GNOME Wayland and X11 in the first release. Use Linux `Super` as the requested Windows/Cmd key.
- [x] Make `Super+.` open the emoji view and `Super+V` open clipboard history from other applications.
- [x] Paste the selected emoji or history item into the previously focused application when possible.
- [x] Keep at most **5 pinned clipboard items plus 20 unpinned clipboard items**. Both groups survive app and system restarts; pinned items stay until explicitly unpinned or deleted.
- [x] Save emoji recents across restarts. Record ordinary clipboard text and images, excluding file cut/copy operations.
- [x] Keep all history local to the current Linux account. Provide pause, delete, and clear actions.

## 1. Prove the desktop integration first

- [x] Create a minimal GNOME Shell extension prototype that receives `Meta.Selection::owner-changed` events while another Wayland application has focus. Verify text and image copies from GTK, Qt Wayland, and XWayland applications. (All three passed in isolated GNOME 50.1; the Qt Wayland test uses a painted, focused window and a fresh input event before each copy.)
- [x] Prove that the extension can call a resident Qt process over the session D-Bus and that a clipboard event received before the Qt process starts can activate it. (A GTK Wayland copy activated the hidden Qt service from no owner in an isolated session; the GNOME 50.1 extension now forwards bounded text/image payloads over session D-Bus.)
- [x] Prove `Super+.` and `Super+V` can be registered after the existing IBus emoji and GNOME message-tray bindings are released. (Both opened the correct Qt view from a focused GTK window in an isolated GNOME 50.1 session; extension defaults remain disabled until guided setup.)
- [x] Prove the extension can remember the focused window, show a Qt window, restore the target window, and send the appropriate paste shortcut. Test a normal text field and a terminal. (GNOME 50.1: GTK 4 entry received `Ctrl+V`; a VTE terminal with `org.gnome.Ptyxis` app ID received `Ctrl+Shift+V` in the isolated session.)
- [x] Test temporary emoji clipboard replacement followed by restoration of prior text and image content. Never restore over a newer copy made by the user; record cases where restoration cannot be guaranteed. (GNOME 50.1: prior UTF-8 text and PNG image were restored by Shell; newer copies with different and identical emoji content were kept. Restore is skipped if the prior format cannot be read, a clipboard owner change is observed, or the current clipboard no longer contains the temporary emoji. Paste acceptance cannot be confirmed for every target; the 750 ms wait is a best-effort interval.)
- [x] Prove X11 clipboard change monitoring through XFixes and global shortcut/paste behavior through X11 key grabs and XTest. (Automated Xvfb test uses a separate Qt target process: text/image clipboard owner changes, both Super shortcuts, ordinary Ctrl+V paste, and terminal Ctrl+Shift+V paste pass.)
- [x] Record the tested GNOME Shell versions and any version-specific API branches. Test GNOME 50 on the current machine first. (GNOME Shell 50.1 tested; the extension uses `global.context.get_wayland_compositor()` on this version and has no version branches.)

## 2. Create the Qt application foundation

- [x] Add a CMake project pinned to Qt 6.11 components: Core, Gui, Quick, QuickControls2, Qml, Sql, and DBus. Add X11/XFixes/XTest dependencies only for the optional X11 backend.
- [x] Create one resident process with a single-instance D-Bus name, a hidden-by-default QML picker, and a command-line option to open either view for debugging.
- [x] Register C++ models and controllers as a QML module with `qt_add_qml_module`; expose typed properties and invokable actions rather than placing storage logic in QML JavaScript.
- [x] Define one desktop integration interface used by the GNOME Wayland and X11 implementations: capture clipboard content, open a view, remember the target, paste a selection, and report failures. (`DesktopIntegration` is the shared contract; `PickerService` forwards its D-Bus calls to the selected backend, with a copy-only fallback.)
- [x] Add a stable desktop-file ID and app icon so GNOME can identify and activate the Qt window. (`org.emojikeyboard.App` is set at startup; CMake installs its desktop entry and icon.)
- [x] Make backend or extension failure visible in the picker, with a copy-only fallback for a selected item. (A persistent notice reports backend and shortcut failures; selecting an item keeps it on the clipboard when integration is unavailable, and the GNOME notice updates when the extension appears or disappears.)

## 3. Build the emoji catalog and recents

- [x] Import a pinned Unicode emoji data release containing fully qualified sequences, group/subgroup order, and names; document its version and license.
- [x] Add English names and searchable keywords from a compatible CLDR release, falling back to the Unicode name when an annotation is unavailable. (Pinned CLDR 48.2 covers 3,944 catalog entries; the 19 newer Emoji 18.0 entries use their Unicode names.)
- [x] Expose categories, emoji items, filtered search results, and recently used emoji through C++ models. Keep the exact selected skin-tone sequence in recents.
- [x] Group skin-tone variants behind a variant chooser so the category grid remains usable. (The full grid shows 1,923 base choices; a focused popup offers the 2,040 tone sequences, including mixed tones. Recents preserve exact selections.)
- [x] Limit emoji recents to the 24 most recently selected sequences; update an existing sequence instead of duplicating it.
- [x] Use the system emoji font with a readable name/placeholder for any sequence the installed font cannot render. (Qt chooses the system emoji fallback; glyph shaping is checked on demand and missing sequences show a named placeholder.)

## 4. Implement persistent clipboard history

- [x] Create an SQLite database in `QStandardPaths::AppDataLocation`; create its directory with mode `0700` and database-related files with mode `0600`.
- [x] Store each entry's ID, kind, MIME type, payload, content hash, pin state, and capture/use timestamps. Store emoji recents separately; add a schema version and migration path. (Version 0 databases migrate transactionally to version 1; incompatible schemas fail without a partial upgrade.)
- [x] Accept plain UTF-8 text and common bitmap image MIME types. Prefer the image representation when the same copy offers both image and text. Normalize unsupported bitmap encodings before saving. (PNG, JPEG, BMP, GIF, WebP, and TIFF input is decoded when available and stored as PNG.)
- [x] Set capture limits of 1 MiB for text and 10 MiB for encoded images, plus a decoded-image dimension limit. Skip empty, unsupported, secret-marked, and oversized content; surface skipped-item status without saving payload data in logs.
- [x] Deduplicate by content type and SHA-256 payload hash. Recopying an item updates its timestamp and position while preserving its pin state.
- [x] Enforce 5 pinned and 20 unpinned entries in a transaction. Evict only the oldest unpinned entry; disable Pin with an explanation when 5 are already pinned.
- [x] On unpin, move the item into the unpinned recents and apply the 20-item cap. Let Clear recent remove only unpinned items; provide separate confirmed Clear all and individual Delete actions. (Clear all preserves emoji recents.)
- [x] Implement pause/resume capture. Suppress the app's own temporary clipboard writes so emoji insertion and history paste do not create unintended duplicate captures. (The GNOME restore path also suppresses its expected prior text/image; a newer copy is captured.)
- [x] Generate bounded image thumbnails for QML without decoding full-resolution images in every delegate.

## 5. Build the QML picker

- [x] Use a compact desktop window with two views: **Emoji** and **Clipboard**. Open the requested view directly from its shortcut; repeated activation toggles or switches the view predictably. (GNOME Shell passes picker focus to the Qt service; X11 checks its focused window. Switching views preserves the original paste target.)
- [x] Emoji view: search field, persistent Recents, category navigation, lazy emoji grid, accessible names, and skin-tone variant chooser. (The live X11 keyboard check selected an emoji and an exact skin-tone variant; search text survived a view switch.)
- [x] Clipboard view: Pinned section, newest-first Recent section, text previews, image thumbnails, search/filter, Pin/Unpin, Delete, Clear recent, and pause control.
- [x] Use Qt Quick Templates for customized controls, system-aware light/dark colors, scalable typography, and a visible keyboard focus ring. (Type and control sizes follow the application font; a 1.5× type preview was checked.)
- [x] Support Tab, arrow keys, Enter, and Escape throughout. Ensure every pointer action also works from the keyboard and has an accessible label. (Live X11 keyboard checks copied an emoji and a history card, selected an exact tone with arrows, and closed the tone popup with Escape.)
- [x] Show useful states for empty recents, no search results, all five pins used, capture paused, missing desktop integration, skipped oversized content, and failed paste. (Oversize and paste failures remain in an attention banner when the picker is reopened.)
- [x] Keep the picker usable at normal and large desktop font sizes and on smaller displays. (Checked 640×480 at 1.5× DPI; a short window combines view tabs and actions in one row, leaving two emoji rows visible.)
- [x] Open beside the mouse pointer inside the current monitor's work area. (Qt positions the X11 window; the GNOME Shell extension moves the Wayland window after it appears. Isolated X11 and GNOME edge tests pass.)
- [x] Keep the default picker at 340 × 390 logical pixels and allow dragging the visible window by its grip or title. (The compact header retains the status footer; an isolated GNOME Wayland pointer test checks that dragging moves the window.)

## 6. Finish Wayland and X11 paste flows

- [x] On GNOME Wayland, the extension captures the target when a shortcut fires and activates Qt. Qt puts the selected text/image on the clipboard and asks Shell to restore focus and paste. (A disposable GNOME 50.1 session retested Qt Wayland, GTK Wayland, XWayland, and a terminal-style target; Shell activation also restores picker focus when it was already visible.)
- [x] On X11, capture the active window before showing the picker, then hide the picker, restore the target through an EWMH window-manager request (direct focus fallback without one), and synthesize paste using XTest. (Xvfb integration test passes.)
- [x] Use `Ctrl+V` for ordinary applications and `Ctrl+Shift+V` for recognized terminals, with a persistent override in the Clipboard view. (Both X11 and GNOME pass the selected mode to their paste path.)
- [x] For a clipboard-history selection, make the selected item current and paste it. Touch its recent position without changing its pin state. (Xvfb tests text/image paste and pinned history use.)
- [x] For an emoji selection, temporarily put it on the clipboard and restore prior supported text/image after paste only while it remains current. If the target disappears or paste fails, leave the emoji copied with a manual-paste message. (Xvfb tests text/image restoration, newer copies, and missing target.)
- [x] Guard against paste loops, double injection, focus theft after cancellation, and replacing a clipboard change made during the operation. (Generation checks, focus checks, GNOME cancellation, and X11 clipboard-owner/revision checks; Xvfb covers cancellation and same-content newer copies.)

## 7. Setup, startup, and removal

- [x] Provide an idempotent user-level installer for the binary, desktop entry, GNOME extension, session D-Bus activation, and a systemd user service that starts at login and restarts after a crash. (`scripts/emoji_keyboard.py install` copies the existing build and tracks managed file hashes.)
- [x] In a guided setup step, explain that this machine currently uses `Super+.` for IBus emoji input and `Super+V` for GNOME's message tray. Ask once before changing either setting. (A noninteractive install leaves conflicting settings untouched.)
- [x] Save the original shortcut arrays; remove only the conflicting combinations, leaving other IBus and message-tray shortcuts intact. Restore each original value on uninstall only if the user has not changed it since setup. (Installer tests cover idempotence, preservation, and user edits.)
- [x] Register the two GNOME extension keybindings, report conflicts, and offer a recheck action. On X11, report a failed key grab instead of claiming the shortcut works. (`setup-shortcuts` sets extension bindings after known conflicts clear; `recheck` reports their status. X11 already reports failed grabs in the picker.)
- [x] Document the GNOME logout/login needed when first installing an extension, and provide clear extension/service status checks. (README and `status`/`recheck` commands.)
- [x] Make uninstall disable the extension and service, remove installed integration files, restore eligible shortcuts, and offer a separate explicit choice to keep or purge the history database. (`uninstall --purge-history` is explicit; default keeps history.)

## 8. Verification and release checklist

- [x] Run a clean CMake build with Qt 6.11.1, `qmllint`, and automated storage/model tests.
- [ ] Test 6th pin rejection; 21st unpinned eviction; recopy of pinned and unpinned entries; unpin at capacity; deletion; clear recent; pause/resume; and persistence after app restart and logout/login. (Automated tests cover each storage/controller case and reopening SQLite. History and emoji recents also survived a fresh disposable GNOME Shell, D-Bus, and Qt session; a real installed-session logout/login check remains.)
- [x] Test UTF-8 text, multiline text, duplicate text, PNG/JPEG images, unsupported file-copy MIME types, oversized content, and corrupted or outdated database recovery. (Qt tests cover these inputs, version 0 migration, corrupt files, and rejection of newer schemas.)
- [x] On GNOME Wayland, test both shortcuts from GTK, Qt Wayland, XWayland, and terminal windows; verify focus restoration, paste, image previews, and clipboard preservation behavior. (The disposable GNOME 50.1 run passes GTK, Qt Wayland, XWayland, and a GTK terminal-style target, plus text/image capture, emoji and history-image paste, prior text/image restoration, and a rendered thumbnail. An earlier isolated test used a VTE terminal. The inherited Snap GSettings path caused the earlier headless startup failure; the test now uses system schemas in its private profile.)
- [x] On X11, test XFixes capture, shortcut grabs, text/image paste, and startup in a separate X11 session or automated Xvfb harness. (Automated Xvfb test passes with a separate target process; the compiled picker also passes an X11 smoke launch.)
- [x] Test both shortcut-conflict setup and uninstall restoration, including the case where the user changes a binding after installation. (An isolated real GSettings keyfile run verifies install, guided setup, preservation of other shortcuts, user edits, and uninstall restoration without changing the current profile.)
- [x] Verify no clipboard payload appears in logs, no data is sent over the network, file permissions are user-only, and failures are explained in the UI. (Synthetic-payload log test, source audit for network calls, private-file tests, and controller/X11 failure tests pass; Shell warnings use fixed messages.)
- [x] Write README instructions for build, install, use, troubleshooting, supported desktops, known paste/restoration limits, and data removal.

## Technical references

- [Wayland clipboard selection and focus](https://wayland.freedesktop.org/docs/book/Protocol.html)
- [GNOME `Meta.Selection` clipboard events](https://gnome.pages.gitlab.gnome.org/mutter/meta/class.Selection.html)
- [GNOME runtime keybindings](https://gnome.pages.gitlab.gnome.org/mutter/meta/class.Display.html)
- [Qt clipboard behavior](https://doc.qt.io/qt-6/qclipboard.html)
- [Unicode emoji data guidance](https://www.unicode.org/reports/tr51/)
