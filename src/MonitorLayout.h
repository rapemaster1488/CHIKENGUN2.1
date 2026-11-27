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

// Основной монитор: первый, помеченный primary, иначе нулевой.
inline int PrimaryIndex(const Monitor* monitors, int count) {
    for (int i = 0; i < count; ++i) {
        if (monitors[i].primary) return i;
    }
    return count > 0 ? 0 : -1;
}

} // namespace MonitorLayout
