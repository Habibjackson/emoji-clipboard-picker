pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

T.ApplicationWindow {
    id: window

    required property AppController controller
    property string currentView: controller.initialView
    readonly property int availableWidth: Screen.width > 0 ? Screen.width : 720
    readonly property int availableHeight: Screen.height > 0 ? Screen.height : 520
    readonly property string pasteModeLabel: controller.pasteShortcutMode === "ctrl-v"
                                              ? qsTr("Ctrl+V")
                                              : controller.pasteShortcutMode === "ctrl-shift-v"
                                                ? qsTr("Ctrl+Shift+V") : qsTr("Auto detect")

    visible: false
    flags: Qt.Tool | Qt.FramelessWindowHint
    color: "transparent"
    width: Math.min(Theme.fontSize(468), availableWidth - 24)
    height: Math.min(Theme.fontSize(520), availableHeight - 24)
    minimumWidth: Math.min(Theme.fontSize(360), availableWidth - 24)
    minimumHeight: Math.min(Theme.fontSize(420), availableHeight - 24)
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
    }

    Connections {
        target: window.controller
        function onItemCopied() { window.hide() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.fontSize(12)
        spacing: Theme.fontSize(8)

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.fontSize(44)
            spacing: Theme.fontSize(9)

            Rectangle {
                Layout.preferredWidth: Theme.fontSize(34)
                Layout.preferredHeight: Theme.fontSize(34)
                radius: Theme.smallRadius
                color: Theme.accent

                Text {
                    anchors.centerIn: parent
                    text: "✦"
                    color: Theme.accentInk
                    font.pixelSize: Theme.fontSize(19)
                    Accessible.ignored: true
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Emoji Keyboard")
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(15)
                font.weight: Font.Bold
                elide: Text.ElideRight
            }

            ActionButton {
                Layout.preferredWidth: Theme.fontSize(44)
                text: "⋯"
                font.pixelSize: Theme.fontSize(20)
                selected: actionsPopup.visible
                accessibilityLabel: qsTr("History actions")
                onClicked: actionsPopup.open()
            }
            ActionButton {
                Layout.preferredWidth: Theme.fontSize(44)
                text: "×"
                font.pixelSize: Theme.fontSize(20)
                accessibilityLabel: qsTr("Hide picker")
                onClicked: window.hide()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.fontSize(48)
            radius: Theme.smallRadius
            color: Theme.rail

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.fontSize(2)
                spacing: Theme.fontSize(3)

                ActionButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("Emoji")
                    selected: window.currentView === "emoji"
                    onClicked: window.currentView = "emoji"
                }
                ActionButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("Clipboard")
                    selected: window.currentView === "clipboard"
                    onClicked: window.currentView = "clipboard"
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
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.fontSize(22)
            spacing: Theme.fontSize(8)

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
        id: actionsPopup
        parent: window.contentItem
        x: window.width - width - Theme.fontSize(12)
        y: Theme.fontSize(58)
        width: Math.min(Theme.fontSize(232), window.width - Theme.fontSize(24))
        height: actionsContent.implicitHeight + topPadding + bottomPadding
        padding: Theme.fontSize(8)
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: pauseButton.forceActiveFocus()

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
        id: pasteShortcutPopup
        parent: window.contentItem
        x: window.width - width - Theme.fontSize(12)
        y: Theme.fontSize(58)
        width: Math.min(Theme.fontSize(208), window.width - Theme.fontSize(24))
        height: pasteChoices.implicitHeight + topPadding + bottomPadding
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
        enabled: !actionsPopup.visible && !pasteShortcutPopup.visible && !clearAllDialog.visible
        onActivated: window.hide()
    }
    Shortcut {
        sequences: ["Ctrl+Q"]
        onActivated: Qt.quit()
    }
}
