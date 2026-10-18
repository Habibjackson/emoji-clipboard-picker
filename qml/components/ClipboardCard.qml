import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

T.Control {
    id: card

    required property var entryId
    required property string kind
    required property string previewText
    required property string imageSource
    required property bool pinned
    required property bool canPin
    required property string timeText

    signal copyRequested()
    signal pinRequested(bool pinned)
    signal deleteRequested()

    implicitHeight: Theme.layoutSize(kind === "image" ? 110 : 76)
    padding: Theme.layoutSize(8)
    hoverEnabled: true
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: kind === "image" ? qsTr("Image saved %1").arg(timeText) : previewText
    Accessible.description: qsTr("Press Enter to copy")
    Keys.onReturnPressed: card.copyRequested()
    Keys.onEnterPressed: card.copyRequested()

    background: Rectangle {
        radius: Theme.smallRadius
        color: card.hovered ? Theme.surfaceHover : Theme.surface
        border.color: card.visualFocus ? Theme.accent : Theme.line
        border.width: card.visualFocus ? 2 : 1
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.layoutSize(3)

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.layoutSize(5)

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Image {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.min(parent.width, Theme.layoutSize(84))
                    visible: card.kind === "image"
                    source: card.imageSource
                    sourceSize.width: 192
                    sourceSize.height: 192
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: Image.AlignLeft
                    verticalAlignment: Image.AlignVCenter
                    asynchronous: true
                    cache: false
                    Accessible.ignored: true
                }

                Text {
                    anchors.fill: parent
                    visible: card.kind !== "image"
                    text: card.previewText
                    textFormat: Text.PlainText
                    color: Theme.ink
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(12)
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Accessible.ignored: true
                }

                TapHandler {
                    onTapped: card.copyRequested()
                }
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }

            IconButton {
                iconName: card.pinned ? "unpin" : "pin"
                enabled: card.pinned || card.canPin
                accessibilityLabel: card.pinned ? qsTr("Unpin item")
                                                 : card.canPin ? qsTr("Pin item")
                                                               : qsTr("All five pins are in use")
                onClicked: card.pinRequested(!card.pinned)
            }
            IconButton {
                iconName: "delete"
                accessibilityLabel: qsTr("Delete item")
                onClicked: card.deleteRequested()
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.fontSize(14)
            text: card.timeText
            textFormat: Text.PlainText
            color: Theme.muted
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.fontSize(10)
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Accessible.ignored: true
        }
    }
}
