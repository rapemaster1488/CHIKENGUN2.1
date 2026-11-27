#pragma once

// CasinoTable.h — единая раскладка стола рулетки.
//
// ЗАЧЕМ. 2D-панель и 3D-стол считали позиции клеток ДВУМЯ копиями одной
// формулы — любая правка разводила фишки, подсветку и клик по разным местам.
// Теперь раскладка одна: 3D-объекты, клик и бросок фишки берут клетку отсюда.
// Модуль без Urho3D — только числа, поэтому проверяется тестом
// (tests/test_casino_table.cpp).
//
// ГЕОМЕТРИЯ 2026-11 (по фото настоящего стола). Раньше стол был почти
// квадратный (3.05 x 2.15), разметка лежала в центре, а колесо стояло прямо
// на ней. Настоящий рулеточный стол:
//
//   • вытянут от дилера к игроку (ось X, игрок смотрит в +X);
//   • колесо — на своём подиуме у дилера, ЗА разметкой, а не поверх;
//   • сетка чисел — 12 столбцов по 3 числа, от игрока к дилеру;
//   • под сеткой (со стороны игрока) — полосы внешних ставок;
//   • зеро — отдельная клетка вплотную к 1-2-3, у дилера.
//
//        +X ← игрок                                  дилер → ...
//   ┌─────────────────────────────────────────────────────────┐
//   │  [внешние]  │ разметка 3 x 12 │зеро│   (колесо)         │
//   └─────────────────────────────────────────────────────────┘

