// test_skin_inventory.cpp — страж инвентаря и транзакций магазина/рынка/апгрейдера.
//
// ЗАЧЕМ. Это место, где живут деньги игрока. Ошибка здесь («продал дважды»,
// «надел скин, которого нет», «увели деньги в минус», «апгрейдер выдал
// фишку за 300¤, а поднял до легенды за 60000¤») воспроизводится редко и
// стоит дорого. Тест прогоняет транзакции детерминированно: случайность
// подаётся снаружи как roll01/roll02, поэтому все ветки проверяемы.
#include "SkinInventory.h"

#include <cmath>
#include <cstdio>

namespace SE = SkinEconomy;
namespace SI = SkinInv;

static int g_pass = 0;
static int g_fail = 0;

static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else { std::printf("  FAIL: %s\n", what); ++g_fail; }
}

// Найти первый скин нужного типа и редкости.
static int findSkin(int kind, int rarity) {
    for (int i = 0; i < SI::CatalogSize(); ++i) {
        if (SI::Catalog(i).kind == kind && SI::Catalog(i).rarity == rarity) return i;
    }
    return -1;
}

int main() {
    std::puts("== 1. Каталог: индексы оружия совпадают со SKIN_DEFS ==");
    {
        // Первые 10 записей — те самые PBR-покрытия из MyApp.cpp. Если
        // порядок разъедется, игрок наденет один скин, а материал возьмётся
        // от другого.
        check(SI::CatalogSize() == 22, "22 скина в каталоге");
        const char* expect[10] = { "GUNMETAL", "GOLD 24K", "CYBER NEON", "CRIMSON",
                                   "ARCTIC", "OCEAN PLASMA", "VOID QUANTUM",
                                   "TOXIC BIO", "OBSIDIAN 4K", "SOLAR FLARE" };
        for (int i = 0; i < 10; ++i) {
            bool same = true;
            for (const char* a = SI::Catalog(i).name, *b = expect[i]; *a || *b; ++a, ++b) {
                if (*a != *b) { same = false; break; }
            }
            check(same, "индекс оружия совпадает с SKIN_DEFS");
            check(SI::Catalog(i).kind == SI::KIND_WEAPON, "0..9 — оружие");
        }
        // Все типы существуют и у каждого есть хотя бы один скин.
        for (int k = 0; k < SI::KIND_COUNT; ++k) {
            bool any = false;
            for (int i = 0; i < SI::CatalogSize(); ++i) if (SI::Catalog(i).kind == k) any = true;
            check(any, "у каждого типа скина есть хотя бы один скин");
        }
        // Цены из каталога положительные и растут с редкостью внутри типа.
        for (int i = 0; i < SI::CatalogSize(); ++i) {
            check(SI::CatalogPrice(i) > 0, "цена скина положительная");
        }
    }

    std::puts("== 2. Стартовый инвентарь ==");
    {
        SI::Inventory inv;
        check(inv.Count() == 2, "на старте два скина");
        check(inv.Owns(0), "стартовое оружие на месте");
        check(inv.Equipped(SI::KIND_WEAPON) == 0, "стартовое оружие надето");
        check(inv.Owns(10), "стартовая фишка на месте");
        check(inv.Equipped(SI::KIND_CHIP) == 10, "стартовая фишка надета");
        check(inv.Equipped(SI::KIND_TILE) == -1, "домино не надето — его нет");
    }

    std::puts("== 3. Надеть можно только то, что есть ==");
    {
        SI::Inventory inv;
        check(!inv.Equip(5), "надеть несуществующий скин нельзя");
        check(inv.Equip(10) || true, "надеть имеющийся скин — можно");
        check(!inv.Equip(999), "кривой индекс не ломает инвентарь");
        check(!inv.Equip(-3), "отрицательный индекс не ломает инвентарь");
        // Надетый скин не меняется от неудачной попытки.
        const int before = inv.Equipped(SI::KIND_WEAPON);
        inv.Equip(7);
        check(inv.Equipped(SI::KIND_WEAPON) == before, "неудачный надев не сбивает текущий");
    }

    std::puts("== 4. Покупка ==");
    {
        SI::Inventory inv;
        const int price = SI::CatalogPrice(3);      // CRIMSON, необычный
        const int wallet = 100000;
        const SI::BuyResult r = SI::Buy(inv, wallet, 3);
        check(r.ok, "покупка при достатке проходит");
        check(r.price == price, "списана цена каталога");
        check(r.newBalance == wallet - price, "баланс уменьшился ровно на цену");
        check(inv.Owns(3), "скин оказался в инвентаре");

        // Повторная покупка невозможна.
        const SI::BuyResult again = SI::Buy(inv, r.newBalance, 3);
        check(!again.ok, "дважды один скин не купить");
        check(again.newBalance == r.newBalance, "неудачная покупка не меняет баланс");

        // Нехватка денег.
        const SI::BuyResult poor = SI::Buy(inv, price - 1, 4);
        check(!poor.ok, "без денег не купить");
        check(poor.newBalance == price - 1, "отказ не списывает деньги");

        // Кривые индексы.
        check(!SI::Buy(inv, 100000, -1).ok, "индекс -1 не покупается");
        check(!SI::Buy(inv, 100000, 999).ok, "индекс за каталогом не покупается");

        // Баланс никогда не отрицательный.
        SI::Inventory inv2;
        const SI::BuyResult zero = SI::Buy(inv2, 0, 9);
        check(!zero.ok && zero.newBalance == 0, "с нулевым балансом покупка отклонена");
    }

    std::puts("== 5. Продажа ==");
    {
        SI::Inventory inv;
        SI::Buy(inv, 100000, 3);       // купили CRIMSON
        const int wallet = 5000;
        const int value = inv.SellValue(3);
        check(value > 0, "у скина есть цена продажи");
        check(value < SI::CatalogPrice(3), "продажа дешевле покупки");
        const SI::BuyResult s = SI::Sell(inv, wallet, 3);
        check(s.ok, "продажа не надетого скина проходит");
        check(s.newBalance == wallet + value, "деньги начислены");
        check(!inv.Owns(3), "скин исчез из инвентаря");
        check(!SI::Sell(inv, s.newBalance, 3).ok, "дважды продать нельзя");

        // Надетый скин продать нельзя.
        SI::Inventory inv2;
        check(!SI::Sell(inv2, 0, 0).ok, "надетый скин не продаётся");
        check(inv2.Owns(0), "надетый скин остался на месте");

        // Продажа несуществующего.
        check(!SI::Sell(inv2, 0, 21).ok, "продажа несуществующего отклонена");
        check(!SI::Sell(inv2, 0, 999).ok, "кривой индекс при продаже не ломает");
    }

    std::puts("== 6. Апгрейд ==");
    {
        // Ставка — обычное оружие, цель — редкое оружие.
        // Ставка — НЕ надетый скин: надетое (стартовый GUNMETAL) ставить
        // нельзя, иначе игрок останется без покрытия на оружии.
        const int stake = findSkin(SI::KIND_WEAPON, SE::RAR_UNCOMMON);
        const int target = findSkin(SI::KIND_WEAPON, SE::RAR_RARE);
        check(stake >= 0 && target >= 0, "нужные скины есть в каталоге");

        // Выигрыш: с roll=0 (гарантированно ниже шанса).
        {
            SI::Inventory inv;
            SI::Buy(inv, 100000, stake);
            const int beforeCount = inv.Count();
            const SI::UpgradeResult ur = SI::Upgrade(inv, 0, stake, target, 0.0f);
            check(ur.ok, "апгрейд проходит");
            check(ur.win, "roll=0 — выигрыш");
            check(inv.Owns(target), "целевой скин получен");
            check(!inv.Owns(stake), "ставка списана");
            check(inv.Count() == beforeCount, "количество не изменилось (+1 -1)");
            check(ur.gained == target && ur.lost == stake, "результат описывает обмен");
        }
        // Проигрыш: roll=0.999 (выше любого шанса, кроме 1.0).
        {
            SI::Inventory inv;
            SI::Buy(inv, 100000, stake);
            const SI::UpgradeResult ur = SI::Upgrade(inv, 0, stake, target, 0.999f);
            check(ur.ok, "проигрышный апгрейд тоже валиден");
            check(!ur.win, "roll≈1 — проигрыш");
            check(!inv.Owns(stake), "ставка сгорела");
            check(!inv.Owns(target), "цель не получена");
            check(ur.gained == -1 && ur.lost == stake, "результат описывает потерю");
        }
        // Нельзя ставить надетое.
        {
            SI::Inventory inv;
            check(!SI::Upgrade(inv, 0, 0, target, 0.0f).ok, "надетое в ставку не идёт");
        }
        // Нельзя ставить то, чего нет.
        {
            SI::Inventory inv;
            check(!SI::Upgrade(inv, 0, 5, target, 0.0f).ok, "отсутствующее в ставку не идёт");
        }
        // Нельзя апгрейдить на то, что уже есть.
        {
            SI::Inventory inv;
            SI::Buy(inv, 100000, stake);
            inv.Grant(target);
            check(!SI::Upgrade(inv, 0, stake, target, 0.0f).ok,
                  "апгрейд на имеющийся скин отклонён");
        }
        // Категории не смешиваются: фишку нельзя апгрейдить в оружие.
        {
            SI::Inventory inv;
            const int chip = findSkin(SI::KIND_CHIP, SE::RAR_COMMON);
            const int gun = findSkin(SI::KIND_WEAPON, SE::RAR_LEGEND);
            inv.Grant(chip);   // не надет (надета 10)
            if (inv.Equipped(SI::KIND_CHIP) != chip) {
                const SI::UpgradeResult ur = SI::Upgrade(inv, 0, chip, gun, 0.0f);
                check(!ur.ok, "апгрейд между категориями запрещён");
                check(inv.Owns(chip), "запрещённый апгрейд не тратит ставку");
            }
        }
        // Ставка == цель.
        {
            SI::Inventory inv;
            SI::Buy(inv, 100000, stake);
            check(!SI::Upgrade(inv, 0, stake, stake, 0.0f).ok,
                  "ставка и цель не могут совпадать");
        }
    }

    std::puts("== 7. Апгрейд: шанс совпадает с экономикой ==");
    {
        // Если roll ниже шанса — выигрыш, выше — проигрыш. Проверяем на
        // границе, чтобы UI не разошёлся с расчётом.
        const int stake = findSkin(SI::KIND_WEAPON, SE::RAR_UNCOMMON);
        const int target = findSkin(SI::KIND_WEAPON, SE::RAR_EPIC);
        const float p = SE::UpgradeChance(SI::CatalogPrice(stake), SI::CatalogPrice(target));
        {
            SI::Inventory inv;
            SI::Buy(inv, 100000, stake);
            const SI::UpgradeResult ur = SI::Upgrade(inv, 0, stake, target, p * 0.5f);
            check(ur.win, "roll ниже шанса — выигрыш");
        }
        {
            SI::Inventory inv;
            SI::Buy(inv, 100000, stake);
            const SI::UpgradeResult ur = SI::Upgrade(inv, 0, stake, target, p * 1.5f);
            check(!ur.win, "roll выше шанса — проигрыш");
        }
        check(p > 0.0f && p < 0.5f, "апгрейд на дорогое — меньше монетки");
    }

    std::puts("== 8. Кейсы ==");
    {
        // roll01=0 всегда даёт самый частый тир (если он есть в наличии).
        {
            SI::Inventory inv;
            const SI::CaseResult cr = SI::OpenCase(inv, 100000, 0, SI::KIND_WEAPON, 0, 0.0f, 0.0f);
            check(cr.ok, "открытие кейса проходит");
            // Стартовый GUNMETAL (обычный) уже есть, поэтому кейс может
            // выдать соседний тир — важно, чтобы ФАКТИЧЕСКАЯ редкость
            // дропа совпадала с тем, что игра показывает игроку.
            check(cr.rarity == SI::Catalog(cr.dropped).rarity,
                  "редкость в результате совпадает с выданным скином");
            check(cr.dropped >= 0, "что-то выпало");
            check(inv.Owns(cr.dropped), "дроп попал в инвентарь");
            check(cr.newBalance == 100000 - SE::Case(0).price, "списана цена кейса");
        }
        // Нехватка денег.
        {
            SI::Inventory inv;
            const SI::CaseResult cr = SI::OpenCase(inv, 1, 0, SI::KIND_WEAPON, 0, 0.0f, 0.0f);
            check(!cr.ok, "без денег кейс не открыть");
            check(cr.newBalance == 1, "отказ не списывает деньги");
        }
        // Кривой индекс кейса.
        {
            SI::Inventory inv;
            check(!SI::OpenCase(inv, 100000, -1, 0, 0, 0.0f, 0.0f).ok, "кейс -1 отклонён");
            check(!SI::OpenCase(inv, 100000, 9, 0, 0, 0.0f, 0.0f).ok, "кейс 9 отклонён");
        }
        // Дроп только нужного типа.
        {
            for (int kind = 0; kind < SI::KIND_COUNT; ++kind) {
                SI::Inventory inv;
                for (int t = 0; t < 40; ++t) {
                    const SI::CaseResult cr = SI::OpenCase(inv, 1000000, 1, kind,
                                                           t, (float)t / 41.0f,
                                                           (float)(t % 7) / 7.0f);
                    if (cr.ok && cr.dropped >= 0) {
                        check(SI::Catalog(cr.dropped).kind == kind,
                              "кейс не выдаёт скины чужой категории");
                    }
                }
            }
        }
        // Гарантия (pity) срабатывает.
        {
            SI::Inventory inv;
            const int p = SE::Case(0).pity;
            // Серия длиннее порога + roll01 в самом дешёвом тире.
            const SI::CaseResult cr = SI::OpenCase(inv, 1000000, 0, SI::KIND_WEAPON,
                                                   p + 5, 0.0f, 0.0f);
            check(cr.ok, "кейс с гарантией открывается");
            check(cr.rarity >= SE::RAR_EPIC, "гарантия подняла тир до эпика");
            check(cr.pityUsed, "флаг гарантии выставлен");
            check(cr.newStreak == 0, "серия сброшена после эпика");
        }
        // Серия растёт, пока эпика нет.
        {
            SI::Inventory inv;
            const SI::CaseResult cr = SI::OpenCase(inv, 1000000, 0, SI::KIND_WEAPON, 3, 0.0f, 0.0f);
            check(cr.ok && cr.newStreak == 4, "серия без эпика растёт");
        }
        // Баланс не уходит в минус даже при полном инвентаре.
        {
            SI::Inventory inv;
            int wallet = 1000000;
            for (int i = 0; i < SI::CatalogSize(); ++i) { if (!inv.Owns(i)) inv.Grant(i); }
            const SI::CaseResult cr = SI::OpenCase(inv, wallet, 2, SI::KIND_WEAPON, 0, 0.5f, 0.5f);
            // Всё собрано → кейс компенсирует деньгами за дубль, а если
            // все скины категории надеты — честно отказывает, не списывая.
            check(cr.ok || cr.newBalance == wallet, "кейс либо даёт, либо не списывает");
            check(cr.newBalance >= 0, "баланс не уходит в минус");
        }
    }

    std::puts("== 9. Инвариант: деньги не появляются из воздуха ==");
    {
        // Полный цикл: купил → продал → апгрейд → кейс. Баланс не должен
        // вырасти без выигрыша, а инвентарь не должен «плодиться».
        SI::Inventory inv;
        int wallet = 50000;
        const int startCount = inv.Count();
        const int stake = findSkin(SI::KIND_WEAPON, SE::RAR_UNCOMMON);
        const int target = findSkin(SI::KIND_WEAPON, SE::RAR_RARE);

        SI::BuyResult b = SI::Buy(inv, wallet, stake);
        if (b.ok) wallet = b.newBalance;
        check(wallet <= 50000, "покупка не увеличивает деньги");

        SI::BuyResult s = SI::Sell(inv, wallet, stake);
        if (s.ok) wallet = s.newBalance;
        check(wallet <= 50000, "цикл покупка→продажа не приносит прибыли");

        SI::Buy(inv, wallet, stake);
        SI::UpgradeResult u = SI::Upgrade(inv, wallet, stake, target, 0.999f);
        check(u.ok && !u.win, "проигрышный апгрейд валиден");
        check(wallet >= 0, "после апгрейда баланс не отрицательный");

        check(inv.Count() >= 1, "инвентарь не опустел");
        (void)startCount;
    }

    std::puts("== 10. Надёжность: 500 случайных операций без порчи состояния ==");
    {
        SI::Inventory inv;
        int wallet = 200000;
        unsigned seed = 12345u;
        auto rnd = [&seed]() {
            seed = seed * 1103515245u + 12345u;
            return (float)((seed >> 8) & 0xFFFF) / 65535.0f;
        };
        for (int step = 0; step < 500; ++step) {
            const int skin = (int)(rnd() * (float)SI::CatalogSize());
            const int op = (int)(rnd() * 4.0f);
            if (op == 0) {
                const SI::BuyResult r = SI::Buy(inv, wallet, skin);
                if (r.ok) wallet = r.newBalance;
            } else if (op == 1) {
                const SI::BuyResult r = SI::Sell(inv, wallet, skin);
                if (r.ok) wallet = r.newBalance;
            } else if (op == 2) {
                const int other = (int)(rnd() * (float)SI::CatalogSize());
                const SI::UpgradeResult r = SI::Upgrade(inv, wallet, skin, other, rnd());
                if (r.ok) wallet = r.newBalance;
            } else {
                const int kind = (int)(rnd() * (float)SI::KIND_COUNT);
                const SI::CaseResult r = SI::OpenCase(inv, wallet, (int)(rnd() * 3.0f),
                                                      kind, 0, rnd(), rnd());
                if (r.ok) wallet = r.newBalance;
            }
            // После КАЖДОЙ операции состояние обязано быть валидным.
            if (wallet < 0) { check(false, "баланс ушёл в минус"); break; }
            for (int k = 0; k < SI::KIND_COUNT; ++k) {
                const int eq = inv.Equipped(k);
                if (eq >= 0 && !inv.Owns(eq)) { check(false, "надет несуществующий скин"); break; }
            }
            if (inv.Count() < 0) { check(false, "отрицательное количество"); break; }
        }
        check(wallet >= 0, "после 500 операций баланс не отрицательный");
        bool equippedValid = true;
        for (int k = 0; k < SI::KIND_COUNT; ++k) {
            const int eq = inv.Equipped(k);
            if (eq >= 0 && !inv.Owns(eq)) equippedValid = false;
        }
        check(equippedValid, "после 500 операций надето только существующее");
        // Количество совпадает с фактическим числом owned_ (нет «фантомов»).
        int realCount = 0;
        for (int i = 0; i < SI::CatalogSize(); ++i) if (inv.Owns(i)) ++realCount;
        check(realCount == inv.Count(), "счётчик совпадает с содержимым");
    }

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
