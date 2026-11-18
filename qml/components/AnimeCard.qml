import QtQuick
import QtQuick.Layouts
import Andub

// Grid card: poster + two-line title + one meta line.
Item {
    id: card
    property string title
    property string poster
    property string rating
    property string meta
    property real entranceDelay: -1   // ms; < 0 means appear without the staggered rise
    signal clicked()
    signal hovered()

    readonly property bool isHovered: hover.hovered
    implicitHeight: posterBox.height + info.implicitHeight + 10

    opacity: 0
    transform: Translate { id: lift; y: 0 }

    Component.onCompleted: entranceDelay >= 0 ? enter.start() : fadeIn.start()
    SequentialAnimation {
        id: enter
        PauseAnimation { duration: card.entranceDelay }
        ParallelAnimation {
            NumberAnimation { target: card; property: "opacity"; from: 0; to: 1; duration: 320; easing.type: Easing.OutCubic }
            NumberAnimation { target: lift; property: "y"; from: 14; to: 0; duration: 420; easing.type: Easing.OutCubic }
        }
    }
    NumberAnimation { id: fadeIn; target: card; property: "opacity"; from: 0; to: 1; duration: 180 }

    Item {
        id: posterBox
        width: parent.width
        height: Math.round(width * 1.45)
        scale: tap.pressed ? 0.98 : (card.isHovered ? 1.025 : 1)
        Behavior on scale { NumberAnimation { duration: Theme.durMed; easing.type: Easing.OutCubic } }

        Skeleton {
            anchors.fill: parent
            radius: Theme.radiusLg
            visible: !img.ready
        }
        RoundedImage {
            id: img
            anchors.fill: parent
            source: Theme.poster(card.poster)
        }
        // Hover ring
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.radiusLg + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.text
            opacity: card.isHovered ? 0.85 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.durMed } }
        }
        Rectangle {
            visible: card.rating !== ""
            x: 8; y: 8
            height: 24
            width: ratingRow.implicitWidth + 14
            radius: Theme.radiusSm
            color: Qt.rgba(0.06, 0.06, 0.07, 0.78)
            Row {
                id: ratingRow
                anchors.centerIn: parent
                spacing: 4
                Icon { anchors.verticalCenter: parent.verticalCenter; name: "star-fill"; size: 12; color: "#f5c451" }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: card.rating
                    color: "#f2f2f4"
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.Bold
                }
            }
        }
    }

    ColumnLayout {
        id: info
        anchors.top: posterBox.bottom
        anchors.topMargin: 10
        width: parent.width
        spacing: 3
        Text {
            Layout.fillWidth: true
            text: card.title
            color: Theme.text
            font.pixelSize: Theme.fontMd
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            lineHeight: 1.15
        }
        Text {
            Layout.fillWidth: true
            text: card.meta
            visible: text !== ""
            color: Theme.textMuted
            font.pixelSize: Theme.fontXs
            elide: Text.ElideRight
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
        onHoveredChanged: if (hovered) card.hovered()
    }
    TapHandler {
        id: tap
        onTapped: card.clicked()
    }
}
