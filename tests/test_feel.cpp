// Testy "sochnosti" strel'by i byudzheta istochnikov sveta.
#include "../src/Feel.h"
#include <cstdio>

static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}
static bool near(float a, float b, float eps = 0.001f) {
    return (a > b ? a - b : b - a) <= eps;
}

int main() {
    using namespace Feel;

    std::puts("== 1. Tryaska kamery ==");
    check(near(ShakeFor(0.05f, 2.0f), 0.10f), "masshtab 2x");
    check(near(ShakeFor(0.05f, 0.0f), 0.0f), "nulevoy masshtab");
    check(near(ShakeFor(0.05f, -3.0f), 0.0f), "otricatelnyy ne lomaet");
    check(ShotShake(FEEL_HEAVY) > ShotShake(FEEL_STANDARD), "tyazheloe b'et silnee");
    check(ShotShake(FEEL_STANDARD) > ShotShake(FEEL_LIGHT), "standart silnee legkogo");
    check(ShotShake(FEEL_SPINUP) < ShotShake(FEEL_LIGHT), "minigan drobit, ne b'et");
    check(ShotShake(99) > 0.0f, "neizvestnyy klass ne dayot nol");

    std::puts("== 2. Zatuhanie tryaski ==");
    float s = 0.5f;
    for (int i = 0; i < 60; ++i) s = DecayShake(s, 1.0f / 60.0f, 12.0f);
    check(s < 0.001f, "za sekundu zatihaet v nol");
    check(DecayShake(0.5f, 0.0f, 12.0f) == 0.5f, "pri dt=0 ne menyaet");
    check(DecayShake(0.0f, 0.1f, 12.0f) == 0.0f, "iz nulya ne uhodit v minus");
    check(DecayShake(0.5f, 0.5f, 12.0f) < 0.5f * 0.01f, "bolshoy dt stabilen (eksponenta)");
    check(DecayShake(0.5f, 0.016f, 12.0f) > 0.0f, "na carbonate ne obnulyaetsya");

    std::puts("== 3. Sdvig kamery ==");
    float off[2];
    ShakeOffset(0.0f, 0.05f, off);
    check(off[0] != off[1], "osi ne sovpadayut (ne odnoosnyy shum)");
    check(off[0] <= 0.05f && off[0] >= -0.05f, "X v predelah amplitudy");
    check(off[1] <= 0.05f && off[1] >= -0.05f, "Y v predelah amplitudy");
    ShakeOffset(0.0f, 0.0f, off);
    check(near(off[0], 0.0f) && near(off[1], 0.0f), "nulevaya amplituda = nulevoy sdvig");

    std::puts("== 4. Otdacha ==");
    float pitch = 0.0f, yaw = 0.0f;
    RecoilKick(pitch, yaw, FEEL_STANDARD, 0);
    check(pitch > 0.0f, "otdacha vedet vverh");
    check(yaw > 0.0f, "pervyy vystrel v odnu storonu");
    float p2 = 0.0f, y2 = 0.0f;
    RecoilKick(p2, y2, FEEL_STANDARD, 1);
    check(y2 < 0.0f, "vtoroy v druguyu (ochered ne upolzaet)");
    float p3 = 0.0f, y3 = 0.0f;
    RecoilKick(p3, y3, FEEL_HEAVY, 0);
    check(p3 > pitch, "tyazheloe kidayet vyshe");
    // Summa ocheredi iz 10 vystrelov ne dolzhna uyti v storonu
    float yawAcc = 0.0f, dummy = 0.0f;
    for (int i = 0; i < 10; ++i) RecoilKick(dummy, yawAcc, FEEL_STANDARD, i);
    check(near(yawAcc, 0.0f, 0.001f), "10 vystrelov: bokovoy snos = 0");

    std::puts("== 5. Vspyshka urona ==");
    check(near(DamageFlashAlpha(-0.1f), 0.0f), "do popadaniya net");
    check(DamageFlashAlpha(0.0f) > 0.9f, "v moment udara yarko");
    check(DamageFlashAlpha(0.275f) < DamageFlashAlpha(0.05f), "zatihaet so vremenem");
    check(near(DamageFlashAlpha(0.6f), 0.0f), "cherez 0.55s ugasla");
    check(DamageFlashAlpha(5.0f) == 0.0f, "davno — nol");

    std::puts("== 6. Nizkoe HP ==");
    check(near(LowHealthPulseAlpha(0.9f, 1.0f), 0.0f), "pri normalnom HP net pulsacii");
    check(LowHealthPulseAlpha(0.2f, 0.0f) >= 0.0f, "pri nizkom HP ne otricatelno");
    float maxA = 0.0f, minA = 1.0f;
    for (int i = 0; i < 200; ++i) {
        float a = LowHealthPulseAlpha(0.05f, (float)i * 0.01f);
        if (a > maxA) maxA = a;
        if (a < minA) minA = a;
    }
    check(maxA > 0.5f, "puls dostigaet zametnoy yarkosti");
    check(minA < 0.2f, "puls uspokaivaetsya (ne gorit postoyanno)");
    check(maxA <= 1.0f, "ne vylazit za edinicu");

    std::puts("== 7. Byudzhet sveta ==");
    LightCost c; c.points = 4; c.spots = 1;
    check(LightPasses(c) == 6, "4 tochki + 1 spot = 6 prohodov");
    c.shadowed = 1;
    check(LightPasses(c) == 9, "ten' stoit 3 prohoda");
    check(WithinBudget(LightCost{2, 0, 0}, 0), "vstraika: 2 tochki vlezaet");
    check(!WithinBudget(LightCost{9, 0, 0}, 0), "vstraika: 9 tochek NE vlezaet");
    check(WithinBudget(LightCost{9, 0, 0}, 3), "moschnaya: 9 tochek ok");

    std::puts("== 8. Urezanie sveta po zhelezu ==");
    check(FloodLightsFor(0) == 2, "vstraika: 2 zalivki vmesto 4");
    check(FloodLightsFor(3) == 4, "moschnaya: vse 4");
    check(FloodLightsFor(0) < FloodLightsFor(3), "vstraika rezhet bolshe");
    check(KeepNeonLights(0) == false, "vstraika: neon otklyuchen");
    check(KeepNeonLights(3) == true, "moschnaya: neon est'");
    check(TransientLightBudget(0) == 1, "vstraika: 1 vremennyy ogon'");
    check(TransientLightBudget(3) == 4, "moschnaya: 4 vremennyh");

    std::puts("== 9. Itogovaya scena vlezala v byudzhet ==");
    // Arena posle urezaniya: solnce(1) + zalivka(2) + 2 neona vykl = 3 tochki
    const int tier = 0;
    LightCost after; after.points = 1 + FloodLightsFor(tier) + (KeepNeonLights(tier) ? 2 : 0);
    check(after.points == 3, "posle urezaniya 3 istochnika");
    check(WithinBudget(after, tier), "scena vlezaet v byudzhet vstraiki");
    // Do urezaniya: solnce + 4 zalivki + 2 neona + spot = 8
    LightCost before; before.points = 1 + 4 + 2; before.spots = 1;
    check(!WithinBudget(before, tier), "do urezaniya byudzhet prevyshen");
    check(LightPasses(after) < LightPasses(before), "posle urezaniya deshevle");

    std::puts("== 10. Dal'nost' melochi ==");
    check(DetailRange(100.0f, 0) == 55.0f, "vstraika: 55% dalnosti");
    check(DetailRange(100.0f, 3) == 100.0f, "moschnaya: polnaya");
    check(DetailRange(100.0f, 0) < DetailRange(100.0f, 2), "vstraika rezhet silnee");
    check(DetailRange(100.0f, 4) == 100.0f, "neizvestnaya = polnaya (ne portim)");
    check(DetailRange(0.0f, 0) == 0.0f, "nulevaya baza");

    std::printf("\n---\nProy`deno: %d, provaleno: %d\n", g_pass, g_fail);
    if (g_fail == 0) std::puts("VSE TESTY FEEL PROYDENY");
    return g_fail == 0 ? 0 : 1;
}
