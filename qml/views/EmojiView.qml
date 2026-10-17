pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Item {
    id: view

    required property AppController controller

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: view.controller.emojiModel.query.length > 0
                      ? qsTr("Search results")
                      : view.controller.emojiModel.category === "All"
                        ? qsTr("Find an emoji")
                        : view.controller.emojiModel.category
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: 26
                font.weight: Font.Bold
            }
            Text {
                text: qsTr("Choose an emoji to copy, then paste it where you need it.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: 13
            }
        }

        SearchField {
            id: search
            Layout.fillWidth: true
            placeholderText: qsTr("Search emoji by name")
            onTextChanged: view.controller.emojiModel.query = text
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("%1 emoji").arg(view.controller.emojiModel.resultCount)
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("CLICK TO COPY")
                color: Theme.accent
                font.family: Theme.bodyFamily
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 1.3
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: 80
            cellHeight: 80
            model: view.controller.emojiModel
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds

            delegate: EmojiTile {
                width: grid.cellWidth - 8
                height: grid.cellHeight - 8
                onPicked: symbol => view.controller.copyEmoji(symbol)
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: view.controller.emojiModel.resultCount === 0
                text: view.controller.emojiModel.category === "Recent"
                      ? qsTr("Your recently copied emoji will appear here.")
                      : qsTr("No emoji found. Try another name.")
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
