// Testy detekcii zheleza i podbora preseta (bez Urho3D).
#include "../src/PerfTier.h"
#include <cstdio>
#include <string>

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

int main() {
    using namespace Perf;

    std::puts("== 1. Opredelenie vstraivaniya ==");
    // Real'nye stroki GL_RENDERER s rasprostranennyh iGPU
    check(DetectTier("Intel(R) UHD Graphics 620") == TIER_INTEGRATED, "Intel UHD 620");
    check(DetectTier("Intel(R) HD Graphics 4000") == TIER_INTEGRATED, "Intel HD 4000");
    check(DetectTier("Intel(R) Iris(R) Xe Graphics") == TIER_INTEGRATED, "Intel Iris Xe");
    check(DetectTier("AMD Radeon(TM) Vega 8 Graphics") == TIER_INTEGRATED, "AMD Vega 8");
    check(DetectTier("Mesa Intel(R) UHD Graphics (CML GT2)") == TIER_INTEGRATED, "Mesa Intel UHD");
    check(DetectTier("llvmpipe (LLVM 15.0.7, 256 bits)") == TIER_INTEGRATED, "llvmpipe");
    check(DetectTier("Mali-G78 MP14") == TIER_INTEGRATED, "Mali-G78");
    check(DetectTier("Adreno (TM) 650") == TIER_INTEGRATED, "Adreno 650");

    std::puts("== 2. Diskretki ne pusataya s vstraivaniem ==");
    check(DetectTier("NVIDIA GeForce RTX 3060/PCIe/SSE2") == TIER_HIGH, "RTX 3060 -> high");
    check(DetectTier("NVIDIA GeForce RTX 4090") == TIER_HIGH, "RTX 4090 -> high");
    check(DetectTier("NVIDIA GeForce GTX 1660 SUPER") == TIER_HIGH, "GTX 1660 -> high");
    check(DetectTier("NVIDIA GeForce GTX 1050 Ti") == TIER_MID, "GTX 1050 -> mid");
    check(DetectTier("AMD Radeon RX 6600 XT") == TIER_HIGH, "RX 6600 -> high");
    // Intel+NV Optimus: stroka soderzhit oba brenda, no diskretka vazhnee
    check(DetectTier("Intel(R) UHD Graphics 630 + NVIDIA GeForce GTX 1650") != TIER_INTEGRATED,
          "Optimus ne schitaetsya vstraivaniem");

    std::puts("== 3. Neizvestnoe zhelezo ne ponizhaet kartinku ==");
    check(DetectTier("") == TIER_UNKNOWN, "pustaya stroka -> unknown");
    check(DetectTier("Some Renderer 3000") == TIER_UNKNOWN, "neponyatnoe -> unknown");
    check(PresetForTier(TIER_UNKNOWN).qualityIdx == 2, "unknown -> preset MID");

    std::puts("== 4. Presety ==");
    const Preset integ = PresetForTier(TIER_INTEGRATED);
    check(integ.msaaIdx == 0, "vstraika: MSAA off");
    check(integ.ssao == false, "vstraika: SSAO off");
    check(integ.shadowIdx == 0, "vstraika: teni 1024");
    check(integ.castShadows == false, "vstraika: dynamic shadows off");
    check(integ.resIdx == 4, "vstraika: razreshenie 1600x900 (glavnyy rychag)");
    check(integ.hdr == true, "vstraika: HDR ostavlen (ona deshevaya)");
    check(integ.glowIdx >= 1, "vstraika: bloom ostavlen");

    const Preset hi = PresetForTier(TIER_HIGH);
    check(hi.qualityIdx == 3 && hi.ssao && hi.msaaIdx == 2, "high: ultra + SSAO + MSAA");
    check(hi.resIdx == 0, "high: razreshenie avto/monitor");

    std::puts("== 5. Cena preseta ==");
    const float cInteg = CostEstimate(integ);
    const float cWeak  = CostEstimate(PresetForTier(TIER_WEAK));
    const float cMid   = CostEstimate(PresetForTier(TIER_MID));
    const float cHigh  = CostEstimate(hi);
    check(cInteg < cWeak, "vstraika deshevle slaboy");
    check(cWeak < cMid, "slabaya deshevle sredney");
    check(cMid < cHigh, "srednyaya deshevle moschnoy");
    check(cHigh / cInteg > 4.0f, "ultra dorozhe vstraiki bolee chem v 4 raza");

    std::puts("== 6. Render-scale ==");
    check(ScaledSize(1920, 100) == 1920, "100% ne menyayet");
    check(ScaledSize(1920, 70) == 1344, "1920 * 70% = 1344");
    check(ScaledSize(3840, 70) == 2688, "4K * 70% = 2688");
    check(ScaledSize(100, 10) == 40, "nizhe 40% ne opuskaem (100px * 40%)");
    check(ScaledSize(40, 100) == 40 && 40 > 0, "krupnyy klamp ne tratit");
    check(ScaledSize(0, 70) == 0, "nulevoy razmer");
    check(ScaledSize(1920, 200) == 1920, ">100% ne uvelichivaet bez rezhima");

    std::puts("== 7. Pamyat' tekstur ==");
    const float m1k = TexMemMB(17, 1024, false);
    const float m4k = TexMemMB(17, 4096, false);
    check(m4k / m1k > 15.0f, "4K v ~16 raz tyazhelee 1K");
    check(m1k > 40.0f && m1k < 60.0f, "17 kart 1024 = ~51 MB");
    check(TexMemMB(1, 1024, true) > TexMemMB(1, 1024, false), "mipy dobavlyayut pamyat'");

    std::puts("== 8. Imena dlya UI ==");
    check(std::string(TierName(TIER_INTEGRATED)) == "ВСТРОЕННАЯ", "imya vstraiki");
    check(TierName(TIER_HIGH) != nullptr, "imya high");
    check(TierName(TIER_COUNT) != nullptr, "imya dlya neizvestnogo indeksa");

    std::printf("\n---\nProy`deno: %d, provaleno: %d\n", g_pass, g_fail);
    if (g_fail == 0) std::puts("VSE TESTY PERF TIER PROYDENY");
    return g_fail == 0 ? 0 : 1;
}
