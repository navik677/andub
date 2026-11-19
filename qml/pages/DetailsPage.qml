import QtQuick
import QtQuick.Layouts
import Andub

// Title page: artwork, synopsis, actions and the episode grid.
Item {
    id: page
    required property Details details
    signal back()
    signal play(int index)

    readonly property bool narrow: width < 860
    readonly property int side: width >= 1280 ? 48 : (narrow ? 18 : 32)
    readonly property int resume: details.resumeIndex
    readonly property int episodeCount: details.episodes.count

    function startEpisode(index) {
        if (Settings.externalPlayer) details.playExternal(index)
        else page.play(index)
    }

    GridView {
        id: grid
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        leftMargin: page.side
        rightMargin: page.side - gap
        bottomMargin: 40
        model: details.episodes
        cacheBuffer: 800

        readonly property int gap: 10
        readonly property int columns: Math.max(1, Math.floor((width - leftMargin - rightMargin) / (page.narrow ? 200 : 236)))
        cellWidth: Math.floor((width - leftMargin - rightMargin) / columns)
        cellHeight: 70

        header: Item {
            width: grid.width - grid.leftMargin - grid.rightMargin + grid.gap
            height: hero.implicitHeight + 156

            Backdrop {
                x: -grid.leftMargin
                y: -40
                width: page.width
                height: 560
                poster: page.details.poster
                strength: Theme.dark ? 0.7 : 0.45
            }

            GridLayout {
                id: hero
                y: 72
                width: parent.width - grid.gap
                columns: page.narrow ? 1 : 2
                columnSpacing: 40
                rowSpacing: 24

                Item {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: page.narrow ? 150 : 250
                    Layout.preferredHeight: Math.round(Layout.preferredWidth * 1.45)
                    Skeleton { anchors.fill: parent; radius: Theme.radiusXl; visible: !posterImg.ready }
                    RoundedImage {
                        id: posterImg
                        anchors.fill: parent
                        radius: Theme.radiusXl
                        source: Theme.poster(page.details.poster)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 0

                    Text {
                        text: page.details.providerName
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        text: page.details.title
                        color: Theme.text
                        font.pixelSize: page.narrow ? Theme.fontXxl : Theme.fontDisplay
                        font.weight: Font.ExtraBold
                        font.letterSpacing: -0.6
                        wrapMode: Text.Wrap
                        lineHeight: 1.08
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        visible: text !== ""
                        text: page.details.titleEn
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontLg
                        wrapMode: Text.Wrap
                    }

                    Row {
                        Layout.topMargin: 14
                        spacing: 14
                        Repeater {
                            model: [
                                page.details.year > 0 ? String(page.details.year) : "",
                                page.details.status,
                                page.details.ageRating,
                                page.episodeCount > 0 ? page.episodeCount + " сер." : ""
                            ].filter(s => s)
                            Text {
                                required property string modelData
                                text: modelData
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.Medium
                            }
                        }
                        Row {
                            visible: page.details.rating !== ""
                            spacing: 5
                            Icon { anchors.verticalCenter: parent.verticalCenter; name: "star-fill"; size: 14; color: "#f5c451" }
                            Text {
                                text: page.details.rating
                                color: Theme.text
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 14
                        visible: page.details.genres.length > 0
                        spacing: 6
                        Repeater {
                            model: page.details.genres
                            Rectangle {
                                required property string modelData
                                height: 26
                                width: g.implicitWidth + 20
                                radius: 13
                                color: "transparent"
                                border.width: 1
                                border.color: Theme.lineStrong
                                Text {
                                    id: g
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.Medium
                                }
                            }
                        }
                    }

                    Text {
                        id: synopsis
                        property bool expanded: false
                        Layout.fillWidth: true
                        Layout.maximumWidth: 760
                        Layout.topMargin: 18
                        visible: text !== ""
                        text: page.details.description
                        color: Theme.text
                        opacity: 0.86
                        font.pixelSize: Theme.fontMd + 1
                        lineHeight: 1.45
                        wrapMode: Text.Wrap
                        maximumLineCount: expanded ? 1000 : 4
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: synopsis.truncated || synopsis.expanded
                        Layout.topMargin: 4
                        text: synopsis.expanded ? "Згорнути" : "Читати далі"
                        color: Theme.accent
                        font.pixelSize: Theme.fontSm
                        font.weight: Font.DemiBold
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: synopsis.expanded = !synopsis.expanded }
                    }

                    RowLayout {
                        Layout.topMargin: 22
                        spacing: 10
                        AppButton {
                            kind: "primary"
                            iconName: "play-fill"
                            enabled: page.episodeCount > 0
                            implicitHeight: 44
                            text: {
                                if (page.details.loading) return "Завантаження серій"
                                if (page.episodeCount === 0) return "Немає серій"
                                if (!page.details.hasProgress) return "Дивитись"
                                const at = page.details.resumePosition > 0 ? " з " + Theme.formatTime(page.details.resumePosition) : ""
                                return (page.narrow ? "Серія " : "Продовжити: серія ") + page.details.resumeNumber + at
                            }
                            onClicked: page.startEpisode(page.resume)
                        }
                        // Watch the title again from episode 1: drops "Переглянуто" and saved positions
                        AppButton {
                            visible: page.details.hasProgress && page.episodeCount > 0
                            implicitHeight: 44
                            iconName: "arrow-counter-clockwise"
                            text: page.narrow ? "" : "Переглянути знову"
                            tip: page.narrow ? "Переглянути знову з 1 серії" : "Скинути позначки «Переглянуто» і почати з 1 серії"
                            onClicked: {
                                page.details.resetProgress()
                                page.startEpisode(0)
                            }
                        }
                        AppButton {
                            id: favButton
                            implicitHeight: 44
                            iconName: page.details.isFavorite ? "heart-fill" : "heart"
                            foreground: page.details.isFavorite ? Theme.accent : Theme.text
                            text: page.narrow ? "" : (page.details.isFavorite ? "В улюблених" : "До улюблених")
                            tip: page.narrow ? (page.details.isFavorite ? "Прибрати з улюблених" : "До улюблених") : ""
                            onClicked: {
                                page.details.toggleFavorite()
                                pop.restart()
                            }
                            SequentialAnimation {
                                id: pop
                                NumberAnimation { target: favButton; property: "scale"; to: 1.06; duration: 110; easing.type: Easing.OutCubic }
                                NumberAnimation { target: favButton; property: "scale"; to: 1; duration: 220; easing.type: Easing.OutBack }
                            }
                        }
                    }
                }
            }

            // Episodes heading
            RowLayout {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18
                width: parent.width - grid.gap
                spacing: 12
                Text {
                    text: "Серії"
                    color: Theme.text
                    font.pixelSize: Theme.fontXl
                    font.weight: Font.Bold
                }
                Text {
                    visible: page.episodeCount > 0
                    text: page.details.watchedCount > 0
                          ? "переглянуто " + page.details.watchedCount + " з " + page.episodeCount
                          : page.episodeCount
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.Medium
                }
                Item { Layout.fillWidth: true }
            }
        }

        delegate: Item {
            required property int index
            required property string number
            required property string title
            required property bool watched
            required property real progress
            required property bool busy
            width: grid.cellWidth
            height: grid.cellHeight
            EpisodeTile {
                width: parent.width - grid.gap
                height: parent.height - grid.gap
                number: parent.number
                title: parent.title
                watched: parent.watched
                busy: parent.busy
                current: parent.index === page.resume && page.details.hasProgress
                progress: parent.progress
                onPlay: page.startEpisode(parent.index)
                onDownload: page.details.download(parent.index)
            }
        }

        footer: Item {
            width: grid.width - grid.leftMargin - grid.rightMargin
            height: page.details.loading || page.episodeCount === 0 ? 200 : 0
            Grid {
                visible: page.details.loading
                columns: grid.columns
                spacing: grid.gap
                Repeater {
                    model: grid.columns * 2
                    Skeleton { width: grid.cellWidth - grid.gap; height: grid.cellHeight - grid.gap; radius: Theme.radiusLg }
                }
            }
            EmptyState {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 10
                visible: !page.details.loading && page.episodeCount === 0
                iconName: "film-slate"
                title: "Серій не знайдено"
                message: "Джерело не повернуло список серій для цього тайтлу."
                actionText: "Оновити"
                onAction: page.details.reload()
            }
        }
    }

    // Floating top bar; gains a solid background once content scrolls under it
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: Theme.bg
        opacity: Math.min(1, Math.max(0, (grid.contentY - grid.originY - 20) / 80))
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
    }
    RowLayout {
        x: page.side - 10
        y: 12
        width: page.width - page.side * 2 + 10
        AppButton {
            kind: "ghost"
            iconName: "arrow-left"
            text: "Назад"
            compact: true
            onClicked: page.back()
        }
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            text: page.details.title
            elide: Text.ElideRight
            color: Theme.text
            font.pixelSize: Theme.fontMd
            font.weight: Font.DemiBold
            opacity: Math.min(1, Math.max(0, (grid.contentY - grid.originY - 260) / 80))
        }
    }

    Keys.onEscapePressed: page.back()
    Keys.onBackPressed: page.back()
}
