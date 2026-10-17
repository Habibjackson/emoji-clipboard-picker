pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

T.ApplicationWindow {
    id: window

    required property AppController controller
    property string currentView: controller.initialView
    readonly property bool compact: width < 700
    readonly property int availableWidth: Screen.width > 0 ? Screen.width : 720
    readonly property int availableHeight: Screen.height > 0 ? Screen.height : 520

    visible: false
    flags: Qt.Tool | Qt.FramelessWindowHint
    width: Math.min(Theme.fontSize(720), availableWidth - 24)
    height: Math.min(Theme.fontSize(520), availableHeight - 24)
    minimumWidth: Math.min(520, availableWidth - 24)
    minimumHeight: Math.min(420, availableHeight - 24)
    title: qsTr("Emoji Keyboard")
    background: Rectangle { color: Theme.canvas }
    onClosing: function(close) {
        close.accepted = false
        window.hide()
    }

    Connections {
        target: window.controller
        function onItemCopied() { window.hide() }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: Math.min(Theme.fontSize(196), window.width * 0.31)
            Layout.fillHeight: true
            color: Theme.rail

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.fontSize(window.compact ? 10 : 16)
                spacing: Theme.fontSize(window.compact ? 6 : 10)

                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.fontSize(window.compact ? 8 : 20)
                    spacing: Theme.fontSize(10)
                    Rectangle {
                        Layout.preferredWidth: Theme.fontSize(38)
                        Layout.preferredHeight: Theme.fontSize(38)
                        radius: Theme.smallRadius
                        color: Theme.accent
                        Text {
                            anchors.centerIn: parent
                            text: "✦"
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontSize(22)
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: qsTr("Emoji")
                            color: Theme.ink
                            font.family: Theme.bodyFamily
                            font.pixelSize: Theme.fontSize(16)
                            font.weight: Font.Bold
                        }
                        Text {
                            text: qsTr("KEYBOARD")
                            color: Theme.muted
                            font.family: Theme.bodyFamily
                            font.pixelSize: Theme.fontSize(9)
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.8
                        }
                    }
                }

                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Emoji")
                    selected: window.currentView === "emoji"
                    onClicked: window.currentView = "emoji"
                }
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Clipboard")
                    selected: window.currentView === "clipboard"
                    onClicked: window.currentView = "clipboard"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.topMargin: Theme.fontSize(12)
                    Layout.bottomMargin: Theme.fontSize(6)
                    color: Theme.line
                }

                Text {
                    text: window.currentView === "emoji" ? qsTr("CATEGORIES") : qsTr("HISTORY")
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(10)
                    font.weight: Font.Bold
                    font.letterSpacing: 1.3
                    leftPadding: Theme.fontSize(10)
                }

                ListView {
                    id: categories
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.currentView === "emoji"
                    clip: true
                    spacing: 2
                    model: window.controller.emojiModel.categories
                    activeFocusOnTab: true
                    delegate: CategoryButton {
                        required property int index
                        required property string modelData
                        width: categories.width
                        categoryName: modelData
                        selected: window.controller.emojiModel.category === categoryName
                        focus: ListView.isCurrentItem
                        onClicked: {
                            categories.currentIndex = index
                            window.controller.emojiModel.category = categoryName
                        }
                    }
                }

                Flickable {
                    id: clipboardActions
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.currentView === "clipboard"
                    clip: true
                    contentWidth: width
                    contentHeight: actionColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds

                    function reveal(item) {
                        if (item.y < contentY)
                            contentY = item.y
                        else if (item.y + item.height > contentY + height)
                            contentY = item.y + item.height - height
                    }

                    ColumnLayout {
                        id: actionColumn
                        width: clipboardActions.width
                        spacing: Theme.fontSize(8)

                        Text {
                            Layout.fillWidth: true
                            text: window.compact && window.controller.backgroundCaptureLimited
                                  ? qsTr("On Wayland, use Capture current for missed copies.")
                                  : qsTr("History stays on this device. Pin important items to keep them.")
                            color: Theme.muted
                            wrapMode: Text.Wrap
                            font.family: Theme.bodyFamily
                            font.pixelSize: Theme.fontSize(12)
                        }
                        ActionButton {
                            id: pauseButton
                            Layout.fillWidth: true
                            text: window.compact ? window.controller.paused ? qsTr("Resume")
                                                                     : qsTr("Pause")
                                                 : window.controller.paused ? qsTr("Resume capture")
                                                                            : qsTr("Pause capture")
                            accessibilityLabel: window.controller.paused
                                                ? qsTr("Resume clipboard capture")
                                                : qsTr("Pause clipboard capture")
                            onClicked: window.controller.paused = !window.controller.paused
                            onActiveFocusChanged: if (activeFocus) clipboardActions.reveal(pauseButton)
                        }
                        Text {
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.fontSize(10)
                            text: qsTr("PASTE SHORTCUT")
                            color: Theme.muted
                            font.family: Theme.bodyFamily
                            font.pixelSize: Theme.fontSize(10)
                            font.weight: Font.Bold
                            font.letterSpacing: 1.3
                        }
                        ActionButton {
                            id: pasteShortcutButton
                            Layout.fillWidth: true
                            text: window.controller.pasteShortcutMode === "ctrl-v" ? qsTr("Ctrl+V")
                                  : window.controller.pasteShortcutMode === "ctrl-shift-v"
                                    ? qsTr("Ctrl+Shift+V") : qsTr("Auto detect")
                            accessibilityLabel: qsTr("Paste shortcut: %1. Open choices").arg(text)
                            onClicked: pasteShortcutPopup.open()
                            onActiveFocusChanged: if (activeFocus) clipboardActions.reveal(pasteShortcutButton)
                        }
                        ActionButton {
                            id: clearRecentButton
                            Layout.fillWidth: true
                            text: qsTr("Clear recent")
                            onClicked: window.controller.clearRecent()
                            onActiveFocusChanged: if (activeFocus) clipboardActions.reveal(clearRecentButton)
                        }
                        ActionButton {
                            id: clearAllButton
                            Layout.fillWidth: true
                            text: qsTr("Clear all")
                            accessibilityLabel: qsTr("Clear all clipboard history")
                            onClicked: clearAllDialog.open()
                            onActiveFocusChanged: if (activeFocus) clipboardActions.reveal(clearAllButton)
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: !window.compact
                    text: qsTr("LOCAL · PRIVATE")
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(10)
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.margins: Theme.fontSize(window.compact ? 12 : 24)
            spacing: Theme.fontSize(window.compact ? 8 : 12)

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: integrationNotice.implicitHeight + Theme.fontSize(20)
                visible: window.controller.integrationWarning.length > 0
                radius: Theme.smallRadius
                color: Theme.accentSoft

                Column {
                    id: integrationNotice
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.fontSize(10)
                    spacing: Theme.fontSize(4)

                    Text {
                        width: parent.width
                        visible: !window.compact
                        text: window.controller.copyOnly ? qsTr("Copy only")
                                                         : qsTr("Shortcut unavailable")
                        color: Theme.accent
                        font.family: Theme.bodyFamily
                        font.pixelSize: Theme.fontSize(12)
                        font.weight: Font.Bold
                    }
                    Text {
                        width: parent.width
                        text: window.compact && window.controller.copyOnly
                              ? qsTr("Copy only. %1 Select an item, then paste manually.")
                                    .arg(window.controller.integrationWarning)
                              : window.controller.integrationWarning
                        textFormat: Text.PlainText
                        color: Theme.ink
                        wrapMode: Text.Wrap
                        font.family: Theme.bodyFamily
                        font.pixelSize: Theme.fontSize(12)
                    }
                    Text {
                        width: parent.width
                        visible: !window.compact && window.controller.copyOnly
                        text: qsTr("Select an emoji or history item to copy it, then paste manually in your app.")
                        color: Theme.ink
                        wrapMode: Text.Wrap
                        font.family: Theme.bodyFamily
                        font.pixelSize: Theme.fontSize(12)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: attentionText.implicitHeight + Theme.fontSize(20)
                visible: window.controller.attention.length > 0
                radius: Theme.smallRadius
                color: Theme.accentSoft

                Text {
                    id: attentionText
                    anchors.fill: parent
                    anchors.margins: Theme.fontSize(10)
                    text: window.controller.attention
                    textFormat: Text.PlainText
                    color: Theme.ink
                    wrapMode: Text.Wrap
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(12)
                }
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: window.currentView === "clipboard" ? clipboardComponent
                                                                   : emojiComponent
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.line
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    wrapMode: Text.Wrap
                    maximumLineCount: window.compact ? 1 : 2
                    text: window.controller.status.length > 0 ? window.controller.status
                                                              : qsTr("Select an item to copy")
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(12)
                    elide: Text.ElideRight
                }
                Text {
                    text: qsTr("Ctrl+1 Emoji · Ctrl+2 Clipboard · Esc Hide")
                    visible: window.width >= 900
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(11)
                }
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
        id: pasteShortcutPopup
        parent: window.contentItem
        x: Math.min(window.width - width - 12,
                    pasteShortcutButton.mapToItem(window.contentItem,
                                                  pasteShortcutButton.width + 8, 0).x)
        y: Math.max(12, Math.min(window.height - height - 12,
                                pasteShortcutButton.mapToItem(window.contentItem, 0, 0).y))
        width: Theme.fontSize(190)
        padding: Theme.fontSize(8)
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: autoPasteButton.forceActiveFocus()

        background: Rectangle {
            radius: Theme.smallRadius
            color: Theme.surface
            border.color: Theme.line
            border.width: 1
        }
        contentItem: ColumnLayout {
            spacing: Theme.fontSize(4)
            ActionButton {
                id: autoPasteButton
                Layout.fillWidth: true
                text: qsTr("Auto detect")
                selected: window.controller.pasteShortcutMode === "auto"
                accessibilityLabel: qsTr("Automatically detect the paste shortcut")
                onClicked: {
                    window.controller.pasteShortcutMode = "auto"
                    pasteShortcutPopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                text: qsTr("Ctrl+V")
                selected: window.controller.pasteShortcutMode === "ctrl-v"
                accessibilityLabel: qsTr("Always paste with Ctrl+V")
                onClicked: {
                    window.controller.pasteShortcutMode = "ctrl-v"
                    pasteShortcutPopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                text: qsTr("Ctrl+Shift+V")
                selected: window.controller.pasteShortcutMode === "ctrl-shift-v"
                accessibilityLabel: qsTr("Always paste with Ctrl+Shift+V")
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
        width: Math.min(Theme.fontSize(390), parent.width - 32)
        height: confirmation.implicitHeight + topPadding + bottomPadding
        padding: Theme.fontSize(20)
        modal: true
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: cancelClear.forceActiveFocus()

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.line
            border.width: 1
        }
        contentItem: ColumnLayout {
            id: confirmation
            spacing: Theme.fontSize(16)

            Text {
                Layout.fillWidth: true
                text: qsTr("Clear all clipboard history?")
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(18)
                font.weight: Font.Bold
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("This deletes all pinned and recent clipboard items from this device. Emoji recents stay.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(13)
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
        enabled: !clearAllDialog.visible
        onActivated: window.hide()
    }
    Shortcut {
        sequences: ["Ctrl+Q"]
        onActivated: Qt.quit()
    }
}
