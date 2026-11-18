import QtQuick

// Drives the UI into a given state for screenshots during development.
// ANDUB_DEV_ACTION: settings[:end] | downloads | favorites | details:<row> | play:<row> | testfile:<path> | reopen:<path>
Item {
    readonly property var parts: devAction.split(":")
    readonly property string what: parts[0]
    readonly property string arg: parts.slice(1).join(":")

    Timer {
        interval: 2500
        running: true
        onTriggered: {
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
}
