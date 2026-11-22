import QtQuick
import QtQuick.Layouts
import Andub

// Poster grid for the provider catalog and for favorites.
Item {
    id: page
    signal openAnime(int row)

    readonly property bool favorites: Catalog.favoritesMode
    readonly property bool narrow: width < 860
    readonly property int side: width >= 1280 ? 40 : (narrow ? 18 : 28)
    property string ambientPoster: ""
    property double entranceStart: 0

    Connections {
        target: Catalog
        // Back/forward restores the query from history: show it in the field
        function onQueryChanged() {
            if (search.text !== Catalog.query) {
                searchDebounce.stop()
                search.text = Catalog.query
            }
        }
        function onGenerationChanged() {
            page.entranceStart = Date.now()
            grid.contentY = grid.originY
            if (Catalog.results.count > 0 && !page.ambientPoster)
                page.ambientPoster = grid.posterAt(0)
        }
    }

    Backdrop {
        width: parent.width
        height: 460
        poster: page.ambientPoster
    }

    Timer {
        id: hoverDebounce
        interval: 140
        property string pending
        onTriggered: page.ambientPoster = pending
    }

    // Typing debounce: the request goes out once the user pauses
    Timer {
        id: searchDebounce
        interval: 350
        onTriggered: Catalog.query = search.text
    }

    ColumnLayout {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: page.side
        anchors.rightMargin: page.side
        anchors.topMargin: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                visible: !page.narrow
                text: page.favorites ? "Улюблені" : "Каталог"
                color: Theme.text
                font.pixelSize: Theme.fontXxl
                font.weight: Font.Bold
                font.letterSpacing: -0.4
            }
            Text {
                visible: !page.narrow && page.favorites && Catalog.results.count > 0
                Layout.alignment: Qt.AlignBaseline
                text: Catalog.results.count
                color: Theme.textFaint
                font.pixelSize: Theme.fontXl
                font.weight: Font.Bold
            }
            Item { Layout.fillWidth: true; visible: !page.narrow }

            SearchField {
                id: search
                Layout.fillWidth: page.narrow
                Layout.preferredWidth: page.narrow ? -1 : Math.min(440, page.width * 0.36)
                placeholderText: page.favorites ? "Пошук в улюблених" : "Пошук аніме"
                text: Catalog.query
                onTextEdited: searchDebounce.restart()
                onAccepted: { searchDebounce.stop(); Catalog.query = text }
            }

            AppButton {
                id: sortButton
                readonly property var options: {
                    const sp = Catalog.support
                    return [
                        { label: "Як на сайті", value: 0, ok: true },
                        { label: "Нещодавно оновлені", value: 1, ok: sp.sortUpdated > 0 },
                        { label: "За рейтингом", value: 2, ok: sp.sortRating > 0 },
                        { label: "Спершу новіші", value: 3, ok: sp.sortYear > 0 },
                        { label: "Спершу старіші", value: 4, ok: sp.sortYear > 0 },
                        { label: "За назвою", value: 5, ok: sp.sortTitle > 0 },
                    ].filter(o => o.ok)
                }
                readonly property string label: {
                    for (const o of options) if (o.value === Catalog.sort) return o.label
                    return "Як на сайті"
                }
                kind: "secondary"
                iconName: "arrows-down-up"
                text: page.narrow ? "" : label
                tip: page.narrow ? "Сортування: " + label : "Сортування"
                implicitHeight: 42
                onClicked: sortMenu.open()

                MenuSheet {
                    id: sortMenu
                    y: parent.height + 6
                    x: parent.width - width
                    title: "Сортування"
                    value: Catalog.sort
                    model: sortButton.options
                    onPicked: v => Catalog.sort = v
                }
            }

            AppButton {
                id: filterButton
                kind: Catalog.activeFilterCount > 0 ? "primary" : "secondary"
                iconName: "funnel"
                text: page.narrow ? "" : (Catalog.activeFilterCount > 0 ? "Фільтри · " + Catalog.activeFilterCount : "Фільтри")
                tip: page.narrow ? "Фільтри" : ""
                implicitHeight: 42
                onClicked: filterPanel.open()
            }
        }

        // Provider switcher
        ListView {
            id: providers
            visible: !page.favorites
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            orientation: ListView.Horizontal
            spacing: 8
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: Catalog.providers
            currentIndex: Catalog.providerIndex
            delegate: Chip {
                required property string modelData
                required property int index
                text: modelData
                checked: index === Catalog.providerIndex
                onClicked: Catalog.providerIndex = index
            }
        }

        // Active filters, each removable on its own
        Flow {
            id: activeFilters
            Layout.fillWidth: true
            visible: tokens.length > 0
            spacing: 6
            readonly property var tokens: {
                const t = []
                const seasons = ["", "Зима", "Весна", "Літо", "Осінь"]
                const kinds = ["", "ТБ-серіал", "Фільм", "OVA", "ONA", "Спешл"]
                const sp = Catalog.support
                if (sp.years > 0 && (Catalog.yearFrom || Catalog.yearTo)) {
                    const l = Catalog.yearFrom, u = Catalog.yearTo
                    t.push({ label: l && u ? (l === u ? String(l) : l + "–" + u) : (l ? "з " + l : "до " + u), key: "years" })
                }
                if (sp.season > 0 && Catalog.season) t.push({ label: seasons[Catalog.season], key: "season" })
                if (sp.kinds > 0) for (const k of Catalog.kinds) t.push({ label: kinds[k], key: "kind", value: k })
                if (sp.airing > 0 && Catalog.airing) t.push({ label: Catalog.airing === 1 ? "Онґоїнґ" : "Завершений", key: "airing" })
                if (sp.age > 0 && Catalog.maxAge) t.push({ label: "До " + Catalog.maxAge + "+", key: "age" })
                const sel = sp.genresMulti ? Catalog.selectedGenres : Catalog.selectedGenres.slice(0, 1)
                for (const g of sel) {
                    let label = g
                    for (const x of Catalog.genres) if (x.value === g) label = x.label
                    t.push({ label: label, key: "genre", value: g })
                }
                return t
            }
            function remove(tok) {
                switch (tok.key) {
                case "years": Catalog.yearFrom = 0; Catalog.yearTo = 0; break
                case "season": Catalog.season = 0; break
                case "kind": Catalog.toggleKind(tok.value); break
                case "airing": Catalog.airing = 0; break
                case "age": Catalog.maxAge = 0; break
                case "genre": Catalog.toggleGenre(tok.value); break
                }
            }

            Repeater {
                model: activeFilters.tokens
                Chip {
                    id: tokenChip
                    required property var modelData
                    small: true
                    checkable: false
                    checked: true
                    text: modelData.label + "   ✕"
                    onClicked: activeFilters.remove(modelData)
                }
            }
            Chip {
                small: true
                checkable: false
                visible: activeFilters.tokens.length > 1
                text: "Скинути все"
                onClicked: Catalog.resetFilters()
            }
        }
    }

    // Thin indeterminate bar while a new result set is loading over an old one
    Rectangle {
        id: refreshBar
        anchors.top: header.bottom
        anchors.topMargin: 14
        x: page.side
        width: page.width - page.side * 2
        height: 2
        radius: 1
        color: Theme.line
        clip: true
        visible: Catalog.loading && Catalog.results.count > 0
        Rectangle {
            width: parent.width * 0.3
            height: parent.height
            radius: 1
            color: Theme.accent
            NumberAnimation on x {
                from: -refreshBar.width * 0.3
                to: refreshBar.width
                duration: 1100
                loops: Animation.Infinite
                running: refreshBar.visible
            }
        }
    }

    GridView {
        id: grid
        anchors.top: header.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        leftMargin: page.side
        rightMargin: page.side - gap
        topMargin: 18  // room for the hover ring and lift of the first row (the view clips)
        bottomMargin: 32
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: 1200
        model: Catalog.results
        visible: Catalog.results.count > 0
        opacity: Catalog.loading ? 0.45 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.durMed } }

        readonly property int gap: page.narrow ? 14 : 22
        readonly property int minCard: page.narrow ? 140 : 168
        readonly property int columns: Math.max(2, Math.floor((width - leftMargin - rightMargin + 0) / (minCard + gap)))
        cellWidth: Math.floor((width - leftMargin - rightMargin) / columns)
        cellHeight: Math.round((cellWidth - gap) * 1.45) + 78 + gap

        function posterAt(row) {
            const item = itemAtIndex(row)
            return item ? item.posterUrl : ""
        }

        onContentYChanged: {
            if (contentHeight > 0 && contentY + height > contentHeight - cellHeight * 2)
                Catalog.loadMore()
        }

        delegate: Item {
            id: cell
            required property int index
            required property string title
            required property string poster
            required property string rating
            required property int year
            required property string status
            required property string ageRating
            required property string providerName
            readonly property string posterUrl: poster

            width: grid.cellWidth
            height: grid.cellHeight

            AnimeCard {
                width: parent.width - grid.gap
                title: cell.title
                poster: cell.poster
                rating: cell.rating
                meta: [cell.year > 0 ? cell.year : "",
                       page.favorites ? cell.providerName : (cell.ageRating || cell.status)]
                      .filter(s => s !== "").join("  ·  ")
                entranceDelay: (Date.now() - page.entranceStart < 600) ? Math.min(cell.index, 20) * 28 : -1
                onClicked: page.openAnime(cell.index)
                onHovered: {
                    hoverDebounce.pending = cell.poster
                    hoverDebounce.restart()
                }
            }
        }

        footer: Item {
            width: grid.width - grid.leftMargin - grid.rightMargin
            height: Catalog.loadingMore ? 72 : 0
            Spinner {
                anchors.centerIn: parent
                running: Catalog.loadingMore
                size: 26
            }
        }
    }

    // First load: skeleton grid in the final layout
    Grid {
        anchors.top: header.bottom
        anchors.topMargin: 24
        x: page.side
        columns: grid.columns
        columnSpacing: grid.gap
        rowSpacing: grid.gap + 78
        visible: Catalog.loading && Catalog.results.count === 0
        Repeater {
            model: grid.columns * 2
            Skeleton {
                width: grid.cellWidth - grid.gap
                height: Math.round(width * 1.45)
                radius: Theme.radiusLg
            }
        }
    }

    EmptyState {
        anchors.centerIn: grid
        visible: !Catalog.loading && Catalog.results.count === 0
        iconName: page.favorites ? "heart" : "magnifying-glass"
        title: {
            if (page.favorites && !Catalog.query && !Catalog.activeFilterCount) return "Тут поки порожньо"
            return "Нічого не знайдено"
        }
        message: {
            if (page.favorites && !Catalog.query && !Catalog.activeFilterCount)
                return "Додавайте тайтли до улюблених на їхній сторінці, і вони з'являться тут."
            if (Catalog.query || Catalog.activeFilterCount) return "Спробуйте інший запит або приберіть частину фільтрів. Частину фільтрів джерело перевіряє лише на кількох сторінках, тож прокрутка може знайти ще."
            return "Джерело не відповіло. Можливо, сайт тимчасово недоступний."
        }
        actionText: (Catalog.query || Catalog.activeFilterCount) ? "Скинути фільтри"
                    : (page.favorites ? "Перейти до каталогу" : "Спробувати ще раз")
        onAction: {
            if (Catalog.query || Catalog.activeFilterCount) {
                search.text = ""
                Catalog.query = ""
                Catalog.resetFilters()
            } else if (page.favorites) {
                Catalog.favoritesMode = false
            } else {
                Catalog.refresh()
            }
        }
    }

    FilterPanel { id: filterPanel }

    // Typing anywhere on the page starts a search
    Keys.onPressed: event => {
        if (event.text.length === 1 && event.text.trim() !== "" && !(event.modifiers & Qt.ControlModifier)) {
            search.forceActiveFocus()
            search.text += event.text
            searchDebounce.restart()
            event.accepted = true
        }
    }
    function focusSearch() { search.forceActiveFocus(); search.selectAll() }
    function openFilters() { filterPanel.open() }
    // Back to the front page of the current source (see Catalog.goHome)
    function goHome() {
        searchDebounce.stop()
        search.text = ""
        filterPanel.close()
        grid.contentY = grid.originY
        Catalog.goHome()
    }
}
