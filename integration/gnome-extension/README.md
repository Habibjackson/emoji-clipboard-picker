# GNOME Shell bridge

This extension targets GNOME Shell 50. It listens for `Meta.Selection::owner-changed` on the regular clipboard, classifies offered MIME types as text, image, or other, and exposes event counts over session D-Bus. Text and image events activate the hidden Qt picker through D-Bus. It does not read or log clipboard payloads. On X11 it stays inactive because the native Qt backend handles monitoring and shortcuts.

For a normal user install, build the application and run `python3 scripts/emoji_keyboard.py install` from the repository root. This installs the extension, compiles its schema, starts the user service, and guides the known shortcut changes. `python3 scripts/emoji_keyboard.py recheck` reports known conflicts. See the root [README](../../README.md#install-and-shortcuts) for logout/login and removal steps.

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

The four returned numbers are total, text, image, and other owner changes since the extension was enabled. They reset when the extension reloads. Repeat with a GTK app, a Qt Wayland app, and an XWayland app to validate those paths. The classification and settings-schema tests run through `ctest --test-dir build --output-on-failure` when their GNOME tools are installed.

In an isolated GNOME Shell 50.1 test, GTK Wayland, Qt Wayland, and Qt XWayland each produced text and PNG image events. `build/clipboard_test_source` is a painted Qt window for this check. The Qt Wayland source needs keyboard focus and a fresh input event before each clipboard write; an unmapped or unfocused test window cannot establish a Wayland clipboard selection.

With the Qt D-Bus service installed, the extension can activate the hidden picker before keyboard shortcuts are wired up:

```sh
gdbus call --session --dest org.emojikeyboard.ClipboardProbe \
    --object-path /org/emojikeyboard/ClipboardProbe \
    --method org.emojikeyboard.ClipboardProbe.ShowEmoji
```

Use `ShowClipboard` to open the history view. The installer writes the session D-Bus activation file for the installed binary and user service. For a source-tree test, the generated `build/org.emojikeyboard.App.service` points to the executable in the current build directory.

The extension contains `show-emoji` and `show-clipboard` bindings with empty defaults. In an isolated GNOME 50.1 session, removing only `<Super>period` from IBus's emoji hotkey and `<Super>v` from GNOME's message-tray shortcut allowed both bindings to register. Synthetic `Super+.` and `Super+V` presses from a focused GTK window opened the corresponding Qt views. The installer backs up and removes just those known conflicts after one prompt, and restores unchanged values on uninstall.

For shortcut-opened selections, Shell remembers the focused window, waits for it to regain focus, then sends `Ctrl+V` or `Ctrl+Shift+V` for recognized terminals. The Clipboard view's **Paste shortcut** setting can force either combination when detection is wrong; the Qt app sends that choice with each paste request. Closing the picker cancels an outstanding paste request. The isolated GNOME 50.1 test used a GTK 4 entry and a VTE terminal with the `org.gnome.Ptyxis` app ID. If focus cannot be restored, the item remains copied for manual paste. An emoji selection temporarily replaces the clipboard; Shell restores prior plain text or PNG data after 750 ms only while that emoji is still current and no newer clipboard owner event has occurred. Text, image, and newer-copy preservation, including another copy of the same emoji, passed in the isolated session before the override and cancellation changes; those changes have D-Bus contract tests but still need a live GNOME retest. Clipboard payloads are transferred only within the user's session D-Bus for restoration, never written to the extension log. The delay cannot guarantee that every target has finished reading the temporary clipboard.
