import QtQuick
import QtQuick.Effects
import Andub

// Heavily blurred artwork used as ambient page lighting. Cross-fades when the
// poster changes. Decoded at a tiny size, so the blur is cheap.
Item {
    id: root
    property string poster
    property real strength: Theme.dark ? 0.55 : 0.35
    clip: true

    property bool flip: false
    onPosterChanged: {
        if (!poster) return
        const next = flip ? a : b
        const src = Theme.poster(poster)
        if (next.source == src && next.status === Image.Ready) flip = !flip
        else next.source = src
    }

    Item {
        id: layers
        anchors.fill: parent
        visible: false
        Image {
            id: a
            anchors.fill: parent
            sourceSize.width: 72
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: root.flip ? 0 : 1
            onStatusChanged: if (status === Image.Ready && source == Theme.poster(root.poster)) root.flip = false
            Behavior on opacity { NumberAnimation { duration: 600; easing.type: Easing.InOutQuad } }
        }
        Image {
            id: b
            anchors.fill: parent
            sourceSize.width: 72
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: root.flip ? 1 : 0
            onStatusChanged: if (status === Image.Ready && source == Theme.poster(root.poster)) root.flip = true
            Behavior on opacity { NumberAnimation { duration: 600; easing.type: Easing.InOutQuad } }
        }
    }
    MultiEffect {
        anchors.fill: parent
        source: layers
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: 0.15
        opacity: root.strength
    }
    // Fade into the page background at the bottom
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, 0.25) }
            GradientStop { position: 0.65; color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, 0.8) }
            GradientStop { position: 1.0; color: Theme.bg }
        }
    }
}
