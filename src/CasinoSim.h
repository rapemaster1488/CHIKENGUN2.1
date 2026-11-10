#pragma once
// ============================================================================
// CasinoSim.h — безопасная внутриигровая рулетка для desktop-клиента.
// Только виртуальные кредиты: реальные деньги, платежи и внешние сервисы
// намеренно отсутствуют. Чистые правила вынесены сюда, чтобы тестировать их
// отдельно от Urho3D UI.
// ============================================================================

#include <algorithm>

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
        case BET_CORNER: return 8;
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

} // namespace Casino
