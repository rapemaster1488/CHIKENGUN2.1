# Native Desktop Guide: сборка и запуск CHIKENGUN 2.5 (NixOS / Linux / macOS / Windows)

Проект полностью работает как единый **нативный C++17 / Urho3D desktop-клиент** (`src/MyApp.cpp`).
Вся функциональность (арена 2.5 «ПЕРО СУДЬБЫ», 12 видов оружия, 4K PBR, «Кочет в баньке», «Золотая Рулетка» + «Кибер-Слоты 777», «Pixel Arcade» на 6 мини-игр, интро-заставка 2.5.6) находится внутри одного нативного бинарника.

---

## 1. Nix / NixOS (Flakes и классический `nix-shell`)

### Вариант A — запуск в 1 команду (Flakes)

```bash
# Соберёт Urho3D 1.9.0 в Nix store, прогонит тесты и сразу запустит игру
nix run
```

### Вариант B — окружение разработчика (Flakes или `nix-shell`)

```bash
# С Flakes (Urho3D 1.9.0 уже собран и подключен через URHO3D_HOME):
nix develop -c ./run.sh

# Без Flakes (классический nix-shell, на первом запуске соберёт кэш .cache/urho3d-sdk):
nix-shell --run ./run.sh
```

> В рабочем Git-дереве Flakes видит изменённые файлы, но не новые неотслеживаемые. Перед локальной проверкой Flake добавьте новые исходники и тесты в индекс Git; после обычного коммита это не требуется.

> На NixOS скрипты `run.sh`, `build.sh` и `flake.nix` автоматически добавляют `/run/opengl-driver/lib`, `/run/opengl-driver-32/lib`, SDL2, X11, Mesa, ALSA, PulseAudio и udev в `LD_LIBRARY_PATH` и пробрасывают `CHIKENGUN_DATA_DIR` / `CHIKENGUN_COREDATA_DIR`.

---

## 2. Linux (Ubuntu / Debian, Fedora, Arch, openSUSE, Alpine, Void) и macOS

```bash
git clone https://github.com/rapemaster1488/CHIKENGUN2.1.git
cd CHIKENGUN2.1

# На чистой машине (поставит системные пакеты, соберёт статический Urho3D 1.9.0 в .cache/urho3d-sdk,
# сгенерирует PBR-текстуры/звуки и запустит игру):
./run.sh --install-deps
```

Поддерживаемые пакетные менеджеры при `--install-deps`: `apt`, `dnf`, `pacman`, `zypper`, `apk` (Alpine musl), `xbps` (Void), `brew` (macOS Intel & Apple Silicon), плюс авто-вход в Nix на NixOS.

После первого запуска:

```bash
./run.sh
```

### Диагностика SDL на Linux

```bash
./run.sh --diagnose-sdl
# Для Flakes та же проверка в установленном клиенте:
nix run . -- --diagnose-sdl
```

Проверка показывает версию и **скомпилированные в используемый Urho3D SDL** видео- и аудиодрайверы, значения `DISPLAY`, `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR`, `SDL_VIDEODRIVER`, `SDL_AUDIODRIVER`, затем пробует подключиться к дисплею, создать небольшое скрытое окно с OpenGL и открыть приостановленное аудиоустройство. Если выбран драйвер `disk`, аудиофайл не открывается и не перезаписывается при проверке. Успех этой минимальной OpenGL-проверки не гарантирует поддержку всех графических настроек игры. Код возврата ненулевой при ошибке дисплея/OpenGL; отсутствие звука не препятствует запуску окна.

- При `No available video device` проверьте, что игра запущена **в вашей X11/Wayland-сессии**, доступен соответствующий сокет и авторизация X11 (`XAUTHORITY`), а для Wayland доступны `XDG_RUNTIME_DIR` и библиотеки клиента. В SSH нужен настроенный X11 forwarding, в WSL — работающий WSLg, в контейнере — корректно проброшенный графический сеанс и OpenGL. Если `DISPLAY`/`WAYLAND_DISPLAY` отсутствуют, сами по себе переменные не создадут дисплей. Игра не включает dummy/offscreen-видеодрайвер вместо окна.
- Если SDL не содержит `x11` или `wayland`, либо не содержит `alsa`/`pulseaudio`, установите dev-пакеты и **пересоберите ранее закэшированный** локальный SDK (обычный `--rebuild` пересобирает только игру):

  ```bash
  ./run.sh --install-deps --rebuild-urho3d
  ```

  Для внешнего SDK (`URHO3D_HOME` или системного пакета) используйте его способ пересборки; локальный флаг внешний SDK не заменяет. Nix Flake собирает свой SDK в store; классический `nix-shell` содержит зависимости для сборки локального кэша. Для намеренной сборки без аудиозаголовков можно использовать `CHIKENGUN_ALLOW_SILENT_BUILD=1`, но звук в таком SDL может отсутствовать.
- Если backend скомпилирован, но недоступен сервер звука, устройство или runtime-библиотека, Linux-клиент сообщает об этом и продолжает без звука. Для намеренно тихого запуска используйте `./run.sh -nosound`; при неудаче инициализации SDL может использовать **только** dummy-аудиодрайвер, если он был собран.
- Заранее выставленные `SDL_VIDEODRIVER` / `SDL_AUDIODRIVER` ограничивают выбор SDL. Если значения устарели, попробуйте `env -u SDL_VIDEODRIVER -u SDL_AUDIODRIVER ./run.sh --diagnose-sdl`. Принудительный `SDL_VIDEODRIVER=x11` имеет смысл только при рабочем X11/XWayland и скомпилированном `x11`; `wayland` — при работающем Wayland и скомпилированном backend.

