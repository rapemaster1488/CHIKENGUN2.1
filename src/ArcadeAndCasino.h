#pragma once
// ============================================================================
// ArcadeAndCasino.h — Нативный C++17 порт КАЗИНО «ЗОЛОТАЯ РУЛЕТКА + СЛОТЫ 777»
// (из casino.html) и зала «PIXEL ARCADE — 6 МИНИ-ИГР» (из minigames.html:
// 01 Змейка, 02 Pong, 03 Арканоид, 04 Flappy, 05 Dino Run, 06 Тетрис).
// Рендерит чёткую пиксельную картинку в RGBA8 буфер 320x220 (для Texture2D с
// FILTER_NEAREST в Urho3D UI) и интегрирован с кошельком кредитов игрока (₵).
// ============================================================================

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace MiniHub {

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 220;

struct RGB {
    uint8_t r, g, b;
};

// ============================================================================
// 1. ПИКСЕЛЬНЫЙ ХОЛСТ 320x220 RGBA8 + МИНИ-ШРИФТ 3x5
// ============================================================================
class PixelCanvas {
public:
    std::array<uint8_t, SCREEN_W * SCREEN_H * 4> data{};

    void Clear(uint8_t r, uint8_t g, uint8_t b) {
        for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
            data[i * 4 + 0] = r;
            data[i * 4 + 1] = g;
            data[i * 4 + 2] = b;
            data[i * 4 + 3] = 255;
        }
    }

    inline void Put(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if ((unsigned)x >= (unsigned)SCREEN_W || (unsigned)y >= (unsigned)SCREEN_H) return;
        int idx = (y * SCREEN_W + x) * 4;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
        data[idx + 3] = 255;
    }

    void FillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
        int x0 = std::max(0, x), y0 = std::max(0, y);
        int x1 = std::min(SCREEN_W, x + w), y1 = std::min(SCREEN_H, y + h);
        for (int py = y0; py < y1; ++py) {
            int row = py * SCREEN_W * 4;
            for (int px = x0; px < x1; ++px) {
                int idx = row + px * 4;
                data[idx + 0] = r;
                data[idx + 1] = g;
                data[idx + 2] = b;
                data[idx + 3] = 255;
            }
        }
    }

    void DrawBorder(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
        FillRect(x, y, w, 1, r, g, b);
        FillRect(x, y + h - 1, w, 1, r, g, b);
        FillRect(x, y, 1, h, r, g, b);
        FillRect(x + w - 1, y, 1, h, r, g, b);
    }

    void FillCircle(int cx, int cy, int rad, uint8_t r, uint8_t g, uint8_t b) {
        int r2 = rad * rad;
        for (int dy = -rad; dy <= rad; ++dy) {
            for (int dx = -rad; dx <= rad; ++dx) {
                if (dx * dx + dy * dy <= r2) Put(cx + dx, cy + dy, r, g, b);
            }
        }
    }

    void DrawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (int guard = 0; guard < 600; ++guard) {
            Put(x0, y0, r, g, b);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Компактный глиф 3x5 (15 бит) для цифр и латинских меток на CRT-экране
    static uint16_t Glyph3x5(char c) {
        if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
        switch (c) {
            case '0': return 0b111'101'101'101'111;
            case '1': return 0b010'110'010'010'111;
            case '2': return 0b111'001'111'100'111;
            case '3': return 0b111'001'111'001'111;
            case '4': return 0b101'101'111'001'001;
            case '5': return 0b111'100'111'001'111;
            case '6': return 0b111'100'111'101'111;
            case '7': return 0b111'001'001'010'010;
            case '8': return 0b111'101'111'101'111;
            case '9': return 0b111'101'111'001'111;
            case 'A': return 0b010'101'111'101'101;
            case 'B': return 0b110'101'110'101'110;
            case 'C': return 0b111'100'100'100'111;
            case 'D': return 0b110'101'101'101'110;
            case 'E': return 0b111'100'110'100'111;
            case 'F': return 0b111'100'110'100'100;
            case 'G': return 0b111'100'101'101'111;
            case 'H': return 0b101'101'111'101'101;
            case 'I': return 0b111'010'010'010'111;
            case 'J': return 0b001'001'001'101'111;
            case 'K': return 0b101'101'110'101'101;
            case 'L': return 0b100'100'100'100'111;
            case 'M': return 0b101'111'101'101'101;
            case 'N': return 0b111'101'101'101'101;
            case 'O': return 0b111'101'101'101'111;
            case 'P': return 0b111'101'111'100'100;
            case 'R': return 0b110'101'110'101'101;
            case 'S': return 0b111'100'111'001'111;
            case 'T': return 0b111'010'010'010'010;
            case 'U': return 0b101'101'101'101'111;
            case 'V': return 0b101'101'101'101'010;
            case 'W': return 0b101'101'101'111'101;
            case 'X': return 0b101'101'010'101'101;
            case 'Y': return 0b101'101'010'010'010;
            case 'Z': return 0b111'001'010'100'111;
            case ':': return 0b000'010'000'010'000;
            case '-': return 0b000'000'111'000'000;
            case '+': return 0b000'010'111'010'000;
            case '!': return 0b010'010'010'000'010;
            case '/': return 0b001'001'010'100'100;
            default:  return 0;
        }
    }

    void DrawText(const char* s, int x, int y, int sc, uint8_t r, uint8_t g, uint8_t b) {
        if (!s || sc <= 0) return;
        int cx = x;
        for (int i = 0; s[i] != '\0'; ++i) {
            uint16_t gbits = Glyph3x5(s[i]);
            if (gbits != 0) {
                for (int gy = 0; gy < 5; ++gy) {
                    for (int gx = 0; gx < 3; ++gx) {
                        int bit = 14 - (gy * 3 + gx);
                        if ((gbits >> bit) & 1)
                            FillRect(cx + gx * sc, y + gy * sc, sc, sc, r, g, b);
                    }
                }
            }
            cx += 4 * sc;
        }
    }

    void DrawInt(int val, int x, int y, int sc, uint8_t r, uint8_t g, uint8_t b) {
        char buf[24];
        bool neg = val < 0;
        unsigned u = neg ? (unsigned)(-val) : (unsigned)val;
        int p = 0;
        if (u == 0) buf[p++] = '0';
        while (u > 0 && p < 20) { buf[p++] = char('0' + (u % 10)); u /= 10; }
        if (neg) buf[p++] = '-';
        for (int i = 0; i < p / 2; ++i) std::swap(buf[i], buf[p - 1 - i]);
        buf[p] = '\0';
        DrawText(buf, x, y, sc, r, g, b);
    }
};

