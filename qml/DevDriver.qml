import QtQuick

// Drives the UI into a given state for screenshots during development.
// ANDUB_DEV_ACTION: settings[:end] | downloads | favorites | details:<row> | play:<row> | testfile:<path> | reopen:<path>
//                   filters[:<provider index>] (opens the filter panel) | filtered[:<provider index>] (sample filter applied)
//                   gohome[:<provider index>] (filter + search + details page, then "Каталог" in the rail)
//                   resume (details of row 0, logs the resume state, presses the main button, logs the player position)
//                   rewatch (plays row 0 to 95%, logs "watched", then resets the title's progress and logs again)
Item {
    readonly property var parts: devAction.split(":")
    readonly property string what: parts[0]
    readonly property string arg: parts.slice(1).join(":")

    Timer {
        interval: 2500
        running: true
        onTriggered: {
            if (what === "rewatch") {
                window.openDetails(0)
                rewatchStep.start()
                return
            }
            if (what === "resume") {
                window.openDetails(0)
                resumeStep.start()
                return
            }
            if (what === "gohome") {
                if (arg !== "") Catalog.providerIndex = parseInt(arg)
                Catalog.sort = 2
                Catalog.kinds = [1]
                Catalog.query = "а"
                home.start()
                return
            }
            if (what === "filters" || what === "filtered") {
                if (arg !== "") Catalog.providerIndex = parseInt(arg)
                if (what === "filters") catalogPage.openFilters()
                else {
                    Catalog.sort = 2
                    Catalog.selectedGenres = ["Комедия"]
                    Catalog.kinds = [1]
                    Catalog.yearFrom = 2015
                }
                return
            }
            if (what === "settings" || what === "downloads" || what === "favorites") {
                window.navigate(what)
                if (arg === "end") {
                    const page = pages.children[pages.currentIndex]
                    page.contentY = Math.max(0, page.contentHeight - page.height)
                }
                return
            }
            window.openDetails(what === "testfile" || what === "reopen" ? 0 : parseInt(arg || "0"))
            if (what !== "details") play.start()
        }
    }
    Timer {
        id: play
        interval: 2500
        onTriggered: {
            browse.currentItem.play(0)
            pause.start()
        }
    }
    Timer {
        id: pause
        interval: what === "testfile" || what === "reopen" ? 5000 : 9000
        onTriggered: {
            if (!player.item) return
            const p = player.item.mpvPlayer
            if (what === "testfile" || what === "reopen") {
                p.load("file://" + arg, {})
                freeze.start()
            } else {
                p.seek(400)
                freeze.start()
            }
        }
    }
    Timer {
        id: freeze
        interval: 2500
        onTriggered: {
            if (what === "reopen" && !reopened) {
                reopened = true
                player.item.close()   // tears down MpvPlayer while a file is playing
                play.start()
                return
            }
            player.item.mpvPlayer.paused = true
            player.item.poke()
        }
    }
    property bool reopened: false
    Timer {
        id: rewatchStep
        interval: 7000
        property int step: 0
        onTriggered: {
            const page = browse.currentItem
            const d = page.details
            const p = player.item ? player.item.mpvPlayer : null
            const log = tag => console.log("[rewatch] " + tag + ": watched=" + d.watchedCount + " hasProgress=" + d.hasProgress
                                           + " resume=" + d.resumeNumber + " at=" + Math.round(d.resumePosition))
            switch (step++) {
            case 0: log("before"); page.play(0); interval = 9000; restart(); break
            case 1: p.seek(p.duration * 0.95); interval = 4000; restart(); break
            case 2: p.paused = true; interval = 1500; restart(); break
            case 3: player.item.close(); log("after 95%"); d.resetProgress(); log("after reset"); break
            }
        }
    }
    Timer {
        id: resumeStep
        interval: 7000
        property int step: 0
        onTriggered: {
            const d = browse.currentItem.details
            if (step === 0) {
                console.log("[resume] hasProgress=" + d.hasProgress + " index=" + d.resumeIndex + " number=" + d.resumeNumber
                            + " position=" + d.resumePosition + " watched=" + d.watchedCount)
                browse.currentItem.startEpisode(browse.currentItem.resume)
                step = 1
                restart()
            } else {
                const p = player.item ? player.item.mpvPlayer : null
                console.log("[resume] player position=" + (p ? Math.round(p.position) : -1) + " duration=" + (p ? Math.round(p.duration) : -1))
            }
        }
    }
    Timer {
        id: home
        interval: 6000
        property int step: 0
        // also logs the state at each step: [gohome] depth=... (QT_FORCE_STDERR_LOGGING=1)
        onTriggered: {
            const state = () => console.log("[gohome] depth=" + browse.depth + " section=" + window.section
                + " query='" + Catalog.query + "' sort=" + Catalog.sort + " kinds=" + JSON.stringify(Catalog.kinds)
                + " filters=" + Catalog.activeFilterCount + " favorites=" + Catalog.favoritesMode
                + " results=" + Catalog.results.count + " loading=" + Catalog.loading)
            if (step === 0) { window.openDetails(0); state(); step = 1; restart() }
            else if (step === 1) { window.navigate("catalog"); state(); step = 2; restart() }
            else state()
        }
    }
}
