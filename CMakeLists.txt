cmake_minimum_required(VERSION 3.24)

project(EmojiKeyboard VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml DBus Quick QuickControls2 Sql)
qt_standard_project_setup(REQUIRES 6.11)

qt_add_executable(emoji-keyboard
    src/main.cpp
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
    FILES resources/emoji/emoji-test.txt
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

set(EMOJI_KEYBOARD_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/emoji-keyboard")
configure_file(integration/dbus/org.emojikeyboard.App.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.emojikeyboard.App.service" @ONLY)

install(TARGETS emoji-keyboard
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
    qt_add_executable(tst_history_store
        tests/unit/tst_history_store.cpp
        src/storage/HistoryStore.cpp
    )
    target_include_directories(tst_history_store PRIVATE src)
    target_link_libraries(tst_history_store PRIVATE Qt6::Core Qt6::Gui Qt6::Sql Qt6::Test)
    add_test(NAME history_store COMMAND tst_history_store)

    qt_add_executable(tst_app_controller
        tests/integration/tst_app_controller.cpp
        src/app/AppController.cpp
        src/models/EmojiModel.cpp
        src/models/ClipboardModel.cpp
        src/storage/HistoryStore.cpp
    )
    qt_add_resources(tst_app_controller test_emoji_data
        PREFIX /data
        FILES resources/emoji/emoji-test.txt
    )
    target_include_directories(tst_app_controller PRIVATE src)
    target_link_libraries(tst_app_controller PRIVATE Qt6::Quick Qt6::Sql Qt6::Test)
    add_test(NAME app_controller COMMAND tst_app_controller)
    set_tests_properties(app_controller PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

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
endif()
