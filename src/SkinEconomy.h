#pragma once

// SkinEconomy.h — магазин, рынок и апгрейдер скинов с кейсами.
//
// ЗАЧЕМ ОТДЕЛЬНЫЙ МОДУЛЬ. Всё это — правила экономики: редкость, цены,
// шансы апгрейда, дроп из кейсов, курсы рынка. Если писать их прямо в UI,
// то балансить их нельзя (каждая правка требует запуска игры), а проверить
// тем более. Здесь только числа и чистые функции: без Urho3D, без RNG из
// движка, без состояния рендера — значит, проверяется тестом.
// tests/test_skin_economy.cpp гоняет инварианты (сумма шансов, EV апгрейдера,
// монотонность цен), а MyApp.cpp только рисует то, что посчитано тут.
//
// МОДЕЛЬ (как в CS-подобных играх, но в валюте казино — ¤):
//   * Магазин — покупка скина по фиксированной цене.
//   * Рынок — продажа своих скинов и курс «сколько дают за скин» (0.62 цены,
//     чтобы нельзя было бесконечно крутить арбитраж магазин↔рынок).
//   * Апгрейдер — ставка свой скин против чужого: если выпало, получаешь
//     целевой скин, иначе теряешь свой. Шанс = f(соотношение цен), с полом
//     и потолком, чтобы апгрейдер не стал ни печатным станком, ни казино
//     без шансов.
//   * Кейс — платный сундук: взвешенный дроп по редкости, гарантия «не
//     хуже» по pity-счётчику.

