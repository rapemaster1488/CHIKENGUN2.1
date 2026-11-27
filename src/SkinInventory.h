#pragma once

// SkinInventory.h — что у игрока есть, что надето и сколько стоит.
//
// ЗАЧЕМ ОТДЕЛЬНО ОТ UI. Инвентарь — это состояние, которое сохраняется между
// сессиями, переживает открытие/закрытие магазина и участвует в трёх разных
// экранах (магазин, рынок, апгрейдер). Если держать его в MyApp.cpp рядом с
// кнопками, то любая новая вкладка начинает править те же поля напрямую, и
// «продал дважды» или «надел несуществующее» становится вопросом времени.
//
// Здесь только данные и правила: без Urho3D, без UI, без RNG. Случайность
// передаётся снаружи (roll01), поэтому тест может проверить любой сценарий
// детерминированно. Правила цен/шансов — в SkinEconomy.h.
//
// КОНТРАКТ (то, что важно игроку и что проверяет test_skin_inventory.cpp):
//   1. Надетым может быть только скин, который есть в инвентаре.
//   2. Продать можно только не надетый и не последний экземпляр.
//   3. Купить можно только то, чего ещё нет (один скин = один экземпляр),
//      и только если хватает денег.
//   4. Апгрейд снимает ставку и цель из инвентаря, начисляет выигрыш.
//   5. Деньги не уходят в минус ни в одной операции.
#include "SkinEconomy.h"

namespace SkinInv {

constexpr int kMaxSkins = 64;

// Тип скина: 0 — оружие (PBR-покрытие), 1 — фишка казино, 2 — домино/карты.
// Разные категории не мержатся между собой: апгрейдер выдаёт скин только
// того же типа, что и ставка (иначе «фишку за 300¤» можно поднять в
// «легенду оружия за 60000¤», и вся экономика разъезжается).
enum SkinKind { KIND_WEAPON = 0, KIND_CHIP, KIND_TILE, KIND_COUNT };

inline const char* KindName(int k) {
    switch (k) {
    case KIND_WEAPON: return "ОРУЖИЕ";
    case KIND_CHIP:   return "ФИШКИ";
    case KIND_TILE:   return "ДОМИНО";
    default:          return "?";
    }
}

struct SkinInfo {
    const char* name;
    int rarity;     // SkinEconomy::Rarity
    int kind;       // SkinKind
};

// Каталог. Первые 10 — те самые PBR-покрытия оружия, что уже были в игре
// (SKIN_DEFS в MyApp.cpp), у них просто появилась редкость и цена. Дальше —
// фишки казино и домино, чтобы у рынка было что продавать с самого начала.
inline int CatalogSize() { return 22; }

inline const SkinInfo& Catalog(int i) {
    static const SkinInfo k[] = {
        // 0..9 — оружие (соответствует SKIN_DEFS по индексу)
        { "GUNMETAL",     SkinEconomy::RAR_COMMON,   KIND_WEAPON },
        { "GOLD 24K",     SkinEconomy::RAR_LEGEND,   KIND_WEAPON },
        { "CYBER NEON",   SkinEconomy::RAR_RARE,     KIND_WEAPON },
        { "CRIMSON",      SkinEconomy::RAR_UNCOMMON, KIND_WEAPON },
        { "ARCTIC",       SkinEconomy::RAR_UNCOMMON, KIND_WEAPON },
        { "OCEAN PLASMA", SkinEconomy::RAR_RARE,     KIND_WEAPON },
        { "VOID QUANTUM", SkinEconomy::RAR_EPIC,     KIND_WEAPON },
        { "TOXIC BIO",    SkinEconomy::RAR_RARE,     KIND_WEAPON },
        { "OBSIDIAN 4K",  SkinEconomy::RAR_EPIC,     KIND_WEAPON },
        { "SOLAR FLARE",  SkinEconomy::RAR_EPIC,     KIND_WEAPON },
        // 10..15 — фишки казино
        { "ФИШКА БРОНЗА", SkinEconomy::RAR_COMMON,   KIND_CHIP },
        { "ФИШКА СТАЛЬ",  SkinEconomy::RAR_COMMON,   KIND_CHIP },
        { "ФИШКА НЕОН",   SkinEconomy::RAR_UNCOMMON, KIND_CHIP },
        { "ФИШКА ОНИКС",  SkinEconomy::RAR_RARE,     KIND_CHIP },
        { "ФИШКА ЗОЛОТО", SkinEconomy::RAR_EPIC,     KIND_CHIP },
        { "ФИШКА ИМПЕРИЯ",SkinEconomy::RAR_LEGEND,   KIND_CHIP },
        // 16..21 — домино
        { "ДОМИНО КОСТЬ", SkinEconomy::RAR_COMMON,   KIND_TILE },
        { "ДОМИНО МРАМОР",SkinEconomy::RAR_UNCOMMON, KIND_TILE },
        { "ДОМИНО ЛАК",   SkinEconomy::RAR_UNCOMMON, KIND_TILE },
        { "ДОМИНО ХРОМ",  SkinEconomy::RAR_RARE,     KIND_TILE },
        { "ДОМИНО КОСМОС",SkinEconomy::RAR_EPIC,     KIND_TILE },
        { "ДОМИНО КОРОНА",SkinEconomy::RAR_LEGEND,   KIND_TILE },
    };
    if (i < 0) i = 0;
    if (i >= 22) i = 21;
    return k[i];
}

// Цена скина из каталога (учитывает редкость).
inline int CatalogPrice(int i) {
    return SkinEconomy::BasePrice(Catalog(i).rarity);
}

// ── Инвентарь ─────────────────────────────────────────────────────────────
struct Inventory {
    // owned_[i] — есть ли скин каталога i (один экземпляр на скин).
    bool owned_[kMaxSkins];
    // equipped_ по типам: надетый скин оружия / фишек / домино.
    int equipped_[KIND_COUNT];
    int count_;

