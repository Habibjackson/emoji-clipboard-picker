const TERMINAL_IDS = new Set([
    'alacritty',
    'com.mitchellh.ghostty',
    'foot',
    'gnome-terminal',
    'io.elementary.terminal',
    'kitty',
    'konsole',
    'org.gnome.console',
    'org.gnome.ptyxis',
    'org.gnome.terminal',
    'org.wezfurlong.wezterm',
    'ptyxis',
    'terminator',
    'tilix',
]);

export function pasteShortcutFor(appId, wmClass, mode = 'auto') {
    if (mode === 'ctrl-v')
        return 'Ctrl+V';
    if (mode === 'ctrl-shift-v')
        return 'Ctrl+Shift+V';
    const identifiers = [appId, wmClass].filter(Boolean)
        .map(identifier => identifier.toLowerCase());
    return identifiers.some(identifier => TERMINAL_IDS.has(identifier))
        ? 'Ctrl+Shift+V' : 'Ctrl+V';
}