// ============================================================================
// 2. КАЗИНО «ЗОЛОТАЯ РУЛЕТКА + КИБЕР-СЛОТЫ 777» (по casino.html)
// ============================================================================
class CasinoEngine {
public:
    // Точный порядок 37 секторов европейской рулетки из casino.html
    static constexpr int WHEEL_COUNT = 37;
    static constexpr int WHEEL_ORDER[WHEEL_COUNT] = {
        0,32,15,19,4,21,2,25,17,34,6,27,13,36,11,30,8,23,10,5,24,16,33,1,20,14,31,9,22,18,29,7,28,12,35,3,26
    };

    enum BetZone {
        BET_RED = 0,       // Красное (x2)
        BET_BLACK,         // Чёрное (x2)
        BET_EVEN,          // Чёт (x2)
        BET_ODD,           // Нечет (x2)
        BET_LOW,           // 1-18 (x2)
        BET_HIGH,          // 19-36 (x2)
        BET_DOZEN_1,       // 1-12 (x3)
        BET_DOZEN_2,       // 13-24 (x3)
        BET_DOZEN_3,       // 25-36 (x3)
        BET_STRAIGHT,      // Точное число pickedNumber_ (x36)
        BET_VOISINS,       // Voisins du Zero — соседи зеро (17 чисел)
        BET_TIERS,         // Tiers du Cylindre — треть колеса (12 чисел)
        BET_ZONE_COUNT
    };

    static constexpr int CHIP_VALUES[6] = { 10, 25, 100, 500, 1000, 5000 };

    int modeTab_ = 0; // 0 = Золотая Рулетка, 1 = Кибер-Слоты 777
    int chipIdx_ = 1; // по умолчанию фишка 25₵
    int selectedZone_ = BET_RED;
    int pickedNumber_ = 7; // 0..36 для ставки на число
    std::array<int, BET_ZONE_COUNT> bets_{};
    std::array<int, BET_ZONE_COUNT> lastBets_{};

    bool spinning_ = false;
    bool autoSpin_ = false;
    float spinTimer_ = 0.0f;
    float spinDuration_ = 3.1f;
    float wheelAngle_ = 0.0f;
    float ballAngle_ = 0.0f;
    float startWheelAngle_ = 0.0f;
    float targetWheelAngle_ = 0.0f;
    int targetSectorIdx_ = 0;
    int lastResultNum_ = 0;
    int lastPayout_ = 0;
    int lastNet_ = 0;
    bool hasResult_ = false;
    std::array<int, 12> history_{};
    int historyCount_ = 0;

    // Слоты 777
    std::array<int, 3> reels_{ 4, 4, 4 };
    bool slotSpinning_ = false;
    float slotTimer_ = 0.0f;
    int slotBet_ = 50;
    int slotLastWin_ = 0;

    uint32_t rng_ = 0xC0FFEE77u;

