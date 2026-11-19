# Andub

Десктопний застосунок для пошуку, перегляду та завантаження аніме з українською та російською озвучкою.
Версія для **Windows 10/11 (x64)**. Ядро написане на **C++20** (MSVC), інтерфейс на **Qt 6 Quick (QML)**, відео відтворюється через **libmpv**, мережа через вбудований у Windows **WinHTTP**.

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
- **Улюблені та історія переглядів.**
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

## Встановлення

Завантажте `Andub-Setup-x86_64.exe` з [релізів](https://github.com/navik677/andub/releases) і запустіть.
Інсталятор ставить програму в `%LOCALAPPDATA%\Programs\Andub` без прав адміністратора, створює ярлики й
пункт у «Програми та компоненти». У «Параметрах» інсталятора можна вибрати іншу теку. Тихе встановлення:
`Andub-Setup-x86_64.exe /quiet` (`DESKTOP_SHORTCUT=0` пропускає ярлик на робочому столі, `INSTALLFOLDER="..."` задає теку).

Якщо стоїть стара версія з NSIS-інсталятора (1.x або 2.0), інсталятор попросить спершу видалити її
в «Параметри → Програми». Улюблені, історія й налаштування при цьому залишаться.

Є й портативна версія `Andub-Windows-x86_64.zip`: розпакуйте й запустіть `andub.exe`.

Усе потрібне вже в комплекті: Qt, libmpv, `mpv.exe` для зовнішнього плеєра, `yt-dlp.exe` для завантажень
і runtime MSVC. Окремо нічого ставити не треба.

## Збірка з вихідного коду

Потрібні:
- **Visual Studio 2022** або **Build Tools for Visual Studio 2022** з робочим навантаженням «Desktop development with C++»;
- **Qt 6.6+** для `msvc2022_64` з модулем `qtimageformats`;
- **CMake 3.21+**. WiX Toolset для інсталятора скрипт завантажує сам.

```powershell
winget install Microsoft.VisualStudio.2022.BuildTools Kitware.CMake
pip install aqtinstall
python -m aqt install-qt windows desktop 6.10.3 win64_msvc2022_64 -m qtimageformats -O $HOME\Qt
.\scripts\build_windows.ps1
```

Скрипт сам знаходить Visual Studio і Qt у `%USERPROFILE%\Qt` (або візьміть `-QtDir`), а `scripts/fetch_deps.ps1`
завантажує в `third_party\` готовий libmpv і `mpv.exe` ([shinchiro/mpv-winbuild-cmake](https://github.com/shinchiro/mpv-winbuild-cmake)),
а також `yt-dlp.exe` і WiX Toolset 6. Що виходить у `dist\`:
- `Andub\`: готова тека з програмою;
- `Andub-Windows-x86_64.zip`: портативна версія;
- `Andub-Setup-x86_64.exe`: інсталятор одним файлом. Це WiX-бандл, усередині якого MSI-пакет (`installer/Package.wxs`, `installer/Bundle.wxs`).

`-Tests` додає тестові програми (`test_core`, `test_providers`, ...), `-NoInstaller` пропускає інсталятор.
CI (`.github/workflows/build-release.yml`) робить те саме на `windows-2022` і публікує реліз для тегів `v*`.

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
│   └── utils/              # HTTP (WinHTTP), JSON, Kodik, шляхи, рядки, mpv/yt-dlp
├── qml/
│   ├── Main.qml            # вікно, навігація, плеєр поверх усього
│   ├── Theme.qml           # дизайн-токени: кольори, радіуси, шрифти, анімації
│   ├── components/         # кнопки, картки, сітка серій, шкала перемотки, тости...
│   └── pages/              # Каталог, Тайтл, Плеєр, Завантаження, Налаштування
├── resources/
│   ├── fonts/              # Manrope (OFL)
│   ├── icons/              # Phosphor Icons (MIT)
│   └── shaders/            # шейдери апскейлу
├── installer/              # MSI (WiX) і бандл Setup.exe
├── scripts/                # build_windows.ps1, fetch_deps.ps1
└── third_party/            # libmpv, mpv.exe, yt-dlp.exe (завантажуються, не в git)
```

Налаштування, улюблені та історія зберігаються в `%LOCALAPPDATA%\anime-gui`, кеш постерів у `%LOCALAPPDATA%\cache\anime-gui`. Серії завантажуються в `Завантаження\Anime`.

---

## Ліцензія

MIT. Сторонні ресурси мають власні ліцензії:
- шрифт Manrope: SIL OFL 1.1 (`resources/fonts/OFL-manrope.txt`);
- іконки Phosphor: MIT (`resources/icons/LICENSE-phosphor.txt`);
- шейдери в `resources/shaders/`: FSR (MIT, AMD), Anime4K (MIT, див. `Anime4K_LICENSE`), FSRCNNX (LGPLv3), adaptive-sharpen (BSD-2-Clause).
