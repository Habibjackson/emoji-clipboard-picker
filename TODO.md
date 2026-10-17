# Emoji Keyboard for Linux — Implementation TODO

## Product contract

- [ ] Build the picker UI with Qt 6.11 and QML; use C++ for persistence and desktop integration.
- [ ] Support GNOME Wayland and X11 in the first release. Use Linux `Super` as the requested Windows/Cmd key.
- [ ] Make `Super+.` open the emoji view and `Super+V` open clipboard history from other applications.
- [ ] Paste the selected emoji or history item into the previously focused application when possible.
- [x] Keep at most **5 pinned clipboard items plus 20 unpinned clipboard items**. Both groups survive app and system restarts; pinned items stay until explicitly unpinned or deleted.
- [ ] Save emoji recents across restarts. Record ordinary clipboard text and images, excluding file cut/copy operations.
- [x] Keep all history local to the current Linux account. Provide pause, delete, and clear actions.

## 1. Prove the desktop integration first

- [x] Create a minimal GNOME Shell extension prototype that receives `Meta.Selection::owner-changed` events while another Wayland application has focus. Verify text and image copies from GTK, Qt Wayland, and XWayland applications. (All three passed in isolated GNOME 50.1; the Qt Wayland test uses a painted, focused window and a fresh input event before each copy.)
- [x] Prove that the extension can call a resident Qt process over the session D-Bus and that a clipboard event received before the Qt process starts can activate it. (A GTK Wayland copy activated the hidden Qt service from no owner in an isolated session; payload transfer is still pending.)
- [x] Prove `Super+.` and `Super+V` can be registered after the existing IBus emoji and GNOME message-tray bindings are released. (Both opened the correct Qt view from a focused GTK window in an isolated GNOME 50.1 session; extension defaults remain disabled until guided setup.)
- [x] Prove the extension can remember the focused window, show a Qt window, restore the target window, and send the appropriate paste shortcut. Test a normal text field and a terminal. (GNOME 50.1: GTK 4 entry received `Ctrl+V`; a VTE terminal with `org.gnome.Ptyxis` app ID received `Ctrl+Shift+V` in the isolated session.)
- [x] Test temporary emoji clipboard replacement followed by restoration of prior text and image content. Never restore over a newer copy made by the user; record cases where restoration cannot be guaranteed. (GNOME 50.1: prior UTF-8 text and PNG image were restored by Shell; newer copies with different and identical emoji content were kept. Restore is skipped if the prior format cannot be read, a clipboard owner change is observed, or the current clipboard no longer contains the temporary emoji. Paste acceptance cannot be confirmed for every target; the 750 ms wait is a best-effort interval.)
- [x] Prove X11 clipboard change monitoring through XFixes and global shortcut/paste behavior through X11 key grabs and XTest. (Automated Xvfb test uses a separate Qt target process: text/image clipboard owner changes, both Super shortcuts, ordinary Ctrl+V paste, and terminal Ctrl+Shift+V paste pass.)
- [x] Record the tested GNOME Shell versions and any version-specific API branches. Test GNOME 50 on the current machine first. (GNOME Shell 50.1 tested; the extension currently targets only version 50 and has no version branches.)

## 2. Create the Qt application foundation

- [x] Add a CMake project pinned to Qt 6.11 components: Core, Gui, Quick, QuickControls2, Qml, Sql, and DBus. Add X11/XFixes/XTest dependencies only for the optional X11 backend.
- [x] Create one resident process with a single-instance D-Bus name, a hidden-by-default QML picker, and a command-line option to open either view for debugging.
- [x] Register C++ models and controllers as a QML module with `qt_add_qml_module`; expose typed properties and invokable actions rather than placing storage logic in QML JavaScript.
- [x] Define one desktop integration interface used by the GNOME Wayland and X11 implementations: capture clipboard content, open a view, remember the target, paste a selection, and report failures. (`DesktopIntegration` is the shared contract; `PickerService` forwards its D-Bus calls to the selected backend, with a copy-only fallback.)
- [x] Add a stable desktop-file ID and app icon so GNOME can identify and activate the Qt window. (`org.emojikeyboard.App` is set at startup; CMake installs its desktop entry and icon.)
- [ ] Make backend or extension failure visible in the picker, with a copy-only fallback for a selected item.

## 3. Build the emoji catalog and recents

- [x] Import a pinned Unicode emoji data release containing fully qualified sequences, group/subgroup order, and names; document its version and license.
- [ ] Add English names and searchable keywords from a compatible CLDR release, falling back to the Unicode name when an annotation is unavailable.
- [x] Expose categories, emoji items, filtered search results, and recently used emoji through C++ models. Keep the exact selected skin-tone sequence in recents.
- [ ] Group skin-tone variants behind a variant chooser so the category grid remains usable.
- [x] Limit emoji recents to the 24 most recently selected sequences; update an existing sequence instead of duplicating it.
- [ ] Use the system emoji font with a readable name/placeholder for any sequence the installed font cannot render.

## 4. Implement persistent clipboard history

