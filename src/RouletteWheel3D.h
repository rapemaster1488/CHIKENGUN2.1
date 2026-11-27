#pragma once

// RouletteWheel3D — геометрия 3D-колеса, вынесенная из MyApp.
//
// Зачем отдельный файл. Колесо собирается из 37 лунок, 37 перегородок и 37
// номеров, и любая арифметическая ошибка в раскладке видна только глазами на
// конкретном кадре: лунки наезжают друг на друга, номера уезжают с карманов,
// шарик «падает» мимо сектора. Проверить это на GPU в CI нельзя, а проверить
// числа — можно. Поэтому все радиусы, ширины и углы живут здесь, а
// tests/test_roulette_wheel3d.cpp проверяет, что:
//
//   * 37 лунок помещаются по окружности и не перекрываются;
//   * угол каждой лунки совпадает с Casino::WheelSectorAngleDeg;
//   * номер стоит НА СВОЕЙ лунке, а не на соседней;
//   * дорожка шарика шире кольца лунок — иначе шарик не может упасть внутрь;
//   * перегородка стоит ровно между двумя лунками.
//
// Модуль без Urho3D и без SDL: только числа (sin/cos из <cmath>).

#include <cmath>

namespace Wheel3D {

// ── Радиусы (метры, локальные координаты хаба) ───────────────────────────────
// Чаша: юбка → корпус → борт → дорожка шарика.
constexpr float kSkirtR      = 0.870f;
constexpr float kBodyR       = 0.850f;
constexpr float kLipR        = 0.830f;
constexpr float kGoldRimR    = 0.832f;
constexpr float kTrackR      = 0.750f;   // внешняя кромка дорожки шарика
constexpr float kTrackInnerR = 0.680f;   // внутренняя кромка дорожки

// Барабан с лунками.
constexpr float kDrumR       = 0.580f;   // внешний край кольца лунок
constexpr float kPocketR     = 0.455f;   // середина лунки по радиусу
constexpr float kPocketW     = 0.064f;   // ширина лунки (хорда)
constexpr float kPocketLen   = 0.225f;   // длина лунки к центру
constexpr float kNumberR     = 0.545f;   // где стоит номер внутри лунки
constexpr float kFretW       = 0.008f;   // толщина золотой перегородки
constexpr float kFunnelR     = 0.310f;   // воронка в центре
constexpr float kBallR       = 0.024f;   // радиус шарика (половина диаметра)

// Высоты: шарик идёт по треку выше кольца лунок и падает вниз-внутрь.
constexpr float kTrackY      = 0.108f;
constexpr float kLipY        = 0.138f;
constexpr float kPocketY     = 0.015f;
constexpr float kNumberY     = 0.032f;
constexpr float kBallTrackY  = 0.130f;   // шарик на дорожке
constexpr float kBallPocketY = 0.015f;   // шарик в лунке
constexpr float kBallTrackRadius = 0.660f; // радиус дорожки для физики шарика
constexpr float kBallPocketRadius = 0.455f; // радиус лунки для физики шарика

// ── Углы ─────────────────────────────────────────────────────────────────────
constexpr int kPockets = 37;

inline float SectorAngleDeg(int index) {
    return (float)index * (360.0f / (float)kPockets);
}

inline float SectorAngleRad(int index) {
    return SectorAngleDeg(index) * 0.017453292519943295f;
}

// Половина углового шага лунки. Нужна и для перегородки, и для проверки,
// что соседние лунки не наезжают друг на друга.
inline float HalfStepDeg() { return 180.0f / (float)kPockets; }

// Ширина лунки в угловых градусах на радиусе kPocketR: сколько градусов
// занимает хорда kPocketW. Должна быть меньше шага, иначе лунки слипнутся.
inline float PocketAngularWidthDeg() {
    const float half = (kPocketW * 0.5f) / kPocketR;      // радианы
    return 2.0f * half * 57.29577951308232f;
}

// Есть ли зазор между соседними лунками.
inline bool PocketsFit() {
    return PocketAngularWidthDeg() < (360.0f / (float)kPockets);
}

// Угол центра лунки номер index (0..36).
inline float PocketCenterDeg(int index) {
    return SectorAngleDeg(index);
}

// Угол перегородки ПОСЛЕ лунки index — ровно на полшага дальше.
inline float FretAngleDeg(int index) {
    return SectorAngleDeg(index) + HalfStepDeg();
}

// Координаты (x, z) точки на радиусе r под углом deg. Та же формула, что в
// MyApp: x = sin, z = cos — иначе номер окажется с другой стороны колеса.
inline void PolarToXZ(float r, float deg, float& x, float& z) {
    const float rad = deg * 0.017453292519943295f;
    x = std::sin(rad) * r;
    z = std::cos(rad) * r;
}

// Лежит ли номер на своей лунке: радиальное расстояние от центра лунки
// должно быть меньше её полудлины.
inline bool NumberInsidePocket(int /*index*/) {
    const float along = std::fabs(kNumberR - kPocketR);
    return along < (kPocketLen * 0.5f);
}

// Шарик физически может упасть с дорожки в лунку: дорожка дальше от центра,
// чем лунка, и обе — внутри чаши.
inline bool BallCanDrop() {
    return kBallTrackRadius > kBallPocketRadius &&
           kBallTrackRadius + kBallR < kTrackR &&
           kBallPocketRadius - kBallR > 0.0f;
}

} // namespace Wheel3D
