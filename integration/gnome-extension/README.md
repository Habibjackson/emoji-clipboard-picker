# GNOME clipboard event probe

This development extension targets GNOME Shell 50. It listens for `Meta.Selection::owner-changed` on the regular clipboard, classifies offered MIME types as text, image, or other, and exposes event counts over session D-Bus. Text and image events activate the hidden Qt picker through D-Bus. It does not read or log clipboard payloads or change GNOME's existing shortcut settings.

Build and install the probe with:

```sh
gnome-extensions pack integration/gnome-extension --out-dir /tmp -f \
    --extra-source=clipboardProbe.js
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

Use `ShowClipboard` to open the history view. For a source-tree test, copy the generated `build/org.emojikeyboard.App.service` into `~/.local/share/dbus-1/services/`. It points to the executable in the current build directory. The user-level installer will generate a service file for the final installed path later.

The extension contains `show-emoji` and `show-clipboard` bindings with empty defaults. In an isolated GNOME 50.1 session, removing only `<Super>period` from IBus's emoji hotkey and `<Super>v` from GNOME's message-tray shortcut allowed both bindings to register. Synthetic `Super+.` and `Super+V` presses from a focused GTK window opened the corresponding Qt views. Guided setup and restoration of the original bindings are still pending, so do not change your desktop bindings for this development probe.
