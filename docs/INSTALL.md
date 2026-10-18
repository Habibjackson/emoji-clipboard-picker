# Install and use Emoji Keyboard

## Requirements

- Linux with GNOME Wayland or an X11 session. GNOME Shell 50.1 is the tested Wayland target; other Wayland compositors currently have copy-only behavior.
- Qt 6.11 with Core, Gui, Qml, DBus, Quick, QuickControls2, Sql, and Test modules; CMake 3.24+, Ninja, and a C++20 compiler.
- For a user install: Python 3, `glib-compile-schemas`, `systemctl --user`, `gsettings`, and (on GNOME) `gnome-extensions`.
- For native X11 integration: X11, XFixes, XTest, Xi, and Xext development files before configuring CMake. CMake reports when this optional backend is disabled.

## Build from source

From the repository root, adjust the Qt path to match your installation. Reuse this `build/` directory for later changes:

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target emoji-keyboard_qmllint
./build/emoji-keyboard
```

`./build/emoji-keyboard --clipboard` opens history directly. `--background` starts the resident process hidden, and `--quit` stops it. A second launch contacts the running process rather than creating another picker. Use `Ctrl+Q` to quit from the window. The picker stays running when hidden with `Escape`.

## Install for the current user

```sh
python3 scripts/emoji_keyboard.py install
python3 scripts/emoji_keyboard.py status
```

The script copies the executable to `~/.local/bin`, installs the desktop entry, icon, session D-Bus activation file, GNOME Shell extension and schema, and enables a user systemd service. It does not bundle Qt runtime libraries. If your binary is outside `build/`, use `install --binary /absolute/path/to/emoji-keyboard`.

On GNOME Wayland, log out and back in after a new installation or extension update, then run `python3 scripts/emoji_keyboard.py recheck`. An immediate post-install message that the extension does not exist can mean Shell has not discovered it yet. `recheck` reports whether the extension, service, and known shortcut settings are ready. Repeating `install` updates managed files but refuses to overwrite files changed outside the installer.

## Shortcuts and paste behavior

The default global bindings are `Super+.` for Emoji and `Super+V` for Clipboard. GNOME's IBus emoji or message-tray bindings may already own them. During installation, guided setup shows the overlaps and asks before changing only the conflicting combinations. If you skipped setup or installed noninteractively, run `python3 scripts/emoji_keyboard.py setup-shortcuts` from a terminal. `install --no-shortcuts` skips this step; `setup-shortcuts --yes` explicitly accepts the displayed changes without a prompt. Other Shell extensions or custom system bindings may need separate changes in GNOME Settings.

In the picker, open **More actions → Keyboard shortcuts…** to record each key. Use `Super` plus a key or `Ctrl+Alt` plus a key; `Shift` is optional. `Esc` cancels recording. The choices persist in the extension's GSettings on GNOME and in Qt settings on X11. An unavailable X11 grab leaves the previous binding active and displays a notice.

When a registered shortcut opens the picker, selecting an emoji or history item attempts to restore the target window and send `Ctrl+V`, or `Ctrl+Shift+V` for recognized terminals. Clipboard history remains visible after selection and can paste another item into that target; hide it with `Escape` or the close icon. Emoji selection hides the picker. The **Paste shortcut** menu setting can force either combination when detection is wrong. A manual launch copies without an armed target. If automatic paste fails, paste the copied selection yourself. Emoji selection temporarily changes the clipboard; the prior plain text or PNG is restored after about 750 ms only if it is still safe to do so. Some targets may read the clipboard later and receive the restored content.

The top controls switch views and theme, open actions, and hide the picker. **Follow system** tracks the desktop appearance portal with Qt's color scheme hint as fallback. Click a clipboard row to copy; its icons pin or delete. The actions menu pauses capture, clears recent unpinned items, or clears all clipboard history after confirmation. The **Capture current** icon is useful when an automatic copy was missed. Hover or focus icons for tooltips.

## Update or remove

Build in the same directory and rerun `python3 scripts/emoji_keyboard.py install` to update. On GNOME, log out and back in to load updated extension code. To remove the integration:

```sh
python3 scripts/emoji_keyboard.py uninstall
# Also remove the local SQLite history database:
python3 scripts/emoji_keyboard.py uninstall --purge-history
```

Uninstall disables the extension and service, removes unchanged managed files, and restores shortcut values only when they still match the installer's changes. It keeps user-edited files and shortcut values. History is retained unless `--purge-history` is supplied; the purge can be run after a previous uninstall. Qt settings are separate and are not removed by the history purge. See [Privacy and local data](PRIVACY.md) for the exact data paths.
