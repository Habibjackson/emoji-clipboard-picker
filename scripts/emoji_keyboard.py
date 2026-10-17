#!/usr/bin/env python3
"""Install, inspect, and remove the user-level desktop integration."""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
UUID = "emoji-keyboard@emojikeyboard.local"
GNOME_SCHEMA = "org.gnome.shell.extensions.emoji-keyboard"
SHORTCUTS = {
    "ibus": ("org.freedesktop.ibus.panel.emoji", "hotkey", "<Super>period"),
    "tray": ("org.gnome.shell.keybindings", "toggle-message-tray", "<Super>v"),
    "emoji": (GNOME_SCHEMA, "show-emoji", "<Super>period"),
    "clipboard": (GNOME_SCHEMA, "show-clipboard", "<Super>v"),
}


class SetupError(Exception):
    pass


def xdg_dir(variable: str, fallback: str) -> Path:
    value = Path(os.environ.get(variable, "")) if os.environ.get(variable) else None
    return value if value is not None and value.is_absolute() else Path.home() / fallback


def locations() -> dict[str, Path]:
    data = xdg_dir("XDG_DATA_HOME", ".local/share")
    config = xdg_dir("XDG_CONFIG_HOME", ".config")
    return {
        "bin": Path.home() / ".local/bin/emoji-keyboard",
        "desktop": data / "applications/org.emojikeyboard.App.desktop",
        "icon": data / "icons/hicolor/scalable/apps/org.emojikeyboard.App.svg",
        "dbus": data / "dbus-1/services/org.emojikeyboard.App.service",
        "extension": data / f"gnome-shell/extensions/{UUID}",
        "unit": config / "systemd/user/emoji-keyboard.service",
        "state": config / "emoji-keyboard/install-state.json",
    }


def is_gnome_session() -> bool:
    desktops = os.environ.get("XDG_CURRENT_DESKTOP", "").casefold().split(":")
    return "gnome" in desktops


def run(*args: str, check: bool = True, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(args, text=True, capture_output=True, env=env, check=False)
    if check and result.returncode:
        detail = result.stderr.strip() or result.stdout.strip() or f"exit {result.returncode}"
        raise SetupError(f"{' '.join(args[:2])}: {detail}")
    return result


def private_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)
    path.chmod(0o700)


