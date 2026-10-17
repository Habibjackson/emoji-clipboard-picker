# Repository Guidelines

## Project Structure & Module Organization

`src/main.cpp` starts the Qt application; `qml/Main.qml` is the current window. Put application controllers in `src/app/`, list models in `src/models/`, SQLite code in `src/storage/`, and desktop backends in `src/platform/gnome/` or `src/platform/x11/`. Put reusable QML controls in `qml/components/` and screens in `qml/views/`. Emoji data and icons belong in `resources/`; the GNOME extension, desktop entry, D-Bus, and systemd files belong in `integration/`. Use `tests/unit/` for model and storage tests and `tests/integration/` for desktop behavior. Follow the implementation order in `TODO.md`.

## Build, Test, and Development Commands

Use Qt 6.11, CMake, Ninja, and a C++20 compiler:

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target emoji-keyboard_qmllint
./build/emoji-keyboard
```

These commands configure, compile, run tests, lint QML, and open the picker. Keep generated files under `build/`.

## Coding Style & Naming Conventions

Use four spaces and follow the existing brace style in C++. Name C++ types and QML components with `UpperCamelCase`; use `lowerCamelCase` for properties, methods, and QML IDs. Keep UI layout and interaction in QML, with persistence and desktop integration in C++. Register QML files through `qt_add_qml_module`; avoid relative imports and storage logic in QML JavaScript. Run the QML lint target for each QML change.

## Testing Guidelines

Qt Test covers storage and clipboard controller behavior; a GJS test checks GNOME clipboard MIME classification. No coverage threshold is set. Add focused Qt Test cases for C++ behavior and Qt Quick Test cases for QML behavior as those modules arrive. Name test files `tst_<feature>.cpp` or `tst_<Feature>.qml`. Verify clipboard limits, pinning, deduplication, and restart persistence; exercise GNOME Wayland and X11 flows in their respective sessions.

## Commit & Pull Request Guidelines

Use short, imperative commit subjects such as `Add clipboard history model`. In pull requests, describe the user-facing change, link the relevant TODO item or issue, list build and test results, and include screenshots for QML changes. State which desktop sessions were tested and note any shortcut or clipboard behavior changes.

## Privacy & Local Data

Clipboard content is sensitive. Never commit captured data, databases, or payload-bearing logs. Store history only for the current user, enforce user-only file permissions, and avoid logging clipboard contents. Keep the five pinned and twenty unpinned item limits in storage transactions.
