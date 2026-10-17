pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

T.ApplicationWindow {
    id: window

    required property AppController controller
    property string currentView: controller.initialView

    visible: true
    width: 780
    height: 580
    minimumWidth: 620
    minimumHeight: 480
    title: qsTr("Emoji Keyboard")
    background: Rectangle { color: Theme.canvas }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 196
            Layout.fillHeight: true
            color: Theme.rail

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 20
                    spacing: 10
                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: Theme.smallRadius
                        color: Theme.accent
                        Text {
                            anchors.centerIn: parent
                            text: "✦"
                            color: Theme.accentInk
                            font.pixelSize: 22
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: qsTr("Emoji")
                            color: Theme.ink
                            font.family: Theme.bodyFamily
                            font.pixelSize: 16
                            font.weight: Font.Bold
                        }
                        Text {
                            text: qsTr("KEYBOARD")
                            color: Theme.muted
                            font.family: Theme.bodyFamily
                            font.pixelSize: 9
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
                    Layout.topMargin: 12
                    Layout.bottomMargin: 6
                    color: Theme.line
                }

                Text {
                    text: window.currentView === "emoji" ? qsTr("CATEGORIES") : qsTr("HISTORY")
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    font.letterSpacing: 1.3
                    leftPadding: 10
                }

                ListView {
                    id: categories
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.currentView === "emoji"
                    clip: true
                    spacing: 2
                    model: window.controller.emojiModel.categories
                    delegate: CategoryButton {
                        required property string modelData
                        width: categories.width
                        categoryName: modelData
                        selected: window.controller.emojiModel.category === categoryName
                        onClicked: window.controller.emojiModel.category = categoryName
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.currentView === "clipboard"
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("History stays on this device. Pin important items to keep them.")
                        color: Theme.muted
                        wrapMode: Text.Wrap
                        font.family: Theme.bodyFamily
                        font.pixelSize: 12
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        text: window.controller.paused ? qsTr("Resume capture") : qsTr("Pause capture")
                        onClicked: window.controller.paused = !window.controller.paused
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        text: qsTr("Clear recent")
                        onClicked: window.controller.clearRecent()
                    }
                    Item { Layout.fillHeight: true }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("LOCAL · PRIVATE")
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 24
            spacing: 12

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
                    text: window.controller.status.length > 0 ? window.controller.status
                                                              : qsTr("Select an item to copy")
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                Text {
                    text: qsTr("Ctrl+1 Emoji · Ctrl+2 Clipboard")
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: 11
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
        onActivated: window.close()
    }
}
