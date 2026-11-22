import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Andub

ApplicationWindow {
    id: window
    width: 1320
    height: 860
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: player.active ? player.title + " - Andub" : "Andub"
    color: Theme.bg

    property string section: Catalog.favoritesMode ? "favorites" : "catalog"

    // ---- Browser-style history --------------------------------------------
    // Each entry: { section, details (Details object or null), catalog (Catalog.saveState()) }.
    // A new move drops the forward entries; Back/Forward re-apply an entry.
    property var history: []
    property int historyIndex: -1
    property bool restoringHistory: false
    readonly property bool canGoBack: historyIndex > 0 && !player.active
    readonly property bool canGoForward: historyIndex >= 0 && historyIndex < history.length - 1 && !player.active

    function currentEntry() {
        return {
            section: section,
            details: pages.currentIndex === 0 && browse.depth > 1 && browse.currentItem ? browse.currentItem.details : null,
            catalog: Catalog.saveState()
        }
    }
    function recordHistory() {
        if (restoringHistory) return
        const e = currentEntry()
        const h = history.slice(0, historyIndex + 1)
        const last = h.length ? h[h.length - 1] : null
        if (last && last.section === e.section && last.details === e.details) {
            last.catalog = e.catalog       // same place: just refresh its catalog state
        } else {
            h.push(e)
            if (h.length > 100) h.shift()  // keep it bounded
        }
        history = h
        historyIndex = h.length - 1
    }
    function goHistory(delta) {
        const target = historyIndex + delta
        if (player.active || target < 0 || target >= history.length) return
        // Leaving this place: remember what the catalog showed here
        if (historyIndex >= 0) history[historyIndex].catalog = Catalog.saveState()
        restoringHistory = true
        applyEntry(history[target])
        historyIndex = target
        restoringHistory = false
    }
    function goBack() { goHistory(-1) }
    function goForward() { goHistory(1) }
    function applyEntry(e) {
        if (e.section === "catalog" || e.section === "favorites") {
            Catalog.restoreState(e.catalog)
            pages.currentIndex = 0
            const shown = browse.depth > 1 && browse.currentItem ? browse.currentItem.details : null
            if (shown !== e.details) {
                browse.pop(null)
                if (e.details) browse.push(detailsComponent, { details: e.details })
            }
        } else {
            pages.currentIndex = e.section === "downloads" ? 1 : 2
        }
        section = e.section
    }

    function navigate(target) {
        if (target === "catalog") {
            // Always the front page: no search, filters or sorting left over
            browse.pop(null)
            catalogPage.goHome()
            pages.currentIndex = 0
            section = target
        } else if (target === "favorites") {
            browse.pop(null)
            Catalog.favoritesMode = true
            pages.currentIndex = 0
            section = target
        } else {
            pages.currentIndex = target === "downloads" ? 1 : 2
            section = target
        }
        recordHistory()
    }

    function openDetails(row) {
        // Keep the catalog state of the page we're leaving
        if (historyIndex >= 0) history[historyIndex].catalog = Catalog.saveState()
        const d = Catalog.openDetails(row)
        if (d) {
            browse.push(detailsComponent, { details: d })
            recordHistory()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavRail {
            Layout.fillHeight: true
            current: window.section
            downloadsActive: Downloads.activeCount
            canGoBack: window.canGoBack
            canGoForward: window.canGoForward
            onNavigate: s => window.navigate(s)
            onBack: window.goBack()
            onForward: window.goForward()
        }

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackView {
                id: browse
                initialItem: CatalogPage {
                    id: catalogPage
                    focus: true
                    onOpenAnime: row => window.openDetails(row)
                }
                onCurrentItemChanged: if (currentItem) currentItem.forceActiveFocus()

                pushEnter: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 260; easing.type: Easing.OutCubic }
                        NumberAnimation { property: "x"; from: 32; to: 0; duration: 320; easing.type: Easing.OutCubic }
                    }
                }
                pushExit: Transition {
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 180 }
                }
                popEnter: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 240; easing.type: Easing.OutCubic }
                }
                popExit: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200 }
                        NumberAnimation { property: "x"; from: 0; to: 32; duration: 220; easing.type: Easing.InCubic }
                    }
                }
            }

            DownloadsPage {}
            SettingsPage {}
        }
    }

    Component {
        id: detailsComponent
        DetailsPage {
            onBack: {
                const prev = window.historyIndex > 0 ? window.history[window.historyIndex - 1] : null
                if (prev && prev.details === null && (prev.section === "catalog" || prev.section === "favorites")) {
                    window.goBack()
                } else {
                    browse.pop()
                    window.recordHistory()
                }
            }
            onPlay: index => {
                const s = details.createSession(index)
                if (s) player.open(s)
            }
        }
    }

    // The player covers the whole window, including the navigation rail
    Loader {
        id: player
        anchors.fill: parent
        active: false
        z: 10
        property var session: null
        property string title: session ? session.animeTitle : ""
        function open(s) {
            session = s
            active = true
        }
        sourceComponent: PlayerPage {
            session: player.session
            onClosed: {
                player.active = false
                player.session = null
                const top = browse.currentItem as Item
                if (top) top.forceActiveFocus()
            }
        }
        onLoaded: item.forceActiveFocus()
    }

    ToastHost {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        z: 20
    }

    Shortcut {
        sequences: [StandardKey.Find, "Ctrl+K"]
        enabled: !player.active && pages.currentIndex === 0 && browse.depth === 1
        onActivated: catalogPage.focusSearch()
    }

    // Alt+Left/Right like a browser; Backspace when no text field takes it
    Shortcut {
        sequences: ["Alt+Left", StandardKey.Back, "Backspace"]
        enabled: window.canGoBack
        onActivated: window.goBack()
    }
    Shortcut {
        sequences: ["Alt+Right", StandardKey.Forward]
        enabled: window.canGoForward
        onActivated: window.goForward()
    }
    // Mouse side buttons, anywhere in the window (the player has its own controls)
    TapHandler {
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        enabled: !player.active
        onTapped: (point, button) => button === Qt.BackButton ? window.goBack() : window.goForward()
    }

    Component.onCompleted: {
        Catalog.refresh()
        recordHistory()
    }

    // Screenshot automation, only loaded when ANDUB_DEV_ACTION is set (see main.cpp)
    Loader {
        active: devAction !== ""
        source: "DevDriver.qml"
    }
}
