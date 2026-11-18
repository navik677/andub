import QtQuick
import QtQuick.Templates as T
import Andub

// Pill toggle. Checked chips invert to text color instead of using the accent,
// which stays reserved for primary actions and progress.
T.Button {
    id: control
    checkable: true
    property bool small: false

    implicitHeight: small ? 28 : 34
    implicitWidth: label.implicitWidth + leftPadding + rightPadding
    leftPadding: small ? 11 : 15
    rightPadding: small ? 11 : 15
    font.pixelSize: small ? Theme.fontXs : Theme.fontSm
    font.weight: checked ? Font.DemiBold : Font.Medium
    focusPolicy: Qt.TabFocus

    contentItem: Text {
        id: label
        text: control.text
        font: control.font
        color: control.checked ? Theme.bg : (control.hovered ? Theme.text : Theme.textMuted)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }
    background: Rectangle {
        radius: height / 2
        color: control.checked ? Theme.text : (control.hovered ? Theme.surfaceHi2 : Theme.surfaceHi)
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }
    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
