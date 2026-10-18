# Privacy and local data

Emoji Keyboard is a local clipboard manager. The application makes no network requests. On GNOME Wayland, its Shell extension reads supported clipboard payloads and sends them to the resident Qt process over the current user's session D-Bus. On X11, the native backend observes clipboard changes directly.

## What is stored

The SQLite history stores ordinary UTF-8 text and supported bitmap images, normalized to PNG. It keeps at most five pinned and twenty unpinned entries. Recopying an item updates its position while preserving its pin state. Emoji recents are stored separately, up to 24 selected sequences. These records survive application restarts and logout/login.

Capture skips empty and unsupported content, file copy operations, and recognized secret-marked clipboard formats. Text is limited to 1 MiB of UTF-8; encoded images to 10 MiB and bounded dimensions. These checks depend on MIME metadata and cannot identify every password or sensitive string. Pause capture before copying material you do not want saved, and remove any item captured unexpectedly.

The database directory is restricted to the current user (`0700`), and the database file to user read/write (`0600`). The history is **not encrypted**. Other software running as your user, backups, or someone with access to your unlocked account may still read it. The app avoids logging payloads; decoder diagnostics that could contain image bytes are suppressed.

## Find and delete local data

Run `./build/emoji-keyboard --print-data-paths` to see the exact `historyDirectory` and `settingsFile` chosen by Qt. The history database is `history.sqlite` inside that directory. Qt settings store theme, paste mode, and X11 shortcut preferences separately; GNOME shortcut preferences live in GSettings.

The clipboard view can delete one entry, clear unpinned recent entries, or clear all clipboard entries. **Clear all** does not remove emoji recents. To remove the history database and its SQLite sidecar files during uninstall, run `python3 scripts/emoji_keyboard.py uninstall --purge-history`. A normal uninstall keeps history so a later reinstall can reuse it; the purge does not remove Qt settings, GNOME settings, or copies made by a system backup. Database deletion is not a guarantee of secure erasure on every filesystem.

Automatic emoji paste may temporarily replace the current clipboard. The app restores a prior plain text or PNG item only while its temporary emoji is still current and no newer copy was observed. Unsupported or oversized prior content may not be restorable. See [Install and use](INSTALL.md#shortcuts-and-paste-behavior) for the paste flow.
