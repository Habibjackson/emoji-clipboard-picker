# Architecture

## Runtime

`src/main.cpp` creates one resident Qt process and a QML picker. It owns the session D-Bus name `org.emojikeyboard.App`; later command-line launches forward an open or quit request to that process. The installer supplies D-Bus activation and a `systemd --user` service so the picker can start hidden at login or on demand.

`qml/Main.qml` hosts the compact window; `qml/views/` contains Emoji and Clipboard views; `qml/components/` holds reusable controls. `src/app/AppController.*` exposes UI actions and capture state to QML. `src/models/EmojiModel.*` loads the pinned Unicode/CLDR catalog and recents; `src/models/ClipboardModel.*` presents searchable history. `src/storage/HistoryStore.*` owns the versioned SQLite schema, deduplication, pinning, and eviction.

## Desktop integration

Both backends implement `src/platform/DesktopIntegration.*`. They open the requested view, remember the previously focused target, handle capture, and attempt paste. If the backend is unavailable, the picker displays a notice and selecting an item still copies it.

On GNOME Wayland, `integration/gnome-extension/` registers global shortcuts, observes clipboard owner changes while other apps are focused, and forwards bounded text or image payloads over the current user's session D-Bus. `src/platform/gnome/` bridges those events to Qt. The extension positions the window near the pointer because a regular Wayland client cannot choose global screen coordinates. GNOME Shell must load the extension after installation.

On X11, `src/platform/x11/` watches clipboard ownership through XFixes, grabs the configured shortcuts, positions the window near the pointer, restores focus, and sends paste through XTest. CMake builds this backend only when the X11 development dependencies are present.

For shortcut-opened emoji selections, the app copies a temporary emoji, the backend sends a paste key, and the previous plain text or PNG clipboard content may be restored after 750 ms. Restoration is skipped if a newer clipboard owner or incompatible content makes it unsafe. Paste acceptance cannot be confirmed for every target. A manual launch leaves the selected item copied for manual paste.

## Storage and verification

`HistoryStore` writes `history.sqlite` in Qt's per-user `AppDataLocation`. It keeps at most five pinned and twenty unpinned entries, and 24 recent emoji sequences. Clipboard payloads are kept locally; [Privacy and local data](PRIVACY.md) describes limits and deletion.

Run `ctest --test-dir build --output-on-failure` for Qt Test, installer, GJS, QML tooltip, and available desktop integration tests. `cmake --build build --target emoji-keyboard_qmllint` checks QML. Some tests are registered only when their tools are installed. `tests/integration/tst_gnome_headless.py` exercises an isolated GNOME Shell session; `tst_x11_backend` runs under Xvfb when the X11 backend and Xvfb are available. A real installed-session logout/login check remains on [TODO.md](../TODO.md).