    uint32_t NextRand() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return rng_;
    }

    static bool IsRed(int num) {
        switch (num) {
            case 1: case 3: case 5: case 7: case 9: case 12:
            case 14: case 16: case 18: case 19: case 21: case 23:
            case 25: case 27: case 30: case 32: case 34: case 36:
                return true;
            default:
                return false;
        }
    }

    static bool InVoisins(int n) {
        static constexpr int v[17] = {22,18,29,7,28,12,35,3,26,0,32,15,19,4,21,2,25};
        for (int x : v) if (x == n) return true;
        return false;
    }

    static bool InTiers(int n) {
        static constexpr int t[12] = {27,13,36,11,30,8,23,10,5,24,16,33};
        for (int x : t) if (x == n) return true;
        return false;
    }

    int TotalBet() const {
        int s = 0;
        for (int b : bets_) s += b;
        return s;
    }

    static int ZoneMultiplier(int zone, int hitNum, int pickedNumber) {
        switch (zone) {
            case BET_RED:      return (hitNum > 0 && IsRed(hitNum)) ? 2 : 0;
            case BET_BLACK:    return (hitNum > 0 && !IsRed(hitNum)) ? 2 : 0;
            case BET_EVEN:     return (hitNum > 0 && (hitNum % 2 == 0)) ? 2 : 0;
            case BET_ODD:      return (hitNum > 0 && (hitNum % 2 == 1)) ? 2 : 0;
            case BET_LOW:      return (hitNum >= 1 && hitNum <= 18) ? 2 : 0;
            case BET_HIGH:     return (hitNum >= 19 && hitNum <= 36) ? 2 : 0;
            case BET_DOZEN_1:  return (hitNum >= 1 && hitNum <= 12) ? 3 : 0;
            case BET_DOZEN_2:  return (hitNum >= 13 && hitNum <= 24) ? 3 : 0;
            case BET_DOZEN_3:  return (hitNum >= 25 && hitNum <= 36) ? 3 : 0;
            case BET_STRAIGHT: return (hitNum == pickedNumber) ? 36 : 0;
            case BET_VOISINS:  return InVoisins(hitNum) ? 2 : 0;
            case BET_TIERS:    return InTiers(hitNum) ? 3 : 0;
            default:           return 0;
        }
    }

    bool PlaceBet(int zone, int& playerCredits) {
        if (spinning_ || zone < 0 || zone >= BET_ZONE_COUNT) return false;
        int val = CHIP_VALUES[std::clamp(chipIdx_, 0, 5)];
        if (playerCredits < val) return false;
        playerCredits -= val;
        bets_[zone] += val;
        return true;
    }

    void ClearBets(int& playerCredits) {
        if (spinning_) return;
        playerCredits += TotalBet();
        bets_.fill(0);
    }

    bool RepeatBets(int& playerCredits) {
        if (spinning_) return false;
        int need = 0;
        for (int b : lastBets_) need += b;
        if (need <= 0 || playerCredits < need + TotalBet()) return false;
        ClearBets(playerCredits);
        for (int i = 0; i < BET_ZONE_COUNT; ++i) {
            bets_[i] = lastBets_[i];
            playerCredits -= lastBets_[i];
        }
        return true;
    }

    bool StartSpin(int forcedSectorIdx = -1) {
        if (spinning_ || TotalBet() <= 0) return false;
        lastBets_ = bets_;
        spinning_ = true;
        spinTimer_ = 0.0f;
        targetSectorIdx_ = (forcedSectorIdx >= 0 && forcedSectorIdx < WHEEL_COUNT)
                         ? forcedSectorIdx
                         : int(NextRand() % WHEEL_COUNT);
        startWheelAngle_ = wheelAngle_;
        const float segRad = 6.2831853f / float(WHEEL_COUNT);
        targetWheelAngle_ = startWheelAngle_ + 6.2831853f * 4.0f + float(targetSectorIdx_) * segRad;
        return true;
    }

    // Возвращает true в кадр завершения спина
    bool Update(float dt, int& playerCredits) {
        bool finishedEvent = false;
        if (spinning_) {
            spinTimer_ += dt;
            float t = std::clamp(spinTimer_ / spinDuration_, 0.0f, 1.0f);
            float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            wheelAngle_ = startWheelAngle_ + (targetWheelAngle_ - startWheelAngle_) * ease;
            ballAngle_  = -wheelAngle_ * 1.35f * (1.0f - t);
            if (t >= 1.0f) {
                spinning_ = false;
                lastResultNum_ = WHEEL_ORDER[targetSectorIdx_];
                int staked = TotalBet();
                int payout = 0;
                for (int z = 0; z < BET_ZONE_COUNT; ++z) {
                    if (bets_[z] > 0)
                        payout += bets_[z] * ZoneMultiplier(z, lastResultNum_, pickedNumber_);
                }
                playerCredits += payout;
                lastPayout_ = payout;
                lastNet_ = payout - staked;
                hasResult_ = true;
                bets_.fill(0);

                // История последних 12 спинов
                for (int i = std::min(historyCount_, 11); i > 0; --i)
                    history_[i] = history_[i - 1];
                history_[0] = lastResultNum_;
                if (historyCount_ < 12) ++historyCount_;
                finishedEvent = true;

                if (autoSpin_ && RepeatBets(playerCredits)) {
                    StartSpin();
                } else {
                    autoSpin_ = false;
                }
            }
        }

        if (slotSpinning_) {
            slotTimer_ += dt;
            if (slotTimer_ < 1.15f) {
                reels_[0] = int((NextRand() >> 3) % 6);
                reels_[1] = int((NextRand() >> 5) % 6);
                reels_[2] = int((NextRand() >> 7) % 6);
            } else {
                slotSpinning_ = false;
                reels_[0] = int(NextRand() % 6);
                reels_[1] = int(NextRand() % 6);
                reels_[2] = int(NextRand() % 6);
                int mult = 0;
                if (reels_[0] == reels_[1] && reels_[1] == reels_[2]) {
                    mult = (reels_[0] == 5) ? 25 : (reels_[0] == 4 ? 15 : 8);
                } else if (reels_[0] == reels_[1] || reels_[1] == reels_[2] || reels_[0] == reels_[2]) {
                    mult = 2;
                }
                slotLastWin_ = slotBet_ * mult;
                playerCredits += slotLastWin_;
                finishedEvent = true;
            }
        }
        return finishedEvent;
    }

    bool SpinSlots(int& playerCredits) {
        if (slotSpinning_ || playerCredits < slotBet_) return false;
        playerCredits -= slotBet_;
        slotSpinning_ = true;
        slotTimer_ = 0.0f;
        slotLastWin_ = 0;
        return true;
    }

    void Render(PixelCanvas& cv) const {
        cv.Clear(10, 28, 20); // бархатное сукно казино
        cv.DrawBorder(2, 2, SCREEN_W - 4, SCREEN_H - 4, 232, 185, 60);

        if (modeTab_ == 1) {
            // --- Рендер автомата КИБЕР-СЛОТЫ 777 ---
            cv.FillRect(16, 14, SCREEN_W - 32, 26, 26, 14, 42);
            cv.DrawBorder(16, 14, SCREEN_W - 32, 26, 255, 205, 55);
            cv.DrawText("CYBER SLOTS 777", 72, 21, 2, 255, 215, 60);

            static const char* symName[6] = { "CHK", "EGG", "GLD", "24K", "777", "DIA" };
            static const RGB symCol[6] = {
                {245,245,235}, {255,215,90}, {255,165,40}, {65,230,255}, {255,55,75}, {195,90,255}
            };
            for (int r = 0; r < 3; ++r) {
                int rx = 34 + r * 88;
                cv.FillRect(rx, 54, 76, 92, 18, 20, 30);
                cv.DrawBorder(rx, 54, 76, 92, 232, 185, 60);
                int s = reels_[r] % 6;
                RGB c = symCol[s];
                cv.FillCircle(rx + 38, 92, 20, c.r / 3, c.g / 3, c.b / 3);
                cv.DrawText(symName[s], rx + 16, 86, 3, c.r, c.g, c.b);
            }
            cv.DrawText("BET:", 36, 166, 2, 200, 220, 210);
            cv.DrawInt(slotBet_, 82, 166, 2, 255, 215, 70);
            cv.DrawText("WIN:", 170, 166, 2, 200, 220, 210);
            cv.DrawInt(slotLastWin_, 218, 166, 2, 80, 255, 140);
            cv.DrawText("SPACE: SPIN   TAB: ROULETTE", 44, 196, 1, 230, 200, 110);
            return;
        }

        // --- Рендер колеса ЗОЛОТОЙ РУЛЕТКИ (левая половина) ---
        const int cx = 98, cy = 108;
        cv.FillCircle(cx, cy, 86, 92, 52, 20);  // красное дерево обода
        cv.FillCircle(cx, cy, 78, 225, 182, 62);// золотое кольцо
        cv.FillCircle(cx, cy, 74, 18, 22, 28);

        for (int i = 0; i < WHEEL_COUNT; ++i) {
            int num = WHEEL_ORDER[i];
            float ang = wheelAngle_ + float(i) * (6.2831853f / float(WHEEL_COUNT));
            int px = cx + int(std::cos(ang) * 63.0f);
            int py = cy + int(std::sin(ang) * 63.0f);
            RGB col = (num == 0) ? RGB{32, 185, 92}
                    : (IsRed(num) ? RGB{220, 44, 62} : RGB{38, 44, 56});
            cv.FillCircle(px, py, 6, col.r, col.g, col.b);
        }
        cv.FillCircle(cx, cy, 46, 24, 56, 38);
        cv.FillCircle(cx, cy, 16, 235, 192, 68); // центральная золотая турель
        for (int sp = 0; sp < 4; ++sp) {
            float a = wheelAngle_ + sp * 1.570796f;
            cv.DrawLine(cx - int(std::cos(a)*28), cy - int(std::sin(a)*28),
                        cx + int(std::cos(a)*28), cy + int(std::sin(a)*28), 235, 192, 68);
        }

        // Шарик рулетки
        float finalAng = float(targetSectorIdx_) * (6.2831853f / float(WHEEL_COUNT));
        float bAng = spinning_ ? ballAngle_ : (wheelAngle_ + finalAng);
        float bRad = spinning_ ? 70.0f : 62.0f;
        int bx = cx + int(std::cos(bAng) * bRad);
        int by = cy + int(std::sin(bAng) * bRad);
        cv.FillCircle(bx, by, 4, 255, 255, 255);

        // В центре колеса — последний выпавший номер
        if (hasResult_) {
            RGB rc = (lastResultNum_ == 0) ? RGB{40, 220, 110}
                   : (IsRed(lastResultNum_) ? RGB{255, 80, 95} : RGB{220, 230, 245});
            cv.DrawInt(lastResultNum_, cx - (lastResultNum_ >= 10 ? 11 : 5), cy - 34, 3, rc.r, rc.g, rc.b);
        }

        // --- Правая половина: стол ставок (12 зон) ---
        static const char* zLbl[BET_ZONE_COUNT] = {
            "RED X2", "BLK X2", "EVEN", "ODD", "1-18", "19-36",
            "1ST 12", "2ND 12", "3RD 12", "NUM X36", "VOISIN", "TIERS"
        };
        for (int z = 0; z < BET_ZONE_COUNT; ++z) {
            int col = z % 2;
            int row = z / 2;
            int x = 194 + col * 60;
            int y = 14 + row * 28;
            bool sel = (z == selectedZone_);
            uint8_t br = (z == BET_RED) ? 140 : (z == BET_BLACK ? 26 : 18);
            uint8_t bg = (z == BET_STRAIGHT) ? 75 : 54;
            uint8_t bb = (z == BET_BLACK) ? 34 : 38;
            cv.FillRect(x, y, 56, 24, br, bg, bb);
            cv.DrawBorder(x, y, 56, 24, sel ? 255 : 140, sel ? 225 : 145, sel ? 55 : 90);
            cv.DrawText(zLbl[z], x + 4, y + 4, 1, 245, 245, 240);
            if (bets_[z] > 0) {
                cv.FillCircle(x + 47, y + 16, 5, 255, 200, 45);
                cv.DrawInt(bets_[z], x + 4, y + 14, 1, 255, 230, 90);
            } else if (z == BET_STRAIGHT) {
                cv.DrawInt(pickedNumber_, x + 36, y + 14, 1, 120, 255, 170);
            }
        }

        // Лента истории выпавших секторов внизу
        cv.DrawText("HIST:", 12, 202, 1, 210, 210, 190);
        for (int h = 0; h < historyCount_; ++h) {
            int num = history_[h];
            RGB hc = (num == 0) ? RGB{32, 195, 95} : (IsRed(num) ? RGB{225, 50, 65} : RGB{55, 62, 78});
            int hx = 40 + h * 22;
            cv.FillRect(hx, 198, 19, 13, hc.r, hc.g, hc.b);
            cv.DrawInt(num, hx + 2, 202, 1, 255, 255, 255);
        }
    }
};

