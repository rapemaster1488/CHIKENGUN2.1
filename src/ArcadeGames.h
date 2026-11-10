#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Arcade {

// ============================================================================
// 1. ЗОЛОТАЯ РУЛЕТКА (Европейская рулетка 0..36, интеграция с кредитами ¤)
// ============================================================================

inline constexpr std::array<int, 37> ROULETTE_ORDER = {
    0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10,
    5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26
};

inline bool IsRouletteRed(int n) {
    switch (n) {
        case 1: case 3: case 5: case 7: case 9: case 12:
        case 14: case 16: case 18: case 19: case 21: case 23:
        case 25: case 27: case 30: case 32: case 34: case 36:
            return true;
        default:
            return false;
    }
}

enum RouletteBetType {
    BET_RED = 0,
    BET_BLACK,
    BET_EVEN,
    BET_ODD,
    BET_LOW,      // 1..18
    BET_HIGH,     // 19..36
    BET_DOZ1,     // 1..12
    BET_DOZ2,     // 13..24
    BET_DOZ3,     // 25..36
    BET_ZERO,     // 0 (x36)
    BET_STRAIGHT  // exact number 0..36 (x36)
};

struct RouletteBet {
    RouletteBetType type = BET_RED;
    int keyNum = 0;
    int amount = 0;
};

struct RouletteSim {
    uint32_t rng = 0x777A123u;
    bool spinning = false;
    float spinTimer = 0.0f;
    float spinDur = 2.2f;
    float wheelDeg = 0.0f;
    float ballDeg = 0.0f;
    int targetIdx = 0;
    int targetNum = 0;

    int selectedChip = 25;
    int pickedNumber = 7;

    std::vector<RouletteBet> bets;
    std::vector<RouletteBet> lastBets;
    std::vector<int> history;

    int spins = 0;
    int totalWagered = 0;
    int totalWon = 0;
    int bestWin = 0;
    int lastPayout = 0;
    int lastNet = 0;
    int lastNumber = -1;

    uint32_t NextRand() {
        uint32_t x = rng;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rng = x ? x : 0x777A123u;
        return rng;
    }

    static int BetMultiplier(RouletteBetType t) {
        switch (t) {
            case BET_DOZ1:
            case BET_DOZ2:
            case BET_DOZ3: return 3;
            case BET_ZERO:
            case BET_STRAIGHT: return 36;
            default: return 2;
        }
    }

    static bool IsBetWin(const RouletteBet& b, int n) {
        switch (b.type) {
            case BET_RED:      return n > 0 && IsRouletteRed(n);
            case BET_BLACK:    return n > 0 && !IsRouletteRed(n);
            case BET_EVEN:     return n > 0 && (n % 2 == 0);
            case BET_ODD:      return n > 0 && (n % 2 == 1);
            case BET_LOW:      return n >= 1 && n <= 18;
            case BET_HIGH:     return n >= 19 && n <= 36;
            case BET_DOZ1:     return n >= 1 && n <= 12;
            case BET_DOZ2:     return n >= 13 && n <= 24;
            case BET_DOZ3:     return n >= 25 && n <= 36;
            case BET_ZERO:     return n == 0;
            case BET_STRAIGHT: return n == b.keyNum;
        }
        return false;
    }

    int CurrentWager() const {
        int sum = 0;
        for (const auto& b : bets) sum += b.amount;
        return sum;
    }

    bool PlaceBet(RouletteBetType t, int keyNum, int chip, int& wallet) {
        if (spinning || chip <= 0 || wallet < chip) return false;
        wallet -= chip;
        for (auto& b : bets) {
            if (b.type == t && (t != BET_STRAIGHT || b.keyNum == keyNum)) {
                b.amount += chip;
                return true;
            }
        }
        if (bets.size() >= 32) {
            wallet += chip;
            return false;
        }
        bets.push_back(RouletteBet{t, keyNum, chip});
        return true;
    }

    bool UndoBet(int& wallet) {
        if (spinning || bets.empty()) return false;
        wallet += bets.back().amount;
        bets.pop_back();
        return true;
    }

