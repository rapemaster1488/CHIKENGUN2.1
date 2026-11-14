# Docker: запуск CHIKENGUN 2.5 где угодно (Linux, Windows, macOS, серверы)

Официальный Docker-образ собирает нативный C++17 / Urho3D клиент и запускает его
**в браузере через noVNC** — без локальной компиляции, драйверов и зависимостей.
Работает на любой ОС с Docker (или Podman): достаточно открыть `http://localhost:6080/`.

---

## 1. Быстрый старт (игра в браузере)

```bash
./docker.sh                 # собрать образ и запустить
# или без обёртки:
docker compose up --build
```

Откройте в браузере: **http://localhost:6080/** — игра подключится автоматически.

- Кликните по игровому окну, чтобы браузер захватил клавиатуру (`WASD`, `1..9`, `F11`-кнопка noVNC для полного экрана).
- Первый запуск собирает Urho3D 1.9.0 SDK и PBR-текстуры — это занимает несколько минут. Повторные пересборки
  кэшируют SDK между сборками образа (BuildKit cache mount), поэтому правки `src/*` пересобирают только клиент.
- Остановить: `Ctrl+C` (или `./docker.sh down`).

Требования: **Docker 20.10+ с Compose v2** (BuildKit включён по умолчанию) либо Podman с `podman compose`.

---

## 2. Режимы запуска

| Режим | Команда | Что происходит |
|---|---|---|
| `web` (по умолчанию) | `./docker.sh` | Xvfb + x11vnc + noVNC: игра в браузере на порту 6080, работает везде |
| `x11` | `./docker.sh x11` | Нативное окно через X11-сокет хоста (Linux / XWayland) |
| `offscreen` | `CHIKENGUN_MODE=offscreen docker compose run --rm chikengun --diagnose-sdl` | Xvfb без VNC: диагностика SDL, скриншоты, CI |

### Нативный X11-режим (Linux)

```bash
xhost +local:                     # разрешить локальный доступ к X-серверу
./docker.sh x11                   # то же самое: docker compose --profile x11 run --rm chikengun-x11
```

Звук в X11-режиме: раскомментируйте в `docker-compose.yml` (сервис `chikengun-x11`)
проброс `/dev/snd` (ALSA) или PulseAudio-сокета. В веб-режиме звука нет (noVNC не
переносит аудио) — игра работает без него, как и на системах без звукового устройства.

---

## 3. Настройки

### Переменные окружения (можно вынести в `.env`)

| Переменная | По умолчанию | Назначение |
|---|---|---|
| `RESOLUTION` | `1280x720x24` | Геометрия виртуального дисплея Xvfb |
| `NOVNC_PORT` | `6080` | Порт веб-доступа на хосте |
| `VNC_PASSWORD` | пусто | VNC-пароль (**обязателен на публичных серверах**) |
| `TEX_SIZE` | `1024` | Разрешение процедурных PBR-текстур при сборке (1024 или 4096) |
| `LIBGL_ALWAYS_SOFTWARE` | `1` | `1` = llvmpipe (программный OpenGL), `0` = GPU через `/dev/dri` |
| `SHM_SIZE` | `512mb` | Размер `/dev/shm` (llvmpipe/Xvfb требуют больше дефолтных 64mb) |

### Аргументы игры

Всё, что идёт после `--`, передаётся `MyShooter`:

```bash
docker compose run --rm chikengun -nosound          # запуск без звука
docker compose --profile tools run --rm chikengun-diagnose   # SDL-диагностика
```

### Сохранения

`settings.ini` (кредиты казино `¤`, графика, достижения) хранится в именованном томе
`chikengun-save` и переживает пересоздание контейнера. Сброс: `./docker.sh down`
(удалит и тома) или `docker volume rm chikengun-save`.

---

## 4. 4K UHD-текстуры

```bash
./docker.sh build --4k      # TEX_SIZE=4096 — полный 4K PBR-набор + Skybox4K IBL
./docker.sh                 # запуск собранных 4K-текстур
```

Генерация 4096×4096 текстур требует ~4 ГБ RAM и заметно больше времени сборки.

---

## 5. Публичный сервер (игра из любого места мира)

```bash
VNC_PASSWORD='секрет' NOVNC_PORT=6080 docker compose up --build -d
# затем: https://ваш-сервер:6080/  (рекомендуется поверх HTTPS-реверс-прокси)
```

**Всегда задавайте `VNC_PASSWORD` на серверах, доступных из интернета**, и закрывайте
порт 5900 (прямой VNC) фаерволом — наружу нужен только 6080.

---

## 6. Готовый образ из GitHub Container Registry

При пуше в `main` и тегах `v*` CI публикует образ в GHCR
(`.github/workflows/docker.yml`):

```bash
docker pull ghcr.io/rapemaster1488/chikengun2.1:latest
docker run --rm -p 6080:6080 --shm-size=512m -v chikengun-save:/data \
    ghcr.io/rapemaster1488/chikengun2.1:latest
# браузер: http://localhost:6080/
```

---

## 7. GPU-ускорение (необязательно)

По умолчанию рендерит Mesa llvmpipe (программно) — работает везде. Для аппаратного
OpenGL пробросьте устройства видеокарты и выключите программный режим:

```bash
# Intel / AMD:
docker compose run --rm \
    --device /dev/dri:/dev/dri \
    -e LIBGL_ALWAYS_SOFTWARE=0 chikengun

# NVIDIA (нужен NVIDIA Container Toolkit):
docker compose run --rm --gpus all -e LIBGL_ALWAYS_SOFTWARE=0 chikengun
```

То же самое можно добавить в `docker-compose.yml` (секции `devices` / `deploy`).

---

## 8. Podman

Образ и compose-файл совместимы с Podman:

```bash
podman compose up --build     # ./docker.sh автоматически найдёт podman compose
```

---

## 9. Troubleshooting

| Симптом | Решение |
|---|---|
| `docker: command not found` | Установите Docker Engine + Compose v2 или Podman (см. §1) |
| Сборка падает на `git clone` Urho3D | Нужен доступ в сеть к `github.com` при первой сборке образа |
| Ошибка `RUN --mount=type=cache` | Включите BuildKit: `DOCKER_BUILDKIT=1 docker compose build` |
| Чёрный экран / артефакты в браузере | Увеличьте `SHM_SIZE=1gb` и пересоздайте контейнер |
| Клавиатура «не слышится» | Кликните по игровому окну (захват ввода); из полного экрана — `Esc` |
| Нет звука в веб-режиме | Ограничение noVNC: используйте `./docker.sh x11` со звуком (см. §2) |
| `x11`-режим: `Authorization required` | На хосте выполните `xhost +local:` (и проверьте `DISPLAY`) |
| Права на `/data` при bind-mount | `chown -R 1000:1000` на каталоге хоста, смонтированном в `/data` |
| Хотите проверить образ без игры | `docker compose --profile tools run --rm chikengun-diagnose` |

Статические проверки Docker-упаковки (без демона Docker) входят в тесты проекта:

```bash
./tests/test_docker_files.sh
```
