#pragma once
// ============================================================================
// CyberSystems.h — Тактические системы CHIKENGUN 2.4:
// мутаторы волн, орбитальный удар «ЯЙЦЕГЕДДОН», экономика перков, достижения,
// боевая статистика рейда, комбо-множитель и урон с критами.
// Не зависит от заголовков рендера — тестируется напрямую в tests/test_core.cpp.
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Cyber {

// --- 1. Мутаторы волн арены ---
enum WaveMutator : int {
    MUT_STANDARD = 0,   // Стандартный бой
    MUT_LOW_GRAV = 1,   // Лунная гравитация: гравитация x0.52, высокие прыжки
    MUT_TURBO    = 2,   // Турбо-стая: скорость кур x1.28, очки и кредиты x1.35
    MUT_GOLDRUSH = 3,   // Золотая лихорадка: кредиты x2.0 за каждое убийство
    MUT_VOLATILE = 4,   // Пиро-охота: каждая поверженная курица взрывается микро-фугасом
    MUT_TOXIC    = 5,   // Ядовитый туман: густой туман, быстрые и злые куры
    MUT_GLASS    = 6,   // Стеклянные куры: враги хрупкие, но наносят больше урона
    MUT_SWARM    = 7,   // Рой яиц: дополнительный выводок летающих мини-боссов
    MUT_COUNT    = 8
};

struct MutatorInfo {
    const char* name;
    const char* desc;
    float gravityMul;
    float speedMul;
    float creditMul;
    bool  volatileDeaths;
    float fogMul     = 1.0f;    // плотность тумана сцены
    float enemyHpMul = 1.0f;    // множитель HP врагов
    float damageMul  = 1.0f;    // множитель урона врагов по игроку
    bool  extraEggs  = false;   // дополнительный выводок летающих яиц
};

inline const MutatorInfo& GetMutatorInfo(int mut) {
    static const MutatorInfo kTable[MUT_COUNT] = {
        { "СТАНДАРТНЫЙ ПРОТОКОЛ", "Штатная гравитация и стандартные награды",          1.00f, 1.00f, 1.00f, false, 1.00f, 1.00f, 1.00f, false },
        { "ЛУННАЯ ГРАВИТАЦИЯ",    "Гравитация снижена вдвое — гигантские прыжки!",     0.52f, 1.00f, 1.15f, false, 1.00f, 1.00f, 1.00f, false },
        { "ТУРБО-СТАЯ",           "Курицы на форсаже (+28% скорости, +35% кредитов)",  1.00f, 1.28f, 1.35f, false, 1.00f, 1.00f, 1.00f, false },
        { "ЗОЛОТАЯ ЛИХОРАДКА",    "Двойные кибер-кредиты (x2₵) за каждого врага!",     1.00f, 1.05f, 2.00f, false, 1.00f, 1.00f, 1.00f, false },
        { "ПИРО-ШТОРМ",           "Все куры детонируют осколочным взрывом при гибели!",1.00f, 1.08f, 1.25f, true,  1.10f, 1.00f, 1.00f, false },
        { "ЯДОВИТЫЙ ТУМАН",       "Туман сгущается, куры быстрее и злее (+45% кредитов)", 0.92f, 1.12f, 1.45f, false, 1.85f, 1.00f, 1.10f, false },
        { "СТЕКЛЯННЫЕ КУРЫ",      "Враги хрупкие (45% HP), но бьют больнее (+60% кредитов)", 1.00f, 1.10f, 1.60f, false, 1.00f, 0.45f, 1.25f, false },
        { "РОЙ ЯИЦ",              "Выводок летающих яиц-мини-боссов (+30% кредитов)",      0.80f, 1.05f, 1.30f, false, 1.00f, 1.00f, 1.00f, true  },
    };
    int idx = (mut >= 0 && mut < MUT_COUNT) ? mut : 0;
    return kTable[idx];
}

// Детерминированно-разнообразный выбор мутатора для волны (волна 1 и босс-волны — без хаоса или с бонусом)
inline int PickWaveMutator(int waveNum, int randomSeed) {
    if (waveNum <= 1) return MUT_STANDARD;
    if (waveNum % 5 == 0) return MUT_GOLDRUSH; // на волне с Мега-Индюком всегда x2 кредитов!
    int pool[7] = { MUT_LOW_GRAV, MUT_TURBO, MUT_GOLDRUSH, MUT_VOLATILE, MUT_TOXIC, MUT_GLASS, MUT_SWARM };
    int idx = std::abs(waveNum * 7 + randomSeed) % 8;
    if (idx == 7) return MUT_STANDARD;
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

// --- 3. Экономика кибер-перков [B] и подсчёт достижений (12 шт., биты 0..11) ---
constexpr int TOTAL_ACHIEVEMENTS = 12;
constexpr int ALL_ACHIEVEMENTS_MASK = (1 << TOTAL_ACHIEVEMENTS) - 1; // 4095

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

// --- 4. Мини-босс «ЛЕТАЮЩЕЕ ЯЙЦО» (с 3-й волны на каждой нечётной) ---
inline bool ShouldSpawnEggBoss(int waveNum) {
    return waveNum >= 3 && (waveNum % 2 == 1);
}

inline int EggBossHp(int waveNum) {
    return 240 + waveNum * 45;
}

inline int EggBossEggsPerVolley(bool enraged) {
    return enraged ? 12 : 8;
}

// --- 5. Хрупкие куры (мутатор «Стеклянные куры») ---
inline int ScaleEnemyHp(int baseHp, int mutator) {
    float hp = static_cast<float>(baseHp);
    const MutatorInfo& m = GetMutatorInfo(mutator);
    // fragile-мутатор описан в таблице: MUT_GLASS => 45% HP
    if (mutator == MUT_GLASS) hp *= 0.45f;
    (void)m;
    return std::max(1, static_cast<int>(hp + 0.5f));
}

// --- 6. Боевая статистика рейда (экран GAME OVER) ---
struct RaidStats {
    int   score;
    int   wave;
    int   kills;
    int   headshots;
    int   shotsFired;
    int   shotsHit;
    int   timeAliveSec;
    int   maxCombo;
    int   bestWave;
    float damageDealt;
    float damageTaken;
};

inline int AccuracyPercent(int shotsFired, int shotsHit) {
    if (shotsFired <= 0) return 0;
    int hit = std::max(0, std::min(shotsHit, shotsFired));
    return static_cast<int>(100.0f * hit / shotsFired + 0.5f);
}

// Ранг рейда: 0..5 (S, A, B, C, D, F) по совокупности показателей.
inline char RaidGrade(const RaidStats& st) {
    int acc = AccuracyPercent(st.shotsFired, st.shotsHit);
    int pts = st.kills * 40 + st.wave * 120 + acc * 8 + st.headshots * 25 + st.maxCombo * 15;
    if (st.kills == 0) return 'F';
    if (pts >= 5200) return 'S';
    if (pts >= 3600) return 'A';
    if (pts >= 2200) return 'B';
    if (pts >= 1200) return 'C';
    if (pts >= 500)  return 'D';
    return 'F';
}

// Комбо-множитель очков: растёт, но с жёстким пределом x4 (аудит §9: множитель
// без предела обесценивал рекорд).
inline float ComboMultiplier(int comboKills) {
    if (comboKills <= 1) return 1.0f;
    return std::min(4.0f, 1.0f + 0.25f * static_cast<float>(comboKills - 1));
}

// Итоговые очки за фраг с учётом комбо и множителя мутатора.
inline int AwardKillScore(int basePoints, int comboKills, int mutator) {
    float combo = ComboMultiplier(comboKills);
    float wave = GetMutatorInfo(mutator).creditMul;
    return static_cast<int>(static_cast<float>(basePoints) * combo * (0.75f + 0.25f * wave) + 0.5f);
}

// --- 7. Урон с критами и хэдшотами (единая формула для клиента и веба) ---
inline float ComputeShotDamage(float baseDamage, bool headshot, bool crit,
                               int tungstenPerkLevel, bool overdrive) {
    float dmg = baseDamage;
    dmg *= (1.0f + 0.15f * static_cast<float>(std::max(0, tungstenPerkLevel)));
    if (overdrive) dmg *= 2.0f;
    if (crit) dmg *= 2.0f;
    if (headshot) dmg *= 2.0f;
    return dmg;
}

inline float CritChanceFromPerks(int tungstenPerkLevel) {
    return 0.06f + 0.03f * static_cast<float>(std::max(0, tungstenPerkLevel));
}

// --- 8. Пул частиц: защита длинных сессий от деградации ---
inline int ClampParticleBudget(int current, int limit) {
    if (limit <= 0) return 0;
    return current < limit ? current : limit;
}

// --- 9. Форматирование времени рейда для HUD / GAME OVER (MM:SS) ---
inline void FormatRaidTime(int seconds, char* out, int outSize) {
    if (!out || outSize <= 0) return;
    if (seconds < 0) seconds = 0;
    int mm = seconds / 60;
    int ss = seconds % 60;
    std::snprintf(out, static_cast<size_t>(outSize), "%d:%02d", mm, ss);
}

} // namespace Cyber
