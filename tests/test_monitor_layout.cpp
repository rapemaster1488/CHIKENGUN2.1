// Тесты многомониторной раскладки.
// Жалоба: «оно на два моника растянулось». Проверяем именно то, что
// приводит к такому окну: разрешение из таблицы больше монитора, центрирование
// по объединённому bbox и потерянное окно после отключения второго экрана.
#include "../src/MonitorLayout.h"
#include <cstdio>

using namespace MonitorLayout;

static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

int main() {
    // Типичный проблемный сетап: ноутбук 1920x1080 слева, внешний 2560x1440 справа.
    const Rect laptop{0, 0, 1920, 1080};
    const Rect external{1920, 0, 2560, 1440};
    const Rect monitors[2] = {laptop, external};

    std::puts("== 1. Один монитор — поведение не меняется ==");
    check(!IsMultiMonitor(1), "1 монитор — не multimonitor");
    check(IsMultiMonitor(2), "2 монитора — multimonitor");
    {
        int w = 1920, h = 1080;
        check(!ClampResolutionToMonitor(w, h, laptop), "1920x1080 не режется на 1920x1080");
        check(w == 1920 && h == 1080, "размер сохранён");
    }

    std::puts("== 2. Разрешение не вылезает за выбранный монитор ==");
    {
        // Пресет 4K на ноутбучном экране 1920x1080: было бы окно 3840x2160,
        // которое накрывает оба дисплея. Должно урезаться до 1920x1080.
        int w = 3840, h = 2160;
        check(ClampResolutionToMonitor(w, h, laptop), "4K на 1080p урезается");
        check(w == 1920 && h == 1080, "4K -> 1920x1080 (ровно размер монитора)");
        check(w <= laptop.w, "ширина не больше монитора");
    }
    {
        // Внешний 2560x1440 при запросе 3840x2160.
        int w = 3840, h = 2160;
        ClampResolutionToMonitor(w, h, external);
        check(w == 2560 && h == 1440, "4K на 2560x1440 -> 2560x1440");
    }
    {
        // Запрос меньше монитора не трогаем.
        int w = 1600, h = 900;
        check(!ClampResolutionToMonitor(w, h, external), "1600x900 не режется");
        check(w == 1600 && h == 900, "1600x900 сохранён");
    }

    std::puts("== 3. Размер окна по размеру конкретного монитора ==");
    {
        int w = 0, h = 0;
        ResolveWindowSize(external, 0, 0, w, h);
        check(w == 2560 && h == 1440, "request 0 -> полный размер внешнего");
        ResolveWindowSize(laptop, 0, 0, w, h);
        check(w == 1920 && h == 1080, "request 0 -> полный размер ноутбука");
        ResolveWindowSize(laptop, 3840, 2160, w, h);
        check(w == 1920, "запрос больше монитора -> ширина монитора");
        ResolveWindowSize(external, 1600, 900, w, h);
        check(w == 1600 && h == 900, "запрос меньше монитора -> как просили");
    }

    std::puts("== 4. Центрирование на выбранном мониторе ==");
    {
        const Rect r = CenterOnMonitor(external, 1600, 900);
        check(r.w == 1600 && r.h == 900, "размер сохранён");
        check(r.x == external.x + (2560 - 1600) / 2, "центр по X внутри внешнего");
        check(r.x >= external.x, "окно не уезжает левее своего монитора");
        check(r.x + r.w <= external.x + external.w, "правая кромка внутри монитора");
    }
    {
        // Окно шире монитора — прижимаем, иначе оно снова на два экрана.
        const Rect r = CenterOnMonitor(laptop, 3840, 2160);
        check(r.x == laptop.x, "слишком широкое окно прижато к левому краю");
        check(r.w == laptop.w, "ширина урезана до монитора");
        check(r.x + r.w <= laptop.x + laptop.w, "не выходит за монитор");
    }

    std::puts("== 5. Окно определено на своём мониторе ==");
    {
        const Rect win = CenterOnMonitor(external, 1600, 900);
        check(MonitorForRect(monitors, 2, win) == 1, "окно на внешнем = монитор 1");
        const Rect win2 = CenterOnMonitor(laptop, 1600, 900);
        check(MonitorForRect(monitors, 2, win2) == 0, "окно на ноутбуке = монитор 0");
    }
    {
        // Окно, которое накрывает стык: считаем «своим» тот, где больше площади.
        const Rect straddle{1500, 100, 1200, 800};
        check(MonitorForRect(monitors, 2, straddle) == 1, "на стыке побеждает монитор с большей площадью");
    }
    {
        const Rect nowhere{-5000, -5000, 100, 100};
        check(MonitorForRect(monitors, 2, nowhere) == -1, "окно вне мониторов -> -1");
        check(IsOffScreen(monitors, 2, nowhere), "такое окно считается потерянным");
    }

    std::puts("== 6. Потерянное окно (отключили второй экран) ==");
    {
        // Окно осталось в координатах второго монитора, но он отключён.
        const Rect onlyLaptop[1] = {laptop};
        const Rect stale{3000, 200, 1600, 900};
        check(IsOffScreen(onlyLaptop, 1, stale), "окно за пределами единственного монитора");
        const Rect fixed = CenterOnMonitor(laptop, 1600, 900);
        check(!IsOffScreen(onlyLaptop, 1, fixed), "после переноса окно снова видно");
    }

    std::puts("== 7. Переключение монитора ==");
    check(CycleMonitor(0, 2, 1) == 1, "0 -> 1 вперёд");
    check(CycleMonitor(1, 2, 1) == 0, "1 -> 0 по кругу");
    check(CycleMonitor(0, 2, -1) == 1, "0 -> 1 назад по кругу");
    check(CycleMonitor(0, 1, 1) == 0, "один монитор — некуда переключать");
    check(CycleMonitor(5, 2, 1) == 0, "индекс вне диапазона нормализуется");
    check(ClampMonitor(7, 2) == 1, "ClampMonitor режет по числу мониторов");
    check(ClampMonitor(-3, 2) == 0, "отрицательный индекс -> 0");

    std::puts("== 8. Основной монитор ==");
    {
        const Monitor ms[2] = {{laptop, false}, {external, true}};
        check(PrimaryIndex(ms, 2) == 1, "primary помеченный — второй");
        const Monitor ms2[2] = {{laptop, false}, {external, false}};
        check(PrimaryIndex(ms2, 2) == 0, "без пометки — нулевой");
    }

    std::puts("== 9. Вырожденные входы ==");
    check(!IsValidSize(0, 900), "нулевая ширина невалидна");
    check(!IsValidSize(1600, 0), "нулевая высота невалидна");
    check(IsValidSize(1600, 900), "нормальный размер валиден");
    {
        const Rect a{0, 0, 100, 100}, b{200, 200, 100, 100};
        Rect r;
        check(!Intersect(a, b, r), "непересекающиеся -> false");
        check(OverlapArea(a, b) == 0, "площадь пересечения 0");
        check(OverlapArea(a, a) == 10000, "площадь с самим собой = площадь");
    }
    {
        const Rect m{0, 0, 0, 0};
        int w = 0, h = 0;
        ResolveWindowSize(m, 0, 0, w, h);
        check(w == 0 && h == 0, "вырожденный монитор даёт вырожденный размер");
        check(!IsValidSize(w, h), "и он невалиден — окно так создавать нельзя");
    }

    // ── «Склеенный» рабочий стол ─────────────────────────────────────────
    // Жалоба: «видит два моих моника как 1 3480 пикселей». 3480 — это ширина
    // объединённого пространства. Игра не может разлепить его за игрока, но
    // обязана это распознать и сказать, где искать настройку.
    std::puts("== Склеенный рабочий стол ==");
    {
        check(LooksLikeStitchedDesktop(3480, 1080), "3480x1080 (1920+1560) — склейка");
        check(LooksLikeStitchedDesktop(3840, 1080), "3840x1080 (два FullHD) — склейка");
        check(LooksLikeStitchedDesktop(5120, 1440), "5120x1440 (два QHD) — склейка");
        check(LooksLikeStitchedDesktop(5760, 1080), "5760x1080 (три FullHD) — склейка");

        // Одиночные широкие экраны — это НЕ склейка, окно не надо трогать.
        check(!LooksLikeStitchedDesktop(1920, 1080), "1920x1080 — один монитор");
        check(!LooksLikeStitchedDesktop(2560, 1440), "2560x1440 — один монитор");
        check(!LooksLikeStitchedDesktop(3440, 1440), "3440x1440 (21:9) — один монитор");
        check(!LooksLikeStitchedDesktop(5120, 2160), "5120x2160 (21:9 4K) — один монитор");
        check(!LooksLikeStitchedDesktop(0, 0), "нули не считаются склейкой");
        check(!LooksLikeStitchedDesktop(3480, 0), "без высоты не гадаем");

        const DesktopDiag one = Diagnose(1, 3480, 1080);
        check(one.stitched && one.monitors == 1, "один дисплей + 3480px -> склейка");
        const DesktopDiag two = Diagnose(2, 4480, 1440);
        check(!two.stitched && two.monitors == 2, "два нормальных дисплея -> не склейка");
        const DesktopDiag bad = Diagnose(0, 0, 0);
        check(bad.monitors == 1 && !bad.stitched, "нулевой count -> один монитор");
        check(StitchedHint()[0] != '\0', "подсказка игроку не пустая");
    }

    // ── Половины склеенного экрана ────────────────────────────────────────
    // «Видит два моника как 1, 3480 пикселей» — значит окно можно поставить
    // на левую или правую половину этого одного «дисплея». Проверяем, что
    // половины не перекрываются, вместе дают целое и окно в них помещается.
    std::puts("== Половины склеенного экрана ==");
    {
        const Rect stitched{0, 0, 3480, 1080};
        const Rect left  = StripeOf(stitched, 0, 2);
        const Rect right = StripeOf(stitched, 1, 2);
        check(left.x == 0 && left.w == 1740, "левая половина 0..1740");
        check(right.x == 1740 && right.w == 1740, "правая половина 1740..3480");
        check(left.w + right.w == stitched.w, "половины вместе дают всю ширину");
        check(left.x + left.w == right.x, "половины стыкуются без разрыва");
        check(left.h == stitched.h && right.h == stitched.h, "высота сохраняется");

        // Нечётная ширина: остаток уходит правой, ничего не теряется.
        const Rect odd{0, 0, 3839, 1080};
        const Rect ol = StripeOf(odd, 0, 2), orr = StripeOf(odd, 1, 2);
        check(ol.w + orr.w == 3839, "нечётная ширина делится без потерь");
        check(ol.w == orr.w || orr.w == ol.w + 1, "разница не больше пикселя");

        // Смещённый дисплей (второй экран справа от первого).
        const Rect offset{1920, 0, 3480, 1080};
        const Rect offL = StripeOf(offset, 0, 2);
        check(offL.x == 1920 && offL.w == 1740, "смещение дисплея учитывается");

        // Обычный монитор: половин нет, StripeOf возвращает его же.
        check(HStripeCount(1920, 1080) == 1, "1920x1080 — без половин");
        check(HStripeCount(3480, 1080) == 2, "3480x1080 — две половины");
        check(HStripeCount(3840, 1080) == 2, "3840x1080 — две половины");
        const Rect normal = StripeOf(Rect{0, 0, 1920, 1080}, 0, 1);
        check(normal.w == 1920, "для одного монитора половин нет");

        // Окно в половине: не шире половины и внутри неё.
        const Rect win = FitWindowToStripe(stitched, 0, 2, 1920, 1080);
        check(win.w <= 1740, "окно не шире половины");
        check(win.x >= 0 && win.x + win.w <= 1740, "окно целиком в левой половине");
        check(win.h <= 1080, "окно не выше экрана");
        const Rect winR = FitWindowToStripe(stitched, 1, 2, 1920, 1080);
        check(winR.x >= 1740 && winR.x + winR.w <= 3480, "правая половина: окно внутри");
        // Запрос 0x0 — «во всю половину».
        const Rect full = FitWindowToStripe(stitched, 1, 2, 0, 0);
        check(full.w == 1740 && full.x == 1740, "0x0 -> вся правая половина");
        // Номер половины за пределами — прижимается, а не вылетает.
        const Rect clampHi = StripeOf(stitched, 9, 2);
        const Rect clampLo = StripeOf(stitched, -3, 2);
        check(clampHi.x == 1740, "номер половины сверху прижат");
        check(clampLo.x == 0, "номер половины снизу прижат");
    }

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
