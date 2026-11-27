// Тесты геометрии 3D-колеса рулетки.
//
// ЖАЛОБА: «колесо сделай вот такое» с картинкой настоящего колеса вместо
// цветных брусков. Здесь проверяется арифметика раскладки: 37 лунок помещаются
// по окружности, номера стоят на своих лунках, шарик может упасть с дорожки
// внутрь, и всё это совпадает с логикой из CasinoSim (иначе камера наводится
// на один сектор, а шарик падает в другой).
#include "../src/RouletteWheel3D.h"
#include "../src/CasinoSim.h"

#include <cstdio>
#include <cmath>

using namespace Wheel3D;

static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}
static bool near(float a, float b, float eps = 1e-4f) {
    const float d = a - b;
    return (d < 0 ? -d : d) <= eps;
}

int main() {
    // ── 1. Лунки помещаются по кругу ─────────────────────────────────────────
    std::puts("== 1. 37 лунок на окружности ==");
    {
        check(kPockets == 37, "европейское колесо: 37 лунок");
        check(PocketsFit(), "лунки не наезжают друг на друга");
        // Шаг 360/37 ≈ 9.73°, ширина лунки по хорде должна быть заметно меньше.
        const float step = 360.0f / 37.0f;
        check(PocketAngularWidthDeg() > 2.0f, "лунка не вырождена в точку");
        check(PocketAngularWidthDeg() < step * 0.9f, "между лунками есть перегородка");
        check(near(HalfStepDeg() * 2.0f, step, 1e-3f), "полшага x2 = полный шаг");
    }

    // ── 2. Углы совпадают с логикой CasinoSim ────────────────────────────────
    // Критично: камера кинематики наводится на Casino::WheelSectorAngleDeg,
    // а кольцо-маркер и лунки строятся по этой таблице. Разъедутся — игрок
    // увидит «число одно, а шарик в другом кармане».
    std::puts("== 2. Совпадение с CasinoSim ==");
    {
        bool allMatch = true;
        for (int i = 0; i < 37; ++i) {
            const float sim = Casino::WheelSectorAngleDeg(i);
            if (!near(SectorAngleDeg(i), sim, 1e-3f)) allMatch = false;
            if (!near(SectorAngleDeg(i), PocketCenterDeg(i))) allMatch = false;
        }
        check(allMatch, "угол лунки == Casino::WheelSectorAngleDeg для всех 37");
        check(near(SectorAngleDeg(0), 0.0f), "первая лунка на 0°");
        check(SectorAngleDeg(36) < 360.0f, "последняя лунка не замыкается на 360");
    }

    // ── 3. Полярные координаты ───────────────────────────────────────────────
    std::puts("== 3. Полярные координаты ==");
    {
        float x = 0.0f, z = 0.0f;
        PolarToXZ(1.0f, 0.0f, x, z);
        check(near(x, 0.0f) && near(z, 1.0f), "0° -> (0, 1): вперёд по Z");
        PolarToXZ(1.0f, 90.0f, x, z);
        check(near(x, 1.0f) && near(z, 0.0f), "90° -> (1, 0): вправо по X");
        PolarToXZ(2.0f, 180.0f, x, z);
        check(near(x, 0.0f) && near(z, -2.0f), "180° -> (0, -2)");
        // Радиус сохраняется на любом угле.
        bool radiusOk = true;
        for (int i = 0; i < 37; ++i) {
            PolarToXZ(kPocketR, SectorAngleDeg(i), x, z);
            if (std::fabs(std::sqrt(x * x + z * z) - kPocketR) > 1e-3f) radiusOk = false;
        }
        check(radiusOk, "радиус лунки одинаков на всех углах");
    }

    // ── 4. Номер внутри своей лунки ──────────────────────────────────────────
    std::puts("== 4. Номера стоят в карманах ==");
    {
        check(NumberInsidePocket(0), "номер не вылетает из лунки");
        check(kNumberR > kPocketR, "номер ближе к внешнему краю — как в настоящем колесе");
        // Номер обязан быть внутри кольца: иначе он окажется на перегородке
        // или на воронке.
        check(kNumberR + kPocketW * 0.5f <= kDrumR + 1e-3f, "номер не выходит за кольцо лунок");
        check(kNumberR - kPocketLen * 0.5f > kFunnelR, "номер не залезает на воронку");
    }

    // ── 5. Перегородка между лунками ────────────────────────────────────────
    std::puts("== 5. Перегородки ==");
    {
        bool between = true;
        for (int i = 0; i < 37; ++i) {
            const float fret = FretAngleDeg(i);
            const float a = SectorAngleDeg(i);
            const float b = SectorAngleDeg((i + 1) % 37);
            // Перегородка должна быть равноудалена от центров соседних лунок.
            float d1 = std::fabs(fret - a);
            float d2 = std::fabs(b - fret);
            if (i == 36) d2 = std::fabs((b + 360.0f) - fret);
            if (std::fabs(d1 - d2) > 1e-3f) between = false;
        }
        check(between, "перегородка ровно посередине между лунками");
        check(near(FretAngleDeg(0), HalfStepDeg()), "первая перегородка на полшага");
        check(kFretW < kPocketW * 0.25f, "перегородка тоньше лунки");
    }

    // ── 6. Шарик может упасть в лунку ───────────────────────────────────────
    std::puts("== 6. Физика шарика ==");
    {
        check(BallCanDrop(), "с дорожки шарик попадает в лунку");
        check(kBallTrackRadius > kBallPocketRadius, "дорожка дальше от центра, чем лунка");
        check(kBallTrackRadius + kBallR < kTrackR, "шарик не вылетает за борт");
        check(kBallTrackY > kPocketY, "дорожка выше дна лунки — есть куда падать");
        check(kBallPocketY < kPocketY + 1e-3f, "шарик лежит на дне кармана");
        // Диаметр шарика меньше ширины лунки: иначе он висит над карманом.
        check(kBallR * 2.0f < kPocketW, "шарик проваливается в карман, а не лежит сверху");
    }

    // ── 7. Радиусы идут по порядку, без «вывернутого» колеса ─────────────────
    std::puts("== 7. Порядок радиусов ==");
    {
        check(kLipR < kBodyR, "борт внутри корпуса чаши");
        check(kBodyR < kSkirtR, "корпус внутри юбки");
        check(kTrackR < kLipR, "дорожка шарика внутри борта");
        check(kTrackInnerR < kTrackR, "у дорожки есть внутренняя кромка");
        check(kDrumR < kTrackInnerR, "барабан с лунками не залезает на дорожку");
        check(kFunnelR < kPocketR - kPocketLen * 0.5f, "воронка не перекрывает лунки");
        check(kGoldRimR > kLipR - 0.01f && kGoldRimR < kSkirtR, "золотой обод по борту");
    }

    // ── 8. Карта номеров соответствует колесу ────────────────────────────────
    std::puts("== 8. Карта номеров ==");
    {
        static const int kRedCount = 18;
        static const int kBlackCount = 18;
        int reds = 0, blacks = 0, zeros = 0;
        for (int i = 0; i < 37; ++i) {
            const int num = Casino::kEuropeanWheelOrder[i];
            if (num == 0) ++zeros;
            else if (Casino::IsRed(num)) ++reds;
            else ++blacks;
            // Обратный поиск обязан находить ту же лунку.
            if (Casino::WheelIndexForNumber(num) != i) g_fail++;
        }
        ++g_pass;
        check(zeros == 1, "ровно одно зеро");
        check(reds == kRedCount, "18 красных");
        check(blacks == kBlackCount, "18 чёрных");
        check(reds + blacks + zeros == 37, "все лунки учтены");
        // Соседи зеро в европейском колесе — 26 и 32.
        check(Casino::kEuropeanWheelOrder[(0 + 1) % 37] == 32 ||
              Casino::kEuropeanWheelOrder[36] == 26,
              "зеро на своём месте в карте колеса");
    }

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
