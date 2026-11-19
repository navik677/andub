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
    }

    function openDetails(row) {
        const d = Catalog.openDetails(row)
        if (d) browse.push(detailsComponent, { details: d })
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavRail {
            Layout.fillHeight: true
            current: window.section
            downloadsActive: Downloads.activeCount
            onNavigate: s => window.navigate(s)
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
            onBack: browse.pop()
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

    Component.onCompleted: Catalog.refresh()

    // Screenshot automation, only loaded when ANDUB_DEV_ACTION is set (see main.cpp)
    Loader {
        active: devAction !== ""
        source: "DevDriver.qml"
    }
}
