#pragma once
// ============================================================================
// CasinoSim.h — безопасная внутриигровая рулетка для desktop-клиента.
// Только виртуальные кредиты: реальные деньги, платежи и внешние сервисы
// намеренно отсутствуют. Чистые правила вынесены сюда, чтобы тестировать их
// отдельно от Urho3D UI.
// ============================================================================

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Casino {

enum BetType : int {
    BET_RED = 0,
    BET_BLACK,
    BET_EVEN,
    BET_ODD,
    BET_LOW,
    BET_HIGH,
    BET_NUMBER,
    BET_DOZEN,
    BET_COLUMN,
    BET_SPLIT,
    BET_STREET,
    BET_CORNER,
    BET_SIXLINE,
    BET_COUNT
};

struct Bet {
    BetType type = BET_RED;
    int amount = 25;
    int number = 7; // число или опорный сектор для сложной ставки, диапазон 0..36
};

inline bool IsRed(int number) {
    switch (number) {
        case 1: case 3: case 5: case 7: case 9: case 12: case 14: case 16: case 18:
        case 19: case 21: case 23: case 25: case 27: case 30: case 32: case 34: case 36:
            return true;
        default:
            return false;
    }
}

inline bool IsBlack(int number) {
    return number > 0 && !IsRed(number);
}

inline const char* BetName(BetType type) {
    switch (type) {
        case BET_RED:    return "КРАСНОЕ";
        case BET_BLACK:  return "ЧЁРНОЕ";
        case BET_EVEN:   return "ЧЁТ";
        case BET_ODD:    return "НЕЧЁТ";
        case BET_LOW:    return "1–18";
        case BET_HIGH:   return "19–36";
        case BET_NUMBER: return "ЧИСЛО";
        case BET_DOZEN:  return "ДЮЖИНА";
        case BET_COLUMN: return "КОЛОНКА";
        case BET_SPLIT:  return "СПЛИТ";
        case BET_STREET: return "СТРИТ";
        case BET_CORNER: return "КОРНЕР";
        case BET_SIXLINE:return "СИКС-ЛАЙН";
        default:         return "СТАВКА";
    }
}

inline const char* ColorName(int number) {
    if (number == 0) return "ЗЕРО";
    return IsRed(number) ? "КРАСНОЕ" : "ЧЁРНОЕ";
}

inline int PayoutMultiplier(BetType type) {
    switch (type) {
        case BET_NUMBER: return 36;
        case BET_SPLIT:  return 18;
        case BET_STREET: return 12;
        case BET_CORNER: return 9; // 8:1 profit plus the original stake
        case BET_SIXLINE:return 6;
        case BET_DOZEN:
        case BET_COLUMN: return 3;
        default:         return 2;
    }
}

inline bool Wins(const Bet& bet, int result) {
    if (result < 0 || result > 36) return false;
    switch (bet.type) {
        case BET_RED:    return IsRed(result);
        case BET_BLACK:  return IsBlack(result);
        case BET_EVEN:   return result != 0 && result % 2 == 0;
        case BET_ODD:    return result % 2 == 1;
        case BET_LOW:    return result >= 1 && result <= 18;
        case BET_HIGH:   return result >= 19 && result <= 36;
        case BET_NUMBER: return result == std::max(0, std::min(36, bet.number));
        case BET_DOZEN: {
            int dozen = std::max(1, std::min(3, (std::max(1, std::min(36, bet.number)) + 11) / 12));
            return result >= 1 && result <= 36 && ((result - 1) / 12 + 1) == dozen;
        }
        case BET_COLUMN: {
            int column = ((std::max(1, std::min(36, bet.number)) - 1) % 3) + 1;
            return result >= 1 && result <= 36 && ((result - 1) % 3 + 1) == column;
        }
        case BET_SPLIT: {
            int n = std::max(1, std::min(36, bet.number));
            if (n % 3 == 0) --n;
            if (n >= 36) n = 35;
            return result == n || result == n + 1;
        }
        case BET_STREET: {
            int base = ((std::max(1, std::min(36, bet.number)) - 1) / 3) * 3 + 1;
            return result >= base && result <= base + 2;
        }
        case BET_CORNER: {
            int n = std::max(1, std::min(36, bet.number));
            int base = ((n - 1) / 3) * 3 + 1 + std::min((n - 1) % 3, 1);
            base = std::max(1, std::min(32, base));
            return result == base || result == base + 1 || result == base + 3 || result == base + 4;
        }
        case BET_SIXLINE: {
            int base = ((std::max(1, std::min(36, bet.number)) - 1) / 6) * 6 + 1;
            return result >= base && result <= base + 5;
        }
        default:         return false;
    }
}

inline int Payout(const Bet& bet, int result) {
    const int amount = std::max(0, bet.amount);
    return Wins(bet, result) ? amount * PayoutMultiplier(bet.type) : 0;
}

inline int Net(const Bet& bet, int result) {
    return Payout(bet, result) - std::max(0, bet.amount);
}

inline int ClampNumber(int number) {
    return std::max(0, std::min(36, number));
}

inline int NextChipAmount(int current, int direction) {
    static const int kChips[] = { 10, 25, 50, 100, 250, 500 };
    int idx = 0;
    for (int i = 0; i < static_cast<int>(sizeof(kChips) / sizeof(kChips[0])); ++i) {
        if (kChips[i] == current) { idx = i; break; }
        if (kChips[i] < current) idx = i;
    }
    idx = std::max(0, std::min(static_cast<int>(sizeof(kChips) / sizeof(kChips[0])) - 1, idx + (direction < 0 ? -1 : 1)));
    return kChips[idx];
}

// --- «КИБЕР-СЛОТЫ 777» (перенос из веб-казино в нативный C++17) ---
enum SlotSymbol : int {
    SYM_CHERRY = 0,
    SYM_ROOSTER,
    SYM_DIAMOND,
    SYM_STAR,
    SYM_CROWN,
    SYM_SEVEN,
    SYM_COUNT
};

inline const char* SlotLabel(int sym) {
    switch (sym) {
        case SYM_CHERRY:  return "[ВИШНЯ]";
        case SYM_ROOSTER: return "[КОЧЕТ]";
        case SYM_DIAMOND: return "[АЛМАЗ]";
        case SYM_STAR:    return "[ЗВЕЗДА]";
        case SYM_CROWN:   return "[КОРОНА]";
        case SYM_SEVEN:   return "[ 777 ]";
        default:          return "[  ?  ]";
    }
}

inline int SlotsMultiplier(int r0, int r1, int r2) {
    if (r0 == r1 && r1 == r2) {
        switch (r0) {
            case SYM_SEVEN:   return 50;
            case SYM_CROWN:   return 20;
            case SYM_STAR:    return 12;
            case SYM_DIAMOND: return 8;
            case SYM_ROOSTER: return 6;
            default:          return 4;
        }
    }
    if (r0 == r1 || r1 == r2 || r0 == r2) {
        int pairSym = (r0 == r1 || r0 == r2) ? r0 : r1;
        return (pairSym == SYM_SEVEN || pairSym == SYM_CROWN) ? 3 : 2;
    }
    return 0;
}

inline int SlotsPayout(int betAmount, int r0, int r1, int r2) {
    return std::max(0, betAmount) * SlotsMultiplier(r0, r1, r2);
}

// --- Настоящий порядок 37 секторов европейского 3D-колеса рулетки ---
inline constexpr int kEuropeanWheelOrder[37] = {
    0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10,
    5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26
};

inline int WheelIndexForNumber(int number) {
    for (int i = 0; i < 37; ++i) {
        if (kEuropeanWheelOrder[i] == number) return i;
    }
    return 0;
}

inline float WheelSectorAngleDeg(int wheelIdx) {
    int idx = ((wheelIdx % 37) + 37) % 37;
    return static_cast<float>(idx) * (360.0f / 37.0f);
}

inline const char* RoflStatusTitle(int balance) {
    if (balance <= 0)      return "Банкрот на бумаге (жмите +5000 ¤)";
    if (balance < 500)     return "Карманник с одной фишкой";
    if (balance < 2500)    return "Постоялец подпольного клуба";
    if (balance < 10000)   return "Туз в фантиках";
    if (balance < 50000)   return "Миллионер понарошку";
    if (balance < 250000)  return "Магнат картонных слитков";
    return "Верховный Шейх Рофл-Рояля";
}

inline const char* RoflDisclaimerShort() {
    return "100% РОФЛ • ВСЁ ПОНАРОШКУ • БЕЗ РЕАЛЬНЫХ ДЕНЕГ, ПЛАТЕЖЕЙ И ВЫВОДА";
}

// ============================================================================
// НАСТОЯЩИЙ СТОЛ РУЛЕТКИ (европейский layout 0–36 + внешние ставки)
// Игрок кликает по ячейке стола «как в настоящем» — зона сразу превращается
// в Bet и участвует в обычном Payout(). Чистая логика, без Urho3D.
// ============================================================================
struct TableZone {
    BetType type;
    int     number;   // для BET_NUMBER — число, для дюжин/колонок — якорь
    const char* label;
};

inline const TableZone (&GetTableZones())[49] {
    static const TableZone kZones[49] = {
        { BET_NUMBER, 0,  "0"  }, { BET_NUMBER, 1,  "1"  }, { BET_NUMBER, 2,  "2"  },
        { BET_NUMBER, 3,  "3"  }, { BET_NUMBER, 4,  "4"  }, { BET_NUMBER, 5,  "5"  },
        { BET_NUMBER, 6,  "6"  }, { BET_NUMBER, 7,  "7"  }, { BET_NUMBER, 8,  "8"  },
        { BET_NUMBER, 9,  "9"  }, { BET_NUMBER, 10, "10" }, { BET_NUMBER, 11, "11" },
        { BET_NUMBER, 12, "12" }, { BET_NUMBER, 13, "13" }, { BET_NUMBER, 14, "14" },
        { BET_NUMBER, 15, "15" }, { BET_NUMBER, 16, "16" }, { BET_NUMBER, 17, "17" },
        { BET_NUMBER, 18, "18" }, { BET_NUMBER, 19, "19" }, { BET_NUMBER, 20, "20" },
        { BET_NUMBER, 21, "21" }, { BET_NUMBER, 22, "22" }, { BET_NUMBER, 23, "23" },
        { BET_NUMBER, 24, "24" }, { BET_NUMBER, 25, "25" }, { BET_NUMBER, 26, "26" },
        { BET_NUMBER, 27, "27" }, { BET_NUMBER, 28, "28" }, { BET_NUMBER, 29, "29" },
        { BET_NUMBER, 30, "30" }, { BET_NUMBER, 31, "31" }, { BET_NUMBER, 32, "32" },
        { BET_NUMBER, 33, "33" }, { BET_NUMBER, 34, "34" }, { BET_NUMBER, 35, "35" },
        { BET_NUMBER, 36, "36" },
        { BET_RED,    0,  "КРАСНОЕ" }, { BET_BLACK, 0, "ЧЁРНОЕ" },
        { BET_EVEN,   0,  "ЧЁТ"     }, { BET_ODD,   0, "НЕЧЁТ"  },
        { BET_LOW,    0,  "1–18"    }, { BET_HIGH,  0, "19–36"  },
        { BET_DOZEN,  1,  "1-Я ДЮЖ." }, { BET_DOZEN, 13, "2-Я ДЮЖ." },
        { BET_DOZEN,  25, "3-Я ДЮЖ." },
        { BET_COLUMN, 1,  "КОЛ.1"   }, { BET_COLUMN, 2, "КОЛ.2"   },
        { BET_COLUMN, 3,  "КОЛ.3"   }
    };
    return kZones;
}

inline constexpr int kTableZoneCount = 49;

inline Bet BetForTableZone(int zoneIdx, int amount) {
    int idx = std::max(0, std::min(kTableZoneCount - 1, zoneIdx));
    const TableZone& z = GetTableZones()[idx];
    Bet b;
    b.type = z.type;
    b.amount = std::max(0, amount);
    b.number = z.number;
    return b;
}

inline int TableZoneForNumber(int number) {
    return ClampNumber(number); // зоны 0..36 совпадают с числами
}

inline bool TableZoneIsRed(int zoneIdx) {
    if (zoneIdx < 0 || zoneIdx > 36) return false;
    return IsRed(zoneIdx);
}

// Pending chips are reserved logically; the wallet is debited only at Begin.
// A locked snapshot cannot be edited or paid twice.
class RouletteRound {
public:
    std::vector<Bet> chips;
    bool locked = false;
    int Total() const { int n = 0; for (const auto& b : chips) n += b.amount; return n; }
    bool Add(const Bet& b, int balance) {
        if (locked || b.amount <= 0 || b.amount > 1000000 || chips.size() >= 512 ||
            b.type < BET_RED || b.type >= BET_COUNT || b.number < 0 || b.number > 36 ||
            Total() > std::min(balance, 1000000) - b.amount) return false;
        chips.push_back(b); return true;
    }
    void Undo() { if (!locked && !chips.empty()) chips.pop_back(); }
    void Clear() { if (!locked) chips.clear(); }
    bool Begin(int& balance) {
        if (locked || chips.empty() || Total() > balance) return false;
        balance -= Total(); locked = true; return true;
    }
    int Settle(int result) {
        if (!locked || result < 0 || result > 36) return 0;
        int payout = 0;
        for (const auto& b : chips) payout += Payout(b, result);
        chips.clear(); locked = false;
        return payout;
    }
    int OnZone(int zone) const {
        const Bet target = BetForTableZone(zone, 1);
        int n = 0;
        for (const auto& b : chips)
            if (b.type == target.type && (b.type < BET_NUMBER || b.number == target.number)) n += b.amount;
        return n;
    }
};

// ============================================================================
// БЛЭКДЖЕК «КОЧЕТ-21» — второй настоящий стол казино (чистая логика).
// Колода 52 карты, дилер добирает до 17, блэкджек платит 3:2, дабл — удвоение.
// ============================================================================
struct Card {
    int rank = 1; // 1=Туз, 11=Валет, 12=Дама, 13=Король
    int suit = 0; // 0..3
};

inline int CardValue(int rank) {
    if (rank == 1) return 11;
    if (rank >= 10) return 10;
    return std::max(2, std::min(10, rank));
}

inline int HandValue(const Card* cards, int count) {
    int total = 0, aces = 0;
    for (int i = 0; i < count; ++i) {
        total += CardValue(cards[i].rank);
        if (cards[i].rank == 1) ++aces;
    }
    while (total > 21 && aces > 0) { total -= 10; --aces; }
    return total;
}

inline bool IsSoftHand(const Card* cards, int count) {
    int total = 0, aces = 0;
    for (int i = 0; i < count; ++i) {
        total += CardValue(cards[i].rank);
        if (cards[i].rank == 1) ++aces;
    }
    while (total > 21 && aces > 0) { total -= 10; --aces; }
    return aces > 0 && total <= 21;
}

inline bool IsBlackjack(const Card* cards, int count) {
    return count == 2 && HandValue(cards, count) == 21;
}

inline bool DealerMustHit(const Card* cards, int count) {
    return HandValue(cards, count) < 17;
}

inline const char* CardLabel(int rank) {
    switch (rank) {
        case 1:  return "Т";
        case 11: return "В";
        case 12: return "Д";
        case 13: return "К";
        default: return "?";
    }
}

// Возврат игроку (включая ставку): 0 — проигрыш, bet — пуш, 2*bet — победа,
// 2*bet + bet/2 — блэкджек 3:2. Дабл уже учтён в betAmount вызывающим кодом.
inline int BlackjackPayout(int betAmount, const Card* player, int pCount,
                           const Card* dealer, int dCount) {
    int bet = std::max(0, betAmount);
    int pv = HandValue(player, pCount);
    int dv = HandValue(dealer, dCount);
    bool pBj = IsBlackjack(player, pCount);
    bool dBj = IsBlackjack(dealer, dCount);
    if (pv > 21) return 0;
    if (pBj && dBj) return bet;
    if (pBj) return bet * 2 + bet / 2;
    if (dBj) return 0;
    if (dv > 21) return bet * 2;
    if (pv > dv) return bet * 2;
    if (pv == dv) return bet;
    return 0;
}

inline void MakeDeck(Card out[52]) {
    int idx = 0;
    for (int s = 0; s < 4; ++s)
        for (int r = 1; r <= 13; ++r)
            out[idx++] = Card{r, s};
}

inline void ShuffleDeck(Card* deck, int count, uint32_t& rngState) {
    if (!deck || count < 2) return;
    if (rngState == 0) rngState = 0xB1ACu;
    for (int i = count - 1; i > 0; --i) {
        uint32_t x = rngState;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        rngState = x ? x : 0xB1ACu;
        int j = (int)(rngState % (uint32_t)(i + 1));
        Card tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
    }
}

} // namespace Casino
