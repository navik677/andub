import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Andub

// Side sheet with catalog filters. Every control writes straight to Catalog,
// which debounces and reloads; sections the current source can't filter on
// are dimmed with a short reason.
T.Popup {
    id: panel
    parent: T.Overlay.overlay
    x: parent ? parent.width - width : 0
    y: 0
    width: parent ? Math.min(420, parent.width) : 420
    height: parent ? parent.height : 600
    modal: true
    dim: true
    focus: true
    padding: 0
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    readonly property var support: Catalog.support
    readonly property string sourceName: Catalog.favoritesMode ? "Улюблені" : Catalog.providers[Catalog.providerIndex]

    T.Overlay.modal: Rectangle {
        color: Theme.scrim
        Behavior on opacity { NumberAnimation { duration: Theme.durMed } }
    }

    enter: Transition {
        NumberAnimation { property: "x"; from: panel.parent ? panel.parent.width : 0; to: panel.parent ? panel.parent.width - panel.width : 0; duration: Theme.durSlow; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "x"; to: panel.parent ? panel.parent.width : 0; duration: Theme.durMed; easing.type: Easing.InCubic }
    }

    background: Rectangle {
        color: Theme.dark ? Theme.surface : Theme.bg
        Rectangle { width: 1; height: parent.height; color: Theme.lineStrong }
    }

    // One titled block; `available` false dims it and shows `reason`
    component Section: ColumnLayout {
        id: sec
        property string title
        property bool available: true
        property string reason
        property string hint
        default property alias content: body.data
        Layout.fillWidth: true
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: sec.title
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
                font.weight: Font.Bold
                font.letterSpacing: 0.6
                font.capitalization: Font.AllUppercase
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: sec.hint !== "" && sec.available
                text: sec.hint
                color: Theme.textFaint
                font.pixelSize: Theme.fontXs
            }
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: body.childrenRect.height
            opacity: sec.available ? 1 : 0.35
            enabled: sec.available
            Item { id: body; width: parent.width; height: childrenRect.height }
        }
        Text {
            visible: !sec.available && sec.reason !== ""
            Layout.fillWidth: true
            text: sec.reason
            color: Theme.textFaint
            font.pixelSize: Theme.fontXs
            wrapMode: Text.WordWrap
        }
    }

    component Pick: Chip {
        property var value
        property bool on: false
        checkable: false
        checked: on
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 14
            Layout.topMargin: 18
            Layout.bottomMargin: 6
            ColumnLayout {
                spacing: 2
                Text {
                    text: "Фільтри"
                    color: Theme.text
                    font.pixelSize: Theme.fontXl
                    font.weight: Font.Bold
                }
                Text {
                    text: panel.sourceName
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSm
                }
            }
            Item { Layout.fillWidth: true }
            IconButton {
                iconName: "x"
                tip: "Закрити"
                onClicked: panel.close()
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: sections.implicitHeight + 24
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: sections
                x: 24
                width: flick.width - 48
                y: 12
                spacing: 26

                Section {
                    title: "Роки"
                    available: panel.support.years > 0
                    reason: panel.sourceName + " не вказує рік у списку."
                    YearRange {
                        width: parent.width
                        from: Catalog.minYear
                        to: Catalog.maxYear
                        lower: Catalog.yearFrom
                        upper: Catalog.yearTo
                        onEdited: (lo, hi) => { Catalog.yearFrom = lo; Catalog.yearTo = hi }
                    }
                }

                Section {
                    title: "Сезон"
                    available: panel.support.season > 0
                    reason: panel.sourceName + " не вказує сезон виходу."
                    Flow {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: [{ l: "Будь-який", v: 0 }, { l: "Зима", v: 1 }, { l: "Весна", v: 2 }, { l: "Літо", v: 3 }, { l: "Осінь", v: 4 }]
                            Pick {
                                required property var modelData
                                text: modelData.l
                                on: Catalog.season === modelData.v
                                onClicked: Catalog.season = modelData.v
                            }
                        }
                    }
                }

                Section {
                    title: "Тип"
                    available: panel.support.kinds > 0
                    reason: panel.sourceName + " не вказує тип (ТВ, фільм, OVA) у списку."
                    hint: Catalog.kinds.length > 1 ? "будь-який з обраних" : ""
                    Flow {
                        width: parent.width
                        spacing: 8
                        Pick {
                            text: "Будь-який"
                            on: Catalog.kinds.length === 0
                            onClicked: Catalog.kinds = []
                        }
                        Repeater {
                            model: [{ l: "ТБ-серіал", v: 1 }, { l: "Фільм", v: 2 }, { l: "OVA", v: 3 }, { l: "ONA", v: 4 }, { l: "Спешл", v: 5 }]
                            Pick {
                                required property var modelData
                                text: modelData.l
                                on: Catalog.kinds.indexOf(modelData.v) >= 0
                                onClicked: Catalog.toggleKind(modelData.v)
                            }
                        }
                    }
                }

                Section {
                    title: "Статус"
                    available: panel.support.airing > 0
                    reason: panel.sourceName + " не вказує, чи тайтл ще виходить."
                    Flow {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: [{ l: "Будь-який", v: 0 }, { l: "Онґоїнґ", v: 1 }, { l: "Завершений", v: 2 }]
                            Pick {
                                required property var modelData
                                text: modelData.l
                                on: Catalog.airing === modelData.v
                                onClicked: Catalog.airing = modelData.v
                            }
                        }
                    }
                }

                Section {
                    title: "Вік"
                    available: panel.support.age > 0
                    reason: panel.sourceName + " не вказує вікове обмеження."
                    Flow {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: [{ l: "Будь-який", v: 0 }, { l: "До 6+", v: 6 }, { l: "До 12+", v: 12 }, { l: "До 16+", v: 16 }]
                            Pick {
                                required property var modelData
                                text: modelData.l
                                on: Catalog.maxAge === modelData.v
                                onClicked: Catalog.maxAge = modelData.v
                            }
                        }
                    }
                }

                Section {
                    title: Catalog.selectedGenres.length ? "Жанри · " + Catalog.selectedGenres.length : "Жанри"
                    hint: panel.support.genresMulti ? (Catalog.selectedGenres.length > 1 ? "усі обрані одночасно" : "")
                                                    : "лише один жанр"
                    Flow {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: Catalog.genres
                            Pick {
                                required property var modelData
                                small: true
                                text: modelData.label
                                on: Catalog.selectedGenres.indexOf(modelData.value) >= 0
                                onClicked: Catalog.toggleGenre(modelData.value)
                            }
                        }
                    }
                }
            }
        }

        // Footer
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            spacing: 10
            AppButton {
                kind: "ghost"
                text: "Скинути"
                enabled: Catalog.activeFilterCount > 0
                onClicked: Catalog.resetFilters()
            }
            Item { Layout.fillWidth: true }
            AppButton {
                kind: "primary"
                text: Catalog.loading ? "Шукаю…" : (Catalog.results.count > 0 ? "Показати " + Catalog.results.count + (Catalog.hasMore ? "+" : "") : "Нічого не знайдено")
                onClicked: panel.close()
            }
        }
    }
}
