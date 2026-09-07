# Anime GUI 🌸 (C++20 & GTK4)

Сучасний графічний застосунок (GUI) мовою **C++20** на базі фреймворку **GTK4** для пошуку, перегляду та завантаження аніме.
Повна нативна заміна старої консольної версії: швидка, красива, з підтримкою багатьох провайдерів і низьким споживанням ресурсів.

---

## Особливості 🌟
- 🎨 **Сучасний інтерфейс GTK4:** красиві картки тайтлів із закругленими обкладинками високої якості, розмиттям фону при виборі тайтлу, значками рейтингів та плавними анімаціями.
- 🚀 **Миттєвий асинхронний пошук:** живий пошук із затримкою вводу (debounce) без блокування інтерфейсу.
- 🎭 **Пошук за жанрами:** вибір жанрів російською/українською з автоматичною фільтрацією.
- 🎨 **Кольорові схеми (Теми):** Шляхетна темна (Midnight Purple), Смарагдова (Emerald Dark), Неонова кіберпанк (Cyberpunk Neon), Сонячний захід (Sunset Crimson) та Світла (Nordic Light).
- 🔄 **Мультипровайдерність:**
  - 🇺🇦 **AniTube (UA)** — найбільша база українського дубляжу з прямими потоками HLS (.m3u8 Ashdi).
  - ⚡ **Shiza Project** — офіційний GraphQL API студії Shiza Project з повними метаданими та епізодами.
  - 🎬 **AniBaza** — каталог та пошуковий API з інтеграцією Kodik.
  - 🎙️ **AniDub** — офіційний каталог AniDub з прямими стрімами Sibnet (.mp4).
  - 🌸 **АніЛібрія** — офіційний каталог AniLibria з високою якістю до 1080p.
  - 🎞️ **AnimeVost** — величезний архів озвучок AnimeVost.
- 🖼️ **Розумне кешування постерів:** автоматичне завантаження та кешування обкладинок у `~/.cache/anime-gui/covers/`.
- 📺 **Стрімінг через mpv:** швидке відтворення HLS `.m3u8` та прямих потоків з розширеним буфером, автоматичним підхопленням якості та заголовків.
- 💾 **Менеджер фонових завантажень:** завантаження серій через `yt-dlp` у фоні з діалогом прогресу та сповіщеннями робочого столу (`notify-send`).
- ❤️ **Улюблені та історія:** збереження улюблених аніме та відмітки переглянутих серій (`✓`).
- 📦 **Готові пакунки:** автономний **Linux AppImage** та портативний **Windows ZIP bundle**.

---

## Готові релізи (Завантажити) 📦
- **Linux:** [dist/Anime-GUI-x86_64.AppImage](file:///home/ona/anime-tui/dist/Anime-GUI-x86_64.AppImage) (зробіть виконуваним `chmod +x` і запускайте).
- **Windows:** [dist/Anime-GUI-Windows-x86_64.zip](file:///home/ona/anime-tui/dist/Anime-GUI-Windows-x86_64.zip) (розпакуйте та запустіть `anime-gui.exe`).

---

## Встановлення в систему 🛠️

Для автоматичної збірки та встановлення в меню програм:
```bash
./install.sh
```

Для видалення:
```bash
./uninstall.sh
```

### Встановлення залежностей вручну (для збірки з вихідного коду)

#### Fedora
```bash
sudo dnf install -y gcc-c++ meson ninja-build gtk4-devel libcurl-devel mpv yt-dlp
```

#### Ubuntu / Debian
```bash
sudo apt update
sudo apt install -y g++ meson ninja-build libgtk-4-dev libcurl4-openssl-dev mpv yt-dlp
```

#### Arch Linux
```bash
sudo pacman -S gcc meson ninja gtk4 curl mpv yt-dlp
```

---

## Ручна збірка ⚡

```bash
meson setup build --buildtype=release
ninja -C build
./build/anime-gui
```

Для створення AppImage:
```bash
./scripts/build_appimage.sh
```

Для створення Windows збірки (потрібен MinGW-w64):
```bash
./scripts/build_windows.sh
```

---

## Структура проєкту 📁
```
anime-gui/
├── src/
│   ├── main.cpp                  # Точка входу застосунку
│   ├── application.hpp/.cpp      # Ініціалізація GtkApplication та CSS теми
│   ├── models/                   # Моделі даних (Anime, Episode, Stream, Quality)
│   ├── utils/                    # JSON парсер на C++20, HTTP клієнт
│   ├── providers/                # Реалізація провайдерів (Shiza, AniBaza, AniDub, AniTube, AniLibria, AnimeVost)
│   ├── services/                 # Кеш зображень, запуск mpv, історія, улюблені, завантаження, теми
│   └── ui/                       # Віджети GTK4 (Картка аніме, сторінка деталей, діалог завантажень, головне вікно)
├── resources/                    # CSS стилі, іконки
├── desktop/                      # .desktop файл та системні іконки
├── scripts/                      # Скрипти збірки AppImage та Windows
└── dist/                         # Готові бінарні пакунки
```

---

## Ліцензія
MIT