// ============================================================================
// 3. PIXEL ARCADE — ВСЕ 6 МИНИ-ИГР ИЗ minigames.html НА ЧИСТОМ C++17
// ============================================================================
class ArcadeSuite {
public:
    enum GameId {
        G_SNAKE = 0,     // 01 ЗМЕЙКА
        G_PONG,          // 02 PONG (1P vs AI)
        G_BREAKOUT,      // 03 АРКАНОИД
        G_FLAPPY,        // 04 FLAPPY CHICKEN
        G_DINO,          // 05 DINO RUN
        G_TETRIS,        // 06 ТЕТРИС
        GAME_COUNT
    };

    int activeGame_ = G_SNAKE;
    int score_ = 0;
    std::array<int, GAME_COUNT> highScores_{};
    bool gameOver_ = false;
    bool paused_ = false;
    uint32_t rng_ = 0xA2CADE01u;

    // Ввод на текущий кадр
    bool holdLeft_ = false, holdRight_ = false, holdUp_ = false, holdDown_ = false;
    bool tapLeft_ = false, tapRight_ = false, tapUp_ = false, tapDown_ = false, tapAction_ = false;

    uint32_t Rnd() {
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        return rng_;
    }

    // --- Состояние 01 SNAKE ---
    static constexpr int SN_W = 20, SN_H = 12;
    struct Cell { int x, y; };
    std::array<Cell, SN_W * SN_H> snake_{};
    int snakeLen_ = 4;
    int snDirX_ = 1, snDirY_ = 0;
    int nextDirX_ = 1, nextDirY_ = 0;
    Cell apple_{ 14, 6 };
    float snStepT_ = 0.0f;

