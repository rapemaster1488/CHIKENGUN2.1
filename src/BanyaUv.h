// BanyaUv.h — sxema UV dlya geometrii bani (srub, valgonka, polok).
//
// Zachem otdel'nyy header. Geometriya bani sobiraetsya v CustomGeometry
// vnutri MyApp.cpp, i ee nel'zya proverit' testom. A oshibka v UV-tayling
// kriticheskaya: odin tayl na vse 6 metrov brevna = rastyanutaya polosa,
// kotoraya i chitalas' kak "parasha". Zdes' lezhit chistaya arifmetika
// razmera tayla, a test_banya_uv.cpp derzhit ee ot regressiy.
#pragma once

namespace BanyaUv {

// Skol'ko metrov brevna prihoditsya na odin tayl tekstury vdol' stvola.
// 0.5 m — diametr 46-sm brevna umeshchaetsya v tayl pochti tselikom,
// volokna chitayutsya pri lyubom razreshenii ot 1024 do 4096.
inline float LogTileMeters() { return 0.5f; }

// Skol'ko metrov doski na tayl vdol' volokna (valgonka, polok).
inline float PlankTileMeters() { return 1.0f; }

// Chislo povtorov tekstury vdol' elementa. Dolzhno byt' > 1 dlya lyubogo
// elementa dlinnee odnogo metra, inache risunok mylitsya rastyazheniem.
inline float TilesAlong(float lengthMeters, float tileMeters) {
    if (tileMeters <= 0.0f || lengthMeters <= 0.0f) return 0.0f;
    return lengthMeters / tileMeters;
}

// Minimal'no dopustimoe chislo povtorov dlya dlinnyh elementov srubа.
// Brevno 6 m pri tayle 0.5 m = 12 povtorov; doska polka 1.3 m = 1.3.
inline bool HasEnoughDetail(float lengthMeters, float tileMeters, float minTiles) {
    return TilesAlong(lengthMeters, tileMeters) >= minTiles;
}

// UV-vdol' stvola dlya tochki na rasstoyanii d along ot nachala brevna.
inline float LogU(float d) { return d / LogTileMeters(); }

// UV-poperek stvola: 0..1 na polnyy oborot.
// Polnyy oborot vozvraschaet rovno 1.0, a ne 0.0: geometriya srubа opiraet
// posledniy stolbets setki na V=1, i esli svernut' ego v 0, shov tekstury
// nanizyvаetsya na sebya i normal' na krayu razvorachivaetsya.
inline float LogV(float angleRad) {
    const float pi2 = 6.28318530717958647692f;
    float f = angleRad / pi2;
    while (f < 0.0f) f += 1.0f;
    while (f > 1.0f) f -= 1.0f;
    // Polnyy oborot (2pi) chislenno daet 0.9999999989 — okruglyaem do 1.0,
    // chtoby posledniy stolbets setki ne "provorachivalsya" nazad v 0.
    if (f > 0.9999999f) f = 1.0f;
    return f;
}

// Kadr srubа: radius i shag venсa, v kotorom brevno est' brevno.
inline float LogRadius() { return 0.23f; }
inline float LogStep() { return LogRadius() * 1.94f; }

} // namespace BanyaUv
