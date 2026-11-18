import QtQuick
import QtQuick.Layouts
import Andub

// Left navigation rail
Rectangle {
    id: rail
    property string current: "catalog"
    property int downloadsActive: 0
    signal navigate(string section)

    implicitWidth: 92
    color: Theme.dark ? Qt.darker(Theme.bg, 1.15) : Theme.surface

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.line
    }

    component NavItem: Item {
        id: item
        property string section
        property string iconName
        property string label
        property int badge: 0
        readonly property bool active: rail.current === section

        Layout.fillWidth: true
        Layout.preferredHeight: 62

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 6
            width: 52
            height: 32
            radius: 16
            color: item.active ? Theme.surfaceHi2 : (hover.hovered ? Theme.hover : "transparent")
            Behavior on color { ColorAnimation { duration: Theme.durFast } }

            Icon {
                anchors.centerIn: parent
                name: item.iconName + (item.active && (item.iconName === "heart") ? "-fill" : "")
                size: 21
                color: item.active ? Theme.text : Theme.textMuted
            }
            Rectangle {
                visible: item.badge > 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 6
                anchors.topMargin: -2
                width: Math.max(16, badgeText.implicitWidth + 8)
                height: 16
                radius: 8
                color: Theme.accent
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: item.badge
                    color: Theme.onAccent
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 42
            text: item.label
            color: item.active ? Theme.text : Theme.textMuted
            font.pixelSize: 11
            font.weight: item.active ? Font.DemiBold : Font.Medium
        }
        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: rail.navigate(item.section) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 18
        anchors.bottomMargin: 12
        spacing: 4

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 18
            text: "andub"
            color: Theme.text
            font.pixelSize: 17
            font.weight: Font.ExtraBold
            font.letterSpacing: -0.4
        }

        NavItem { section: "catalog"; iconName: "squares-four"; label: "Каталог" }
        NavItem { section: "favorites"; iconName: "heart"; label: "Улюблені" }
        NavItem { section: "downloads"; iconName: "download-simple"; label: "Завантаження"; badge: rail.downloadsActive }

        Item { Layout.fillHeight: true }

        NavItem { section: "settings"; iconName: "gear-six"; label: "Налаштування" }
    }
}
