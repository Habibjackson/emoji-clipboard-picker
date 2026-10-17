pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

Item {
    id: view

    required property AppController controller

    Component.onCompleted: search.forceActiveFocus()

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
                text: qsTr("Choose an emoji to copy. Tiles marked ▾ offer skin tones.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: 13
            }
        }

        SearchField {
            id: search
            Layout.fillWidth: true
            text: view.controller.emojiModel.query
            placeholderText: qsTr("Search emoji by name or keyword")
            onTextChanged: view.controller.emojiModel.query = text
            KeyNavigation.tab: grid
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("%1 choices").arg(view.controller.emojiModel.resultCount)
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("PICK · ▾ FOR TONES")
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
            activeFocusOnTab: true
            KeyNavigation.backtab: search

            delegate: EmojiTile {
                width: grid.cellWidth - 8
                height: grid.cellHeight - 8
                focus: GridView.isCurrentItem
                onPicked: symbol => view.controller.copyEmoji(symbol)
                onVariantsRequested: (symbol, emojiName) => {
                    variantPopup.baseName = emojiName
                    variantPopup.choices = view.controller.emojiModel.variantsFor(symbol)
                    variantPopup.open()
                }
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

    T.Popup {
        id: variantPopup
        parent: view
        property string baseName
        property var choices: []

        x: (view.width - width) / 2
        y: (view.height - height) / 2
        width: Math.min(view.width - 24, 360)
        height: Math.min(view.height - 24, Math.ceil(choices.length / 4) * 80 + 96)
        padding: 16
        modal: true
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
        onOpened: variantGrid.forceActiveFocus()
        onClosed: grid.forceActiveFocus()

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.line
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 10
            Text {
                Layout.fillWidth: true
                text: qsTr("Choose a skin tone")
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: 18
                font.weight: Font.Bold
            }
            Text {
                Layout.fillWidth: true
                text: variantPopup.baseName
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            GridView {
                id: variantGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: variantPopup.choices
                cellWidth: 80
                cellHeight: 80
                clip: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                delegate: EmojiTile {
                    required property var modelData
                    width: variantGrid.cellWidth - 8
                    height: variantGrid.cellHeight - 8
                    focus: GridView.isCurrentItem
                    symbol: modelData.symbol
                    emojiName: modelData.emojiName
                    variantCount: 0
                    renderable: modelData.renderable
                    onPicked: symbol => {
                        variantPopup.close()
                        view.controller.copyEmoji(symbol)
                    }
                }
            }
        }
    }

    Shortcut {
        sequences: ["Ctrl+F"]
        onActivated: search.forceActiveFocus()
    }
}
