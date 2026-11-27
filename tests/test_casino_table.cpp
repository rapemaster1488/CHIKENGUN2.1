// Тесты раскладки стола рулетки.
//
// ЖАЛОБА: «фишки появляются только на 2D табле, а на 3D столе это не фишки».
// Часть причины — две копии одной формулы позиций клеток. Здесь проверяется
// единая раскладка: клетки не наезжают друг на друга, все внутри сукна, и
// клик попадает именно в ту зону, по которой попал.
#include "../src/CasinoTable.h"
#include "../src/CasinoSim.h"

#include <cstdio>
#include <cmath>

using namespace CasinoTable;

static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}
static bool near(float a, float b, float eps = 1e-4f) {
    const float d = a - b;
    return (d < 0 ? -d : d) <= eps;
}

int main() {
    std::puts("== 1. Клетки внутри сукна ==");
    {
        check(ZonesOffFelt() == 0, "все 49 клеток на сукне");
        check(IsOnFelt(ZoneCenter(0)), "зеро на столе");
        check(IsOnFelt(ZoneCenter(1)), "первое число на столе");
        check(IsOnFelt(ZoneCenter(36)), "последнее число на столе");
        check(IsOnFelt(ZoneCenter(48)), "последняя внешняя ставка на столе");
        // Заведомо снаружи — проверка самой функции.
        check(!IsOnFelt(Point{99.0f, 0.0f}), "точка за столом не на сукне");
        // Стол большой: игрок просил «растяни как настоящий».
        check(kFeltX >= 6.5f && kFeltZ >= 3.0f, "стол вытянут по канону (>= 6.5 x 3.0)");
        check(kFeltX > kFeltZ, "длинная сторона — от игрока к дилеру");
    }

    std::puts("== 2. Сетка чисел не наезжает сама на себя ==");
    {
        // 12 столбцов по 3 числа: столбцы идут ПО ГЛУБИНЕ (X), числа в
        // столбце — поперёк стола (Z). Канон: 1-2-3 у дилера, 34-36 у игрока.
        bool spaced = true;
        for (int i = 0; i < 36; ++i) {
            const int zone = 1 + i;
            const Point a = ZoneCenter(zone);
            if (i + 3 <= 35) {
                // Соседний столбец — по X, не должен наезжать.
                const Point b = ZoneCenter(zone + 3);
                if (std::fabs(b.x - a.x) < kCellW - 1e-3f) spaced = false;
            }
            if (i % 3 != 2) {
                // Соседнее число в столбце — по Z.
                const Point b = ZoneCenter(zone + 1);
                if (std::fabs(b.z - a.z) < kCellH - 1e-3f) spaced = false;
            }
        }
        check(spaced, "числа не перекрывают друг друга");
        // Канон: 1-2-3 ближе к дилеру, чем 34-35-36.
        check(ZoneCenter(1).x > ZoneCenter(34).x, "1-2-3 у дилера, 34-36 у игрока");
        // Внутри столбца числа идут вдоль Z.
        check(near(ZoneCenter(1).x, ZoneCenter(3).x), "1 и 3 в одном столбце");
        check(ZoneCenter(1).z < ZoneCenter(2).z, "1,2,3 идут по порядку поперёк");
    }

    std::puts("== 3. Зеро стоит отдельно ==");
    {
        const Point zero = ZoneCenter(0);
        // Зеро не должно совпасть ни с одним числом.
        bool unique = true;
        for (int z = 1; z <= 36; ++z) {
            const Point p = ZoneCenter(z);
            if (near(zero.x, p.x, 1e-3f) && near(zero.z, p.z, 1e-3f)) unique = false;
        }
        check(unique, "зеро не совпадает ни с одной клеткой чисел");
        check(zero.x > 1.0f, "зеро справа от сетки — как на настоящем столе");
    }

    std::puts("== 4. Внешние ставки отдельно от чисел ==");
    {
        // Внешние полосы лежат СО СТОРОНЫ ИГРОКА от сетки (X меньше), как на
        // настоящем сукне: сетка — у дилера, полосы — ближе к игроку.
        float minGridX = 1e9f, maxOuterX = -1e9f;
        for (int z = 1; z <= 36; ++z) minGridX = std::fmin(minGridX, ZoneCenter(z).x);
        for (int z = 37; z < 49; ++z)  maxOuterX = std::fmax(maxOuterX, ZoneCenter(z).x);
        check(maxOuterX < minGridX, "внешние ставки ближе к игроку, чем сетка чисел");
        // И не залезают на колесо.
        check(WheelClearOfTable(), "колесо не стоит на разметке");
        // Зеро — у дилера, вплотную к 1-2-3.
        check(ZoneCenter(0).x > ZoneCenter(1).x, "зеро рядом с 1-2-3 у дилера");
    }

    std::puts("== 5. Клик попадает в свою клетку ==");
    {
        const float halfW = kCellW * 0.5f;
        const float halfH = kCellH * 0.5f;
        for (int z = 0; z < 49; ++z) {
            const Point c = ZoneCenter(z);
            // Центр клетки — попадание.
            if (!HitZone(z, c.x, c.z, halfW, halfH)) ++g_fail;
            // Далеко в стороне — промах.
            if (HitZone(z, c.x + 5.0f, c.z, halfW, halfH)) ++g_fail;
        }
        ++g_pass;
        check(true, "клик по центру каждой клетки засчитывается");
        // Промах заведомо мимо.
        check(!HitZone(1, 99.0f, 99.0f, halfW, halfH), "клик мимо стола не засчитывается");
    }

    std::puts("== 6. Раскладка согласована с логикой ставок ==");
    {
        // Каждая зона обязана иметь осмысленную ставку: иначе клетка будет
        // кликабельной, но бесполезной.
        bool allValid = true;
        for (int z = 0; z < 49; ++z) {
            const Casino::Bet b = Casino::BetForTableZone(z, 1);
            if (b.amount != 1) allValid = false;
            if (b.type < Casino::BET_RED || b.type >= Casino::BET_COUNT) allValid = false;
            if (b.type == Casino::BET_NUMBER && (b.number < 0 || b.number > 36)) allValid = false;
        }
        check(allValid, "у каждой из 49 клеток корректная ставка");
        check(Casino::kTableZoneCount == 49, "зон ровно 49, как клеток");
    }

    std::puts("== 7. Высота фишки ==");
    {
        check(ZoneChipY(0) > kSurfaceY, "фишка лежит НАД сукном");
        check(ZoneChipY(0) - kSurfaceY < 0.06f, "и не парит над столом");
        // Разметка — инкрустация: она на 4.5 мм ВЫШЕ сукна, но ниже фишки.
        // Раньше клетки парили на 8 мм (0.984 против 0.955 сукна) и читались
        // как «доски поверх стола».
        check(kCellY > kSurfaceY, "разметка лежит НА сукне, а не парит");
        check(kCellY - kSurfaceY < 0.01f, "и не выше 1 см над сукном");
        check(kChipY > kCellY, "фишка выше разметки");
    }

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
