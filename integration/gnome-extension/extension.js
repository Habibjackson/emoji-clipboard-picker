import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Clutter from 'gi://Clutter';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

import {classifyMimeTypes} from './clipboardProbe.js';
import {pasteShortcutFor} from './pasteShortcut.js';

const BUS_NAME = 'org.emojikeyboard.ClipboardProbe';
const OBJECT_PATH = '/org/emojikeyboard/ClipboardProbe';
const INTERFACE_XML = `
<node>
  <interface name="org.emojikeyboard.ClipboardProbe">
    <method name="GetStats">
      <arg name="total" type="u" direction="out"/>
      <arg name="text" type="u" direction="out"/>
      <arg name="image" type="u" direction="out"/>
      <arg name="other" type="u" direction="out"/>
    </method>
    <method name="ShowEmoji"/>
    <method name="ShowClipboard"/>
    <method name="CancelPaste"/>
    <method name="PasteSelection">
      <arg name="shortcutMode" type="s" direction="in"/>
      <arg name="status" type="s" direction="out"/>
      <arg name="shortcut" type="s" direction="out"/>
      <arg name="clipboardRevision" type="u" direction="out"/>
    </method>
    <method name="RestoreClipboardIfCurrent">
      <arg name="clipboardRevision" type="u" direction="in"/>
      <arg name="temporaryText" type="s" direction="in"/>
      <arg name="mimeType" type="s" direction="in"/>
      <arg name="priorContent" type="ay" direction="in"/>
      <arg name="status" type="s" direction="out"/>
    </method>
    <signal name="Changed">
      <arg name="kind" type="s"/>
    </signal>
  </interface>
</node>`;

export default class EmojiKeyboardExtension extends Extension {
    enable() {
        // The native X11 backend owns clipboard monitoring and key grabs there.
        this._inactiveOnX11 = !Meta.is_wayland_compositor();
        if (this._inactiveOnX11)
            return;
        this._counts = {total: 0, text: 0, image: 0, other: 0};
        this._reportedActivationError = false;
        this._clipboardRevision = 0;
        this._openClipboardRevision = null;
        this._targetWindow = null;
        this._pickerPid = 0;
        this._pasteGeneration = 0;
        try {
            this._bus = Gio.bus_get_sync(Gio.BusType.SESSION, null);
            this._exported = Gio.DBusExportedObject.wrapJSObject(INTERFACE_XML, this);
            this._exported.export(this._bus, OBJECT_PATH);
            this._busOwner = Gio.bus_own_name_on_connection(this._bus, BUS_NAME,
                Gio.BusNameOwnerFlags.NONE, null, null);
            this._selection = global.display.get_selection();
            this._selectionSignal = this._selection.connect('owner-changed',
                (_selection, selectionType) => this._onOwnerChanged(selectionType));
            this._settings = this.getSettings();
            Main.wm.addKeybinding('show-emoji', this._settings,
                Meta.KeyBindingFlags.NONE, Shell.ActionMode.NORMAL,
                () => this._showFromShortcut('emoji'));
            this._emojiBindingAdded = true;
            Main.wm.addKeybinding('show-clipboard', this._settings,
                Meta.KeyBindingFlags.NONE, Shell.ActionMode.NORMAL,
                () => this._showFromShortcut('clipboard'));
            this._clipboardBindingAdded = true;
        } catch (error) {
            this.disable();
            throw error;
        }
    }

    disable() {
        if (this._inactiveOnX11) {
            this._inactiveOnX11 = false;
            return;
        }
        this._pasteGeneration++;
        this._targetWindow = null;
        this._openClipboardRevision = null;
        this._pickerPid = 0;
        this._keyboard = null;
        if (this._emojiBindingAdded) {
            Main.wm.removeKeybinding('show-emoji');
            this._emojiBindingAdded = false;
        }
        if (this._clipboardBindingAdded) {
            Main.wm.removeKeybinding('show-clipboard');
            this._clipboardBindingAdded = false;
        }
        this._settings = null;
        if (this._selectionSignal) {
            this._selection.disconnect(this._selectionSignal);
            this._selectionSignal = 0;
        }
        this._selection = null;
        if (this._busOwner) {
            Gio.bus_unown_name(this._busOwner);
            this._busOwner = 0;
        }
        this._exported?.unexport();
        this._exported = null;
        this._bus = null;
        this._counts = null;
    }

    GetStats() {
        const counts = this._counts;
        return [counts.total, counts.text, counts.image, counts.other];
    }

    ShowEmoji() {
        this._showPicker('ShowEmoji');
    }

    ShowClipboard() {
        this._showPicker('ShowClipboard');
    }

    CancelPaste() {
        this._pasteGeneration++;
        this._targetWindow = null;
        this._openClipboardRevision = null;
    }

