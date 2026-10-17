pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Item {
    id: view

    required property AppController controller
    readonly property bool compact: width < Theme.fontSize(420)

    Component.onCompleted: search.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.fontSize(view.compact ? 8 : 14)

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.fontSize(4)
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("Clipboard history")
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(26)
                font.weight: Font.Bold
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: !view.compact
                text: qsTr("Five permanent pins and twenty recent items, saved on this device.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(13)
                wrapMode: Text.Wrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: pauseContent.implicitHeight + Theme.fontSize(20)
            visible: view.controller.paused && !view.compact
            radius: Theme.smallRadius
            color: Theme.accentSoft

            ColumnLayout {
                id: pauseContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.fontSize(10)
                spacing: Theme.fontSize(6)

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTr("Capture is paused. New clipboard copies will not be saved.")
                    textFormat: Text.PlainText
                    color: Theme.ink
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(12)
                    wrapMode: Text.Wrap
                }
                ActionButton {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Resume capture")
                    onClicked: view.controller.paused = false
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: view.controller.paused && view.compact
            text: qsTr("Capture paused. Use Resume capture in the sidebar to save new copies.")
            textFormat: Text.PlainText
            color: Theme.accent
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.fontSize(12)
            wrapMode: Text.Wrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: warning.implicitHeight + Theme.fontSize(20)
            radius: Theme.smallRadius
            color: Theme.accentSoft
            visible: !view.compact && !view.controller.paused
                     && view.controller.backgroundCaptureLimited
            Text {
                id: warning
                anchors.fill: parent
                anchors.margins: Theme.fontSize(10)
                text: qsTr("On Wayland, use Capture current if a copy made in another app is missing from history.")
                textFormat: Text.PlainText
                color: Theme.ink
                wrapMode: Text.Wrap
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(12)
            }
        }

        SearchField {
            id: search
            Layout.fillWidth: true
            text: view.controller.clipboardModel.query
            placeholderText: qsTr("Search saved text")
            onTextChanged: view.controller.clipboardModel.query = text
            KeyNavigation.tab: list
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("%1 pinned · %2 recent")
                      .arg(view.controller.clipboardModel.pinnedCount)
                      .arg(view.controller.clipboardModel.recentCount)
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(12)
            }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: qsTr("Capture current")
                prominent: true
                enabled: !view.controller.paused
                accessibilityLabel: enabled ? qsTr("Capture current clipboard item")
                                            : qsTr("Resume capture before saving the current clipboard")
                onClicked: view.controller.captureNow()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: view.controller.clipboardModel.totalPinnedCount >= 5
            text: qsTr("All five pins are in use. Unpin an item to pin another.")
            color: Theme.accent
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.fontSize(12)
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.fontSize(8)
            model: view.controller.clipboardModel
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            KeyNavigation.backtab: search
            section.property: "section"
            section.criteria: ViewSection.FullString
            section.delegate: Text {
                required property string section
                width: list.width
                height: 36
                text: section
                color: Theme.accent
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(12)
                font.weight: Font.Bold
                verticalAlignment: Text.AlignVCenter
            }
            delegate: ClipboardCard {
                width: list.width - 8
                focus: ListView.isCurrentItem
                canPin: view.controller.clipboardModel.totalPinnedCount < 5
                onCopyRequested: view.controller.copyEntry(entryId)
                onPinRequested: pinned => view.controller.pinEntry(entryId, pinned)
                onDeleteRequested: view.controller.deleteEntry(entryId)
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: view.controller.clipboardModel.resultCount === 0
                text: search.text.length > 0 ? qsTr("No saved text matches your search.")
                                             : qsTr("No clipboard history yet. Copy text or an image to begin.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(14)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }
    }

    Shortcut {
        sequences: ["Ctrl+F"]
        onActivated: search.forceActiveFocus()
    }
}
