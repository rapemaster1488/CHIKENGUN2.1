// Тесты фазовой модели анимаций казино (src/CasinoAnim2.h).
// Без Urho3D и OpenGL — только математика, поэтому можно гонять в CI.
#include "../src/CasinoAnim2.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace A = CasinoAnim2;

static int g_passed = 0;
static int g_failed = 0;

static void check(const char* name, bool cond, const char* detail = "") {
    if (cond) {
        ++g_passed;
        std::printf("  [OK] %s\n", name);
    } else {
        ++g_failed;
        std::printf("  [FAIL] %s %s\n", name, detail);
    }
}

static void checkNear(const char* name, float got, float want, float eps = 0.02f) {
    const bool ok = std::fabs(got - want) <= eps;
    if (!ok) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "(got %.3f, want %.3f)", got, want);
        check(name, false, buf);
    } else {
        check(name, true);
    }
}

int main() {
    std::printf("\n[1] Тайминги фаз\n");
    {
        // Обычный спин: барабаны должны вставать по одному, строго позже предыдущего.
        const float t0 = A::ReelStopTime(0), t1 = A::ReelStopTime(1), t2 = A::ReelStopTime(2);
        check("барабаны встают по порядку", t0 < t1 && t1 < t2);
        check("первый стоп после «вздоха» зала", t0 > A::Timing::kAnticipation);
        checkNear("шаг между стопами = kReelStagger", t1 - t0, A::Timing::kReelStagger, 0.001f);

        const float normal = A::SpinDuration(3, false, false);
        const float near = A::SpinDuration(3, true, false);
        const float jack = A::SpinDuration(3, false, true);
        check("спин не затянут (>1.5 c)", normal > 1.5f);
        check("спин не раздражает (<3.0 c)", normal < 3.0f);
        check("near-miss дольше обычного", near > normal);
        check("джекпот дольше обычного", jack > normal);
        check("последний барабан встаёт внутри спина", t2 < normal);
    }

    std::printf("\n[2] Кривые скорости\n");
    {
        // Разгон, крейсер, полная остановка к моменту stopAt.
        check("на старте барабан уже крутится", A::ReelSpeed(0.01f, 1.2f) > 5.0f);
        check("крейсер быстрый", A::ReelSpeed(0.6f, 1.2f) > 15.0f);
        check("к стопу скорость падает", A::ReelSpeed(1.1f, 1.2f) < A::ReelSpeed(0.6f, 1.2f));
        checkNear("ровно в момент стопа — ноль", A::ReelSpeed(1.2f, 1.2f), 0.0f, 0.001f);
        check("после стопа — ноль", A::ReelSpeed(1.4f, 1.2f) == 0.0f);
        check("до старта — ноль", A::ReelSpeed(-0.1f, 1.2f) == 0.0f);
    }

    std::printf("\n[3] Отскок после стопа\n");
    {
        checkNear("в момент стопа отскока нет", A::StopBounce(0.0f), 0.0f, 0.001f);
        check("сразу после стопа есть импульс", std::fabs(A::StopBounce(0.04f)) > 0.5f);
        check("импульс затухает", std::fabs(A::StopBounce(0.35f)) < std::fabs(A::StopBounce(0.04f)));
        checkNear("через секунду — покой", A::StopBounce(1.0f), 0.0f, 0.01f);
    }

    std::printf("\n[4] Near-miss\n");
    {
        check("7-7-3 это near-miss", A::IsNearMiss(5, 5, 2));
        check("3-7-7 это near-miss", A::IsNearMiss(2, 5, 5));
        check("7-3-7 это near-miss", A::IsNearMiss(5, 2, 5));
        check("7-7-7 это НЕ near-miss", !A::IsNearMiss(5, 5, 5));
        check("1-2-3 это НЕ near-miss", !A::IsNearMiss(0, 1, 2));
        const unsigned m = A::NearMissReels(5, 5, 2);
        check("подсвечиваются ровно 2 барабана",
              ((m & 1u) != 0) + ((m & 2u) != 0) + ((m & 4u) != 0) == 2);
        check("тройка распознаётся", A::IsTriple(5, 5, 5));
        check("не тройка", !A::IsTriple(5, 5, 2));
    }

    std::printf("\n[5] Счётчик выигрыша\n");
    {
        check("в начале — ноль", A::CountUpValue(1000, 0.0f) == 0);
        check("в конце — вся сумма", A::CountUpValue(1000, 1.0f) == 1000);
        check("на середине — уже больше половины (ease-out)",
              A::CountUpValue(1000, 0.5f) > 500);
        check("отрицательный проигрыш тикает вниз", A::CountUpValue(-500, 1.0f) == -500);
        check("монотонно растёт", A::CountUpValue(1000, 0.3f) <= A::CountUpValue(1000, 0.6f));
        check("не перелетает сумму", A::CountUpValue(1000, 1.2f) == 1000);
    }

    std::printf("\n[6] Бегущий огонёк рамки\n");
    {
        // В любой момент времени ровно одна лампа светит ярче всех.
        float best = -1.0f;
        int bright = 0;
        for (int i = 0; i < 24; ++i) {
            const float v = A::MarqueePulse(i, 24, 1.234f);
            if (v > best) { best = v; bright = i; }
            if (!(v >= 0.0f && v <= 1.0f)) {
                check("яркость лампы в диапазоне 0..1", false);
            }
        }
        (void)bright;
        // Проверка бегущего огня: яркая лампа смещается со временем.
        int a = 0, b = 0;
        float ba = -1.0f, bb = -1.0f;
        for (int i = 0; i < 24; ++i) {
            // Шаг 0.12 с при 16 ламп/с — это почти две лампы: огонь обязан сместиться.
            const float va = A::MarqueePulse(i, 24, 0.00f);
            const float vb = A::MarqueePulse(i, 24, 0.12f);
            if (va > ba) { ba = va; a = i; }
            if (vb > bb) { bb = vb; b = i; }
        }
        check("огонёк бежит по рамке", a != b);
        check("нулевое число ламп безопасно", A::MarqueePulse(0, 0, 1.0f) == 0.0f);
    }

    std::printf("\n[7] Палитра фаз\n");
    {
        const auto antic = A::PaletteForPhase(A::PH_ANTICIPATION, false);
        const auto near = A::PaletteForPhase(A::PH_NEAR_MISS, false);
        const auto win = A::PaletteForPhase(A::PH_REVEAL, false);
        const auto jack = A::PaletteForPhase(A::PH_REVEAL, true);
        check("фазы различаются по цвету",
              !(std::fabs(near.accentR - win.accentR) < 0.01f &&
                std::fabs(near.accentG - win.accentG) < 0.01f));
        check("near-miss пульсирует сильнее спина", near.pulse > antic.pulse);
        check("джекпот отличается от обычного выигрыша",
              std::fabs(jack.accentB - win.accentB) > 0.05f);
        check("idle почти не пульсирует", A::PaletteForPhase(A::PH_IDLE, false).pulse < 0.2f);
        for (int p = 0; p < A::PH_COUNT; ++p) {
            const auto pal = A::PaletteForPhase(p, false);
            const bool sane = pal.accentR >= 0.0f && pal.accentR <= 1.0f &&
                              pal.accentG >= 0.0f && pal.accentG <= 1.0f &&
                              pal.accentB >= 0.0f && pal.accentB <= 1.0f &&
                              pal.pulse >= 0.0f && pal.pulse <= 1.0f;
            check(A::SlotPhaseName(p), sane);
        }
    }

    std::printf("\n────────────────────────────────────────────\n");
    std::printf("Пройдено: %d, провалено: %d\n", g_passed, g_failed);
    if (g_failed) return 1;
    std::printf("ВСЕ ТЕСТЫ АНИМАЦИЙ КАЗИНО ПРОЙДЕНЫ\n\n");
    return 0;
}
