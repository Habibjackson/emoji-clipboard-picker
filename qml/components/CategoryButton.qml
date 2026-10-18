import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    property bool selected: false
    property string categoryName: ""
    property bool toolTipDismissed: false

    function symbolFor(category) {
        switch (category) {
        case "All": return "🗂️"
        case "Recent": return "🕘"
        case "Smileys & Emotion": return "😀"
        case "People & Body": return "🧍"
        case "Component": return "🧩"
        case "Animals & Nature": return "🐾"
        case "Food & Drink": return "🍎"
        case "Travel & Places": return "✈️"
        case "Activities": return "⚽"
        case "Objects": return "💡"
        case "Symbols": return "🔣"
        case "Flags": return "🏁"
        default: return "❔"
        }
    }

    implicitHeight: Theme.layoutSize(44)
    implicitWidth: Theme.layoutSize(44)
    hoverEnabled: true
    Accessible.name: categoryName
    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()
    onHoveredChanged: {
        if (!hovered)
            toolTipDismissed = false
    }
    onActiveFocusChanged: {
        if (!activeFocus && !hovered)
            toolTipDismissed = false
    }

    Connections {
        target: control
        function onClicked() { control.toolTipDismissed = true }
    }

    contentItem: Text {
        id: label
        text: control.symbolFor(control.categoryName)
        font.pixelSize: Theme.fontSize(22)
        renderType: Text.NativeRendering
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.smallRadius
        color: control.selected ? Theme.accentSoft
                                : control.down || control.hovered ? Theme.surfaceHover : "transparent"
        border.color: control.visualFocus ? Theme.accent : "transparent"
        border.width: control.visualFocus ? 2 : 0
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    PickerToolTip {
        anchorItem: control
        requested: !control.toolTipDismissed && (control.hovered || control.visualFocus)
        text: control.categoryName
    }
}