namespace CasinoTable {

// ── Габариты сукна ─────────────────────────────────────────────────────────
// Игрок просил «стол побольше растяни»: было 3.05 x 2.15, стало 6.80 x 3.40.
constexpr float kFeltX      = 8.20f;   // длина вдоль стола (от игрока к дилеру)
constexpr float kFeltZ      = 3.40f;   // ширина поперёк
constexpr float kSurfaceY   = 0.99f;   // высота сукна
constexpr float kCellY      = 0.9945f; // разметка ЛЕЖИТ на сукне (+4.5 мм), а не парит
constexpr float kChipY      = 1.012f;  // высота лежащей фишки

// ── Сетка чисел ────────────────────────────────────────────────────────────
// 12 столбцов по 3 числа. Порядок канонический: 1-2-3 у дилера, 34-35-36
// у игрока, внутри столбца — поперёк стола (ось Z).
constexpr int kCols = 3;
constexpr int kRows = 12;
constexpr float kCellW   = 0.30f;     // глубина клетки вдоль X
constexpr float kCellH   = 0.36f;     // ширина клетки поперёк Z
constexpr float kGridX0  = -2.25f;    // край сетки со стороны игрока
constexpr float kGridX1  = kGridX0 + (float)kRows * kCellW;   // край у дилера (1.35)
constexpr float kGridZ0  = -(float)kCols * kCellH * 0.5f;     // -0.54

// Зеро — отдельная клетка вплотную к столбцу 1-2-3, у дилера.
constexpr float kZeroW  = kCellW;
constexpr float kZeroH  = kCellH;
constexpr float kZeroX  = kGridX1 + kZeroW * 0.5f;
constexpr float kZeroZ  = 0.0f;

// Внешние ставки: три полосы, со стороны игрока (X < kGridX0).
constexpr float kOuterH    = (float)kCols * kCellH;   // 1.08 — во всю высоту сетки
constexpr float kOuterX0   = kGridX0;                 // край у сетки
constexpr float kOuterStep = kCellW;                  // шаг полос к игроку
constexpr float kOuterZ0   = kGridZ0;

// Колесо — на подиуме у дилера, ЗА разметкой. Радиус чаши 0.87
// (Wheel3D::kSkirtR) + зеро + зазор: центр отодвинут на 2.85.
constexpr float kWheelX = 2.85f;
constexpr float kWheelZ = 0.00f;
constexpr float kWheelY = kSurfaceY;

// Обод стола (дерево + латунный кант) — снаружи сукна.
constexpr float kRailWidth = 0.30f;
constexpr float kRailY     = kSurfaceY - 0.02f;

struct Point {
    float x = 0.0f;
    float z = 0.0f;
};

// Позиция клетки ставки в мировых координатах стола.
// zone: 0 — зеро, 1..36 — числа, 37..48 — внешние ставки.
inline Point ZoneCenter(int zone) {
    Point p;
    if (zone <= 0) {
        p.x = kZeroX;
        p.z = kZeroZ;
        return p;
    }
    if (zone <= 36) {
        const int i = zone - 1;
        // i / 3 — номер столбца, считая от дилера. Столбец 0 (1-2-3) у дилера,
        // последний (34-35-36) у игрока. i % 3 — ряд поперёк стола.
        const int colFromDealer = i / kCols;
        p.x = kGridX1 - (float)colFromDealer * kCellW - kCellW * 0.5f;
        p.z = kGridZ0 + (float)(i % kCols) * kCellH + kCellH * 0.5f;
        return p;
    }
    // Внешние ставки, полосами от сетки к игроку:
    //   37..42 — шесть стандартных пар (красное/чёрное, чёт/нечет, 1-18/19-36),
    //            по две в три яруса,
    //   43..45 — три дюжины,
    //   46..48 — три колонки.
    const int i = zone - 37;
    if (i < 6) {
        // Три яруса по две клетки: ярус i/2, внутри яруса половина i%2.
        p.x = kOuterX0 - kOuterStep * 0.5f;
        p.z = kOuterZ0 + ((float)(i / 2) + 0.25f + 0.5f * (float)(i % 2)) * (kOuterH / 3.0f);
    } else if (i < 9) {
        p.x = kOuterX0 - kOuterStep * 1.5f;
        p.z = kOuterZ0 + (float)(i - 6) * (kOuterH / 3.0f) + (kOuterH / 6.0f);
    } else {
        p.x = kOuterX0 - kOuterStep * 2.5f;
        p.z = kOuterZ0 + (float)(i - 9) * (kOuterH / 3.0f) + (kOuterH / 6.0f);
    }
    return p;
}

// Высота, на которой лежит фишка на этой клетке.
inline float ZoneChipY(int /*zone*/) { return kChipY; }

// Полуразмеры клетки зоны (для отрисовки разметки и для пика).
inline void ZoneHalfSize(int zone, float& halfX, float& halfZ) {
    if (zone == 0) {
        halfX = kZeroW * 0.5f;
        halfZ = kZeroH * 0.5f;
        return;
    }
    if (zone <= 36) {
        halfX = kCellW * 0.5f;
        halfZ = kCellH * 0.5f;
        return;
    }
    // 37..42 стоят ПАРАМИ в ярусе (половина яруса), дюжины и колонки
    // занимают весь ярус.
    halfX = kCellW * 0.5f;
    halfZ = (zone <= 42) ? (kOuterH / 12.0f) : (kOuterH / 6.0f);
}

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
// любом наборе ставок и чтобы клетку можно было нарисовать на сукне.
inline bool IsOnFelt(const Point& p) {
    const float ax = p.x < 0.0f ? -p.x : p.x;
    const float az = p.z < 0.0f ? -p.z : p.z;
    return ax <= kFeltX * 0.5f && az <= kFeltZ * 0.5f;
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

// Не залезает ли разметка на колесо: фишки не должны падать на чашу.
inline bool WheelClearOfTable() {
    const float dx = kWheelX - (kZeroX + kZeroW * 0.5f);
    const float dz = kWheelZ - kZeroZ;
    return (dx * dx + dz * dz) > 0.90f * 0.90f;
}

// Не пересекаются ли две зоны (проверка разметки тестом).
inline bool ZonesOverlap(int a, int b) {
    if (a == b) return false;
    Point ca = ZoneCenter(a), cb = ZoneCenter(b);
    float hax, haz, hbx, hbz;
    ZoneHalfSize(a, hax, haz);
    ZoneHalfSize(b, hbx, hbz);
    const float dx = ca.x - cb.x;
    const float dz = ca.z - cb.z;
    const float ax = dx < 0.0f ? -dx : dx;
    const float az = dz < 0.0f ? -dz : dz;
    return ax < (hax + hbx) - 1e-3f && az < (haz + hbz) - 1e-3f;
}

} // namespace CasinoTable
