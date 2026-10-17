import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    property bool selected: false
    property string categoryName: ""

    text: categoryName
    implicitHeight: Math.max(44, Theme.fontSize(40))
    implicitWidth: Theme.fontSize(156)
    hoverEnabled: true
    Accessible.name: categoryName
    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()

    contentItem: Text {
        text: control.text
        color: control.selected ? Theme.accent : Theme.muted
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.fontSize(13)
        font.weight: control.selected ? Font.DemiBold : Font.Medium
        verticalAlignment: Text.AlignVCenter
        leftPadding: Theme.fontSize(14)
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
