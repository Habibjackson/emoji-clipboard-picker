import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    property bool selected: false
    property bool prominent: false
    property string accessibilityLabel: text

    implicitHeight: 44
    implicitWidth: Math.max(72, label.implicitWidth + 28)
    padding: 10
    hoverEnabled: true
    font.family: Theme.bodyFamily
    font.pixelSize: 13
    Accessible.name: accessibilityLabel

    contentItem: Text {
        id: label
        text: control.text
        color: !control.enabled ? Theme.muted
                               : control.prominent ? Theme.accentInk
                                 : control.selected ? Theme.accent : Theme.ink
        font.family: Theme.bodyFamily
        font.pixelSize: 13
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
