import QtQuick
import QtQuick.Templates as T
import Andub

// Popup list for picking one value. model: list of { label, value, hint? }
T.Popup {
    id: popup
    property var model: []
    property var value
    property string title
    property int maxListHeight: 380
    signal picked(var value)

    padding: 6
    implicitWidth: 260
    implicitHeight: Math.min(column.implicitHeight, maxListHeight + 52) + topPadding + bottomPadding
    modal: false
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durFast }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.durMed; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durFast } }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.dark ? Theme.surfaceHi : Theme.surface
        border.width: 1
        border.color: Theme.lineStrong
    }

    contentItem: Column {
        id: column
        Text {
            visible: popup.title !== ""
            text: popup.title
            leftPadding: 12
            topPadding: 8
            bottomPadding: 8
            color: Theme.textMuted
            font.pixelSize: Theme.fontXs
            font.weight: Font.DemiBold
        }
        ListView {
            id: list
            width: parent.width
            height: Math.min(contentHeight, popup.maxListHeight)
            clip: true
            model: popup.model
            boundsBehavior: Flickable.StopAtBounds
            Component.onCompleted: positionViewAtIndex(Math.max(0, currentSel()), ListView.Contain)
            function currentSel() {
                for (let i = 0; i < popup.model.length; ++i) if (popup.model[i].value === popup.value) return i
                return -1
            }
            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property bool selected: modelData.value === popup.value
                width: list.width
                height: 38
                radius: Theme.radius
                color: rowHover.hovered ? Theme.hover : "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.right: check.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.label
                    color: Theme.text
                    font.pixelSize: Theme.fontMd
                    font.weight: row.selected ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }
                Icon {
                    id: check
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.selected
                    name: "check"
                    size: 16
                    color: Theme.accent
                }
                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        popup.picked(row.modelData.value)
                        popup.close()
                    }
                }
            }
        }
    }
}
