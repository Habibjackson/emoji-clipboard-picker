import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

import {classifyMimeTypes} from './clipboardProbe.js';

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
    <signal name="Changed">
      <arg name="kind" type="s"/>
    </signal>
  </interface>
</node>`;

export default class EmojiKeyboardExtension extends Extension {
    enable() {
        this._counts = {total: 0, text: 0, image: 0, other: 0};
        this._reportedActivationError = false;
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
                () => this._showPicker('ShowEmoji'));
            this._emojiBindingAdded = true;
            Main.wm.addKeybinding('show-clipboard', this._settings,
                Meta.KeyBindingFlags.NONE, Shell.ActionMode.NORMAL,
                () => this._showPicker('ShowClipboard'));
            this._clipboardBindingAdded = true;
        } catch (error) {
            this.disable();
            throw error;
        }
    }

    disable() {
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

    _showPicker(method) {
        this._bus.call('org.emojikeyboard.App', '/org/emojikeyboard/App',
            'org.emojikeyboard.App', method, null, null, Gio.DBusCallFlags.NONE,
            -1, null, (connection, result) => {
                try {
                    connection.call_finish(result);
                } catch (error) {
                    console.warn(`Emoji Keyboard could not open: ${error.message}`);
                }
            });
    }

    _onOwnerChanged(selectionType) {
        if (selectionType !== Meta.SelectionType.SELECTION_CLIPBOARD)
            return;

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
                } catch (error) {
                    if (!this._reportedActivationError) {
                        console.warn(`Emoji Keyboard could not monitor clipboard: ${error.message}`);
                        this._reportedActivationError = true;
                    }
                }
            });
    }
}
