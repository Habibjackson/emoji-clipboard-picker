# GNOME Shell bridge

This extension targets GNOME Shell 50. It listens for `Meta.Selection::owner-changed` on the regular clipboard, classifies offered MIME types as text, image, or other, and exposes event counts over session D-Bus. For text and supported images it reads the current payload and sends up to the capture limit plus one byte to the Qt picker over the current user's session bus (1 MiB for text, 10 MiB for images). Qt enforces the limits and saves accepted items locally. The extension neither stores nor logs clipboard payloads. On X11 it stays inactive because the native Qt backend handles monitoring and shortcuts.

For a normal user install, build the application and run `python3 scripts/emoji_keyboard.py install` from the repository root. This installs the extension, compiles its schema, starts the user service, and guides the known shortcut changes. `python3 scripts/emoji_keyboard.py recheck` reports known conflicts. See [Installation and daily use](../../docs/INSTALL.md) for logout/login and removal steps.

Build and install the probe with:

```sh
gnome-extensions pack integration/gnome-extension --out-dir /tmp -f \
    --extra-source=clipboardProbe.js --extra-source=pasteShortcut.js
gnome-extensions install /tmp/emoji-keyboard@emojikeyboard.local.shell-extension.zip
gnome-extensions enable emoji-keyboard@emojikeyboard.local
```

On Wayland, a newly installed extension may need a logout and login before Shell discovers it. For isolated development, use a nested GNOME Shell session as described in the [GNOME extension guide](https://gjs.guide/extensions/development/creating.html#wayland-sessions). Run `gnome-extensions info emoji-keyboard@emojikeyboard.local` to check its state.

With the extension enabled, copy text and an image from another application, then read the counters:

```sh
gdbus call --session --dest org.emojikeyboard.ClipboardProbe \
    --object-path /org/emojikeyboard/ClipboardProbe \
    --method org.emojikeyboard.ClipboardProbe.GetStats
```

The four returned numbers are total, text, image, and other owner changes since the extension was enabled. They reset when the extension reloads. The classification and settings-schema tests run through `ctest --test-dir build --output-on-failure` when their GNOME tools are installed. When Qt opens the picker, it asks Shell to place the window beside the mouse pointer within the current monitor's work area; ordinary Wayland clients cannot set a global window position themselves.

Run the GNOME 50.1 end-to-end test in a disposable headless Shell and D-Bus session:

```sh
cmake --build build --target emoji-keyboard gnome_test_target
python3 tests/integration/tst_gnome_headless.py
```

It copies the extension and creates private XDG settings and data paths; it does not change the current desktop profile. It checks pointer placement near the upper left and screen edge, both shortcuts from GTK Wayland, Qt Wayland, XWayland, and a terminal-style target; text/image capture, a rendered image thumbnail, focus restoration, history image paste, prior text/image restoration, and survival across a fresh Shell and bus session. Qt Wayland clipboard writes require a focused window and a fresh input event. A real installed-session logout/login remains a release check.

With the Qt D-Bus service installed, the extension can activate the hidden picker before keyboard shortcuts are wired up:

```sh
gdbus call --session --dest org.emojikeyboard.ClipboardProbe \
    --object-path /org/emojikeyboard/ClipboardProbe \
    --method org.emojikeyboard.ClipboardProbe.ShowEmoji
```

Use `ShowClipboard` to open the history view. The installer writes the session D-Bus activation file for the installed binary and user service. For a source-tree test, the generated `build/org.emojikeyboard.App.service` points to the executable in the current build directory.

The extension contains `show-emoji` and `show-clipboard` bindings with empty defaults. Guided setup enables `Super+.` and `Super+V` unless the user has chosen different keys in **More actions → Keyboard shortcuts…**. The picker writes those GSettings keys directly; Shell updates the bindings without restarting. In an isolated GNOME 50.1 session, removing only overlapping combinations from IBus's emoji hotkey and GNOME's message-tray shortcut allowed the picker bindings to register. The installer backs up changed arrays after one prompt and restores unchanged values on uninstall.

For shortcut-opened selections, Shell remembers the focused window, activates the Qt picker, then restores target focus and sends `Ctrl+V` or `Ctrl+Shift+V` for recognized terminals. The Clipboard view's **Paste shortcut** setting can force either combination when detection is wrong; the Qt app sends that choice with each paste request. Closing the picker cancels an outstanding paste request. The headless test uses a GTK 4 entry with the `org.gnome.Ptyxis` app ID to check terminal shortcut detection. If focus cannot be restored, the item remains copied for manual paste. An emoji selection temporarily replaces the clipboard; Shell restores prior plain text or PNG data after 750 ms only while that emoji is still current and no newer clipboard owner event has occurred. Newer-copy preservation, including another copy of the same emoji, is covered by isolated GNOME and controller tests. The delay cannot guarantee that every target has finished reading the temporary clipboard.
