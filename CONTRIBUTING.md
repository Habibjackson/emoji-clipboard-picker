# Contributing

Thanks for helping build Emoji Keyboard. The first Qt 6.11/QML version supports emoji browsing and local clipboard history; planned desktop features and their order are in [TODO.md](TODO.md). Check the TODO and existing issues before starting a change. For a new storage format, desktop integration approach, or shortcut behavior, open an issue to discuss the design first.

## Set up and verify

Use Qt 6.11, CMake, Ninja, and a C++20 compiler. Adjust the Qt path for your installation:

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target emoji-keyboard_qmllint
./build/emoji-keyboard
```

The commands build the app, run its history and clipboard controller tests, lint QML, and open the picker. Keep generated files in `build/` and out of commits.

## Make a focused change

Follow [AGENTS.md](AGENTS.md) for directory layout and naming. Keep QML responsible for presentation and interaction; put models, persistence, and desktop integration in C++. Add a focused test for new storage or model behavior. For GNOME Wayland or X11 changes, record the desktop session and applications used to verify copy, shortcut, focus, and paste behavior.

Clipboard contents are private. Use synthetic test data, never attach real clipboard databases or screenshots containing sensitive content, and do not log payloads. Record the source, version, and redistribution license for any imported emoji data, artwork, or other third-party asset. The repository's MIT license does not replace those licenses.

## Submit a pull request

Use a short imperative commit subject, such as `Add clipboard history model`. In the pull request, explain the behavior, link an issue or TODO item, list build/lint/test results, and note which desktop sessions were tested. Include before-and-after screenshots for visible QML changes. Call out changes to shortcuts, clipboard persistence, data limits, or permissions. Contributions to original project code are submitted under the repository's [MIT license](LICENSE).
