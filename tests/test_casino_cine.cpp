// Тесты кинематики крупного плана рулетки.
// Жалоба/задача: «чтоб приближалось колесо и показывало, как шарик крутит и
// что выпадает, а потом полно». Проверяем именно это: камера реально
// подлетает, шарик в лунке только после падения, а к концу камера обязана
// вернуться в полный вид зала — иначе игрок навсегда останется в крупном плане.
#include "../src/CasinoCinematic.h"
#include <cstdio>

using namespace CasinoCine;

static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}
static bool near(float a, float b, float eps = 1e-3f) {
    const float d = a - b;
    return (d < 0 ? -d : d) <= eps;
}

int main() {
    const float spin = 2.85f;

    std::puts("== 1. Фазы идут по порядку ==");
    {
        check(Eval(-1.0f, spin).phase == PH_FLY_IN, "отрицательное время -> подлёт");
        check(Eval(0.1f, spin).phase == PH_FLY_IN, "начало -> FLY_IN");
        check(Eval(1.0f, spin).phase == PH_TRACK, "середина спина -> TRACK");
        check(Eval(spin + 0.2f, spin).phase == PH_REVEAL, "после спина -> REVEAL");
        const float back = FlyBackStart(spin) + 0.1f;
        check(Eval(back, spin).phase == PH_FLY_BACK, "после выдержки -> FLY_BACK");
        check(Eval(TotalDuration(spin) + 1.0f, spin).phase == PH_DONE, "в конце -> DONE");
    }

    std::puts("== 2. Камера реально приближается ==");
    {
        check(near(Eval(0.0f, spin).closeUp, 0.0f, 1e-3f), "в момент 0 камера ещё в зале");
        check(near(Eval(0.3f, spin).closeUp, 1.0f, 0.5f), "к 0.3 с уже близко");
        check(near(Eval(1.0f, spin).closeUp, 1.0f), "во время спина — полный крупный план");
        check(Eval(1.0f, spin).closeUp > Eval(0.05f, spin).closeUp, "со временем ближе, а не дальше");
    }

    std::puts("== 3. Шарик крутится, потом падает ==");
    {
        check(!Eval(spin * 0.2f, spin).ballInPocket, "в начале спина шарик ещё на треке");
        check(!Eval(spin * 0.5f, spin).ballInPocket, "на середине спина ещё крутится");
        check(!Eval(spin * 0.70f, spin).ballInPocket, "перед падением ещё не в лунке");
        check(Eval(spin * 0.85f, spin).ballInPocket, "после падения — в лунке");
        check(Eval(spin + 0.1f, spin).ballInPocket, "на выдержке шарик лежит в лунке");
    }

    std::puts("== 4. Что выпало — показывается ==");
    {
        check(Eval(spin + 0.05f, spin).reveal > 0.0f, "число появляется сразу после остановки");
        check(near(Eval(spin + 1.0f, spin).reveal, 1.0f), "к середине выдержки плашка полностью видна");
        check(Eval(spin + 0.6f, spin).reveal > 0.9f, "плашка появляется быстро, а не в конце");
    }

    std::puts("== 5. «А потом полно» — камера возвращается ==");
    {
        const Frame end = Eval(TotalDuration(spin) + 0.5f, spin);
        check(end.finished, "кинематика завершается");
        check(near(end.closeUp, 0.0f, 1e-3f), "в конце камера снова в полном виде зала");
        check(near(end.reveal, 0.0f, 1e-3f), "плашка убрана");
        check(Eval(TotalDuration(spin) - 0.01f, spin).phase == PH_FLY_BACK, "последняя фаза — именно возврат");
    }
    {
        const float half = FlyBackStart(spin) + Timing::kFlyBack * 0.5f;
        const Frame mid = Eval(half, spin);
        check(mid.closeUp > 0.0f && mid.closeUp < 1.0f, "на возврате камера плавно в середине пути");
    }

    std::puts("== 6. Тайминги ==");
    {
        check(TotalDuration(spin) > spin, "полная длительность больше самого спина");
        check(near(TotalDuration(spin), spin + Timing::kRevealHold + Timing::kFlyBack, 1e-3f),
              "длительность = спин + выдержка + возврат");
        check(FlyBackStart(spin) > spin, "возврат начинается после остановки колеса");
        check(Timing::kFlyIn < 1.0f, "подлёт короче секунды — нет провала ожидания");
        check(Timing::kRevealHold >= 1.5f, "выдержка даёт прочитать число");
    }

    std::puts("== 7. Камера: геометрия ==");
    {
        const V3 wheel = MakeV3(-0.62f, 0.99f, -0.02f);
        const Camera close = CloseUp(wheel, 1.2f, 0.0f);
        check(close.pos.y > wheel.y, "крупный план смотрит сверху");
        check(Length(Sub(close.pos, wheel)) < 1.0f, "камера действительно рядом с колесом");
        check(Length(Sub(close.look, wheel)) < 0.6f, "взгляд направлен в колесо");
        // Камера повёрнута к выпавшему сектору: расстояние до него меньше, чем до центра.
        const V3 sector = SectorPoint(wheel, 1.2f, 0.30f);
        const V3 opposite = SectorPoint(wheel, 1.2f + 3.14159f, 0.30f);
        check(Length(Sub(close.pos, sector)) < Length(Sub(close.pos, opposite)),
              "камера стоит над выпавшим сектором, а не с другой стороны");
    }
    {
        const Camera base = {MakeV3(0.0f, 2.34f, -2.85f), MakeV3(0.0f, 1.04f, 0.0f)};
        const V3 wheel = MakeV3(-0.62f, 0.99f, -0.02f);
        Frame f = Eval(1.0f, spin);
        const Camera full = CameraFor(base, wheel, 0.0f, f);
        // Камера зала стоит в трёх метрах, крупный план — над колесом:
        // смещение обязано быть заметным, иначе игрок ничего не разглядит.
        check(Length(Sub(full.pos, base.pos)) > 1.0f, "в крупном плане камера реально ушла с камеры зала");
        check(Length(Sub(full.pos, wheel)) < 1.0f, "и при этом стоит у самого колеса");
        Frame doneFrame = Eval(TotalDuration(spin) + 0.1f, spin);
        const Camera back = CameraFor(base, wheel, 0.0f, doneFrame);
        check(near(back.pos.x, base.pos.x) && near(back.pos.z, base.pos.z), "после кинематики камера вернулась");
    }
    {
        // Смешивание не проваливает камеру ниже стола.
        const Camera base = {MakeV3(0.0f, 2.34f, -2.85f), MakeV3(0.0f, 1.04f, 0.0f)};
        const Camera close = {MakeV3(0.0f, 1.6f, 0.0f), MakeV3(0.0f, 1.0f, 0.0f)};
        for (int i = 0; i <= 10; ++i) {
            const Camera c = Blend(base, close, (float)i / 10.0f);
            check(c.pos.y > 1.0f, "камера не уходит под стол");
        }
    }

    std::puts("== 8. Плашка с числом ==");
    {
        const BadgeColor zero = ColorForNumber(0, false);
        check(zero.g > zero.r, "зеро — зелёная плашка");
        const BadgeColor red = ColorForNumber(7, true);
        check(red.r > red.g, "красное число — красная плашка");
        const BadgeColor black = ColorForNumber(8, false);
        check(black.r < 0.3f && black.g < 0.3f, "чёрное число — тёмная плашка");
        check(BadgePulse(0.0f) > 0.9f && BadgePulse(0.0f) < 1.2f, "пульс плашки в разумных пределах");
        check(BadgePulse(0.15f) > 1.0f || BadgePulse(0.4f) > 0.9f, "пульс живой, но не взрывается");
    }

    std::puts("== 9. Вырожденные входы ==");
    {
        const Frame f0 = Eval(0.0f, 0.0f);
        check(f0.phase == PH_FLY_IN, "нулевой спин не ломает первую фазу");
        const Frame fBig = Eval(1e9f, spin);
        check(fBig.finished && near(fBig.closeUp, 0.0f, 1e-3f), "огромное время -> камера в зале");
        const Frame fNeg = Eval(-999.0f, spin);
        check(fNeg.closeUp >= 0.0f && fNeg.closeUp <= 1.0f, "отрицательное время не ломает closeUp");
        check(TotalDuration(0.0f) > 0.0f, "нулевой спин всё равно даёт положительную длительность");
    }

    // ── 10. Сценарий интеграции: спин 2.85 с, как в StartCasinoSpin ────────
    //   Кадр за кадром прогоняем ровно то, что делает UpdateCasino с шагом
    //   кадра, и проверяем общий хронометраж и «залипания».
    std::puts("== 10. Прогон реального спина по кадрам ==");
    {
        const float spinDur = 2.85f;
        const float total = TotalDuration(spinDur);
        check(near(total, 5.75f, 0.01f), "полный хронометраж 5.75 с");

        float t = 0.0f;
        const float dt = 1.0f / 60.0f;
        int frames = 0;
        float maxJump = 0.0f;          // скачок closeUp за кадр
        float prevCloseUp = 0.0f;
        float prevReveal = 0.0f;
        bool sawReveal = false, sawPocket = false;
        int phaseChanges = 0;
        int lastPhase = Eval(0.0f, spinDur).phase;
        float firstCloseUpAt = -1.0f;
        float pocketShownAt = -1.0f;
        float badgeFullAt = -1.0f;

        while (t <= total + 0.5f) {
            const Frame f = Eval(t, spinDur);
            if (f.phase != lastPhase) { ++phaseChanges; lastPhase = f.phase; }
            maxJump = std::max(maxJump, std::fabs(f.closeUp - prevCloseUp));
            if (firstCloseUpAt < 0.0f && f.closeUp > 0.85f) firstCloseUpAt = t;
            if (f.reveal > 0.02f) sawReveal = true;
            if (f.reveal > 0.99f && badgeFullAt < 0.0f) badgeFullAt = t;
            if (f.ballInPocket && !sawPocket) { sawPocket = true; pocketShownAt = t; }
            // плашка не должна «дёргаться»: после полной видимости только вниз
            if (prevReveal > 0.99f) check(f.reveal <= 1.0f + 1e-4f, "reveal не превышает 1");
            prevCloseUp = f.closeUp;
            prevReveal = f.reveal;
            t += dt;
            ++frames;
        }

        check(frames > 300, "кадров достаточно для плавной анимации");
        check(maxJump < 0.16f, "камера не телепортируется между кадрами");
        check(firstCloseUpAt > 0.0f && firstCloseUpAt <= 0.62f,
              "крупный план наступает быстрее 0.62 с от нажатия");
        check(sawReveal, "плашка с числом появилась");
        check(badgeFullAt > 0.0f && badgeFullAt < spinDur + 0.7f,
              "номер читается в первую треть выдержки");
        check(sawPocket && pocketShownAt > spinDur * 0.7f,
              "шарик в лунке — к концу вращения, а не раньше");
        check(phaseChanges == 4, "ровно четыре перехода фаз: подлёт, трек, выдержка, возврат");
        check(Eval(total + 0.6f, spinDur).finished, "после конца кинематики камера в зале");

        // Пропуск выдержки: прыжок к возврату обязан оставить closeUp=1,
        // иначе камера «моргнёт» в зал и обратно.
        const float jumpT = FlyBackStart(spinDur) + 0.001f;
        const Frame jf = Eval(jumpT, spinDur);
        check(jf.phase == PH_FLY_BACK && jf.closeUp > 0.99f,
              "пропуск выдержки не моргает камерой");
        check(near(jf.reveal, 1.0f, 0.02f), "пропуск выдержки не теряет номер сразу");
    }

    // ── 11. Наводка на сектор: без разворота через 360 ─────────────────────
    std::puts("== 11. Наводка камеры по кратчайшей дуге ==");
    {
        check(near(WrapDeg360(370.0f), 10.0f), "370 -> 10");
        check(near(WrapDeg360(-10.0f), 350.0f), "-10 -> 350");
        check(near(ShortestDeltaDeg(350.0f, 10.0f), 20.0f), "с 350 на 10 — вперёд на 20");
        check(near(ShortestDeltaDeg(10.0f, 350.0f), -20.0f), "с 10 на 350 — назад на 20");
        check(std::fabs(ShortestDeltaDeg(0.0f, 180.0f)) <= 180.0f, "полуоборот не переворачивается");

        // Сглаживание: один шаг не перескакивает цель и не даёт разворота.
        float a = 350.0f;
        float maxStep = 0.0f;
        for (int i = 0; i < 240; ++i) {
            const float prev = a;
            a = ApproachAngleDeg(a, 10.0f, 0.06f);
            maxStep = std::max(maxStep, std::fabs(ShortestDeltaDeg(prev, a)));
        }
        check(maxStep < 4.0f, "наводка идёт без рывков");
        check(ShortestDeltaDeg(a, 10.0f) > -1.0f && ShortestDeltaDeg(a, 10.0f) < 1.0f,
              "наводка доезжает до цели через 360");
        check(a >= 0.0f && a < 360.0f, "угол наводки остаётся в 0..360");
    }

    // ── 12. Замедление у лунки: визуальное время отстаёт, но не ломается ────
    std::puts("== 12. Замедление колеса у лунки ==");
    {
        const float spinDur = 2.85f;
        const float dropAt = spinDur * Timing::kBallDropAt;
        // Вне окна падения визуальный прогресс равен реальному.
        check(near(TrackProgress(0.10f, 0.2f, spinDur), 0.10f), "до падения прогресс не тормозится");
        check(near(TrackProgress(0.95f, spinDur + 1.0f, spinDur), 0.95f), "после падения прогресс догоняет");
        // В окне падения — отстаёт, и отстаёт немного.
        const float inWindow = TrackProgress(0.72f, dropAt, spinDur);
        check(inWindow < 0.72f, "у лунки колесо притормаживает");
        check(inWindow > 0.72f - 0.08f, "но не останавливается");
        // Монотонность: отставание не даёт «прыжков назад» по времени.
        float prev = -1.0f;
        bool monotonic = true;
        for (int i = 0; i < 400; ++i) {
            const float t = (float)i * (spinDur + 0.5f) / 399.0f;
            const float prog = Clamp01(t / spinDur);
            const float v = TrackProgress(prog, t, spinDur);
            if (v < prev - 1e-4f) monotonic = false;
            prev = v;
        }
        check(monotonic, "визуальный прогресс не идёт назад");
        check(TrackProgress(0.5f, 0.1f, 0.0f) >= 0.0f && TrackProgress(0.5f, 0.1f, 0.0f) <= 1.0f,
              "вырожденная длительность спина не ломает прогресс");
    }

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