    // --- Состояние 02 PONG ---
    float pongPY_ = 90.0f, pongAIY_ = 90.0f;
    float pongBX_ = 160.0f, pongBY_ = 110.0f, pongBVX_ = 135.0f, pongBVY_ = 75.0f;
    int pongAIScore_ = 0;

    // --- Состояние 03 BREAKOUT ---
    static constexpr int BR_COLS = 10, BR_ROWS = 5;
    std::array<bool, BR_COLS * BR_ROWS> bricks_{};
    float padX_ = 132.0f;
    float brBX_ = 160.0f, brBY_ = 165.0f, brVX_ = 110.0f, brVY_ = -125.0f;

    // --- Состояние 04 FLAPPY ---
    float flapY_ = 100.0f, flapVY_ = 0.0f;
    struct Pipe { float x; float gapY; bool scored; };
    std::array<Pipe, 3> pipes_{};

    // --- Состояние 05 DINO RUN ---
    float dinoY_ = 0.0f, dinoVY_ = 0.0f;
    float dinoSpeed_ = 145.0f;
    float dinoDistAcc_ = 0.0f;
    struct Obstacle { float x; int kind; }; // 0=низкий кактус, 1=высокий, 2=птица
    std::array<Obstacle, 3> obs_{};

    // --- Состояние 06 TETRIS ---
    static constexpr int TW = 10, TH = 18;
    std::array<uint8_t, TW * TH> board_{};
    int curPiece_ = 0, nextPiece_ = 1, curRot_ = 0, curX_ = 3, curY_ = 0;
    float tetFallT_ = 0.0f;

    static const char* GetGameTitle(int g) {
        static const char* kNames[GAME_COUNT] = {
            "01 ЗМЕЙКА (SNAKE)",
            "02 PONG (1P VS AI)",
            "03 АРКАНОИД (BREAKOUT)",
            "04 FLAPPY CHICKEN",
            "05 CYBER DINO RUN",
            "06 ТЕТРИС (TETRIS)"
        };
        return kNames[std::clamp(g, 0, GAME_COUNT - 1)];
    }

    ArcadeSuite() { ResetGame(G_SNAKE); }

    void SpawnApple() {
        for (int tries = 0; tries < 64; ++tries) {
            int ax = int(Rnd() % SN_W);
            int ay = int(Rnd() % SN_H);
            bool hit = false;
            for (int i = 0; i < snakeLen_; ++i)
                if (snake_[i].x == ax && snake_[i].y == ay) { hit = true; break; }
            if (!hit) { apple_ = { ax, ay }; return; }
        }
        apple_ = { 0, 0 };
    }

    static bool TetCell(int piece, int rot, int cx, int cy) {
        // 7 фигур Тетриса в матрице 4x4 (поворот 0..3 вычисляется аналитически)
        static const uint16_t kShapes[7] = {
            0b0000'1111'0000'0000, // I
            0b0000'0110'0110'0000, // O
            0b0000'0100'1110'0000, // T
            0b0000'0110'1100'0000, // S
            0b0000'1100'0110'0000, // Z
            0b0000'1000'1110'0000, // J
            0b0000'0010'1110'0000  // L
        };
        int rx = cx, ry = cy;
        for (int r = 0; r < (rot & 3); ++r) {
            int nx = 3 - ry, ny = rx;
            rx = nx; ry = ny;
        }
        int bit = 15 - (ry * 4 + rx);
        return ((kShapes[piece % 7] >> bit) & 1) != 0;
    }

    bool TetFits(int piece, int rot, int px, int py) const {
        for (int cy = 0; cy < 4; ++cy) {
            for (int cx = 0; cx < 4; ++cx) {
                if (!TetCell(piece, rot, cx, cy)) continue;
                int bx = px + cx, by = py + cy;
                if (bx < 0 || bx >= TW || by >= TH) return false;
                if (by >= 0 && board_[by * TW + bx] != 0) return false;
            }
        }
        return true;
    }

    void ResetGame(int gameId) {
        activeGame_ = std::clamp(gameId, 0, GAME_COUNT - 1);
        score_ = 0;
        gameOver_ = false;
        paused_ = false;

        if (activeGame_ == G_SNAKE) {
            snakeLen_ = 4;
            for (int i = 0; i < 4; ++i) snake_[i] = { 6 - i, 6 };
            snDirX_ = nextDirX_ = 1; snDirY_ = nextDirY_ = 0;
            snStepT_ = 0.0f;
            SpawnApple();
        } else if (activeGame_ == G_PONG) {
            pongPY_ = 95.0f; pongAIY_ = 95.0f;
            pongBX_ = 160.0f; pongBY_ = 110.0f;
            pongBVX_ = 140.0f; pongBVY_ = 78.0f;
            pongAIScore_ = 0;
        } else if (activeGame_ == G_BREAKOUT) {
            bricks_.fill(true);
            padX_ = 132.0f;
            brBX_ = 160.0f; brBY_ = 170.0f;
            brVX_ = 108.0f; brVY_ = -132.0f;
        } else if (activeGame_ == G_FLAPPY) {
            flapY_ = 100.0f; flapVY_ = 0.0f;
            for (int i = 0; i < 3; ++i)
                pipes_[i] = { 210.0f + i * 112.0f, 65.0f + float(Rnd() % 85), false };
        } else if (activeGame_ == G_DINO) {
            dinoY_ = 0.0f; dinoVY_ = 0.0f; dinoSpeed_ = 148.0f; dinoDistAcc_ = 0.0f;
            for (int i = 0; i < 3; ++i)
                obs_[i] = { 250.0f + i * 135.0f, int(Rnd() % 3) };
        } else if (activeGame_ == G_TETRIS) {
            board_.fill(0);
            curPiece_ = int(Rnd() % 7);
            nextPiece_ = int(Rnd() % 7);
            curRot_ = 0; curX_ = 3; curY_ = 0;
            tetFallT_ = 0.0f;
        }
    }

    void ClearEdgeInputs() {
        tapLeft_ = tapRight_ = tapUp_ = tapDown_ = tapAction_ = false;
    }

