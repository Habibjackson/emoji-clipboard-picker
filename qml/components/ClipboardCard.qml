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

    implicitHeight: Theme.fontSize(kind === "image" ? 78 : 70)
    padding: Theme.fontSize(8)
    hoverEnabled: true
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: kind === "image" ? qsTr("Image, %1").arg(previewText) : previewText
    Accessible.description: qsTr("Press Enter to copy")
    Keys.onReturnPressed: card.copyRequested()
    Keys.onEnterPressed: card.copyRequested()

    background: Rectangle {
        radius: Theme.smallRadius
        color: card.hovered ? Theme.surfaceHover : Theme.surface
        border.color: card.activeFocus ? Theme.accent : Theme.line
        border.width: card.activeFocus ? 2 : 1
    }

    contentItem: RowLayout {
        spacing: Theme.fontSize(5)

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                anchors.fill: parent
                spacing: Theme.fontSize(8)

                Rectangle {
                    Layout.preferredWidth: Theme.fontSize(54)
                    Layout.fillHeight: true
                    visible: card.kind === "image"
                    radius: Theme.smallRadius
                    color: Theme.rail

                    Image {
                        anchors.fill: parent
                        anchors.margins: Theme.fontSize(4)
                        source: card.imageSource
                        sourceSize.width: 108
                        sourceSize.height: 108
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.fontSize(2)

                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: card.previewText
                        textFormat: Text.PlainText
                        color: Theme.ink
                        font.family: Theme.bodyFamily
                        font.pixelSize: Theme.fontSize(12)
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        text: card.timeText
                        color: Theme.muted
                        font.family: Theme.bodyFamily
                        font.pixelSize: Theme.fontSize(10)
                    }
                }
            }

            TapHandler {
                onTapped: card.copyRequested()
            }
            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
        }

        ActionButton {
            Layout.preferredWidth: Theme.fontSize(card.pinned ? 57 : 45)
            Layout.preferredHeight: Theme.fontSize(44)
            dense: true
            text: card.pinned ? qsTr("Unpin") : qsTr("Pin")
            enabled: card.pinned || card.canPin
            accessibilityLabel: card.pinned ? qsTr("Unpin item")
                                             : card.canPin ? qsTr("Pin item")
                                                           : qsTr("All five pins are in use")
            onClicked: card.pinRequested(!card.pinned)
        }
        ActionButton {
            Layout.preferredWidth: Theme.fontSize(54)
            Layout.preferredHeight: Theme.fontSize(44)
            dense: true
            text: qsTr("Delete")
            accessibilityLabel: qsTr("Delete item")
            onClicked: card.deleteRequested()
        }
    }
}
