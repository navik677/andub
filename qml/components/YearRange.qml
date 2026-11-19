import QtQuick
import QtQuick.Templates as T
import Andub

// Two-handle year range. 0 at either end means "no bound": the handle sits at
// the edge and the label reads "будь-який". lower/upper are inputs (bind them);
// edited() fires when a handle is released.
Item {
    id: root
    property int from: 1970
    property int to: 2027
    property int lower: 0
    property int upper: 0
    signal edited(int lower, int upper)

    implicitHeight: 70

    // Live values while dragging
    property int dragLower: lower
    property int dragUpper: upper
    readonly property bool dragging: slider.first.pressed || slider.second.pressed
    readonly property int shownLower: dragging ? dragLower : lower
    readonly property int shownUpper: dragging ? dragUpper : upper

    function sync() {
        slider.setValues(lower > 0 ? lower : from, upper > 0 ? upper : to)
    }
    onLowerChanged: if (!dragging) sync()
    onUpperChanged: if (!dragging) sync()
    Component.onCompleted: sync()

    Text {
        id: caption
        anchors.left: parent.left
        anchors.top: parent.top
        color: Theme.text
        font.pixelSize: Theme.fontMd
        font.weight: Font.DemiBold
        text: {
            const l = root.shownLower, u = root.shownUpper
            if (l === 0 && u === 0) return "Будь-який рік"
            if (l > 0 && u > 0) return l === u ? l + " рік" : l + " – " + u
            return l > 0 ? "з " + l + " року" : "до " + u + " року"
        }
    }

    T.RangeSlider {
        id: slider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: caption.bottom
        anchors.topMargin: 8
        height: 32
        from: root.from
        to: root.to
        stepSize: 1
        snapMode: T.RangeSlider.SnapAlways

        first.onMoved: root.dragLower = Math.round(first.value) <= root.from ? 0 : Math.round(first.value)
        second.onMoved: root.dragUpper = Math.round(second.value) >= root.to ? 0 : Math.round(second.value)
        first.onPressedChanged: {
            if (first.pressed) { root.dragLower = root.lower; root.dragUpper = root.upper }
            else root.edited(root.dragLower, root.dragUpper)
        }
        second.onPressedChanged: {
            if (second.pressed) { root.dragLower = root.lower; root.dragUpper = root.upper }
            else root.edited(root.dragLower, root.dragUpper)
        }

        background: Item {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - 2
            width: slider.availableWidth
            height: 4
            Rectangle { anchors.fill: parent; radius: 2; color: Theme.surfaceHi2 }
            Rectangle {
                x: slider.first.position * parent.width
                width: (slider.second.position - slider.first.position) * parent.width
                height: parent.height
                radius: 2
                color: (root.shownLower > 0 || root.shownUpper > 0) ? Theme.accent : Theme.textFaint
            }
        }

        first.handle: Rectangle {
            x: slider.leftPadding + slider.first.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 18
            implicitHeight: 18
            radius: 9
            color: Theme.text
            border.width: slider.first.pressed || slider.first.hovered ? 4 : 0
            border.color: Theme.accentSoft
            Behavior on border.width { NumberAnimation { duration: Theme.durFast } }
        }
        second.handle: Rectangle {
            x: slider.leftPadding + slider.second.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 18
            implicitHeight: 18
            radius: 9
            color: Theme.text
            border.width: slider.second.pressed || slider.second.hovered ? 4 : 0
            border.color: Theme.accentSoft
            Behavior on border.width { NumberAnimation { duration: Theme.durFast } }
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: slider.bottom
        height: 16
        Text { anchors.left: parent.left; text: root.from; color: Theme.textFaint; font.pixelSize: Theme.fontXs }
        Text { anchors.right: parent.right; text: root.to; color: Theme.textFaint; font.pixelSize: Theme.fontXs }
    }
}
