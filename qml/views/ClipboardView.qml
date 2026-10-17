pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Item {
    id: view

    required property AppController controller

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Text {
                text: qsTr("Clipboard history")
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: 26
                font.weight: Font.Bold
            }
            Text {
                text: qsTr("Five permanent pins and twenty recent items, saved on this device.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: 13
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: warning.implicitHeight + 20
            radius: Theme.smallRadius
            color: Theme.accentSoft
            visible: view.controller.backgroundCaptureLimited
            Text {
                id: warning
                anchors.fill: parent
                anchors.margins: 10
                text: qsTr("On Wayland, use Capture current if a copy made in another app is missing from history.")
                textFormat: Text.PlainText
                color: Theme.ink
                wrapMode: Text.Wrap
                font.family: Theme.bodyFamily
                font.pixelSize: 12
            }
        }

        SearchField {
            id: search
            Layout.fillWidth: true
            placeholderText: qsTr("Search saved text")
            onTextChanged: view.controller.clipboardModel.query = text
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("%1 pinned · %2 recent")
                      .arg(view.controller.clipboardModel.pinnedCount)
                      .arg(view.controller.clipboardModel.recentCount)
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: qsTr("Capture current")
                prominent: true
                onClicked: view.controller.captureNow()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: view.controller.clipboardModel.totalPinnedCount >= 5
            text: qsTr("All five pins are in use. Unpin an item to pin another.")
            color: Theme.accent
            font.family: Theme.bodyFamily
            font.pixelSize: 12
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: view.controller.clipboardModel
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            section.property: "section"
            section.criteria: ViewSection.FullString
            section.delegate: Text {
                required property string section
                width: list.width
                height: 36
                text: section
                color: Theme.accent
                font.family: Theme.bodyFamily
                font.pixelSize: 12
                font.weight: Font.Bold
                verticalAlignment: Text.AlignVCenter
            }
            delegate: ClipboardCard {
                width: list.width - 8
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
                font.pixelSize: 14
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
