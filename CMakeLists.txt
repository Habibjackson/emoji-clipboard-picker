cmake_minimum_required(VERSION 3.24)

project(EmojiKeyboard VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml DBus Quick QuickControls2 Sql)
qt_standard_project_setup(REQUIRES 6.11)

qt_add_executable(emoji-keyboard
    src/main.cpp
    src/platform/DesktopIntegration.h src/platform/DesktopIntegration.cpp
    src/platform/gnome/GnomeBackend.h src/platform/gnome/GnomeBackend.cpp
    src/platform/gnome/PickerService.h src/platform/gnome/PickerService.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
foreach(qml_file IN ITEMS
    qml/Main.qml
    qml/Theme.qml
    qml/components/ActionButton.qml
    qml/components/CategoryButton.qml
    qml/components/SearchField.qml
    qml/components/EmojiTile.qml
    qml/components/ClipboardCard.qml
    qml/views/EmojiView.qml
    qml/views/ClipboardView.qml
)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()

set_source_files_properties(resources/emoji/emoji-test.txt PROPERTIES
    QT_RESOURCE_ALIAS emoji-test.txt
)
set_source_files_properties(resources/emoji/cldr-en-48.2.tsv PROPERTIES
    QT_RESOURCE_ALIAS cldr-en-48.2.tsv
)
set_source_files_properties(resources/icons/org.emojikeyboard.App.svg PROPERTIES
    QT_RESOURCE_ALIAS org.emojikeyboard.App.svg
)

qt_add_qml_module(emoji-keyboard
    URI EmojiKeyboard
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/ActionButton.qml
        qml/components/CategoryButton.qml
        qml/components/SearchField.qml
        qml/components/EmojiTile.qml
        qml/components/ClipboardCard.qml
        qml/views/EmojiView.qml
        qml/views/ClipboardView.qml
    SOURCES
        src/app/AppController.h src/app/AppController.cpp
        src/app/ClipboardImageProvider.h src/app/ClipboardImageProvider.cpp
        src/models/EmojiModel.h src/models/EmojiModel.cpp
        src/models/ClipboardModel.h src/models/ClipboardModel.cpp
        src/storage/HistoryStore.h src/storage/HistoryStore.cpp
)

qt_add_resources(emoji-keyboard emoji_data
    PREFIX /data
    FILES resources/emoji/emoji-test.txt resources/emoji/cldr-en-48.2.tsv
)
qt_add_resources(emoji-keyboard app_icon
    PREFIX /icons
    FILES resources/icons/org.emojikeyboard.App.svg
)

target_include_directories(emoji-keyboard PRIVATE src src/app src/models)
target_link_libraries(emoji-keyboard PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::DBus
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
)

option(EMOJI_ENABLE_X11 "Build the X11 backend when XFixes and XTest are available" ON)
if(EMOJI_ENABLE_X11 AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 QUIET COMPONENTS Xfixes Xtst Xi Xext)
    if(X11_FOUND AND TARGET X11::Xfixes AND TARGET X11::Xtst
        AND TARGET X11::Xi AND TARGET X11::Xext)
        set(EMOJI_X11_AVAILABLE TRUE)
        target_sources(emoji-keyboard PRIVATE
            src/platform/x11/X11Backend.h src/platform/x11/X11Backend.cpp)
        target_compile_definitions(emoji-keyboard PRIVATE EMOJI_HAS_X11)
        target_link_libraries(emoji-keyboard PRIVATE X11::X11 X11::Xfixes X11::Xtst)
    else()
        message(STATUS "X11 backend disabled: XFixes/XTest development files not found")
    endif()
endif()

set(EMOJI_KEYBOARD_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/emoji-keyboard")
configure_file(integration/dbus/org.emojikeyboard.App.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.emojikeyboard.App.service" @ONLY)

install(TARGETS emoji-keyboard
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES integration/desktop/org.emojikeyboard.App.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES resources/icons/org.emojikeyboard.App.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME setup_installer COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/tst_setup.py")
    endif()
    qt_add_executable(tst_history_store
        tests/unit/tst_history_store.cpp
        src/storage/HistoryStore.cpp
    )
    target_include_directories(tst_history_store PRIVATE src)
    target_link_libraries(tst_history_store PRIVATE Qt6::Core Qt6::Gui Qt6::Sql Qt6::Test)
    add_test(NAME history_store COMMAND tst_history_store)

    qt_add_executable(tst_emoji_model
        tests/unit/tst_emoji_model.cpp
        src/models/EmojiModel.cpp
    )
    qt_add_resources(tst_emoji_model emoji_model_test_data
        PREFIX /data
        FILES resources/emoji/emoji-test.txt resources/emoji/cldr-en-48.2.tsv
    )
    target_include_directories(tst_emoji_model PRIVATE src)
    target_link_libraries(tst_emoji_model PRIVATE Qt6::Gui Qt6::Qml Qt6::Test)
    add_test(NAME emoji_model COMMAND tst_emoji_model)
    set_tests_properties(emoji_model PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(tst_app_controller
        tests/integration/tst_app_controller.cpp
        src/app/AppController.cpp
        src/models/EmojiModel.cpp
        src/models/ClipboardModel.cpp
        src/storage/HistoryStore.cpp
    )
    qt_add_resources(tst_app_controller test_emoji_data
        PREFIX /data
        FILES resources/emoji/emoji-test.txt resources/emoji/cldr-en-48.2.tsv
    )
    target_include_directories(tst_app_controller PRIVATE src)
    target_link_libraries(tst_app_controller PRIVATE Qt6::Quick Qt6::Sql Qt6::Test)
    add_test(NAME app_controller COMMAND tst_app_controller)
    set_tests_properties(app_controller PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

    find_program(DBUS_RUN_SESSION_EXECUTABLE NAMES dbus-run-session)
    if(DBUS_RUN_SESSION_EXECUTABLE)
        qt_add_executable(tst_desktop_integration
            tests/integration/tst_desktop_integration.cpp
            src/platform/DesktopIntegration.h src/platform/DesktopIntegration.cpp
            src/platform/gnome/GnomeBackend.h src/platform/gnome/GnomeBackend.cpp
            src/platform/gnome/PickerService.h src/platform/gnome/PickerService.cpp
            src/app/AppController.cpp
            src/models/EmojiModel.cpp
            src/models/ClipboardModel.cpp
            src/storage/HistoryStore.cpp)
        qt_add_resources(tst_desktop_integration desktop_test_emoji_data
            PREFIX /data FILES resources/emoji/emoji-test.txt
                resources/emoji/cldr-en-48.2.tsv)
        target_include_directories(tst_desktop_integration PRIVATE src)
        target_link_libraries(tst_desktop_integration PRIVATE
            Qt6::DBus Qt6::Quick Qt6::Sql Qt6::Test)
        add_test(NAME desktop_integration
            COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" --
                "$<TARGET_FILE:tst_desktop_integration>")
        set_tests_properties(desktop_integration PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    endif()

    find_program(GJS_EXECUTABLE NAMES gjs)
    if(GJS_EXECUTABLE)
        add_test(NAME gnome_clipboard_probe
            COMMAND "${GJS_EXECUTABLE}" -m
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/tst_gnome_clipboard_probe.js")
        add_test(NAME gnome_paste_shortcut
            COMMAND "${GJS_EXECUTABLE}" -m
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/tst_gnome_paste_shortcut.js")
    endif()

    find_program(GLIB_COMPILE_SCHEMAS_EXECUTABLE NAMES glib-compile-schemas)
    if(GLIB_COMPILE_SCHEMAS_EXECUTABLE)
        add_test(NAME gnome_shortcut_schema
            COMMAND "${GLIB_COMPILE_SCHEMAS_EXECUTABLE}" --strict --dry-run
                "${CMAKE_CURRENT_SOURCE_DIR}/integration/gnome-extension/schemas")
    endif()

    qt_add_executable(clipboard_test_source
        tests/integration/clipboard_test_source.cpp)
    target_link_libraries(clipboard_test_source PRIVATE Qt6::Gui)

    qt_add_executable(gnome_test_target
        tests/integration/gnome_test_target.cpp)
    target_link_libraries(gnome_test_target PRIVATE Qt6::Gui)

    if(EMOJI_X11_AVAILABLE)
        find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
        qt_add_executable(x11_test_target tests/integration/x11_test_target.cpp)
        target_link_libraries(x11_test_target PRIVATE Qt6::Gui X11::X11)

        qt_add_executable(tst_x11_backend
            tests/integration/tst_x11_backend.cpp
            src/platform/DesktopIntegration.h src/platform/DesktopIntegration.cpp
            src/platform/x11/X11Backend.h src/platform/x11/X11Backend.cpp
            src/app/AppController.cpp
            src/models/EmojiModel.cpp
            src/models/ClipboardModel.cpp
            src/storage/HistoryStore.cpp)
        qt_add_resources(tst_x11_backend x11_test_emoji_data
            PREFIX /data FILES resources/emoji/emoji-test.txt
                resources/emoji/cldr-en-48.2.tsv)
        target_include_directories(tst_x11_backend PRIVATE src)
        target_compile_definitions(tst_x11_backend PRIVATE
            X11_TEST_TARGET="$<TARGET_FILE:x11_test_target>")
        target_link_libraries(tst_x11_backend PRIVATE
            Qt6::Quick Qt6::Sql Qt6::Test X11::X11 X11::Xfixes X11::Xtst)
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME x11_backend
                COMMAND "${XVFB_RUN_EXECUTABLE}" -a "$<TARGET_FILE:tst_x11_backend>")
            set_tests_properties(x11_backend PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=xcb;QT_QUICK_BACKEND=software")
        endif()
    endif()
endif()
