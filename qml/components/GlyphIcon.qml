import QtQuick
import QtQuick.Shapes

Item {
    id: glyph

    required property string name
    property color color: Theme.ink
    property int iconSize: 24

    implicitWidth: iconSize
    implicitHeight: iconSize
    Accessible.ignored: true

    function pathFor(value) {
        switch (value) {
        case "emoji":
            return "M12 3 A9 9 0 1 1 12 21 A9 9 0 1 1 12 3 M8.5 9.5 L8.6 9.5 M15.4 9.5 L15.5 9.5 M8 14 C9 16 10.5 17 12 17 C13.5 17 15 16 16 14"
        case "clipboard":
            return "M9 3 H15 V6 H9 Z M9 4 H6 A2 2 0 0 0 4 6 V20 A2 2 0 0 0 6 22 H18 A2 2 0 0 0 20 20 V6 A2 2 0 0 0 18 4 H15 M8 10 H16 M8 14 H16 M8 18 H14"
        case "pin":
            return "M9 3 H15 L14.5 8 L17 11 V13 H7 V11 L9.5 8 Z M12 13 V21"
        case "unpin":
            return "M9 3 H15 L14.5 8 L17 11 V13 H7 V11 L9.5 8 Z M12 13 V21 M4 4 L20 20"
        case "delete":
            return "M4 7 H20 M9 7 V4 H15 V7 M6 7 L7 20 H17 L18 7 M10 10 V17 M14 10 V17"
        case "menu":
            return "M5 12 L5.1 12 M12 12 L12.1 12 M19 12 L19.1 12"
        case "close":
            return "M5 5 L19 19 M19 5 L5 19"
        case "system":
            return "M4 5 H20 V17 H4 Z M8 21 H16 M12 17 V21"
        case "light":
            return "M12 8 A4 4 0 1 1 12 16 A4 4 0 1 1 12 8 M12 2 V4 M12 20 V22 M4.9 4.9 L6.3 6.3 M17.7 17.7 L19.1 19.1 M2 12 H4 M20 12 H22 M4.9 19.1 L6.3 17.7 M17.7 6.3 L19.1 4.9"
        case "dark":
            return "M20.4 15.2 A8.5 8.5 0 0 1 8.8 3.6 A8.5 8.5 0 1 0 20.4 15.2 Z"
        case "capture":
            return "M7 3 H17 V6 M4 6 H16 V19 H4 Z M9 10 H20 V21 H9 Z"
        default:
            return ""
        }
    }

    Shape {
        width: 24
        height: 24
        anchors.centerIn: parent
        scale: glyph.iconSize / 24

        ShapePath {
            strokeColor: glyph.color
            strokeWidth: 1.8
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: glyph.pathFor(glyph.name) }
        }
    }
}