    Inventory() : count_(0) {
        for (int i = 0; i < kMaxSkins; ++i) owned_[i] = false;
        for (int k = 0; k < KIND_COUNT; ++k) equipped_[k] = -1;
        // Стартовый набор: обычный скин оружия и обычная фишка — иначе
        // магазин открывается на пустом месте и играть нечем.
        owned_[0] = true;  equipped_[KIND_WEAPON] = 0;  ++count_;
        owned_[10] = true; equipped_[KIND_CHIP] = 10;   ++count_;
    }

    bool Owns(int i) const {
        return i >= 0 && i < kMaxSkins && owned_[i];
    }
    int Count() const { return count_; }

    int Equipped(int kind) const {
        return (kind >= 0 && kind < KIND_COUNT) ? equipped_[kind] : -1;
    }

    // Надеть скин. Можно только то, что есть; тип берётся из каталога.
    bool Equip(int i) {
        if (!Owns(i)) return false;
        const int kind = Catalog(i).kind;
        if (kind < 0 || kind >= KIND_COUNT) return false;
        equipped_[kind] = i;
        return true;
    }

    // Выдать скин (дроп, покупка). false — уже есть или кривой индекс.
    bool Grant(int i) {
        if (i < 0 || i >= kMaxSkins || owned_[i]) return false;
        owned_[i] = true;
        ++count_;
        // Если по этому типу ничего не надето — надеваем сразу, чтобы
        // игрок увидел покупку без лишнего клика.
        const int kind = Catalog(i).kind;
        if (kind >= 0 && kind < KIND_COUNT && equipped_[kind] < 0) equipped_[kind] = i;
        return true;
    }

    // Снять скин из инвентаря (продажа/ставка в апгрейдере).
    // Нельзя снять надетый: сначала надень другой, иначе игрок останется
    // с «надетым тем, чего нет» и материал оружия станет невалидным.
    bool Remove(int i) {
        if (!Owns(i)) return false;
        const int kind = Catalog(i).kind;
        if (kind >= 0 && kind < KIND_COUNT && equipped_[kind] == i) return false;
        owned_[i] = false;
        --count_;
        return true;
    }

    // Продажа: цена по рынку. Если скин надет — сначала пробуем надеть
    // другой скин того же типа (иначе продажа невозможна).
    bool CanSell(int i) const {
        if (!Owns(i)) return false;
        const int kind = Catalog(i).kind;
        if (kind >= 0 && kind < KIND_COUNT && equipped_[kind] == i) return false;
        return true;
    }

