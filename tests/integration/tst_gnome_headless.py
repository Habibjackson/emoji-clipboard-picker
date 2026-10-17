#!/usr/bin/env python3
"""Run a GNOME Wayland bridge smoke test in a disposable headless session.

Run manually after building: python3 tests/integration/tst_gnome_headless.py
The test uses private XDG paths and a separate session bus; it does not install
the extension or change shortcuts in the current desktop session.
"""

from __future__ import annotations

import ast
import os
from pathlib import Path
import shutil
import signal
import sqlite3
import struct
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[2]
UUID = "emoji-keyboard@emojikeyboard.local"


def call(*command: str, timeout: int = 5) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, timeout=timeout,
                          check=False)


def install_test_input(extension: Path) -> None:
    """Add a virtual-keyboard hook only to the disposable extension copy."""
    source = extension / "extension.js"
    contents = source.read_text()
    interface_anchor = '    <method name="GetStats">'
    method_anchor = '    GetStats() {'
    if contents.count(interface_anchor) != 1 or contents.count(method_anchor) != 1:
        raise RuntimeError("Could not add the isolated shortcut test hook")
    contents = contents.replace(interface_anchor,
        '    <method name="InjectShortcut">\n'
        '      <arg name="view" type="s" direction="in"/>\n'
        '      <arg name="state" type="s" direction="out"/>\n'
        '    </method>\n'
        '    <method name="InjectKey">\n'
        '      <arg name="key" type="i" direction="in"/>\n'
        '    </method>\n'
        '    <method name="FocusTitle">\n'
        '      <arg name="title" type="s" direction="in"/>\n'
        '      <arg name="found" type="b" direction="out"/>\n'
        '    </method>\n'
        '    <method name="FocusedTitle">\n'
        '      <arg name="title" type="s" direction="out"/>\n'
        '    </method>\n'
        '    <method name="XDisplay">\n'
        '      <arg name="display" type="s" direction="out"/>\n'
        '    </method>\n'
        '    <method name="SetPointer">\n'
        '      <arg name="x" type="i" direction="in"/>\n'
        '      <arg name="y" type="i" direction="in"/>\n'
        '    </method>\n'
        '    <method name="PointerLocation">\n'
        '      <arg name="x" type="i" direction="out"/>\n'
        '      <arg name="y" type="i" direction="out"/>\n'
        '    </method>\n'
        '    <method name="NudgePointer">\n'
        '      <arg name="dx" type="i" direction="in"/>\n'
        '      <arg name="dy" type="i" direction="in"/>\n'
        '    </method>\n'
        '    <method name="PickerFrame">\n'
        '      <arg name="x" type="i" direction="out"/>\n'
        '      <arg name="y" type="i" direction="out"/>\n'
        '      <arg name="width" type="i" direction="out"/>\n'
        '      <arg name="height" type="i" direction="out"/>\n'
        '    </method>\n' + interface_anchor)
    contents = contents.replace(method_anchor,
        '''    InjectKey(key) {
        const seat = Clutter.get_default_backend().get_default_seat();
        this._testKeyboard ??= seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);
        const time = GLib.get_monotonic_time();
        this._testKeyboard.notify_key(time, key, Clutter.KeyState.PRESSED);
        this._testKeyboard.notify_key(time + 1000, key, Clutter.KeyState.RELEASED);
    }

    FocusTitle(title) {
        const target = global.display.list_all_windows().find(window => window.get_title() === title);
        if (!target)
            return false;
        Main.overview.hide();
        target.activate(global.get_current_time());
        return true;
    }

    FocusedTitle() {
        return global.display.get_focus_window()?.get_title() ?? '';
    }

    XDisplay() {
        return GLib.getenv('DISPLAY') ?? '';
    }

    SetPointer(x, y) {
        const seat = Clutter.get_default_backend().get_default_seat();
        this._testPointer ??= seat.create_virtual_device(Clutter.InputDeviceType.POINTER_DEVICE);
        this._testPointer.notify_absolute_motion(GLib.get_monotonic_time(), x, y);
    }

    PointerLocation() {
        const [x, y] = global.get_pointer();
        return [x, y];
    }

    NudgePointer(dx, dy) {
        this._testPointer.notify_relative_motion(GLib.get_monotonic_time(), dx, dy);
    }

    PickerFrame() {
        const picker = global.display.list_all_windows().find(window =>
            window.get_title() === 'Emoji Keyboard' && window.showing_on_its_workspace());
        if (!picker)
            return [0, 0, 0, 0];
        const frame = picker.get_frame_rect();
        return [frame.x, frame.y, frame.width, frame.height];
    }

    InjectShortcut(view) {
        const key = view === 'emoji' ? 52 : view === 'clipboard' ? 47 : 0;
        if (!key)
            return 'invalid';
        const state = JSON.stringify({
            emoji: this._settings.get_strv('show-emoji'),
            clipboard: this._settings.get_strv('show-clipboard'),
            overview: Main.overview.visible,
        });
        Main.overview.hide();
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, 400, () => {
            const seat = Clutter.get_default_backend().get_default_seat();
            this._testKeyboard ??= seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);
            const time = GLib.get_monotonic_time();
            this._testKeyboard.notify_key(time, 125, Clutter.KeyState.PRESSED);
            this._testKeyboard.notify_key(time + 1000, key, Clutter.KeyState.PRESSED);
            this._testKeyboard.notify_key(time + 2000, key, Clutter.KeyState.RELEASED);
            this._testKeyboard.notify_key(time + 3000, 125, Clutter.KeyState.RELEASED);
            return GLib.SOURCE_REMOVE;
        });
        return state;
    }

''' + method_anchor)
    source.write_text(contents)


