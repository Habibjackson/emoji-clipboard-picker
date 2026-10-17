import {pasteShortcutFor} from '../../integration/gnome-extension/pasteShortcut.js';

const cases = [
    [null, 'Gtk4Demo', 'Ctrl+V'],
    ['org.gnome.Ptyxis', null, 'Ctrl+Shift+V'],
    [null, 'gnome-terminal', 'Ctrl+Shift+V'],
    [null, 'Konsole', 'Ctrl+Shift+V'],
    [null, 'unknown-editor', 'Ctrl+V'],
    ['org.gnome.Ptyxis', null, 'Ctrl+V', 'ctrl-v'],
    [null, 'Gtk4Demo', 'Ctrl+Shift+V', 'ctrl-shift-v'],
];

for (const [appId, wmClass, expected, mode] of cases) {
    const actual = pasteShortcutFor(appId, wmClass, mode);
    if (actual !== expected)
        throw new Error(`Expected ${expected}, got ${actual}`);
}