    // Возвращает заработанные за этот кадр кибер-кредиты (₵)
    int Update(float dt) {
        if (paused_ || gameOver_) { ClearEdgeInputs(); return 0; }
        int earnedCredits = 0;

        switch (activeGame_) {
        case G_SNAKE: {
            if (tapUp_ && snDirY_ == 0)    { nextDirX_ = 0;  nextDirY_ = -1; }
            if (tapDown_ && snDirY_ == 0)  { nextDirX_ = 0;  nextDirY_ =  1; }
            if (tapLeft_ && snDirX_ == 0)  { nextDirX_ = -1; nextDirY_ =  0; }
            if (tapRight_ && snDirX_ == 0) { nextDirX_ =  1; nextDirY_ =  0; }
            snStepT_ += dt;
            float period = std::max(0.065f, 0.145f - float(score_) * 0.002f);
            if (snStepT_ >= period) {
                snStepT_ -= period;
                snDirX_ = nextDirX_; snDirY_ = nextDirY_;
                Cell head = { snake_[0].x + snDirX_, snake_[0].y + snDirY_ };
                if (head.x < 0 || head.x >= SN_W || head.y < 0 || head.y >= SN_H) {
                    gameOver_ = true; break;
                }
                for (int i = 0; i < snakeLen_; ++i)
                    if (snake_[i].x == head.x && snake_[i].y == head.y) { gameOver_ = true; break; }
                if (gameOver_) break;

                bool ate = (head.x == apple_.x && head.y == apple_.y);
                if (ate && snakeLen_ < SN_W * SN_H - 1) ++snakeLen_;
                for (int i = snakeLen_ - 1; i > 0; --i) snake_[i] = snake_[i - 1];
                snake_[0] = head;
                if (ate) {
                    score_ += 10;
                    earnedCredits += 5;
                    SpawnApple();
                }
            }
            break;
        }
        case G_PONG: {
            if (holdUp_)   pongPY_ -= 165.0f * dt;
            if (holdDown_) pongPY_ += 165.0f * dt;
            pongPY_ = std::clamp(pongPY_, 24.0f, float(SCREEN_H - 42));

            float aiCenter = pongAIY_ + 18.0f;
            if (pongBY_ < aiCenter - 4.0f) pongAIY_ -= 128.0f * dt;
            if (pongBY_ > aiCenter + 4.0f) pongAIY_ += 128.0f * dt;
            pongAIY_ = std::clamp(pongAIY_, 24.0f, float(SCREEN_H - 42));

            pongBX_ += pongBVX_ * dt;
            pongBY_ += pongBVY_ * dt;
            if (pongBY_ < 26.0f) { pongBY_ = 26.0f; pongBVY_ = std::fabs(pongBVY_); }
            if (pongBY_ > SCREEN_H - 8.0f) { pongBY_ = SCREEN_H - 8.0f; pongBVY_ = -std::fabs(pongBVY_); }

            // Левая ракетка игрока (x=18..24)
            if (pongBX_ <= 25.0f && pongBX_ >= 16.0f && pongBY_ >= pongPY_ - 4.0f && pongBY_ <= pongPY_ + 40.0f) {
                pongBX_ = 25.5f;
                pongBVX_ = std::min(260.0f, std::fabs(pongBVX_) * 1.05f);
                pongBVY_ = ((pongBY_ - (pongPY_ + 18.0f)) / 18.0f) * 145.0f;
            }
            // Правая ракетка ИИ (x=296..302)
            if (pongBX_ >= 294.0f && pongBX_ <= 304.0f && pongBY_ >= pongAIY_ - 4.0f && pongBY_ <= pongAIY_ + 40.0f) {
                pongBX_ = 293.5f;
                pongBVX_ = -std::min(260.0f, std::fabs(pongBVX_) * 1.04f);
                pongBVY_ = ((pongBY_ - (pongAIY_ + 18.0f)) / 18.0f) * 140.0f;
            }
            if (pongBX_ > SCREEN_W) {
                score_ += 1;
                earnedCredits += 15;
                pongBX_ = 160.0f; pongBY_ = 110.0f; pongBVX_ = -140.0f; pongBVY_ = 75.0f;
            } else if (pongBX_ < 0.0f) {
                pongAIScore_++;
                if (pongAIScore_ >= 5) gameOver_ = true;
                pongBX_ = 160.0f; pongBY_ = 110.0f; pongBVX_ = 140.0f; pongBVY_ = -75.0f;
            }
            break;
        }
        case G_BREAKOUT: {
            if (holdLeft_)  padX_ -= 195.0f * dt;
            if (holdRight_) padX_ += 195.0f * dt;
            padX_ = std::clamp(padX_, 8.0f, float(SCREEN_W - 60));

            brBX_ += brVX_ * dt;
            brBY_ += brVY_ * dt;
            if (brBX_ < 8.0f) { brBX_ = 8.0f; brVX_ = std::fabs(brVX_); }
            if (brBX_ > SCREEN_W - 8.0f) { brBX_ = SCREEN_W - 8.0f; brVX_ = -std::fabs(brVX_); }
            if (brBY_ < 26.0f) { brBY_ = 26.0f; brVY_ = std::fabs(brVY_); }
            if (brBY_ > SCREEN_H) { gameOver_ = true; break; }

            if (brVY_ > 0.0f && brBY_ >= 198.0f && brBY_ <= 207.0f &&
                brBX_ >= padX_ - 4.0f && brBX_ <= padX_ + 56.0f) {
                brBY_ = 197.5f;
                brVY_ = -std::fabs(brVY_);
                brVX_ = ((brBX_ - (padX_ + 26.0f)) / 26.0f) * 155.0f;
            }

            bool anyAlive = false;
            for (int r = 0; r < BR_ROWS; ++r) {
                for (int c = 0; c < BR_COLS; ++c) {
                    if (!bricks_[r * BR_COLS + c]) continue;
                    anyAlive = true;
                    int bx = 12 + c * 30, by = 34 + r * 14;
                    if (brBX_ >= bx - 3 && brBX_ <= bx + 29 && brBY_ >= by - 3 && brBY_ <= by + 13) {
                        bricks_[r * BR_COLS + c] = false;
                        brVY_ = -brVY_;
                        score_ += 15;
                        earnedCredits += 4;
                    }
                }
            }
            if (!anyAlive) {
                bricks_.fill(true);
                earnedCredits += 50;
                brBY_ = 165.0f; brVY_ = -std::fabs(brVY_) * 1.08f;
            }
            break;
        }
        case G_FLAPPY: {
            if (tapAction_ || tapUp_) flapVY_ = -175.0f;
            flapVY_ += 420.0f * dt;
            flapY_ += flapVY_ * dt;
            if (flapY_ < 24.0f || flapY_ > SCREEN_H - 12.0f) { gameOver_ = true; break; }

            for (auto& p : pipes_) {
                p.x -= 96.0f * dt;
                if (!p.scored && p.x + 26.0f < 68.0f) {
                    p.scored = true;
                    score_ += 1;
                    earnedCredits += 8;
                }
                if (p.x < -30.0f) {
                    p.x += 336.0f;
                    p.gapY = 62.0f + float(Rnd() % 88);
                    p.scored = false;
                }
                // Коллизия с трубой (x птицы = 68, радиус 7, окно gapY +- 27)
                if (68.0f + 7.0f > p.x && 68.0f - 7.0f < p.x + 26.0f) {
                    if (flapY_ - 7.0f < p.gapY - 27.0f || flapY_ + 7.0f > p.gapY + 27.0f)
                        gameOver_ = true;
                }
            }
            break;
        }
        case G_DINO: {
            if ((tapAction_ || tapUp_) && dinoY_ <= 0.5f) dinoVY_ = 255.0f;
            dinoVY_ -= 620.0f * dt;
            dinoY_ = std::max(0.0f, dinoY_ + dinoVY_ * dt);
            if (dinoY_ == 0.0f && dinoVY_ < 0.0f) dinoVY_ = 0.0f;

            dinoSpeed_ += 4.5f * dt;
            dinoDistAcc_ += dinoSpeed_ * dt * 0.1f;
            int newScore = int(dinoDistAcc_);
            if (newScore / 25 > score_ / 25) earnedCredits += 6;
            score_ = newScore;

            for (auto& o : obs_) {
                o.x -= dinoSpeed_ * dt;
                if (o.x < -24.0f) {
                    o.x += 405.0f + float(Rnd() % 65);
                    o.kind = int(Rnd() % 3);
                }
                float ow = (o.kind == 1) ? 18.0f : 14.0f;
                float oh = (o.kind == 1) ? 30.0f : 20.0f;
                float oy = (o.kind == 2) ? 24.0f : 0.0f; // птица летит выше земли
                if (54.0f + 14.0f > o.x && 54.0f < o.x + ow) {
                    if (dinoY_ < oy + oh && dinoY_ + 20.0f > oy)
                        gameOver_ = true;
                }
            }
            break;
        }
        case G_TETRIS: {
            if (tapLeft_  && TetFits(curPiece_, curRot_, curX_ - 1, curY_)) curX_--;
            if (tapRight_ && TetFits(curPiece_, curRot_, curX_ + 1, curY_)) curX_++;
            if (tapUp_) {
                int nr = (curRot_ + 1) & 3;
                if (TetFits(curPiece_, nr, curX_, curY_)) curRot_ = nr;
                else if (TetFits(curPiece_, nr, curX_ - 1, curY_)) { curX_--; curRot_ = nr; }
                else if (TetFits(curPiece_, nr, curX_ + 1, curY_)) { curX_++; curRot_ = nr; }
            }
            if (tapAction_) {
                while (TetFits(curPiece_, curRot_, curX_, curY_ + 1)) curY_++;
                tetFallT_ = 10.0f; // мгновенная фиксация
            }

            float stepPeriod = holdDown_ ? 0.045f : std::max(0.12f, 0.48f - float(score_) * 0.0002f);
            tetFallT_ += dt;
            if (tetFallT_ >= stepPeriod) {
                tetFallT_ = 0.0f;
                if (TetFits(curPiece_, curRot_, curX_, curY_ + 1)) {
                    curY_++;
                } else {
                    // Фиксируем фигуру
                    for (int cy = 0; cy < 4; ++cy)
                        for (int cx = 0; cx < 4; ++cx)
                            if (TetCell(curPiece_, curRot_, cx, cy) && curY_ + cy >= 0 && curY_ + cy < TH)
                                board_[(curY_ + cy) * TW + (curX_ + cx)] = uint8_t(curPiece_ + 1);

                    // Сжигаем заполненные линии
                    int lines = 0;
                    for (int y = TH - 1; y >= 0; --y) {
                        bool full = true;
                        for (int x = 0; x < TW; ++x) if (!board_[y * TW + x]) { full = false; break; }
                        if (full) {
                            ++lines;
                            for (int yy = y; yy > 0; --yy)
                                for (int x = 0; x < TW; ++x) board_[yy * TW + x] = board_[(yy - 1) * TW + x];
                            for (int x = 0; x < TW; ++x) board_[x] = 0;
                            ++y;
                        }
                    }
                    if (lines > 0) {
                        static const int kPts[5] = { 0, 100, 300, 500, 800 };
                        score_ += kPts[std::min(4, lines)];
                        earnedCredits += lines * 18;
                    }
                    curPiece_ = nextPiece_;
                    nextPiece_ = int(Rnd() % 7);
                    curRot_ = 0; curX_ = 3; curY_ = 0;
                    if (!TetFits(curPiece_, curRot_, curX_, curY_)) gameOver_ = true;
                }
            }
            break;
        }
        }

        if (score_ > highScores_[activeGame_])
            highScores_[activeGame_] = score_;

        ClearEdgeInputs();
        return earnedCredits;
    }