    void ClearBets(int& wallet) {
        if (spinning) return;
        for (const auto& b : bets) wallet += b.amount;
        bets.clear();
    }

    bool Rebet(int& wallet) {
        if (spinning || lastBets.empty()) return false;
        int need = 0;
        for (const auto& b : lastBets) need += b.amount;
        if (need <= 0 || wallet < need) return false;
        ClearBets(wallet);
        wallet -= need;
        bets = lastBets;
        return true;
    }

    bool StartSpin(int forcedIdx = -1) {
        if (spinning) return false;
        spinning = true;
        spinTimer = 0.0f;
        spinDur = 1.95f;
        targetIdx = (forcedIdx >= 0 && forcedIdx < 37) ? forcedIdx : (int)(NextRand() % 37u);
        targetNum = ROULETTE_ORDER[(size_t)targetIdx];
        return true;
    }

    // Возвращает true в кадр завершения спина
    bool Step(float dt, int& wallet) {
        if (!spinning) {
            wheelDeg = std::fmod(wheelDeg + dt * 12.0f, 360.0f);
            return false;
        }
        spinTimer += dt;
        float t = std::min(1.0f, spinTimer / std::max(0.01f, spinDur));
        float ease = 1.0f - std::pow(1.0f - t, 3.0f);
        float sectorDeg = (float)targetIdx * (360.0f / 37.0f);
        wheelDeg = std::fmod(ease * 720.0f + sectorDeg, 360.0f);
        ballDeg  = std::fmod(-ease * 1260.0f + sectorDeg, 360.0f);
        if (t < 1.0f) return false;

        spinning = false;
        lastNumber = targetNum;
        history.insert(history.begin(), targetNum);
        if (history.size() > 14) history.pop_back();

        int wager = CurrentWager();
        int payout = 0;
        for (const auto& b : bets) {
            if (IsBetWin(b, targetNum)) payout += b.amount * BetMultiplier(b.type);
        }
        wallet += payout;
        ++spins;
        totalWagered += wager;
        totalWon += payout;
        if (payout > bestWin) bestWin = payout;
        lastPayout = payout;
        lastNet = payout - wager;
        if (!bets.empty()) lastBets = bets;
        bets.clear();
        return true;
    }
};

// ============================================================================
// 2. PIXEL ARCADE (4 локальные игры: Змейка, Арканоид, Куро-Ран, Пинг-Понг)
// ============================================================================

enum ArcadeGameKind {
    GAME_SNAKE = 0,
    GAME_BREAKOUT = 1,
    GAME_DINO = 2,
    GAME_PONG = 3,
    GAME_COUNT = 4
};

struct ArcadeRect {
    float x = 0.0f, y = 0.0f, w = 10.0f, h = 10.0f;
    uint8_t colorTag = 0; // 0=cyan,1=pink,2=gold,3=green,4=red,5=white
};

struct PixelArcadeSim {
    static constexpr float VIEW_W = 480.0f;
    static constexpr float VIEW_H = 320.0f;
    static constexpr int SNAKE_COLS = 24;
    static constexpr int SNAKE_ROWS = 16;

    ArcadeGameKind kind = GAME_SNAKE;
    uint32_t rng = 0xA2CADEu;
    bool paused = false;
    bool over = false;
    bool won = false;
    bool rewardClaimed = false;
    int score = 0;
    int lives = 3;
    std::array<int, GAME_COUNT> highScores{};

    // --- Snake ---
    struct Cell { int x = 0, y = 0; };
    std::vector<Cell> snakeBody;
    int snakeDirX = 1, snakeDirY = 0;
    int snakeNextX = 1, snakeNextY = 0;
    Cell snakeFood{15, 8};
    float snakeTick = 0.0f;

    // --- Breakout ---
    float padX = 200.0f, padW = 84.0f;
    float ballX = 240.0f, ballY = 230.0f, ballVx = 175.0f, ballVy = -195.0f;
    struct Brick { float x, y, w, h; bool alive; uint8_t row; };
    std::vector<Brick> bricks;

