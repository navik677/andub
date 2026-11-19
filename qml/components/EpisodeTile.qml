import QtQuick
import QtQuick.Layouts
import Andub

Rectangle {
    id: tile
    property string number
    property string title
    property bool watched
    property bool busy
    property bool current: false
    property real progress: 0   // 0..1 for an episode stopped midway
    signal play()
    signal download()

    readonly property bool isHovered: hover.hovered
    radius: Theme.radiusLg
    color: isHovered ? Theme.surfaceHi : Theme.surface
    border.width: current ? 1.5 : 1
    border.color: current ? Theme.accent : Theme.line
    Behavior on color { ColorAnimation { duration: Theme.durFast } }
    scale: tap.pressed ? 0.98 : 1
    Behavior on scale { NumberAnimation { duration: Theme.durFast } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 8
        spacing: 12

        Item {
            Layout.preferredWidth: 30
            Layout.fillHeight: true
            Text {
                anchors.centerIn: parent
                visible: !tile.isHovered && !tile.busy
                text: tile.number
                color: tile.watched ? Theme.textFaint : Theme.text
                font.pixelSize: tile.number.length > 3 ? Theme.fontMd : Theme.fontXl
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
            }
            Icon {
                anchors.centerIn: parent
                visible: tile.isHovered && !tile.busy
                name: "play-fill"
                size: 20
                color: Theme.accent
            }
            Spinner {
                anchors.centerIn: parent
                running: tile.busy
                size: 20
                lineWidth: 2.5
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: tile.title !== "" ? tile.title : "Серія " + tile.number
                color: tile.watched ? Theme.textMuted : Theme.text
                font.pixelSize: Theme.fontSm
                font.weight: Font.Medium
                elide: Text.ElideRight
            }
            Text {
                text: {
                    if (tile.watched) return "Переглянуто"
                    const pct = tile.progress > 0 ? Math.max(1, Math.round(tile.progress * 100)) + "%" : ""
                    if (tile.current) return pct ? "Продовжити · " + pct : "Наступна"
                    return pct ? "Переглянуто " + pct : ""
                }
                visible: text !== ""
                color: tile.current && !tile.watched ? Theme.accent : Theme.textFaint
                font.pixelSize: Theme.fontXs
            }
        }
        IconButton {
            visible: tile.isHovered && !tile.busy
            size: 32
            iconSize: 17
            iconName: "download-simple"
            iconColor: Theme.textMuted
            tip: "Завантажити"
            onClicked: tile.download()
        }
        Icon {
            visible: tile.watched && !tile.isHovered
            name: "check-circle-fill"
            size: 18
            color: Theme.textFaint
            Layout.rightMargin: 6
        }
    }

    // How far into the episode the viewer got
    Rectangle {
        visible: tile.progress > 0 && !tile.watched
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 1
        height: 3
        radius: 1.5
        color: Theme.surfaceHi2
        Rectangle {
            width: parent.width * Math.min(1, tile.progress)
            height: parent.height
            radius: parent.radius
            color: Theme.accent
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: if (!tile.busy) tile.play() }
}
