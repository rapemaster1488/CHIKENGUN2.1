// Testy UV-sxemy srubа i valgonki.
// Derzhat regressiyu: odin tayl tekstury ne dolzhen rastyagivat'sya na vse
// shest' metrov brevna, inache volokna prevrashchayutsya v polosu.
#include "../src/BanyaUv.h"
#include <cstdio>

using namespace BanyaUv;

static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

static bool near(float a, float b, float eps = 1e-4f) {
    float d = a - b;
    return (d < 0 ? -d : d) <= eps;
}

int main() {
    std::puts("== 1. Tayl brevna = 0.5 m ==");
    check(near(LogTileMeters(), 0.5f), "tayl brevna rovno 0.5 m");
    check(LogTileMeters() > 0.0f, "tayl polozhitel'nyy");

    std::puts("== 2. Brevno 6 m dayot 12 povtorov ==");
    check(near(TilesAlong(6.0f, LogTileMeters()), 12.0f), "6 m / 0.5 m = 12");
    check(HasEnoughDetail(6.0f, LogTileMeters(), 8.0f),
          "u 6-metrovogo brevna hvataet detala (ne men'she 8 povtorov)");
    // Regressiya: ran'she Cylinder.mdl dавал 1 tayl na vse shest' metrov.
    check(!HasEnoughDetail(6.0f, 6.0f, 2.0f),
          "odin tayl na shest' metrov NE prohodit porog detala");

    std::puts("== 3. Valgonka i polok ==");
    check(near(PlankTileMeters(), 1.0f), "tayl doski rovno 1 m");
    check(TilesAlong(6.2f, PlankTileMeters()) > 6.0f, "polovitsa 6.2 m daet >6 povtorov");
    check(TilesAlong(0.9f, PlankTileMeters()) >= 0.9f, "korotkaya doska 0.9 m — pochti tayl");

    std::puts("== 4. V poperek stvola odin oborot ==");
    check(near(LogV(0.0f), 0.0f), "ugol 0 -> v 0");
    check(near(LogV(3.14159265f), 0.5f), "poloborota -> 0.5");
    // Polnyy oborot daet V=1, a ne V=0: dlya tekstury eto ta zhe tochka (tayl
    // zamknut), no dlya setki geometrii — posledniy stolbets, a ne pervyy.
    // Esli vernut' 0, normal' na shve razvernetsya i poyavitsya temnaya polosa.
    check(near(LogV(6.2831853f), 1.0f), "polnyy oborot -> V=1 (shov tayla, ne otkat v 0)");
    check(LogV(-1.5708f) > 0.7f, "otritsatel'nyy ugol normalizovan v 0..1");

    std::puts("== 5. U vdol' stvola ==");
    check(near(LogU(0.0f), 0.0f), "nachalo -> U 0");
    check(near(LogU(0.5f), 1.0f), "polmetra -> U 1 (odin tayl)");
    check(near(LogU(3.0f), 6.0f), "tri metra -> shest' taylov");

    std::puts("== 6. Kadr srubа ==");
    check(near(LogRadius(), 0.23f), "radius brevna 23 sm");
    check(LogStep() > LogRadius() * 1.8f && LogStep() < LogRadius() * 2.0f,
          "shag ventsa — pochti dva radiusa (brevna szhaty, no ne perekryvayutsya)");
    // Vosem' ventsov po shagu dolzhny zakryt' stenku 3.3 m.
    check(LogStep() * 8.0f + LogRadius() * 2.0f > 3.3f, "vosem' ventsov zakryvayut stenu do potolka");

    std::puts("== 7. Vyrozhdennye vhody ==");
    check(near(TilesAlong(0.0f, LogTileMeters()), 0.0f), "nulevaya dlina -> 0 povtorov");
    check(near(TilesAlong(5.0f, 0.0f), 0.0f), "nulevoy tayl -> 0 (bez deleniia na nol')");
    check(!HasEnoughDetail(0.0f, LogTileMeters(), 1.0f), "pustaya dlina ne prohodit porog");

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
