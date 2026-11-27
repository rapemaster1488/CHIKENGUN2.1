#pragma once

// CasinoTable.h — единая раскладка стола рулетки.
//
// ЗАЧЕМ. Жалоба: «фишки появляются только на 2D табле, а на 3D столе это не
// фишки». Часть причины была в том, что 2D-панель и 3D-стол считали позиции
// клеток ДВУМЯ разными копиями одной и той же формулы — в CreateCasinoScene3D
// и во взгляде на стол. Любая правка в одной копии рассинхронизировала их:
// фишка летела в одну клетку, а подсветка загоралась на другой.
//
// Теперь раскладка одна: и 3D-объекты, и клик, и бросок фишки берут клетку
// отсюда. Модуль без Urho3D — только числа, поэтому проверяется тестом
// (tests/test_casino_table.cpp).

namespace CasinoTable {

// Размеры стола в метрах. Стол вытянут по X (длинная сторона к игроку).
constexpr float kFeltX      = 3.05f;
constexpr float kFeltZ      = 2.15f;
constexpr float kSurfaceY   = 0.99f;   // высота сукна
constexpr float kCellY      = 0.9915f; // инкрустация: +1.5 мм над сукном
constexpr float kChipY      = 1.012f;  // высота лежащей фишки

// Сетка чисел: 3 колонки x 12 рядов справа, зеро отдельно.
constexpr int kCols = 3;
constexpr int kRows = 12;
constexpr float kCellW = 0.32f;
constexpr float kCellH = 0.09f;

struct Point {
    float x = 0.0f;
    float z = 0.0f;
};

// Позиция клетки ставки в мировых координатах стола.
// zone: 0 — зеро, 1..36 — числа, 37..48 — внешние ставки.
inline Point ZoneCenter(int zone) {
    Point p;
    if (zone <= 0) {
        // Зеро — отдельная клетка справа от сетки чисел.
        p.x = 1.13f;
        p.z = 0.72f;
        return p;
    }
    if (zone <= 36) {
        const int i = zone - 1;
        p.x = 0.79f + (float)(i % kCols) * 0.34f;
        p.z = 0.60f - (float)(i / kCols) * 0.10f;
        return p;
    }
    // Внешние ставки (дюжины, колонки, красное/чёрное, чёт/нечет).
    const int i = zone - 37;
    p.x = 0.79f + (float)(i % 3) * 0.34f;
    p.z = -0.65f - (float)(i / 3) * 0.12f;
    return p;
}

// Высота, на которой лежит фишка на этой клетке.
inline float ZoneChipY(int /*zone*/) { return kChipY; }

// Попадает ли точка клика в клетку зоны (допуск — половина клетки).
inline bool HitZone(int zone, float x, float z, float halfW, float halfH) {
    const Point c = ZoneCenter(zone);
    const float dx = x - c.x;
    const float dz = z - c.z;
    const float ax = dx < 0.0f ? -dx : dx;
    const float az = dz < 0.0f ? -dz : dz;
    return ax <= halfW && az <= halfH;
}

// Лежит ли клетка внутри сукна. Нужно, чтобы фишка не улетала за стол при
// любом наборе ставок.
inline bool IsOnFelt(const Point& p) {
    const float ax = p.x < 0.0f ? -p.x : p.x;
    const float az = p.z < 0.0f ? -p.z : p.z;
    return ax <= kFeltX * 0.5f && az <= kFeltZ * 0.5f;
}

// Полуразмеры клетки: разметка старой комнаты — плотная сетка 3 x 12
// у дилера, внешние ставки полосами ниже, зеро отдельной клеткой справа.
inline void ZoneHalfSize(int zone, float& halfX, float& halfZ) {
    if (zone <= 0) {
        halfX = 0.10f;
        halfZ = 0.15f;
        return;
    }
    if (zone <= 36) {
        halfX = kCellW * 0.5f;
        halfZ = kCellH * 0.5f;
        return;
    }
    halfX = kCellW * 0.5f;
    halfZ = 0.06f;
}

// Колесо на компактном столе стоит в центре зала, рядом с разметкой.
// Радиус чаши (Wheel3D::kSkirtR = 0.87) не должен накрывать ни одну клетку.
constexpr float kWheelX = -0.62f;
constexpr float kWheelZ = -0.02f;
constexpr float kWheelY = kSurfaceY;   // чаша стоит прямо на сукне
constexpr float kWheelR = 0.87f;

inline bool WheelClearOfTable() {
    for (int z = 0; z < 49; ++z) {
        const Point c = ZoneCenter(z);
        float hx, hz;
        ZoneHalfSize(z, hx, hz);
        // Ближайшая точка клетки к центру колеса.
        const float dx = (c.x - kWheelX) - hx > 0.0f ? (c.x - kWheelX) - hx
                       : ((c.x - kWheelX) + hx < 0.0f ? -((c.x - kWheelX) + hx) : 0.0f);
        const float dz = (c.z - kWheelZ) - hz > 0.0f ? (c.z - kWheelZ) - hz
                       : ((c.z - kWheelZ) + hz < 0.0f ? -((c.z - kWheelZ) + hz) : 0.0f);
        if (dx * dx + dz * dz < kWheelR * kWheelR) return false;
    }
    return true;
}

// Все ли 49 клеток помещаются на сукно. Проверяется тестом: если раскладку
// подвинут и клетка вылезет за борт, игрок не сможет поставить туда фишку.
inline int ZonesOffFelt() {
    int n = 0;
    for (int z = 0; z < 49; ++z) {
        if (!IsOnFelt(ZoneCenter(z))) ++n;
    }
    return n;
}

} // namespace CasinoTable