    // --- Dino Run ---
    float dinoY = 240.0f, dinoVy = 0.0f;
    bool dinoOnGround = true;
    float dinoSpeed = 215.0f;
    float dinoDistAcc = 0.0f;
    struct Cactus { float x, w, h; bool passed; };
    std::vector<Cactus> cacti;
    float cactusSpawnT = 0.9f;

    // --- Pong ---
    float pongP1Y = 128.0f, pongAiY = 128.0f;
    int pongP1Score = 0, pongAiScore = 0;

    uint32_t NextRand() {
        uint32_t x = rng;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        rng = x ? x : 0xA2CADEu;
        return rng;
    }

    static const char* GameTitle(ArcadeGameKind k) {
        switch (k) {
            case GAME_SNAKE:    return "01. ЗМЕЙКА";
            case GAME_BREAKOUT: return "02. АРКАНОИД";
            case GAME_DINO:     return "03. КУРО-РАН";
            case GAME_PONG:     return "04. PONG vs ИИ";
            default:            return "ARCADE";
        }
    }

    static const char* GameControls(ArcadeGameKind k) {
        switch (k) {
            case GAME_SNAKE:    return "WASD / Стрелки — поворот  •  SPACE — пауза";
            case GAME_BREAKOUT: return "A/D или Стрелки — платформа  •  SPACE — пауза";
            case GAME_DINO:     return "W / SPACE / Вверх — прыжок  •  S / Вниз — пике";
            case GAME_PONG:     return "W/S или Стрелки — ракетка (до 5 очков)";
            default:            return "";
        }
    }

    void SpawnSnakeFood() {
        for (int attempt = 0; attempt < 256; ++attempt) {
            int fx = (int)(NextRand() % (uint32_t)SNAKE_COLS);
            int fy = (int)(NextRand() % (uint32_t)SNAKE_ROWS);
            bool hit = false;
            for (const auto& c : snakeBody) {
                if (c.x == fx && c.y == fy) { hit = true; break; }
            }
            if (!hit) { snakeFood = {fx, fy}; return; }
        }
        snakeFood = {0, 0};
    }

