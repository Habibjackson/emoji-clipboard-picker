pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

RowLayout {
    id: recorder

    required property ShortcutSettings settings
    required property string viewName
    required property string title
    property bool recording: false
    readonly property string shortcutLabel: viewName === "emoji"
                                            ? settings.emojiLabel : settings.clipboardLabel

    function focusShortcut() {
        keyButton.forceActiveFocus()
    }

    spacing: Theme.layoutSize(8)

    Text {
        Layout.fillWidth: true
        text: recorder.title
        color: Theme.ink
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.fontSize(12)
        font.weight: Font.Medium
    }

    T.Button {
        id: keyButton
        Layout.preferredWidth: Theme.layoutSize(160)
        Layout.preferredHeight: Theme.layoutSize(38)
        enabled: recorder.settings.available
        hoverEnabled: true
        Accessible.name: qsTr("Set %1 shortcut, current: %2")
                         .arg(recorder.title).arg(recorder.shortcutLabel)
        Accessible.description: qsTr("Press Super and a key, or Ctrl+Alt and a key")
        onClicked: {
            recorder.settings.clearError()
            recorder.recording = true
            forceActiveFocus()
        }
        onActiveFocusChanged: {
            if (!activeFocus)
                recorder.recording = false
        }
        Keys.onPressed: function(event) {
            if (!recorder.recording)
                return
            event.accepted = true
            if (event.key === Qt.Key_Escape) {
                recorder.recording = false
                recorder.settings.clearError()
                return
            }
            if (event.key === Qt.Key_Meta || event.key === Qt.Key_Super_L
                || event.key === Qt.Key_Super_R || event.key === Qt.Key_Control
                || event.key === Qt.Key_Alt || event.key === Qt.Key_Shift)
                return
            if (recorder.settings.assignKey(recorder.viewName, event.key, event.modifiers))
                recorder.recording = false
        }

        contentItem: Text {
            text: recorder.recording ? qsTr("Press keys…") : recorder.shortcutLabel
            textFormat: Text.PlainText
            color: recorder.recording ? Theme.accent : Theme.ink
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.fontSize(11)
            font.weight: recorder.recording ? Font.DemiBold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.smallRadius
            color: recorder.recording ? Theme.accentSoft
                                      : keyButton.hovered ? Theme.surfaceHover : Theme.rail
            border.color: recorder.recording || keyButton.visualFocus
                          ? Theme.accent : Theme.line
            border.width: 1
            Behavior on color { ColorAnimation { duration: 130 } }
        }
        PickerToolTip {
            anchorItem: keyButton
            requested: keyButton.hovered && !recorder.recording
            text: qsTr("Click, then press a new shortcut")
        }
    }
}
