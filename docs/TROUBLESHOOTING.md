# Troubleshooting

Run commands from the repository root. Start with `python3 scripts/emoji_keyboard.py recheck` after installing. It reports the installed files, service, GNOME extension, and known shortcut conflicts. On X11, press each shortcut from another application: setup checks cannot prove a key grab will remain available.

| Symptom | Check and action |
| --- | --- |
| `Super+.` or `Super+V` does nothing on GNOME Wayland | Log out and back in after installing or updating the extension, then run `recheck`. If it is still inactive, inspect `gnome-extensions info emoji-keyboard@emojikeyboard.local` and the user service. |
| `recheck` says extension code is a previous version | Log out and back in. Disabling and re-enabling an already loaded extension can leave old code in Shell memory. |
| GNOME shortcut conflict | Run `python3 scripts/emoji_keyboard.py setup-shortcuts` for known IBus and message-tray overlaps. Check GNOME Settings and other extensions for remaining conflicts. You can record different keys in **More actions → Keyboard shortcuts…**. |
| X11 shortcut unavailable | Read the picker notice; another app owns the key or the optional X11 backend was not built. Install XFixes/XTest/Xi/Xext development files, reconfigure the same `build/` directory, and rebuild if CMake disabled it. |
| History is empty while another Wayland app has focus | Confirm the GNOME extension and user service are active. Use **Capture current** for a missed copy. File operations, secret-marked data, unsupported formats, and oversized content are intentionally skipped. |
| Selection copies but does not paste | Open the picker with its registered shortcut to arm the prior target. For a manual launch or failed focus restore, paste yourself. Change **Paste shortcut** between Auto, `Ctrl+V`, and `Ctrl+Shift+V` if terminal detection is wrong. |
| Emoji appears as a name or placeholder | Install or update a system emoji font. The picker uses system fonts and can still copy a sequence when its local font cannot render it. |
| Theme stays light | Choose **Follow system** or select **Dark** in the top theme control. Follow system reads the desktop appearance portal, then Qt's color scheme hint. |
| App fails to start from an IDE terminal with a GSettings schema error | Remove an inherited `QT_QPA_PLATFORMTHEME` override or launch with `QT_QPA_PLATFORMTHEME=generic ./build/emoji-keyboard`. The app normally selects the generic Qt theme automatically. |

For service errors, inspect `systemctl --user status emoji-keyboard.service` and `journalctl --user -u emoji-keyboard.service -b --no-pager`. If you built from source but have not installed the user integration, `./build/emoji-keyboard` opens the picker for manual copy; global GNOME shortcuts and background Wayland capture require the extension. `./build/emoji-keyboard --print-data-paths` shows where the current user stores history and settings.