    void SpawnBreakoutBricks() {
        bricks.clear();
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 8; ++c) {
                bricks.push_back(Brick{
                    24.0f + c * 55.0f,
                    36.0f + r * 22.0f,
                    48.0f, 15.0f, true, (uint8_t)r
                });
            }
        }
    }

    void Reset(ArcadeGameKind newKind) {
        kind = newKind;
        paused = false;
        over = false;
        won = false;
        rewardClaimed = false;
        score = 0;
        lives = 3;

        if (kind == GAME_SNAKE) {
            snakeBody = {{10, 8}, {9, 8}, {8, 8}};
            snakeDirX = 1; snakeDirY = 0;
            snakeNextX = 1; snakeNextY = 0;
            snakeTick = 0.0f;
            SpawnSnakeFood();
        } else if (kind == GAME_BREAKOUT) {
            padX = 198.0f; padW = 84.0f;
            ballX = 240.0f; ballY = 236.0f;
            ballVx = 175.0f; ballVy = -205.0f;
            SpawnBreakoutBricks();
        } else if (kind == GAME_DINO) {
            dinoY = 240.0f; dinoVy = 0.0f; dinoOnGround = true;
            dinoSpeed = 220.0f; dinoDistAcc = 0.0f;
            cacti.clear();
            cactusSpawnT = 0.85f;
        } else if (kind == GAME_PONG) {
            pongP1Y = 128.0f; pongAiY = 128.0f;
            pongP1Score = 0; pongAiScore = 0;
            ballX = 240.0f; ballY = 160.0f;
            ballVx = -220.0f; ballVy = 135.0f;
        }
    }

    void AddScore(int pts) {
        score += pts;
        size_t idx = (size_t)std::clamp((int)kind, 0, (int)GAME_COUNT - 1);
        if (score > highScores[idx]) highScores[idx] = score;
    }

    void FinishRun(bool isWin) {
        over = true;
        won = isWin;
        size_t idx = (size_t)std::clamp((int)kind, 0, (int)GAME_COUNT - 1);
        if (score > highScores[idx]) highScores[idx] = score;
    }

    static int ComputeCreditsReward(int sc, bool isWin) {
        if (sc < 10 && !isWin) return 0;
        int cr = sc / 20 + (isWin ? 15 : 0);
        return std::clamp(cr, 5, 50);
    }

    void SetSnakeDir(int dx, int dy) {
        if (dx != 0 && snakeDirX == -dx) return;
        if (dy != 0 && snakeDirY == -dy) return;
        snakeNextX = dx;
        snakeNextY = dy;
    }

    void Step(float dt, int moveX, int moveY, bool actionJump) {
        if (paused || over) return;
        dt = std::min(dt, 0.05f);

        if (kind == GAME_SNAKE) {
            if (moveX < 0) SetSnakeDir(-1, 0);
            else if (moveX > 0) SetSnakeDir(1, 0);
            else if (moveY < 0) SetSnakeDir(0, -1);
            else if (moveY > 0) SetSnakeDir(0, 1);

            float stepPeriod = std::max(0.065f, 0.135f - (float)(score / 20) * 0.005f);
            snakeTick += dt;
            while (snakeTick >= stepPeriod && !over) {
                snakeTick -= stepPeriod;
                snakeDirX = snakeNextX;
                snakeDirY = snakeNextY;
                Cell head = snakeBody.front();
                head.x += snakeDirX;
                head.y += snakeDirY;
                if (head.x < 0 || head.x >= SNAKE_COLS || head.y < 0 || head.y >= SNAKE_ROWS) {
                    FinishRun(false);
                    return;
                }
                bool eat = (head.x == snakeFood.x && head.y == snakeFood.y);
                size_t checkLen = eat ? snakeBody.size() : (snakeBody.size() > 0 ? snakeBody.size() - 1 : 0);
                for (size_t i = 0; i < checkLen; ++i) {
                    if (snakeBody[i].x == head.x && snakeBody[i].y == head.y) {
                        FinishRun(false);
                        return;
                    }
                }
                snakeBody.insert(snakeBody.begin(), head);
                if (eat) {
                    AddScore(15);
                    SpawnSnakeFood();
                } else {
                    snakeBody.pop_back();
                }
            }
            return;
        }

        if (kind == GAME_BREAKOUT) {
            padX = std::clamp(padX + (float)moveX * 330.0f * dt, 8.0f, VIEW_W - padW - 8.0f);
            ballX += ballVx * dt;
            ballY += ballVy * dt;
            if (ballX <= 6.0f) { ballX = 6.0f; ballVx = std::fabs(ballVx); }
            if (ballX >= VIEW_W - 6.0f) { ballX = VIEW_W - 6.0f; ballVx = -std::fabs(ballVx); }
            if (ballY <= 6.0f) { ballY = 6.0f; ballVy = std::fabs(ballVy); }

            // Отскок от платформы
            if (ballVy > 0.0f && ballY >= 282.0f && ballY <= 298.0f &&
                ballX >= padX - 6.0f && ballX <= padX + padW + 6.0f) {
                float hit = ((ballX - (padX + padW * 0.5f)) / (padW * 0.5f));
                hit = std::clamp(hit, -1.0f, 1.0f);
                float spd = std::min(380.0f, std::hypot(ballVx, ballVy) * 1.02f);
                ballVx = hit * spd * 0.84f;
                ballVy = -std::sqrt(std::max(10000.0f, spd * spd - ballVx * ballVx));
                ballY = 281.5f;
            }

            // Проверка кирпичей
            bool anyAlive = false;
            for (auto& b : bricks) {
                if (!b.alive) continue;
                anyAlive = true;
                if (ballX + 5.0f >= b.x && ballX - 5.0f <= b.x + b.w &&
                    ballY + 5.0f >= b.y && ballY - 5.0f <= b.y + b.h) {
                    b.alive = false;
                    ballVy = -ballVy;
                    AddScore(15);
                    break;
                }
            }
            if (!anyAlive) {
                AddScore(100);
                SpawnBreakoutBricks();
            }

            if (ballY > VIEW_H + 10.0f) {
                --lives;
                if (lives <= 0) {
                    FinishRun(false);
                } else {
                    ballX = padX + padW * 0.5f;
                    ballY = 240.0f;
                    ballVx = 175.0f;
                    ballVy = -200.0f;
                }
            }
            return;
        }

        if (kind == GAME_DINO) {
            constexpr float GROUND_Y = 240.0f;
            if (actionJump && dinoOnGround) {
                dinoVy = -450.0f;
                dinoOnGround = false;
            }
            if (!dinoOnGround) {
                float grav = (moveY > 0) ? 2100.0f : 1280.0f;
                dinoVy += grav * dt;
                dinoY += dinoVy * dt;
                if (dinoY >= GROUND_Y) {
                    dinoY = GROUND_Y;
                    dinoVy = 0.0f;
                    dinoOnGround = true;
                }
            }
            dinoSpeed = std::min(430.0f, dinoSpeed + dt * 6.5f);
            dinoDistAcc += dt * 14.0f;
            if (dinoDistAcc >= 1.0f) {
                int pts = (int)dinoDistAcc;
                dinoDistAcc -= (float)pts;
                AddScore(pts);
            }
            cactusSpawnT -= dt;
            if (cactusSpawnT <= 0.0f) {
                cactusSpawnT = 0.90f + (float)(NextRand() % 80u) * 0.009f;
                float ch = (NextRand() % 2u) ? 42.0f : 28.0f;
                float cw = (NextRand() % 2u) ? 20.0f : 14.0f;
                cacti.push_back(Cactus{VIEW_W + 20.0f, cw, ch, false});
            }
            constexpr float DX = 64.0f, DW = 26.0f, DH = 32.0f;
            float dyTop = dinoY;
            for (auto& c : cacti) {
                c.x -= dinoSpeed * dt;
                float cyTop = (GROUND_Y + DH) - c.h;
                if (DX + DW - 4.0f > c.x && DX + 4.0f < c.x + c.w &&
                    dyTop + DH - 4.0f > cyTop && dyTop + 4.0f < GROUND_Y + DH) {
                    FinishRun(false);
                    return;
                }
                if (!c.passed && c.x + c.w < DX) {
                    c.passed = true;
                    AddScore(15);
                }
            }
            cacti.erase(std::remove_if(cacti.begin(), cacti.end(),
                                       [](const Cactus& c) { return c.x + c.w < -20.0f; }),
                        cacti.end());
            return;
        }

        if (kind == GAME_PONG) {
            constexpr float PADDLE_H = 68.0f;
            pongP1Y = std::clamp(pongP1Y + (float)moveY * 290.0f * dt, 10.0f, VIEW_H - PADDLE_H - 10.0f);
            float aiCenter = pongAiY + PADDLE_H * 0.5f;
            float aiSpeed = 215.0f;
            if (ballY < aiCenter - 6.0f) pongAiY -= aiSpeed * dt;
            else if (ballY > aiCenter + 6.0f) pongAiY += aiSpeed * dt;
            pongAiY = std::clamp(pongAiY, 10.0f, VIEW_H - PADDLE_H - 10.0f);

            ballX += ballVx * dt;
            ballY += ballVy * dt;
            if (ballY <= 8.0f) { ballY = 8.0f; ballVy = std::fabs(ballVy); }
            if (ballY >= VIEW_H - 8.0f) { ballY = VIEW_H - 8.0f; ballVy = -std::fabs(ballVy); }

            // Левая ракетка игрока
            if (ballVx < 0.0f && ballX <= 30.0f && ballX >= 14.0f &&
                ballY >= pongP1Y - 6.0f && ballY <= pongP1Y + PADDLE_H + 6.0f) {
                float hit = std::clamp((ballY - (pongP1Y + PADDLE_H * 0.5f)) / (PADDLE_H * 0.5f), -1.0f, 1.0f);
                ballVx = std::min(410.0f, std::fabs(ballVx) * 1.06f);
                ballVy = hit * 240.0f;
                ballX = 30.5f;
                AddScore(10);
            }
            // Правая ракетка ИИ
            if (ballVx > 0.0f && ballX >= VIEW_W - 30.0f && ballX <= VIEW_W - 14.0f &&
                ballY >= pongAiY - 6.0f && ballY <= pongAiY + PADDLE_H + 6.0f) {
                float hit = std::clamp((ballY - (pongAiY + PADDLE_H * 0.5f)) / (PADDLE_H * 0.5f), -1.0f, 1.0f);
                ballVx = -std::min(410.0f, std::fabs(ballVx) * 1.05f);
                ballVy = hit * 240.0f;
                ballX = VIEW_W - 30.5f;
            }
            if (ballX < 0.0f) {
                ++pongAiScore;
                if (pongAiScore >= 5) { FinishRun(false); return; }
                ballX = 240.0f; ballY = 160.0f; ballVx = 215.0f; ballVy = 110.0f;
            } else if (ballX > VIEW_W) {
                ++pongP1Score;
                AddScore(45);
                if (pongP1Score >= 5) { FinishRun(true); return; }
                ballX = 240.0f; ballY = 160.0f; ballVx = -215.0f; ballVy = -110.0f;
            }
        }
    }

    std::vector<ArcadeRect> BuildDrawRects() const {
        std::vector<ArcadeRect> out;
        if (kind == GAME_SNAKE) {
            constexpr float CS = 20.0f;
            out.push_back(ArcadeRect{(float)snakeFood.x * CS + 2.0f, (float)snakeFood.y * CS + 2.0f, CS - 4.0f, CS - 4.0f, 1});
            for (size_t i = 0; i < snakeBody.size(); ++i) {
                uint8_t col = (i == 0) ? 2 : 3;
                out.push_back(ArcadeRect{(float)snakeBody[i].x * CS + 1.5f, (float)snakeBody[i].y * CS + 1.5f, CS - 3.0f, CS - 3.0f, col});
            }
        } else if (kind == GAME_BREAKOUT) {
            for (const auto& b : bricks) {
                if (b.alive) {
                    uint8_t tag = (b.row == 0) ? 1 : (b.row == 1) ? 2 : (b.row == 2) ? 0 : 3;
                    out.push_back(ArcadeRect{b.x, b.y, b.w, b.h, tag});
                }
            }
            out.push_back(ArcadeRect{padX, 286.0f, padW, 11.0f, 0});
            out.push_back(ArcadeRect{ballX - 5.0f, ballY - 5.0f, 10.0f, 10.0f, 5});
        } else if (kind == GAME_DINO) {
            out.push_back(ArcadeRect{0.0f, 272.0f, VIEW_W, 4.0f, 3});
            // Куро-бегун (тело + гребешок + клюв)
            out.push_back(ArcadeRect{64.0f, dinoY + 4.0f, 24.0f, 26.0f, 5});
            out.push_back(ArcadeRect{70.0f, dinoY - 2.0f, 12.0f, 6.0f, 4});
            out.push_back(ArcadeRect{88.0f, dinoY + 10.0f, 8.0f, 6.0f, 2});
            for (const auto& c : cacti) {
                out.push_back(ArcadeRect{c.x, 272.0f - c.h, c.w, c.h, 1});
            }
        } else if (kind == GAME_PONG) {
            for (int y = 8; y < 312; y += 22)
                out.push_back(ArcadeRect{VIEW_W * 0.5f - 2.0f, (float)y, 4.0f, 12.0f, 0});
            out.push_back(ArcadeRect{16.0f, pongP1Y, 12.0f, 68.0f, 0});
            out.push_back(ArcadeRect{VIEW_W - 28.0f, pongAiY, 12.0f, 68.0f, 1});
            out.push_back(ArcadeRect{ballX - 6.0f, ballY - 6.0f, 12.0f, 12.0f, 2});
        }
        return out;
    }
};

} // namespace Arcade
