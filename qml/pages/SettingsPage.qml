import QtQuick
import QtQuick.Layouts
import Andub

Flickable {
    id: page
    readonly property int side: width >= 1280 ? 40 : (width < 860 ? 18 : 28)
    contentHeight: column.implicitHeight + 64
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    component Section: ColumnLayout {
        id: section
        property string title
        default property alias content: body.data
        Layout.fillWidth: true
        spacing: 14
        Text {
            text: section.title
            color: Theme.text
            font.pixelSize: Theme.fontLg
            font.weight: Font.Bold
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: body.implicitHeight + 8
            radius: Theme.radiusXl
            color: Theme.surface
            border.width: 1
            border.color: Theme.line
            ColumnLayout {
                id: body
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 4
                spacing: 0
            }
        }
    }

    component SettingRow: RowLayout {
        id: settingRow
        property string label
        property string hint
        default property alias control: slot.data
        Layout.fillWidth: true
        Layout.leftMargin: 18
        Layout.rightMargin: 14
        Layout.topMargin: 12
        Layout.bottomMargin: 12
        spacing: 16
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text {
                text: settingRow.label
                color: Theme.text
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: settingRow.hint
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
                wrapMode: Text.Wrap
            }
        }
        Item {
            id: slot
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        Layout.leftMargin: 18
        height: 1
        color: Theme.line
    }

    ColumnLayout {
        id: column
        x: page.side
        y: 24
        width: Math.min(page.width - page.side * 2, 760)
        spacing: 28

        Text {
            text: "Налаштування"
            color: Theme.text
            font.pixelSize: Theme.fontXxl
            font.weight: Font.Bold
            font.letterSpacing: -0.4
        }

        Section {
            title: "Вигляд"
            SettingRow {
                label: "Тема"
                Segmented {
                    model: [{ label: "Темна", value: "dark" }, { label: "OLED", value: "oled" }, { label: "Світла", value: "light" }]
                    value: Settings.theme
                    onActivated: v => Settings.theme = v
                }
            }
            Divider {}
            SettingRow {
                label: "Акцентний колір"
                hint: "Кнопки, прогрес перегляду, активні елементи"
                Row {
                    spacing: 10
                    Repeater {
                        model: Theme.accents
                        Rectangle {
                            required property var modelData
                            readonly property bool selected: Settings.accent.toLowerCase() === modelData.value
                            width: 28; height: 28; radius: 14
                            color: modelData.value
                            border.width: selected ? 3 : 0
                            border.color: Theme.surface
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: -3
                                radius: width / 2
                                color: "transparent"
                                border.width: 2
                                border.color: parent.selected ? parent.color : "transparent"
                            }
                            HoverHandler { id: sw; cursorShape: Qt.PointingHandCursor }
                            AppToolTip { visible: sw.hovered; text: modelData.name }
                            TapHandler { onTapped: Settings.accent = modelData.value }
                        }
                    }
                }
            }
        }

        Section {
            title: "Відтворення"
            SettingRow {
                label: "Плеєр"
                hint: Settings.externalPlayer ? "Серії відкриваються в окремому вікні mpv" : "Серії відтворюються всередині застосунку"
                Segmented {
                    model: [{ label: "Вбудований", value: false }, { label: "mpv", value: true }]
                    value: Settings.externalPlayer
                    onActivated: v => Settings.externalPlayer = v
                }
            }
            Divider {}
            SettingRow {
                label: "Апскейл у вбудованому плеєрі"
                hint: Settings.shadersAvailable
                      ? "Покращення картинки на відеокарті. Помітно, коли вікно більше за відео, найкраще у повноекранному режимі."
                      : "Файли шейдерів не знайдено, апскейл недоступний."
                AppButton {
                    compact: true
                    enabled: Settings.shadersAvailable
                    iconName: "sparkle"
                    text: Settings.upscalerName(Settings.upscaler)
                    onClicked: upMenu.open()
                    MenuSheet {
                        id: upMenu
                        y: parent.height + 6
                        x: parent.width - width
                        value: Settings.upscaler
                        model: Settings.upscalerProfiles.map(p => ({ label: p.name, value: p.id }))
                        onPicked: v => Settings.upscaler = v
                    }
                }
            }
        }

        Section {
            title: "Клавіші плеєра"
            GridLayout {
                Layout.fillWidth: true
                Layout.margins: 18
                columns: page.width < 700 ? 1 : 2
                columnSpacing: 32
                rowSpacing: 12
                Repeater {
                    model: [
                        ["Пробіл", "Пауза"], ["← →", "Перемотка на 10 с"],
                        ["↑ ↓", "Гучність"], ["M", "Вимкнути звук"],
                        ["S", "Пропустити опенінг"], ["N / P", "Наступна / попередня серія"],
                        ["F", "Повний екран"], ["Ctrl+U", "Змінити апскейл"]
                    ]
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 12
                        Rectangle {
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 26
                            radius: Theme.radiusSm
                            color: Theme.surfaceHi
                            border.width: 1
                            border.color: Theme.line
                            Text {
                                anchors.centerIn: parent
                                text: modelData[0]
                                color: Theme.text
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.Bold
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData[1]
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                        }
                    }
                }
            }
        }

        Section {
            title: "Про застосунок"
            SettingRow {
                label: "Тека завантажень"
                hint: Settings.downloadsPath
                AppButton {
                    compact: true
                    iconName: "folder-open"
                    text: "Відкрити"
                    onClicked: Settings.openFolder(Settings.downloadsPath)
                }
            }
            Divider {}
            SettingRow {
                label: "Andub " + Settings.version
                hint: "Відтворення через libmpv, інтерфейс на Qt Quick"
            }
            Divider {}
            SettingRow {
                label: "Автор"
                hint: "github.com/navik677"
                AppButton {
                    compact: true
                    iconName: "github-logo"
                    text: "GitHub"
                    tip: "Відкрити профіль у браузері"
                    onClicked: Qt.openUrlExternally("https://github.com/navik677")
                }
            }
        }
    }
}
