#pragma once

// SkinShopUI.h — экран магазина / рынка / апгрейдера / кейсов.
//
// ПОЧЕМУ ОТДЕЛЬНЫЙ ФАЙЛ. MyApp.cpp уже 20 тысяч строк, и добавление четырёх
// вкладок с двумя сотнями элементов прямо туда сделало бы его неподъёмным.
// Здесь только UI: раскладка, отрисовка строк, реакция на клик. Вся логика
// и деньги — в SkinInventory.h / SkinEconomy.h, которые проверены тестами.
//
// ПАНЕЛЬ НЕ ЗНАЕТ ПРО CASINO. Она не трогает камеру, стол, деньги казино и
// ввод — ей передают кошелёк и колбэк «деньги изменились». Поэтому магазин
// можно открыть и из меню, и из казино, и из арсенала, не ломая каждый раз
// сцену.
//
// РАСКЛАДКА. Полоса вкладок сверху, слева список скинов, справа карточка
// выбранного с кнопкой действия. Всё монохромно-тёплое, как казино, без
// неона: игрок просил «ультра красиво, но без выёбона».
#include <Urho3D/Core/Context.h>
#include <Urho3D/Urho3D.h>
#include <Urho3D/Container/Str.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/UI/BorderImage.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#include "SkinInventory.h"

namespace SkinShopUI {

enum Tab {
    TAB_SHOP = 0,     // купить в магазине
    TAB_MARKET,       // продать / рынок
    TAB_UPGRADE,      // апгрейдер
    TAB_CASES,        // кейсы
    TAB_COUNT
};

inline const char* TabName(int t) {
    switch (t) {
    case TAB_SHOP:    return "МАГАЗИН";
    case TAB_MARKET:  return "РЫНОК";
    case TAB_UPGRADE: return "АПГРЕЙДЕР";
    case TAB_CASES:   return "КЕЙСЫ";
    default:          return "?";
    }
}

// Цвет тира — для полоски и названия. Тёплые, не кислотные.
inline Urho3D::Color RarityColor(int r) {
    switch (r) {
    case SkinEconomy::RAR_COMMON:   return Urho3D::Color(0.62f, 0.64f, 0.68f, 1.0f);
    case SkinEconomy::RAR_UNCOMMON: return Urho3D::Color(0.46f, 0.78f, 0.52f, 1.0f);
    case SkinEconomy::RAR_RARE:     return Urho3D::Color(0.36f, 0.62f, 0.94f, 1.0f);
    case SkinEconomy::RAR_EPIC:     return Urho3D::Color(0.72f, 0.44f, 0.92f, 1.0f);
    case SkinEconomy::RAR_LEGEND:   return Urho3D::Color(0.98f, 0.76f, 0.26f, 1.0f);
    default:                        return Urho3D::Color::WHITE;
    }
}

class Panel {
public:
    Panel(Urho3D::Context* ctx, Urho3D::UIElement* root, Urho3D::Font* font,
          float uiScale)
        : ctx_(ctx), root_(root), font_(font), scale_(uiScale),
          inventory_(nullptr), wallet_(0), open_(false),
          tab_(TAB_SHOP), selShop_(-1), selMarket_(-1), selUpStake_(-1),
          selUpTarget_(-1), selCase_(0), lastCaseDrop_(-1), lastCaseRarity_(-1),
          lastUpgradeWin_(false), lastUpgradeDone_(false), streak_(0),
          caseKind_(SkinInv::KIND_WEAPON), message_() {
        Build();
    }

    // Связываем с инвентарём и кошельком. Кошелёк копируется: панель не
    // владеет деньгами, она возвращает новое значение через Wallet().
    void Bind(SkinInv::Inventory* inv, int wallet) {
        inventory_ = inv;
        wallet_ = wallet;
        Refresh();
    }

    int  Wallet() const { return wallet_; }
    void SetWallet(int w) { wallet_ = w < 0 ? 0 : w; Refresh(); }

    bool IsOpen() const { return open_; }
    int  Tab() const { return tab_; }

    void Show(bool on) {
        open_ = on;
        if (panel_) panel_->SetVisible(on);
        if (on) Refresh();
    }