    int SellValue(int i) const {
        const SkinInfo& si = Catalog(i);
        return SkinEconomy::SellPrice(SkinEconomy::BasePrice(si.rarity), si.rarity);
    }

    int BuyPrice(int i) const {
        return SkinEconomy::MarketBuyPrice(SkinEconomy::BasePrice(Catalog(i).rarity));
    }

    // Есть ли у игрока хоть один скин данного типа, кроме i.
    bool HasOtherOfKind(int i) const {
        const int kind = Catalog(i).kind;
        for (int k = 0; k < kMaxSkins; ++k) {
            if (k != i && Owns(k) && Catalog(k).kind == kind) return true;
        }
        return false;
    }
};

// ── Транзакции ────────────────────────────────────────────────────────────
// Все операции возвращают новое состояние денег и пишут в out-параметры,
// что произошло. Ни одна не бросает исключений и не уходит в минус —
// это проверяет тест.

struct BuyResult {
    bool ok;
    int price;
    int newBalance;
    const char* reason;
};

inline BuyResult Buy(Inventory& inv, int wallet, int skinIndex) {
    BuyResult r{ false, 0, wallet, "" };
    if (skinIndex < 0 || skinIndex >= CatalogSize()) { r.reason = "нет такого скина"; return r; }
    if (inv.Owns(skinIndex)) { r.reason = "уже есть"; return r; }
    const int price = CatalogPrice(skinIndex);
    r.price = price;
    if (wallet < price) { r.reason = "не хватает ¤"; return r; }
    inv.Grant(skinIndex);
    r.ok = true;
    r.newBalance = wallet - price;
    return r;
}

inline BuyResult Sell(Inventory& inv, int wallet, int skinIndex) {
    BuyResult r{ false, 0, wallet, "" };
    if (skinIndex < 0 || skinIndex >= CatalogSize()) { r.reason = "нет такого скина"; return r; }
    if (!inv.Owns(skinIndex)) { r.reason = "нет в инвентаре"; return r; }
    if (!inv.CanSell(skinIndex)) { r.reason = "скин надет"; return r; }
    const int price = inv.SellValue(skinIndex);
    r.price = price;
    inv.Remove(skinIndex);
    r.ok = true;
    r.newBalance = wallet + price;
    return r;
}

// Итог апгрейда. Вызывается УЖЕ после броска (roll01 решил исход):
// сам бросок делает UI, чтобы анимация могла быть любой длительности,
// а состояние менялось ровно один раз.
struct UpgradeResult {
    bool ok;
    bool win;
    int newBalance;
    int gained;    // индекс полученного скина или -1
    int lost;      // индекс потерянного скина или -1
    const char* reason;
};

inline UpgradeResult Upgrade(Inventory& inv, int wallet, int stakeIdx, int targetIdx,
                             float roll01) {
    UpgradeResult r{ false, false, wallet, -1, -1, "" };
    if (stakeIdx < 0 || stakeIdx >= CatalogSize() ||
        targetIdx < 0 || targetIdx >= CatalogSize()) {
        r.reason = "нет такого скина"; return r;
    }
    if (stakeIdx == targetIdx) { r.reason = "ставка и цель совпадают"; return r; }
    if (!inv.Owns(stakeIdx)) { r.reason = "ставки нет в инвентаре"; return r; }
    if (!inv.CanSell(stakeIdx)) { r.reason = "ставка надета"; return r; }
    if (inv.Owns(targetIdx)) { r.reason = "цель уже есть"; return r; }
    if (Catalog(stakeIdx).kind != Catalog(targetIdx).kind) {
        r.reason = "разные категории"; return r;
    }

    const int stakeVal = SkinEconomy::BasePrice(Catalog(stakeIdx).rarity);
    const int targetVal = SkinEconomy::BasePrice(Catalog(targetIdx).rarity);
    const float chance = SkinEconomy::UpgradeChance(stakeVal, targetVal);
    r.win = roll01 < chance;

    // Ставка уходит в любом случае — это суть апгрейдера.
    inv.Remove(stakeIdx);
    r.lost = stakeIdx;
    if (r.win) {
        inv.Grant(targetIdx);
        r.gained = targetIdx;
    }
    r.ok = true;
    return r;
}

// ── Кейсы ─────────────────────────────────────────────────────────────────
struct CaseResult {
    bool ok;
    int newBalance;
    int dropped;      // индекс выпавшего скина или -1
    int rarity;
    bool pityUsed;    // сработала гарантия
    int newStreak;    // обновлённый счётчик «без эпика»
    const char* reason;
};

// Открытие кейса. Конкретный скин внутри тира выбирается по roll01
// (тир) и roll02 (какой именно скин этого тира и типа кейса).
// caseKind — какого типа скины сыплет кейс, чтобы доминошный кейс не выдавал
// покрытия оружия.
inline CaseResult OpenCase(Inventory& inv, int wallet, int caseIdx, int caseKind,
                           int streak, float roll01, float roll02) {
    CaseResult r{ false, wallet, -1, -1, false, streak, "" };
    if (caseIdx < 0 || caseIdx >= SkinEconomy::CaseCount()) { r.reason = "нет такого кейса"; return r; }
    const int price = SkinEconomy::Case(caseIdx).price;
    if (wallet < price) { r.reason = "не хватает ¤"; return r; }

    int rarity = SkinEconomy::RollRarity(caseIdx, roll01);
    // Гарантия: если серия дошла до порога — поднимаем минимум до эпика.
    if (SkinEconomy::PityForcesEpic(caseIdx, streak) && rarity < SkinEconomy::PityMinRarity()) {
        rarity = SkinEconomy::PityMinRarity();
        r.pityUsed = true;
    }
    if (rarity >= SkinEconomy::PityMinRarity()) r.newStreak = 0;
    else ++r.newStreak;

    // Целевой тир: выпавший, но не ниже порога гарантии. Гарантия обязана
    // работать именно на ВЫБОРЕ, а не только в отчёте: иначе игрок видит
    // «гарантия сработала», а получает обычный скин.
    int wantRarity = rarity;
    if (r.pityUsed && wantRarity < SkinEconomy::PityMinRarity()) {
        wantRarity = SkinEconomy::PityMinRarity();
    }

    // Выбираем скин нужного типа. Сначала ровно целевой тир, потом соседние
    // вверх и вниз — но НИКОГДА другой тип: категория важнее редкости
    // (иначе кейс домино выдал бы покрытие оружия).
    int pick = -1;
    for (int ring = 0; ring < SkinEconomy::RAR_COUNT && pick < 0; ++ring) {
        for (int dir = 0; dir < 2 && pick < 0; ++dir) {
            if (ring == 0 && dir == 1) break;
            const int rt = dir == 0 ? wantRarity + ring : wantRarity - ring;
            if (rt < 0 || rt >= SkinEconomy::RAR_COUNT) continue;
            int idxs[kMaxSkins];
            int n = 0;
            for (int i = 0; i < CatalogSize(); ++i) {
                if (Catalog(i).kind == caseKind && Catalog(i).rarity == rt && !inv.Owns(i)) {
                    idxs[n++] = i;
                }
            }
            if (n > 0) {
                int sel = (int)(roll02 * (float)n);
                if (sel < 0) sel = 0;
                if (sel >= n) sel = n - 1;
                pick = idxs[sel];
            }
        }
    }

    // Всё этого типа уже собрано: компенсируем ценой продажи ЛЮБОГО
    // НЕ НАДЕТОГО скина типа (надетый снять нельзя — инвариант инвентаря).
    if (pick < 0) {
        int comp = 0;
        bool canCompensate = false;
        for (int i = 0; i < CatalogSize(); ++i) {
            if (Catalog(i).kind == caseKind && inv.Owns(i) && inv.CanSell(i)) {
                comp = inv.SellValue(i);
                canCompensate = true;
                break;
            }
        }
        if (!canCompensate) { r.reason = "все скины категории надеты"; return r; }
        r.ok = true;
        r.newBalance = wallet - price + comp;
        r.dropped = -1;
        r.rarity = -1;
        return r;
    }

    inv.Grant(pick);
    r.ok = true;
    r.newBalance = wallet - price;
    r.dropped = pick;
    r.rarity = Catalog(pick).rarity;
    return r;
}

} // namespace SkinInv
