import QtQuick
import QtTest
import EmojiKeyboard

TestCase {
    id: testCase
    name: "PickerToolTip"
    width: 320
    height: 240
    when: windowShown

    Item {
        id: anchor
        x: 20
        y: 20
        width: 44
        height: 44

        PickerToolTip {
            id: tip
            anchorItem: anchor
            text: "Pin item"
            requested: false
        }
    }

    function init() {
        tip.requested = false
        tryCompare(tip, "visible", false)
        anchor.x = 20
        anchor.y = 20
    }

    function test_delayedOpenAndClose() {
        tip.requested = true
        compare(tip.visible, false)
        tryCompare(tip, "visible", true, 1000)
        verify(tip.width > 0 && tip.height > 0)
        tip.requested = false
        tryCompare(tip, "visible", false, 1000)
    }

    function test_keepsTipInsideWindow() {
        anchor.x = 276
        anchor.y = 188
        tip.requested = true
        tryCompare(tip, "visible", true, 1000)
        verify(tip.x + anchor.x >= 0)
        verify(tip.x + anchor.x + tip.width <= testCase.width,
               "tip=" + tip.x + ", width=" + tip.width + ", host="
               + tip.hostWindow.width + ", anchor=" + tip.anchorPosition.x)
        verify(tip.y < 0)
    }
}