def bus_call(destination: str, method: str, path: str,
             *args: str) -> subprocess.CompletedProcess[str]:
    return call("gdbus", "call", "--session", "--dest", destination,
                "--object-path", path, "--method", method, *args)


def wait_for_view(view: str, visible: bool = True) -> None:
    deadline = time.monotonic() + 5
    state = current = None
    while time.monotonic() < deadline:
        state = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.IsVisible",
                         "/org/emojikeyboard/App")
        current = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.CurrentView",
                           "/org/emojikeyboard/App")
        if state.returncode == 0 and current.returncode == 0 \
                and ("true" in state.stdout.lower()) == visible \
                and f"'{view}'" in current.stdout:
            return
        time.sleep(0.1)
    raise RuntimeError(f"Picker did not reach {view} view (visible={visible}); "
                       f"actual: {state.stdout.strip() if state else '?'} "
                       f"{current.stdout.strip() if current else '?'}")


def wait_for_file(path: Path, expected: str, timeout: float = 5) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.exists() and path.read_text() == expected:
            return
        time.sleep(0.1)
    actual = path.read_text() if path.exists() else "<missing>"
    raise RuntimeError(f"Target paste result: expected {expected!r}, got {actual!r}")


def wait_for_focus(title: str) -> None:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        result = bus_call("org.emojikeyboard.ClipboardProbe",
                          "org.emojikeyboard.ClipboardProbe.FocusedTitle",
                          "/org/emojikeyboard/ClipboardProbe")
        if result.returncode == 0 and title in result.stdout:
            return
        time.sleep(0.1)
    raise RuntimeError(f"Target did not gain focus: {title}")


def focus_title(title: str) -> None:
    deadline = time.monotonic() + 5
    result = None
    focused = None
    while time.monotonic() < deadline:
        result = bus_call("org.emojikeyboard.ClipboardProbe",
                          "org.emojikeyboard.ClipboardProbe.FocusTitle",
                          "/org/emojikeyboard/ClipboardProbe", title)
        focused = bus_call("org.emojikeyboard.ClipboardProbe",
                           "org.emojikeyboard.ClipboardProbe.FocusedTitle",
                           "/org/emojikeyboard/ClipboardProbe")
        if result.returncode == 0 and "true" in result.stdout.lower() \
                and focused.returncode == 0 and title in focused.stdout:
            return
        time.sleep(0.25)
    raise RuntimeError(f"Could not focus {title}: found={result.stdout.strip() if result else '?'} "
                       f"focused={focused.stdout.strip() if focused else '?'}")


def inject_shortcut(view: str) -> None:
    result = bus_call("org.emojikeyboard.ClipboardProbe",
                      "org.emojikeyboard.ClipboardProbe.InjectShortcut",
                      "/org/emojikeyboard/ClipboardProbe", view)
    if result.returncode:
        raise RuntimeError(f"Could not inject {view} shortcut: {result.stderr.strip()}")


def inject_key(key: int) -> None:
    press = bus_call("org.emojikeyboard.ClipboardProbe",
                     "org.emojikeyboard.ClipboardProbe.InjectKey",
                     "/org/emojikeyboard/ClipboardProbe", str(key))
    if press.returncode:
        raise RuntimeError(f"Could not inject key {key}: {press.stderr.strip()}")


