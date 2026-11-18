import QtQuick
import QtQuick.Shapes
import Andub

// Indeterminate ring
Item {
    id: root
    property int size: 32
    property real lineWidth: 3
    property color color: Theme.text
    width: size
    height: size
    visible: running
    property bool running: true

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.rgba(root.color.r, root.color.g, root.color.b, 0.15)
            strokeWidth: root.lineWidth
            fillColor: "transparent"
            PathAngleArc {
                centerX: root.size / 2; centerY: root.size / 2
                radiusX: (root.size - root.lineWidth) / 2; radiusY: radiusX
                startAngle: 0; sweepAngle: 360
            }
        }
        ShapePath {
            strokeColor: root.color
            strokeWidth: root.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.size / 2; centerY: root.size / 2
                radiusX: (root.size - root.lineWidth) / 2; radiusY: radiusX
                startAngle: -90; sweepAngle: 90
            }
        }
        RotationAnimation on rotation {
            from: 0; to: 360; duration: 900
            loops: Animation.Infinite
            running: root.running && root.visible
        }
    }
}
