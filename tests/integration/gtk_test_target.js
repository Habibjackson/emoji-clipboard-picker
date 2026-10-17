import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Gdk from 'gi://Gdk?version=4.0';
import Gtk from 'gi://Gtk?version=4.0';

const [resultPath, title, applicationId, mode] = ARGV;
if (!resultPath || !title || !applicationId)
    throw new Error('Expected a result path, title, and application ID');

const app = new Gtk.Application({
    application_id: applicationId,
    flags: Gio.ApplicationFlags.NON_UNIQUE,
});

app.connect('activate', () => {
    const window = new Gtk.ApplicationWindow({application: app, title});
    window.set_default_size(320, 160);
    const entry = new Gtk.Entry({placeholder_text: 'Paste here'});
    window.set_child(entry);

    const copyKey = new Gtk.EventControllerKey();
    copyKey.set_propagation_phase(Gtk.PropagationPhase.CAPTURE);
    copyKey.connect('key-pressed', (_controller, keyval, _keycode, modifiers) => {
        if (mode === 'terminal' || keyval !== Gdk.KEY_c
            || modifiers !== 0)
            return false;
        const bytes = new GLib.Bytes(new TextEncoder().encode('GNOME GTK prior text'));
        const provider = Gdk.ContentProvider.new_for_bytes('text/plain;charset=utf-8', bytes);
        Gdk.Display.get_default().get_clipboard().set_content(provider);
        return true;
    });
    window.add_controller(copyKey);

    if (mode === 'terminal') {
        const keys = new Gtk.EventControllerKey();
        keys.connect('key-pressed', (_controller, keyval, _keycode, modifiers) => {
            if (keyval !== Gdk.KEY_v && keyval !== Gdk.KEY_V
                || !(modifiers & Gdk.ModifierType.CONTROL_MASK))
                return false;
            if (!(modifiers & Gdk.ModifierType.SHIFT_MASK))
                return true;
            const clipboard = Gdk.Display.get_default().get_clipboard();
            clipboard.read_text_async(null, (source, result) => {
                const text = source.read_text_finish(result);
                if (text !== null)
                    GLib.file_set_contents(resultPath, new TextEncoder().encode(text));
            });
            return true;
        });
        entry.add_controller(keys);
    } else {
        entry.connect('notify::text', () => {
            GLib.file_set_contents(resultPath, new TextEncoder().encode(entry.text));
        });
    }

    window.present();
    entry.grab_focus();
    print('READY');
});

app.run([]);
