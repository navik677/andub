import QtQuick
import QtQuick.Templates as T
import Andub

T.ToolTip {
    id: tip
    delay: 450
    timeout: 4000
    margins: 8
    padding: 0
    topPadding: 6
    bottomPadding: 6
    leftPadding: 10
    rightPadding: 10
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    y: -implicitHeight - 6
    x: (parent ? (parent.width - implicitWidth) / 2 : 0)

    contentItem: Text {
        text: tip.text
        color: Theme.dark ? "#141518" : "#f4f4f6"
        font.pixelSize: Theme.fontXs
        font.weight: Font.Medium
    }
    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.dark ? "#ececee" : "#1c1d21"
    }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durFast } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durFast } }
}