def set_pointer(x: int, y: int) -> None:
    result = bus_call("org.emojikeyboard.ClipboardProbe",
                      "org.emojikeyboard.ClipboardProbe.SetPointer",
                      "/org/emojikeyboard/ClipboardProbe", str(x), str(y))
    if result.returncode:
        raise RuntimeError(f"Could not move test pointer: {result.stderr.strip()}")
    deadline = time.monotonic() + 3
    actual = None
    while time.monotonic() < deadline:
        location = bus_call("org.emojikeyboard.ClipboardProbe",
                            "org.emojikeyboard.ClipboardProbe.PointerLocation",
                            "/org/emojikeyboard/ClipboardProbe")
        if location.returncode:
            raise RuntimeError(f"Could not read test pointer: {location.stderr.strip()}")
        actual = ast.literal_eval(location.stdout.strip())
        if actual == (x, y):
            return
        nudge = bus_call("org.emojikeyboard.ClipboardProbe",
                         "org.emojikeyboard.ClipboardProbe.NudgePointer",
                         "/org/emojikeyboard/ClipboardProbe",
                         str(x - actual[0]), str(y - actual[1]))
        if nudge.returncode:
            raise RuntimeError(f"Could not nudge test pointer: {nudge.stderr.strip()}")
        time.sleep(0.1)
    raise RuntimeError(f"Test pointer did not move to ({x}, {y}); actual={actual}")


def wait_for_picker_near(x: int, y: int, flip: bool) -> None:
    deadline = time.monotonic() + 5
    frame = None
    while time.monotonic() < deadline:
        result = bus_call("org.emojikeyboard.ClipboardProbe",
                          "org.emojikeyboard.ClipboardProbe.PickerFrame",
                          "/org/emojikeyboard/ClipboardProbe")
        if result.returncode:
            raise RuntimeError(f"Could not read picker frame: {result.stderr.strip()}")
        frame = ast.literal_eval(result.stdout.strip())
        left, top, width, height = frame
        expected_x = x - width - 12 if flip else x + 12
        expected_y = y - height - 12 if flip else y + 12
        if width > 0 and height > 0 and abs(left - expected_x) <= 12 \
                and abs(top - expected_y) <= 12 and left >= 0 and top >= 0 \
                and left + width <= 800 and top + height <= 600:
            return
        time.sleep(0.1)
    pointer = bus_call("org.emojikeyboard.ClipboardProbe",
                       "org.emojikeyboard.ClipboardProbe.PointerLocation",
                       "/org/emojikeyboard/ClipboardProbe")
    raise RuntimeError(f"Picker did not open near ({x}, {y}); frame={frame}; "
                       f"pointer={pointer.stdout.strip()}")


def wait_for_history(test_root: Path, kind: str, minimum: int = 1) -> None:
    deadline = time.monotonic() + 5
    row = None
    databases = []
    while time.monotonic() < deadline:
        databases = list((test_root / "data").rglob("history.sqlite"))
        if databases:
            with sqlite3.connect(f"file:{databases[0]}?mode=ro", uri=True) as connection:
                row = connection.execute(
                    "SELECT count(*), max(length(preview)) FROM clipboard_entries WHERE kind = ?",
                    (kind,)).fetchone()
            if row[0] >= minimum and (kind != "image" or row[1]):
                return
        time.sleep(0.1)
    stats = bus_call("org.emojikeyboard.ClipboardProbe",
                     "org.emojikeyboard.ClipboardProbe.GetStats",
                     "/org/emojikeyboard/ClipboardProbe")
    status = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.CurrentStatus",
                      "/org/emojikeyboard/App")
    raise RuntimeError(f"Qt picker did not save {kind} clipboard history; "
                       f"database={databases}, row={row}, extension stats={stats.stdout.strip()}, "
                       f"picker status={status.stdout.strip() or status.stderr.strip()}")


def wait_for_saved_text(test_root: Path, expected: str) -> None:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        databases = list((test_root / "data").rglob("history.sqlite"))
        if databases:
            with sqlite3.connect(f"file:{databases[0]}?mode=ro", uri=True) as connection:
                found = connection.execute(
                    "SELECT count(*) FROM clipboard_entries WHERE kind = 'text' AND payload = ?",
                    (expected.encode(),)).fetchone()[0]
            if found:
                return
        time.sleep(0.1)
    raise RuntimeError(f"Clipboard text was not saved: {expected}")


def stop(process: subprocess.Popen, name: str) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)
        print(f"Forced {name} to stop")


def wait_for_ready(process: subprocess.Popen, log: Path, name: str) -> None:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"{name} exited with status {process.returncode}; "
                               f"log: {log.read_text(errors='replace')[-1000:].replace(chr(10), ' ')}")
        if "READY" in log.read_text(errors="replace"):
            return
        time.sleep(0.1)
    raise RuntimeError(f"{name} did not start")


