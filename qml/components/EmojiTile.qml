import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    required property string symbol
    required property string emojiName

    signal picked(string symbol)

    implicitWidth: 72
    implicitHeight: 72
    hoverEnabled: true
    Accessible.name: emojiName
    onClicked: picked(symbol)

    contentItem: Column {
        spacing: 0
        Text {
            width: parent.width
            height: 48
            text: control.symbol
            font.family: Theme.emojiFamily
            font.pixelSize: 29
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
        }
        Text {
            width: parent.width
            height: 18
            text: control.emojiName
            textFormat: Text.PlainText
            color: Theme.muted
            font.family: Theme.bodyFamily
            font.pixelSize: 9
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: Theme.smallRadius
        color: control.down ? Theme.accentSoft
                            : control.hovered ? Theme.surfaceHover : Theme.surface
        border.color: control.activeFocus ? Theme.accent : Theme.line
        border.width: control.activeFocus ? 2 : 1
    }

    T.ToolTip {
        visible: control.hovered
        delay: 600
        text: control.emojiName
    }
}
