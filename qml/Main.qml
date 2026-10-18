pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

T.ApplicationWindow {
    id: window

    required property AppController controller
    required property SystemAppearance systemAppearance
    required property ShortcutSettings shortcutSettings
    property string currentView: controller.initialView
    readonly property int availableWidth: Screen.width > 0 ? Screen.width : 720
    readonly property int availableHeight: Screen.height > 0 ? Screen.height : 520
    readonly property bool shortHeight: height < Theme.layoutSize(380)
    readonly property string themeModeLabel: controller.themeMode === "light" ? qsTr("Light")
                                             : controller.themeMode === "dark" ? qsTr("Dark")
                                                                                 : qsTr("System")
    readonly property string pasteModeLabel: controller.pasteShortcutMode === "ctrl-v"
                                              ? qsTr("Ctrl+V")
                                              : controller.pasteShortcutMode === "ctrl-shift-v"
                                                ? qsTr("Ctrl+Shift+V") : qsTr("Auto detect")

    visible: false
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    width: Math.min(Theme.layoutSize(340), availableWidth - 24)
    height: Math.min(Theme.layoutSize(390), availableHeight - 24)
    minimumWidth: Math.min(Theme.layoutSize(320), availableWidth - 24)
    minimumHeight: Math.min(Theme.layoutSize(360), availableHeight - 24)
    title: qsTr("Emoji Keyboard")
    onClosing: function(close) {
        close.accepted = false
        window.hide()
    }

    background: Rectangle {
        radius: Theme.radius
        color: Theme.canvas
        border.color: Theme.line
        border.width: 1
        Behavior on color {
            ColorAnimation { duration: 180 }
        }
    }

    Binding {
        target: Theme
        property: "mode"
        value: window.controller.themeMode
    }
    Binding {
        target: Theme
        property: "systemDark"
        value: window.systemAppearance.dark
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.layoutSize(10)
        spacing: Theme.layoutSize(6)

        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.layoutSize(44)
            radius: Theme.smallRadius
            color: Theme.rail
            Behavior on color {
                ColorAnimation { duration: 180 }
            }

            Row {
                id: viewTabs
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                IconButton {
                    iconName: "emoji"
                    accessibilityLabel: qsTr("Emoji")
                    selected: window.currentView === "emoji"
                    onClicked: window.currentView = "emoji"
                }
                IconButton {
                    iconName: "clipboard"
                    accessibilityLabel: qsTr("Clipboard history")
                    selected: window.currentView === "clipboard"
                    onClicked: window.currentView = "clipboard"
                }
            }

            Item {
                id: dragArea
                width: Theme.layoutSize(44)
                height: parent.height
                x: Math.min(Math.max(viewTabs.width + Theme.layoutSize(4),
                                     (topBar.width - width) / 2),
                            topActions.x - width - Theme.layoutSize(4))
                Accessible.name: qsTr("Drag to move picker")

                Rectangle {
                    anchors.centerIn: parent
                    width: dragHover.hovered ? Theme.layoutSize(35) : Theme.layoutSize(30)
                    height: Theme.layoutSize(4)
                    radius: height / 2
                    color: dragHover.hovered ? Theme.accent : Theme.muted
                    Accessible.ignored: true
                    Behavior on width {
                        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                    }
                    Behavior on color {
                        ColorAnimation { duration: 140 }
                    }
                }
                DragHandler {
                    target: null
                    cursorShape: Qt.SizeAllCursor
                    onActiveChanged: {
                        if (active)
                            window.startSystemMove()
                    }
                }
                HoverHandler {
                    id: dragHover
                    cursorShape: Qt.SizeAllCursor
                }
                PickerToolTip {
                    anchorItem: dragArea
                    requested: dragHover.hovered
                    text: qsTr("Drag to move picker")
                }
            }

            Row {
                id: topActions
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter

                IconButton {
                    iconName: window.controller.themeMode
                    accessibilityLabel: qsTr("Theme: %1").arg(window.themeModeLabel)
                    toolTipBlocked: themePopup.visible
                    onClicked: themePopup.open()
                }
                IconButton {
                    iconName: "menu"
                    accessibilityLabel: qsTr("More actions")
                    selected: actionsPopup.visible
                    toolTipBlocked: actionsPopup.visible
                    onClicked: actionsPopup.open()
                }
                IconButton {
                    iconName: "close"
                    accessibilityLabel: qsTr("Hide picker")
                    onClicked: window.hide()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(Theme.fontSize(38), integrationNotice.implicitHeight
                                            + Theme.fontSize(14))
            visible: window.controller.integrationWarning.length > 0
            radius: Theme.smallRadius
            color: Theme.accentSoft

            Text {
                id: integrationNotice
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Theme.fontSize(8)
                text: window.controller.copyOnly
                      ? qsTr("Copy only · %1").arg(window.controller.integrationWarning)
                      : window.controller.integrationWarning
                textFormat: Text.PlainText
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(11)
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(Theme.fontSize(36), attentionText.implicitHeight
                                            + Theme.fontSize(12))
            visible: window.controller.attention.length > 0
            radius: Theme.smallRadius
            color: Theme.accentSoft

            Text {
                id: attentionText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Theme.fontSize(8)
                text: window.controller.attention
                textFormat: Text.PlainText
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(11)
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: window.currentView === "clipboard" ? clipboardComponent
                                                                  : emojiComponent
        }

        RowLayout {
            visible: !window.shortHeight
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.layoutSize(20)
            spacing: Theme.layoutSize(8)

            Text {
                Layout.fillWidth: true
                text: window.controller.status.length > 0 ? window.controller.status
                                                          : qsTr("Select an item to copy")
                textFormat: Text.PlainText
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(11)
                elide: Text.ElideRight
            }
            Text {
                text: qsTr("Esc to hide")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(10)
            }
        }
    }

    Component {
        id: emojiComponent
        EmojiView { controller: window.controller }
    }
    Component {
        id: clipboardComponent
        ClipboardView { controller: window.controller }
    }

    T.Popup {
        id: themePopup
        parent: window.contentItem
        x: window.width - width - Theme.layoutSize(10)
        y: Theme.layoutSize(54)
        width: Math.min(Theme.layoutSize(208), window.width - Theme.layoutSize(24))
        height: themeChoices.implicitHeight + topPadding + bottomPadding
        padding: Theme.layoutSize(8)
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: systemThemeButton.forceActiveFocus()
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 }
        }

        background: Rectangle {
            radius: Theme.smallRadius
            color: Theme.surface
            border.color: Theme.line
            border.width: 1
        }
        contentItem: ColumnLayout {
            id: themeChoices
            spacing: Theme.layoutSize(3)

            ActionButton {
                id: systemThemeButton
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Follow system")
                selected: window.controller.themeMode === "system"
                onClicked: {
                    window.controller.themeMode = "system"
                    themePopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Light")
                selected: window.controller.themeMode === "light"
                onClicked: {
                    window.controller.themeMode = "light"
                    themePopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Dark")
                selected: window.controller.themeMode === "dark"
                onClicked: {
                    window.controller.themeMode = "dark"
                    themePopup.close()
                }
            }
        }
    }

    T.Popup {
        id: actionsPopup
        parent: window.contentItem
        x: window.width - width - Theme.fontSize(12)
        y: Theme.layoutSize(54)
        width: Math.min(Theme.fontSize(232), window.width - Theme.fontSize(24))
        height: actionsContent.implicitHeight + topPadding + bottomPadding
        padding: Theme.fontSize(8)
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: pauseButton.forceActiveFocus()
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 }
        }

        background: Rectangle {
            radius: Theme.smallRadius
            color: Theme.surface
            border.color: Theme.line
            border.width: 1
        }
        contentItem: ColumnLayout {
            id: actionsContent
            spacing: Theme.fontSize(3)

            ActionButton {
                id: pauseButton
                Layout.fillWidth: true
                menuItem: true
                text: window.controller.paused ? qsTr("Resume capture") : qsTr("Pause capture")
                onClicked: {
                    window.controller.paused = !window.controller.paused
                    actionsPopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Paste shortcut · %1").arg(window.pasteModeLabel)
                accessibilityLabel: qsTr("Choose paste shortcut")
                onClicked: {
                    actionsPopup.close()
                    pasteShortcutPopup.open()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Keyboard shortcuts…")
                accessibilityLabel: qsTr("Configure emoji and clipboard keyboard shortcuts")
                onClicked: {
                    actionsPopup.close()
                    keyboardShortcutsPopup.open()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: Theme.fontSize(4)
                Layout.bottomMargin: Theme.fontSize(4)
                color: Theme.line
            }
            ActionButton {
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Clear recent history")
                onClicked: {
                    window.controller.clearRecent()
                    actionsPopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Clear all history…")
                onClicked: {
                    actionsPopup.close()
                    clearAllDialog.open()
                }
            }
        }
    }

    T.Popup {
        id: keyboardShortcutsPopup
        parent: window.contentItem
        x: window.width - width - Theme.layoutSize(10)
        y: Theme.layoutSize(54)
        width: Math.min(Theme.layoutSize(310), window.width - Theme.layoutSize(24))
        height: shortcutChoices.implicitHeight + topPadding + bottomPadding
        padding: Theme.layoutSize(10)
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: {
            window.shortcutSettings.reload()
            emojiRecorder.focusShortcut()
        }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 }
        }
        background: Rectangle {
            radius: Theme.smallRadius
            color: Theme.surface
            border.color: Theme.line
            border.width: 1
        }
        contentItem: ColumnLayout {
            id: shortcutChoices
            spacing: Theme.layoutSize(7)

            Text {
                text: qsTr("Open picker")
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(13)
                font.weight: Font.DemiBold
            }
            ShortcutRecorder {
                id: emojiRecorder
                Layout.fillWidth: true
                settings: window.shortcutSettings
                viewName: "emoji"
                title: qsTr("Emoji")
            }
            ShortcutRecorder {
                Layout.fillWidth: true
                settings: window.shortcutSettings
                viewName: "clipboard"
                title: qsTr("Clipboard")
            }
            Text {
                Layout.fillWidth: true
                text: window.shortcutSettings.available
                      ? qsTr("Press Super+key or Ctrl+Alt+key. Esc cancels.")
                      : qsTr("Install and start GNOME or X11 integration to edit shortcuts.")
                textFormat: Text.PlainText
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(10)
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                visible: window.shortcutSettings.errorMessage.length > 0
                text: window.shortcutSettings.errorMessage
                textFormat: Text.PlainText
                color: Theme.accent
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(10)
                wrapMode: Text.Wrap
            }
        }
    }

    T.Popup {
        id: pasteShortcutPopup
        parent: window.contentItem
        x: window.width - width - Theme.fontSize(12)
        y: Theme.layoutSize(54)
        width: Math.min(Theme.fontSize(208), window.width - Theme.fontSize(24))
        height: pasteChoices.implicitHeight + topPadding + bottomPadding
        padding: Theme.fontSize(8)
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: autoPasteButton.forceActiveFocus()
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 }
        }

        background: Rectangle {
            radius: Theme.smallRadius
            color: Theme.surface
            border.color: Theme.line
            border.width: 1
        }
        contentItem: ColumnLayout {
            id: pasteChoices
            spacing: Theme.fontSize(3)

            ActionButton {
                id: autoPasteButton
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Auto detect")
                selected: window.controller.pasteShortcutMode === "auto"
                onClicked: {
                    window.controller.pasteShortcutMode = "auto"
                    pasteShortcutPopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Ctrl+V")
                selected: window.controller.pasteShortcutMode === "ctrl-v"
                onClicked: {
                    window.controller.pasteShortcutMode = "ctrl-v"
                    pasteShortcutPopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                menuItem: true
                text: qsTr("Ctrl+Shift+V")
                selected: window.controller.pasteShortcutMode === "ctrl-shift-v"
                onClicked: {
                    window.controller.pasteShortcutMode = "ctrl-shift-v"
                    pasteShortcutPopup.close()
                }
            }
        }
    }

    T.Popup {
        id: clearAllDialog
        parent: window.contentItem
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        width: Math.min(Theme.fontSize(390), parent.width - Theme.fontSize(24))
        height: confirmation.implicitHeight + topPadding + bottomPadding
        padding: Theme.fontSize(18)
        modal: true
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: cancelClear.forceActiveFocus()
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 }
        }

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.line
            border.width: 1
        }
        contentItem: ColumnLayout {
            id: confirmation
            spacing: Theme.fontSize(12)

            Text {
                Layout.fillWidth: true
                text: qsTr("Clear all clipboard history?")
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(17)
                font.weight: Font.Bold
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("This deletes all pinned and recent clipboard items. Emoji recents stay.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(12)
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ActionButton {
                    id: cancelClear
                    text: qsTr("Cancel")
                    onClicked: clearAllDialog.close()
                }
                ActionButton {
                    text: qsTr("Clear all")
                    prominent: true
                    accessibilityLabel: qsTr("Confirm clear all clipboard history")
                    onClicked: {
                        window.controller.clearAll()
                        clearAllDialog.close()
                    }
                }
            }
        }
    }

    Shortcut {
        sequences: ["Ctrl+1"]
        onActivated: window.currentView = "emoji"
    }
    Shortcut {
        sequences: ["Ctrl+2"]
        onActivated: window.currentView = "clipboard"
    }
    Shortcut {
        sequences: ["Escape"]
        enabled: !actionsPopup.visible && !themePopup.visible
                 && !pasteShortcutPopup.visible && !clearAllDialog.visible
        onActivated: window.hide()
    }
    Shortcut {
        sequences: ["Ctrl+Q"]
        onActivated: Qt.quit()
    }
}