def check_child(test_root: Path) -> int:
    shell_log = test_root / "shell.log"
    app_log = test_root / "app.log"
    target_log = test_root / "target.log"
    gtk_log = test_root / "gtk.log"
    terminal_log = test_root / "terminal.log"
    xwayland_log = test_root / "xwayland.log"
    result_path = test_root / "paste-result"
    shell = None
    app = None
    target = None
    gtk_target = None
    terminal_target = None
    xwayland_target = None
    with shell_log.open("w") as shell_output, app_log.open("w") as app_output, \
            target_log.open("w") as target_output, gtk_log.open("w") as gtk_output, \
            terminal_log.open("w") as terminal_output, \
            xwayland_log.open("w") as xwayland_output:
        try:
            shell_command = ["gnome-shell", "--wayland", "--headless", "--virtual-monitor",
                             "800x600"]
            shell = subprocess.Popen(shell_command, stdout=shell_output,
                                     stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                if shell.poll() is not None:
                    raise RuntimeError(f"GNOME Shell exited with status {shell.returncode}")
                result = call("gnome-extensions", "list", "--active")
                if UUID in result.stdout.splitlines():
                    break
                time.sleep(0.5)
            else:
                raise RuntimeError("GNOME Shell did not activate the extension")
            print("GNOME extension: active")

            stats = call("gdbus", "call", "--session", "--dest",
                         "org.emojikeyboard.ClipboardProbe", "--object-path",
                         "/org/emojikeyboard/ClipboardProbe", "--method",
                         "org.emojikeyboard.ClipboardProbe.GetStats")
            if stats.returncode:
                raise RuntimeError(f"Extension D-Bus bridge unavailable: {stats.stderr.strip()}")
            print(f"Extension clipboard counters: {stats.stdout.strip()}")

            app_env = os.environ.copy()
            app_env["WAYLAND_DISPLAY"] = "wayland-0"
            app_env["QT_QPA_PLATFORM"] = "wayland"
            app_env["QT_QPA_PLATFORMTHEME"] = "generic"
            app = subprocess.Popen([str(ROOT / "build/emoji-keyboard"), "--background"],
                                   env=app_env, stdout=app_output, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if app.poll() is not None:
                    raise RuntimeError(f"Qt app exited with status {app.returncode}")
                ping = call("gdbus", "call", "--session", "--dest", "org.emojikeyboard.App",
                            "--object-path", "/org/emojikeyboard/App", "--method",
                            "org.emojikeyboard.App.IsVisible")
                if ping.returncode == 0:
                    break
                time.sleep(0.25)
            else:
                raise RuntimeError("Qt app did not acquire its session D-Bus name")
            print("Qt picker: running on isolated Wayland display")

            set_pointer(80, 80)
            show = call("gdbus", "call", "--session", "--dest",
                        "org.emojikeyboard.ClipboardProbe", "--object-path",
                        "/org/emojikeyboard/ClipboardProbe", "--method",
                        "org.emojikeyboard.ClipboardProbe.ShowEmoji")
            if show.returncode:
                raise RuntimeError(f"Shell could not open the picker: {show.stderr.strip()}")
            visible = call("gdbus", "call", "--session", "--dest", "org.emojikeyboard.App",
                           "--object-path", "/org/emojikeyboard/App", "--method",
                           "org.emojikeyboard.App.IsVisible")
            if visible.returncode or "true" not in visible.stdout.lower():
                raise RuntimeError(f"Picker did not become visible: {visible.stdout.strip()}")
            wait_for_picker_near(80, 80, flip=False)
            print("Extension to Qt D-Bus activation: passed")

            hide = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.Hide",
                            "/org/emojikeyboard/App")
            if hide.returncode:
                raise RuntimeError(f"Could not reset picker: {hide.stderr.strip()}")
            wait_for_view("emoji", visible=False)
            set_pointer(780, 570)
            show = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.ShowClipboard",
                            "/org/emojikeyboard/App")
            if show.returncode:
                raise RuntimeError(f"Could not open picker at screen edge: {show.stderr.strip()}")
            wait_for_view("clipboard")
            wait_for_picker_near(780, 570, flip=True)
            hide = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.Hide",
                            "/org/emojikeyboard/App")
            if hide.returncode:
                raise RuntimeError(f"Could not reset picker: {hide.stderr.strip()}")
            wait_for_view("clipboard", visible=False)
            set_pointer(400, 300)
            print("GNOME Wayland: picker placed by pointer and flipped at screen edge")
            for view, expected_visible in (("emoji", True), ("clipboard", True),
                                           ("clipboard", False)):
                inject = bus_call("org.emojikeyboard.ClipboardProbe",
                                  "org.emojikeyboard.ClipboardProbe.InjectShortcut",
                                  "/org/emojikeyboard/ClipboardProbe", view)
                if inject.returncode:
                    raise RuntimeError(f"Could not inject {view} shortcut: "
                                       f"{inject.stderr.strip()}")
                try:
                    wait_for_view(view, visible=expected_visible)
                except RuntimeError as error:
                    raise RuntimeError(f"{error}; injection state: {inject.stdout.strip()}") from error
            print("GNOME shortcuts: Super+. opens emoji; Super+V switches and toggles")

            title = "Emoji Keyboard Qt Wayland test target"
            target = subprocess.Popen([str(ROOT / "build/gnome_test_target"),
                                       str(result_path), title], env=app_env,
                                      stdout=target_output, stderr=subprocess.STDOUT)
            wait_for_ready(target, target_log, "Qt Wayland target")
            focus_title(title)
            inject_key(46)  # C: copy synthetic prior text from the focused Qt window.
            inject_key(19)
            wait_for_file(Path(str(result_path) + ".clipboard"), "GNOME test prior text")
            wait_for_history(test_root, "text")
            inject_shortcut("emoji")
            wait_for_view("emoji")
            for key in (15, 15, 28):  # Search, categories, grid, then first emoji.
                inject_key(key)
                time.sleep(0.2)
            try:
                wait_for_file(result_path, "😀")
            except RuntimeError as error:
                focus = bus_call("org.emojikeyboard.ClipboardProbe",
                                 "org.emojikeyboard.ClipboardProbe.FocusedTitle",
                                 "/org/emojikeyboard/ClipboardProbe")
                status = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.CurrentStatus",
                                  "/org/emojikeyboard/App")
                raise RuntimeError(f"{error}; focus={focus.stdout.strip()}; "
                                   f"status={status.stdout.strip()}") from error
            wait_for_focus(title)
            time.sleep(1)
            inject_key(19)  # R: read clipboard after the temporary emoji is restored.
            wait_for_file(Path(str(result_path) + ".clipboard"), "GNOME test prior text")
            print("Qt Wayland: focus, emoji paste, and prior text restoration passed")

            inject_key(23)  # I: copy a synthetic image from the focused Qt window.
            wait_for_history(test_root, "image")
            inject_key(19)
            wait_for_file(Path(str(result_path) + ".clipboard"), "IMAGE 2x2")
            print("Qt Wayland: image captured with a stored thumbnail")

            result_path.unlink(missing_ok=True)
            Path(str(result_path) + ".clipboard").unlink(missing_ok=True)
            inject_shortcut("emoji")
            wait_for_view("emoji")
            time.sleep(0.35)
            inject_key(28)  # Grid focus remains on the previously used first emoji.
            wait_for_file(result_path, "😀")
            wait_for_focus(title)
            time.sleep(1)
            inject_key(19)
            wait_for_file(Path(str(result_path) + ".clipboard"), "IMAGE 2x2")
            print("Qt Wayland: prior image restoration passed")

            inject_shortcut("clipboard")
            wait_for_view("clipboard")
            print("Qt Wayland: Super+V opens clipboard history")

            result_path.unlink(missing_ok=True)
            time.sleep(0.35)
            inject_key(15)  # Search to Capture current.
            inject_key(15)  # Capture current to the first history card.
            time.sleep(0.2)
            inject_key(28)
            wait_for_file(result_path, "IMAGE 2x2")
            wait_for_focus(title)
            print("Qt Wayland: saved image history pasted into the target")

            preview_path = test_root / "clipboard-preview.png"
            preview = subprocess.run([str(ROOT / "build/emoji-keyboard"), "--clipboard",
                                      "--screenshot", str(preview_path)], env=app_env,
                                     text=True, capture_output=True, timeout=10,
                                     check=False)
            if preview.returncode or not preview_path.is_file():
                raise RuntimeError(f"Clipboard preview screenshot failed: {preview.stderr[-600:]}")
            png = preview_path.read_bytes()
            if not png.startswith(b"\x89PNG\r\n\x1a\n") \
                    or min(struct.unpack(">II", png[16:24])) < 300:
                raise RuntimeError("Clipboard preview screenshot is invalid or too small")
            if output_path := os.environ.get("EMOJI_TEST_SCREENSHOT_OUT"):
                shutil.copyfile(preview_path, output_path)
            print("Clipboard view: screenshot generated with image history present")

            gtk_title = "Emoji Keyboard GTK Wayland test target"
            gtk_result = test_root / "gtk-paste-result"
            gtk_env = app_env.copy()
            gtk_env["GDK_BACKEND"] = "wayland"
            gtk_target = subprocess.Popen(["gjs", "-m", str(ROOT / "tests/integration/gtk_test_target.js"),
                                           str(gtk_result), gtk_title,
                                           "org.emojikeyboard.TestGtk", "normal"],
                                          env=gtk_env, stdout=gtk_output,
                                          stderr=subprocess.STDOUT)
            wait_for_ready(gtk_target, gtk_log, "GTK Wayland target")
            focus_title(gtk_title)
            time.sleep(0.3)
            inject_key(46)
            try:
                wait_for_saved_text(test_root, "GNOME GTK prior text")
            except RuntimeError as error:
                stats = bus_call("org.emojikeyboard.ClipboardProbe",
                                 "org.emojikeyboard.ClipboardProbe.GetStats",
                                 "/org/emojikeyboard/ClipboardProbe")
                raise RuntimeError(f"{error}; extension stats={stats.stdout.strip()}; "
                                   f"GTK log={gtk_log.read_text(errors='replace')[-1000:].replace(chr(10), ' ')}") from error
            inject_shortcut("emoji")
            wait_for_view("emoji")
            time.sleep(0.35)
            inject_key(15)
            inject_key(15)
            time.sleep(0.2)
            inject_key(28)
            try:
                wait_for_file(gtk_result, "😀")
            except RuntimeError as error:
                status = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.CurrentStatus",
                                  "/org/emojikeyboard/App")
                visible = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.IsVisible",
                                   "/org/emojikeyboard/App")
                focus = bus_call("org.emojikeyboard.ClipboardProbe",
                                 "org.emojikeyboard.ClipboardProbe.FocusedTitle",
                                 "/org/emojikeyboard/ClipboardProbe")
                raise RuntimeError(f"{error}; picker status={status.stdout.strip()}, "
                                   f"visible={visible.stdout.strip()}, "
                                   f"focus={focus.stdout.strip()}") from error
            wait_for_focus(gtk_title)
            print("GTK Wayland: clipboard capture, shortcut, focus, and emoji paste passed")

            inject_shortcut("clipboard")
            wait_for_view("clipboard")
            print("GTK Wayland: Super+V opens clipboard history")

            stop(gtk_target, "GTK target")
            gtk_target = None
            terminal_title = "Emoji Keyboard terminal test target"
            terminal_result = test_root / "terminal-paste-result"
            terminal_target = subprocess.Popen(
                ["gjs", "-m", str(ROOT / "tests/integration/gtk_test_target.js"),
                 str(terminal_result), terminal_title, "org.gnome.Ptyxis", "terminal"],
                env=gtk_env, stdout=terminal_output, stderr=subprocess.STDOUT)
            wait_for_ready(terminal_target, terminal_log, "terminal target")
            focus_title(terminal_title)
            inject_shortcut("emoji")
            wait_for_view("emoji")
            time.sleep(0.3)
            inject_key(15)
            inject_key(15)
            time.sleep(0.2)
            inject_key(28)
            try:
                wait_for_file(terminal_result, "😀")
            except RuntimeError as error:
                status = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.CurrentStatus",
                                  "/org/emojikeyboard/App")
                visible = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.IsVisible",
                                   "/org/emojikeyboard/App")
                focus = bus_call("org.emojikeyboard.ClipboardProbe",
                                 "org.emojikeyboard.ClipboardProbe.FocusedTitle",
                                 "/org/emojikeyboard/ClipboardProbe")
                raise RuntimeError(f"{error}; picker status={status.stdout.strip()}, "
                                   f"visible={visible.stdout.strip()}, "
                                   f"focus={focus.stdout.strip()}") from error
            wait_for_focus(terminal_title)
            print("Terminal: Super+. restored focus and pasted with Ctrl+Shift+V")

            inject_shortcut("clipboard")
            wait_for_view("clipboard")
            print("Terminal: Super+V opens clipboard history")
            display = bus_call("org.emojikeyboard.ClipboardProbe",
                               "org.emojikeyboard.ClipboardProbe.XDisplay",
                               "/org/emojikeyboard/ClipboardProbe")
            if display.returncode:
                raise RuntimeError(f"Could not find XWayland display: {display.stderr.strip()}")
            xdisplay = ast.literal_eval(display.stdout.strip())[0]
            if not xdisplay:
                raise RuntimeError("GNOME Shell did not start XWayland")
            x_env = app_env.copy()
            x_env.pop("WAYLAND_DISPLAY", None)
            x_env["DISPLAY"] = xdisplay
            x_env["QT_QPA_PLATFORM"] = "xcb"
            auth_files = list((test_root / "runtime").glob(".mutter-Xwaylandauth.*"))
            if len(auth_files) != 1:
                raise RuntimeError("GNOME Shell did not create one XWayland auth file")
            x_env["XAUTHORITY"] = str(auth_files[0])
            x_title = "Emoji Keyboard Qt XWayland test target"
            x_result = test_root / "xwayland-paste-result"
            xwayland_target = subprocess.Popen([str(ROOT / "build/gnome_test_target"),
                                                str(x_result), x_title], env=x_env,
                                               stdout=xwayland_output,
                                               stderr=subprocess.STDOUT)
            try:
                wait_for_ready(xwayland_target, xwayland_log, "Qt XWayland target")
            except RuntimeError as error:
                runtime_files = [str(path.relative_to(test_root))
                                 for path in (test_root / "runtime").rglob("*") if path.is_file()]
                raise RuntimeError(f"{error}; runtime files={runtime_files}") from error
            focus_title(x_title)
            inject_key(46)
            wait_for_saved_text(test_root, "GNOME test XWayland text")
            inject_shortcut("emoji")
            wait_for_view("emoji")
            time.sleep(0.35)
            inject_key(15)
            inject_key(15)
            time.sleep(0.2)
            inject_key(28)
            wait_for_file(x_result, "😀")
            wait_for_focus(x_title)
            time.sleep(1)
            inject_key(19)
            wait_for_file(Path(str(x_result) + ".clipboard"), "GNOME test XWayland text")
            print("Qt XWayland: shortcut, focus, paste, and text restoration passed")

            inject_key(23)
            wait_for_history(test_root, "image", minimum=2)
            print("Qt XWayland: image captured with a stored thumbnail")
            inject_shortcut("clipboard")
            wait_for_view("clipboard")
            print("Qt XWayland: Super+V opens clipboard history")
            return 0
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            print(f"Headless GNOME check failed: {error}", file=sys.stderr)
            return 1
        finally:
            if xwayland_target:
                stop(xwayland_target, "XWayland target")
            if terminal_target:
                stop(terminal_target, "terminal target")
            if gtk_target:
                stop(gtk_target, "GTK target")
            if target:
                stop(target, "Qt target")
            if app:
                stop(app, "Qt app")
            if shell:
                stop(shell, "GNOME Shell")
            shell_output.flush()
            app_output.flush()
            gtk_output.flush()
            terminal_output.flush()
            xwayland_output.flush()
            for path in (shell_log, app_log, target_log, gtk_log, terminal_log,
                         xwayland_log):
                for line in path.read_text(errors="replace").splitlines():
                    if UUID in line or "Emoji Keyboard" in line or "keybinding" in line.lower():
                        print(f"{path.name}: {line[:240]}")


