import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    required property string iconName
    required property string accessibilityLabel
    property string toolTipText: accessibilityLabel
    property bool selected: false
    property bool prominent: false
    property bool toolTipDismissed: false
    property bool toolTipBlocked: false

    implicitWidth: Theme.layoutSize(44)
    implicitHeight: Theme.layoutSize(44)
    padding: Theme.layoutSize(9)
    hoverEnabled: true
    Accessible.name: accessibilityLabel
    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()
    onHoveredChanged: {
        if (!hovered && !toolTipBlocked)
            toolTipDismissed = false
    }
    onActiveFocusChanged: {
        if (!activeFocus && !hovered && !toolTipBlocked)
            toolTipDismissed = false
    }
    onToolTipBlockedChanged: {
        if (!toolTipBlocked && !hovered)
            toolTipDismissed = false
    }

    Connections {
        target: control
        function onClicked() { control.toolTipDismissed = true }
    }

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
        border.color: control.visualFocus ? Theme.accent : "transparent"
        border.width: control.visualFocus ? 2 : 0
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    PickerToolTip {
        anchorItem: control
        requested: control.enabled && !control.toolTipDismissed && !control.toolTipBlocked
                   && (control.hovered || control.visualFocus)
        text: control.toolTipText
    }
}
