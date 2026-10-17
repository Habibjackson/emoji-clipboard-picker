export function classifyMimeTypes(mimeTypes) {
    if (!mimeTypes || mimeTypes.length === 0)
        return 'empty';

    const types = mimeTypes.map(type => type.toLowerCase());
    if (types.some(type => type === 'text/uri-list'
            || type === 'x-special/gnome-copied-files'
            || type === 'application/x-kde-passwordmanagerhint'
            || type === 'application/x-keepassxc-secret')) {
        return 'other';
    }
    if (types.some(type => type.startsWith('image/')))
        return 'image';
    if (types.some(type => type === 'text/plain' || type.startsWith('text/plain;')))
        return 'text';
    return 'other';
}
