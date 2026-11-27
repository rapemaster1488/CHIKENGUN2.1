// test_skin_economy.cpp — страж экономики скинов (магазин/рынок/апгрейдер/кейсы).
//
// ЗАЧЕМ. Все числа баланса живут в src/SkinEconomy.h. Без теста любая
// «небольшая правка» цены может превратить апгрейдер в печатный станок
// (игрок получает бесконечные ¤) или, наоборот, в казино без шансов.
// Тест держит инварианты, а не конкретные цифры где только можно.
#include "SkinEconomy.h"

#include <cmath>
#include <cstdio>

namespace SE = SkinEconomy;

static int g_pass = 0;
static int g_fail = 0;

static void check(bool ok, const char* what) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("  FAIL: %s\n", what);
    }
}

static bool near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

int main() {
    std::puts("== 1. Цены растут с редкостью ==");
    {
        for (int r = 1; r < SE::RAR_COUNT; ++r) {
            check(SE::BasePrice(r) > SE::BasePrice(r - 1),
                  "цена следующего тира выше предыдущего");
        }
        check(SE::BasePrice(SE::RAR_COMMON) > 0, "обычный скин не бесплатный");
        check(SE::BasePrice(SE::RAR_LEGEND) >= SE::BasePrice(SE::RAR_COMMON) * 20,
              "легенда на порядок дороже обычного — она событие");
    }

    std::puts("== 2. Апгрейдер: шанс монотонно падает с ценой цели ==");
    {
        const int stake = SE::BasePrice(SE::RAR_RARE);
        float prev = 2.0f;
        for (int r = SE::RAR_RARE; r < SE::RAR_COUNT; ++r) {
            const float ch = SE::UpgradeChance(stake, SE::BasePrice(r));
            check(ch <= prev + 1e-4f, "чем дороже цель, тем меньше шанс");
            prev = ch;
        }
        // Апгрейд на равную цену — честная монетка.
        check(near(SE::UpgradeChance(stake, stake), 0.5f, 0.01f),
              "апгрейд 1:1 — около 50%");
        // Цель дешевле ставки — забираем всегда (но это и не апгрейд).
        check(near(SE::UpgradeChance(stake, SE::BasePrice(SE::RAR_COMMON)), 1.0f),
              "цель дешевле ставки — 100%");
        // Пол и потолок.
        check(SE::UpgradeChance(stake, SE::BasePrice(SE::RAR_LEGEND)) >= 0.02f,
              "даже на легенду шанс не ниже 2% — апгрейдер не безнадёжен");
        check(SE::UpgradeChance(1, 1) <= 1.0f, "шанс не превышает 100%");
        // Нулевые/отрицательные значения не должны ломать формулу.
        check(near(SE::UpgradeChance(0, 100), 0.0f), "нулевая ставка — 0%");
        check(near(SE::UpgradeChance(100, 0), 0.0f), "нулевая цель — 0%");
    }

    std::puts("== 3. Апгрейдер: дом имеет преимущество, но не грабительское ==");
    {
        for (int a = 0; a < SE::RAR_COUNT; ++a) {
            for (int b = a + 1; b < SE::RAR_COUNT; ++b) {
                const int stake = SE::BasePrice(a);
                const int target = SE::BasePrice(b);
                const float ev = SE::UpgradeExpectedValue(stake, target);
                // EV не должна быть катастрофической: игрок теряет в среднем
                // не больше 65% ставки, иначе апгрейдером невыгодно играть
                // вообще и он превращается в «кнопку слить всё».
                check(ev >= -0.65f * (float)stake,
                      "EV апгрейда не хуже -65% ставки");
                // И не должна быть положительной (иначе это печатный станок).
                check(ev <= 0.02f * (float)stake,
                      "EV апгрейда не даёт гарантированной прибыли");
            }
        }
    }

    std::puts("== 4. Рынок: продажа не даёт арбитража с магазином ==");
    {
        for (int r = 0; r < SE::RAR_COUNT; ++r) {
            const int base = SE::BasePrice(r);
            const int sell = SE::SellPrice(base, r);
            check(sell < base, "продажа дешевле покупки в магазине");
            check(sell > 0, "продажа всё же что-то даёт");
            check(SE::MarketBuyPrice(base) > base,
                  "рынок продаёт дороже магазина");
            // Главный инвариант: купил в магазине → продал на рынке = убыток.
            check(SE::SellPrice(base, r) < base, "цикл магазин→рынок убыточен");
        }
        // Доля продажи растёт с редкостью: легенды держат цену.
        for (int r = 1; r < SE::RAR_COUNT; ++r) {
            check(SE::SellRatio(r) >= SE::SellRatio(r - 1),
                  "доля продажи не падает с редкостью");
        }
    }

    std::puts("== 5. Кейсы: шансы нормированы и сумма = 1 ==");
    {
        for (int c = 0; c < SE::CaseCount(); ++c) {
            float sum = 0.0f;
            for (int r = 0; r < SE::RAR_COUNT; ++r) {
                const float ch = SE::CaseChance(c, r);
                check(ch >= 0.0f && ch <= 1.0f, "шанс тира в кейсе — вероятность");
                sum += ch;
            }
            check(near(sum, 1.0f, 1e-3f), "сумма шансов кейса = 1");
            check(SE::Case(c).price > 0, "кейс стоит денег");
        }
        // Чем дороже кейс, тем выше шанс легенды.
        for (int c = 1; c < SE::CaseCount(); ++c) {
            check(SE::CaseChance(c, SE::RAR_LEGEND) >=
                      SE::CaseChance(c - 1, SE::RAR_LEGEND),
                  "дорогой кейс чаще даёт легенду");
        }
        // Базовый тир в любом кейсе — самый частый.
        for (int c = 0; c < SE::CaseCount(); ++c) {
            for (int r = 1; r < SE::RAR_COUNT; ++r) {
                check(SE::CaseChance(c, SE::RAR_COMMON) >= SE::CaseChance(c, r),
                      "обычный тир выпадает чаще редких");
            }
        }
    }

    std::puts("== 6. Кейсы: не печатают деньги ==");
    {
        for (int c = 0; c < SE::CaseCount(); ++c) {
            const float ev = SE::CaseExpectedValue(c);
            // EV дропа не должна превышать цену кейса, иначе выгодно
            // открывать кейсы бесконечно (в пересчёте на продажу — тем более).
            check(ev <= (float)SE::Case(c).price,
                  "EV дропа не выше цены кейса");
            // Но и не должна быть мизерной: кейс обязан быть осмысленным.
            check(ev >= 0.35f * (float)SE::Case(c).price,
                  "EV дропа хотя бы 35% цены кейса");
        }
    }

    std::puts("== 7. Дроп: roll корректно раскладывается по тирам ==");
    {
        for (int c = 0; c < SE::CaseCount(); ++c) {
            check(SE::RollRarity(c, 0.0f) == SE::RAR_COMMON,
                  "roll=0 даёт самый частый тир");
            check(SE::RollRarity(c, 0.9999f) == SE::RAR_LEGEND,
                  "roll≈1 даёт самый редкий тир");
            // Границы не вылетают за диапазон.
            check(SE::RollRarity(c, -5.0f) == SE::RAR_COMMON,
                  "отрицательный roll не ломает выбор");
            check(SE::RollRarity(c, 5.0f) == SE::RAR_LEGEND,
                  "roll > 1 не ломает выбор");
            // Монотонность: больший roll даёт тир не ниже.
            int prev = -1;
            bool mono = true;
            for (int i = 0; i <= 100; ++i) {
                const int r = SE::RollRarity(c, (float)i / 101.0f);
                if (r < prev) mono = false;
                prev = r;
            }
            check(mono, "тир не убывает с ростом roll");
        }
    }

    std::puts("== 8. Pity-гарантия ==");
    {
        for (int c = 0; c < SE::CaseCount(); ++c) {
            const int p = SE::Case(c).pity;
            if (p <= 0) continue;
            check(SE::PityLeft(c, 0) == p, "в начале серии гарантия далеко");
            check(SE::PityLeft(c, p) == 0, "после порога гарантия сработала");
            check(!SE::PityForcesEpic(c, 0), "в начале серии гарантия молчит");
            check(SE::PityForcesEpic(c, p), "на пороге гарантия включается");
            // Pity не должен требовать больше открытий, чем цена кейса
            // позволяет отбить: 40 открытий дешёвого кейса = 24 000 ¤, это
            // дороже эпика (15 000) — то есть гарантия честная.
            check((float)p * (float)SE::Case(c).price >=
                      (float)SE::BasePrice(SE::RAR_EPIC),
                  "гарантия не дешевле эпика");
        }
    }

    std::puts("== 9. Динамическая цена рынка ==");
    {
        const int base = SE::BasePrice(SE::RAR_EPIC);
        check(SE::DynamicPrice(base, 1.0f) == base, "множитель 1 — цена та же");
        check(SE::DynamicPrice(base, 1.08f) > base, "спрос поднимает цену");
        check(SE::DynamicPrice(base, 0.92f) < base, "предложение снижает цену");
        // Выход за коридор обрезается.
        check(SE::DynamicPrice(base, 99.0f) <= (int)(base * 1.10f),
              "множитель сверху ограничен");
        check(SE::DynamicPrice(base, 0.0f) >= (int)(base * 0.90f),
              "множитель снизу ограничен");
        check(SE::DynamicPrice(base, 1.0f) > 0, "цена остаётся положительной");
    }

    std::puts("== 10. Редкости согласованы ==");
    {
        for (int r = 0; r < SE::RAR_COUNT; ++r) {
            check(SE::RarityName(r) != nullptr, "у тира есть имя");
            check(SE::RarityName(r)[0] != '?', "имя тира осмысленное");
        }
        check(SE::RAR_COUNT == 5, "пять тиров — как в контракте скинов");
    }

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
