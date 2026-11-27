#pragma once

// WindowMonitor — реальная работа с многомониторным окном через SDL.
//
// Здесь только обёртки над SDL плюс арифметика из MonitorLayout.h. Вся
// политика (что считать «своим» монитором, как урезать пресет 4K до 1080p,
// куда центрировать) живёт в MonitorLayout.h и покрыта тестами — её можно
// проверять без дисплея, чего нельзя сделать с SDL.
//
// Заголовок SDL подключается только на десктопе. На остальных платформах
// функции вырождаются в «ничего не делать», чтобы не тянуть SDL в тесты.

#include "MonitorLayout.h"

#include <cstdlib>
#include <cstring>

#if !defined(__ANDROID__) && !defined(IOS) && !defined(TVOS) && !defined(__EMSCRIPTEN__)
#  define CHIKENGUN_HAS_SDL_WINDOW 1
#  if defined(__has_include) && __has_include(<SDL/SDL.h>)
#    include <SDL/SDL.h>
#  elif defined(__has_include) && __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#  else
#    include <SDL.h>
#  endif
#else
#  define CHIKENGUN_HAS_SDL_WINDOW 0
#endif

namespace WindowMonitor {

using MonitorLayout::Monitor;
using MonitorLayout::Rect;

struct Display {
    int count = 1;
    int current = 0;          // на каком мониторе окно сейчас (по площади пересечения)
    Rect desktop;             // размер монитора, на котором окно
    Rect bounds;              // положение монитора в виртуальных координатах
};

#if CHIKENGUN_HAS_SDL_WINDOW

inline bool VideoReady() { return SDL_WasInit(SDL_INIT_VIDEO) != 0; }

inline SDL_Window* MainWindow() { return SDL_GL_GetCurrentWindow(); }

inline Rect DisplayBounds(int index) {
    Rect r;
    SDL_Rect s;
    if (SDL_GetDisplayBounds(index, &s) == 0) {
        r = Rect{s.x, s.y, s.w, s.h};
    }
    return r;
}

// Опрос текущей раскладки. Возвращает false, если видео ещё не поднято —
// вызывающая сторона тогда просто не трогает окно.
inline bool Query(Display& out) {
    if (!VideoReady()) return false;
    out.count = SDL_GetNumVideoDisplays();
    if (out.count <= 0) { out.count = 1; out.current = 0; return false; }

    SDL_Window* win = MainWindow();
    out.current = win ? SDL_GetWindowDisplayIndex(win) : 0;
    if (out.current < 0) out.current = 0;

    out.bounds = DisplayBounds(out.current);
    if (out.bounds.w <= 0 || out.bounds.h <= 0) {
        // Fallback: рабочий стол конкретного монитора, а НЕ объединённый bbox
        // всех дисплеев. Именно bbox и давал окно на два экрана.
        SDL_DisplayMode mode;
        if (SDL_GetDesktopDisplayMode(out.current, &mode) == 0) {
            out.bounds.w = mode.w;
            out.bounds.h = mode.h;
        }
    }
    out.desktop = out.bounds;
    if (out.desktop.w <= 0) out.desktop.w = 1920;
    if (out.desktop.h <= 0) out.desktop.h = 1080;
    return true;
}

// Рабочая область — без панелей задач и доков. Для оконного режима берём
// именно её: окно, равное полному экрану, на мультимониторе часто съезжает
// под панель и «прилипает» к соседнему экрану.
inline Rect UsableBounds(int index) {
    Rect r = DisplayBounds(index);
#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_Rect u;
    if (SDL_GetDisplayUsableBounds(index, &u) == 0 && u.w > 0 && u.h > 0) {
        r = Rect{u.x, u.y, u.w, u.h};
    }
#else
    // В старых SDL рабочая область недоступна — считаем ею весь монитор.
    (void)index;
#endif
    return r;
}

// Перенос окна на монитор index: размер по размеру ЭТОГО монитора, позиция —
// по центру. Это и есть лечение «растянулось на два моника».
inline void MoveToMonitor(int index, int wantW, int wantH) {
    SDL_Window* win = MainWindow();
    if (!win) return;
    // Захват мыши отпускаем на время переноса: иначе WM может проигнорировать
    // смену позиции, пока курсор grab'нут. Возврат делает вызывающая сторона.
    SDL_SetWindowGrab(win, SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    const int count = SDL_GetNumVideoDisplays();
    if (count <= 0) return;
    index = MonitorLayout::ClampMonitor(index, count);

    const Rect target = UsableBounds(index);
    if (target.w <= 0 || target.h <= 0) return;

    int w = wantW, h = wantH;
    MonitorLayout::ResolveWindowSize(target, wantW, wantH, w, h);
    if (!MonitorLayout::IsValidSize(w, h)) return;

    const Rect placed = MonitorLayout::CenterOnMonitor(target, w, h);
    SDL_SetWindowPosition(win, placed.x, placed.y);
    if (SDL_GetWindowFlags(win) & SDL_WINDOW_RESIZABLE)
        SDL_SetWindowSize(win, placed.w, placed.h);

    // Если окно оказалось за пределами выбранного монитора (бывает после
    // смены раскладки), позиционируем жёстко ещё раз.
    int wx = 0, wy = 0;
    SDL_GetWindowPosition(win, &wx, &wy);
    if (wx < target.x || wy < target.y) SDL_SetWindowPosition(win, placed.x, placed.y);
}

// Поставить окно на половину «склеенного» дисплея (Surround/Eyefinity).
// stripe = 0 — левая половина, 1 — правая. Если экран один и не склеен
// (половин нет), функция ничего не делает: обычный монитор не трогаем.
inline bool MoveToStripe(int stripe) {
    SDL_Window* win = MainWindow();
    if (!win) return false;
    const int count = SDL_GetNumVideoDisplays();
    if (count <= 0) return false;
    const int disp = SDL_GetWindowDisplayIndex(win);
    const int display = disp >= 0 ? disp : 0;

    Rect frame = DisplayBounds(display);
    if (frame.w <= 0 || frame.h <= 0) {
        SDL_DisplayMode mode;
        if (SDL_GetDesktopDisplayMode(display, &mode) != 0) return false;
        frame = Rect{0, 0, mode.w, mode.h};
    }
    const int stripes = MonitorLayout::HStripeCount(frame.w, frame.h);
    if (stripes <= 1) return false;   // не склеенный экран — делить нечего

    SDL_SetWindowGrab(win, SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);

    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);
    const MonitorLayout::Rect placed =
        MonitorLayout::FitWindowToStripe(frame, stripe, stripes, w, h);
    if (!MonitorLayout::IsValidSize(placed.w, placed.h)) return false;

    // Сначала снимаем «на весь экран», иначе WM проигнорирует размер.
    SDL_SetWindowSize(win, placed.w, placed.h);
    SDL_SetWindowPosition(win, placed.x, placed.y);
    return true;
}

// Возврат «потерянного» окна: если оно целиком вне всех мониторов (например,
// второй экран отключили, а координаты остались), ставим на основной.
inline bool RecoverIfOffScreen() {
    Display d;
    if (!Query(d)) return false;
    SDL_Window* win = MainWindow();
    if (!win) return false;
    int wx = 0, wy = 0, ww = 0, wh = 0;
    SDL_GetWindowPosition(win, &wx, &wy);
    SDL_GetWindowSize(win, &ww, &wh);
    if (!MonitorLayout::IsValidSize(ww, wh)) { MoveToMonitor(d.current, 1600, 900); return true; }

    Monitor list[8];
    const int n = d.count < 8 ? d.count : 8;
    for (int i = 0; i < n; ++i) list[i].bounds = DisplayBounds(i);
    const Rect window{wx, wy, ww, wh};
    if (MonitorLayout::IsOffScreen(reinterpret_cast<const Rect*>(list), n, window)) {
        MoveToMonitor(d.current, 1600, 900);
        return true;
    }
    return false;
}

// Включать ли захват мыши. Главная ловушка: пока окно НЕ в фокусе, забирать
// мышь нельзя — SDL на некоторых WM отдаёт захват и не возвращает его, после
// чего и «клава не работает», и курсор не кликает по меню.
// Разрешение на захват мыши.
// ВАЖНО: проверяем только явные «окно не на экране» — свёрнуто/скрыто.
// Флаг SDL_WINDOW_INPUT_FOCUS на части WM (особенно Wayland и alguns X11)
// не выставляется или выставляется с задержкой, хотя клавиши и мышь в окно
// приходят. Если запрещать захват по нему, обзор мышью ломается сразу после
// старта — ровно то, на что жалуется игрок. Поэтому здесь fail-open:
// сомневаешься — разрешай, а не отбирай управление.
inline bool CanGrabMouse() {
    SDL_Window* win = MainWindow();
    if (!win) return false;
    const Uint32 flags = SDL_GetWindowFlags(win);
    if (flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) return false;
    return true;
}

// Точный признак «окно точно не на экране». Используется там, где ошибка
// в сторону «не трогать» дешевле, чем ошибка в сторону «вмешаться».
inline bool DefinitelyUnusable() {
    SDL_Window* win = MainWindow();
    if (!win) return true;
    return (SDL_GetWindowFlags(win) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
}

inline bool HasFocus() {
    SDL_Window* win = MainWindow();
    if (!win) return false;
    return (SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

// ── Возврат фокуса окну ───────────────────────────────────────────────────
// На Linux без этого «клава не работает» после alt-tab: окно видно, игра
// идёт, но SDL не получает события клавиш, потому что WM считает фокус
// потерянным. Просто поднять окно недостаточно — нужен ещё и забор ввода.
//
// SDL_SetWindowInputFocus есть только с 2.0.5, поэтому версия проверяется
// на этапе компиляции: сборочный Urho3D тащит именно свою SDL 2.0.10, но
// системная может оказаться старше.
inline void RequestInputFocus() {
    SDL_Window* win = MainWindow();
    if (!win) return;
#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_SetWindowInputFocus(win);
#endif
}

inline bool EnsureFocus() {
    SDL_Window* win = MainWindow();
    if (!win) return false;
    if (HasFocus()) return true;

    const Uint32 flags = SDL_GetWindowFlags(win);
    // Свёрнутое окно сначала разворачиваем: иначе WM может восстановить его
    // на объединённой области двух экранов — ровно та «растянутость».
    if (flags & SDL_WINDOW_MINIMIZED) SDL_RestoreWindow(win);
    if (flags & SDL_WINDOW_HIDDEN) SDL_ShowWindow(win);

    SDL_RaiseWindow(win);
    RequestInputFocus();
    return HasFocus();
}

#else // не десктоп

inline bool Query(Display&) { return false; }
inline Rect DisplayBounds(int) { return Rect(); }
inline Rect UsableBounds(int) { return Rect(); }
inline void MoveToMonitor(int, int, int) {}
inline bool MoveToStripe(int) { return false; }
inline bool RecoverIfOffScreen() { return false; }
inline bool CanGrabMouse() { return true; }
inline bool HasFocus() { return true; }

#endif

} // namespace WindowMonitor