namespace SkinEconomy {

// ── Редкость ──────────────────────────────────────────────────────────────
// Индекс = тир. Цены и веса дропа привязаны к тиру, а не к конкретному
// скину: так новый скин не требует перебалансировки кейсов.
enum Rarity {
    RAR_COMMON = 0,   // ширпотреб
    RAR_UNCOMMON,     // чуть реже
    RAR_RARE,         // «синий»
    RAR_EPIC,         // «фиолетовый»
    RAR_LEGEND,       // «золотой»
    RAR_COUNT
};

inline const char* RarityName(int r) {
    switch (r) {
    case RAR_COMMON:   return "ОБЫЧНЫЙ";
    case RAR_UNCOMMON: return "НЕОБЫЧНЫЙ";
    case RAR_RARE:     return "РЕДКИЙ";
    case RAR_EPIC:     return "ЭПИЧЕСКИЙ";
    case RAR_LEGEND:   return "ЛЕГЕНДА";
    default:           return "?";
    }
}

// Базовые цены по тиру (в ¤). Дорогие тиры дорожают нелинейно: легенда
// должна быть событием, а не «ещё одной покупкой».
inline int BasePrice(int rarity) {
    switch (rarity) {
    case RAR_COMMON:   return 400;
    case RAR_UNCOMMON: return 1200;
    case RAR_RARE:     return 4000;
    case RAR_EPIC:     return 15000;
    case RAR_LEGEND:   return 60000;
    default:           return 0;
    }
}

// ── Кривая апгрейда ───────────────────────────────────────────────────────
// Шанс зависит от отношения цен:  p = 0.5 / (1 + 0.9 * (ratio - 1)).
//
// Почему так, а не «0.5 ^ ratio» и не «stake / target»:
//   * «0.5 ^ ratio» при отношении 10 даёт 0.1% — кнопка «на легенду»
//     мертва, это не апгрейдер, а скринсейвер;
//   * «stake / target» — честная математика без преимущества дома, то есть
//     бесконечный обмен по себестоимости;
//   * рациональная кривая ниже даёт 1:1 ровно 50% (монетка, как игрок и
//     ждёт), 1:4 — 26%, 1:150 — 0.4%, и при этом EV ВСЕГДА отрицательная:
//     игрок в среднем теряет 20-45% ставки, а не выигрывает в разы.
// Это ровно тот инвариант, который держит test_skin_economy.cpp: если
// кто-то «улучшит» кривую до щедрой, тест упадёт на EV > 0.
inline float UpgradeChance(int stakeValue, int targetValue) {
    if (stakeValue <= 0 || targetValue <= 0) return 0.0f;
    if (targetValue < stakeValue) return 1.0f;    // цель дешевле — забираем
    const float ratio = (float)targetValue / (float)stakeValue;
    float chance = 0.5f / (1.0f + 0.9f * (ratio - 1.0f));
    if (chance < 0.0005f) chance = 0.0005f;       // не ровно ноль
    if (chance > 0.98f) chance = 0.98f;
    return chance;
}

// Математическое ожидание апгрейда: chance * target - (1-chance) * stake.
// Отрицательное — нормально (это дом), но не должно быть разорительным.
// Тест требует: EV ≤ 0 (иначе печатный станок) и EV ≥ -65% ставки
// (иначе апгрейдером нельзя пользоваться вообще).
inline float UpgradeExpectedValue(int stakeValue, int targetValue) {
    if (stakeValue <= 0 || targetValue <= 0) return 0.0f;
    const float p = UpgradeChance(stakeValue, targetValue);
    return p * (float)targetValue - (1.0f - p) * (float)stakeValue;
}

// ── Рынок ─────────────────────────────────────────────────────────────────
// Продажа приносит долю цены. Доля растёт с редкостью: обычные скины
// обесцениваются быстро, легенды держат цену.
inline float SellRatio(int rarity) {
    switch (rarity) {
    case RAR_COMMON:   return 0.45f;
    case RAR_UNCOMMON: return 0.52f;
    case RAR_RARE:     return 0.60f;
    case RAR_EPIC:     return 0.66f;
    case RAR_LEGEND:   return 0.72f;
    default:           return 0.5f;
    }
}

inline int SellPrice(int basePrice, int rarity) {
    int price = (int)((float)basePrice * SellRatio(rarity));
    return price < 1 ? 1 : price;
}

// Наценка рынка на покупку чужого скина: рынок всегда дороже магазина,
// иначе магазин не нужен.
inline int MarketBuyPrice(int basePrice) {
    return (int)((float)basePrice * 1.15f);
}

// ── Кейсы ─────────────────────────────────────────────────────────────────
struct CaseDef {
    const char* name;
    int price;
    // Веса дропа по тирам, сумма не обязана быть 100 — нормируется.
    float weight[RAR_COUNT];
    // Гарантия: после стольких открытий без эпика/легенды кейс обязан
    // выдать хотя бы эпик (pity). 0 — гарантии нет.
    int pity;
};

// Три кейса: дешёвый базовый, средний и «премиум». Веса подобраны так,
// чтобы базовый кейс отбивался примерно в ноль на дистанции (это не
// благотворительность), а премиум имел заметный шанс на легенду.
inline const CaseDef& Case(int idx) {
    static const CaseDef kCases[3] = {
        { "СТАЛЬНОЙ ЯЩИК", 2400,
          { 60.0f, 25.0f, 11.0f, 3.2f, 0.8f }, 40 },
        { "ХРОМ-КЕЙС", 6300,
          { 34.0f, 31.0f, 21.0f, 10.5f, 3.5f }, 22 },
        { "ЛЕГЕНДА АРСЕНАЛА", 9000,
          { 30.0f, 27.0f, 22.0f, 15.0f, 6.0f }, 12 },
    };
    if (idx < 0) idx = 0;
    if (idx > 2) idx = 2;
    return kCases[idx];
}

inline int CaseCount() { return 3; }

// Суммарный вес кейса (для нормировки).
inline float CaseTotalWeight(int idx) {
    const CaseDef& c = Case(idx);
    float sum = 0.0f;
    for (int i = 0; i < RAR_COUNT; ++i) sum += c.weight[i];
    return sum > 0.0f ? sum : 1.0f;
}

// Шанс выпадения тира в кейсе, 0..1.
inline float CaseChance(int idx, int rarity) {
    if (rarity < 0 || rarity >= RAR_COUNT) return 0.0f;
    return Case(idx).weight[rarity] / CaseTotalWeight(idx);
}

// Ожидаемая стоимость дропа кейса — чтобы можно было проверить, что кейс
// не печатает деньги бесконечно (EV ≤ цены кейса).
inline float CaseExpectedValue(int idx) {
    float ev = 0.0f;
    for (int r = 0; r < RAR_COUNT; ++r) {
        ev += CaseChance(idx, r) * (float)BasePrice(r);
    }
    return ev;
}

// Выбор тира по случайному числу [0,1). Монотонное накопление весов:
// r выпадает раньше более редких тиров, поэтому один скин на тир
// достаточно — конкретный скин внутри тира выбирает игра.
inline int RollRarity(int idx, float roll01) {
    if (roll01 < 0.0f) roll01 = 0.0f;
    if (roll01 >= 1.0f) roll01 = 0.999999f;
    const CaseDef& c = Case(idx);
    const float total = CaseTotalWeight(idx);
    float acc = 0.0f;
    for (int r = 0; r < RAR_COUNT; ++r) {
        acc += c.weight[r] / total;
        if (roll01 < acc) return r;
    }
    return RAR_COUNT - 1;
}

// Pity: сколько ещё открытий осталось до гарантии эпика.
// streak — сколько кейсов подряд открыто БЕЗ эпика и выше.
inline int PityLeft(int idx, int streak) {
    const int p = Case(idx).pity;
    if (p <= 0) return -1;              // гарантии нет
    if (streak >= p) return 0;          // гарантия уже сработала бы
    return p - streak;
}

// Сработала ли гарантия: тогда тир поднимается минимум до эпика.
inline bool PityForcesEpic(int idx, int streak) {
    const int p = Case(idx).pity;
    return p > 0 && streak >= p - 1;    // (pity-1)-й кейс без эпика → гарантия
}

inline int PityMinRarity() { return RAR_EPIC; }

// ── Утилиты ───────────────────────────────────────────────────────────────
inline int ClampPrice(int v) { return v < 0 ? 0 : v; }

// Цена с учётом состояния рынка: спрос/предложение колеблется ±8%.
// factor — множитель из игры (0.92..1.08), чтобы цены «жили».
inline int DynamicPrice(int basePrice, float factor) {
    if (factor < 0.90f) factor = 0.90f;
    if (factor > 1.10f) factor = 1.10f;
    const int p = (int)((float)basePrice * factor);
    return p < 1 ? 1 : p;
}

} // namespace SkinEconomy
