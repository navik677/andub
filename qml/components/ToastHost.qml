import QtQuick
import Andub

// Stack of transient messages at the bottom of the window
Column {
    id: host
    spacing: 8
    width: 420

    function show(text, error) {
        model.append({ text: text, error: !!error })
        if (model.count > 3) model.remove(0)
    }

    Connections {
        target: Notifier
        function onToast(text, error) { host.show(text, error) }
    }

    ListModel { id: model }

    Repeater {
        model: model
        delegate: Rectangle {
            id: toast
            required property string text
            required property bool error
            required property int index
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(host.width, row.implicitWidth + 32)
            height: 44
            radius: Theme.radiusXl
            color: Theme.dark ? "#f0f0f2" : "#1c1d21"
            opacity: 0
            Component.onCompleted: appear.start()

            NumberAnimation { id: appear; target: toast; property: "opacity"; to: 1; duration: Theme.durMed; easing.type: Easing.OutCubic }
            Timer {
                interval: toast.error ? 5000 : 3200
                running: true
                onTriggered: vanish.start()
            }
            SequentialAnimation {
                id: vanish
                NumberAnimation { target: toast; property: "opacity"; to: 0; duration: Theme.durMed }
                ScriptAction { script: if (toast.index >= 0 && toast.index < model.count) model.remove(toast.index) }
            }

            Row {
                id: row
                anchors.centerIn: parent
                spacing: 10
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: toast.error ? "warning-circle" : "check-circle"
                    size: 18
                    color: toast.error ? Theme.danger : Theme.accent
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, host.width - 70)
                    text: toast.text
                    elide: Text.ElideRight
                    color: Theme.dark ? "#141518" : "#f4f4f6"
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.Medium
                }
            }
            TapHandler { onTapped: vanish.start() }
        }
    }
}
