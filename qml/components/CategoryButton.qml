import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    property bool selected: false
    property string categoryName: ""

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
        border.color: control.activeFocus ? Theme.accent : "transparent"
        border.width: control.activeFocus ? 2 : 0
    }

    PickerToolTip {
        parent: control
        x: (control.width - width) / 2
        y: control.height + Theme.layoutSize(3)
        visible: control.hovered || control.activeFocus
        text: control.categoryName
    }
}
