import QtQuick
import Andub

Item {
    id: s
    property real value: 100   // 0..100
    signal moved(real value)
    signal released()
    implicitWidth: 96
    implicitHeight: 24

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 4
        radius: 2
        color: Qt.rgba(1, 1, 1, 0.22)
        Rectangle {
            width: parent.width * s.value / 100
            height: parent.height
            radius: 2
            color: "#ffffff"
        }
    }
    Rectangle {
        width: 12; height: 12; radius: 6
        color: "#ffffff"
        anchors.verticalCenter: parent.verticalCenter
        x: s.width * s.value / 100 - width / 2
        scale: area.containsMouse || area.pressed ? 1 : 0
        Behavior on scale { NumberAnimation { duration: Theme.durFast } }
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        function update(x) { s.moved(Math.round(Math.max(0, Math.min(1, x / width)) * 100)) }
        onPressed: e => update(e.x)
        onPositionChanged: e => { if (pressed) update(e.x) }
        onReleased: s.released()
    }
}
