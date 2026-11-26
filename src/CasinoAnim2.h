#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// CasinoAnim2 — фазовая модель анимаций казино.
//
// Смысл: игрок должен ВИДЕТЬ, что происходит, без чтения мелкого текста.
// Каждый спин проходит явные фазы, у каждой есть свой интерфейсный акцент:
//
//   Idle → 00 Anticipation (лампы набегают, барабан вздрагивает)
//        → 01 Spinning     (символы смазаны, скролл 24/7)
//        → 02 ReelStop     (по одному: стоп + импульс + пыль)
//        → 03 NearMiss     (два совпали, третья «почти» — вздох зала)
//        → 04 Reveal       (линия выигрыша, вспышка)
//        → 05 CountUp      (цифры ПРИБЫЛИ тикают вверх)
//        → 06 Idle
//
// Модуль намеренно без Urho3D: только математика, чтобы фазы и тайминги
// можно было проверить обычными unit-тестами (tests/test_casino_anim.cpp).
// ─────────────────────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace CasinoAnim2 {

// ── Фазы слота ──────────────────────────────────────────────────────────────
enum SlotPhase {
    PH_IDLE = 0,
    PH_ANTICIPATION,   // лампа бежит по рамке, барабаны дрожат
    PH_SPINNING,       // все барабаны крутятся
    PH_REELS_STOPPING, // барабаны встают по одному
    PH_NEAR_MISS,      // два совпали, третий «почти» — драматическая пауза
    PH_REVEAL,         // линия выигрыша + вспышка
    PH_COUNTUP,        // тикающая сумма
    PH_COUNT
};

inline const char* SlotPhaseName(int p) {
    switch (p) {
        case PH_IDLE:           return "IDLE";
        case PH_ANTICIPATION:   return "ANTICIPATION";
        case PH_SPINNING:       return "SPINNING";
        case PH_REELS_STOPPING: return "REELS_STOPPING";
        case PH_NEAR_MISS:      return "NEAR_MISS";
        case PH_REVEAL:         return "REVEAL";
        case PH_COUNTUP:        return "COUNTUP";
        default:                return "?";
    }
}

// ── Тайминги (секунды) ──────────────────────────────────────────────────────
// Подобраны так, чтобы спин ощущался «дорого», но не заставлял ждать:
// ~2.6 с на обычный, ~3.4 с на near-miss, ~4.2 с на джекпот.
struct Timing {
    static constexpr float kAnticipation = 0.34f;
    static constexpr float kSpinLead      = 0.16f;  // пауза перед первым стопом
    static constexpr float kReelStagger   = 0.26f;  // смещение стопов барабанов
    static constexpr float kReelSettle    = 0.20f;  // отскок после стопа
    static constexpr float kNearMissHold  = 0.38f;
    static constexpr float kReveal        = 0.42f;
    static constexpr float kCountUpWin    = 0.85f;
    static constexpr float kCountUpJackpot= 1.55f;
    static constexpr float kRevealHold   = 0.55f;  // итог висит, чтобы его прочли
};

// Полная длительность спина. nearMiss/jackpot добавляют драматургии.
inline float SpinDuration(int reels, bool nearMiss, bool jackpot) {
    const float base = Timing::kAnticipation + Timing::kSpinLead
                     + Timing::kReelStagger * static_cast<float>(std::max(1, reels) - 1)
                     + Timing::kReelSettle
                     + Timing::kReveal
                     + (jackpot ? Timing::kCountUpJackpot : Timing::kCountUpWin);
    return base + (nearMiss ? Timing::kNearMissHold : 0.0f);
}

// Момент остановки барабана reel (0..2) от начала спина.
inline float ReelStopTime(int reel) {
    return Timing::kAnticipation + Timing::kSpinLead
         + Timing::kReelStagger * static_cast<float>(std::max(0, reel));
}

