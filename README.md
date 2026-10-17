# Emoji Keyboard for Linux

Qt 6.11/QML emoji picker with local clipboard history. This first version lets you browse 3,963 Unicode Emoji 18.0 entries by category, search by name, copy emoji, and keep recently used emoji. It saves copied text and images across restarts, with up to five permanent pins and twenty unpinned items. The remaining desktop integration work is tracked in [TODO.md](TODO.md).

## Build

Use Qt 6.11 and a C++20 compiler. For an official Qt installation in `~/Qt/6.11.1/gcc_64`:

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target emoji-keyboard_qmllint
./build/emoji-keyboard
```

Run `./build/emoji-keyboard --clipboard` to open the history view directly. The picker runs as one resident process: later launches reopen the existing window, `Escape` hides it, and `Ctrl+Q` or `./build/emoji-keyboard --quit` exits. Use `--background` to start it hidden. `Ctrl+1` and `Ctrl+2` switch views; `Ctrl+F` focuses search. A manual launch copies the selected emoji or saved item for you to paste. You can pin, unpin, delete, clear unpinned history, or pause capture in the clipboard view.

On Linux, the app defaults to Qt's generic platform theme because its custom QML controls do not use GTK styling. This avoids a GTK/GSettings startup crash when launched from some Snap-hosted IDE terminals. An explicit `QT_QPA_PLATFORMTHEME` value still takes precedence.

Clipboard monitoring uses Qt's clipboard notifications while the application runs. GNOME Wayland may not expose copies made while this window is unfocused; use **Capture current** after returning to the picker to save the current clipboard item. A GNOME Shell 50 development extension in [`integration/gnome-extension/`](integration/gnome-extension/README.md) detects clipboard owner changes, starts the hidden picker through session D-Bus, and registers `Super+.` / `Super+V` bindings. When opened by a registered shortcut, the picker returns focus to the prior window and sends `Ctrl+V`, or `Ctrl+Shift+V` for recognized terminals. The bindings default to disabled until guided setup can release existing GNOME shortcuts. No shortcut settings are changed by this version.

After an automatic emoji paste, the GNOME extension restores prior plain text or PNG clipboard content after 750 ms if the temporary emoji is still current and no clipboard owner change was observed. A newer copy is left untouched, even if it contains the same emoji. Unsupported, oversized, or unreadable prior content cannot be restored; the emoji then stays copied. Some applications may read the clipboard later than the delay and receive the restored prior content. The extension cannot confirm that every target accepted the paste. The development bridge does not yet capture full clipboard payloads in the background.

History is stored in SQLite under Qt's user-specific `AppDataLocation` with a private directory and database file. Text is capped at 1 MiB and encoded images at 10 MiB. File copy operations and recognized secret-marked content are skipped. The application makes no network requests.

## Layout

| Path | Intended contents |
| --- | --- |
| `src/app/` | Application state and QML-facing controllers |
| `src/models/` | Emoji and clipboard list models |
| `src/storage/` | SQLite history and migrations |
| `src/platform/gnome/` | GNOME Wayland D-Bus bridge |
| `src/platform/x11/` | X11 clipboard, shortcuts, and paste backend |
| `qml/views/`, `qml/components/` | Picker views and reusable controls |
| `resources/emoji/`, `resources/icons/` | Versioned emoji data and visual assets |
| `integration/` | GNOME Shell extension, desktop entry, D-Bus and systemd files |
| `tests/unit/`, `tests/integration/` | Model/storage tests and desktop integration tests |
| `scripts/`, `docs/` | Development tools and design/operational notes |

The `.gitkeep` files preserve directories until their implementations are added.

## Emoji data

The catalog is a pinned copy of Unicode's `emoji-test.txt` version 18.0. Its source, checksum, and separate Unicode License v3 notice are in [resources/emoji/README.md](resources/emoji/README.md). Emoji artwork is rendered by installed system fonts; no artwork is bundled.

## Contributing and license

See [CONTRIBUTING.md](CONTRIBUTING.md) for development and pull request steps, [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for community expectations, and [SECURITY.md](SECURITY.md) for private vulnerability reporting. Original project code is licensed under the [MIT License](LICENSE); imported data and assets must retain their own license notices.

Qt is an external dependency and is not covered by this project's MIT license. The Qt 6.11 modules currently used are available under LGPLv3. This source repository does not bundle Qt; a binary release must provide the applicable Qt license text and notices and meet the LGPL distribution requirements.
