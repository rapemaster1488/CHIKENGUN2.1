#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Arcade {

// ============================================================================
// PIXEL ARCADE (6 локальных игр: Змейка, Арканоид, Куро-Ран, Пинг-Понг,
// Flappy Кочет, Кибер-Тетрис)
// (Нативное Казино вынесено в src/CasinoSim.h)
// ============================================================================

enum ArcadeGameKind {
    GAME_SNAKE = 0,
    GAME_BREAKOUT = 1,
    GAME_DINO = 2,
    GAME_PONG = 3,
    GAME_FLAPPY = 4,
    GAME_TETRIS = 5,
    GAME_COUNT = 6
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

    // --- Flappy Кочет ---
    float flappyY = 148.0f, flappyVy = 0.0f;
    bool flappyJumpHeld = false;
    struct Pipe { float x, gapY, gapH; bool passed; };
    std::vector<Pipe> pipes;
    float pipeSpawnT = 0.7f;

    // --- Кибер-Тетрис ---
    static constexpr int TET_W = 10;
    static constexpr int TET_H = 20;
    std::array<uint8_t, TET_W * TET_H> tetBoard{};
    int tetCurType = 0, tetNextType = 1, tetRot = 0, tetX = 3, tetY = 0;
    int tetLines = 0;
    float tetFallAcc = 0.0f;

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
            case GAME_FLAPPY:   return "05. FLAPPY КОЧЕТ";
            case GAME_TETRIS:   return "06. КИБЕР-ТЕТРИС";
            default:            return "ARCADE";
        }
    }

    static const char* GameControls(ArcadeGameKind k) {
        switch (k) {
            case GAME_SNAKE:    return "WASD / Стрелки — поворот  •  SPACE — пауза";
            case GAME_BREAKOUT: return "A/D или Стрелки — платформа  •  SPACE — пауза";
            case GAME_DINO:     return "W / SPACE / Вверх — прыжок  •  S / Вниз — пике";
            case GAME_PONG:     return "W/S или Стрелки — ракетка (до 5 очков)";
            case GAME_FLAPPY:   return "SPACE / W / ЛКМ — взмах крылом сквозь трубы";
            case GAME_TETRIS:   return "A/D — сдвиг • W — поворот • S — вниз • SPACE — сброс";
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

    static bool TetCell(int type, int rot, int cx, int cy) {
        // 7 классических тетромино в 4x4 маске (I, O, T, S, Z, J, L)
        static const uint16_t kShapes[7][4] = {
            { 0x0F00, 0x2222, 0x00F0, 0x4444 }, // I
            { 0x6600, 0x6600, 0x6600, 0x6600 }, // O
            { 0x4E00, 0x4640, 0x0E40, 0x4C40 }, // T
            { 0x6C00, 0x4620, 0x06C0, 0x8C40 }, // S
            { 0xC600, 0x2640, 0x0C60, 0x4C80 }, // Z
            { 0x8E00, 0x6440, 0x0E20, 0x44C0 }, // J
            { 0x2E00, 0x4460, 0x0E80, 0xC440 }  // L
        };
        uint16_t mask = kShapes[std::clamp(type, 0, 6)][rot & 3];
        int bit = 15 - (cy * 4 + cx);
        return ((mask >> bit) & 1u) != 0;
    }

    bool TetFits(int type, int rot, int px, int py) const {
        for (int cy = 0; cy < 4; ++cy) {
            for (int cx = 0; cx < 4; ++cx) {
                if (!TetCell(type, rot, cx, cy)) continue;
                int bx = px + cx;
                int by = py + cy;
                if (bx < 0 || bx >= TET_W || by >= TET_H) return false;
                if (by >= 0 && tetBoard[(size_t)(by * TET_W + bx)] != 0) return false;
            }
        }
        return true;
    }

    void SpawnTetPiece() {
        tetCurType = tetNextType;
        tetNextType = (int)(NextRand() % 7u);
        tetRot = 0;
        tetX = 3;
        tetY = 0;
        if (!TetFits(tetCurType, tetRot, tetX, tetY)) {
            FinishRun(false);
        }
    }

    void LockTetPiece() {
        uint8_t colTag = (uint8_t)(1 + (tetCurType % 5));
        for (int cy = 0; cy < 4; ++cy) {
            for (int cx = 0; cx < 4; ++cx) {
                if (!TetCell(tetCurType, tetRot, cx, cy)) continue;
                int bx = tetX + cx, by = tetY + cy;
                if (by >= 0 && by < TET_H && bx >= 0 && bx < TET_W) {
                    tetBoard[(size_t)(by * TET_W + bx)] = colTag;
                }
            }
        }
        int cleared = 0;
        for (int y = TET_H - 1; y >= 0; --y) {
            bool full = true;
            for (int x = 0; x < TET_W; ++x) {
                if (tetBoard[(size_t)(y * TET_W + x)] == 0) { full = false; break; }
            }
            if (full) {
                ++cleared;
                for (int yy = y; yy > 0; --yy) {
                    for (int x = 0; x < TET_W; ++x) {
                        tetBoard[(size_t)(yy * TET_W + x)] = tetBoard[(size_t)((yy - 1) * TET_W + x)];
                    }
                }
                for (int x = 0; x < TET_W; ++x) tetBoard[(size_t)x] = 0;
                ++y;
            }
        }
        if (cleared > 0) {
            static const int kLinePts[5] = { 0, 100, 300, 500, 800 };
            tetLines += cleared;
            AddScore(kLinePts[std::min(4, cleared)]);
        } else {
            AddScore(10);
        }
        SpawnTetPiece();
    }

    void TetrisMove(int dx) {
        if (kind != GAME_TETRIS || paused || over) return;
        if (TetFits(tetCurType, tetRot, tetX + dx, tetY)) tetX += dx;
    }

    void TetrisRotate() {
        if (kind != GAME_TETRIS || paused || over) return;
        int nr = (tetRot + 1) & 3;
        static const int kKicks[5] = { 0, -1, 1, -2, 2 };
        for (int kick : kKicks) {
            if (TetFits(tetCurType, nr, tetX + kick, tetY)) {
                tetRot = nr;
                tetX += kick;
                return;
            }
        }
    }

    void TetrisHardDrop() {
        if (kind != GAME_TETRIS || paused || over) return;
        while (TetFits(tetCurType, tetRot, tetX, tetY + 1)) {
            ++tetY;
            AddScore(2);
        }
        LockTetPiece();
    }

    void FlappyFlap() {
        if (kind != GAME_FLAPPY || paused || over) return;
        flappyVy = -265.0f;
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
        } else if (kind == GAME_FLAPPY) {
            flappyY = 148.0f;
            flappyVy = -140.0f;
            flappyJumpHeld = false;
            pipes.clear();
            pipeSpawnT = 0.55f;
        } else if (kind == GAME_TETRIS) {
            tetBoard.fill(0);
            tetNextType = (int)(NextRand() % 7u);
            tetLines = 0;
            tetFallAcc = 0.0f;
            SpawnTetPiece();
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
            return;
        }

        if (kind == GAME_FLAPPY) {
            if (actionJump && !flappyJumpHeld) FlappyFlap();
            flappyJumpHeld = actionJump;
            flappyVy += 740.0f * dt;
            flappyY += flappyVy * dt;
            if (flappyY < 4.0f) { flappyY = 4.0f; flappyVy = 0.0f; }
            if (flappyY > 280.0f) { FinishRun(false); return; }

            pipeSpawnT -= dt;
            if (pipeSpawnT <= 0.0f) {
                pipeSpawnT = 1.48f;
                float gapH = std::max(72.0f, 102.0f - (float)(score / 60) * 3.0f);
                float gapY = 66.0f + (float)(NextRand() % 130u);
                pipes.push_back(Pipe{VIEW_W + 20.0f, gapY, gapH, false});
            }
            constexpr float FX = 112.0f, FW = 22.0f, FH = 18.0f;
            float speed = std::min(245.0f, 148.0f + (float)(score / 40) * 4.5f);
            for (auto& p : pipes) {
                p.x -= speed * dt;
                if (FX + FW > p.x && FX < p.x + 38.0f) {
                    if (flappyY < p.gapY || flappyY + FH > p.gapY + p.gapH) {
                        FinishRun(false);
                        return;
                    }
                }
                if (!p.passed && p.x + 38.0f < FX) {
                    p.passed = true;
                    AddScore(20);
                }
            }
            pipes.erase(std::remove_if(pipes.begin(), pipes.end(),
                                       [](const Pipe& p) { return p.x + 42.0f < -10.0f; }),
                        pipes.end());
            return;
        }

        if (kind == GAME_TETRIS) {
            float basePeriod = std::max(0.11f, 0.56f - (float)(tetLines / 5) * 0.045f);
            float period = (moveY > 0) ? std::min(basePeriod, 0.055f) : basePeriod;
            tetFallAcc += dt;
            while (tetFallAcc >= period && !over) {
                tetFallAcc -= period;
                if (TetFits(tetCurType, tetRot, tetX, tetY + 1)) {
                    ++tetY;
                } else {
                    LockTetPiece();
                    break;
                }
            }
            return;
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
        } else if (kind == GAME_FLAPPY) {
            out.push_back(ArcadeRect{0.0f, 298.0f, VIEW_W, 22.0f, 1});
            for (const auto& p : pipes) {
                if (p.gapY > 2.0f) {
                    out.push_back(ArcadeRect{p.x, 0.0f, 38.0f, p.gapY, 3});
                    out.push_back(ArcadeRect{p.x - 3.0f, std::max(0.0f, p.gapY - 10.0f), 44.0f, 10.0f, 3});
                }
                float botY = p.gapY + p.gapH;
                if (botY < 298.0f) {
                    out.push_back(ArcadeRect{p.x, botY, 38.0f, 298.0f - botY, 3});
                    out.push_back(ArcadeRect{p.x - 3.0f, botY, 44.0f, 10.0f, 3});
                }
            }
            // Кочет в полёте
            out.push_back(ArcadeRect{112.0f, flappyY, 22.0f, 18.0f, 5});
            out.push_back(ArcadeRect{118.0f, flappyY - 5.0f, 10.0f, 5.0f, 4});
            out.push_back(ArcadeRect{134.0f, flappyY + 6.0f, 7.0f, 5.0f, 2});
        } else if (kind == GAME_TETRIS) {
            constexpr float OX = 165.0f, OY = 10.0f, CS = 15.0f;
            out.push_back(ArcadeRect{OX - 3.0f, OY, 3.0f, TET_H * CS, 0});
            out.push_back(ArcadeRect{OX + TET_W * CS, OY, 3.0f, TET_H * CS, 0});
            out.push_back(ArcadeRect{OX - 3.0f, OY + TET_H * CS, TET_W * CS + 6.0f, 3.0f, 0});
            for (int y = 0; y < TET_H; ++y) {
                for (int x = 0; x < TET_W; ++x) {
                    uint8_t cell = tetBoard[(size_t)(y * TET_W + x)];
                    if (cell != 0) {
                        out.push_back(ArcadeRect{OX + x * CS + 1.0f, OY + y * CS + 1.0f, CS - 2.0f, CS - 2.0f, cell});
                    }
                }
            }
            uint8_t curTag = (uint8_t)(1 + (tetCurType % 5));
            for (int cy = 0; cy < 4; ++cy) {
                for (int cx = 0; cx < 4; ++cx) {
                    if (!TetCell(tetCurType, tetRot, cx, cy)) continue;
                    int bx = tetX + cx, by = tetY + cy;
                    if (by >= 0 && by < TET_H && bx >= 0 && bx < TET_W) {
                        out.push_back(ArcadeRect{OX + bx * CS + 1.0f, OY + by * CS + 1.0f, CS - 2.0f, CS - 2.0f, curTag});
                    }
                }
            }
            // Превью следующей фигуры справа
            constexpr float PX = 355.0f, PY = 48.0f;
            uint8_t nxtTag = (uint8_t)(1 + (tetNextType % 5));
            for (int cy = 0; cy < 4; ++cy) {
                for (int cx = 0; cx < 4; ++cx) {
                    if (TetCell(tetNextType, 0, cx, cy)) {
                        out.push_back(ArcadeRect{PX + cx * 13.0f, PY + cy * 13.0f, 11.0f, 11.0f, nxtTag});
                    }
                }
            }
        }
        return out;
    }
};

} // namespace Arcade