    void SetTab(int t) {
        if (t < 0 || t >= TAB_COUNT) return;
        tab_ = t;
        // Таб сохраняется: игрок возвращается туда, где был.
        for (int i = 0; i < TAB_COUNT; ++i) {
            if (tabButtons_[i]) {
                tabButtons_[i]->SetColor(i == tab_
                    ? Urho3D::Color(0.42f, 0.34f, 0.12f, 0.95f)
                    : Urho3D::Color(0.10f, 0.12f, 0.13f, 0.92f));
            }
        }
        Refresh();
    }

    void CycleTab(int dir) { SetTab((tab_ + dir + TAB_COUNT) % TAB_COUNT); }

    // Категория скинов (7/8/9 в игре): оружие / фишки / домино.
    void SetCaseKind(int k) {
        if (k < 0 || k >= SkinInv::KIND_COUNT) return;
        caseKind_ = k;
        selShop_ = selMarket_ = selUpStake_ = selUpTarget_ = -1;
        Refresh();
    }
    int CaseKind() const { return caseKind_; }

    // Сброс выбора на апгрейдере (C).
    void ClearSelection() {
        selUpStake_ = -1;
        selUpTarget_ = -1;
        Refresh();
    }

    // Действие текущей вкладки (SPACE/ENTER): ровно то, что делает кнопка.
    void Activate() {
        switch (tab_) {
        case TAB_SHOP:    DoBuy(); break;
        case TAB_MARKET:  DoSell(); break;
        case TAB_UPGRADE: DoUpgrade(); break;
        case TAB_CASES:   DoOpenCase(); break;
        default: break;
        }
    }

    // Клик по экрану. Возвращает true, если панель поглотила клик.
    bool HandleClick(int mx, int my) {
        if (!open_ || !panel_) return false;
        for (int i = 0; i < TAB_COUNT; ++i) {
            if (Hit(tabButtons_[i], mx, my)) { SetTab(i); return true; }
        }
        if (Hit(closeBtn_, mx, my)) { Show(false); return true; }

        if (tab_ == TAB_CASES) {
            for (int i = 0; i < 3; ++i) {
                if (Hit(caseBtns_[i], mx, my)) { selCase_ = i; Refresh(); return true; }
            }
            if (Hit(openCaseBtn_, mx, my)) { DoOpenCase(); return true; }
            return true;
        }

        // Строки списка.
        for (int i = 0; i < kRows; ++i) {
            if (!rows_[i] || !rows_[i]->IsVisible()) continue;
            if (!Hit(rows_[i], mx, my)) continue;
            const int skin = rowSkin_[i];
            if (tab_ == TAB_SHOP) selShop_ = skin;
            else if (tab_ == TAB_MARKET) selMarket_ = skin;
            else if (tab_ == TAB_UPGRADE) {
                // Первый клик — ставка, второй — цель (если другого типа).
                if (selUpStake_ < 0 || selUpStake_ == skin) selUpStake_ = skin;
                else selUpTarget_ = skin;
            }
            Refresh();
            return true;
        }

        if (tab_ == TAB_SHOP && Hit(actionBtn_, mx, my)) { DoBuy(); return true; }
        if (tab_ == TAB_MARKET && Hit(actionBtn_, mx, my)) { DoSell(); return true; }
        if (tab_ == TAB_UPGRADE) {
            if (Hit(actionBtn_, mx, my)) { DoUpgrade(); return true; }
            if (Hit(clearBtn_, mx, my)) { selUpStake_ = -1; selUpTarget_ = -1; Refresh(); return true; }
        }
        return true;   // панель модальна внутри себя
    }

    // Сбр��с «анимации» апгрейда: UI вызывает это, когда доиграла
    // кинематика (стрелка прокрутилась).
    void ApplyPendingUpgrade() {
        if (!pendingUpgrade_) return;
        pendingUpgrade_ = false;
        Refresh();
    }

    bool HasPendingUpgrade() const { return pendingUpgrade_; }

    // Финальная выплата апгрейда — вызывается после анимации, чтобы деньги
    // менялись ровно один раз и ровно в момент показа результата.
    void CommitUpgrade() {
        if (!pendingUpgrade_ || !inventory_) return;
        const SkinInv::UpgradeResult ur =
            SkinInv::Upgrade(*inventory_, wallet_, pendStake_, pendTarget_, pendRoll_);
        pendingUpgrade_ = false;
        if (ur.ok) {
            lastUpgradeWin_ = ur.win;
            lastUpgradeDone_ = true;
            wallet_ = ur.newBalance;
            selUpStake_ = -1;
            selUpTarget_ = -1;
            message_ = ur.win ? "АПГРЕЙД УДАЛСЯ" : "СТАВКА СГОРЕЛА";
        } else {
            message_ = ur.reason && *ur.reason ? Urho3D::String(ur.reason) : Urho3D::String("нельзя");
        }
        Refresh();
    }

