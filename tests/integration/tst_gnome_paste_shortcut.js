import {pasteShortcutFor} from '../../integration/gnome-extension/pasteShortcut.js';

const cases = [
    [null, 'Gtk4Demo', 'Ctrl+V'],
    ['org.gnome.Ptyxis', null, 'Ctrl+Shift+V'],
    [null, 'gnome-terminal', 'Ctrl+Shift+V'],
    [null, 'Konsole', 'Ctrl+Shift+V'],
    [null, 'unknown-editor', 'Ctrl+V'],
];

for (const [appId, wmClass, expected] of cases) {
    const actual = pasteShortcutFor(appId, wmClass);
    if (actual !== expected)
        throw new Error(`Expected ${expected}, got ${actual}`);
}