// ── Кривые ──────────────────────────────────────────────────────────────────
inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// «Тяжёлый старт, мягкая остановка» — скролл барабана.
inline float EaseOutCubic(float t) { t = Clamp01(t); const float u = 1.0f - t; return 1.0f - u * u * u; }
inline float EaseInOutCubic(float t) {
    t = Clamp01(t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}
// Упругое возвращение для «прилипания» барабана.
inline float EaseOutBack(float t) {
    t = Clamp01(t);
    constexpr float c1 = 1.70158f, c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}
// Затухающий отскок после стопа: 1 → 0 с двумя колебаниями.
inline float StopBounce(float sinceStop) {
    if (sinceStop < 0.0f) return 0.0f;
    return std::exp(-sinceStop * 13.0f) * std::sin(sinceStop * 34.0f) * 8.0f;
}

// ── Скорость барабана, об/сек ───────────────────────────────────────────────
// Разгон → крейсер → резкая остановка. Скорость нужна и для motion blur,
// и для звука «тик-тик-тик», который замедляется перед стопом.
inline float ReelSpeed(float sinceStart, float stopAt) {
    if (sinceStart < 0.0f) return 0.0f;
    const float t = sinceStart / std::max(0.05f, stopAt);
    if (t >= 1.0f) return 0.0f;
    if (t < 0.18f) return 9.0f + 11.0f * (t / 0.18f);            // разгон 9→20
    if (t < 0.80f) return 20.0f;                                  // крейсер
    const float u = (t - 0.80f) / 0.20f;                          // торможение
    return 20.0f * (1.0f - u * u * u);
}

// ── Подсветка линий «почти» и выигрыша ──────────────────────────────────────
// Возвращает маску барабанов, которые надо подсветить при near-miss:
// два совпавших символа тянут на себя внимание.
inline uint32_t NearMissReels(int s0, int s1, int s2) {
    if (s0 == s1 && s1 != s2) return (1u << 0) | (1u << 1);
    if (s1 == s2 && s0 != s1) return (1u << 1) | (1u << 2);
    if (s0 == s2 && s0 != s1) return (1u << 0) | (1u << 2);
    return 0u;
}

inline bool IsNearMiss(int s0, int s1, int s2) { return NearMissReels(s0, s1, s2) != 0u; }
inline bool IsTriple(int s0, int s1, int s2) { return s0 == s1 && s1 == s2; }

// ── Тикающая сумма (count-up) ───────────────────────────────────────────────
// Тикаем по НАКОПЛЕННОЙ кривой, а не по линейному времени: последние 20%
// времени показывают почти всю сумму, поэтому цифры «догоняют» результат.
inline int CountUpValue(int net, float t) {
    const float e = EaseOutCubic(std::min(1.0f, Clamp01(t) / 0.85f));
    return static_cast<int>(std::lround(static_cast<double>(net) * static_cast<double>(e)));
}

// ── Пульс лампы по рамке (бегущий огонёк) ───────────────────────────────────
// lampIndex — номер лампы, count — сколько их, t — время. Возвращает 0..1.
inline float MarqueePulse(int lampIndex, int count, float t, float speed = 1.0f) {
    if (count <= 0) return 0.0f;
    // 7.3 лампы/сек — НЕ пропорционально count: иначе на 24 лампах и dt=0.05 с
    // сдвиг выходит ровно 8.4 позиции и «яркая» лампа визуально стоит на месте.
    const float phase = std::fmod(t * speed * 16.0f, static_cast<float>(count));
    const float d = std::fabs(static_cast<float>(lampIndex) - phase);
    const float wrapped = std::min(d, static_cast<float>(count) - d);
    return Clamp01(1.0f - wrapped * 0.34f);   // мягкий хвост на 3 лампы
}

// ── Аккорд по фазе ──────────────────────────────────────────────────────────
// Один и тот же набор цветов ведёт игрока: спин — циан, стоп — золото,
// почти — янтарь, выигрыш — изумруд, джекпот — фиолет + золото.
struct PhasePalette {
    float accentR, accentG, accentB;   // акцентный цвет фазы
    float pulse;                       // 0..1, насколько сильно пульсировать
};

inline PhasePalette PaletteForPhase(int phase, bool jackpot) {
    switch (phase) {
        case PH_ANTICIPATION:   return { 0.35f, 0.85f, 1.00f, 0.55f };
        case PH_SPINNING:       return { 0.30f, 0.78f, 1.00f, 0.35f };
        case PH_REELS_STOPPING: return { 1.00f, 0.82f, 0.30f, 0.70f };
        case PH_NEAR_MISS:      return { 1.00f, 0.62f, 0.18f, 1.00f };
        case PH_REVEAL:         return jackpot ? PhasePalette{ 0.80f, 0.55f, 1.00f, 1.00f }
                                               : PhasePalette{ 0.35f, 1.00f, 0.62f, 1.00f };
        case PH_COUNTUP:        return jackpot ? PhasePalette{ 1.00f, 0.86f, 0.35f, 0.85f }
                                               : PhasePalette{ 0.35f, 1.00f, 0.62f, 0.60f };
        default:                return { 0.55f, 0.65f, 0.78f, 0.12f };
    }
}

} // namespace CasinoAnim2
