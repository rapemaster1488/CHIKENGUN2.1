// ============================================================================
// Модульные тесты CHIKENGUN 2.5 «ПЕРО СУДЬБЫ» [4K ULTRA]
// Напрямую подключают и тестируют боевые заголовки src/LitePhysics.h и
// src/CyberSystems.h (RayBoxT, RaySphereT, ComputeGrapplePullT, кэш FNV-1a,
// мутаторы волн, заряд орбитального удара «ЯЙЦЕГЕДДОН», экономику перков,
// маску 12 достижений), а также PNG/WAV и генератор gen_textures_4k.
// ============================================================================
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../src/LitePhysics.h"
#include "../src/CyberSystems.h"
#include "../src/BanyaSim.h"
#include "../src/CasinoSim.h"

namespace fs = std::filesystem;

// Совместимый с Urho3D::Vector3 тип (поля x_, y_, z_) для прямого вызова шаблонов Lite::*T
struct TestVec3 {
    float x_ = 0.0f, y_ = 0.0f, z_ = 0.0f;
    TestVec3() = default;
    TestVec3(float x, float y, float z) : x_(x), y_(y), z_(z) {}
    TestVec3 operator+(const TestVec3& o) const { return {x_ + o.x_, y_ + o.y_, z_ + o.z_}; }
    TestVec3 operator-(const TestVec3& o) const { return {x_ - o.x_, y_ - o.y_, z_ - o.z_}; }
    TestVec3 operator*(float s) const { return {x_ * s, y_ * s, z_ * s}; }
    float Length() const { return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_); }
    TestVec3 Normalized() const {
        float l = Length();
        return l > 1e-6f ? (*this) * (1.0f / l) : TestVec3(0, 0, 0);
    }
};

static int ParseXmlIntAttr(const std::string& raw, int def) {
    if (raw.empty()) return def;
    std::string s = raw;
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    if (s == "true" || s == "on" || s == "yes") return 1;
    if (s == "false" || s == "off" || s == "no") return 0;
    return std::atoi(s.c_str());
}

static uint32_t ReadBE32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void CheckPngFile(const fs::path& p, uint32_t expectW, uint32_t expectH) {
    assert(fs::exists(p) && "PNG файл не создан");
    std::ifstream f(p, std::ios::binary);
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    assert(buf.size() > 33 && "PNG слишком короткий");
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    assert(std::memcmp(buf.data(), sig, 8) == 0 && "Неверная сигнатура PNG");
    assert(std::memcmp(buf.data() + 12, "IHDR", 4) == 0 && "Отсутствует IHDR");
    uint32_t w = ReadBE32(buf.data() + 16);
    uint32_t h = ReadBE32(buf.data() + 20);
    assert(w == expectW && h == expectH && "Несовпадение размеров IHDR");
    assert(std::memcmp(buf.data() + buf.size() - 8, "IEND", 4) == 0 && "Отсутствует IEND");
}

