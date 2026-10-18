import QtQuick
import QtQuick.Templates as T

T.ToolTip {
    id: tip

    delay: 600
    timeout: 3000
    padding: Theme.layoutSize(7)
    margins: Theme.layoutSize(6)

    background: Rectangle {
        radius: Theme.smallRadius
        color: Theme.ink
    }
    contentItem: Text {
        text: tip.text
        textFormat: Text.PlainText
        color: Theme.canvas
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.fontSize(11)
    }
}
