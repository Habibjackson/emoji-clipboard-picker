import {classifyMimeTypes} from '../../integration/gnome-extension/clipboardProbe.js';

const cases = [
    [[], 'empty'],
    [['text/plain;charset=utf-8'], 'text'],
    [['image/png', 'text/plain'], 'image'],
    [['text/uri-list', 'text/plain'], 'other'],
    [['application/x-keepassxc-secret', 'text/plain'], 'other'],
    [['application/octet-stream'], 'other'],
];

for (const [formats, expected] of cases) {
    const actual = classifyMimeTypes(formats);
    if (actual !== expected)
        throw new Error(`Expected ${expected}, got ${actual}`);
}
