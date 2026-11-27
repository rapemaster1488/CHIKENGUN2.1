#pragma once

// MonitorLayout — арифметика многомониторной раскладки.
//
// Зачем отдельный заголовок. «Игра растянулась на два монитора» — это не баг
// рендера, а баг геометрии окна: окно получило ширину, равную объединённой
// рабочей области (или размер второго монитора), и SDL честно создал его
// поперёк обоих экранов. Причины всегда одни и те же:
//
//   1. Разрешение берётся из таблицы (3840x2160), а не из конкретного монитора.
//      На ноутбуке 1920x1080 + внешний 2560x1440 окно 3840 шириной накрывает
//      оба экрана.
//   2. Разрешение «по размеру рабочего стола» считается по объединённому
//      bbox всех дисплеев (SDL_GetDisplayBounds для виртуального рабочего стола).
//   3. Окно создаётся в координатах (0,0), которые на multimonitor-сетапе
//      принадлежат первому монитору, а не выбранному.
//   4. Позиция окна не пересчитывается, когда пользователь меняет монитор:
//      окно остаётся за краем выбранного экрана или накрывает оба.
//
// Вся арифметика здесь чистая и покрыта тестами (tests/test_monitor_layout.cpp),
// потому что проверить её на реальном двухмониторном стенде в CI невозможно.

