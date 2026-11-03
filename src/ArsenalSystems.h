#pragma once
// ============================================================================
// ArsenalSystems.h — Расширенный арсенал CHIKENGUN 2.5 (20 видов оружия:
// 16 огнестрельных/энергетических стволов + 4 боевых ножа) и процедурная
// кинематика анимаций (доставание/флип, 3-фазная перезарядка, инерция sway,
// тактический спринт, 3-ударное комбо ножей, тяжёлый выпад и осмотр [I]).
// Чистый C++17 без зависимостей от рендера — напрямую покрыт tests/test_core.cpp.
// ============================================================================

#include <algorithm>
#include <cmath>

namespace Arsenal {

enum WeaponId {
    W_PISTOL = 0,
    W_SMG,
    W_RIFLE,
    W_SHOTGUN,
    W_SNIPER,
    W_MINIGUN,
    W_LAUNCHER,
    W_RAILGUN,
    W_PLASMA,
    // --- Новые пушки 2.5 (слоты 10..16) ---
    W_REVOLVER,       // Магнум .500 «Куробой» (барабан на 6, мощный точный выстрел)
    W_AUTOSHOTGUN,    // Авто-дробовик AA-12 (барабан на 16, автоматический огонь дробью)
    W_CROSSBOW,       // Неоновый арбалет (снайперский болт, повышенный множитель в голову)
    W_FLAMETHROWER,   // Плазмо-огнемёт «Гриль-4K» (струя огненной плазмы ближнего/среднего боя)
    W_TESLA,          // Тесла-пушка «Зевс» (цепная молния до 3 соседних целей)
    W_DUAL_UZI,       // Парные Микро-Узи Акимбо (стрельба с двух крыльев поочерёдно)
    W_BFG,            // Аннигилятор «ЯЙЦЕ-9000» (сфера антиматерии + дуги молний + мега-взрыв)
    // --- Боевые ножи 2.5 (слоты 17..20, быстрый цикл на [K]) ---
    W_KNIFE_COMBAT,   // Тактический штык-нож M9
    W_KNIFE_KARAMBIT, // Кибер-керамбит «Коготь»
    W_KNIFE_BUTTERFLY,// Нож-бабочка «Балисонг»
    W_KNIFE_KATANA,   // Неоновая катана «Мурамаса»
    WEAPON_TOTAL_COUNT
};

constexpr int FIRST_KNIFE_ID = W_KNIFE_COMBAT;
constexpr int LAST_KNIFE_ID  = W_KNIFE_KATANA;
constexpr int KNIFE_COUNT    = LAST_KNIFE_ID - FIRST_KNIFE_ID + 1;

inline bool IsKnife(int weaponId) {
    return weaponId >= FIRST_KNIFE_ID && weaponId <= LAST_KNIFE_ID;
}

inline int NextKnifeId(int currentWeaponId) {
    if (!IsKnife(currentWeaponId)) return FIRST_KNIFE_ID;
    int next = currentWeaponId + 1;
    return (next > LAST_KNIFE_ID) ? FIRST_KNIFE_ID : next;
}

struct WeaponSpec {
    const char* codeName;
    const char* displayName;
    float damage;
    float fireRate;       // сек между атаками
    int   magSize;        // для ножей = 0 (бесконечный клинок)
    int   reserveMax;
    float spread;
    float reloadTime;
    int   pellets;
    unsigned char r, g, b;
    bool  isMelee;
    float meleeRange;     // дальность клинка (м)
    float speedBonusMul;  // множитель скорости бега с этим оружием в руках
    float bonusCritChance;// доп. шанс крита (особенно у керамбита/катаны)
};

inline const WeaponSpec& GetWeaponSpec(int id) {
    static const WeaponSpec kSpecs[WEAPON_TOTAL_COUNT] = {
        { "PISTOL",      "PISTOL M1911",         34.0f, 0.220f,  12,  96, 0.0120f, 1.40f, 1, 120,120,130, false, 0.0f, 1.02f, 0.00f },
        { "SMG",         "SMG VECTOR",           18.0f, 0.075f,  30, 180, 0.0300f, 2.00f, 1,  90, 95,105, false, 0.0f, 1.04f, 0.00f },
        { "RIFLE",       "ASSAULT RIFLE AK-4K",  27.0f, 0.098f,  30, 150, 0.0175f, 2.35f, 1,  70, 80, 70, false, 0.0f, 1.00f, 0.00f },
        { "SHOTGUN",     "PUMP SHOTGUN",         16.0f, 0.820f,   6,  48, 0.0450f, 2.70f, 8, 100, 70, 40, false, 0.0f, 0.98f, 0.00f },
        { "SNIPER",      "SNIPER AWP-4K",       115.0f, 1.180f,   5,  30, 0.0018f, 3.10f, 1,  60, 60, 66, false, 0.0f, 0.95f, 0.08f },
        { "MINIGUN",     "VULCAN MINIGUN",       14.5f, 0.045f, 120, 400, 0.0440f, 4.10f, 1, 110, 85, 60, false, 0.0f, 0.88f, 0.00f },
        { "LAUNCHER",    "ROCKET LAUNCHER",     155.0f, 1.480f,   4,  20, 0.0200f, 3.50f, 1,  70, 95, 70, false, 0.0f, 0.92f, 0.00f },
        { "RAILGUN",     "QUANTUM RAILGUN",     185.0f, 1.300f,   4,  24, 0.0008f, 2.85f, 1,  55,195,255, false, 0.0f, 0.96f, 0.10f },
        { "PLASMA",      "PLASMA CASTER",        56.0f, 0.190f,  20, 120, 0.0130f, 2.10f, 1,  45,240,155, false, 0.0f, 1.00f, 0.02f },
        // Новые пушки 10..16
        { "REVOLVER",    "MAGNUM .500",          92.0f, 0.440f,   6,  54, 0.0055f, 2.25f, 1, 215,180, 75, false, 0.0f, 1.03f, 0.12f },
        { "AUTOSHOTGUN", "AA-12 DRUM SHOTGUN",   13.5f, 0.240f,  16,  96, 0.0560f, 2.95f, 6, 138, 62, 48, false, 0.0f, 0.97f, 0.00f },
        { "CROSSBOW",    "NEON CROSSBOW",       148.0f, 0.950f,   5,  35, 0.0015f, 2.30f, 1, 165, 68,245, false, 0.0f, 1.02f, 0.18f },
        { "FLAMER",      "GRILL-4K FLAMER",      22.0f, 0.065f,  75, 300, 0.0650f, 2.80f, 1, 255,105, 28, false, 0.0f, 0.96f, 0.00f },
        { "TESLA",       "TESLA ARC CANNON",     64.0f, 0.340f,  18, 108, 0.0080f, 2.40f, 1,  70,165,255, false, 0.0f, 0.99f, 0.06f },
        { "DUAL_UZI",    "AKIMBO MICRO-UZI",     15.5f, 0.048f,  60, 300, 0.0380f, 2.45f, 1, 235, 65,165, false, 0.0f, 1.08f, 0.03f },
        { "BFG9000",     "EGG-9000 ANNIHILATOR",320.0f, 2.100f,   3,  12, 0.0040f, 3.80f, 1, 120,255, 65, false, 0.0f, 0.86f, 0.15f },
        // Боевые ножи 17..20
        { "BAYONET",     "ШТЫК-НОЖ M9",          78.0f, 0.320f,  99,  99, 0.0000f, 0.50f, 1, 145,155,160, true,  2.95f, 1.18f, 0.15f },
        { "KARAMBIT",    "КЕРАМБИТ КОГОТЬ",      68.0f, 0.220f,  99,  99, 0.0000f, 0.50f, 1, 240, 42, 92, true,  2.75f, 1.25f, 0.35f },
        { "BUTTERFLY",   "НОЖ-БАБОЧКА",          74.0f, 0.255f,  99,  99, 0.0000f, 0.50f, 1,  55,235,255, true,  2.85f, 1.22f, 0.28f },
        { "KATANA",      "КАТАНА МУРАМАСА",     112.0f, 0.380f,  99,  99, 0.0000f, 0.50f, 1, 255,195, 45, true,  3.85f, 1.16f, 0.25f },
    };
    int idx = std::max(0, std::min(WEAPON_TOTAL_COUNT - 1, id));
    return kSpecs[idx];
}

// ============================================================================
// ПРОЦЕДУРНЫЕ КРИВЫЕ АНИМАЦИЙ ОРУЖИЯ И НОЖЕЙ
// Все углы — в градусах, смещения — в метрах локального пространства камеры.
// ============================================================================
struct AnimPose {
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
    float magDropY = 0.0f;      // смещение магазина вниз при перезарядке
    float slideBackZ = 0.0f;    // откат затвора назад
    float spinDeg = 0.0f;       // вращение стволов/барабана/кольца керамбита
    float balisongOpenDeg = 0.0f; // раскрытие половинок рукояти ножа-бабочки
};

// 1. Анимация доставания / экипировки (progress: 0 = только достали, 1 = готово)
inline AnimPose ComputeEquipPose(int weaponId, float progress01) {
    float t = std::clamp(progress01, 0.0f, 1.0f);
    float inv = 1.0f - t;
    float ease = inv * inv; // плавный выход к нулю
    AnimPose p{};
    p.posY = -0.22f * ease;
    p.posZ = -0.08f * ease;
    p.pitch = 28.0f * ease;
    p.roll  = -22.0f * ease;

    if (weaponId == W_REVOLVER) {
        // Прокрутка револьвера на скобе при доставании
        p.pitch = -360.0f * inv;
    } else if (weaponId == W_KNIFE_KARAMBIT) {
        // Двойной проворот керамбита на указательном пальце
        p.spinDeg = 720.0f * inv;
        p.roll = -45.0f * std::sin(t * 3.14159265f);
    } else if (weaponId == W_KNIFE_BUTTERFLY) {
        // Раскрытие балисонга с перебросом рукоятей
        p.balisongOpenDeg = 148.0f * std::sin(t * 3.14159265f);
        p.roll = 35.0f * std::sin(t * 6.2831853f) * inv;
    } else if (weaponId == W_KNIFE_KATANA) {
        // Выхват катаны из ножен слева-снизу («Иайдо»)
        p.posX = -0.28f * ease;
        p.yaw  = -52.0f * ease;
        p.roll = -38.0f * ease;
    } else if (t > 0.62f && !IsKnife(weaponId)) {
        // Финальный рывок затвора на 62..100% доставания
        float rack = std::sin(((t - 0.62f) / 0.38f) * 3.14159265f);
        p.slideBackZ = -0.055f * rack;
    }
    return p;
}

// 2. Трёхфазная тактическая перезарядка (progress: 0 = старт, 1 = завершена)
inline AnimPose ComputeReloadPose(int weaponId, float progress01) {
    float t = std::clamp(progress01, 0.0f, 1.0f);
    AnimPose p{};
    if (IsKnife(weaponId)) return p;

    float env = std::sin(t * 3.14159265f);
    p.posX  = -0.06f * env;
    p.posY  = -0.07f * env;
    p.pitch =  16.0f * env;
    p.roll  =  28.0f * env;

    if (t < 0.36f) {
        // Фаза 1: сброс пустого магазина вниз
        float k = t / 0.36f;
        p.magDropY = -0.24f * (k * k);
    } else if (t < 0.74f) {
        // Фаза 2: досылание полного магазина снизу с хлопком на 0.74
        float k = (t - 0.36f) / 0.38f;
        p.magDropY = -0.24f * (1.0f - k);
        if (k > 0.82f) p.posY += 0.035f * std::sin(((k - 0.82f) / 0.18f) * 3.14159265f);
    } else {
        // Фаза 3: передёргивание затвора / проворот барабана
        float k = (t - 0.74f) / 0.26f;
        float rack = std::sin(k * 3.14159265f);
        p.slideBackZ = -0.07f * rack;
        p.pitch -= 8.0f * rack;
        p.roll = 28.0f * (1.0f - k);
        if (weaponId == W_REVOLVER) p.spinDeg = 360.0f * k;
    }
    return p;
}

// 3. Анимация удара ножом: 3-шаговое комбо (step 0,1,2) или тяжёлый выпад (heavy=true)
inline AnimPose ComputeKnifeSlashPose(int weaponId, int comboStep, bool heavy, float progress01) {
    float t = std::clamp(progress01, 0.0f, 1.0f);
    float swing = std::sin(t * 3.14159265f);
    AnimPose p{};

    if (heavy) {
        // ПКМ: мощный пробивной укол вперёд (или горизонтальный Иайдо-слэш у Катаны)
        if (weaponId == W_KNIFE_KATANA) {
            float arc = std::cos(t * 3.14159265f); // +1 -> -1
            p.posX = -0.22f * arc;
            p.posZ =  0.26f * swing;
            p.yaw  = -68.0f * arc;
            p.roll = -72.0f * swing;
        } else {
            p.posX  = -0.08f * swing;
            p.posY  =  0.04f * swing;
            p.posZ  =  0.34f * swing;
            p.pitch = -18.0f * swing;
            p.roll  =  95.0f * swing;
        }
        return p;
    }

    int st = ((comboStep % 3) + 3) % 3;
    float arc = std::cos(t * 3.14159265f); // от +1 (замах) до -1 (конец реза)
    if (st == 0) {
        // Рез 1: диагональ справа-сверху налево-вниз
        p.posX  =  0.18f * arc;
        p.posY  =  0.08f * arc;
        p.posZ  =  0.19f * swing;
        p.yaw   = -52.0f * arc;
        p.pitch =  22.0f * swing;
        p.roll  = -38.0f * swing;
    } else if (st == 1) {
        // Рез 2: обратная диагональ слева-снизу направо-вверх
        p.posX  = -0.18f * arc;
        p.posY  = -0.06f * arc;
        p.posZ  =  0.20f * swing;
        p.yaw   =  50.0f * arc;
        p.pitch = -18.0f * swing;
        p.roll  =  42.0f * swing;
    } else {
        // Рез 3: круговой вертушечный финишер
        p.posZ    = 0.25f * swing;
        p.yaw     = -62.0f * arc;
        p.spinDeg = 360.0f * t;
        p.roll    = -55.0f * swing;
    }
    return p;
}

// 4. Многофазный осмотр [I] с уникальными трюками для ножей и револьвера
inline AnimPose ComputeInspectPose(int weaponId, float progress01) {
    float t = std::clamp(progress01, 0.0f, 1.0f);
    float env = std::sin(t * 3.14159265f);
    AnimPose p{};
    p.posX = -0.11f * env;
    p.posY =  0.055f * env;
    p.posZ =  0.04f * env;

    if (weaponId == W_KNIFE_BUTTERFLY) {
        p.balisongOpenDeg = 155.0f * std::fabs(std::sin(t * 9.424778f)) * env;
        p.spinDeg = 360.0f * t;
        p.roll = 26.0f * std::sin(t * 6.2831853f) * env;
    } else if (weaponId == W_KNIFE_KARAMBIT) {
        p.spinDeg = 1080.0f * t; // тройная вертушка на кольце!
        p.yaw = -24.0f * std::sin(t * 6.2831853f) * env;
    } else if (weaponId == W_REVOLVER) {
        p.spinDeg = 720.0f * t;
        p.roll = 30.0f * env;
        p.yaw = -32.0f * std::sin(t * 6.2831853f) * env;
    } else {
        p.pitch = -16.0f * env + 10.0f * std::sin(t * 9.424778f) * env;
        p.yaw   = -42.0f * std::sin(t * 6.2831853f) * env;
        p.roll  =  28.0f * std::cos(t * 3.14159265f) * env;
        if (t > 0.65f && !IsKnife(weaponId))
            p.slideBackZ = -0.045f * std::sin(((t - 0.65f) / 0.35f) * 3.14159265f);
    }
    return p;
}

} // namespace Arsenal
