import QtQuick
import QtQuick.Layouts
import Andub

Item {
    id: page
    readonly property int side: width >= 1280 ? 40 : (width < 860 ? 18 : 28)

    RowLayout {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: page.side
        anchors.rightMargin: page.side
        anchors.topMargin: 24
        spacing: 10
        Text {
            Layout.fillWidth: true
            text: "Завантаження"
            color: Theme.text
            font.pixelSize: Theme.fontXxl
            font.weight: Font.Bold
            font.letterSpacing: -0.4
        }
        AppButton {
            kind: "ghost"
            compact: true
            visible: Downloads.count > Downloads.activeCount
            text: "Прибрати завершені"
            onClicked: Downloads.clearFinished()
        }
        AppButton {
            compact: true
            iconName: "folder-open"
            text: "Відкрити папку"
            onClicked: Settings.openFolder(Settings.downloadsPath)
        }
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.topMargin: 20
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - page.side * 2, 900)
        clip: true
        spacing: 8
        bottomMargin: 24
        model: Downloads
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            id: row
            required property int index
            required property string animeTitle
            required property string episodeTitle
            required property string quality
            required property real progress
            required property string speed
            required property string eta
            required property bool active
            required property bool done
            required property bool failed
            required property string errorText

            width: list.width
            height: 76
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: 1
            border.color: Theme.line

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 10
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: 20
                    color: row.failed ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.14)
                                      : row.done ? Theme.accentSoft : Theme.surfaceHi
                    Icon {
                        anchors.centerIn: parent
                        visible: !row.active
                        name: row.failed ? "warning-circle" : "check"
                        size: 20
                        color: row.failed ? Theme.danger : Theme.accent
                    }
                    Spinner { anchors.centerIn: parent; running: row.active; size: 20; lineWidth: 2.5 }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        text: row.animeTitle
                        color: Theme.text
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: {
                            const parts = [row.episodeTitle, row.quality]
                            if (row.failed) parts.push(row.errorText)
                            else if (row.active && row.speed) parts.push(row.speed + (row.eta ? ", залишилось " + row.eta : ""))
                            return parts.filter(s => s).join("  ·  ")
                        }
                        color: row.failed ? Theme.danger : Theme.textMuted
                        font.pixelSize: Theme.fontSm
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        visible: row.active
                        Layout.preferredHeight: 3
                        radius: 1.5
                        color: Theme.surfaceHi2
                        Rectangle {
                            height: parent.height
                            radius: parent.radius
                            width: parent.width * row.progress
                            color: Theme.accent
                            Behavior on width { NumberAnimation { duration: 400 } }
                        }
                    }
                }
                Text {
                    visible: row.active
                    text: Math.round(row.progress * 100) + "%"
                    color: Theme.text
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                }
                IconButton {
                    visible: row.done
                    iconName: "folder-open"
                    iconColor: Theme.textMuted
                    tip: "Показати у папці"
                    onClicked: Downloads.openFolder(row.index)
                }
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        visible: Downloads.count === 0
        iconName: "download-simple"
        title: "Немає завантажень"
        message: "Наведіть курсор на серію на сторінці тайтлу і натисніть кнопку завантаження. Файли зберігаються в теці «Anime» у ваших завантаженнях."
    }
}
