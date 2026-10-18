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
    Behavior on color {
        ColorAnimation { duration: 150 }
    }

    function pathFor(value) {
        switch (value) {
        case "emoji":
            return "M12 3.5 A8.5 8.5 0 1 1 12 20.5 A8.5 8.5 0 1 1 12 3.5 M8 14 C9.1 16 10.4 17 12 17 C13.6 17 14.9 16 16 14"
        case "clipboard":
            return "M8 4 H6 A2 2 0 0 0 4 6 V19 A2 2 0 0 0 6 21 H18 A2 2 0 0 0 20 19 V6 A2 2 0 0 0 18 4 H16 M8 4 H9 A3 3 0 0 1 15 4 H16 V7 H8 Z M8 11 H16 M8 15 H16 M8 18 H13"
        case "pin":
            return "M12 13 V21"
        case "unpin":
            return "M12 13 V21 M4 4 L20 20"
        case "delete":
            return "M4 7 H20 M9 7 V4 H15 V7 M6 7 L7 19 A2 2 0 0 0 9 21 H15 A2 2 0 0 0 17 19 L18 7 M10 11 V17 M14 11 V17"
        case "menu":
            return ""
        case "close":
            return "M5 5 L19 19 M19 5 L5 19"
        case "system":
            return "M4 5 H20 V17 H4 Z M8 21 H16 M12 17 V21"
        case "light":
            return "M12 8 A4 4 0 1 1 12 16 A4 4 0 1 1 12 8 M12 2 V4 M12 20 V22 M4.9 4.9 L6.3 6.3 M17.7 17.7 L19.1 19.1 M2 12 H4 M20 12 H22 M4.9 19.1 L6.3 17.7 M17.7 6.3 L19.1 4.9"
        case "dark":
            return ""
        case "capture":
            return "M9 4 H6 A2 2 0 0 0 4 6 V19 A2 2 0 0 0 6 21 H17 A2 2 0 0 0 19 19 V12 M9 4 H10 A2 2 0 0 1 14 4 H15 V7 H9 Z M17 3 V9 M14 6 H20"
        default:
            return ""
        }
    }

    function fillFor(value) {
        switch (value) {
        case "emoji":
            return "M9 8.5 A1.1 1.1 0 1 1 9 10.7 A1.1 1.1 0 1 1 9 8.5 M15 8.5 A1.1 1.1 0 1 1 15 10.7 A1.1 1.1 0 1 1 15 8.5"
        case "pin":
        case "unpin":
            return "M9 3.5 H15 L14.4 8 L17 11 V13 H7 V11 L9.6 8 Z"
        case "menu":
            return "M5 10 A2 2 0 1 1 5 14 A2 2 0 1 1 5 10 M12 10 A2 2 0 1 1 12 14 A2 2 0 1 1 12 10 M19 10 A2 2 0 1 1 19 14 A2 2 0 1 1 19 10"
        case "dark":
            return "M20.4 15.2 A8.5 8.5 0 0 1 8.8 3.6 A8.5 8.5 0 1 0 20.4 15.2 Z"
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
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: glyph.pathFor(glyph.name) }
        }
        ShapePath {
            strokeColor: "transparent"
            fillColor: glyph.color
            PathSvg { path: glyph.fillFor(glyph.name) }
        }
    }
}
