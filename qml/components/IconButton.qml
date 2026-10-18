import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    required property string iconName
    required property string accessibilityLabel
    property string toolTipText: accessibilityLabel
    property bool selected: false
    property bool prominent: false

    implicitWidth: Theme.layoutSize(44)
    implicitHeight: Theme.layoutSize(44)
    padding: Theme.layoutSize(9)
    hoverEnabled: true
    Accessible.name: accessibilityLabel
    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()

    contentItem: Item {
        implicitWidth: Theme.layoutSize(24)
        implicitHeight: Theme.layoutSize(24)

        GlyphIcon {
            anchors.centerIn: parent
            name: control.iconName
            color: !control.enabled ? Theme.muted
                                    : control.prominent ? Theme.accentInk
                                      : control.selected ? Theme.accent : Theme.ink
            iconSize: Theme.layoutSize(22)
            scale: control.down ? 0.88 : control.hovered ? 1.12 : 1
            Behavior on scale {
                NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
            }
        }
    }

    background: Rectangle {
        radius: Theme.smallRadius
        color: control.prominent ? Theme.accent
                                 : control.selected ? Theme.accentSoft
                                                    : control.down || control.hovered
                                                      ? Theme.surfaceHover : "transparent"
        border.color: control.activeFocus ? Theme.accent : "transparent"
        border.width: control.activeFocus ? 2 : 0
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    PickerToolTip {
        anchorItem: control
        requested: control.enabled && (control.hovered || control.activeFocus)
        text: control.toolTipText
    }
}
