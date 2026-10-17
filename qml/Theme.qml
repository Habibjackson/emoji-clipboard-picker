pragma Singleton

import QtQuick

QtObject {
    readonly property bool dark: Application.styleHints.colorScheme === Qt.Dark
    readonly property color canvas: dark ? "#141D20" : "#F5F3ED"
    readonly property color surface: dark ? "#1C292C" : "#FFFFFF"
    readonly property color surfaceHover: dark ? "#27383B" : "#F0EEE8"
    readonly property color rail: dark ? "#172326" : "#EAE8E0"
    readonly property color ink: dark ? "#F4F3ED" : "#19383A"
    readonly property color muted: dark ? "#A8B8B8" : "#607677"
    readonly property color line: dark ? "#34484B" : "#D7DEDA"
    readonly property color accent: dark ? "#FF8A73" : "#C94F3B"
    readonly property color accentSoft: dark ? "#49332F" : "#F9E7E1"
    readonly property color accentInk: dark ? "#201B19" : "#FFFFFF"
    readonly property string bodyFamily: "Noto Sans"
    readonly property int radius: 14
    readonly property int smallRadius: 9
    readonly property int spacing: 12
}
