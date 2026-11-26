#pragma once
//
// Feel.h — «сочность» стрельбы и бюджет источников света.
//
// Здесь живут вещи, которые чувствуются, но не видны в коде: кривая тряски
// камеры, отдача прицела, вспышка урона. Их нельзя проверить глазами в
// песочнице (полной сборки нет), поэтому выносим в чистые функции с тестами.
//
// Второй блок — LightBudget: в forward-рендере Urho3D каждый динамический
// источник света это отдельный проход по экрану. На встройке 9 источников =
// 9 проходов вместо одного, и это главный пожиратель кадров, а не геометрия.
// Поэтому бюджет решается явно и проверяется тестом.
//
#include <algorithm>
#include <cmath>

namespace Feel {

// ── Тряска камеры ───────────────────────────────────────────────────────────
// Единственное место, где задаётся «сила тряски». Раньше вызовы вида
// TriggerCameraShake(0.05f) были разбросаны по коду без масштаба: 0.05 —
// это 5 сантиметров смещения камеры, то есть незаметно, а 0.6 — уже тошнит.
inline float ShakeFor(float base, float scale) {
    if (scale < 0.0f) scale = 0.0f;
    return base * scale;
}

// Тряска для выстрела конкретного оружия. Тяжёлое — бьёт по камере сильнее.
// Значения подобраны так, чтобы пистолет давал «толчок», а миниган — «дрожь»,
// но ни один не сбивал прицел больше чем на пару градусов.
enum WeaponFeelClass {
    FEEL_LIGHT = 0,   // пистолет, ПП
    FEEL_STANDARD,    // винтовка, дробовик
    FEEL_HEAVY,       // снайперка, гранатомёт
    FEEL_SPINUP       // миниган — короткий импульс на каждый патрон
};

inline float ShotShake(int feelClass) {
    switch (feelClass) {
    case FEEL_LIGHT:    return 0.012f;
    case FEEL_STANDARD: return 0.024f;
    case FEEL_HEAVY:    return 0.048f;
    case FEEL_SPINUP:   return 0.008f;
    default:            return 0.020f;
    }
}

// Затухание тряски: экспонента, чтобы «отпускало» быстро, но не рывком.
// exp(-dt*k) — стабильнее, чем линейный Lerp на больших dt (не даёт
// отрицательных значений при просадке кадров).
inline float DecayShake(float current, float dt, float rate) {
    if (current <= 0.0f) return 0.0f;
    if (dt <= 0.0f) return current;
    const float v = current * std::exp(-dt * rate);
    return v < 1e-4f ? 0.0f : v;
}

// Сдвиг камеры из тряски. Детерминированный «шум» на двух косинусах:
// rand() на каждом кадре даёт белый шум, который читается как баг, а не как
// удар. Здесь плавная кривая с разными частотами по осям.
inline void ShakeOffset(float t, float amount, float out[2]) {
    out[0] = amount * std::cos(t * 41.0f) * 0.7f;
    out[1] = amount * std::sin(t * 37.0f + 1.3f);
}

// Отдача: насколько прицел уводит вверх и в сторону от одного выстрела.
// side — детерминированный знак по счётчику выстрелов (половина влево,
// половина вправо), чтобы очередь не уползала строго в одну сторону.
inline void RecoilKick(float& pitch, float& yaw, int feelClass, int shotIndex) {
    float up = 0.0f, side = 0.0f;
    switch (feelClass) {
    case FEEL_LIGHT:    up = 0.55f; side = 0.16f; break;
    case FEEL_STANDARD: up = 0.95f; side = 0.24f; break;
    case FEEL_HEAVY:    up = 1.70f; side = 0.30f; break;
    case FEEL_SPINUP:   up = 0.30f; side = 0.40f; break;
    default:            up = 0.80f; side = 0.20f; break;
    }
    pitch += up;
    yaw += (shotIndex % 2 == 0) ? side : -side;
}

// ── Вспышка урона ───────────────────────────────────────────────────────────
// Прозрачность красной рамки по времени с последнего попадания.
// Пик в момент удара, спад за ~0.55 с: заметно, но не мешает целиться.
inline float DamageFlashAlpha(float sinceHit) {
    if (sinceHit < 0.0f) return 0.0f;
    const float dur = 0.55f;
    if (sinceHit >= dur) return 0.0f;
    const float k = sinceHit / dur;               // 0 → 1
    const float fade = 1.0f - k;
    return std::min(1.0f, fade * fade * 1.15f);
}

// Пульс рамки при низком HP (HP<30%): чем меньше здоровье, тем чаще пульс.
inline float LowHealthPulseAlpha(float hpFraction, float time) {
    if (hpFraction >= 0.30f) return 0.0f;
    const float sev = (0.30f - std::max(0.0f, hpFraction)) / 0.30f;  // 0..1
    const float rate = 2.0f + 4.0f * sev;                           // 2..6 Гц
    const float s = 0.5f + 0.5f * std::sin(time * rate * 6.28318f);
    return std::min(1.0f, (0.22f + 0.55f * sev) * s);
}

// ── Бюджет источников света ─────────────────────────────────────────────────
// Value — «сколько стоит» источник в проходах рендера. Тени дороже вдвое:
// они добавляют проход по карте теней плюс сэмплирование в шейдере.
struct LightCost {
    int   points = 0;
    int   spots  = 0;
    int   shadowed = 0;
};

inline int LightPasses(const LightCost& c) {
    return c.points + c.spots * 2 + c.shadowed * 3;
}

// Сколько угловых светильников оставить при данном классе железа.
// Солнце даёт основную картинку, поэтому урезаем именно заливку зала:
// на встройке два угловых источника вместо четырёх почти неотличимы по
// картинке, но дают минус два полных прохода по экрану.
inline int FloodLightsFor(int tier) {
    switch (tier) {
    case 0:  return 2;   // встройка
    case 1:  return 2;   // слабая
    case 2:  return 4;   // средняя
    case 3:  return 4;   // мощная
    default: return 4;
    }
}

// Оставить ли пульсирующие неоновые TubeLight'ы (по бокам площади).
// На встройке они «съедают» два прохода, а их вклад в картинку дублирует цвет
// источников заливки — поэтому на слабом железе отключаем.
inline bool KeepNeonLights(int tier) { return tier >= 2 || tier == 4; }

// Дополнительные точечные огни (дульные вспышки, ядра кур) — отдельный лимит:
// они короткоживущие, поэтому их выгоднее включать импульсно, а не держать.
inline int TransientLightBudget(int tier) {
    switch (tier) {
    case 0:  return 1;
    case 1:  return 2;
    case 2:  return 3;
    case 3:  return 4;
    default: return 3;
    }
}

// Уложится ли сцена в бюджет для данного класса железа.
// Ориентир: 6 проходов — комфорт для iGPU, 10+ начинает «жевать» кадры.
inline bool WithinBudget(const LightCost& c, int tier) {
    const int limit = (tier <= 1) ? 7 : (tier == 2 ? 12 : 20);
    return LightPasses(c) <= limit;
}

inline int PassLimitFor(int tier) {
    return (tier <= 1) ? 7 : (tier == 2 ? 12 : 20);
}

// ── Дальность прорисовки ────────────────────────────────────────────────────
// На встройке режем не «дальность видимости» (это ломает геймплей), а
// дальность отрисовки МЕЛОЧИ: трава, декали, частицы.
inline float DetailRange(float baseRange, int tier) {
    switch (tier) {
    case 0:  return baseRange * 0.55f;
    case 1:  return baseRange * 0.70f;
    case 2:  return baseRange * 0.90f;
    default: return baseRange;
    }
}

} // namespace Feel
