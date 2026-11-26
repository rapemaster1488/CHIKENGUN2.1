#pragma once
//
// PerfTier.h — определение класса видеокарты и подбор пресета под неё.
//
// Зачем: раньше единственным механизмом был адаптивный дроп уже В ПРОЦЕССЕ игры
// (UpdateAdaptiveQuality: ждёт 2 секунды, потом шагает вниз). На встроенной
// графике игрок сначала получал 15 FPS в меню, и только потом что-то менялось.
// Здесь мы решаем это ДО первого кадра: читаем строку рендерера OpenGL, узнаём
// iGPU и сразу выставляем пресет.
//
// Модуль намеренно без зависимостей от Urho3D — чтобы его можно было покрыть
// обычными юнит-тестами (tests/test_perf_tier.cpp).
//
#include <cstring>
#include <string>

namespace Perf {

// ── Класс железа ────────────────────────────────────────────────────────────
enum Tier {
    TIER_INTEGRATED = 0,  // Intel UHD/HD/Iris, AMD Vega APU, ARM Mali/Adreno — встройка
    TIER_WEAK       = 1,  // старые/бюджетные дискретки, Intel Arc A3xx
    TIER_MID        = 2,  // GTX 1050/1650, RX 5500, Arc A5xx
    TIER_HIGH       = 3,  // RTX 2060+, RX 5600+, Arc A7xx
    TIER_UNKNOWN    = 4
};
constexpr int TIER_COUNT = 5;

inline const char* TierName(int t) {
    switch (t) {
    case TIER_INTEGRATED: return "ВСТРОЕННАЯ";
    case TIER_WEAK:       return "СЛАБАЯ";
    case TIER_MID:        return "СРЕДНЯЯ";
    case TIER_HIGH:       return "МОЩНАЯ";
    default:              return "НЕИЗВЕСТНА";
    }
}

// ── Пресет, который вытекает из класса железа ───────────────────────────────
// Индексы совпадают с gfxQualityIdx_ в MyApp.cpp: 0 НИЗКИЙ … 3 4K УЛЬТРА.
struct Preset {
    int   qualityIdx    = 2;
    int   msaaIdx       = 1;      // 0=off 1=2x 2=4x 3=8x
    int   shadowIdx     = 1;      // 0=1024 1=2048 2=4096
    int   glowIdx       = 1;      // 0=off 1=мягкий 2=кино
    int   anisoIdx      = 2;      // 0=bilinear 1=trilinear 2=aniso16
    // Индекс пресета разрешения из MyApp: 0=АВТО(монитор) 1=4K 2=2K 3=FullHD 4=1600x900.
    // Рендер-скейла в Urho3D 1.9 нет (проверено по исходникам движка), поэтому
    // на встройке честно опускаем разрешение окна — это тот же выигрыш по пикселям.
    int   resIdx        = 0;
    bool  ssao          = false;
    bool  hdr           = true;
    bool  fxaa          = true;
    bool  sharpen       = true;
    bool  castShadows   = true;
};

// Ключевые слова, по которым узнаём встройку. Порядок важен: сначала
// проверяем намеренно «интегрированные» бренды, потом общие.
inline bool LooksIntegrated(const std::string& r) {
    static const char* kJunk[] = {
        "intel", "hd graphics", "uhd graphics", "iris", "hd3000", "hd4000",
        "mesa intel", "llvmpipe", "swiftshader", "software", "basic render",
        "mali", "adreno", "videocore", "powervr", "tegra",
        "radeon vega 8", "radeon vega 11", "radeon graphics", "vega 8",
        "vega 11", "rgx", "apple m1", "apple m2",
        nullptr
    };
    for (int i = 0; kJunk[i]; ++i)
        if (r.find(kJunk[i]) != std::string::npos) return true;
    return false;
}

// Явный признак дискретки — важнее слов «intel»/«amd» в строке драйвера,
// потому что у Intel+NV Optimus строка часто содержит оба вендора.
inline bool LooksDiscrete(const std::string& r) {
    static const char* kGood[] = {
        "rtx 40", "rtx 30", "rtx 20", "gtx 16", "gtx 10",
        "radeon rx 7", "radeon rx 6", "radeon rx 5", "rx 7800", "rx 6800", "rx 5800",
        "geforce", "quadro", "radeon pro", "arc a7", "arc a5",
        nullptr
    };
    for (int i = 0; kGood[i]; ++i)
        if (r.find(kGood[i]) != std::string::npos) return true;
    return false;
}

// Главная функция: строка GL_RENDERER (в нижнем регистре или нет — не важно,
// приводим сами) → класс железа.
inline int DetectTier(const std::string& rawRenderer) {
    std::string r;
    r.reserve(rawRenderer.size());
    for (char c : rawRenderer) r.push_back((char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c));

    if (r.empty()) return TIER_UNKNOWN;

    const bool discrete = LooksDiscrete(r);
    const bool integrated = LooksIntegrated(r);

    // Дискретка важнее: у ноутбуков с Optimus строка рендерера часто содержит
    // ОБА бренда («Intel UHD 630 + NVIDIA GTX 1650»), и если проверять встройку
    // первой, такой ноутбук ошибочно получит пресет встройки при живой RTX.
    if (discrete) {
        // Грубое деление: *50/*60 и выше — «мощная», *30/*40/*50 — «средняя».
        static const char* kHigh[] = {
            "rtx 4090","rtx 4080","rtx 4070","rtx 4060","rtx 3090","rtx 3080","rtx 3070","rtx 3060",
            "rtx 2080","rtx 2070","rtx 2060","gtx 1660","gtx 1080","gtx 1070","gtx 1060",
            "rx 7900","rx 7800","rx 7700","rx 6900","rx 6800","rx 6700","rx 6600","rx 5800","rx 5700",
            "arc a770","arc a750", nullptr };
        for (int i = 0; kHigh[i]; ++i)
            if (r.find(kHigh[i]) != std::string::npos) return TIER_HIGH;
        return TIER_MID;
    }

    if (integrated) return TIER_INTEGRATED;

    // Не распознали — считаем серединой, чтобы не понижать зря мощную карту.
    return TIER_UNKNOWN;
}

// Пресет под класс. Встройка получает НЕ «всё выключено», иначе пропадает вся
// картинка: LOD текстур и материал НИЗКИЙ, но HDR+ACES и FXAA остаются,
// потому что именно они дают «дорогой» вид, а стоят на iGPU копейки.
// Разрешение — главный рычаг: на встройке 1600x900, а не полный монитор.
inline Preset PresetForTier(int tier) {
    Preset p;
    switch (tier) {
    case TIER_INTEGRATED:
        p.qualityIdx = 0; p.msaaIdx = 0; p.shadowIdx = 0; p.glowIdx = 1;
        p.anisoIdx = 0; p.resIdx = 4; p.ssao = false; p.hdr = true;
        p.fxaa = true;  p.sharpen = true; p.castShadows = false;
        break;
    case TIER_WEAK:
        p.qualityIdx = 1; p.msaaIdx = 0; p.shadowIdx = 0; p.glowIdx = 1;
        p.anisoIdx = 1; p.resIdx = 3; p.ssao = false; p.hdr = true;
        p.fxaa = true;  p.sharpen = true; p.castShadows = false;
        break;
    case TIER_MID:
        p.qualityIdx = 2; p.msaaIdx = 1; p.shadowIdx = 1; p.glowIdx = 1;
        p.anisoIdx = 2; p.resIdx = 0; p.ssao = false; p.hdr = true;
        p.fxaa = true;  p.sharpen = true; p.castShadows = true;
        break;
    case TIER_HIGH:
        p.qualityIdx = 3; p.msaaIdx = 2; p.shadowIdx = 2; p.glowIdx = 2;
        p.anisoIdx = 2; p.resIdx = 0; p.ssao = true; p.hdr = true;
        p.fxaa = true;  p.sharpen = true; p.castShadows = true;
        break;
    default:  // неизвестна → ведём себя как СРЕДНЯЯ, не рискуя испортить картинку
        return PresetForTier(TIER_MID);
    }
    return p;
}

// Разрешение, в котором реально рисуем, при заданном размере окна.
inline int ScaledSize(int windowSize, int percent) {
    if (windowSize <= 0) return 0;
    if (percent >= 100) return windowSize;
    if (percent < 40) percent = 40;
    int v = (windowSize * percent) / 100;
    return v < 16 ? 16 : v;
}

// Сколько байт занимает набор текстур при заданном размере, МБ (RGB, без мипов).
inline float TexMemMB(int count, int size, bool withMips) {
    double one = (double)size * size * 3.0;
    if (withMips) one *= 1.3334;   // мип-цепочка добавляет ~33%
    return (float)((one * (double)count) / (1024.0 * 1024.0));
}

// «Сколько это стоит» в относительных кадрах: грубая модель, чтобы UI мог
// показать игроку предупреждение до применения, а тесты — поймать регрессию,
// когда кто-то снова включит всё сразу.
inline float CostEstimate(const Preset& p) {
    // Доля пикселей относительно «полного монитора». resIdx: 0=авто/100%,
    // 1=4K(100%), 2=2K(~44%), 3=FullHD(~25%), 4=900p(~17%) — считаем по 1920x1080
    // как за 100%, чтобы числа были стабильными и не зависели от монитора.
    static const float kPixelShare[5] = { 1.0f, 1.0f, 0.44f, 0.25f, 0.17f };
    float c = 10.0f;                                   // базовый forward-проход
    if (p.ssao)        c += 9.0f;                      // самый дорогой пост-эффект
    if (p.glowIdx > 0) c += (p.glowIdx >= 2) ? 4.0f : 2.0f;
    if (p.shadowIdx > 0) c += (p.shadowIdx >= 2) ? 7.0f : 4.0f;
    if (p.msaaIdx > 0) c += 2.0f + 1.6f * (float)p.msaaIdx;  // 2x/4x/8x
    if (p.fxaa)        c += 1.2f;
    if (p.sharpen)     c += 0.6f;
    // Пиксельная работа: разрешение влияет квадратично — это главный рычаг.
    const float px = kPixelShare[(p.resIdx >= 0 && p.resIdx <= 4) ? p.resIdx : 0];
    return c * px;
}

} // namespace Perf
