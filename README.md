# Andub

Десктопний застосунок для пошуку, перегляду та завантаження аніме з українською та російською озвучкою.
Ядро написане на **C++20**, інтерфейс на **Qt 6 Quick (QML)**, відео відтворюється через **libmpv**.
Працює на Linux і Windows. macOS збирається тим самим CMake-проєктом, але поки не перевірявся.

---

## Можливості

- **Каталог із постерами.** Адаптивна сітка, яка підлаштовується під ширину вікна, нескінченне прокручування сторінок і фонове підсвічування з обкладинки тайтлу під курсором.
- **Пошук і жанри.** Пошук починається, щойно ви перестали друкувати. Є фільтр за жанром, а `жанр: драма` у рядку пошуку теж працює.
- **8 джерел:** АніЛібрія, Dream Cast, Shiza Project, AniBaza, AniDub, AniTube (UA), AnimeVost, AniStar.
- **Сторінка тайтлу:** опис, жанри, рейтинг, кнопка «Продовжити» з наступної непереглянутої серії та сітка серій з позначками переглянутого.
- **Вбудований плеєр (libmpv):**
  - шкала перемотки з буферизацією та підказкою часу;
  - автоперехід на наступну серію та список серій;
  - пропуск опенінгу, повний екран, керування з клавіатури (працює і в кириличній розкладці);
  - кнопка «Відкрити в mpv».
- **GPU-апскейл у реальному часі:** FSR 1.0, FSRCNNX, Anime4K, Anime4K HQ (`Ctrl+U` або меню в плеєрі).
- **Завантаження** серій через `yt-dlp` у фоні, з прогресом, швидкістю та часом, що залишився.
- **Улюблені та історія переглядів.** Дані старої GTK-версії підхоплюються автоматично.
- **Оформлення:** темна, OLED або світла тема та 5 акцентних кольорів.

## Клавіші плеєра

| Клавіша | Дія |
|---|---|
| Пробіл | Пауза |
| ← / → | Перемотка на 10 с |
| ↑ / ↓ | Гучність |
| M | Вимкнути звук |
| S | Пропустити опенінг (+85 с) |
| N / P | Наступна / попередня серія |
| F, F11, подвійний клік | Повний екран |
| Ctrl+U | Змінити апскейл |
| Esc | Вийти з повного екрана / закрити плеєр |

---

## Встановлення з вихідного коду (Linux)

```bash
./install.sh      # встановлює залежності, збирає та ставить у ~/.local
./uninstall.sh    # видалення (улюблені та історія залишаються)
```

### Залежності

Потрібні Qt **6.6 або новіший**, CMake, libmpv та libcurl. `mpv` і `yt-dlp` потрібні для зовнішнього плеєра та завантажень.

**Fedora**
```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel \
    qt6-qtsvg qt6-qtimageformats mpv-devel libcurl-devel mpv yt-dlp
```

**Arch Linux**
```bash
sudo pacman -S base-devel cmake ninja qt6-base qt6-declarative qt6-svg qt6-imageformats mpv curl yt-dlp
```

**Ubuntu 25.04+ / Debian 13+**
```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
    qml6-module-qtquick-layouts qml6-module-qtquick-effects qml6-module-qtquick-shapes \
    qml6-module-qtqml-workerscript qt6-image-formats-plugins libqt6svg6 \
    libmpv-dev libcurl4-openssl-dev mpv yt-dlp
```
Ubuntu 24.04 має Qt 6.4, цього замало. Там використовуйте AppImage.

### Ручна збірка

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/andub
```

`-DANDUB_BUILD_TESTS=ON` додає тестові програми (`test_core`, `test_providers`, ...).

## Пакунки

- **Linux AppImage:** `./scripts/build_appimage.sh` (використовує linuxdeploy і його Qt-плагін).
- **Windows:** `./scripts/build_windows.sh` в оболонці MSYS2 MINGW64 (список пакетів pacman є на початку скрипта). На виході:
  - `Andub-Setup-x86_64.exe`: інсталятор одним файлом (NSIS, `installer/andub.nsi`). Ставить у `%LOCALAPPDATA%\Programs\Andub` без прав адміністратора, з ярликами й деінсталятором;
  - `Andub-Windows-x86_64.zip`: портативна версія.
- **CI:** `.github/workflows/build-release.yml` збирає обидва варіанти та публікує реліз для тегів `v*`.

---

## Структура проєкту

```
andub/
├── CMakeLists.txt
├── src/
│   ├── main.cpp            # запуск Qt, OpenGL для сцени, шрифт, постер-провайдер
│   ├── app/                # міст C++ ↔ QML: Catalog, Details, PlayerSession,
│   │                       # Downloads, Settings, MpvPlayer (libmpv у Qt Quick), моделі списків
│   ├── models/             # Anime, Episode, Stream
│   ├── providers/          # джерела озвучки
│   ├── services/           # кеш постерів, історія, улюблені, завантаження, апскейлер
│   └── utils/              # HTTP (libcurl), JSON, Kodik, шляхи, рядки
├── qml/
│   ├── Main.qml            # вікно, навігація, плеєр поверх усього
│   ├── Theme.qml           # дизайн-токени: кольори, радіуси, шрифти, анімації
│   ├── components/         # кнопки, картки, сітка серій, шкала перемотки, тости...
│   └── pages/              # Каталог, Тайтл, Плеєр, Завантаження, Налаштування
├── resources/
│   ├── fonts/              # Manrope (OFL)
│   ├── icons/              # Phosphor Icons (MIT)
│   └── shaders/            # шейдери апскейлу
├── desktop/                # .desktop-файл
└── scripts/                # AppImage, Windows, MSI
```

Налаштування, улюблені та історія зберігаються в `~/.config/anime-gui/` (на Windows у `%LOCALAPPDATA%\anime-gui`), кеш постерів у `~/.cache/anime-gui/`.

---

## Ліцензія

MIT. Сторонні ресурси мають власні ліцензії:
- шрифт Manrope: SIL OFL 1.1 (`resources/fonts/OFL-manrope.txt`);
- іконки Phosphor: MIT (`resources/icons/LICENSE-phosphor.txt`);
- шейдери в `resources/shaders/`: FSR (MIT, AMD), Anime4K (MIT, див. `Anime4K_LICENSE`), FSRCNNX (LGPLv3), adaptive-sharpen (BSD-2-Clause).
