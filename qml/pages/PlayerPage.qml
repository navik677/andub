import QtQuick
import QtQuick.Layouts
import Andub

// Full-window embedded player with auto-hiding controls.
Rectangle {
    id: page
    required property PlayerSession session
    signal closed()

    color: "black"
    focus: true

    property alias mpvPlayer: mpv
    property bool fullscreen: false
    property int restoreVisibility: Window.Windowed
    readonly property bool menusOpen: upscaleMenu.opened || episodeMenu.opened
    property bool controlsVisible: true
    readonly property bool showControls: controlsVisible || mpv.paused || menusOpen || seekBar.dragging
                                         || session.resolving || session.error !== ""

    function close() {
        mpv.stop()
        if (fullscreen) toggleFullscreen()
        page.closed()
    }
    function toggleFullscreen() {
        const w = page.Window.window
        if (!w) return
        if (fullscreen) {
            w.visibility = restoreVisibility === Window.FullScreen ? Window.Windowed : restoreVisibility
            fullscreen = false
        } else {
            restoreVisibility = w.visibility
            w.visibility = Window.FullScreen
            fullscreen = true
        }
        osd.show(fullscreen ? "Повний екран" : "Віконний режим")
    }
    function poke() {
        controlsVisible = true
        hideTimer.restart()
    }
    function seekBy(sec, message) {
        mpv.seekRelative(sec)
        poke()
        osd.show(message || ((sec > 0 ? "+" : "") + sec + " с"))
    }
    function changeVolume(delta) {
        mpv.muted = false
        mpv.volume = Math.max(0, Math.min(100, mpv.volume + delta))
        Settings.volume = mpv.volume
        osd.show("Гучність " + Math.round(mpv.volume) + "%")
    }
    function setUpscaler(id) {
        Settings.upscaler = id
        mpv.upscaler = id
        const ok = mpv.applyUpscaler()
        osd.show(ok ? "Апскейл: " + Settings.upscalerName(id) : "Шейдери апскейлу не знайдено")
    }
    function cycleUpscaler() {
        const list = Settings.upscalerProfiles
        let i = list.findIndex(p => p.id === mpv.upscaler)
        setUpscaler(list[(i + 1) % list.length].id)
    }

    Component.onCompleted: {
        mpv.volume = Settings.volume
        mpv.upscaler = Settings.upscaler
        mpv.applyUpscaler()
        session.start(session.index)
        hideTimer.restart()
        forceActiveFocus()
    }

    Connections {
        target: page.session
        function onStreamReady(url, headers) { mpv.load(url, headers) }
    }

    MpvPlayer {
        id: mpv
        anchors.fill: parent
        onEndReached: {
            if (page.session.hasNext) {
                osd.show("Наступна серія")
                page.session.next()
            }
        }
        onLoadFailed: osd.show("Потік не відтворюється", 3000)
    }

    Timer {
        id: hideTimer
        interval: 2600
        onTriggered: page.controlsVisible = (bottomHover.hovered || topHover.hovered)
    }

    // Single click toggles pause, double click toggles fullscreen
    Timer {
        id: clickTimer
        interval: 230
        onTriggered: { mpv.togglePause(); osd.show(mpv.paused ? "Пауза" : "Відтворення") }
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: page.showControls ? Qt.ArrowCursor : Qt.BlankCursor
        acceptedButtons: Qt.LeftButton
        property point last
        onPositionChanged: e => {
            if (Math.abs(e.x - last.x) + Math.abs(e.y - last.y) > 3) page.poke()
            last = Qt.point(e.x, e.y)
        }
        onClicked: clickTimer.restart()
        onDoubleClicked: { clickTimer.stop(); page.toggleFullscreen() }
        onWheel: w => page.changeVolume(w.angleDelta.y > 0 ? 5 : -5)
    }

    // Center: buffering ring, stream errors, OSD
    Spinner {
        anchors.centerIn: parent
        size: 48
        lineWidth: 3.5
        color: "#ffffff"
        running: page.session.resolving || (mpv.buffering && page.session.error === "")
    }
    ColumnLayout {
        anchors.centerIn: parent
        visible: page.session.error !== ""
        spacing: 14
        Icon { Layout.alignment: Qt.AlignHCenter; name: "warning-circle"; size: 40; color: "#ffffff" }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: page.session.error
            color: "#f2f2f4"
            font.pixelSize: Theme.fontLg
            font.weight: Font.DemiBold
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10
            AppButton { text: "Спробувати ще раз"; kind: "primary"; onClicked: page.session.retry() }
            AppButton {
                visible: page.session.hasNext
                text: "Наступна серія"
                onClicked: page.session.next()
            }
        }
    }

    Rectangle {
        id: osd
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.18
        width: osdText.implicitWidth + 32
        height: 40
        radius: 20
        color: Qt.rgba(0.06, 0.06, 0.07, 0.78)
        opacity: 0
        function show(text, ms) {
            osdText.text = text
            osdFade.stop()
            opacity = 1
            osdTimer.interval = ms || 1100
            osdTimer.restart()
        }
        Text {
            id: osdText
            anchors.centerIn: parent
            color: "#f2f2f4"
            font.pixelSize: Theme.fontMd
            font.weight: Font.DemiBold
        }
        Timer { id: osdTimer; onTriggered: osdFade.start() }
        NumberAnimation { id: osdFade; target: osd; property: "opacity"; to: 0; duration: 220 }
    }

    // Top bar
    Item {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        height: 120
        opacity: page.showControls ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.durMed } }
        HoverHandler { id: topHover }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.72) }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 18
            spacing: 14
            IconButton {
                iconName: "arrow-left"
                iconColor: "#ffffff"
                hoverColor: Qt.rgba(1, 1, 1, 0.12)
                size: 40
                tip: "Назад (Esc)"
                onClicked: page.close()
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: page.session.animeTitle
                    color: "#ffffff"
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: page.session.episodeTitle
                    color: Qt.rgba(1, 1, 1, 0.7)
                    font.pixelSize: Theme.fontSm
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                visible: page.session.quality !== ""
                Layout.preferredHeight: 24
                Layout.preferredWidth: qText.implicitWidth + 14
                radius: Theme.radiusSm
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.35)
                Text {
                    id: qText
                    anchors.centerIn: parent
                    text: page.session.quality
                    color: "#ffffff"
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.Bold
                }
            }
            AppButton {
                kind: "ghost"
                compact: true
                iconName: "arrow-square-out"
                foreground: "#ffffff"
                text: page.width > 760 ? "Відкрити в mpv" : ""
                tip: "Продовжити в окремому вікні mpv"
                onClicked: {
                    page.session.openExternal(mpv.position)
                    page.close()
                }
            }
        }
    }

    // Bottom controls
    Item {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 150
        opacity: page.showControls ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.durMed } }
        HoverHandler { id: bottomHover }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.8) }
            }
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 22
            anchors.rightMargin: 22
            anchors.bottomMargin: 14
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                Text {
                    text: Theme.formatTime(seekBar.dragging ? seekBar.dragValue : mpv.position)
                    color: "#ffffff"
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                }
                SeekBar {
                    id: seekBar
                    Layout.fillWidth: true
                    position: mpv.position
                    duration: mpv.duration
                    buffered: mpv.buffered
                    onSeekRequested: s => { mpv.seek(s); page.poke() }
                }
                Text {
                    text: Theme.formatTime(mpv.duration)
                    color: Qt.rgba(1, 1, 1, 0.7)
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                component PlayerButton: IconButton {
                    iconColor: "#ffffff"
                    hoverColor: Qt.rgba(1, 1, 1, 0.12)
                    size: 40
                    iconSize: 22
                }

                PlayerButton { iconName: "skip-back"; tip: "Попередня серія (P)"; enabled: page.session.hasPrev; onClicked: page.session.prev() }
                PlayerButton { iconName: "arrow-counter-clockwise"; tip: "Назад на 10 с (←)"; onClicked: page.seekBy(-10) }
                PlayerButton {
                    size: 48
                    iconSize: 26
                    iconName: mpv.paused ? "play-fill" : "pause-fill"
                    tip: "Пауза (Пробіл)"
                    onClicked: mpv.togglePause()
                }
                PlayerButton { iconName: "arrow-clockwise"; tip: "Вперед на 10 с (→)"; onClicked: page.seekBy(10) }
                PlayerButton { iconName: "skip-forward"; tip: "Наступна серія (N)"; enabled: page.session.hasNext; onClicked: page.session.next() }

                AppButton {
                    Layout.leftMargin: 8
                    visible: page.width > 640
                    kind: "ghost"
                    compact: true
                    iconName: "fast-forward"
                    foreground: "#ffffff"
                    text: "Опенінг"
                    tip: "Пропустити опенінг, +85 с (S)"
                    onClicked: page.seekBy(85, "Опенінг пропущено")
                }

                Item { Layout.fillWidth: true }

                PlayerButton {
                    iconName: "list-numbers"
                    tip: "Серії"
                    onClicked: episodeMenu.open()
                    MenuSheet {
                        id: episodeMenu
                        y: -height - 10
                        x: parent.width - width
                        width: 300
                        title: "Серії"
                        value: page.session.index
                        model: page.session.episodes.map((e, i) => ({
                            label: e.title ? e.number + ". " + e.title : "Серія " + e.number, value: i }))
                        onPicked: v => page.session.start(v)
                        onClosed: page.forceActiveFocus()
                    }
                }
                PlayerButton {
                    iconName: "sparkle"
                    tip: "Апскейл: " + Settings.upscalerName(mpv.upscaler) + " (Ctrl+U)"
                    iconColor: mpv.upscaler !== "off" ? Theme.accent : "#ffffff"
                    onClicked: upscaleMenu.open()
                    MenuSheet {
                        id: upscaleMenu
                        y: -height - 10
                        x: parent.width - width
                        title: Settings.shadersAvailable ? "Апскейл у реальному часі" : "Шейдери апскейлу не знайдено"
                        value: mpv.upscaler
                        model: Settings.upscalerProfiles.map(p => ({ label: p.name, value: p.id }))
                        onPicked: v => page.setUpscaler(v)
                        onClosed: page.forceActiveFocus()
                    }
                }
                PlayerButton {
                    iconName: mpv.muted || mpv.volume <= 0 ? "speaker-x" : (mpv.volume < 40 ? "speaker-low" : "speaker-high")
                    tip: "Звук (M)"
                    onClicked: mpv.muted = !mpv.muted
                }
                VolumeSlider {
                    visible: page.width > 640
                    Layout.preferredWidth: 90
                    Layout.rightMargin: 6
                    value: mpv.muted ? 0 : mpv.volume
                    onMoved: v => { mpv.muted = false; mpv.volume = v }
                    onReleased: Settings.volume = mpv.volume
                }
                PlayerButton {
                    iconName: page.fullscreen ? "corners-in" : "corners-out"
                    tip: "Повний екран (F)"
                    onClicked: page.toggleFullscreen()
                }
            }
        }
    }

    // Keys match by physical letter in Latin, Ukrainian and Russian layouts
    function isKey(event, latin, cyr) {
        return event.key === latin || cyr.indexOf(event.key) >= 0
    }
    Keys.onPressed: event => {
        const ctrl = event.modifiers & Qt.ControlModifier
        if (ctrl && isKey(event, Qt.Key_U, [0x413])) { cycleUpscaler(); event.accepted = true; return }
        if (ctrl) return
        event.accepted = true
        if (event.key === Qt.Key_Space || event.key === Qt.Key_K) { mpv.togglePause(); osd.show(mpv.paused ? "Пауза" : "Відтворення") }
        else if (event.key === Qt.Key_Left) seekBy(-10)
        else if (event.key === Qt.Key_Right) seekBy(10)
        else if (event.key === Qt.Key_Up) changeVolume(5)
        else if (event.key === Qt.Key_Down) changeVolume(-5)
        else if (isKey(event, Qt.Key_M, [0x42C])) { mpv.muted = !mpv.muted; osd.show(mpv.muted ? "Звук вимкнено" : "Звук увімкнено") }
        else if (isKey(event, Qt.Key_S, [0x406, 0x42B])) seekBy(85, "Опенінг пропущено")
        else if (isKey(event, Qt.Key_N, [0x422])) session.next()
        else if (isKey(event, Qt.Key_P, [0x417])) session.prev()
        else if (isKey(event, Qt.Key_F, [0x410]) || event.key === Qt.Key_F11) toggleFullscreen()
        else if (event.key === Qt.Key_Escape) { if (fullscreen) toggleFullscreen(); else close() }
        else event.accepted = false
        if (event.accepted) poke()
    }
}
