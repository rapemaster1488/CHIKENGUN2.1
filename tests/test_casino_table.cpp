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
    }

    std::puts("== 2. Сетка чисел не наезжает сама на себя ==");
    {
        // 36 чисел в сетке 3x12: соседние по колонке не ближе шага клетки.
        bool spaced = true;
        for (int r = 0; r < 12; ++r) {
            for (int c = 0; c < 3; ++c) {
                const int zone = 1 + r * 3 + c;
                const Point a = ZoneCenter(zone);
                check(near(a.x, 0.79f + (float)c * 0.34f) &&
                      near(a.z, 0.60f - (float)r * 0.10f),
                      "координата клетки совпадает с формулой сетки");
                if (c + 1 < 3) {
                    const Point b = ZoneCenter(zone + 1);
                    if (std::fabs(b.x - a.x) < kCellW) spaced = false;
                }
                if (r + 1 < 12) {
                    const Point b = ZoneCenter(zone + 3);
                    if (std::fabs(b.z - a.z) < kCellH) spaced = false;
                }
            }
        }
        check(spaced, "числа не перекрывают друг друга");
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
        // Внешние 12 клеток начинаются ниже сетки чисел (z отрицательнее).
        const Point last = ZoneCenter(36);   // низ сетки чисел
        bool below = true;
        for (int z = 37; z < 49; ++z) {
            if (ZoneCenter(z).z > last.z + 1e-3f) below = false;
        }
        check(below, "внешние ставки ниже сетки чисел");
        check(!near(ZoneCenter(37).z, last.z, 1e-3f), "внешние не наезжают на числа");
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
        check(kCellY < kSurfaceY, "клетки чуть утоплены в сукно");
    }

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
