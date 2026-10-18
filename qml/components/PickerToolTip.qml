import QtQuick
import QtQuick.Templates as T

T.Popup {
    id: tip

    required property Item anchorItem
    required property string text
    property bool requested: false
    readonly property var hostWindow: anchorItem ? anchorItem.Window.window : null
    property point anchorPosition: Qt.point(0, 0)

    parent: anchorItem
    width: Math.min(Theme.layoutSize(240), tipLabel.implicitWidth + 2 * padding)
    height: tipLabel.implicitHeight + 2 * padding
    x: hostWindow ? Math.max(Theme.layoutSize(6) - anchorPosition.x,
                             Math.min((anchorItem.width - width) / 2,
                                      hostWindow.width - Theme.layoutSize(6)
                                      - anchorPosition.x - width))
                  : 0
    y: hostWindow && anchorPosition.y + anchorItem.height + height
       + Theme.layoutSize(8) > hostWindow.height
       ? -height - Theme.layoutSize(4) : anchorItem.height + Theme.layoutSize(4)
    padding: Theme.layoutSize(7)
    focus: false
    modal: false
    closePolicy: T.Popup.NoAutoClose

    onRequestedChanged: {
        if (requested) {
            anchorPosition = anchorItem.mapToItem(null, 0, 0)
            showTimer.restart()
        } else {
            showTimer.stop()
            close()
        }
    }

    Timer {
        id: showTimer
        interval: 450
        repeat: false
        onTriggered: {
            if (tip.requested) {
                tip.anchorPosition = tip.anchorItem.mapToItem(null, 0, 0)
                tip.open()
            }
        }
    }

    background: Rectangle {
        radius: Theme.smallRadius
        color: Theme.ink
        border.color: Theme.line
        border.width: 1
    }
    contentItem: Text {
        id: tipLabel
        text: tip.text
        textFormat: Text.PlainText
        color: Theme.canvas
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.fontSize(11)
        wrapMode: Text.Wrap
    }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 }
    }
}
