import QtQuick
import Andub

// Loading placeholder with a slow shimmer sweep
Rectangle {
    id: root
    radius: Theme.radius
    color: Theme.surfaceHi
    clip: true

    Rectangle {
        id: sweep
        width: root.width * 0.6
        height: root.height
        x: -width
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: Theme.hover }
            GradientStop { position: 1.0; color: "transparent" }
        }
        NumberAnimation on x {
            from: -sweep.width
            to: root.width
            duration: 1400
            loops: Animation.Infinite
            running: root.visible
            easing.type: Easing.InOutQuad
        }
    }
}
