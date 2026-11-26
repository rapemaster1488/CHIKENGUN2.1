#pragma once
// ============================================================================
// CyberSystems.h — Тактические системы CHIKENGUN 2.3:
// мутаторы волн, орбитальный удар «ЯЙЦЕГЕДДОН», экономика перков и достижения.
// Не зависит от заголовков рендера — тестируется напрямую в tests/test_core.cpp.
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Cyber {

// --- 1. Мутаторы волн арены ---
enum WaveMutator : int {
    MUT_STANDARD = 0,   // Стандартный бой
    MUT_LOW_GRAV = 1,   // Лунная гравитация: гравитация x0.52, высокие прыжки
    MUT_TURBO    = 2,   // Турбо-стая: скорость кур x1.28, очки и кредиты x1.35
    MUT_GOLDRUSH = 3,   // Золотая лихорадка: кредиты x2.0 за каждое убийство
    MUT_VOLATILE = 4,   // Пиро-охота: каждая поверженная курица взрывается микро-фугасом
    MUT_COUNT    = 5
};

struct MutatorInfo {
    const char* name;
    const char* desc;
    float gravityMul;
    float speedMul;
    float creditMul;
    bool  volatileDeaths;
};

inline const MutatorInfo& GetMutatorInfo(int mut) {
    static const MutatorInfo kTable[MUT_COUNT] = {
        { "СТАНДАРТНЫЙ ПРОТОКОЛ", "Штатная гравитация и стандартные награды",          1.00f, 1.00f, 1.00f, false },
        { "ЛУННАЯ ГРАВИТАЦИЯ",    "Гравитация снижена вдвое — гигантские прыжки!",     0.52f, 1.00f, 1.15f, false },
        { "ТУРБО-СТАЯ",           "Курицы на форсаже (+28% скорости, +35% кредитов)",  1.00f, 1.28f, 1.35f, false },
        { "ЗОЛОТАЯ ЛИХОРАДКА",    "Двойные кибер-кредиты (x2¤) за каждого врага!",     1.00f, 1.05f, 2.00f, false },
        { "ПИРО-ШТОРМ",           "Все куры детонируют осколочным взрывом при гибели!",1.00f, 1.08f, 1.25f, true  },
    };
    int idx = (mut >= 0 && mut < MUT_COUNT) ? mut : 0;
    return kTable[idx];
}

// Детерминированно-разнообразный выбор мутатора для волны (волна 1 и босс-волны — без хаоса или с бонусом)
inline int PickWaveMutator(int waveNum, int randomSeed) {
    if (waveNum <= 1) return MUT_STANDARD;
    if (waveNum % 5 == 0) return MUT_GOLDRUSH; // на волне с Мега-Индюком всегда x2 кредитов!
    int pool[4] = { MUT_LOW_GRAV, MUT_TURBO, MUT_GOLDRUSH, MUT_VOLATILE };
    int idx = std::abs(waveNum * 7 + randomSeed) % 5;
    if (idx == 4) return MUT_STANDARD;
    return pool[idx];
}

// --- 2. Заряд орбитального ионного удара «ЯЙЦЕГЕДДОН» [X] ---
constexpr float ORBITAL_RADIUS        = 16.5f;
constexpr float ORBITAL_BASE_DAMAGE   = 380.0f;
constexpr int   ORBITAL_EMERGENCY_CREDITS = 150;

inline float AddOrbitalCharge(float currentPct, bool isBoss, bool isHeadshot) {
    float gain = isBoss ? 48.0f : (isHeadshot ? 20.0f : 13.5f);
    return std::min(100.0f, std::max(0.0f, currentPct + gain));
}

// --- 3. Экономика кибер-перков [B] и подсчёт достижений (14 шт., биты 0..13, вкл. 2 банных) ---
constexpr int TOTAL_ACHIEVEMENTS = 14;
constexpr int ALL_ACHIEVEMENTS_MASK = (1 << TOTAL_ACHIEVEMENTS) - 1; // 16383

inline int PerkUpgradeCost(int currentLevel) {
    int lvl = std::max(0, currentLevel);
    return 75 + lvl * 65;
}

inline int CountUnlockedAchievements(int mask) {
    int m = mask & ALL_ACHIEVEMENTS_MASK;
    int cnt = 0;
    for (int i = 0; i < TOTAL_ACHIEVEMENTS; ++i) {
        if (m & (1 << i)) ++cnt;
    }
    return cnt;
}

} // namespace Cyber
