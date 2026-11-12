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

### Полезные флаги (`./build.sh` и `./run.sh`)

- `./run.sh --4k` / `./build.sh --4k` — генерация полного 4096×4096 набора PBR-текстур + 3840×2160 `LoadingBg4K.png` (по умолчанию `TEX_SIZE=1024`).
- `./build.sh --install-deps` — автоустановка системных пакетов сборки.
- `./build.sh --tests-only` — сборка и прогон модульных C++17 тестов (`test_core`) без Urho3D.
- `URHO3D_HOME=/путь/к/sdk ./build.sh` — использовать уже установленный Urho3D SDK.
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

```bash
cmake -S . -B build-desktop -DBUILD_GAME=ON -DAUTO_FETCH_URHO3D=ON
cmake --build build-desktop --config Release -j
```

Только headless-тесты (без OpenGL/SDL2/Urho3D):

```bash
cmake -S . -B build-tests -DBUILD_GAME=OFF
cmake --build build-tests --config Release -j
ctest --test-dir build-tests -C Release --output-on-failure
```

---

## Что проверять вручную в клиенте

1. **Интро 2.5.6**: `koch.studios` → `powered by fall.os` → `CHICHKENCUM 2.1` под `KochSplashTheme.wav` (скип: клик / `Space` / `Enter` / `Esc`).
2. **Меню**: `W/S` выбирают пункт, `Enter` запускает, `Esc` выходит.
3. **Полигон и Арена 2.5 «ПЕРО СУДЬБЫ»**:
   - Неоновый ритуальный круг с взлётными полосами в центре, 4 hazard-ежа, 2 северные/южные арки, вентили на трубах.
   - Оружие 1–9 / 0 / `[` / `]`, цвет перекрестия под цвет ствола, полоса прогресса перезарядки.
   - Живое поведение кур (weaving при сближении, выпад клювом, падение и исчезновение трупов через 3.5 сек) + счётчик оставшихся врагов текущей волны в HUD.
4. **Кочет в баньке `[P]`**: сруб на 3D-арене и в меню, градусник пара и снег за окном, действия `1..5`, загадки `Q/W/E/R`, статистика `6`, 14 концовок и боевые баффы Арены.
5. **Казино `[J]`**: переключение `[TAB]` между **Золотой рулеткой** (с историей последних 10 спинов) и **Кибер-Слотами 777** (выплаты до `×50`).
6. **Pixel Arcade `[K]`**: все 6 игр — `[1] Змейка`, `[2] Арканоид`, `[3] Куро-Ран`, `[4] Pong`, `[5] Flappy Кочет`, `[6] Кибер-Тетрис` (начисление кредитов `¤` за результаты).
7. **Горячие клавиши**: `F1`–`F3`, `F8`, `F9`, `F11`, `F12`, ресайз окна и `settings.ini` в пользовательской директории Urho3D (`GetAppPreferencesDir`).
