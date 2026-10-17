"""Installer checks using an isolated home and simulated GSettings arrays."""

import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[2] / "scripts/emoji_keyboard.py"
spec = importlib.util.spec_from_file_location("emoji_keyboard_setup", SCRIPT)
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class SetupTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.home = Path(self.temporary.name)
        self.environment = mock.patch.dict(os.environ, {
            "HOME": str(self.home),
            "XDG_DATA_HOME": str(self.home / "data"),
            "XDG_CONFIG_HOME": str(self.home / "config"),
            "XDG_CURRENT_DESKTOP": "GNOME",
        })
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.paths = setup.locations()
        self.state = setup.load_state(self.paths)
        schema = self.paths["extension"] / "schemas/gschemas.compiled"
        schema.parent.mkdir(parents=True)
        schema.write_bytes(b"test schema")
        self.values = {
            "ibus": ["<Super>period", "<Super>semicolon"],
            "tray": ["<Super>v", "<Super>m"],
            "emoji": [],
            "clipboard": [],
        }
        getter = mock.patch.object(setup, "gsettings_get", side_effect=lambda _p, n: self.values[n].copy())
        setter = mock.patch.object(setup, "gsettings_set", side_effect=self.set_array)
        getter.start()
        setter.start()
        self.addCleanup(getter.stop)
        self.addCleanup(setter.stop)

    def set_array(self, _paths, name, values):
        self.values[name] = values.copy()

    def test_guided_setup_preserves_other_combinations_and_is_idempotent(self):
        setup.setup_shortcuts(self.paths, self.state, yes=True)
        self.assertEqual(self.values["ibus"], ["<Super>semicolon"])
        self.assertEqual(self.values["tray"], ["<Super>m"])
        self.assertEqual(self.values["emoji"], ["<Super>period"])
        self.assertEqual(self.values["clipboard"], ["<Super>v"])
        self.assertEqual(self.state["shortcuts"]["ibus"]["original"],
                         ["<Super>period", "<Super>semicolon"])
        self.assertEqual(self.paths["state"].stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.paths["state"].parent.stat().st_mode & 0o777, 0o700)
        initial = self.paths["state"].read_bytes()
        setup.setup_shortcuts(self.paths, self.state, yes=True)
        self.assertEqual(self.paths["state"].read_bytes(), initial)

    def test_uninstall_restores_only_unchanged_values(self):
        setup.setup_shortcuts(self.paths, self.state, yes=True)
        self.values["tray"] = ["<Super>n"]
        setup.restore_shortcuts(self.paths, self.state)
        self.assertEqual(self.values["ibus"], ["<Super>period", "<Super>semicolon"])
        self.assertEqual(self.values["tray"], ["<Super>n"])
        self.assertEqual(self.values["emoji"], [])
        self.assertEqual(self.values["clipboard"], [])

    def test_failed_shortcut_write_keeps_backup_retryable(self):
        with mock.patch.object(setup, "gsettings_set", side_effect=setup.SetupError("denied")):
            with self.assertRaises(setup.SetupError):
                setup.change_shortcut(self.paths, self.state, "ibus", ["<Super>semicolon"])
        self.assertNotIn("ibus", self.state["shortcuts"])
        self.assertEqual(self.values["ibus"], ["<Super>period", "<Super>semicolon"])

    def test_noninteractive_setup_does_not_change_conflicts(self):
        with mock.patch("sys.stdin", io.StringIO("y\n")):
            setup.setup_shortcuts(self.paths, self.state)
        self.assertEqual(self.values["ibus"], ["<Super>period", "<Super>semicolon"])
        self.assertEqual(self.values["emoji"], [])
        self.assertEqual(self.state["shortcuts"], {})

    def test_non_gnome_x11_leaves_gnome_settings_alone(self):
        with mock.patch.dict(os.environ, {"XDG_CURRENT_DESKTOP": "XFCE", "XDG_SESSION_TYPE": "x11"}):
            setup.setup_shortcuts(self.paths, self.state, yes=True)
        self.assertEqual(self.values["ibus"], ["<Super>period", "<Super>semicolon"])
        self.assertEqual(self.state["shortcuts"], {})

    def test_install_file_refuses_modified_content(self):
        target = self.paths["desktop"]
        setup.install_file(self.paths, self.state, target, b"original", 0o644)
        target.write_bytes(b"user edit")
        with self.assertRaises(setup.SetupError):
            setup.install_file(self.paths, self.state, target, b"new release", 0o644)
        self.assertEqual(target.read_bytes(), b"user edit")

    def test_install_and_remove_files_without_touching_shortcuts(self):
        (self.paths["extension"] / "schemas/gschemas.compiled").unlink()
        binary = self.home / "build with spaces/emoji-keyboard"
        binary.parent.mkdir()
        binary.write_bytes(b"test executable")
        with mock.patch.object(setup, "data_paths", return_value={
            "historyDirectory": str(self.home / "history"),
            "settingsFile": str(self.home / "settings.ini"),
        }), mock.patch.object(setup, "optional_command", return_value=True), \
             mock.patch.object(setup, "enable_discovered_extension"), \
             mock.patch.object(setup, "ensure_extension_enabled"), \
             mock.patch.object(setup, "remove_enabled_extension"):
            setup.install(self.paths, self.state, binary, no_shortcuts=True)
            setup.install(self.paths, self.state, binary, no_shortcuts=True)
            self.assertEqual(self.paths["bin"].read_bytes(), b"test executable")
            self.assertIn("SystemdService=emoji-keyboard.service", self.paths["dbus"].read_text())
            self.assertIn("ExecStart=", self.paths["unit"].read_text())
            self.assertTrue((self.paths["extension"] / "schemas/gschemas.compiled").is_file())
            self.assertEqual(self.state["shortcuts"], {})
            setup.uninstall(self.paths, self.state, purge=False)
        self.assertFalse(self.paths["bin"].exists())
        self.assertFalse(self.paths["extension"].exists())

    def test_uninstall_keeps_history_until_explicit_purge(self):
        database_dir = self.home / "data/EmojiKeyboard/EmojiKeyboard"
        database_dir.mkdir(parents=True)
        database = database_dir / "history.sqlite"
        database.write_bytes(b"private history")
        self.state["historyDirectory"] = str(database_dir)
        setup.install_file(self.paths, self.state, self.paths["desktop"], b"managed", 0o644)
        with mock.patch.object(setup, "optional_command", return_value=True), \
             mock.patch.object(setup, "remove_enabled_extension"):
            setup.uninstall(self.paths, self.state, purge=False)
        self.assertTrue(database.exists())
        self.assertFalse(self.paths["desktop"].exists())
        with mock.patch.object(setup, "optional_command", return_value=True), \
             mock.patch.object(setup, "remove_enabled_extension"):
            setup.uninstall(self.paths, self.state, purge=True)
        self.assertFalse(database.exists())

    def test_extension_enable_preserves_other_extensions(self):
        arrays = ["another@example.org"]

        def fake_run(*args, **_kwargs):
            if args[:2] == ("gsettings", "get"):
                return subprocess_result(repr(arrays))
            if args[:2] == ("gsettings", "set"):
                arrays[:] = setup.parse_string_array(args[-1], "enabled-extensions")
                return subprocess_result("")
            raise AssertionError(args)

        with mock.patch.object(setup, "run", side_effect=fake_run):
            setup.ensure_extension_enabled()
            setup.ensure_extension_enabled()
            self.assertEqual(arrays, ["another@example.org", setup.UUID])
            setup.remove_enabled_extension()
        self.assertEqual(arrays, ["another@example.org"])

    def test_new_extension_waits_for_shell_login(self):
        listing = subprocess.CompletedProcess([], 0, "another@example.org\n", "")
        output = io.StringIO()
        with mock.patch.object(setup, "run", return_value=listing), \
             mock.patch.object(setup, "optional_command") as enable, \
             mock.patch("sys.stdout", output):
            setup.enable_discovered_extension()
        enable.assert_not_called()
        self.assertIn("log out and log back in", output.getvalue())

    def test_discovered_extension_is_enabled(self):
        listing = subprocess.CompletedProcess([], 0, f"{setup.UUID}\n", "")
        with mock.patch.object(setup, "run", return_value=listing), \
             mock.patch.object(setup, "optional_command") as enable:
            setup.enable_discovered_extension()
        enable.assert_called_once_with("gnome-extensions", "enable", setup.UUID)

    def test_real_gsettings_round_trip_in_isolated_keyfile(self):
        (self.paths["extension"] / "schemas/gschemas.compiled").unlink()
        if not shutil.which("gsettings") or not shutil.which("glib-compile-schemas"):
            self.skipTest("GSettings tools are unavailable")
        schemas = subprocess.run(["gsettings", "list-schemas"], text=True,
                                 capture_output=True, check=False).stdout.splitlines()
        required = {"org.gnome.shell", "org.gnome.shell.keybindings",
                    "org.freedesktop.ibus.panel.emoji"}
        if not required.issubset(schemas):
            self.skipTest("GNOME and IBus schemas are unavailable")

        child_env = os.environ.copy()
        child_env["GSETTINGS_BACKEND"] = "keyfile"
        child_env["XDG_CURRENT_DESKTOP"] = "GNOME"
        child_env["XDG_SESSION_TYPE"] = "wayland"
        child_env["HOME"] = str(self.home)
        fake_commands = self.home / "fake-commands"
        fake_commands.mkdir()
        child_env["PATH"] = f"{fake_commands}:{child_env['PATH']}"
        for name, status in (("systemctl", 0), ("gnome-extensions", 1)):
            command = fake_commands / name
            command.write_text(f"#!/bin/sh\nexit {status}\n")
            command.chmod(0o755)

        history = self.home / "history"
        fake_binary = self.home / "build/emoji-keyboard"
        fake_binary.parent.mkdir()
        fake_binary.write_text("#!/bin/sh\nprintf '%s\\n' '" + json.dumps({
            "historyDirectory": str(history),
            "settingsFile": str(self.home / "settings.ini"),
        }) + "'\n")
        fake_binary.chmod(0o755)

        def gsettings(*arguments):
            return subprocess.run(["gsettings", *arguments], env=child_env, text=True,
                                  capture_output=True, check=True).stdout.strip()

        gsettings("set", "org.freedesktop.ibus.panel.emoji", "hotkey",
                  '["<Super>period", "<Super>semicolon"]')
        gsettings("set", "org.gnome.shell.keybindings", "toggle-message-tray",
                  '["<Super>v", "<Super>m"]')

        def installer(*arguments):
            result = subprocess.run([sys.executable, str(SCRIPT), *arguments], env=child_env,
                                    text=True, capture_output=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)

        installer("install", "--binary", str(fake_binary), "--no-shortcuts")
        installer("setup-shortcuts", "--yes")
        self.assertEqual(gsettings("get", "org.freedesktop.ibus.panel.emoji", "hotkey"),
                         "['<Super>semicolon']")
        self.assertEqual(gsettings("get", "org.gnome.shell.keybindings", "toggle-message-tray"),
                         "['<Super>m']")
        extension_schema = str(self.paths["extension"] / "schemas")
        self.assertEqual(gsettings("--schemadir", extension_schema, "get", setup.GNOME_SCHEMA,
                                   "show-emoji"), "['<Super>period']")
        gsettings("set", "org.gnome.shell.keybindings", "toggle-message-tray",
                  '["<Super>n"]')
        installer("uninstall")
        self.assertEqual(gsettings("get", "org.freedesktop.ibus.panel.emoji", "hotkey"),
                         "['<Super>period', '<Super>semicolon']")
        self.assertEqual(gsettings("get", "org.gnome.shell.keybindings", "toggle-message-tray"),
                         "['<Super>n']")
        self.assertFalse(self.paths["bin"].exists())


def subprocess_result(output):
    return type("Result", (), {"stdout": output})()


if __name__ == "__main__":
    unittest.main()
