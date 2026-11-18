import QtQuick
import Andub

// Segmented control: model is a list of { label, value }
Rectangle {
    id: root
    property var model: []
    property var value
    signal activated(var value)

    implicitHeight: 38
    implicitWidth: row.implicitWidth + 8
    radius: Theme.radius + 2
    color: Theme.surfaceHi

    readonly property int currentIndex: {
        for (let i = 0; i < model.length; ++i) if (model[i].value === value) return i
        return -1
    }

    Rectangle {
        id: thumb
        visible: root.currentIndex >= 0
        y: 4
        height: parent.height - 8
        radius: Theme.radius
        color: Theme.dark ? Theme.surfaceHi2 : Theme.surface
        border.width: 1
        border.color: Theme.line
        property Item target: root.currentIndex >= 0 ? rep.itemAt(root.currentIndex) : null
        x: target ? target.x + 4 : 4
        width: target ? target.width : 0
        Behavior on x { NumberAnimation { duration: Theme.durMed; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Theme.durMed; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        x: 4
        anchors.verticalCenter: parent.verticalCenter
        Repeater {
            id: rep
            model: root.model
            delegate: Item {
                required property var modelData
                required property int index
                width: txt.implicitWidth + 28
                height: root.height - 8
                Text {
                    id: txt
                    anchors.centerIn: parent
                    text: modelData.label
                    font.pixelSize: Theme.fontSm
                    font.weight: index === root.currentIndex ? Font.DemiBold : Font.Medium
                    color: index === root.currentIndex ? Theme.text : Theme.textMuted
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activated(modelData.value)
                }
            }
        }
    }
}