    PasteSelectionAsync([shortcutMode], invocation) {
        const target = this._targetWindow;
        this._targetWindow = null;
        const generation = ++this._pasteGeneration;
        const openClipboardRevision = this._openClipboardRevision;
        const clipboardRevision = openClipboardRevision !== null
            && this._clipboardRevision === openClipboardRevision + 1
            ? this._clipboardRevision : 0;
        this._openClipboardRevision = null;
        const finish = (status, shortcut = '') => {
            invocation.return_value(new GLib.Variant('(ssu)',
                [status, shortcut, clipboardRevision]));
            return GLib.SOURCE_REMOVE;
        };

        if (!target || !global.display.list_all_windows().includes(target)) {
            finish('target-missing');
            return;
        }

        if (openClipboardRevision !== null
            && this._clipboardRevision > openClipboardRevision + 1) {
            finish('clipboard-changed');
            return;
        }

        const shortcut = pasteShortcutFor(target.get_gtk_application_id(),
            target.get_wm_class(), shortcutMode);
        try {
            target.activate(global.get_current_time());
        } catch {
            finish('focus-failed');
            return;
        }

        const started = GLib.get_monotonic_time();
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, 25, () => {
            if (generation !== this._pasteGeneration || !this._bus)
                return finish('cancelled');
            if (!global.display.list_all_windows().includes(target))
                return finish('target-missing');
            if (global.display.get_focus_window() === target) {
                try {
                    this._sendPaste(shortcut);
                    return finish('sent', shortcut);
                } catch {
                    console.warn('Emoji Keyboard could not send paste');
                    return finish('input-failed');
                }
            }
            if (GLib.get_monotonic_time() - started >= 750000)
                return finish('focus-failed');
            return GLib.SOURCE_CONTINUE;
        });
    }

    RestoreClipboardIfCurrentAsync(params, invocation) {
        const [revision, temporaryText, mimeType, data] = params;
        const finish = status => {
            invocation.return_value(new GLib.Variant('(s)', [status]));
        };
        if (!this._bus || !revision || revision !== this._clipboardRevision) {
            finish('changed');
            return;
        }
        if (!temporaryText || temporaryText.length > 256
            || !((mimeType === 'text/plain;charset=utf-8' && data.length <= 1024 * 1024)
                || (mimeType === 'image/png' && data.length <= 10 * 1024 * 1024))) {
            finish('invalid');
            return;
        }
        const clipboard = St.Clipboard.get_default();
        const type = St.ClipboardType.CLIPBOARD;
        const generation = this._pasteGeneration;
        clipboard.get_text(type, (_clipboard, currentText) => {
            if (!this._bus || generation !== this._pasteGeneration
                || revision !== this._clipboardRevision || currentText !== temporaryText) {
                finish('changed');
                return;
            }
            try {
                const bytes = Uint8Array.from(data);
                if (mimeType === 'text/plain;charset=utf-8') {
                    const text = new TextDecoder('utf-8', {fatal: true}).decode(bytes);
                    clipboard.set_text(type, text);
                } else {
                    clipboard.set_content(type, mimeType, new GLib.Bytes(bytes));
                }
                finish('restored');
            } catch {
                console.warn('Emoji Keyboard could not restore clipboard');
                finish('unavailable');
            }
        });
    }

    _showFromShortcut(view) {
        const focused = global.display.get_focus_window();
        const pickerFocused = this._pickerPid !== 0 && !!focused
            && focused.get_pid() === this._pickerPid;
        if (!pickerFocused)
            this._targetWindow = focused;
        this._openClipboardRevision = this._clipboardRevision;
        this._pasteGeneration++;

        this._bus.call('org.emojikeyboard.App', '/org/emojikeyboard/App',
            'org.emojikeyboard.App', 'ShowFromShortcut',
            new GLib.Variant('(sb)', [view, pickerFocused]), new GLib.VariantType('(u)'),
            Gio.DBusCallFlags.NONE, -1, null, (connection, result) => {
                try {
                    const reply = connection.call_finish(result);
                    this._pickerPid = reply.deep_unpack()[0];
                } catch {
                    this._targetWindow = null;
                    console.warn('Emoji Keyboard could not open');
                }
            });
    }

    _sendPaste(shortcut) {
        const seat = Clutter.get_default_backend().get_default_seat();
        this._keyboard ??= seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);
        const time = GLib.get_monotonic_time();
        this._keyboard.notify_key(time, 29, Clutter.KeyState.PRESSED);
        if (shortcut === 'Ctrl+Shift+V')
            this._keyboard.notify_key(time + 1000, 42, Clutter.KeyState.PRESSED);
        this._keyboard.notify_key(time + 2000, 47, Clutter.KeyState.PRESSED);
        this._keyboard.notify_key(time + 3000, 47, Clutter.KeyState.RELEASED);
        if (shortcut === 'Ctrl+Shift+V')
            this._keyboard.notify_key(time + 4000, 42, Clutter.KeyState.RELEASED);
        this._keyboard.notify_key(time + 5000, 29, Clutter.KeyState.RELEASED);
    }

    _showPicker(method) {
        this._bus.call('org.emojikeyboard.App', '/org/emojikeyboard/App',
            'org.emojikeyboard.App', method, null, null, Gio.DBusCallFlags.NONE,
            -1, null, (connection, result) => {
                try {
                    connection.call_finish(result);
                } catch {
                    console.warn('Emoji Keyboard could not open');
                }
            });
    }

    _onOwnerChanged(selectionType) {
        if (selectionType !== Meta.SelectionType.SELECTION_CLIPBOARD)
            return;

        this._clipboardRevision++;

        const mimeTypes = this._selection.get_mimetypes(selectionType);
        const kind = classifyMimeTypes(mimeTypes);
        if (kind === 'empty')
            return;

        this._counts.total++;
        this._counts[kind]++;
        this._exported?.emit_signal('Changed', new GLib.Variant('(s)', [kind]));
        if (kind === 'text' || kind === 'image')
            this._notifyPicker(kind);
    }

    _notifyPicker(kind) {
        this._bus.call('org.emojikeyboard.App', '/org/emojikeyboard/App',
            'org.emojikeyboard.App', 'NotifyClipboardChanged',
            new GLib.Variant('(s)', [kind]), null, Gio.DBusCallFlags.NONE,
            -1, null, (connection, result) => {
                try {
                    connection.call_finish(result);
                    this._reportedActivationError = false;
                } catch {
                    if (!this._reportedActivationError) {
                        console.warn('Emoji Keyboard could not monitor clipboard');
                        this._reportedActivationError = true;
                    }
                }
            });
    }
}
