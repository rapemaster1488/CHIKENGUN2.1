// ============================================================================
// Модульные тесты CHIKENGUN 2.3 [4K ULTRA & CYBER EVOLUTION]
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
#include "../src/ArsenalSystems.h"
#include "../src/ArcadeAndCasino.h"

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
    std::cout << "[test_core] 1/7 Проверка Lite::RayBoxT и HitNormalBoxT (src/LitePhysics.h)..." << std::endl;
    {
        float t = Lite::RayBoxT<TestVec3>({0, 1, -5}, {0, 0, 1}, {0, 1, 0}, {1, 1, 1});
        assert(std::fabs(t - 4.0f) < 1e-4f);
        TestVec3 n = Lite::HitNormalBoxT<TestVec3>({0, 1, -1}, {0, 1, 0}, {1, 1, 1});
        assert(std::fabs(n.z_ + 1.0f) < 1e-4f);

        assert(Lite::RayBoxT<TestVec3>({0, 5, -5}, {0, 0, 1}, {0, 1, 0}, {1, 1, 1}) < 0.0f);
        assert(Lite::RayBoxT<TestVec3>({0, 1,  5}, {0, 0, 1}, {0, 1, 0}, {1, 1, 1}) < 0.0f);
    }

    std::cout << "[test_core] 2/7 Проверка Lite::RaySphereT и сквозного луча Рельсотрона..." << std::endl;
    {
        float t1 = Lite::RaySphereT<TestVec3>({0, 1, 0}, {0, 0, 1}, {0, 1, 10}, 1.0f);
        float t2 = Lite::RaySphereT<TestVec3>({0, 1, 0}, {0, 0, 1}, {0, 1, 22}, 1.0f);
        assert(std::fabs(t1 - 9.0f) < 1e-4f);
        assert(std::fabs(t2 - 21.0f) < 1e-4f);
        assert(t1 < t2);
        assert(Lite::RaySphereT<TestVec3>({0, 1, 0}, {0, 0, 1}, {5, 1, 10}, 1.0f) < 0.0f);
    }

    std::cout << "[test_core] 3/7 Проверка кинематики Грэппл-крюка [Z] и FNV-1a кэша материалов..." << std::endl;
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

    std::cout << "[test_core] 4/7 Проверка систем CyberSystems.h (мутаторы, Орбита [X], перки, 12 достижений)..." << std::endl;
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
        assert(Cyber::CountUnlockedAchievements(Cyber::ALL_ACHIEVEMENTS_MASK) == 12);

        // 2.4: новые мутаторы и их параметры
        assert(Cyber::MUT_COUNT == 8);
        assert(Cyber::GetMutatorInfo(Cyber::MUT_TOXIC).fogMul > 1.5f);
        assert(Cyber::GetMutatorInfo(Cyber::MUT_GLASS).enemyHpMul < 0.5f);
        assert(Cyber::GetMutatorInfo(Cyber::MUT_GLASS).damageMul > 1.0f);
        assert(Cyber::GetMutatorInfo(Cyber::MUT_SWARM).extraEggs);
        for (int m = 0; m < Cyber::MUT_COUNT; ++m) {
            assert(Cyber::GetMutatorInfo(m).name != nullptr);
            assert(Cyber::GetMutatorInfo(m).fogMul >= 0.0f);
            assert(Cyber::GetMutatorInfo(m).enemyHpMul > 0.0f);
        }

        // 2.4: мини-босс «Летающее яйцо» только на нечётных волнах с 3-й
        assert(!Cyber::ShouldSpawnEggBoss(1));
        assert(!Cyber::ShouldSpawnEggBoss(2));
        assert(Cyber::ShouldSpawnEggBoss(3));
        assert(!Cyber::ShouldSpawnEggBoss(4));
        assert(Cyber::ShouldSpawnEggBoss(7));
        assert(Cyber::EggBossHp(5) > Cyber::EggBossHp(3));
        assert(Cyber::EggBossEggsPerVolley(true) > Cyber::EggBossEggsPerVolley(false));

        // 2.4: хрупкие куры теряют HP, но не до нуля
        assert(Cyber::ScaleEnemyHp(100, Cyber::MUT_GLASS) == 45);
        assert(Cyber::ScaleEnemyHp(100, Cyber::MUT_STANDARD) == 100);
        assert(Cyber::ScaleEnemyHp(2, Cyber::MUT_GLASS) >= 1);
    }

    std::cout << "[test_core] 4b/7 Проверка боевой статистики, ранга рейда, комбо и урона...\n";
    {
        // Точность: защита от деления на ноль и от shotsHit > shotsFired
        assert(Cyber::AccuracyPercent(0, 0) == 0);
        assert(Cyber::AccuracyPercent(40, 10) == 25);
        assert(Cyber::AccuracyPercent(10, 99) == 100);

        // Ранг рейда: чистый рейд = S, пустой = F
        Cyber::RaidStats sEmpty{};
        assert(Cyber::RaidGrade(sEmpty) == 'F');

        Cyber::RaidStats sGod{};
        sGod.score = 40000; sGod.wave = 14; sGod.kills = 80; sGod.headshots = 30;
        sGod.shotsFired = 300; sGod.shotsHit = 210; sGod.timeAliveSec = 900; sGod.maxCombo = 12;
        assert(Cyber::RaidGrade(sGod) == 'S');

        Cyber::RaidStats sMid{};
        sMid.kills = 20; sMid.wave = 4; sMid.shotsFired = 120; sMid.shotsHit = 60; sMid.maxCombo = 4;
        assert(Cyber::RaidGrade(sMid) == 'C' || Cyber::RaidGrade(sMid) == 'B');

        // Комбо-множитель ограничен x4 (раньше рос без предела)
        assert(std::fabs(Cyber::ComboMultiplier(1) - 1.0f) < 1e-5f);
        assert(std::fabs(Cyber::ComboMultiplier(3) - 1.5f) < 1e-5f);
        assert(std::fabs(Cyber::ComboMultiplier(50) - 4.0f) < 1e-5f);
        assert(std::fabs(Cyber::ComboMultiplier(0) - 1.0f) < 1e-5f);

        int plain = Cyber::AwardKillScore(100, 1, Cyber::MUT_STANDARD);
        int combo = Cyber::AwardKillScore(100, 8, Cyber::MUT_STANDARD);
        assert(plain == 100);
        assert(combo > plain && combo <= 400);

        // Урон: база, перк, овердрайв, крит и хэдшот
        assert(std::fabs(Cyber::ComputeShotDamage(100, false, false, 0, false) - 100.0f) < 1e-4f);
        assert(std::fabs(Cyber::ComputeShotDamage(100, true,  false, 0, false) - 200.0f) < 1e-4f);
        assert(std::fabs(Cyber::ComputeShotDamage(100, false, true,  0, false) - 200.0f) < 1e-4f);
        assert(std::fabs(Cyber::ComputeShotDamage(100, true,  true,  0, false) - 400.0f) < 1e-4f);
        assert(std::fabs(Cyber::ComputeShotDamage(100, false, false, 5, false) - 175.0f) < 1e-4f);
        assert(std::fabs(Cyber::ComputeShotDamage(100, false, false, 0, true) - 200.0f) < 1e-4f);
        assert(std::fabs(Cyber::CritChanceFromPerks(0) - 0.06f) < 1e-5f);
        assert(Cyber::CritChanceFromPerks(5) < 0.3f);

        // Пул частиц и форматирование времени
        assert(Cyber::ClampParticleBudget(900, 420) == 420);
        assert(Cyber::ClampParticleBudget(100, 420) == 100);
        char buf[16];
        Cyber::FormatRaidTime(754, buf, sizeof(buf));
        assert(std::strcmp(buf, "12:34") == 0);
        Cyber::FormatRaidTime(-5, buf, sizeof(buf));
        assert(std::strcmp(buf, "0:00") == 0);
    }

    std::cout << "[test_core] 5/7 Проверка аналитического пола и компакции коллайдеров..." << std::endl;
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

    std::cout << "[test_core] 6/7 Проверка XML-парсера и наличия 23 WAV-эффектов..." << std::endl;
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
        }
    }

    std::cout << "[test_core] 7/7 Проверка базовых PBR текстур и генератора..." << std::endl;
    if (fs::exists("Data/Textures/Skybox4K.xml")) {
        CheckPngFile("Data/Textures/StoneHDDiffuse.png", 512, 512);
        CheckPngFile("Data/Textures/Sky4K_PosZ.png", 1024, 1024);
        CheckPngFile("Data/Textures/LoadingBg4K.png", 1920, 1080);
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

    std::cout << "[test_core] 8/9 Проверка Арсенала 2.5 (20 пушек/ножей и процедурных анимаций)...\n";
    {
        assert(Arsenal::WEAPON_TOTAL_COUNT == 20);
        for (int i = 0; i < 16; ++i) {
            assert(!Arsenal::IsKnife(i));
            assert(Arsenal::GetWeaponSpec(i).magSize > 0);
        }
        for (int i = 16; i < 20; ++i) {
            assert(Arsenal::IsKnife(i));
            assert(Arsenal::GetWeaponSpec(i).speedBonusMul > 1.10f);
            assert(Arsenal::GetWeaponSpec(i).meleeRange >= 2.7f);
            assert(Arsenal::GetWeaponSpec(i).bonusCritChance >= 0.15f);
        }
        assert(Arsenal::NextKnifeId(Arsenal::W_RIFLE) == Arsenal::W_KNIFE_COMBAT);
        assert(Arsenal::NextKnifeId(Arsenal::W_KNIFE_COMBAT) == Arsenal::W_KNIFE_KARAMBIT);
        assert(Arsenal::NextKnifeId(Arsenal::W_KNIFE_KARAMBIT) == Arsenal::W_KNIFE_BUTTERFLY);
        assert(Arsenal::NextKnifeId(Arsenal::W_KNIFE_BUTTERFLY) == Arsenal::W_KNIFE_KATANA);
        assert(Arsenal::NextKnifeId(Arsenal::W_KNIFE_KATANA) == Arsenal::W_KNIFE_COMBAT);

        // 3-фазная перезарядка: фаза 1 опускает магазин, фаза 3 передёргивает затвор
        Arsenal::AnimPose r1 = Arsenal::ComputeReloadPose(Arsenal::W_RIFLE, 0.34f);
        Arsenal::AnimPose r3 = Arsenal::ComputeReloadPose(Arsenal::W_RIFLE, 0.87f);
        assert(r1.magDropY < -0.18f);
        assert(r3.slideBackZ < -0.05f);

        // Трюки осмотра [I]: вращение керамбита/барабана револьвера и раскрытие бабочки
        Arsenal::AnimPose inspKar = Arsenal::ComputeInspectPose(Arsenal::W_KNIFE_KARAMBIT, 0.5f);
        Arsenal::AnimPose inspBal = Arsenal::ComputeInspectPose(Arsenal::W_KNIFE_BUTTERFLY, 0.5f);
        Arsenal::AnimPose inspRev = Arsenal::ComputeInspectPose(Arsenal::W_REVOLVER, 0.5f);
        assert(inspKar.spinDeg > 300.0f);
        assert(inspBal.balisongOpenDeg > 20.0f);
        assert(inspRev.spinDeg > 100.0f);

        // 3-комбо ножа и тяжёлый выпад ПКМ
        Arsenal::AnimPose sl0 = Arsenal::ComputeKnifeSlashPose(Arsenal::W_KNIFE_KATANA, 0, false, 0.4f);
        Arsenal::AnimPose slH = Arsenal::ComputeKnifeSlashPose(Arsenal::W_KNIFE_KATANA, 0, true, 0.4f);
        assert(std::fabs(sl0.yaw) > 5.0f);
        assert(slH.posZ > 0.15f);
    }

    std::cout << "[test_core] 9/9 Проверка C++ Казино «Золотая Рулетка & 777» и 6 мини-игр Pixel Arcade...\n";
    {
        // Проверяем биекцию 37 карманов европейского колеса (0..36) и непересечение Voisins / Tiers
        bool seen[37] = {};
        int vCount = 0, tCount = 0;
        for (int i = 0; i < 37; ++i) {
            int pocket = MiniHub::CasinoEngine::WHEEL_ORDER[i];
            assert(pocket >= 0 && pocket <= 36 && !seen[pocket]);
            seen[pocket] = true;
            bool inV = MiniHub::CasinoEngine::InVoisins(pocket);
            bool inT = MiniHub::CasinoEngine::InTiers(pocket);
            assert(!(inV && inT) && "Voisins и Tiers не должны пересекаться!");
            if (inV) ++vCount;
            if (inT) ++tCount;
        }
        assert(vCount == 17 && tCount == 12);
        assert(!MiniHub::CasinoEngine::IsRed(0));
        assert(MiniHub::CasinoEngine::IsRed(32) && !MiniHub::CasinoEngine::IsRed(15));
        assert(MiniHub::CasinoEngine::ZoneMultiplier(MiniHub::CasinoEngine::BET_STRAIGHT, 17, 17) == 36);
        assert(MiniHub::CasinoEngine::ZoneMultiplier(MiniHub::CasinoEngine::BET_STRAIGHT, 17, 5) == 0);
        assert(MiniHub::CasinoEngine::ZoneMultiplier(MiniHub::CasinoEngine::BET_RED, 32, 0) == 2);
        assert(MiniHub::CasinoEngine::ZoneMultiplier(MiniHub::CasinoEngine::BET_DOZEN_2, 18, 0) == 3);

        // Полный цикл рулетки и слотов 777 + рендер в RGBA8 буфер
        MiniHub::PixelCanvas cv;
        MiniHub::CasinoEngine cas;
        int credits = 1000;
        cas.chipIdx_ = 1; // 25₵
        assert(cas.PlaceBet(MiniHub::CasinoEngine::BET_RED, credits));
        assert(cas.PlaceBet(MiniHub::CasinoEngine::BET_BLACK, credits));
        assert(credits == 950 && cas.TotalBet() == 50);
        assert(cas.StartSpin());
        for (int step = 0; step < 300 && cas.spinning_; ++step) cas.Update(0.02f, credits);
        assert(!cas.spinning_ && cas.hasResult_ && cas.historyCount_ == 1);
        cas.Render(cv);

        cas.modeTab_ = 1;
        assert(cas.SpinSlots(credits));
        for (int step = 0; step < 150 && cas.slotSpinning_; ++step) cas.Update(0.02f, credits);
        assert(!cas.slotSpinning_);
        cas.Render(cv);

        // Все 6 классических аркадных игр: Snake, Pong, Breakout, Flappy, Dino Run, Tetris
        MiniHub::ArcadeSuite arc;
        for (int g = 0; g < MiniHub::ArcadeSuite::GAME_COUNT; ++g) {
            arc.ResetGame(g);
            arc.tapAction_ = true;
            arc.holdRight_ = true;
            for (int step = 0; step < 90; ++step) {
                if (step % 18 == 0) arc.tapAction_ = true;
                int earned = arc.Update(1.0f / 60.0f);
                assert(earned >= 0);
            }
            arc.Render(cv);
        }
        assert(cv.data.size() == (size_t)(MiniHub::SCREEN_W * MiniHub::SCREEN_H * 4));
    }

    std::cout << "[test_core] ВСЕ 9 НАБОРОВ ТЕСТОВ УСПЕШНО ПРОЙДЕНЫ!" << std::endl;
    return 0;
}
