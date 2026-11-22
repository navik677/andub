import QtQuick
import QtQuick.Templates as T
import Andub

T.TextField {
    id: control
    // compact: toolbar field next to a row of chips (raised, with a key hint)
    property bool compact: false
    property string shortcutHint: ""

    implicitHeight: compact ? 38 : 42
    implicitWidth: 360
    leftPadding: compact ? 38 : 42
    rightPadding: text.length > 0 ? 40 : (hint.visible ? hint.width + 20 : 14)
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.accentSoft
    selectedTextColor: Theme.text
    font.pixelSize: compact ? Theme.fontSm + 1 : Theme.fontMd
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true

    background: Rectangle {
        radius: Theme.radius + (control.compact ? 2 : 0)
        color: control.compact ? (Theme.dark ? Theme.surfaceHi : Theme.surface)
                               : (control.activeFocus ? Theme.surface : Theme.surfaceHi)
        border.width: control.activeFocus ? 1.5 : 1
        border.color: control.activeFocus ? Theme.accent
                      : (control.hovered ? Theme.lineStrong : (control.compact ? Theme.lineStrong : Theme.line))
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

        // Soft drop shadow so the field sits above the backdrop (compact only)
        Rectangle {
            visible: control.compact
            z: -1
            x: 0
            y: 2
            width: parent.width
            height: parent.height
            radius: parent.radius
            color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.07)
        }
        // Focus ring
        Rectangle {
            visible: control.activeFocus
            z: -1
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.width: 4
            border.color: Theme.accentSoft
        }

        Icon {
            x: control.compact ? 13 : 14
            anchors.verticalCenter: parent.verticalCenter
            name: "magnifying-glass"
            size: control.compact ? 17 : 18
            color: control.activeFocus ? Theme.accent : Theme.textMuted
            Behavior on color { ColorAnimation { duration: Theme.durFast } }
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
        // Keyboard shortcut hint, like "Ctrl K"
        Rectangle {
            id: hint
            visible: control.shortcutHint !== "" && control.text.length === 0 && !control.activeFocus
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            width: hintText.implicitWidth + 12
            height: 22
            radius: 5
            color: Theme.dark ? Theme.surfaceHi2 : Theme.surfaceHi
            border.width: 1
            border.color: Theme.line
            Text {
                id: hintText
                anchors.centerIn: parent
                text: control.shortcutHint
                color: Theme.textFaint
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
            }
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
