// Testy otobrazheniya materialov i mipov tekstur.
// Derzhat regressiyu: na Srednem/Vysokom presete obyazan byt' PBR, a tekstury
// ne dolzhny mylit'sya do 256x256.
#include "../src/MaterialTier.h"
#include <cstdio>

using namespace MatTier;

static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

int main() {
    std::puts("== 1. PBR vklyuchen tam, gde dolzhen ==");
    check(UsesPBR(0) == false, "NIZKIY: bez PBR (osoznanno)");
    check(UsesPBR(2) == true, "VYSOKIY (defolt): PBR OBYAZAN");
    check(UsesPBR(3) == true, "ULTRA: PBR");
    check(QualityForPreset(2) == QUALITY_HIGH, "preset 2 -> quality=2 (PBR-tehnika)");
    check(QualityForPreset(3) == QUALITY_HIGH, "preset 3 -> quality=2");
    check(QualityForPreset(1) != QUALITY_LOW, "SREDNIY ne padaet do goloy tekstury");
    // Regressiya: ran'she preset 3 dayot QUALITY_MAX, a 2 -> QUALITY_MEDIUM (bez PBR).
    check(QualityForPreset(2) == QualityForPreset(3),
          "VYSOKIY i ULTRA dayut odnu tehniku (oba PBR)");

    std::puts("== 1b. Kachestvo tekstur = politika mipov ==");
    check(TextureQualityForPreset(2) == QUALITY_HIGH, "VYSOKIY: tekstury HIGH (nul' mipov)");
    check(TextureQualityForPreset(3) == QUALITY_HIGH, "ULTRA: tekstury HIGH");
    check(TextureQualityForPreset(1) == QUALITY_MEDIUM, "SREDNIY: MEDIUM (512, ne 256)");
    check(TextureQualityForPreset(0) == QUALITY_MEDIUM, "NIZKIY: tozhe MEDIUM, ne 256");

    std::puts("== 2. Mipy ne mylyat kartinku ==");
    check(MipsToSkip(2) == 0, "VYSOKIY: polnye 1024");
    check(MipsToSkip(3) == 0, "ULTRA: polnye 1024");
    check(MipsToSkip(1) <= 1, "SREDNIY: ne huzhe 512 (bylo 256 — mylo)");
    check(MipsToSkip(0) <= 1, "NIZKIY: 512, a ne 256 (bylo 256)");
    check(MipsToSkip(0) >= MipsToSkip(2), "nizkiy rezhet ne men'she vysokogo");

    std::puts("== 3. Effektivnyy razmer tekstury ==");
    check(EffectiveTexSize(1024, 2) == 1024, "1024 na VYSOKOM = 1024");
    check(EffectiveTexSize(1024, 3) == 1024, "1024 na ULTRA = 1024");
    check(EffectiveTexSize(1024, 0) == 512, "1024 na NIZKOM = 512");
    check(EffectiveTexSize(1024, 1) == 512, "1024 na SREDNEM = 512");
    check(EffectiveTexSize(16, 3) == 16, "melkaya tekstura ne portitsya");
    check(EffectiveTexSize(1024, 0) > 256, "NIZKIY bolshe chem bylo (256)");

    std::puts("== 4. Poterya detalizacii ==");
    check(DetailLossPercent(1024, 2) == 0, "VYSOKIY: 0% poter'");
    check(DetailLossPercent(1024, 3) == 0, "ULTRA: 0% poter'");
    check(DetailLossPercent(1024, 0) == 50, "NIZKIY: 50% poter' (bylo 75%)");
    check(DetailLossPercent(1024, 0) < 75, "NE hudzhe prezhnih 256px");
    check(DetailLossPercent(0, 0) == 0, "nulevaya baza ne delit na nol");

    std::puts("== 5. Zerkal'nye bliki ==");
    check(UsesSpecular(1) == true, "SREDNIY: bliki est'");
    check(UsesSpecular(2) == true, "VYSOKIY: bliki est'");
    check(UsesSpecular(0) == false, "NIZKIY: bez blikov (ekonomiya)");

    std::puts("== 6. Soglasovannost' po vsem presedam ==");
    for (int p = 0; p <= 3; ++p) {
        // Material quality nikogda ne dolzhen byt' QUALITY_MAX:
        // tehniki quality=15 v XML net, material stal by pustym.
        if (QualityForPreset(p) == QUALITY_MAX) {
            check(false, "preset dayot QUALITY_MAX — net takoy tehniki!");
        }
        if (MipsToSkip(p) < 0) check(false, "otricatel'noe chislo mipov");
    }
    check(true, "vse presety proshli");

    std::printf("\n---\nProy`deno: %d, provaleno: %d\n", g_pass, g_fail);
    if (g_fail == 0) std::puts("VSE TESTY MATERIAL TIER PROYDENY");
    return g_fail == 0 ? 0 : 1;
}
