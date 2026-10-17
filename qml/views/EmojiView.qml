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
        spacing: Theme.fontSize(7)

        SearchField {
            id: search
            Layout.fillWidth: true
            text: view.controller.emojiModel.query
            placeholderText: qsTr("Search emoji")
            onTextChanged: view.controller.emojiModel.query = text
            KeyNavigation.tab: categories
        }

        ListView {
            id: categories
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.fontSize(44)
            orientation: ListView.Horizontal
            spacing: Theme.fontSize(5)
            clip: true
            model: view.controller.emojiModel.categories
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            KeyNavigation.backtab: search
            KeyNavigation.tab: grid

            delegate: CategoryButton {
                required property int index
                required property string modelData
                width: implicitWidth
                categoryName: modelData
                selected: view.controller.emojiModel.category === categoryName
                focus: ListView.isCurrentItem
                onClicked: {
                    categories.currentIndex = index
                    categories.positionViewAtIndex(index, ListView.Contain)
                    view.controller.emojiModel.category = categoryName
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.fontSize(20)

            Text {
                Layout.fillWidth: true
                text: view.controller.emojiModel.query.length > 0
                      ? qsTr("%1 search results").arg(view.controller.emojiModel.resultCount)
                      : qsTr("%1 emoji").arg(view.controller.emojiModel.resultCount)
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(11)
                elide: Text.ElideRight
            }
            Text {
                text: qsTr("▾ skin tones")
                color: Theme.accent
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(10)
            }
        }

        GridView {
            id: grid
            readonly property int columnCount: Math.max(4, Math.floor(width / Theme.fontSize(56)))

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: width / columnCount
            cellHeight: Theme.fontSize(58)
            model: view.controller.emojiModel
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            KeyNavigation.backtab: categories

            delegate: EmojiTile {
                width: grid.cellWidth - Theme.fontSize(4)
                height: grid.cellHeight - Theme.fontSize(4)
                compact: true
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
                width: parent.width - Theme.fontSize(24)
                visible: view.controller.emojiModel.resultCount === 0
                text: view.controller.emojiModel.category === "Recent"
                      ? qsTr("Your recently copied emoji will appear here.")
                      : qsTr("No emoji found. Try another name.")
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(13)
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
        width: Math.min(view.width - Theme.fontSize(16), Theme.fontSize(340))
        height: Math.min(view.height - Theme.fontSize(16),
                         Math.ceil(choices.length / 4) * Theme.fontSize(60)
                         + Theme.fontSize(90))
        padding: Theme.fontSize(14)
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
            spacing: Theme.fontSize(7)

            Text {
                Layout.fillWidth: true
                text: qsTr("Choose a skin tone")
                color: Theme.ink
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(17)
                font.weight: Font.Bold
            }
            Text {
                Layout.fillWidth: true
                text: variantPopup.baseName
                color: Theme.muted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(11)
                elide: Text.ElideRight
            }
            GridView {
                id: variantGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: variantPopup.choices
                cellWidth: width / 4
                cellHeight: Theme.fontSize(60)
                clip: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds

                delegate: EmojiTile {
                    required property var modelData
                    width: variantGrid.cellWidth - Theme.fontSize(4)
                    height: variantGrid.cellHeight - Theme.fontSize(4)
                    compact: true
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