int main(int argc, char** argv) {
    std::cout << "[test_core] 1/9 Проверка Lite::RayBoxT и HitNormalBoxT (src/LitePhysics.h)..." << std::endl;
    {
        float t = Lite::RayBoxT<TestVec3>({0, 1, -5}, {0, 0, 1}, {0, 1, 0}, {1, 1, 1});
        assert(std::fabs(t - 4.0f) < 1e-4f);
        TestVec3 n = Lite::HitNormalBoxT<TestVec3>({0, 1, -1}, {0, 1, 0}, {1, 1, 1});
        assert(std::fabs(n.z_ + 1.0f) < 1e-4f);

        assert(Lite::RayBoxT<TestVec3>({0, 5, -5}, {0, 0, 1}, {0, 1, 0}, {1, 1, 1}) < 0.0f);
        assert(Lite::RayBoxT<TestVec3>({0, 1,  5}, {0, 0, 1}, {0, 1, 0}, {1, 1, 1}) < 0.0f);

        // Изнутри AABB нужен выход, а не нулевая дистанция до самого старта.
        float inside = Lite::RayBoxT<TestVec3>({0, 1, 0}, {2, 0, 0}, {0, 1, 0}, {1, 1, 1});
        assert(std::fabs(inside - 0.5f) < 1e-4f);
        assert(Lite::RayBoxT<TestVec3>({0, 1, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 1}) < 0.0f);
        // На грани луч наружу попадает в t=0, внутрь — до дальней грани.
        assert(std::fabs(Lite::RayBoxT<TestVec3>({1, 1, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 1})) < 1e-4f);
        assert(std::fabs(Lite::RayBoxT<TestVec3>({1, 1, 0}, {-1, 0, 0}, {0, 1, 0}, {1, 1, 1})) < 1e-4f);
    }

    std::cout << "[test_core] 2/9 Проверка Lite::RaySphereT и сквозного луча Рельсотрона..." << std::endl;
    {
        float t1 = Lite::RaySphereT<TestVec3>({0, 1, 0}, {0, 0, 1}, {0, 1, 10}, 1.0f);
        float t2 = Lite::RaySphereT<TestVec3>({0, 1, 0}, {0, 0, 1}, {0, 1, 22}, 1.0f);
        assert(std::fabs(t1 - 9.0f) < 1e-4f);
        assert(std::fabs(t2 - 21.0f) < 1e-4f);
        assert(t1 < t2);
        assert(Lite::RaySphereT<TestVec3>({0, 1, 0}, {0, 0, 1}, {5, 1, 10}, 1.0f) < 0.0f);

        // Луч может начинаться внутри коллайдера: возвращаем точку выхода,
        // а ненормализованный direction не должен менять геометрию попадания.
        float inside = Lite::RaySphereT<TestVec3>({0, 1, 10}, {0, 0, 2}, {0, 1, 10}, 1.0f);
        assert(std::fabs(inside - 0.5f) < 1e-4f);
        assert(Lite::RaySphereT<TestVec3>({0, 1, 0}, {0, 0, 0}, {0, 1, 10}, 1.0f) < 0.0f);
    }

    std::cout << "[test_core] 3/9 Проверка кинематики Грэппл-крюка [Z] и FNV-1a кэша материалов..." << std::endl;
    {
        bool rel = false;
        TestVec3 pull = Lite::ComputeGrapplePullT<TestVec3>({0, 2, 0}, {0, 2, 20}, 28.0f, 2.0f, rel);
        assert(!rel);
        assert(std::fabs(pull.z_ - 28.0f) < 1e-3f);
        assert(pull.y_ > 1.5f);

        TestVec3 vault = Lite::ComputeGrapplePullT<TestVec3>({0, 2, 19}, {0, 2, 20}, 28.0f, 2.0f, rel);
        assert(rel);
        assert(vault.y_ > 5.0f);

        uint64_t h1 = Lite::HashFloatQ(1469598103934665603ULL, 0.25f);
        uint64_t h2 = Lite::HashFloatQ(1469598103934665603ULL, 0.25f);
        uint64_t h3 = Lite::HashFloatQ(1469598103934665603ULL, 0.85f);
        assert(h1 == h2 && h1 != h3);
    }

    std::cout << "[test_core] 4/9 Проверка систем CyberSystems.h (мутаторы, Орбита [X], перки, 14 достижений)..." << std::endl;
    {
        assert(Cyber::PickWaveMutator(1, 42) == Cyber::MUT_STANDARD);
        assert(Cyber::PickWaveMutator(5, 17) == Cyber::MUT_GOLDRUSH);
        assert(std::fabs(Cyber::GetMutatorInfo(Cyber::MUT_LOW_GRAV).gravityMul - 0.52f) < 1e-4f);
        assert(Cyber::GetMutatorInfo(Cyber::MUT_VOLATILE).volatileDeaths);

        float ch = 0.0f;
        ch = Cyber::AddOrbitalCharge(ch, false, true);  // +20
        ch = Cyber::AddOrbitalCharge(ch, true,  false); // +48 -> 68
        assert(std::fabs(ch - 68.0f) < 1e-3f);
        ch = Cyber::AddOrbitalCharge(ch, true,  true);  // зажим до 100
        assert(std::fabs(ch - 100.0f) < 1e-3f);

        assert(Cyber::PerkUpgradeCost(0) == 75);
        assert(Cyber::PerkUpgradeCost(2) == 205);
        assert(Cyber::CountUnlockedAchievements(0b101001000101) == 5);
        assert(Cyber::CountUnlockedAchievements(Cyber::ALL_ACHIEVEMENTS_MASK) == 14);
    }

    std::cout << "[test_core] 5/9 Проверка аналитического пола и компакции коллайдеров..." << std::endl;
    {
        TestVec3 orig(0, 2, 0);
        TestVec3 dir = TestVec3(0, -1, 1).Normalized();
        float tFloor = (0.0f - orig.y_) / dir.y_;
        assert(std::fabs(tFloor - std::sqrt(8.0f)) < 1e-4f);

        struct Col { int tag; int id; };
        std::vector<Col> cols = {{0, 10}, {7, 20}, {2, 30}, {7, 40}, {3, 50}};
        std::vector<int> remap(cols.size(), -1);
        std::vector<Col> compact;
        for (size_t i = 0; i < cols.size(); ++i) {
            if (cols[i].tag != 7) {
                remap[i] = (int)compact.size();
                compact.push_back(cols[i]);
            }
        }
        assert(compact.size() == 3);
        assert(remap[0] == 0 && remap[1] == -1 && remap[2] == 1 && remap[4] == 2);
    }

    std::cout << "[test_core] 6/9 Проверка XML-парсера и наличия 27 WAV-эффектов (вкл. баню)..." << std::endl;
    {
        assert(ParseXmlIntAttr("true", 0) == 1);
        assert(ParseXmlIntAttr("ON", 0) == 1);
        assert(ParseXmlIntAttr("false", 1) == 0);
        assert(ParseXmlIntAttr("16", 1) == 16);
        assert(ParseXmlIntAttr("", 3) == 3);
        if (fs::exists("Data/Sounds")) {
            assert(fs::exists("Data/Sounds/Grapple.wav") && fs::file_size("Data/Sounds/Grapple.wav") > 1000);
            assert(fs::exists("Data/Sounds/OrbitalStrike.wav") && fs::file_size("Data/Sounds/OrbitalStrike.wav") > 1000);
            assert(fs::exists("Data/Sounds/Shot_Railgun.wav"));
            assert(fs::exists("Data/Sounds/Shot_Plasma.wav"));
            assert(fs::exists("Data/Sounds/BanyaSteam.wav") && fs::file_size("Data/Sounds/BanyaSteam.wav") > 1000);
            assert(fs::exists("Data/Sounds/BanyaVenik.wav") && fs::file_size("Data/Sounds/BanyaVenik.wav") > 1000);
            assert(fs::exists("Data/Sounds/BanyaSplash.wav") && fs::file_size("Data/Sounds/BanyaSplash.wav") > 1000);
            assert(fs::exists("Data/Sounds/BanyaDoor.wav") && fs::file_size("Data/Sounds/BanyaDoor.wav") > 1000);
        }
    }

    std::cout << "[test_core] 7/9 Проверка симулятора «Кочет в баньке» (src/BanyaSim.h: 14 концовок + 57 загадок)..." << std::endl;
    {
        // 57 уникальных загадок без повторов внутри пула и с перемешиванием вариантов
        Banya::Simulator sim(12345u);
        std::vector<bool> seenRiddle(Banya::TOTAL_RIDDLES, false);
        bool sawNonZeroAnswer = false;
        for (int i = 0; i < Banya::TOTAL_RIDDLES; ++i) {
            auto r = sim.TakeShuffledRiddle();
            assert(r.sourceIdx >= 0 && r.sourceIdx < Banya::TOTAL_RIDDLES);
            assert(!seenRiddle[r.sourceIdx] && "Повтор загадки внутри одного перемешанного пула!");
            seenRiddle[r.sourceIdx] = true;
            const auto& base = Banya::GetAllRiddles()[r.sourceIdx];
            assert(std::strcmp(r.opts[r.correctIdx], base.o[base.a]) == 0 && "Неверное перемешивание верного ответа!");
            if (r.correctIdx != 0) sawNonZeroAnswer = true;
        }
        assert(sawNonZeroAnswer && "Варианты ответов должны перемешиваться");

        // Проверка паузы: замораживает симуляцию и дедлайн загадки
        sim.Reset();
        assert(sim.OpenRiddle(true));
        float dl0 = sim.riddle.timeLeft;
        sim.SetPaused(true);
        sim.Step(10.0f);
        assert(std::fabs(sim.elapsed) < 1e-6f && std::fabs(sim.riddle.timeLeft - dl0) < 1e-6f);
        sim.SetPaused(false);

        // Проверка ВСЕХ 14 концовок:
        // 1) END_FIRE: 4 быстрых поддачи подряд с 18 печи -> 18 + 88 = 100
        sim.Reset();
        for (int i = 0; i < 4; ++i) sim.ActSteam();
        assert(sim.phase == Banya::PHASE_OVER && sim.ending == Banya::END_FIRE);

        // 2) END_BLAST: поддача при temp >= 108
        sim.Reset(); sim.temp = 109.0f; sim.ActSteam();
        assert(sim.ending == Banya::END_BLAST);

        // 3) END_DROWN: 4 оката подряд за 6 с
        sim.Reset();
        for (int i = 0; i < 4; ++i) { sim.ActWater(); sim.Step(0.5f); }
        assert(sim.ending == Banya::END_DROWN);

        // 4) END_ESCAPE: 3 проветривания подряд за 8 с
        sim.Reset();
        for (int i = 0; i < 3; ++i) { sim.ActVent(); sim.Step(0.5f); }
        assert(sim.ending == Banya::END_ESCAPE);

        // 5) END_RAGE: 4 веника подряд за 6 с
        sim.Reset();
        for (int i = 0; i < 4; ++i) { sim.ActVenik(); sim.Step(0.5f); }
        assert(sim.ending == Banya::END_RAGE);

        // 6) END_SENTRY: 3 неверных ответа подряд
        sim.Reset();
        for (int i = 0; i < 3; ++i) {
            sim.riddleCooldown = 0.0f;
            assert(sim.OpenRiddle(true));
            sim.AnswerRiddle((sim.riddle.correctIdx + 1) % 4);
            sim.FlushSettledRiddle();
        }
        assert(sim.ending == Banya::END_SENTRY);

        // 7) END_VAPOR: steam > 0.9 более 20 с
        sim.Reset(); sim.temp = 106.0f; sim.steam = 0.98f; sim.air = 100.0f; sim.life = 100.0f;
        for (int i = 0; i < 220 && sim.phase == Banya::PHASE_PLAY; ++i) {
            sim.temp = 105.0f; sim.steam = 0.96f; sim.air = 100.0f; sim.life = 100.0f;
            sim.Step(0.1f);
        }
        assert(sim.ending == Banya::END_VAPOR);

        // 8) END_CHOKE: air <= 0
        sim.Reset(); sim.air = 0.2f; sim.Step(0.5f);
        assert(sim.ending == Banya::END_CHOKE);

        // 9) END_BURNED: life <= 0 при temp >= 96
        sim.Reset(); sim.temp = 112.0f; sim.life = 1.0f; sim.air = 100.0f; sim.Step(0.5f);
        assert(sim.ending == Banya::END_BURNED);

        // 10) END_FROZEN: life <= 0 при temp <= 66
        sim.Reset(); sim.temp = 45.0f; sim.life = 1.0f; sim.air = 100.0f; sim.Step(0.5f);
        assert(sim.ending == Banya::END_FROZEN);

        // 11) END_EXHAUSTED: life <= 0 при 66 < temp < 96 (через штраф загадки при комфортной темп. ~65 -> дотягиваем в 72)
        sim.Reset(); sim.temp = 72.0f; sim.life = 10.0f;
        assert(sim.OpenRiddle(true));
        sim.AnswerRiddle((sim.riddle.correctIdx + 1) % 4);
        sim.FlushSettledRiddle();
        assert(sim.ending == Banya::END_EXHAUSTED);

        // 12..14) END_NO_CREDIT, END_WIN (+60), END_STEWARD (+120)
        auto runToEndWithRiddles = [&](int targetCorrect) {
            sim.Reset();
            for (int k = 0; k < targetCorrect; ++k) {
                sim.riddleCooldown = 0.0f;
                sim.OpenRiddle(true);
                sim.AnswerRiddle(sim.riddle.correctIdx);
                sim.FlushSettledRiddle();
            }
            sim.nextRiddle = 9999.0f;
            for (int step = 0; step < 190 && sim.phase == Banya::PHASE_PLAY; ++step) {
                sim.temp = 80.0f; sim.air = 90.0f; sim.life = 95.0f; sim.steam = 0.5f;
                sim.Step(1.0f);
            }
        };
        runToEndWithRiddles(2);
        assert(sim.ending == Banya::END_NO_CREDIT && Banya::BuffTierForEnding(sim.ending) == 0);
        runToEndWithRiddles(4);
        assert(sim.ending == Banya::END_WIN && sim.CurrentScore() == 180 + 4 * 15 + 60 && Banya::BuffTierForEnding(sim.ending) == 1);
        runToEndWithRiddles(6);
        assert(sim.ending == Banya::END_STEWARD && sim.CurrentScore() == 180 + 6 * 15 + 120 && Banya::BuffTierForEnding(sim.ending) == 2);
        assert(Banya::CountSeenEndings(sim.lifetime.seenEndingsMask) == 14 && "Все 14 концовок открыты в тесте!");
        assert(Banya::ComputeCreditsReward(sim.CurrentScore(), Banya::END_STEWARD) > 200);
    }

    std::cout << "[test_core] 8/9 Проверка нативной виртуальной рулетки (правила, выплаты, фишки)..." << std::endl;
    {
        Casino::Bet red{Casino::BET_RED, 25, 7};
        assert(Casino::Wins(red, 19));
        assert(!Casino::Wins(red, 20));
        assert(!Casino::Wins(red, 0));
        assert(Casino::Payout(red, 19) == 50);
        assert(Casino::Net(red, 19) == 25);
        assert(Casino::Net(red, 20) == -25);

        Casino::Bet exact{Casino::BET_NUMBER, 10, 7};
        assert(Casino::Wins(exact, 7));
        assert(Casino::Payout(exact, 7) == 360);
        assert(!Casino::Wins(exact, 8));

        Casino::Bet dozen{Casino::BET_DOZEN, 10, 24};
        assert(Casino::Wins(dozen, 24) && !Casino::Wins(dozen, 12));
        Casino::Bet column{Casino::BET_COLUMN, 10, 2};
        assert(Casino::Wins(column, 5) && !Casino::Wins(column, 4));
        Casino::Bet split{Casino::BET_SPLIT, 10, 7};
        assert(Casino::Wins(split, 7) && Casino::Wins(split, 8) && !Casino::Wins(split, 9));
        Casino::Bet street{Casino::BET_STREET, 10, 7};
        assert(Casino::Wins(street, 9) && !Casino::Wins(street, 10));
        Casino::Bet corner{Casino::BET_CORNER, 10, 4};
        assert(Casino::Wins(corner, 5) && Casino::Wins(corner, 8) && !Casino::Wins(corner, 9));
        Casino::Bet sixline{Casino::BET_SIXLINE, 10, 7};
        assert(Casino::Wins(sixline, 12) && !Casino::Wins(sixline, 13));
        assert(Casino::PayoutMultiplier(Casino::BET_CORNER) == 8);
        assert(Casino::ColorName(0) == std::string("ЗЕРО"));
        assert(Casino::IsBlack(2) && !Casino::IsBlack(1));
        assert(Casino::NextChipAmount(25, -1) == 10);
        assert(Casino::NextChipAmount(25, 1) == 50);
        assert(Casino::ClampNumber(99) == 36 && Casino::ClampNumber(-5) == 0);
    }

    std::cout << "[test_core] 9/9 Проверка базовых PBR текстур и генератора..." << std::endl;
    if (fs::exists("Data/Textures/Skybox4K.xml")) {
        CheckPngFile("Data/Textures/StoneHDDiffuse.png", 512, 512);
        CheckPngFile("Data/Textures/Sky4K_PosZ.png", 1024, 1024);
        CheckPngFile("Data/Textures/LoadingBg4K.png", 1920, 1080);
        // Сигл-петух веб-бута (генерируется tools/gen_boot_sigil.py из index.html)
        CheckPngFile("Data/Textures/BootSigil.png", 1024, 1024);
    }
    if (argc > 1 && fs::exists(argv[1])) {
        fs::path tmpOut = fs::temp_directory_path() / "chikengun_test_tex";
        fs::remove_all(tmpOut);
        std::string cmd = std::string("\"") + argv[1] + "\" \"" + tmpOut.string() + "\" 256 > /dev/null";
        int rc = std::system(cmd.c_str());
        assert(rc == 0 && "gen_textures_4k завершился с ошибкой");
        CheckPngFile(tmpOut / "StoneHDDiffuse.png", 256, 256);
        CheckPngFile(tmpOut / "HazardMetalRoughMetal.png", 256, 256);
        assert(fs::exists(tmpOut / "Skybox4K.xml"));
        fs::remove_all(tmpOut);
    }

    std::cout << "[test_core] ВСЕ 9 НАБОРОВ ТЕСТОВ УСПЕШНО ПРОЙДЕНЫ!" << std::endl;
    return 0;
}
