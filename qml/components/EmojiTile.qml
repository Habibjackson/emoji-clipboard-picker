import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    required property string symbol
    required property string emojiName
    required property int variantCount
    required property bool renderable

    signal picked(string symbol)
    signal variantsRequested(string symbol, string emojiName)

    implicitWidth: 72
    implicitHeight: 72
    hoverEnabled: true
    Accessible.name: variantCount > 0
                     ? qsTr("%1, choose from %2 skin tone options").arg(emojiName).arg(variantCount + 1)
                     : emojiName
    Accessible.description: renderable ? "" : qsTr("Emoji glyph unavailable. Selecting still copies the emoji.")
    onClicked: {
        if (variantCount > 0)
            variantsRequested(symbol, emojiName)
        else
            picked(symbol)
    }

    contentItem: Column {
        spacing: 0
        Item {
            width: parent.width
            height: 48
            Text {
                anchors.fill: parent
                visible: control.renderable
                text: control.symbol
                font.pixelSize: 29
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                renderType: Text.NativeRendering
                Accessible.ignored: true
            }
            Text {
                anchors.fill: parent
                visible: !control.renderable
                text: qsTr("No glyph")
                color: Theme.accent
                font.family: Theme.bodyFamily
                font.pixelSize: 11
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Accessible.ignored: true
            }
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
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: Theme.smallRadius
        color: control.down ? Theme.accentSoft
                            : control.hovered ? Theme.surfaceHover : Theme.surface
        border.color: control.activeFocus ? Theme.accent : Theme.line
        border.width: control.activeFocus ? 2 : 1

        Text {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 3
            visible: control.variantCount > 0
            text: "▾"
            color: Theme.accent
            font.pixelSize: 12
            Accessible.ignored: true
        }
    }

    T.ToolTip {
        visible: control.hovered
        delay: 600
        text: control.variantCount > 0
              ? qsTr("Choose %1 or a skin tone").arg(control.emojiName)
              : control.emojiName
    }
}
