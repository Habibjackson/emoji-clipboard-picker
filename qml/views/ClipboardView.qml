pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Item {
    id: view

    required property AppController controller

    Component.onCompleted: search.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.layoutSize(6)

        SearchField {
            id: search
            Layout.fillWidth: true
            text: view.controller.clipboardModel.query
            placeholderText: qsTr("Search saved text")
            onTextChanged: view.controller.clipboardModel.query = text
            KeyNavigation.tab: captureButton
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.layoutSize(44)
            spacing: Theme.layoutSize(6)

            Text {
                Layout.fillWidth: true
                text: qsTr("%1 pinned · %2 recent")
                      .arg(view.controller.clipboardModel.pinnedCount)
                      .arg(view.controller.clipboardModel.recentCount)
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(11)
                elide: Text.ElideRight
            }
            IconButton {
                id: captureButton
                iconName: "capture"
                prominent: true
                enabled: !view.controller.paused
                accessibilityLabel: enabled ? qsTr("Capture current clipboard item")
                                            : qsTr("Resume capture before saving the current clipboard")
                onClicked: view.controller.captureNow()
                KeyNavigation.tab: list
            }
        }

        Text {
            Layout.fillWidth: true
            visible: view.controller.paused
            text: qsTr("Capture paused. Resume it from the actions menu.")
            color: Theme.accent
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.fontSize(11)
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            visible: view.controller.clipboardModel.totalPinnedCount >= 5
            text: qsTr("Five pins in use. Unpin an item to make room.")
            color: Theme.accent
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.fontSize(11)
            wrapMode: Text.Wrap
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.layoutSize(5)
            model: view.controller.clipboardModel
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            KeyNavigation.backtab: captureButton
            section.property: "section"
            section.criteria: ViewSection.FullString
            section.delegate: Text {
                required property string section
                width: list.width
                height: Theme.layoutSize(26)
                text: section
                color: Theme.accent
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(11)
                font.weight: Font.Bold
                verticalAlignment: Text.AlignVCenter
            }
            delegate: ClipboardCard {
                width: list.width - Theme.fontSize(2)
                focus: ListView.isCurrentItem
                canPin: view.controller.clipboardModel.totalPinnedCount < 5
                onCopyRequested: view.controller.copyEntry(entryId)
                onPinRequested: pinned => view.controller.pinEntry(entryId, pinned)
                onDeleteRequested: view.controller.deleteEntry(entryId)
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - Theme.fontSize(24)
                visible: view.controller.clipboardModel.resultCount === 0
                text: search.text.length > 0 ? qsTr("No saved text matches your search.")
                                             : qsTr("No clipboard history yet. Copy text or an image to begin.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(13)
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
