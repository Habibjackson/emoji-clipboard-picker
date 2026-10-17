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
    readonly property bool narrow: width < Theme.fontSize(360)

    signal copyRequested()
    signal pinRequested(bool pinned)
    signal deleteRequested()

    implicitHeight: (kind === "image" ? Theme.fontSize(76) : Theme.fontSize(50))
                    + (narrow ? Theme.fontSize(88) : Theme.fontSize(44))
                    + Theme.fontSize(32)
    padding: Theme.fontSize(12)
    activeFocusOnTab: true
    Accessible.name: kind === "image" ? qsTr("Image, %1").arg(previewText) : previewText
    Accessible.description: qsTr("Press Enter to copy")
    Keys.onReturnPressed: card.copyRequested()
    Keys.onEnterPressed: card.copyRequested()

    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.color: card.activeFocus ? Theme.accent : Theme.line
        border.width: card.activeFocus ? 2 : 1
    }

    contentItem: ColumnLayout {
        spacing: Theme.fontSize(8)

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: card.kind === "image" ? Theme.fontSize(76)
                                                           : Theme.fontSize(50)
            spacing: Theme.spacing

            Rectangle {
                Layout.preferredWidth: Theme.fontSize(84)
                Layout.fillHeight: true
                radius: Theme.smallRadius
                color: Theme.surfaceHover
                visible: card.kind === "image"

                Image {
                    anchors.fill: parent
                    anchors.margins: Theme.fontSize(4)
                    source: card.imageSource
                    sourceSize.width: 160
                    sourceSize.height: 100
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.fontSize(4)

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: card.previewText
                    textFormat: Text.PlainText
                    color: Theme.ink
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(13)
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    text: card.timeText
                    color: Theme.muted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.fontSize(11)
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: card.narrow ? 2 : 3
            columnSpacing: Theme.fontSize(4)
            rowSpacing: Theme.fontSize(4)

            ActionButton {
                Layout.fillWidth: true
                text: qsTr("Copy")
                prominent: true
                onClicked: card.copyRequested()
            }
            ActionButton {
                Layout.fillWidth: true
                text: card.pinned ? qsTr("Unpin") : qsTr("Pin")
                enabled: card.pinned || card.canPin
                accessibilityLabel: card.pinned ? qsTr("Unpin item")
                                                 : card.canPin ? qsTr("Pin item")
                                                               : qsTr("All five pins are in use")
                onClicked: card.pinRequested(!card.pinned)
            }
            ActionButton {
                Layout.fillWidth: true
                text: qsTr("Delete")
                accessibilityLabel: qsTr("Delete item")
                onClicked: card.deleteRequested()
            }
        }
    }
}
