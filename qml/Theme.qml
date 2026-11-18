pragma Singleton
import QtQuick
import Andub

// Design tokens. One neutral palette per mode, one user-chosen accent.
QtObject {
    readonly property string mode: Settings.theme
    readonly property bool dark: mode !== "light"
    readonly property bool oled: mode === "oled"

    readonly property color accent: Settings.accent
    readonly property color onAccent: luminance(accent) > 0.55 ? "#141518" : "#ffffff"
    readonly property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.18 : 0.14)

    readonly property color bg: oled ? "#050506" : dark ? "#0f1012" : "#f4f4f6"
    readonly property color surface: oled ? "#0e0e10" : dark ? "#17181b" : "#ffffff"
    readonly property color surfaceHi: oled ? "#161618" : dark ? "#1f2024" : "#ebebef"
    readonly property color surfaceHi2: oled ? "#202023" : dark ? "#28292e" : "#e0e0e6"
    readonly property color line: dark ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color lineStrong: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.16)
    readonly property color hover: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.05)
    readonly property color text: dark ? "#eeeef0" : "#141518"
    readonly property color textMuted: dark ? "#a2a4ab" : "#585b63"
    readonly property color textFaint: dark ? "#6d7078" : "#8c8f97"
    readonly property color danger: dark ? "#ff6b6b" : "#d63b3b"
    readonly property color scrim: Qt.rgba(0, 0, 0, 0.55)

    // Shape: posters/cards 10, buttons and inputs 8, chips fully rounded
    readonly property int radiusSm: 6
    readonly property int radius: 8
    readonly property int radiusLg: 10
    readonly property int radiusXl: 14

    readonly property int fontXs: 12
    readonly property int fontSm: 13
    readonly property int fontMd: 14
    readonly property int fontLg: 16
    readonly property int fontXl: 20
    readonly property int fontXxl: 26
    readonly property int fontDisplay: 34

    readonly property int durFast: 120
    readonly property int durMed: 220
    readonly property int durSlow: 380

    readonly property var accents: [
        { name: "Корал", value: "#ff6a4d" },
        { name: "Смарагд", value: "#2fbf71" },
        { name: "Блакить", value: "#3d8bff" },
        { name: "Бурштин", value: "#f2a93b" },
        { name: "Малина", value: "#ef4f86" }
    ]

    function luminance(c) {
        return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
    }
    function icon(name) {
        return "qrc:/qt/qml/Andub/resources/icons/" + name + ".svg"
    }
    function poster(url) {
        return url ? "image://poster/" + encodeURIComponent(url) : ""
    }
    function formatTime(sec) {
        if (!(sec > 0)) sec = 0
        sec = Math.floor(sec)
        const h = Math.floor(sec / 3600), m = Math.floor(sec % 3600 / 60), s = sec % 60
        const mm = (h > 0 && m < 10 ? "0" : "") + m
        return (h > 0 ? h + ":" : "") + mm + ":" + (s < 10 ? "0" : "") + s
    }
}
