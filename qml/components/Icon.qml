import QtQuick
import QtQuick.Controls.impl
import Andub

// Tinted Phosphor icon
Item {
    id: root
    property string name
    property int size: 20
    property color color: Theme.text
    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    IconImage {
        anchors.fill: parent
        source: root.name ? Theme.icon(root.name) : ""
        sourceSize.width: root.size
        sourceSize.height: root.size
        color: root.color
    }
}