namespace MonitorLayout {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

struct Monitor {
    Rect bounds;      // положение и размер монитора в виртуальных координатах
    bool primary = false;
};

// Есть ли вообще второй экран. Всё остальное поведение должно быть
// ровно таким же, как раньше, если монитор один.
inline bool IsMultiMonitor(int count) { return count > 1; }

inline int ClampMonitor(int index, int count) {
    if (count <= 0) return 0;
    if (index < 0) return 0;
    if (index >= count) return count - 1;
    return index;
}

// Пересечение двух прямоугольников. Пустой прямоугольник при отсутствии пересечения.
inline bool Intersect(const Rect& a, const Rect& b, Rect& out) {
    const int x0 = a.x > b.x ? a.x : b.x;
    const int y0 = a.y > b.y ? a.y : b.y;
    const int x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    const int y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    if (x1 <= x0 || y1 <= y0) { out = Rect(); return false; }
    out = Rect{x0, y0, x1 - x0, y1 - y0};
    return true;
}

// Площадь пересечения. Нужна, чтобы понять, на каком мониторе окно реально
// находится: «свой» монитор — тот, с которым пересечение больше.
inline long long OverlapArea(const Rect& a, const Rect& b) {
    Rect r;
    if (!Intersect(a, b, r)) return 0;
    return (long long)r.w * (long long)r.h;
}

// На каком мониторе окно (по максимальному пересечению). -1 — окно целиком
// вне всех мониторов (например, его утащили за край).
inline int MonitorForRect(const Rect* monitors, int count, const Rect& window) {
    int best = -1;
    long long bestArea = 0;
    for (int i = 0; i < count; ++i) {
        const long long area = OverlapArea(monitors[i], window);
        if (area > bestArea) { bestArea = area; best = i; }
    }
    return best;
}

// Центрирование окна на выбранном мониторе.
inline Rect CenterOnMonitor(const Rect& monitor, int w, int h) {
    Rect out;
    out.w = w;
    out.h = h;
    out.x = monitor.x + (monitor.w - w) / 2;
    out.y = monitor.y + (monitor.h - h) / 2;
    // Прижимаем к монитору, если окно шире/выше него: иначе оно снова
    // вылезет на соседний экран, с чего и началась проблема.
    if (out.x < monitor.x) out.x = monitor.x;
    if (out.y < monitor.y) out.y = monitor.y;
    if (w > monitor.w) { out.x = monitor.x; out.w = monitor.w; }
    if (h > monitor.h) { out.y = monitor.y; out.h = monitor.h; }
    return out;
}

// Вписывание запрошенного размера в конкретный монитор.
// Возвращаем ИМЕННО размер этого монитора как потолок, а не bbox всех
// дисплеев: это и есть лекарство от окна на два экрана.
inline Rect FitToMonitor(const Rect& monitor, int wantW, int wantH) {
    Rect out = monitor;
    if (wantW > 0 && wantW < monitor.w) out.w = wantW;
    if (wantH > 0 && wantH < monitor.h) out.h = wantH;
    if (out.w <= 0) out.w = monitor.w;
    if (out.h <= 0) out.h = monitor.h;
    return out;
}

// Запрос пользователя → размер, который реально помещается на этом мониторе.
// requestW/H == 0 означает «во весь экран» (нативное разрешение монитора).
inline void ResolveWindowSize(const Rect& monitor, int requestW, int requestH,
                              int& outW, int& outH) {
    if (requestW <= 0 || requestH <= 0) {
        outW = monitor.w;
        outH = monitor.h;
        return;
    }
    outW = requestW < monitor.w ? requestW : monitor.w;
    outH = requestH < monitor.h ? requestH : monitor.h;
}

// Разрешение из таблицы пресетов, но не больше физического размера монитора.
// Возвращает true, если пришлось урезать (полезно для строки диагностики).
inline bool ClampResolutionToMonitor(int& w, int& h, const Rect& monitor) {
    bool clamped = false;
    if (w > monitor.w) { w = monitor.w; clamped = true; }
    if (h > monitor.h) { h = monitor.h; clamped = true; }
    return clamped;
}

// Следующий монитор по кругу (для пункта настроек «Монитор»).
inline int CycleMonitor(int index, int count, int direction) {
    if (count <= 1) return 0;
    int next = (ClampMonitor(index, count) + (direction >= 0 ? 1 : -1)) % count;
    if (next < 0) next += count;
    return next;
}

// Окно ушло за пределы всех мониторов (например, отключили второй экран и
// координаты остались от него). Такое окно надо вернуть на основной монитор,
// иначе игрок видит чёрный экран и «клава не работает» — окно не в фокусе.
inline bool IsOffScreen(const Rect* monitors, int count, const Rect& window) {
    if (count <= 0) return false;
    return MonitorForRect(monitors, count, window) < 0;
}

// Санity-проверка геометрии окна: нули и отрицательные размеры ломают
// SDL_CreateWindow так же, как «растянутость» на два экрана.
inline bool IsValidSize(int w, int h) { return w > 0 && h > 0; }

// ── «Склеенный» рабочий стол ────────────────────────────────────────────────
// ЖАЛОБА: «это говно видит два моих моника как 1, 3480 пикселей».
// 3480 = 1920 + 1560 — это ШИРИНА ОБЪЕДИНЁННОГО рабочего стола, а не
// отдельного экрана. Значит SDL_GetNumVideoDisplays() вернул 1 и отдал
// десктопный режим объединённого пространства. Причина почти всегда не в
// игре, а в драйвере/режиме: NVIDIA Surround, AMD Eyefinity, Intel Collage —
// или X11-экран, объявленный как один Display шириной в оба монитора.
//
// Починить это изнутри игры нельзя (SDL просто не знает о втором дисплее),
// но можно однозначно ОПОЗНАТЬ и сказать игроку, куда смотреть, вместо того
// чтобы молча растягивать окно. Проверка нужна на старте и в телеметрии.

// Похоже ли разрешение на сумму нескольких экранов: ширины хватает на два
// типовых монитора по горизонтали, при этом пропорции «широкого» экрана.
// Порог 2.2 отводит 32:9 и 21:9 (до 2.4 бывают панели 5120x2160 = 2.37).
inline bool LooksLikeStitchedDesktop(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    const double aspect = (double)w / (double)h;
    if (aspect < 2.6) return false;          // 21:9 и уже — один монитор
    // Второй признак: высота совпадает с типовой высотой монитора, а ширина
    // примерно равна сумме двух таких экранов.
    static const int kTypicalHeights[] = { 1080, 1200, 1440, 1600, 2160, 1440 };
    for (int i = 0; i < 6; ++i) {
        const int hh = kTypicalHeights[i];
        if (h != hh) continue;
        const int single = (hh * 16) / 9;    // 1920 для 1080
        if (w >= single + single / 2) return true;
    }
    return false;
}

// Итог диагностики для строки телеметрии и сообщения игроку.
struct DesktopDiag {
    bool stitched = false;   // режим «один экран шириной в два»
    int  monitors = 1;
};

inline DesktopDiag Diagnose(int monitorCount, int deskW, int deskH) {
    DesktopDiag d;
    d.monitors = monitorCount > 0 ? monitorCount : 1;
    // Один «монитор» гигантской ширины — ровно та ситуация с 3480 px.
    d.stitched = (d.monitors == 1) && LooksLikeStitchedDesktop(deskW, deskH);
    return d;
}

// Текст подсказки. Короткий: его читают в телеметрии поверх боя.
inline const char* StitchedHint() {
    return "Экран объединён драйвером (Surround/Eyefinity/Mosaic): "
           "отключи объединение в панели видеодрайвера, чтобы F10 увидел второй монитор";
}

// ── Половины «склеенного» экрана ────────────────────────────────────────────
// Когда драйвер объединил два монитора в один дисплей (Surround / Eyefinity /
// Collage), SDL показывает ОДИН экран шириной 3480 или 3840. Половины этого
// пространства физически и есть отдельные мониторы — значит окно можно
// поставить на левую или правую половину и получить нормальную картинку на
// одном экране без разрыва посередине.
//
// Это обход, а не лечение: полноценно разлепить экраны может только драйвер
// (и тогда F10 увидит оба настоящих монитора). Но обход работает сразу и без
// перезагрузки.

// Сколько «экранных» половин у дисплея. 1 — обычный монитор.
inline int HStripeCount(int deskW, int deskH) {
    return LooksLikeStitchedDesktop(deskW, deskH) ? 2 : 1;
}

// Подложка (0 — левая половина, 1 — правая) внутри дисплея.
inline Rect StripeOf(const Rect& display, int stripe, int stripes) {
    if (stripes <= 1) return display;
    if (stripe < 0) stripe = 0;
    if (stripe >= stripes) stripe = stripes - 1;
    Rect out;
    // Делим ровно, остаток отдаём правой половине: так две половины вместе
    // всегда дают исходную ширину, ничего не теряется.
    const int w = display.w / stripes;
    out.x = display.x + stripe * w;
    out.y = display.y;
    out.w = (stripe == stripes - 1) ? (display.w - stripe * w) : w;
    out.h = display.h;
    return out;
}

// Вписать окно в половину: размер не больше половины, позиция по центру,
// с прижатием к её краям — та же логика, что CenterOnMonitor.
inline Rect FitWindowToStripe(const Rect& display, int stripe, int stripes,
                              int wantW, int wantH) {
    const Rect half = StripeOf(display, stripe, stripes);
    int w = wantW, h = wantH;
    ResolveWindowSize(half, wantW, wantH, w, h);
    if (!IsValidSize(w, h)) { w = half.w; h = half.h; }
    return CenterOnMonitor(half, w, h);
}

// Основной монитор: первый, помеченный primary, иначе нулевой.
inline int PrimaryIndex(const Monitor* monitors, int count) {
    for (int i = 0; i < count; ++i) {
        if (monitors[i].primary) return i;
    }
    return count > 0 ? 0 : -1;
}

} // namespace MonitorLayout
