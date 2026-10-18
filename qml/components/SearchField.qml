import QtQuick
import QtQuick.Templates as T

T.TextField {
    id: control

    implicitHeight: Math.max(46, Theme.layoutSize(42))
    leftPadding: Theme.fontSize(16)
    rightPadding: Theme.fontSize(16)
    color: Theme.ink
    selectionColor: Theme.accentSoft
    selectedTextColor: Theme.ink
    placeholderTextColor: Theme.muted
    font.family: Theme.bodyFamily
    font.pixelSize: Theme.fontSize(14)
    Accessible.name: placeholderText

    background: Rectangle {
        radius: Theme.smallRadius
        color: Theme.surface
        border.color: control.activeFocus ? Theme.accent : Theme.line
        border.width: control.activeFocus ? 2 : 1
        Behavior on color {
            ColorAnimation { duration: 180 }
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: control.leftPadding
        anchors.verticalCenter: parent.verticalCenter
        text: control.placeholderText
        color: Theme.muted
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.fontSize(14)
        visible: control.text.length === 0
        Accessible.ignored: true
    }
}
