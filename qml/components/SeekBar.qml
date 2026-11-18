import QtQuick
import Andub

// Video timeline: buffered range, played range, hover time preview, drag to scrub.
Item {
    id: bar
    property real position: 0
    property real duration: 0
    property real buffered: 0
    readonly property bool dragging: mouse.pressed
    readonly property bool active: mouse.containsMouse || mouse.pressed
    property real dragValue: 0
    signal seekRequested(real seconds)

    implicitHeight: 24

    function valueAt(x) {
        return duration > 0 ? Math.max(0, Math.min(1, x / width)) * duration : 0
    }
    readonly property real shown: dragging ? dragValue : position

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: bar.active ? 6 : 4
        radius: height / 2
        color: Qt.rgba(1, 1, 1, 0.18)
        Behavior on height { NumberAnimation { duration: Theme.durFast } }

        Rectangle {
            height: parent.height
            radius: parent.radius
            color: Qt.rgba(1, 1, 1, 0.3)
            width: bar.duration > 0 ? Math.min(1, bar.buffered / bar.duration) * parent.width : 0
        }
        Rectangle {
            height: parent.height
            radius: parent.radius
            color: Theme.accent
            width: bar.duration > 0 ? Math.min(1, bar.shown / bar.duration) * parent.width : 0
        }
    }
    Rectangle {
        width: 14
        height: 14
        radius: 7
        color: "#ffffff"
        anchors.verticalCenter: parent.verticalCenter
        x: (bar.duration > 0 ? Math.min(1, bar.shown / bar.duration) * bar.width : 0) - width / 2
        scale: bar.active ? 1 : 0
        Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Easing.OutBack } }
    }
    Rectangle {
        visible: mouse.containsMouse && bar.duration > 0
        y: -height - 8
        x: Math.max(0, Math.min(bar.width - width, mouse.mouseX - width / 2))
        width: tipText.implicitWidth + 14
        height: 24
        radius: Theme.radiusSm
        color: Qt.rgba(0.08, 0.08, 0.09, 0.9)
        Text {
            id: tipText
            anchors.centerIn: parent
            text: Theme.formatTime(bar.valueAt(mouse.mouseX))
            color: "#f2f2f4"
            font.pixelSize: Theme.fontXs
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
        }
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.topMargin: -6
        anchors.bottomMargin: -6
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: e => bar.dragValue = bar.valueAt(e.x)
        onPositionChanged: e => { if (pressed) bar.dragValue = bar.valueAt(e.x) }
        onReleased: e => bar.seekRequested(bar.valueAt(e.x))
    }
}