    // Прокрутить «барабан» апгрейда (0..1) — UI показывает шанс.
    void SetUpgradeSpin(float) {}

    Urho3D::String Message() const { return message_; }

private:
    static const int kRows = 12;

    int S(int v) const { return (int)((float)v * scale_ + 0.5f); }

    static bool Hit(Urho3D::UIElement* el, int mx, int my) {
        if (!el || !el->IsVisible() || !el->IsEnabled()) return false;
        Urho3D::IntVector2 p;
        for (Urho3D::UIElement* e = el; e; e = e->GetParent()) {
            if (!e->IsVisible()) return false;
            p += e->GetPosition();
        }
        const Urho3D::IntVector2 sz = el->GetSize();
        return mx >= p.x_ && mx < p.x_ + sz.x_ && my >= p.y_ && my < p.y_ + sz.y_;
    }

    Urho3D::BorderImage* Btn(Urho3D::UIElement* parent, int x, int y, int w, int h,
                             const Urho3D::String& label, const Urho3D::Color& color,
                             int fontSz = 15) {
        Urho3D::BorderImage* b = parent->CreateChild<Urho3D::BorderImage>();
        b->SetPosition(x, y);
        b->SetSize(w, h);
        b->SetColor(color);
        Urho3D::Text* t = b->CreateChild<Urho3D::Text>();
        t->SetFont(font_, S(fontSz));
        t->SetText(label);
        t->SetTextAlignment(Urho3D::HA_CENTER);
        t->SetPosition(S(4), (h - S(fontSz)) / 2);
        t->SetWidth(w - S(8));
        t->SetColor(Urho3D::Color(1.0f, 0.95f, 0.84f, 1.0f));
        return b;
    }

    void Build();
    void Refresh();
    void DoBuy();
    void DoSell();
    void DoUpgrade();
    void DoOpenCase();
    int  FirstOfKind(int kind) const;

    Urho3D::Context* ctx_;
    Urho3D::UIElement* root_;
    Urho3D::Font* font_;
    float scale_;

    SkinInv::Inventory* inventory_;
    int wallet_;
    bool open_;
    int tab_;
    int selShop_;
    int selMarket_;
    int selUpStake_;
    int selUpTarget_;
    int selCase_;
    int lastCaseDrop_;
    int lastCaseRarity_;
    bool lastUpgradeWin_;
    bool lastUpgradeDone_;
    int streak_;
    int caseKind_;
    Urho3D::String message_;

    // Ожидающий апгрейд (исход уже решён броском, деньги ещё нет).
    bool pendingUpgrade_ = false;
    int pendStake_ = -1;
    int pendTarget_ = -1;
    float pendRoll_ = 0.0f;

    Urho3D::SharedPtr<Urho3D::UIElement> panel_;
    Urho3D::SharedPtr<Urho3D::BorderImage> tabButtons_[TAB_COUNT];
    Urho3D::SharedPtr<Urho3D::BorderImage> closeBtn_;
    Urho3D::SharedPtr<Urho3D::BorderImage> actionBtn_;
    Urho3D::SharedPtr<Urho3D::BorderImage> clearBtn_;
    Urho3D::SharedPtr<Urho3D::BorderImage> caseBtns_[3];
    Urho3D::SharedPtr<Urho3D::BorderImage> openCaseBtn_;
    Urho3D::SharedPtr<Urho3D::UIElement> rows_[kRows];
    Urho3D::SharedPtr<Urho3D::Text> rowTexts_[kRows];
    Urho3D::SharedPtr<Urho3D::Text> rowTags_[kRows];
    int rowSkin_[kRows];

    Urho3D::SharedPtr<Urho3D::Text> titleText_;
    Urho3D::SharedPtr<Urho3D::Text> walletText_;
    Urho3D::SharedPtr<Urho3D::Text> cardTitle_;
    Urho3D::SharedPtr<Urho3D::Text> cardBody_;
    Urho3D::SharedPtr<Urho3D::Text> hintText_;
    Urho3D::SharedPtr<Urho3D::Text> listHead_;
};

} // namespace SkinShopUI
