import QtQuick
import QtQuick.Templates as T
import Andub

T.TextField {
    id: control
    implicitHeight: 42
    implicitWidth: 360
    leftPadding: 42
    rightPadding: text.length > 0 ? 40 : 14
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.accentSoft
    selectedTextColor: Theme.text
    font.pixelSize: Theme.fontMd
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true

    background: Rectangle {
        radius: Theme.radius
        color: control.activeFocus ? Theme.surface : Theme.surfaceHi
        border.width: control.activeFocus ? 1.5 : 1
        border.color: control.activeFocus ? Theme.accent : (control.hovered ? Theme.lineStrong : Theme.line)
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Icon {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            name: "magnifying-glass"
            size: 18
            color: control.activeFocus ? Theme.text : Theme.textFaint
        }
        // T.TextField has no built-in placeholder item
        Text {
            x: control.leftPadding
            anchors.verticalCenter: parent.verticalCenter
            visible: control.text.length === 0 && !control.preeditText
            text: control.placeholderText
            color: control.placeholderTextColor
            font: control.font
        }
        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            visible: control.text.length > 0
            size: 28
            iconSize: 14
            iconName: "x"
            iconColor: Theme.textMuted
            tip: "Очистити"
            onClicked: { control.clear(); control.forceActiveFocus() }
        }
    }
    Keys.onEscapePressed: if (text.length > 0) clear(); else focus = false
}
