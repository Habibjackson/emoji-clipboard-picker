# Emoji Keyboard for Linux

Qt 6.11/QML emoji picker with local clipboard history. Browse 3,963 Unicode Emoji 18.0 entries by category, search by English name or CLDR keyword, and keep recently used emoji. Skin tone sequences sit behind a chooser, while Recents keep the exact selection. Copied text and images survive restarts, with up to five permanent pins and twenty unpinned items. Remaining verification is tracked in [TODO.md](TODO.md).

For setup and operation, see [Installation and daily use](docs/INSTALL.md), [Troubleshooting](docs/TROUBLESHOOTING.md), [Architecture](docs/ARCHITECTURE.md), and [Privacy and local data](docs/PRIVACY.md).

## Build

Use Qt 6.11 and a C++20 compiler. For an official Qt installation in `~/Qt/6.11.1/gcc_64`:

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target emoji-keyboard_qmllint
./build/emoji-keyboard
```

The X11 backend is built when the X11, XFixes, XTest, Xi, and Xext development files are available. If CMake reports that it is disabled, install those development packages and reconfigure the same `build/` directory.

Run `./build/emoji-keyboard --clipboard` to open the history view directly. The picker runs as one resident process: later launches reopen the existing window, `Escape` hides it, and `Ctrl+Q` or `./build/emoji-keyboard --quit` exits. Use `--background` to start it hidden. `Ctrl+1` and `Ctrl+2` switch views; `Ctrl+F` focuses search. The compact picker opens beside the mouse pointer and flips left or upward at monitor edges. Drag the thin bar at the top center to move it. The top icons switch views, choose **Follow system**, **Light**, or **Dark**, open actions, and hide the picker. Hover or focus an icon for its tooltip. GNOME Wayland needs the active Shell extension for initial placement; X11 positions it directly. A manual launch copies the selected emoji or saved item for you to paste. Clipboard history stays open after a selection so you can paste another item; `Escape` or the close icon hides it. Click a clipboard row to copy it, use its pin and delete icons to manage it, and open the top menu to pause capture or clear history.

On Linux, **Follow system** reads the desktop appearance portal and tracks its light/dark changes; Qt's color scheme hint is the fallback. The app defaults to Qt's generic platform theme because its custom QML controls do not use GTK styling. This avoids a GTK/GSettings startup crash when launched from some Snap-hosted IDE terminals. An explicit `QT_QPA_PLATFORMTHEME` value still takes precedence.

Clipboard monitoring uses Qt notifications on X11. On GNOME Wayland, the Shell extension in [`integration/gnome-extension/`](integration/gnome-extension/README.md) reads text and image copies while other windows have focus and forwards bounded payloads to the resident Qt process over the current user's session D-Bus. The **Capture current** icon manually saves the clipboard's current item when automatic capture missed a copy; normally no click is needed. The extension registers emoji and clipboard bindings, initially `Super+.` and `Super+V` after setup. When opened by a registered shortcut, the picker returns focus to the prior window and sends `Ctrl+V`, or `Ctrl+Shift+V` for recognized terminals. The top actions menu has a separate **Paste shortcut** choice to override detection with either key combination; the choice persists across restarts. GNOME bindings default to disabled until guided setup releases existing shortcuts.

After an automatic emoji paste, the GNOME extension restores prior plain text or PNG clipboard content after 750 ms if the temporary emoji is still current and no clipboard owner change was observed. A newer copy is left untouched, even if it contains the same emoji. Unsupported, oversized, or unreadable prior content cannot be restored; the emoji then stays copied. Some applications may read the clipboard later than the delay and receive the restored prior content. The extension cannot confirm that every target accepted the paste. Shell requests the offered clipboard payload and sends supported text or image bytes to Qt for local storage; some applications and formats may still be unavailable.

On X11, the optional backend watches clipboard ownership through XFixes and grabs the configured keys, defaulting to `Super+.` and `Super+V`, to open the matching view. It asks an EWMH window manager to restore the target, or restores focus directly when no compatible manager is present, then sends paste through XTest. A failed shortcut grab appears in a persistent picker notice. After a successful emoji paste, it restores prior plain text or PNG content after 750 ms only if the temporary emoji and clipboard owner are unchanged. The Xvfb test covers text/image capture and paste, shortcuts, restoration, cancellation, and newer-copy preservation. Automatic paste and restoration are best effort; if the target vanishes, the selected item remains copied for manual paste.

If native integration or the GNOME extension is unavailable, the picker shows a copy-only notice. Selecting an emoji or history item still copies it, ready for manual paste. The GNOME notice clears when its extension starts and returns if it stops.

## Install and shortcuts

After building in the existing `build/` directory, run:

```sh
python3 scripts/emoji_keyboard.py install
python3 scripts/emoji_keyboard.py status
```

The installer copies the binary to `~/.local/bin`, installs the desktop entry, icon, D-Bus activation file, GNOME extension and compiled schema under `~/.local/share`, and enables a `systemd --user` service. The service starts with `graphical-session.target` and restarts after a crash. The binary still needs Qt 6.11 runtime libraries on this machine. If the compiled binary lives elsewhere, pass `install --binary /path/to/emoji-keyboard`. Repeating `install` updates managed files; it refuses to overwrite a file edited outside the installer.

Open **More actions → Keyboard shortcuts…** in the picker. Click the Emoji or Clipboard key, then press a new combination. Use `Super` plus a key or `Ctrl+Alt` plus a key; `Shift` is optional. `Esc` cancels recording. Changes take effect immediately and survive restart: GNOME uses the extension's GSettings keys and X11 uses the app's Qt settings file. If a new X11 key is already grabbed, the old one stays active and the picker reports the failure. On GNOME, run `python3 scripts/emoji_keyboard.py setup-shortcuts` if the new key overlaps an IBus or message-tray binding; other Shell extensions and system shortcuts may also need changing in GNOME Settings.

Guided setup shows any overlap between the selected picker keys and IBus or GNOME message-tray shortcuts and asks **once** before removing those combinations. Other shortcuts in those arrays remain. It enables unset extension bindings with the default keys, preserves custom picker bindings, and saves changed values in a private installer-state file. In a noninteractive terminal, conflicting shortcuts stay unchanged; run `python3 scripts/emoji_keyboard.py setup-shortcuts` later. Use `install --no-shortcuts` to install without this step. `setup-shortcuts --yes` is an explicit noninteractive choice.

On GNOME Wayland, log out and log back in after installing or updating the extension so Shell loads its current code. Disabling and re-enabling an already loaded extension may keep the old module in memory. If Shell cannot discover a new extension immediately, the installer queues its UUID in GNOME's enabled list for the next login while keeping other extensions. An immediate `recheck` reports an inactive extension or stale bridge code; there is no need to reinstall. Check it with `gnome-extensions info emoji-keyboard@emojikeyboard.local`; check the service with `systemctl --user status emoji-keyboard.service`. Run `python3 scripts/emoji_keyboard.py recheck` after login to inspect known conflicts and whether the extension/service are active. If a shortcut still fails, check other extensions or custom GNOME bindings. On X11, the native backend handles key grabs and reports failures in the picker; the GNOME extension stays inactive there. `recheck` cannot prove that another X11 client will not grab a key later, so press both shortcuts from another application.

For the remaining release check, pin a copied item and select an emoji, then log out and back in. Confirm that `recheck` reports an active extension and service, `Super+V` still shows the pin, and `Super+.` still shows the emoji in Recents. The automated headless test covers a fresh Shell and session bus with the same private history, but it does not replace this installed-session check.

The installer changes the IBus and message-tray shortcut arrays only during its guided shortcut step. The picker changes its own GNOME extension bindings when you record a key. It does not need root privileges. GNOME Shell 50.1 and the optional X11 backend are the current tested targets; other Wayland compositors are not supported yet.

## Remove and local data

```sh
python3 scripts/emoji_keyboard.py uninstall
# Explicitly delete the saved history database too:
python3 scripts/emoji_keyboard.py uninstall --purge-history
```

Uninstall disables the extension and service, removes unchanged managed files, and restores each shortcut array only if its current value still matches the value setup applied. User-edited arrays and installed files are kept and reported. By default, the SQLite history database is kept; `--purge-history` deletes it and its SQLite sidecars. The command can be run again after a keep-history uninstall; private installer state remains so it knows the history path. To see the exact Qt history and settings paths, run `./build/emoji-keyboard --print-data-paths`. The separate Qt settings file stores X11 picker shortcuts, the paste shortcut, and theme preferences and is not part of the history purge.

History is stored in SQLite under Qt's user-specific `AppDataLocation` with a private directory and database file. Text is capped at 1 MiB and encoded images at 10 MiB. File copy operations and recognized secret-marked content are skipped. The application makes no network requests.

## Layout

| Path | Intended contents |
| --- | --- |
| `src/app/` | Application state and QML-facing controllers |
| `src/models/` | Emoji and clipboard list models |
| `src/storage/` | SQLite history and migrations |
| `src/platform/DesktopIntegration.*` | Shared desktop backend contract and copy-only fallback |
| `src/platform/gnome/` | GNOME Wayland D-Bus bridge |
| `src/platform/x11/` | X11 clipboard, shortcuts, and paste backend |
| `qml/views/`, `qml/components/` | Picker views and reusable controls |
| `resources/emoji/`, `resources/icons/` | Versioned emoji data and visual assets |
| `integration/` | GNOME Shell extension, desktop entry, D-Bus and systemd files |
| `tests/unit/`, `tests/integration/` | Model/storage tests and desktop integration tests |
| `scripts/`, `docs/` | Development tools and design/operational notes |

Some empty directories retain `.gitkeep` files for future modules.

## Emoji data

The catalog is a pinned copy of Unicode's `emoji-test.txt` version 18.0, with English names and search keywords from CLDR 48.2. Their sources, checksums, and separate Unicode License v3 notice are in [resources/emoji/README.md](resources/emoji/README.md). Emoji artwork is rendered by installed system fonts; no artwork is bundled.

Tiles marked ▾ open a skin tone chooser. Selecting a tile in Recents copies its saved sequence directly. Qt uses the system emoji font; when shaping finds a missing glyph or an uncombined sequence, compact tiles show the emoji's name while still allowing copy. Font support can vary between applications and after a font update, so this check reflects the picker's current font configuration.

## Contributing and license

See [CONTRIBUTING.md](CONTRIBUTING.md) for development and pull request steps, [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for community expectations, and [SECURITY.md](SECURITY.md) for private vulnerability reporting. Original project code is licensed under the [MIT License](LICENSE); imported data and assets must retain their own license notices.

Qt is an external dependency and is not covered by this project's MIT license. The Qt 6.11 modules currently used are available under LGPLv3. This source repository does not bundle Qt; a binary release must provide the applicable Qt license text and notices and meet the LGPL distribution requirements.