- [x] Create an SQLite database in `QStandardPaths::AppDataLocation`; create its directory with mode `0700` and database-related files with mode `0600`.
- [ ] Store each entry's ID, kind, MIME type, payload, content hash, pin state, and capture/use timestamps. Store emoji recents separately; add a schema version and migration path.
- [ ] Accept plain UTF-8 text and common bitmap image MIME types. Prefer the image representation when the same copy offers both image and text. Normalize unsupported bitmap encodings before saving.
- [ ] Set capture limits of 1 MiB for text and 10 MiB for encoded images, plus a decoded-image dimension limit. Skip empty, unsupported, secret-marked, and oversized content; surface skipped-item status without saving payload data in logs.
- [x] Deduplicate by content type and SHA-256 payload hash. Recopying an item updates its timestamp and position while preserving its pin state.
- [x] Enforce 5 pinned and 20 unpinned entries in a transaction. Evict only the oldest unpinned entry; disable Pin with an explanation when 5 are already pinned.
- [ ] On unpin, move the item into the unpinned recents and apply the 20-item cap. Let Clear recent remove only unpinned items; provide separate confirmed Clear all and individual Delete actions. (Clear all remains.)
- [ ] Implement pause/resume capture. Suppress the app's own temporary clipboard writes so emoji insertion and history paste do not create unintended duplicate captures. (Pause and copy suppression are implemented; paste flow remains.)
- [x] Generate bounded image thumbnails for QML without decoding full-resolution images in every delegate.

## 5. Build the QML picker

- [ ] Use a compact desktop window with two views: **Emoji** and **Clipboard**. Open the requested view directly from its shortcut; repeated activation toggles or switches the view predictably.
- [ ] Emoji view: search field, persistent Recents, category navigation, lazy emoji grid, accessible names, and skin-tone variant chooser.
- [x] Clipboard view: Pinned section, newest-first Recent section, text previews, image thumbnails, search/filter, Pin/Unpin, Delete, Clear recent, and pause control.
- [ ] Use Qt Quick Templates for customized controls, system-aware light/dark colors, scalable typography, and a visible keyboard focus ring. (Controls, theme, and focus rings are implemented; large-font verification remains.)
- [ ] Support Tab, arrow keys, Enter, and Escape throughout. Ensure every pointer action also works from the keyboard and has an accessible label.
- [ ] Show useful states for empty recents, no search results, all five pins used, capture paused, missing desktop integration, skipped oversized content, and failed paste.
- [ ] Keep the picker usable at normal and large desktop font sizes and on smaller displays. Let the compositor place the window on Wayland rather than relying on absolute positioning.

## 6. Finish Wayland and X11 paste flows

- [ ] On GNOME Wayland, let the extension capture the target window when a shortcut fires, then activate the Qt picker. The Qt app sends the selected text/image to the extension for focus restoration and paste.
- [ ] On X11, capture the active window before showing the picker, then hide the picker, restore the target through the window manager, and synthesize paste using XTest.
- [ ] Use `Ctrl+V` for ordinary applications and `Ctrl+Shift+V` for recognized terminals; provide a setting to override the paste shortcut when detection is wrong.
- [ ] For a clipboard-history selection, make the selected item the current clipboard content and paste it. Update its recent position without changing its pin state.
- [ ] For an emoji selection, temporarily put the emoji on the clipboard, paste, then restore the prior supported text/image content only if the temporary content is still current. If the target disappears or paste fails, leave the emoji copied and tell the user how to paste it manually.
- [ ] Avoid paste loops, double injection, focus theft after cancellation, and replacing a clipboard change made during the operation.

## 7. Setup, startup, and removal

- [ ] Provide an idempotent user-level installer for the binary, desktop entry, GNOME extension, session D-Bus activation, and a systemd user service that starts at login and restarts after a crash.
- [ ] In a guided setup step, explain that this machine currently uses `Super+.` for IBus emoji input and `Super+V` for GNOME's message tray. Ask once before changing either setting.
- [ ] Save the original shortcut arrays; remove only the conflicting combinations, leaving other IBus and message-tray shortcuts intact. Restore each original value on uninstall only if the user has not changed it since setup.
- [ ] Register the two GNOME extension keybindings, report conflicts, and offer a recheck action. On X11, report a failed key grab instead of claiming the shortcut works.
- [ ] Document the GNOME logout/login needed when first installing an extension, and provide clear extension/service status checks.
- [ ] Make uninstall disable the extension and service, remove installed integration files, restore eligible shortcuts, and offer a separate explicit choice to keep or purge the history database.

## 8. Verification and release checklist

- [x] Run a clean CMake build with Qt 6.11.1, `qmllint`, and automated storage/model tests.
- [ ] Test 6th pin rejection; 21st unpinned eviction; recopy of pinned and unpinned entries; unpin at capacity; deletion; clear recent; pause/resume; and persistence after app restart and logout/login.
- [ ] Test UTF-8 text, multiline text, duplicate text, PNG/JPEG images, unsupported file-copy MIME types, oversized content, and corrupted or outdated database recovery.
- [ ] On GNOME Wayland, test both shortcuts from GTK, Qt Wayland, XWayland, and terminal windows; verify focus restoration, paste, image previews, and clipboard preservation behavior.
- [x] On X11, test XFixes capture, shortcut grabs, text/image paste, and startup in a separate X11 session or automated Xvfb harness. (Automated Xvfb test passes with a separate target process; the compiled picker also passes an X11 smoke launch.)
- [ ] Test both shortcut-conflict setup and uninstall restoration, including the case where the user changes a binding after installation.
- [ ] Verify no clipboard payload appears in logs, no data is sent over the network, file permissions are user-only, and failures are explained in the UI.
- [ ] Write README instructions for build, install, use, troubleshooting, supported desktops, known paste/restoration limits, and data removal.

## Technical references

- [Wayland clipboard selection and focus](https://wayland.freedesktop.org/docs/book/Protocol.html)
- [GNOME `Meta.Selection` clipboard events](https://gnome.pages.gitlab.gnome.org/mutter/meta/class.Selection.html)
- [GNOME runtime keybindings](https://gnome.pages.gitlab.gnome.org/mutter/meta/class.Display.html)
- [Qt clipboard behavior](https://doc.qt.io/qt-6/qclipboard.html)
- [Unicode emoji data guidance](https://www.unicode.org/reports/tr51/)