def check_restart_child(test_root: Path) -> int:
    """Open the same private XDG data in a new bus and compositor session."""
    shell_log = test_root / "restart-shell.log"
    app_log = test_root / "restart-app.log"
    shell = None
    app = None
    with shell_log.open("w") as shell_output, app_log.open("w") as app_output:
        try:
            database = next((test_root / "data").rglob("history.sqlite"))
            with sqlite3.connect(f"file:{database}?mode=ro", uri=True) as connection:
                before = connection.execute(
                    "SELECT kind, count(*) FROM clipboard_entries GROUP BY kind").fetchall()
                recents_before = connection.execute(
                    "SELECT count(*) FROM emoji_recents").fetchone()[0]
            if dict(before).get("text", 0) < 3 or dict(before).get("image", 0) < 2 \
                    or recents_before < 1:
                raise RuntimeError("The first session did not save the expected test history")

            shell = subprocess.Popen(["gnome-shell", "--wayland", "--headless",
                                      "--virtual-monitor", "800x600"], stdout=shell_output,
                                     stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                if shell.poll() is not None:
                    raise RuntimeError(f"Restarted GNOME Shell exited: {shell.returncode}")
                active = call("gnome-extensions", "list", "--active")
                if UUID in active.stdout.splitlines():
                    break
                time.sleep(0.5)
            else:
                raise RuntimeError("Extension did not reactivate in the new session")

            app_env = os.environ.copy()
            app_env.update({"WAYLAND_DISPLAY": "wayland-0", "QT_QPA_PLATFORM": "wayland",
                            "QT_QPA_PLATFORMTHEME": "generic"})
            app = subprocess.Popen([str(ROOT / "build/emoji-keyboard"), "--background"],
                                   env=app_env, stdout=app_output, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if app.poll() is not None:
                    raise RuntimeError(f"Restarted Qt picker exited: {app.returncode}")
                ready = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.IsVisible",
                                 "/org/emojikeyboard/App")
                if ready.returncode == 0:
                    break
                time.sleep(0.25)
            else:
                raise RuntimeError("Qt picker did not restart on the new session bus")

            show = bus_call("org.emojikeyboard.App", "org.emojikeyboard.App.ShowClipboard",
                            "/org/emojikeyboard/App")
            if show.returncode:
                raise RuntimeError(f"Restarted picker could not open history: {show.stderr.strip()}")
            wait_for_view("clipboard")
            with sqlite3.connect(f"file:{database}?mode=ro", uri=True) as connection:
                after = connection.execute(
                    "SELECT kind, count(*) FROM clipboard_entries GROUP BY kind").fetchall()
                recents_after = connection.execute(
                    "SELECT count(*) FROM emoji_recents").fetchone()[0]
            if after != before or recents_after != recents_before:
                raise RuntimeError("Clipboard history or emoji recents changed across session restart")
            print("Session restart: GNOME extension, history, and emoji recents retained")
            return 0
        except (OSError, RuntimeError, StopIteration, subprocess.TimeoutExpired) as error:
            print(f"Session restart check failed: {error}", file=sys.stderr)
            return 1
        finally:
            if app:
                stop(app, "restarted Qt app")
            if shell:
                stop(shell, "restarted GNOME Shell")


def main() -> int:
    if len(sys.argv) == 3 and sys.argv[1] == "--child":
        return check_child(Path(sys.argv[2]))
    if len(sys.argv) == 3 and sys.argv[1] == "--restart-child":
        return check_restart_child(Path(sys.argv[2]))
    if not all((ROOT / "build" / name).is_file()
               for name in ("emoji-keyboard", "gnome_test_target")):
        print("Build the application and gnome_test_target in build/ first", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory(prefix="emoji-gnome-headless-") as directory:
        test_root = Path(directory)
        runtime = test_root / "runtime"
        runtime.mkdir(mode=0o700)
        extension = test_root / "data/gnome-shell/extensions" / UUID
        shutil.copytree(ROOT / "integration/gnome-extension", extension)
        install_test_input(extension)
        compiled = call("glib-compile-schemas", "--strict", str(extension / "schemas"))
        if compiled.returncode:
            print(compiled.stderr.strip(), file=sys.stderr)
            return 1
        env = os.environ.copy()
        for key in ("WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS",
                    "GSETTINGS_SCHEMA_DIR", "GSETTINGS_SCHEMA_PATH", "GI_TYPELIB_PATH",
                    "LD_LIBRARY_PATH", "GTK_PATH", "GTK_DATA_PREFIX", "GTK_EXE_PREFIX",
                    "GTK_IM_MODULE_FILE", "SNAP_LIBRARY_PATH"):
            env.pop(key, None)
        for key in tuple(env):
            if key.startswith("SNAP"):
                env.pop(key)
        env.update({"HOME": str(test_root), "XDG_CONFIG_HOME": str(test_root / "config"),
                    "XDG_DATA_HOME": str(test_root / "data"), "XDG_RUNTIME_DIR": str(runtime),
                    "XDG_DATA_DIRS": "/usr/local/share:/usr/share",
                    "XDG_CONFIG_DIRS": "/etc/xdg", "GSETTINGS_BACKEND": "keyfile",
                    "XDG_CURRENT_DESKTOP": "GNOME", "PATH": "/usr/local/bin:/usr/bin:/bin"})
        for schema, key, value in (
            ("org.gnome.shell", "enabled-extensions", f"['{UUID}']"),
            ("org.gnome.shell.keybindings", "toggle-message-tray", "['<Super>m']"),
            ("org.freedesktop.ibus.panel.emoji", "hotkey", "['<Super>semicolon']"),
        ):
            result = subprocess.run(["gsettings", "set", schema, key, value], env=env,
                                    text=True, capture_output=True, check=False)
            if result.returncode:
                print(f"Could not configure isolated GSettings: {result.stderr.strip()}",
                      file=sys.stderr)
                return 1
        for key, value in (("show-emoji", "['<Super>period']"),
                           ("show-clipboard", "['<Super>v']")):
            result = subprocess.run(["gsettings", "--schemadir", str(extension / "schemas"),
                                     "set", "org.gnome.shell.extensions.emoji-keyboard", key,
                                     value], env=env, text=True, capture_output=True, check=False)
            if result.returncode:
                print(f"Could not enable isolated shortcut: {result.stderr.strip()}",
                      file=sys.stderr)
                return 1
        for mode, timeout in (("--child", 40), ("--restart-child", 25)):
            command = ["dbus-run-session", "--", sys.executable,
                       str(Path(__file__).resolve()), mode, str(test_root)]
            process = subprocess.Popen(command, env=env, text=True, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            try:
                output, _ = process.communicate(timeout=timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                output, _ = process.communicate()
                print(output[-3000:])
                print("Headless GNOME check timed out", file=sys.stderr)
                return 1
            for line in output.splitlines():
                if line.startswith(("Headless GNOME check failed:",
                                    "Session restart check failed:", "GNOME extension:",
                                    "Extension clipboard counters:", "Qt picker:",
                                    "Extension to Qt D-Bus activation:", "GNOME shortcuts:",
                                    "Qt Wayland:", "GTK Wayland:", "Terminal:",
                                    "Qt XWayland:", "Clipboard view:",
                                    "Session restart:")):
                    print(line)
            if process.returncode:
                return process.returncode
        return 0


if __name__ == "__main__":
    sys.exit(main())