### Полезные флаги (`./build.sh` и `./run.sh`)

- `./run.sh --4k` / `./build.sh --4k` — генерация полного 4096×4096 набора PBR-текстур + 3840×2160 `LoadingBg4K.png` (по умолчанию `TEX_SIZE=1024`).
- `./build.sh --install-deps` — автоустановка системных пакетов сборки, включая рекомендованные ALSA/PulseAudio и Wayland headers.
- `./run.sh --rebuild-urho3d` — пересобрать **локальный** кэшированный Urho3D SDK после установки недостающих драйверов и затем клиент; внешний `URHO3D_HOME` эта команда не меняет. Для ограничения параллелизма можно добавить `--jobs 4`.
- `./run.sh --diagnose-sdl` — проверить привязанный к клиенту SDL и реальное подключение к дисплею (Linux).
- `./run.sh -nosound` — отключить звук; на Linux также не пытается открыть физическое устройство.
- `./build.sh --tests-only` — сборка и прогон модульных C++17 тестов (`test_core` и Linux SDL-policy) без Urho3D.
- `URHO3D_HOME=/путь/к/sdk ./build.sh` — использовать уже установленный Urho3D SDK. Если в указанном SDK отсутствуют заголовки или библиотека, сборка остановится, а не смешает его с другим SDK.
- `./tools/package_linux.sh` — собрать автономный переносимый архив `build/release/CHIKENGUN-2.5-Linux-<arch>.tar.gz`.

---

## 3. Windows (MSVC / Visual Studio, CMake или MSYS2)

### Нативный PowerShell / Visual Studio (рекомендуется)

```powershell
# Первый запуск сам закэширует Urho3D 1.9.0 (OpenGL + GLSL), сгенерирует PBR-текстуры и запустит игру:
.\run.bat

# Сборка в 4K или только C++17 модульные тесты:
.\build.ps1 -Ultra4K
.\build.ps1 -TestsOnly
```

> Скрипт `build.ps1` и `CMakeLists.txt` принудительно собирают Urho3D для Windows с `-DURHO3D_OPENGL=ON`, чтобы работали кастомные GLSL-шейдеры (`Cinema4K.glsl`, `SSAO4K.glsl`).

### MSYS2 / MinGW64

```bash
./run.sh --install-deps
```

---

## 4. Универсальная сборка через CMake (Linux / NixOS / macOS / Windows)

Сначала установите Urho3D SDK или соберите локальный через `./build.sh` (в Windows — `build.ps1`). Затем укажите путь к SDK:

```bash
cmake -S . -B build-desktop -DBUILD_GAME=ON -DAUTO_FETCH_URHO3D=OFF -DURHO3D_HOME="$PWD/.cache/urho3d-sdk"
cmake --build build-desktop --config Release -j
```

В PowerShell используйте `-DURHO3D_HOME="$PWD/.cache/urho3d-sdk"` или путь к вашему SDK. При переходе на другой SDK используйте новый каталог сборки: CMake кэширует найденные пути к заголовкам и библиотеке. Urho3D 1.9.0 ожидает отдельную сборку из собственного корня; существующий путь `AUTO_FETCH_URHO3D=ON` через `FetchContent` может не сконфигурироваться (в частности, с ошибкой `UrhoCommon`). Локальный SDK, скрипты и Flake собирают Urho3D отдельно.

Только headless-тесты (без OpenGL/SDL2/Urho3D):

```bash
cmake -S . -B build-tests -DBUILD_GAME=OFF
cmake --build build-tests --config Release -j
ctest --test-dir build-tests -C Release --output-on-failure
```

---

## Что проверять вручную в клиенте

1. **Интро**: `koch.studios` → `powered by fall.os` → `CHICKENCUM 2.1` под `KochSplashTheme.wav` (около 15,5 с; скип: клик / `Space` / `Enter` / `Esc`). Проверь плавное свечение и ресайз окна на каждой стадии.
2. **Меню**: `W/S` выбирают пункт, `Enter` запускает, `Esc` выходит.
3. **Полигон и Арена 2.5 «ПЕРО СУДЬБЫ»**:
   - Неоновый ритуальный круг с взлётными полосами в центре, 4 hazard-ежа, 2 северные/южные арки, вентили на трубах.
   - Оружие 1–9 / 0 / `[` / `]`, цвет перекрестия под цвет ствола, полоса прогресса перезарядки.
   - Живое поведение кур (weaving при сближении, выпад клювом, падение и исчезновение трупов через 3.5 сек) + счётчик оставшихся врагов текущей волны в HUD.
4. **Кочет в баньке `[P]`**: сруб на 3D-арене и в меню, градусник пара и снег за окном, действия `1..5`, загадки `Q/W/E/R`, статистика `6`, 14 концовок и боевые баффы Арены.
5. **Казино `[J]`**: переключение `[TAB]` между **Золотой рулеткой** (с историей последних 10 спинов) и **Кибер-Слотами 777** (выплаты до `×50`).
6. **Pixel Arcade `[K]`**: все 6 игр — `[1] Змейка`, `[2] Арканоид`, `[3] Куро-Ран`, `[4] Pong`, `[5] Flappy Кочет`, `[6] Кибер-Тетрис` (начисление кредитов `¤` за результаты).
7. **Горячие клавиши**: `F1`–`F3`, `F8`, `F9`, `F11`, `F12`, ресайз окна и `settings.ini` в пользовательской директории Urho3D (`GetAppPreferencesDir`).
