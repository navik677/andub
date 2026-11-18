import QtQuick
import QtQuick.Layouts
import Andub

ColumnLayout {
    id: root
    property string iconName: "film-slate"
    property string title
    property string message
    property string actionText
    signal action()

    spacing: 10
    width: Math.min(parent ? parent.width - 48 : 420, 420)

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: 6
        Layout.preferredWidth: 64
        Layout.preferredHeight: 64
        radius: 32
        color: Theme.surfaceHi
        Icon { anchors.centerIn: parent; name: root.iconName; size: 28; color: Theme.textMuted }
    }
    Text {
        Layout.fillWidth: true
        text: root.title
        visible: text !== ""
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.text
        font.pixelSize: Theme.fontLg
        font.weight: Font.DemiBold
    }
    Text {
        Layout.fillWidth: true
        text: root.message
        visible: text !== ""
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.textMuted
        font.pixelSize: Theme.fontMd
        lineHeight: 1.35
    }
    AppButton {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8
        visible: root.actionText !== ""
        text: root.actionText
        onClicked: root.action()
    }
}