def atomic_write(path: Path, data: bytes, mode: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".emoji-keyboard-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            os.fchmod(stream.fileno(), mode)
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_state(paths: dict[str, Path]) -> dict:
    try:
        state = json.loads(paths["state"].read_text(encoding="utf-8"))
    except FileNotFoundError:
        return {"version": 1, "files": {}, "shortcuts": {}}
    except (ValueError, OSError) as error:
        raise SetupError(f"Cannot read installer state: {error}") from error
    if state.get("version") != 1 or not isinstance(state.get("files"), dict) or not isinstance(state.get("shortcuts"), dict):
        raise SetupError("Unsupported installer state; refusing to replace existing files")
    return state


def save_state(paths: dict[str, Path], state: dict) -> None:
    private_dir(paths["state"].parent)
    atomic_write(paths["state"], (json.dumps(state, indent=2, sort_keys=True) + "\n").encode(), 0o600)


def install_file(paths: dict[str, Path], state: dict, destination: Path, data: bytes, mode: int) -> None:
    key = str(destination)
    old_hash = state["files"].get(key)
    if destination.exists() or destination.is_symlink():
        if destination.is_symlink() or not destination.is_file():
            raise SetupError(f"Refusing to replace non-file: {destination}")
        current_hash = digest(destination.read_bytes())
        if current_hash != old_hash and current_hash != digest(data):
            raise SetupError(f"Refusing to replace a modified or unmanaged file: {destination}")
    if not destination.exists() or digest(destination.read_bytes()) != digest(data):
        atomic_write(destination, data, mode)
    else:
        destination.chmod(mode)
    state["files"][key] = digest(data)
    save_state(paths, state)


def path_argument(path: Path) -> str:
    if "\n" in str(path) or "\r" in str(path):
        raise SetupError("Install path contains a newline")
    return shlex.quote(str(path))


def service_unit(binary: Path) -> bytes:
    # systemd's quoted words use backslash escaping; percent signs are specifiers.
    value = str(binary).replace("\\", "\\\\").replace('"', '\\"').replace("%", "%%")
    template = (ROOT / "integration/systemd/emoji-keyboard.service.in").read_text()
    return template.replace("@EMOJI_KEYBOARD_EXECUTABLE@", f'"{value}"').encode()


def dbus_service(binary: Path) -> bytes:
    template = (ROOT / "integration/dbus/org.emojikeyboard.App.service.in").read_text()
    return template.replace("@EMOJI_KEYBOARD_EXECUTABLE@", path_argument(binary)).encode()


def gsettings_get(paths: dict[str, Path], name: str) -> list[str]:
    schema, key, _ = SHORTCUTS[name]
    prefix = ["gsettings"]
    if schema == GNOME_SCHEMA:
        prefix += ["--schemadir", str(paths["extension"] / "schemas")]
    output = run(*prefix, "get", schema, key).stdout.strip()
    return parse_string_array(output, f"{schema} {key}")


def parse_string_array(output: str, label: str) -> list[str]:
    if output.startswith("@as "):
        output = output[4:]
    try:
        values = ast.literal_eval(output)
    except (SyntaxError, ValueError) as error:
        raise SetupError(f"Could not parse {label}: {output}") from error
    if not isinstance(values, list) or not all(isinstance(item, str) for item in values):
        raise SetupError(f"Expected a string array for {label}")
    return values


def gsettings_set(paths: dict[str, Path], name: str, values: list[str]) -> None:
    schema, key, _ = SHORTCUTS[name]
    prefix = ["gsettings"]
    if schema == GNOME_SCHEMA:
        prefix += ["--schemadir", str(paths["extension"] / "schemas")]
    run(*prefix, "set", schema, key, json.dumps(values, ensure_ascii=False))


def matches(value: str, target: str) -> bool:
    return value.casefold().replace("<mod4>", "<super>") == target.casefold()


def change_shortcut(paths: dict[str, Path], state: dict, name: str, replacement: list[str]) -> bool:
    current = gsettings_get(paths, name)
    saved = state["shortcuts"].get(name)
    if saved and current != saved["applied"]:
        print(f"{name}: changed since setup; keeping your current value", file=sys.stderr)
        return False
    if current == replacement:
        return True
    prior = None if saved is None else saved.copy()
    if not saved:
        state["shortcuts"][name] = {"original": current, "applied": replacement}
    else:
        saved["applied"] = replacement
    save_state(paths, state)  # Record the backup before touching desktop settings.
    try:
        gsettings_set(paths, name, replacement)
    except (FileNotFoundError, SetupError):
        # A failed write should remain retryable if it did not alter GSettings.
        try:
            unchanged = gsettings_get(paths, name) == current
        except (FileNotFoundError, SetupError):
            unchanged = False
        if unchanged:
            if prior is None:
                del state["shortcuts"][name]
            else:
                state["shortcuts"][name] = prior
            save_state(paths, state)
        raise
    return True


def setup_shortcuts(paths: dict[str, Path], state: dict, yes: bool = False) -> None:
    if not is_gnome_session():
        print("GNOME shortcut setup skipped outside a GNOME session; X11 uses native key grabs.")
        return
    if not (paths["extension"] / "schemas/gschemas.compiled").exists():
        raise SetupError("Install the GNOME extension before configuring its shortcuts")
    current = {name: gsettings_get(paths, name) for name in ("ibus", "tray")}
    changes = {
        name: [item for item in values if not matches(item, SHORTCUTS[name][2])]
        for name, values in current.items()
    }
    conflicts = [name for name in current if changes[name] != current[name]]
    if conflicts:
        print("GNOME currently uses Super+. for IBus emoji input and Super+V for the message tray.")
        for name in conflicts:
            print(f"  {name}: {current[name]} -> {changes[name]}")
        print("Only those combinations will be removed; other shortcuts stay in place.")
        if not yes:
            if not sys.stdin.isatty():
                print("Shortcuts unchanged. Run 'scripts/emoji_keyboard.py setup-shortcuts' interactively.")
                return
            if input("Apply both shortcut changes and enable picker shortcuts? [y/N] ").strip().lower() not in ("y", "yes"):
                print("Shortcuts unchanged.")
                return
    for name in ("ibus", "tray"):
        if name in conflicts and not change_shortcut(paths, state, name, changes[name]):
            return
    # A failed or user-edited desktop setting must never leave the extension claiming the key.
    if any(any(matches(value, SHORTCUTS[name][2]) for value in gsettings_get(paths, name))
           for name in ("ibus", "tray")):
        print("Known GNOME conflicts remain; picker shortcuts were not enabled.", file=sys.stderr)
        return
    registered = all(change_shortcut(paths, state, name, [SHORTCUTS[name][2]])
                     for name in ("emoji", "clipboard"))
    if registered:
        print("Picker shortcuts configured. Run 'scripts/emoji_keyboard.py recheck' after GNOME login.")
    else:
        print("A picker shortcut was changed after setup; recheck its current value.", file=sys.stderr)


def data_paths(binary: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    try:
        values = json.loads(run(str(binary), "--print-data-paths", env=env).stdout)
    except (ValueError, OSError) as error:
        raise SetupError(f"Cannot determine Qt data paths: {error}") from error
    if not isinstance(values.get("historyDirectory"), str) or not isinstance(values.get("settingsFile"), str):
        raise SetupError("Qt did not return usable data paths")
    return values


def optional_command(*args: str) -> bool:
    try:
        result = run(*args, check=False)
    except FileNotFoundError:
        result = None
    if result is None or result.returncode:
        detail = "command unavailable" if result is None else result.stderr.strip() or result.stdout.strip() or f"exit {result.returncode}"
        print(f"Could not run {' '.join(args)}: {detail}", file=sys.stderr)
        return False
    return True


def ensure_extension_enabled() -> None:
    schema, key = "org.gnome.shell", "enabled-extensions"
    current = parse_string_array(run("gsettings", "get", schema, key).stdout.strip(),
                                 f"{schema} {key}")
    if UUID not in current:
        run("gsettings", "set", schema, key, json.dumps(current + [UUID]))
        print("GNOME extension queued for the next login; other extensions are unchanged.")


def remove_enabled_extension() -> None:
    schema, key = "org.gnome.shell", "enabled-extensions"
    current = parse_string_array(run("gsettings", "get", schema, key).stdout.strip(),
                                 f"{schema} {key}")
    if UUID in current:
        run("gsettings", "set", schema, key,
            json.dumps([extension for extension in current if extension != UUID]))


def install(paths: dict[str, Path], state: dict, binary: Path, no_shortcuts: bool) -> None:
    if not binary.is_file():
        raise SetupError(f"Build the application first: {binary}")
    binary = binary.resolve()
    app_paths = data_paths(binary)
    state["historyDirectory"] = app_paths["historyDirectory"]
    state["settingsFile"] = app_paths["settingsFile"]
    save_state(paths, state)
    install_file(paths, state, paths["bin"], binary.read_bytes(), 0o755)
    install_file(paths, state, paths["desktop"],
                 (ROOT / "integration/desktop/org.emojikeyboard.App.desktop").read_bytes(), 0o644)
    install_file(paths, state, paths["icon"],
                 (ROOT / "resources/icons/org.emojikeyboard.App.svg").read_bytes(), 0o644)
    install_file(paths, state, paths["dbus"], dbus_service(paths["bin"]), 0o644)
    install_file(paths, state, paths["unit"], service_unit(paths["bin"]), 0o644)
    source = ROOT / "integration/gnome-extension"
    for name in ("metadata.json", "extension.js", "clipboardProbe.js", "pasteShortcut.js"):
        install_file(paths, state, paths["extension"] / name, (source / name).read_bytes(), 0o644)
    schema = "org.gnome.shell.extensions.emoji-keyboard.gschema.xml"
    install_file(paths, state, paths["extension"] / "schemas" / schema,
                 (source / "schemas" / schema).read_bytes(), 0o644)
    with tempfile.TemporaryDirectory(prefix="emoji-keyboard-schema-") as temporary:
        compiled = Path(temporary) / "gschemas.compiled"
        run("glib-compile-schemas", "--strict", f"--targetdir={temporary}", str(source / "schemas"))
        install_file(paths, state, paths["extension"] / "schemas/gschemas.compiled",
                     compiled.read_bytes(), 0o644)
    optional_command("systemctl", "--user", "daemon-reload")
    optional_command("systemctl", "--user", "enable", "--now", "emoji-keyboard.service")
    optional_command("systemctl", "--user", "try-restart", "emoji-keyboard.service")
    if is_gnome_session():
        optional_command("gnome-extensions", "enable", UUID)
        ensure_extension_enabled()
    print("Installed for this user. On GNOME Wayland, log out and log in so Shell discovers a new extension.")
    if not no_shortcuts:
        setup_shortcuts(paths, state)


def restore_shortcuts(paths: dict[str, Path], state: dict) -> None:
    for name in ("emoji", "clipboard", "ibus", "tray"):
        saved = state["shortcuts"].get(name)
        if not saved:
            continue
        try:
            current = gsettings_get(paths, name)
            if current == saved["applied"]:
                gsettings_set(paths, name, saved["original"])
                print(f"Restored {name} shortcuts")
            else:
                print(f"Kept user-modified {name} shortcuts")
            del state["shortcuts"][name]
            save_state(paths, state)
        except (FileNotFoundError, SetupError) as error:
            print(f"Could not restore {name}: {error}", file=sys.stderr)


def purge_history(state: dict) -> None:
    directory_value = state.get("historyDirectory")
    if not directory_value or not Path(directory_value).is_absolute():
        raise SetupError("History path is unknown; run the installed binary with --print-data-paths")
    directory = Path(directory_value)
    for suffix in ("", "-wal", "-shm", "-journal"):
        path = directory / f"history.sqlite{suffix}"
        if path.is_file() and not path.is_symlink():
            path.unlink()
    try:
        directory.rmdir()
    except OSError:
        pass
    print(f"Purged history database from {directory}")


def uninstall(paths: dict[str, Path], state: dict, purge: bool) -> None:
    optional_command("gnome-extensions", "disable", UUID)
    try:
        remove_enabled_extension()
    except (FileNotFoundError, SetupError) as error:
        print(f"Could not update GNOME's enabled extension list: {error}", file=sys.stderr)
    optional_command("systemctl", "--user", "disable", "--now", "emoji-keyboard.service")
    restore_shortcuts(paths, state)
    for value, expected in list(state["files"].items()):
        path = Path(value)
        if path.is_file() and not path.is_symlink() and digest(path.read_bytes()) == expected:
            path.unlink()
            del state["files"][value]
        elif not path.exists() and not path.is_symlink():
            del state["files"][value]
        else:
            print(f"Kept modified installed file: {path}", file=sys.stderr)
        save_state(paths, state)
    for directory in (paths["extension"] / "schemas", paths["extension"]):
        try:
            directory.rmdir()
        except OSError:
            pass
    optional_command("systemctl", "--user", "daemon-reload")
    if purge:
        purge_history(state)
    else:
        print(f"Kept history database in {state.get('historyDirectory', 'Qt AppDataLocation')}")
        print("Run uninstall --purge-history later to delete it explicitly.")
    save_state(paths, state)


def status(paths: dict[str, Path], recheck: bool) -> int:
    problems = []
    x11 = os.environ.get("XDG_SESSION_TYPE") == "x11"
    gnome = is_gnome_session()
    for name in ("bin", "desktop", "dbus", "unit"):
        present = paths[name].is_file()
        print(f"{name}: {'installed' if present else 'missing'}")
        if not present:
            problems.append(name)
    extension = (paths["extension"] / "schemas/gschemas.compiled").is_file()
    print(f"GNOME extension files: {'installed' if extension else 'missing'}")
    if x11:
        print("X11: native backend checks shortcut grabs when the picker starts; failures appear in the picker.")
    elif gnome:
        try:
            result = run("gnome-extensions", "list", "--active", check=False)
            active_extension = result.returncode == 0 and UUID in result.stdout.splitlines()
        except FileNotFoundError:
            active_extension = False
        print(f"GNOME extension: {'active' if active_extension else 'not active'}")
        if not active_extension:
            problems.append("GNOME extension (a new installation may need logout/login)")
    else:
        problems.append("unsupported Wayland compositor")
    active = run("systemctl", "--user", "is-active", "emoji-keyboard.service", check=False)
    print(f"User service: {active.stdout.strip() or 'unavailable'}")
    if active.stdout.strip() != "active":
        problems.append("user service")
    enabled_service = run("systemctl", "--user", "is-enabled", "emoji-keyboard.service", check=False)
    print(f"Start at login: {enabled_service.stdout.strip() or 'unavailable'}")
    if enabled_service.stdout.strip() != "enabled":
        problems.append("service startup")
    names = ["ibus", "tray"] if gnome else []
    if extension and gnome and not x11:
        names += ["emoji", "clipboard"]
    for name in names:
        values = gsettings_get(paths, name)
        print(f"{name} shortcuts: {values}")
        found = any(matches(value, SHORTCUTS[name][2]) for value in values)
        if (name in ("ibus", "tray") and found) or (name in ("emoji", "clipboard") and not found):
            problems.append(f"{name} shortcut")
    if problems:
        print("Needs attention: " + ", ".join(problems))
        if recheck:
            print("Run setup-shortcuts for known GNOME conflicts; inspect other extensions if keys remain unavailable.")
    else:
        print("Known setup checks passed; press each shortcut in an application to verify registration.")
    return 1 if recheck and problems else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    install_parser = commands.add_parser("install", help="Install from the existing build directory")
    install_parser.add_argument("--binary", type=Path, default=ROOT / "build/emoji-keyboard")
    install_parser.add_argument("--no-shortcuts", action="store_true", help="Skip guided shortcut setup")
    setup_parser = commands.add_parser("setup-shortcuts", help="Back up and configure GNOME shortcuts")
    setup_parser.add_argument("--yes", action="store_true", help="Apply the displayed shortcut changes without a prompt")
    commands.add_parser("status", help="Show installation and shortcut state")
    commands.add_parser("recheck", help="Check known conflicts and return failure if setup is incomplete")
    uninstall_parser = commands.add_parser("uninstall", help="Remove installed files and restore eligible shortcuts")
    uninstall_parser.add_argument("--purge-history", action="store_true", help="Also delete the local history database")
    args = parser.parse_args()
    paths = locations()
    try:
        if args.command == "status" or args.command == "recheck":
            return status(paths, args.command == "recheck")
        state = load_state(paths)
        if args.command == "install":
            install(paths, state, args.binary, args.no_shortcuts)
        elif args.command == "setup-shortcuts":
            setup_shortcuts(paths, state, args.yes)
        elif args.command == "uninstall":
            uninstall(paths, state, args.purge_history)
    except (FileNotFoundError, OSError, SetupError) as error:
        print(f"Setup failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
