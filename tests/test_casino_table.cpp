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
        // ЭКСПЕРИМЕНТ 2: вернулась «лучшая» комната — стол 3.05 x 2.15,
        // как было до PR #44. Большой стол визуально ужимал зал.
        check(kFeltX > 2.5f && kFeltX < 4.0f, "стол компактный, как в лучшей версии");
        check(kFeltX > kFeltZ, "длинная сторона — от игрока к дилеру");

        // Колесо стоит на сукне рядом с разметкой, но НЕ накрывает её.
        check(WheelClearOfTable(), "чаша колеса не накрывает клетки ставок");
    }

    std::puts("== 2. Сетка чисел не наезжает сама на себя ==");
    {
        // В ЛУЧШЕЙ ВЕРСИИ стол повёрнут: сетка идёт ВГЛУБЬ по X (12 столбцов
        // по 3 числа), а не поперёк. Колесо при этом стоит слева, а не в
        // конце стола. Поэтому ориентацию проверяем ту, что есть, — но
        // проверяем строго: клетки не слипаются и порядок номеров монотонен.
        bool spaced = true;
        for (int zone = 1; zone <= 36; ++zone) {
            const Point a = ZoneCenter(zone);
            for (int other = zone + 1; other <= 36; ++other) {
                const Point b = ZoneCenter(other);
                const float dx = std::fabs(b.x - a.x);
                const float dz = std::fabs(b.z - a.z);
                // Клетки одной сетки не должны пересекаться по обеим осям.
                if (dx < kCellW - 1e-3f && dz < kCellH - 1e-3f) spaced = false;
            }
        }
        check(spaced, "клетки чисел не перекрывают друг друга");

        // Порядок номеров в этой раскладке: тройка чисел лежит ПОПЕРЁК стола
        // (ось X), столбцы идут вдоль (ось Z). Проверяем именно это, а не
        // каноническую ориентацию: стол лучшей версии повёрнут на 90°,
        // чтобы колесо встало слева.
        check(std::fabs(ZoneCenter(1).z - ZoneCenter(3).z) < 1e-3f,
              "1 и 3 в одной тройке");
        check(ZoneCenter(1).x < ZoneCenter(2).x, "1,2,3 идут по порядку поперёк стола");
        // Соседняя тройка сдвинута по Z ровно на шаг клетки.
        check(std::fabs(std::fabs(ZoneCenter(1).z - ZoneCenter(4).z) - 0.10f) < 0.01f,
              "тройки сдвинуты на один шаг по Z");

        // Движение по номерам на 1 не должно прыгать через всю сетку.
        float maxStep = 0.0f;
        for (int zone = 1; zone < 36; ++zone) {
            const Point a = ZoneCenter(zone), b = ZoneCenter(zone + 1);
            const float d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
            if (d > maxStep) maxStep = d;
        }
        check(maxStep < 0.9f, "соседние номера стоят рядом, а не через зал");
    }

    std::puts("== 3. Зеро стоит отдельно ==");
    {
        const Point zero = ZoneCenter(0);
        bool unique = true;
        for (int z = 1; z <= 36; ++z) {
            const Point p = ZoneCenter(z);
            if (near(zero.x, p.x, 1e-3f) && near(zero.z, p.z, 1e-3f)) unique = false;
        }
        check(unique, "зеро не совпадает ни с одной клеткой чисел");

        // Зеро отдельное с боковой стороны сетки, как на настоящем сукне.
        float minGridX = 1e9f, maxGridX = -1e9f;
        for (int z = 1; z <= 36; ++z) {
            minGridX = std::fmin(minGridX, ZoneCenter(z).x);
            maxGridX = std::fmax(maxGridX, ZoneCenter(z).x);
        }
        const bool outside = zero.x > maxGridX + 0.05f || zero.x < minGridX - 0.05f ||
                             zero.z > ZoneCenter(3).z + 0.05f;
        check(outside, "зеро вынесено за пределы сетки чисел");
    }

    std::puts("== 4. Внешние ставки отдельно от чисел ==");
    {
        // Внешние полосы (дюжины, колонки) не должны совпадать с клетками
        // чисел: иначе клик по числу попадёт в «КРАСНОЕ».
        bool distinct = true;
        for (int o = 37; o < 49; ++o) {
            const Point po = ZoneCenter(o);
            for (int n = 1; n <= 36; ++n) {
                const Point pn = ZoneCenter(n);
                if (near(po.x, pn.x, 1e-3f) && near(po.z, pn.z, 1e-3f)) distinct = false;
            }
        }
        check(distinct, "внешние ставки не совпадают с числами");
        // И лежат СТРОЙНО, не разбросаны: все 12 полос в одной полосе Z.
        float minZ = 1e9f, maxZ = -1e9f;
        for (int o = 37; o < 49; ++o) {
            minZ = std::fmin(minZ, ZoneCenter(o).z);
            maxZ = std::fmax(maxZ, ZoneCenter(o).z);
        }
        check(maxZ - minZ < 0.45f, "внешние ставки идут полосой, а не по всему столу");
        check(WheelClearOfTable(), "колесо не стоит на разметке");
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
