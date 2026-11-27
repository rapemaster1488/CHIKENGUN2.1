#pragma once

// CasinoTable.h — единая раскладка стола рулетки.
//
// ЗАЧЕМ. 2D-панель и 3D-стол считали позиции клеток ДВУМЯ копиями одной
// формулы — любая правка разводила фишки, подсветку и клик по разным местам.
// Теперь раскладка одна: 3D-объекты, клик и бросок фишки берут клетку отсюда.
// Модуль без Urho3D — только числа, поэтому проверяется тестом
// (tests/test_casino_table.cpp).
//
// ─────────────────────────────────────────────────────────────────────────
// ЭКСПЕРИМЕНТ: СТАРАЯ КОМНАТА (2026-12).
// Игрок: «верни старый обзор, раньше было получше, попробуем воссоздать
// комнату со старым казино как эксперимент». Поэтому вернулась компактная
// раскладка: квадратный стол у дилера, колесо в центре зала, камера из +X.
// Большой канонический стол (8.20 x 3.40, колесо на подиуме) жил в PR #44
// и не прижился — он визуально ужал зал.
//
// Если захочется вернуть большой вариант — это одна правка констант ниже
// плюс блок 7 в CreateCasinoScene3D и камера (см. комментарий у kFeltX).
// ─────────────────────────────────────────────────────────────────────────
//
//        +X ← игрок                                  дилер → ...
//   ┌─────────────────────────────────────────────────────────┐
//   │  [внешние]  │ разметка 3 x 12 │зеро│   (колесо)         │
//   └─────────────────────────────────────────────────────────┘

namespace CasinoTable {

// ── Габариты сукна ─────────────────────────────────────────────────────────
// Старый компактный стол: 3.05 x 2.15 — камера ставится в 2.85 м и зал
// читается «как раньше».
constexpr float kFeltX      = 6.00f;   // длина вдоль стола (от игрока к дилеру)
constexpr float kFeltZ      = 2.10f;   // ширина поперёк
constexpr float kSurfaceY   = 0.99f;   // высота сукна
constexpr float kCellY      = 0.9945f; // разметка ЛЕЖИТ на сукне (+4.5 мм), а не парит
constexpr float kChipY      = 1.012f;  // высота лежащей фишки

// ── Сетка чисел ────────────────────────────────────────────────────────────
// 12 столбцов по 3 числа. Порядок канонический: 1-2-3 у дилера, 34-35-36
// у игрока, внутри столбца — поперёк стола (ось Z).
constexpr int kCols = 3;
constexpr int kRows = 12;
constexpr float kCellW   = 0.215f;    // глубина клетки вдоль X
constexpr float kCellH   = 0.185f;    // ширина клетки поперёк Z
constexpr float kGridX0  = -0.72f;    // край сетки со стороны игрока
constexpr float kGridX1  = kGridX0 + (float)kRows * kCellW;   // край у дилера (1.86)
constexpr float kGridZ0  = -(float)kCols * kCellH * 0.5f;     // -0.2775

// Зеро — отдельная клетка вплотную к столбцу 1-2-3, у дилера.
constexpr float kZeroW  = kCellW;
constexpr float kZeroH  = kCellH;
constexpr float kZeroX  = kGridX1 + kZeroW * 0.5f;
constexpr float kZeroZ  = 0.0f;

// Внешние ставки: три полосы, со стороны игрока (X < kGridX0).
constexpr float kOuterH    = (float)kCols * kCellH;   // 0.555 — во всю высоту сетки
constexpr float kOuterX0   = kGridX0;                 // край у сетки
constexpr float kOuterStep = kCellW;                  // шаг полос к игроку
constexpr float kOuterZ0   = kGridZ0;

// Колесо — в центре зала, как в старой комнате. Радиус чаши 0.87
// (Wheel3D::kSkirtR): на компактном столе оно стоит рядом с разметкой,
// а не на ней, и остаётся героем кадра.
// Чаша радиусом 0.87 не должна наезжать на сетку чисел (её край -0.72):
// центр уводим на -1.62, тогда правый край чаши - 0.75 — ровно у борта.
constexpr float kWheelX = -1.62f;
constexpr float kWheelZ = -0.02f;
// Чаша стоит на своём подиуме (верх постамента 0.918), а не на сукне.
constexpr float kPodiumTopY = 1.02f;
constexpr float kWheelY = kPodiumTopY;

// Обод стола (дерево + латунный кант) — снаружи сукна.
constexpr float kRailWidth = 0.24f;
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
