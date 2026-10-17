import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    required property string symbol
    required property string emojiName
    required property int variantCount
    required property bool renderable
    property bool compact: false

    signal picked(string symbol)
    signal variantsRequested(string symbol, string emojiName)

    function activate() {
        if (variantCount > 0)
            variantsRequested(symbol, emojiName)
        else
            picked(symbol)
    }

    implicitWidth: Theme.fontSize(compact ? 54 : 72)
    implicitHeight: Theme.fontSize(compact ? 54 : 72)
    hoverEnabled: true
    Accessible.name: variantCount > 0
                     ? qsTr("%1, choose from %2 skin tone options").arg(emojiName).arg(variantCount + 1)
                     : emojiName
    Accessible.description: renderable ? "" : qsTr("Emoji glyph unavailable. Selecting still copies the emoji.")
    onClicked: activate()
    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onSpacePressed: activate()

    contentItem: Column {
        spacing: 0
        Item {
            width: parent.width
            height: control.compact ? control.availableHeight : Theme.fontSize(48)
            Text {
                anchors.fill: parent
                visible: control.renderable
                text: control.symbol
                font.pixelSize: Theme.fontSize(control.compact ? 30 : 29)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                renderType: Text.NativeRendering
                Accessible.ignored: true
            }
            Text {
                anchors.fill: parent
                anchors.margins: control.compact ? Theme.fontSize(4) : 0
                visible: !control.renderable
                text: control.compact ? control.emojiName : qsTr("No glyph")
                color: Theme.accent
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.fontSize(control.compact ? 10 : 11)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
                maximumLineCount: control.compact ? 3 : 1
                elide: Text.ElideRight
                Accessible.ignored: true
            }
        }
        Text {
            width: parent.width
            height: Theme.fontSize(18)
            visible: !control.compact
            text: control.emojiName
            textFormat: Text.PlainText
            color: Theme.muted
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.fontSize(9)
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
            anchors.margins: Theme.fontSize(3)
            visible: control.variantCount > 0
            text: "▾"
            color: Theme.accent
            font.pixelSize: Theme.fontSize(12)
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