    void Render(PixelCanvas& cv) const {
        cv.Clear(14, 10, 28);
        // Верхний неоновый бар аркадного автомата
        cv.FillRect(0, 0, SCREEN_W, 20, 28, 16, 52);
        cv.FillRect(0, 20, SCREEN_W, 1, 62, 255, 224);
        cv.DrawText("SCORE:", 8, 5, 2, 180, 210, 255);
        cv.DrawInt(score_, 64, 5, 2, 62, 255, 130);
        cv.DrawText("HI:", 190, 5, 2, 255, 195, 70);
        cv.DrawInt(highScores_[activeGame_], 222, 5, 2, 255, 230, 110);

        switch (activeGame_) {
        case G_SNAKE: {
            const int ox = 20, oy = 30, cs = 14;
            cv.DrawBorder(ox - 2, oy - 2, SN_W * cs + 4, SN_H * cs + 4, 62, 255, 122);
            cv.FillRect(ox + apple_.x * cs + 2, oy + apple_.y * cs + 2, cs - 4, cs - 4, 255, 62, 95);
            for (int i = 0; i < snakeLen_; ++i) {
                uint8_t g = (i == 0) ? 255 : 210;
                cv.FillRect(ox + snake_[i].x * cs + 1, oy + snake_[i].y * cs + 1, cs - 2, cs - 2,
                            i == 0 ? 130 : 62, g, 122);
            }
            break;
        }
        case G_PONG: {
            for (int y = 26; y < SCREEN_H - 6; y += 10)
                cv.FillRect(SCREEN_W / 2 - 1, y, 2, 6, 70, 90, 120);
            cv.FillRect(18, int(pongPY_), 6, 36, 62, 255, 224);
            cv.FillRect(296, int(pongAIY_), 6, 36, 255, 62, 165);
            cv.FillCircle(int(pongBX_), int(pongBY_), 4, 255, 245, 120);
            cv.DrawText("AI:", 134, 5, 2, 255, 95, 175);
            cv.DrawInt(pongAIScore_, 162, 5, 2, 255, 95, 175);
            break;
        }
        case G_BREAKOUT: {
            static const RGB rowC[BR_ROWS] = {
                {255,62,165}, {255,126,62}, {255,233,62}, {62,255,122}, {62,225,255}
            };
            for (int r = 0; r < BR_ROWS; ++r)
                for (int c = 0; c < BR_COLS; ++c)
                    if (bricks_[r * BR_COLS + c])
                        cv.FillRect(12 + c * 30, 34 + r * 14, 26, 10, rowC[r].r, rowC[r].g, rowC[r].b);
            cv.FillRect(int(padX_), 200, 52, 7, 62, 255, 224);
            cv.FillCircle(int(brBX_), int(brBY_), 4, 255, 255, 255);
            break;
        }
        case G_FLAPPY: {
            for (const auto& p : pipes_) {
                int px = int(p.x);
                int topH = int(p.gapY - 27.0f) - 22;
                if (topH > 0) cv.FillRect(px, 22, 26, topH, 52, 215, 115);
                int botY = int(p.gapY + 27.0f);
                if (botY < SCREEN_H) cv.FillRect(px, botY, 26, SCREEN_H - botY, 52, 215, 115);
            }
            cv.FillCircle(68, int(flapY_), 7, 255, 225, 60);
            cv.FillRect(73, int(flapY_) - 1, 5, 3, 255, 110, 35); // клюв
            break;
        }
        case G_DINO: {
            const int groundY = 186;
            cv.FillRect(0, groundY, SCREEN_W, 3, 155, 62, 255);
            int dy = groundY - 20 - int(dinoY_);
            cv.FillRect(54, dy, 15, 20, 255, 225, 75);
            cv.FillRect(65, dy + 3, 6, 4, 255, 90, 45);
            for (const auto& o : obs_) {
                int ox = int(o.x);
                int ow = (o.kind == 1) ? 18 : 14;
                int oh = (o.kind == 1) ? 30 : 20;
                int oy = (o.kind == 2) ? (groundY - 44) : (groundY - oh);
                cv.FillRect(ox, oy, ow, oh, 255, 72, 125);
            }
            break;
        }
        case G_TETRIS: {
            static const RGB pCol[8] = {
                {24,20,40}, {62,240,255}, {255,230,62}, {185,75,255},
                {62,255,130}, {255,70,95}, {75,130,255}, {255,145,55}
            };
            const int ox = 95, oy = 26, cs = 10;
            cv.DrawBorder(ox - 2, oy - 2, TW * cs + 4, TH * cs + 4, 155, 95, 255);
            for (int y = 0; y < TH; ++y) {
                for (int x = 0; x < TW; ++x) {
                    uint8_t v = board_[y * TW + x];
                    RGB c = pCol[v % 8];
                    cv.FillRect(ox + x * cs, oy + y * cs, cs - 1, cs - 1, c.r, c.g, c.b);
                }
            }
            RGB ac = pCol[(curPiece_ % 7) + 1];
            for (int cy = 0; cy < 4; ++cy)
                for (int cx = 0; cx < 4; ++cx)
                    if (TetCell(curPiece_, curRot_, cx, cy) && curY_ + cy >= 0)
                        cv.FillRect(ox + (curX_ + cx) * cs, oy + (curY_ + cy) * cs, cs - 1, cs - 1, ac.r, ac.g, ac.b);

            cv.DrawText("NEXT", 220, 40, 2, 200, 210, 255);
            RGB nc = pCol[(nextPiece_ % 7) + 1];
            for (int cy = 0; cy < 4; ++cy)
                for (int cx = 0; cx < 4; ++cx)
                    if (TetCell(nextPiece_, 0, cx, cy))
                        cv.FillRect(222 + cx * 10, 60 + cy * 10, 9, 9, nc.r, nc.g, nc.b);
            break;
        }
        }

        if (gameOver_) {
            cv.FillRect(62, 86, 196, 52, 18, 8, 28);
            cv.DrawBorder(62, 86, 196, 52, 255, 75, 110);
            cv.DrawText("GAME OVER!", 96, 96, 3, 255, 85, 110);
            cv.DrawText("PRESS R TO RETRY", 96, 122, 1, 240, 240, 240);
        }
    }
};

} // namespace MiniHub
