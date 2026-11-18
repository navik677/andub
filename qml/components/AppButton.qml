import QtQuick
import QtQuick.Templates as T
import Andub

// kind: "primary" (accent fill), "secondary" (neutral fill), "ghost" (no fill)
T.Button {
    id: control
    property string iconName
    property string kind: "secondary"
    property string tip
    property bool compact: false
    property color foreground: kind === "primary" ? Theme.onAccent : Theme.text

    implicitHeight: compact ? 34 : 40
    implicitWidth: Math.max(implicitHeight, implicitContentWidth + leftPadding + rightPadding)
    leftPadding: text ? (iconName ? (compact ? 11 : 14) : (compact ? 13 : 18)) : 0
    rightPadding: text ? (compact ? 13 : 18) : 0
    font.pixelSize: compact ? Theme.fontSm : Theme.fontMd
    font.weight: Font.DemiBold
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.4
    scale: down ? 0.97 : 1
    Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Easing.OutCubic } }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName !== ""
                name: control.iconName
                size: control.compact ? 16 : 18
                color: control.foreground
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.text !== ""
                text: control.text
                font: control.font
                color: control.foreground
            }
        }
    }

    background: Rectangle {
        radius: Theme.radius
        color: {
            if (control.kind === "primary")
                return control.down ? Qt.darker(Theme.accent, 1.12) : control.hovered ? Qt.lighter(Theme.accent, 1.07) : Theme.accent
            if (control.kind === "ghost")
                return control.down ? Theme.surfaceHi2 : control.hovered ? Theme.hover : "transparent"
            return control.down ? Theme.surfaceHi2 : control.hovered ? Theme.surfaceHi2 : Theme.surfaceHi
        }
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
    AppToolTip { visible: control.tip !== "" && control.hovered; text: control.tip }
}
