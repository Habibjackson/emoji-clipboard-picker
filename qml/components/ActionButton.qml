import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    property bool selected: false
    property bool prominent: false
    property string accessibilityLabel: text

    implicitHeight: Math.max(44, Theme.fontSize(40))
    implicitWidth: Math.max(Theme.fontSize(72), label.implicitWidth + Theme.fontSize(28))
    padding: Theme.fontSize(10)
    hoverEnabled: true
    font.family: Theme.bodyFamily
    font.pixelSize: Theme.fontSize(13)
    Accessible.name: accessibilityLabel
    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()

    contentItem: Text {
        id: label
        text: control.text
        color: !control.enabled ? Theme.muted
                               : control.prominent ? Theme.accentInk
                                 : control.selected ? Theme.accent : Theme.ink
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.fontSize(13)
        font.weight: control.selected || control.prominent ? Font.DemiBold : Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.smallRadius
        color: control.prominent ? Theme.accent
                                 : control.selected ? Theme.accentSoft
                                                    : control.down || control.hovered ? Theme.surfaceHover
                                                                                      : "transparent"
        border.color: control.activeFocus ? Theme.accent : "transparent"
        border.width: control.activeFocus ? 2 : 0
    }
}
