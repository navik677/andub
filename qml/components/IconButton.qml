import QtQuick
import QtQuick.Templates as T
import Andub

// Round icon-only button with tooltip
T.Button {
    id: control
    property string iconName
    property string tip
    property int size: 36
    property int iconSize: 20
    property color iconColor: Theme.text
    property color hoverColor: Theme.hover
    property bool filled: false

    implicitWidth: size
    implicitHeight: size
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.35
    scale: down ? 0.92 : 1
    Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Easing.OutCubic } }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.iconColor
        }
    }
    background: Rectangle {
        radius: width / 2
        color: control.filled ? (control.hovered ? Theme.surfaceHi2 : Theme.surfaceHi)
                              : (control.hovered || control.down ? control.hoverColor : "transparent")
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
    AppToolTip { visible: control.tip !== "" && control.hovered; text: control.tip }
}
