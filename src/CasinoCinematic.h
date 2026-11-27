#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// CasinoCinematic — кинематика крупного плана рулетки.
//
// Игрок должен УВИДЕТЬ, как шарик крутится и куда падает, а не читать цифру
// в мелкой плашке. Спин проходит через явные фазы:
//
//   OFF → FLY_IN   (камера подлетает к колесу и опускается сверху)
//       → TRACK    (камера едет по колесу вместе с сектором, шарик крутится)
//       → REVEAL   (шарик в лунке, крупное число выпало, выдержка)
//       → FLY_BACK (камера возвращается в полный вид зала — «потом полно»)
//       → DONE
//
// Модуль намеренно без Urho3D: только математика камеры и тайминги, чтобы
// всё это проверялось обычными unit-тестами (tests/test_casino_cine.cpp).
// ─────────────────────────────────────────────────────────────────────────────

#include <algorithm>
#include <cmath>

namespace CasinoCine {

struct V3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline V3 MakeV3(float x, float y, float z) { V3 v; v.x = x; v.y = y; v.z = z; return v; }
inline V3 Add(const V3& a, const V3& b) { return MakeV3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline V3 Sub(const V3& a, const V3& b) { return MakeV3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline V3 Scale(const V3& a, float s) { return MakeV3(a.x * s, a.y * s, a.z * s); }
inline float Length(const V3& a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float Smooth(float t) { t = Clamp01(t); return t * t * (3.0f - 2.0f * t); }
inline float EaseOutCubic(float t) { t = Clamp01(t); const float u = 1.0f - t; return 1.0f - u * u * u; }
inline float EaseInCubic(float t) { t = Clamp01(t); return t * t * t; }

inline V3 Lerp(const V3& a, const V3& b, float t) {
    t = Clamp01(t);
    return MakeV3(a.x + (b.x - a.x) * t,
                  a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t);
}

// ── Фазы ────────────────────────────────────────────────────────────────────
enum Phase {
    PH_OFF = 0,
    PH_FLY_IN,
    PH_TRACK,
    PH_REVEAL,
    PH_FLY_BACK,
    PH_DONE,
    PH_COUNT
};

inline const char* PhaseName(int p) {
    switch (p) {
        case PH_OFF:      return "OFF";
        case PH_FLY_IN:   return "FLY_IN";
        case PH_TRACK:    return "TRACK";
        case PH_REVEAL:   return "REVEAL";
        case PH_FLY_BACK: return "FLY_BACK";
        case PH_DONE:     return "DONE";
        default:          return "?";
    }
}

// ── Тайминги ────────────────────────────────────────────────────────────────
// Подлёт короче, чем хочется глазу: если он длиннее полусекунды, между
// нажатием «Играть» и первым кадром колеса появляется провал ожидания.
struct Timing {
    static constexpr float kFlyIn      = 0.55f;   // подлёт к колесу
    static constexpr float kRevealHold = 2.10f;   // выдержка крупным планом
    static constexpr float kFlyBack    = 0.80f;   // возврат в полный вид
    // Доля спина, после которой шарик считается лежащим в лунке.
    static constexpr float kBallDropAt = 0.78f;
    // Насколько близко камера подходит к сектору (доля радиуса колеса).
    static constexpr float kCloseRadius = 0.42f;
    // Высота камеры над колесом в крупном плане, в метрах.
    static constexpr float kCloseHeight = 0.62f;
};

// ── Кадр кинематики ─────────────────────────────────────────────────────────
struct Frame {
    int   phase = PH_OFF;
    float closeUp = 0.0f;    // 0 = обычная камера зала, 1 = крупный план колеса
    float reveal = 0.0f;     // 0..1 — насколько показана плашка с числом
    float orbit = 0.0f;      // добавочный доворот камеры вокруг колеса, радианы
    bool  ballInPocket = false;
    bool  finished = false;  // кинематика закончилась, камера снова в зале
};

// Полная длительность кинематики при известном времени спина.
inline float TotalDuration(float spinDuration) {
    return std::max(0.05f, spinDuration) + Timing::kRevealHold + Timing::kFlyBack;
}

// Момент, с которого начинается возврат камеры. Служит и точкой «скипа»:
// игроку не нужно смотреть выдержку, если он уже понял, что выпало.
inline float FlyBackStart(float spinDuration) {
    return std::max(0.05f, spinDuration) + Timing::kRevealHold;
}

// Основная функция: время с начала кинематики → состояние кадра.
inline Frame Eval(float t, float spinDuration) {
    Frame f;
    const float spin = std::max(0.05f, spinDuration);
    const float flyIn = std::min(Timing::kFlyIn, spin);
    const float revealStart = spin;
    const float flyBackStart = FlyBackStart(spinDuration);
    const float end = flyBackStart + Timing::kFlyBack;

    if (t < 0.0f) t = 0.0f;

    if (t < flyIn) {
        f.phase = PH_FLY_IN;
        const float u = flyIn > 0.0f ? t / flyIn : 1.0f;
        f.closeUp = Smooth(u);
        // На подлёте камера чуть доворачивается — оживляет статичный кадр.
        f.orbit = (1.0f - f.closeUp) * -0.30f;
    } else if (t < revealStart) {
        f.phase = PH_TRACK;
        f.closeUp = 1.0f;
    } else if (t < flyBackStart) {
        f.phase = PH_REVEAL;
        f.closeUp = 1.0f;
        const float u = (t - revealStart) / std::max(1e-4f, Timing::kRevealHold);
        // Плашка обязана появиться за ~0.3 с: игрок уже ждал спин и не должен
        // ждать ещё и анимацию цифры. Множитель 3.2 даёт полную видимость
        // к 30% выдержки, остальное время номер просто читается.
        f.reveal = Smooth(std::min(1.0f, u * 3.2f));
    } else if (t < end) {
        f.phase = PH_FLY_BACK;
        const float u = (t - flyBackStart) / std::max(1e-4f, Timing::kFlyBack);
        f.closeUp = 1.0f - Smooth(u);
        f.reveal = 1.0f - Smooth(u);
        f.orbit = Smooth(u) * 0.22f;
    } else {
        f.phase = PH_DONE;
        f.finished = true;
    }

    // Шарик в лунке: по доле спина, но не раньше фактической остановки.
    f.ballInPocket = (t >= spin * Timing::kBallDropAt);
    return f;
}

// ── Наводка на сектор ───────────────────────────────────────────────────────
// Колесо за время подлёта уезжает на десятки градусов, а «переход через 360»
// при наивной интерполяции развернул бы камеру назад. Доводим угол по
// кратчайшей дуге с ограниченной скоростью.
inline float WrapDeg360(float deg) {
    deg = std::fmod(deg, 360.0f);
    if (deg < 0.0f) deg += 360.0f;
    return deg;
}

inline float ShortestDeltaDeg(float from, float to) {
    float d = WrapDeg360(to) - WrapDeg360(from);
    if (d > 180.0f) d -= 360.0f;
    if (d < -180.0f) d += 360.0f;
    return d;
}

// rate — доля оставшегося пути за кадр (0..1). Возвращает новый угол 0..360.
inline float ApproachAngleDeg(float current, float target, float rate) {
    const float k = Clamp01(rate);
    return WrapDeg360(current + ShortestDeltaDeg(current, target) * k);
}

// ── Камера ──────────────────────────────────────────────────────────────────
struct Camera {
    V3 pos;
    V3 look;
};

// Точка сектора на колесе в мировых координатах.
inline V3 SectorPoint(const V3& wheelCenter, float angleRad, float radius) {
    return MakeV3(wheelCenter.x + std::sin(angleRad) * radius,
                  wheelCenter.y,
                  wheelCenter.z + std::cos(angleRad) * radius);
}

// Крупный план: камера над сектором, смотрит вниз-внутрь лунки. Именно такой
// ракурс делает число читаемым — сверху и почти вплотную.
inline Camera CloseUp(const V3& wheelCenter, float sectorAngle, float orbit) {
    const float a = sectorAngle + orbit;
    Camera c;
    const V3 dir = MakeV3(std::sin(a) * Timing::kCloseRadius,
                          Timing::kCloseHeight,
                          std::cos(a) * Timing::kCloseRadius);
    c.pos = Add(wheelCenter, dir);
    // Смотрим не в центр колеса, а на сам сектор: иначе номер уезжает к краю.
    c.look = SectorPoint(wheelCenter, sectorAngle, 0.30f);
    c.look.y = wheelCenter.y + 0.02f;
    return c;
}

// Смешивание обычной камеры зала и крупного плана. Добавка высоты по дуге:
// камера не «проваливается» сквозь стол, а описывает пологую горку.
inline Camera Blend(const Camera& base, const Camera& close, float t) {
    t = Clamp01(t);
    Camera c;
    c.pos = Lerp(base.pos, close.pos, t);
    c.pos.y += std::sin(t * 3.14159265f) * 0.16f;
    c.look = Lerp(base.look, close.look, t);
    return c;
}

// Полный расчёт кадра камеры. base — обычная камера зала, wheelCenter —
// центр колеса в мире, sectorAngle — мировой угол выпавшего сектора.
inline Camera CameraFor(const Camera& base, const V3& wheelCenter,
                        float sectorAngle, const Frame& f) {
    if (f.closeUp <= 0.0f) return base;
    return Blend(base, CloseUp(wheelCenter, sectorAngle, f.orbit), f.closeUp);
}

// ── Плашка с числом ─────────────────────────────────────────────────────────
// Цвет плашки повторяет цвет лунки: 0 — зелёный, красное — красный, иначе
// чёрный. Так игрок узнаёт номер даже не читая цифру.
struct BadgeColor {
    float r, g, b;
};

inline BadgeColor ColorForNumber(int number, bool isRed) {
    if (number == 0) return BadgeColor{ 0.12f, 0.62f, 0.34f };
    if (isRed)       return BadgeColor{ 0.72f, 0.16f, 0.18f };
    return BadgeColor{ 0.14f, 0.16f, 0.20f };
}

// Пульс плашки: короткий «удар» в начале выдержки, потом ровное дыхание.
inline float BadgePulse(float reveal) {
    const float r = Clamp01(reveal);
    return 1.0f + 0.14f * std::sin(r * 12.566f) * (1.0f - r * 0.35f);
}

} // namespace CasinoCine
