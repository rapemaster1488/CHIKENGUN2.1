// SkinShopUI.cpp — реализация экрана магазина/рынка/апгрейдера/кейсов.
//
// Деньги и предметы живут в SkinInventory.h. Здесь — раскладка, тексты и
// реакция на клик. Все суммы, шансы и причины отказа приходят из модулей,
// поэтому UI не может «случайно» начислить больше, чем разрешено.
#include "SkinShopUI.h"

#include <Urho3D/Graphics/Graphics.h>

using namespace Urho3D;

namespace SkinShopUI {

namespace {

String Money(int v) { return String(v) + " \xC2\xA4"; }

String Percent(float p) {
    const int pc = (int)(p * 100.0f + 0.5f);
    return String(pc) + "%";
}

} // namespace

void Panel::Build() {
    auto* gfx = ctx_->GetSubsystem<Graphics>();
    if (!gfx || !root_) return;
    const int sw = gfx->GetWidth();
    const int sh = gfx->GetHeight();

    panel_ = root_->CreateChild<UIElement>("SkinShopPanel");
    panel_->SetSize(sw, sh);
    panel_->SetPriority(880);

    // Затемнение фона: экран модальный, 3D за ним не должен отвлекать.
    BorderImage* dim = panel_->CreateChild<BorderImage>();
    dim->SetSize(sw, sh);
    dim->SetColor(Color(0.010f, 0.016f, 0.020f, 0.90f));

    const int pad = S(28);
    const int headH = S(64);
    const int bodyY = pad + headH + S(10);
    const int bodyH = sh - bodyY - pad;

    // ── Заголовок ────────────────────────────────────────────────────────
    titleText_ = panel_->CreateChild<Text>();
    titleText_->SetFont(font_, S(26));
    titleText_->SetText("АРСЕНАЛ И РЫНОК");
    titleText_->SetPosition(pad, pad + S(8));
    titleText_->SetColor(Color(0.98f, 0.82f, 0.34f, 1.0f));

    walletText_ = panel_->CreateChild<Text>();
    walletText_->SetFont(font_, S(21));
    walletText_->SetPosition(pad + S(330), pad + S(12));
    walletText_->SetColor(Color(0.44f, 1.0f, 0.66f, 1.0f));

    // ── Вкладки ──────────────────────────────────────────────────────────
    const int tabW = S(168);
    const int tabH = S(40);
    const int tabY = pad + headH - tabH + S(6);
    for (int i = 0; i < TAB_COUNT; ++i) {
        tabButtons_[i] = SharedPtr<BorderImage>(Btn(panel_, pad + i * (tabW + S(8)), tabY,
                                                    tabW, tabH, TabName(i),
                                                    Color(0.10f, 0.12f, 0.13f, 0.92f), 15));
    }
    closeBtn_ = SharedPtr<BorderImage>(Btn(panel_, sw - pad - S(120), pad + S(8),
                                           S(120), S(42), "ESC",
                                           Color(0.30f, 0.13f, 0.14f, 0.95f), 15));

    // ── Список слева ─────────────────────────────────────────────────────
    const int listW = S(560);
    listHead_ = panel_->CreateChild<Text>();
    listHead_->SetFont(font_, S(15));
    listHead_->SetPosition(pad, bodyY);
    listHead_->SetColor(Color(0.60f, 0.72f, 0.78f, 1.0f));

    const int rowH = Min(S(38), (bodyH - S(30)) / kRows);
    for (int i = 0; i < kRows; ++i) {
        const int y = bodyY + S(26) + i * rowH;
        BorderImage* row = panel_->CreateChild<BorderImage>();
        row->SetPosition(pad, y);
        row->SetSize(listW, rowH - S(3));
        row->SetColor(Color(0.05f, 0.07f, 0.08f, 0.92f));
        rows_[i] = SharedPtr<UIElement>(row);
        rowSkin_[i] = -1;

        Text* t = row->CreateChild<Text>();
        t->SetFont(font_, S(16));
        t->SetPosition(S(10), S(3));
        t->SetWidth(listW - S(190));
        t->SetColor(Color(0.90f, 0.92f, 0.94f, 1.0f));
        rowTexts_[i] = SharedPtr<Text>(t);

        Text* tag = row->CreateChild<Text>();
        tag->SetFont(font_, S(13));
        tag->SetPosition(listW - S(178), S(5));
        tag->SetWidth(S(168));
        tag->SetTextAlignment(HA_RIGHT);
        rowTags_[i] = SharedPtr<Text>(tag);
    }

    // ── Карточка справа ──────────────────────────────────────────────────
    const int cardX = pad + listW + S(24);
    const int cardW = sw - cardX - pad;
    BorderImage* card = panel_->CreateChild<BorderImage>();
    card->SetPosition(cardX, bodyY);
    card->SetSize(cardW, bodyH);
    card->SetColor(Color(0.03f, 0.05f, 0.055f, 0.94f));

    cardTitle_ = card->CreateChild<Text>();
    cardTitle_->SetFont(font_, S(22));
    cardTitle_->SetPosition(S(18), S(14));
    cardTitle_->SetWidth(cardW - S(36));
    cardTitle_->SetColor(Color(1.0f, 0.94f, 0.80f, 1.0f));

    cardBody_ = card->CreateChild<Text>();
    cardBody_->SetFont(font_, S(16));
    cardBody_->SetPosition(S(18), S(52));
    cardBody_->SetWidth(cardW - S(36));
    cardBody_->SetColor(Color(0.82f, 0.86f, 0.88f, 1.0f));

    actionBtn_ = SharedPtr<BorderImage>(Btn(card, S(18), bodyH - S(72), S(240), S(48),
                                            "—", Color(0.12f, 0.40f, 0.24f, 0.95f), 17));
    clearBtn_ = SharedPtr<BorderImage>(Btn(card, S(270), bodyH - S(72), S(180), S(48),
                                           "СБРОС", Color(0.16f, 0.18f, 0.20f, 0.95f), 15));

    // Кнопки кейсов (видны только на вкладке кейсов).
    for (int i = 0; i < 3; ++i) {
        caseBtns_[i] = SharedPtr<BorderImage>(Btn(card, S(18), S(120) + i * S(62),
                                                  cardW - S(36), S(54),
                                                  String(SkinEconomy::Case(i).name),
                                                  Color(0.12f, 0.14f, 0.16f, 0.95f), 17));
    }
    openCaseBtn_ = SharedPtr<BorderImage>(Btn(card, S(18), bodyH - S(72), cardW - S(36), S(52),
                                              "ОТКРЫТЬ КЕЙС", Color(0.12f, 0.40f, 0.24f, 0.95f), 18));

    hintText_ = panel_->CreateChild<Text>();
    hintText_->SetFont(font_, S(14));
    hintText_->SetPosition(pad, sh - pad - S(2));
    hintText_->SetWidth(sw - pad * 2);
    hintText_->SetColor(Color(0.62f, 0.68f, 0.72f, 1.0f));

    panel_->SetVisible(false);
    SetTab(TAB_SHOP);
}

int Panel::FirstOfKind(int kind) const {
    if (!inventory_) return -1;
    for (int i = 0; i < SkinInv::CatalogSize(); ++i) {
        if (SkinInv::Catalog(i).kind == kind && inventory_->Owns(i)) return i;
    }
    return -1;
}

void Panel::Refresh() {
    if (!panel_) return;
    walletText_->SetText(Money(wallet_));

    const bool isCases = (tab_ == TAB_CASES);
    for (int i = 0; i < 3; ++i) {
        if (caseBtns_[i]) caseBtns_[i]->SetVisible(isCases);
    }
    if (openCaseBtn_) openCaseBtn_->SetVisible(isCases);
    if (clearBtn_) clearBtn_->SetVisible(tab_ == TAB_UPGRADE && !isCases);
    for (int i = 0; i < kRows; ++i) {
        if (rows_[i]) rows_[i]->SetVisible(!isCases);
    }
    if (listHead_) listHead_->SetVisible(!isCases);

    if (isCases) {
        // ── Вкладка кейсов ───────────────────────────────────────────────
        const int ci = selCase_;
        const SkinEconomy::CaseDef& cd = SkinEconomy::Case(ci);
        String body;
        body += "Цена:  " + Money(cd.price) + "\n\n";
        body += "Шансы:\n";
        for (int r = 0; r < SkinEconomy::RAR_COUNT; ++r) {
            body += "  " + String(SkinEconomy::RarityName(r)) + "   " +
                    Percent(SkinEconomy::CaseChance(ci, r)) + "  (" +
                    Money(SkinEconomy::BasePrice(r)) + ")\n";
        }
        body += "\nОжидаемая отдача:  " + Money((int)SkinEconomy::CaseExpectedValue(ci)) +
                "  из " + Money(cd.price) + "\n";
        body += "Гарантия: каждые " + String(cd.pity) + " открытий без эпика — эпик.\n";
        if (streak_ > 0) body += "Серия без эпика: " + String(streak_) + "\n";
        if (lastCaseDrop_ >= 0) {
            body += "\nВыпало:  " + String(SkinInv::Catalog(lastCaseDrop_).name) +
                    "   [" + String(SkinEconomy::RarityName(lastCaseRarity_)) + "]";
        }
        cardTitle_->SetText(String(cd.name));
        cardTitle_->SetColor(RarityColor(SkinEconomy::RAR_LEGEND));
        cardBody_->SetText(body);
        hintText_->SetText("1-3 — выбрать кейс   |   ПРОБЕЛ — открыть   |   Q/E — вкладки   |   ESC — закрыть");
        return;
    }

    // ── Список скинов ────────────────────────────────────────────────────
    int list[kRows];
    int n = 0;
    const int kindNow = caseKind_;

    if (tab_ == TAB_SHOP) {
        listHead_->SetText("МАГАЗИН — " + String(SkinInv::KindName(kindNow)));
        for (int i = 0; i < SkinInv::CatalogSize() && n < kRows; ++i) {
            if (SkinInv::Catalog(i).kind != kindNow) continue;
            if (inventory_ && inventory_->Owns(i)) continue;   // купленное не продаём дважды
            list[n++] = i;
        }
    } else if (tab_ == TAB_MARKET) {
        listHead_->SetText("РЫНОК — ваши скины (" +
                           String(inventory_ ? inventory_->Count() : 0) + ")");
        for (int i = 0; i < SkinInv::CatalogSize() && n < kRows; ++i) {
            if (inventory_ && inventory_->Owns(i)) list[n++] = i;
        }
    } else {
        listHead_->SetText("АПГРЕЙДЕР — выберите ставку, затем цель");
        for (int i = 0; i < SkinInv::CatalogSize() && n < kRows; ++i) {
            if (SkinInv::Catalog(i).kind != kindNow) continue;
            list[n++] = i;
        }
    }

    for (int i = 0; i < kRows; ++i) {
        if (i >= n) {
            rowSkin_[i] = -1;
            if (rows_[i]) rows_[i]->SetVisible(false);
            continue;
        }
        const int skin = list[i];
        rowSkin_[i] = skin;
        const SkinInv::SkinInfo& si = SkinInv::Catalog(skin);
        const bool owned = inventory_ && inventory_->Owns(skin);
        const bool equipped = inventory_ && inventory_->Equipped(si.kind) == skin;
        const bool sel = (tab_ == TAB_SHOP && selShop_ == skin) ||
                         (tab_ == TAB_MARKET && selMarket_ == skin) ||
                         (tab_ == TAB_UPGRADE && (selUpStake_ == skin || selUpTarget_ == skin));
        const Color rc = RarityColor(si.rarity);

        String mark;
        if (equipped) mark = "  \xE2\x9C\x93";
        else if (owned) mark = "  \xE2\x80\xA2";
        rowTexts_[i]->SetText(String(si.name) + mark);
        rowTexts_[i]->SetColor(sel ? Color(1.0f, 0.90f, 0.42f, 1.0f)
                                   : Color(rc.r_ * 0.55f + 0.42f,
                                           rc.g_ * 0.55f + 0.42f,
                                           rc.b_ * 0.55f + 0.42f, 1.0f));
        // Тег справа: цена (магазин/апгрейдер) или выручка (рынок).
        String tag;
        if (tab_ == TAB_SHOP) tag = Money(SkinInv::CatalogPrice(skin));
        else if (tab_ == TAB_MARKET) tag = "+" + Money(inventory_ ? inventory_->SellValue(skin) : 0);
        else tag = Money(SkinInv::CatalogPrice(skin)) +
                   (owned ? String("") : String("  \xE2\x86\x91"));
        rowTags_[i]->SetText(tag);
        rowTags_[i]->SetColor(rc);
        rows_[i]->SetVisible(true);
        rows_[i]->SetColor(sel ? Color(0.16f, 0.14f, 0.06f, 0.96f)
                               : Color(0.05f, 0.07f, 0.08f, 0.92f));
    }

    // ── Карточка ─────────────────────────────────────────────────────────
    if (tab_ == TAB_SHOP) {
        const int skin = selShop_;
        if (skin < 0) {
            cardTitle_->SetText("МАГАЗИН");
            cardTitle_->SetColor(Color(1.0f, 0.94f, 0.80f, 1.0f));
            cardBody_->SetText("Слева — что можно купить. Клик по строке — выбор.\n\n"
                               "Магазин дороже рынка: перепродажа никогда не приносит\n"
                               "прибыли, иначе экономика начнёт крутиться сама.\n\n"
                               "Категорию переключают клавиши 7 / 8 / 9:\n"
                               "оружие, фишки казино, домино.");
            actionBtn_->SetVisible(false);
        } else {
            const SkinInv::SkinInfo& si = SkinInv::Catalog(skin);
            const bool owned = inventory_ && inventory_->Owns(skin);
            cardTitle_->SetText(si.name);
            cardTitle_->SetColor(RarityColor(si.rarity));
            String b;
            b += String(SkinEconomy::RarityName(si.rarity)) + "   \xC2\xB7   " +
                 String(SkinInv::KindName(si.kind)) + "\n\n";
            b += "Цена в магазине:  " + Money(SkinInv::CatalogPrice(skin)) + "\n";
            b += "Продать можно за:  " + Money(inventory_ ? inventory_->SellValue(skin) : 0) + "\n\n";
            b += owned ? "У вас уже есть этот скин." : "Нет в коллекции.";
            cardBody_->SetText(b);
            actionBtn_->SetVisible(true);
            actionBtn_->SetColor(owned ? Color(0.14f, 0.16f, 0.18f, 0.95f)
                                       : Color(0.12f, 0.40f, 0.24f, 0.95f));
            Text* t = actionBtn_->GetChildDynamicCast<Text>(0);
            if (t) t->SetText(owned ? "УЖЕ ЕСТЬ" : "КУПИТЬ");
        }
        hintText_->SetText("ЛКМ — выбрать и купить   |   7/8/9 — категория   |   Q/E — вкладки   |   ESC — закрыть");
        return;
    }

    if (tab_ == TAB_MARKET) {
        const int skin = selMarket_;
        if (skin < 0) {
            cardTitle_->SetText("РЫНОК");
            cardTitle_->SetColor(Color(1.0f, 0.94f, 0.80f, 1.0f));
            cardBody_->SetText("Слева — ваша коллекция. Продажа приносит 45-72% цены\n"
                               "в зависимости от редкости: чем реже скин, тем крепче цена.\n\n"
                               "Надетый скин продать нельзя — сначала наденьте другой.\n\n"
                               "Купить на рынке можно всё, что есть в магазине,\n"
                               "но дороже магазина на 15%.");
            actionBtn_->SetVisible(false);
        } else {
            const SkinInv::SkinInfo& si = SkinInv::Catalog(skin);
            const bool canSell = inventory_ && inventory_->CanSell(skin);
            cardTitle_->SetText(si.name);
            cardTitle_->SetColor(RarityColor(si.rarity));
            String b;
            b += String(SkinEconomy::RarityName(si.rarity)) + "   \xC2\xB7   " +
                 String(SkinInv::KindName(si.kind)) + "\n\n";
            b += "Выручка:  " + Money(inventory_ ? inventory_->SellValue(skin) : 0) + "\n";
            b += String("Доля от цены: ") +
                 Percent(SkinEconomy::SellRatio(si.rarity)) + "\n\n";
            b += canSell ? "Можно продать." : "Надет — продажа запрещена.";
            cardBody_->SetText(b);
            actionBtn_->SetVisible(true);
            actionBtn_->SetColor(canSell ? Color(0.40f, 0.24f, 0.10f, 0.95f)
                                         : Color(0.14f, 0.16f, 0.18f, 0.95f));
            Text* t = actionBtn_->GetChildDynamicCast<Text>(0);
            if (t) t->SetText(canSell ? "ПРОДАТЬ" : "НАДЕТ");
        }
        hintText_->SetText("ЛКМ — выбрать и продать   |   Q/E — вкладки   |   ESC — закрыть");
        return;
    }

    // ── Апгрейдер ────────────────────────────────────────────────────────
    {
        const int st = selUpStake_;
        const int tg = selUpTarget_;
        String b;
        b += "Ставка:  ";
        b += (st >= 0) ? String(SkinInv::Catalog(st).name) +
                         "  (" + Money(SkinInv::CatalogPrice(st)) + ")" : String("—");
        b += "\nЦель:    ";
        b += (tg >= 0) ? String(SkinInv::Catalog(tg).name) +
                         "  (" + Money(SkinInv::CatalogPrice(tg)) + ")" : String("—");
        b += "\n\n";

        bool ready = false;
        if (st >= 0 && tg >= 0) {
            const float p = SkinEconomy::UpgradeChance(SkinInv::CatalogPrice(st),
                                                       SkinInv::CatalogPrice(tg));
            const float ev = SkinEconomy::UpgradeExpectedValue(SkinInv::CatalogPrice(st),
                                                               SkinInv::CatalogPrice(tg));
            b += "Шанс:  " + Percent(p) + "\n";
            b += String("Средний исход:  ") + ((ev < 0) ? String("") : String("+")) +
                 Money((int)ev) + "\n\n";
            ready = true;
            if (inventory_ && !inventory_->CanSell(st)) { b += "Ставка надета — снимите её выбором другого скина.\n"; ready = false; }
            if (inventory_ && inventory_->Owns(tg)) { b += "Целевой скин уже есть.\n"; ready = false; }
            if (SkinInv::Catalog(st).kind != SkinInv::Catalog(tg).kind) {
                b += "Разные категории — апгрейд невозможен.\n";
                ready = false;
            }
        } else {
            b += "Кликните скин в списке: первый клик — ставка, второй — цель.\n"
                 "Ставка сгорает, если бросок не попал в шанс.\n\n"
                 "Апгрейдер не выгоден математически (дом забирает 30-45% ставки),\n"
                 "но даёт шанс поднять дешёвый скин до легенды.";
        }
        if (lastUpgradeDone_) {
            b += "\nПрошлый апгрейд:  ";
            b += lastUpgradeWin_ ? "УДАЛСЯ" : "не попал";
        }
        cardTitle_->SetText("АПГРЕЙДЕР");
        cardTitle_->SetColor(Color(0.98f, 0.78f, 0.36f, 1.0f));
        cardBody_->SetText(b);
        actionBtn_->SetVisible(true);
        actionBtn_->SetColor(ready ? Color(0.42f, 0.20f, 0.42f, 0.95f)
                                   : Color(0.14f, 0.16f, 0.18f, 0.95f));
        Text* t = actionBtn_->GetChildDynamicCast<Text>(0);
        if (t) t->SetText(ready ? "АПГРЕЙД" : "ВЫБЕРИТЕ");

        hintText_->SetText("ЛКМ — ставка/цель   |   ПРОБЕЛ — крутить   |   C — сброс   |   8/9 — категория   |   ESC — закрыть");
    }
}

void Panel::DoBuy() {
    if (!inventory_ || selShop_ < 0) return;
    const SkinInv::BuyResult r = SkinInv::Buy(*inventory_, wallet_, selShop_);
    if (r.ok) {
        wallet_ = r.newBalance;
        message_ = String("Куплено: ") + SkinInv::Catalog(selShop_).name;
        selShop_ = -1;
    } else {
        message_ = r.reason && *r.reason ? String(r.reason) : String("нельзя");
    }
    Refresh();
}

void Panel::DoSell() {
    if (!inventory_ || selMarket_ < 0) return;
    const SkinInv::BuyResult r = SkinInv::Sell(*inventory_, wallet_, selMarket_);
    if (r.ok) {
        wallet_ = r.newBalance;
        message_ = String("Продано за ") + Money(r.price);
        selMarket_ = -1;
    } else {
        message_ = r.reason && *r.reason ? String(r.reason) : String("нельзя");
    }
    Refresh();
}

void Panel::DoUpgrade() {
    if (!inventory_) return;
    if (selUpStake_ < 0 || selUpTarget_ < 0) return;
    // Бросок делаем сразу, а применяем после анимации: иначе игрок увидит
    // результат до того, как «крутилка» докрутится.
    pendStake_ = selUpStake_;
    pendTarget_ = selUpTarget_;
    pendRoll_ = Random(1.0f);
    pendingUpgrade_ = true;
    message_ = "Крутим...";
    Refresh();
}

void Panel::DoOpenCase() {
    if (!inventory_) return;
    const int ci = selCase_;
    const SkinInv::CaseResult r = SkinInv::OpenCase(*inventory_, wallet_, ci, caseKind_,
                                                    streak_, Random(1.0f), Random(1.0f));
    if (r.ok) {
        wallet_ = r.newBalance;
        streak_ = r.newStreak;
        lastCaseDrop_ = r.dropped;
        lastCaseRarity_ = r.rarity;
        if (r.dropped >= 0) {
            message_ = String("Выпало: ") + SkinInv::Catalog(r.dropped).name;
        } else {
            message_ = "Дубль — компенсация зачислена";
        }
    } else {
        message_ = r.reason && *r.reason ? String(r.reason) : String("нельзя");
    }
    Refresh();
}

} // namespace SkinShopUI
