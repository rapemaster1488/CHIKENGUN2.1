#pragma once
//
// MaterialTier.h — какие техники материалов и сколько мипов текстур реально
// включаются при данном пресете графики.
//
// ЗАЧЕМ ЭТОТ ФАЙЛ. Была тихая регрессия, из-за которой баня и казино выглядели
// «пластиком», а PBR-текстуры будто не работали:
//
//   * все 17 PBR-материалов (Data/Materials/Game/*.xml) объявляют три техники:
//       quality="2" → PBRMetallicRoughDiffNormalSpec  (настоящий PBR)
//       quality="1" → DiffNormalSpec                  (старый LitSolid, без PBR)
//       quality="0" → Diff                            (голая текстура)
//   * старое отображение было:  ПРЕСЕТ 0/1 → QUALITY_LOW(0),
//                              ПРЕСЕТ 2   → QUALITY_MEDIUM(1),
//                              ПРЕСЕТ 3   → QUALITY_MAX(15)
//     то есть на СРЕДНЕМ и ВЫСОКОМ (дефолт!) пресете бралась техника quality=1 —
//     НЕ PBR. PBR-материалы включались только на 4K УЛЬТРА.
//
//   * плюс Urho3D по умолчанию рубит мипы: Texture::mipsToSkip_ = {2, 1, 0},
//     то есть при качестве 0 берётся мип 256×256 из 1024×1024, при 1 — 512×512.
//     Нормаль-карты на 256 px превращаются в размытое пятно.
//
// Ниже — чистая логика отображения, чтобы это не «починили» обратно на глаз.
//
#include <cstdint>

namespace MatTier {

// Значения совпадают с Urho3D::MaterialQuality (GraphicsDefs.h).
enum Quality : std::uint32_t {
    QUALITY_LOW    = 0,   // техника quality="0"
    QUALITY_MEDIUM = 1,   // quality="1"
    QUALITY_HIGH   = 2,   // quality="2" — единственная, где есть PBR
    QUALITY_MAX    = 15   // «не резать ничего»
};

// Какую технику материала выбрать для пресета графики (0 НИЗКИЙ … 3 УЛЬТРА).
// Начиная со СТАНДАРТНОГО (1) обязана быть PBR-техника, иначе смысл 17 наборов
// PBR-текстур и всей работы по нормалям пропадает.
inline std::uint32_t QualityForPreset(int presetIdx) {
    if (presetIdx >= 2) return QUALITY_HIGH;   // 2=ВЫСОКИЙ, 3=УЛЬТРА → полный PBR
    if (presetIdx == 1) return QUALITY_MEDIUM; // СРЕДНИЙ → PBR с упрощённым светом
    return QUALITY_LOW;                        // НИЗКИЙ → базовая техника
}

// ВАЖНО ПРО МИПЫ. Texture::SetMipsToSkip() — НЕстатический метод (доступен
// только на живом объекте текстуры), поэтому глобальную политику менять извне
// нельзя. Но она и не нужна: у Urho3D встроенная таблица
//     mipsToSkip_ = { 2, 1, 0 }   для  (LOW, MEDIUM, HIGH)
// то есть само КАЧЕСТВО ТЕКСТУР И ЕСТЬ политика мипов:
//     textureQuality = LOW    → мип 256 из 1024
//     textureQuality = MEDIUM → мип 512 из 1024
//     textureQuality = HIGH   → полные 1024 + мип-цепочка
// Отсюда и «мыло»: раньше на дефолтном ВЫСОКОМ пресете стояло MEDIUM, то есть
// 512. Теперь начиная с ВЫСОКОГО требуем HIGH.
//
// Какое качество текстур (оно же политика мипов) нужно пресету.
inline std::uint32_t TextureQualityForPreset(int presetIdx) {
    if (presetIdx >= 2) return QUALITY_HIGH;   // 0 мипов: полные 1024
    return QUALITY_MEDIUM;                     // 1 мип: 512 (не 256)
}

// Сколько мипов можно пропустить для пресета.
inline int MipsToSkip(int presetIdx) {
    // Выводится из качества текстур, а не задаётся отдельно: движок смотрит
    // только в GetTextureQuality(), поэтому два источника правды разъезжались.
    return (TextureQualityForPreset(presetIdx) == QUALITY_HIGH) ? 0 : 1;
}


// Включён ли PBR (metallic/roughness), а не только diffuse+normal.
inline bool UsesPBR(int presetIdx) { return QualityForPreset(presetIdx) >= QUALITY_HIGH; }

// Включены ли зеркальные блики. Без них металл и золото выглядят матовой
// заливкой — именно так и выглядели баня и казино на НИЗКОМ пресете.
inline bool UsesSpecular(int presetIdx) { return presetIdx >= 1; }

// Пиксельный размер текстуры после отбрасывания мипов, для 1024-базового набора.
inline int EffectiveTexSize(int baseSize, int presetIdx) {
    const int skip = MipsToSkip(presetIdx);
    int s = baseSize;
    for (int i = 0; i < skip; ++i) s /= 2;
    return s < 4 ? 4 : s;
}

// Оценка «сколько детализации потеряно» в процентах от исходной.
// Нужна, чтобы UI/тесты могли поймать возврат регрессии.
inline int DetailLossPercent(int baseSize, int presetIdx) {
    const int eff = EffectiveTexSize(baseSize, presetIdx);
    if (baseSize <= 0) return 0;
    return 100 - (eff * 100) / baseSize;
}

} // namespace MatTier
