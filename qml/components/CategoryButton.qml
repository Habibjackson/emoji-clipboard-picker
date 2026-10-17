import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    property bool selected: false
    property string categoryName: ""

    text: categoryName
    implicitHeight: 44
    implicitWidth: 156
    hoverEnabled: true
    Accessible.name: categoryName

    contentItem: Text {
        text: control.text
        color: control.selected ? Theme.accent : Theme.muted
        font.family: Theme.bodyFamily
        font.pixelSize: 13
        font.weight: control.selected ? Font.DemiBold : Font.Medium
        verticalAlignment: Text.AlignVCenter
        leftPadding: 14
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.smallRadius
        color: control.selected ? Theme.accentSoft
                                : control.down || control.hovered ? Theme.surfaceHover : "transparent"
        border.color: control.activeFocus ? Theme.accent : "transparent"
        border.width: control.activeFocus ? 2 : 0
    }
}
