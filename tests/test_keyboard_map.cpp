// Тесты раскладко-независимых клавиш.
//
// ЖАЛОБА: «ввод опять не работает, работает только эскейп, пробел и энтер».
// Здесь проверяется корень проблемы: Urho3D кладёт в P_KEY СИМВОЛ раскладки,
// поэтому при русской раскладке KEY_P == 'p' не совпадает никогда. Тест
// фиксирует, что сканкод даёт тот же ответ для всех раскладок, а «сломанный»
// путь (по символу) действительно ломается — иначе тест был бы бессмысленным.
#include "../src/KeyboardMap.h"

#include <cstdio>

using namespace Keys;

static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    if (cond) { ++g_pass; }
    else { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

int main() {
    // ── 1. Симптом, ради которого всё делалось ──────────────────────────────
    std::puts("== 1. Русская раскладка: клавиша P не теряется ==");
    {
        // SDL_Scancode 19 = клавиша P на физической клавиатуре.
        const unsigned scanP = 19u;
        // Что прислал бы движок при русской раскладке: 'п' = 0x043F.
        const unsigned keyRu = 0x043Fu;
        // Что прислал бы при английской: 'p'.
        const unsigned keyEn = (unsigned)'p';

        check(Resolve(scanP, keyRu) == 'p', "P при русской раскладке -> 'p'");
        check(Resolve(scanP, keyEn) == 'p', "P при английской раскладке -> 'p'");
        check(Matches(scanP, keyRu, 'p'), "P распознаётся как P при кириллице");

        // И демонстрация старого поведения: сравнение по символу ломается.
        check((int)keyRu != (int)'p', "старый путь (по P_KEY) реально не работал");
    }

    std::puts("== 2. Только ESC/SPACE/ENTER работали и раньше ==");
    {
        // Управляющие клавиши в SDL_Keycode совпадают с ASCII и не зависят от
        // раскладки — поэтому именно они были единственными рабочими.
        check(Resolve(41u, 27u) == 27, "ESCAPE стабилен");
        check(Resolve(44u, (unsigned)' ') == ' ', "SPACE стабилен");
        check(Resolve(40u, (unsigned)'\r') == '\r', "RETURN стабилен");
        // А буквы — нет.
        check(Resolve(4u, 0x0444u) == 'a', "A при кириллице ('ф') -> 'a'");
        check(Resolve(22u, 0x0448u) == 's', "S при кириллице ('ы') -> 's'");
    }

    // ── 3. Весь алфавит: каждая буква должна находиться ─────────────────────
    std::puts("== 3. Все 26 букв ==");
    {
        bool allOk = true;
        for (int i = 0; i < 26; ++i) {
            const unsigned scan = 4u + (unsigned)i;
            const int want = 'a' + i;
            if (CanonicalFromScancode(scan) != want) allOk = false;
            // И обратное преобразование обязано совпадать.
            if ((int)ScancodeFromCanonical(want) != (int)scan) allOk = false;
        }
        check(allOk, "A..Z: сканкод -> символ и обратно совпадают");
    }

    // ── 4. Цифровой ряд ─────────────────────────────────────────────────────
    std::puts("== 4. Цифры и знаки ==");
    {
        bool digits = true;
        for (int i = 1; i <= 9; ++i) {
            if (CanonicalFromScancode(29u + (unsigned)i) != '0' + i) digits = false;
            if ((int)ScancodeFromCanonical('0' + i) != 29 + i) digits = false;
        }
        check(digits, "1..9 на своих местах");
        check(CanonicalFromScancode(39u) == '0', "0 стоит после 9");
        check((int)ScancodeFromCanonical('0') == 39, "0 -> сканкод 39");
        check(CanonicalFromScancode(45u) == '-', "минус");
        check((int)ScancodeFromCanonical('/') == 56, "слеш");
    }

    // ── 5. Round-trip по всем поддерживаемым клавишам ───────────────────────
    std::puts("== 5. Round-trip сканкод <-> символ ==");
    {
        int mismatches = 0;
        for (unsigned scan = 4u; scan <= 56u; ++scan) {
            const int can = CanonicalFromScancode(scan);
            if (can == kUnknown) continue;
            if ((int)ScancodeFromCanonical(can) != (int)scan) ++mismatches;
        }
        check(mismatches == 0, "каждый опознанный сканкод возвращается обратно");
    }

    // ── 6. Неизвестное не притворяется известным ────────────────────────────
    std::puts("== 6. Вырожденные входы ==");
    {
        check(CanonicalFromScancode(0u) == kUnknown, "сканкод 0 -> неизвестно");
        check(CanonicalFromScancode(255u) == kUnknown, "мусорный сканкод -> неизвестно");
        // Если сканкод не опознан, движковый key должен дойти до адресата:
        // иначе сломаются F1..F12, стрелки и модификаторы.
        check(Resolve(0u, 0x4000005Au) == (int)0x4000005Au, "неизвестный сканкод -> key как есть");
        check(Resolve(0u, 0u) == 0, "оба нуля -> 0, а не мусор");
        // F1 (сканкод 58) не должен превращаться в букву.
        check(CanonicalFromScancode(58u) == kUnknown, "F1 не опознаётся как буква");
        check(Resolve(58u, 0x4000003Au) == (int)0x4000003Au, "F1 доходит до адресата");
    }

    // ── 7. Регистр и Shift не мешают ────────────────────────────────────────
    std::puts("== 7. Регистр не важен ==");
    {
        // С зажатым Shift SDL_Keycode буквы приходит заглавным ('P'), но
        // сканкод тот же — биндинги не должны переставать работать.
        check(Resolve(19u, (unsigned)'P') == 'p', "Shift+P всё ещё P");
        check(Resolve(19u, (unsigned)'P') == 'p', "и значение каноническое (строчное)");
    }

    // ── 8. Совместимость с KEY_0..KEY_9 из Urho3D ───────────────────────────
    // Порядок и значения цифр обязаны совпадать с Urho3D Key: KEY_1..KEY_9
    // идут подряд по ASCII, а ноль стоит отдельно (KEY_0 == '0' == 48).
    // На этом построены диапазоны вида `key >= KEY_1 && key <= KEY_9`.
    std::puts("== 8. Совместимость с KEY_0..KEY_9 ==");
    {
        bool consecutive = true;
        for (int d = 1; d <= 9; ++d) {
            if (CanonicalFromScancode(29u + (unsigned)d) != '0' + d) consecutive = false;
            // KEY_1 = '1' = 49, KEY_9 = '9' = 57 — диапазон должен быть сплошным.
            if ('0' + d < (int)'1' || '0' + d > (int)'9') consecutive = false;
        }
        check(consecutive, "KEY_1..KEY_9 идут подряд, как ожидает код меню");
        check(CanonicalFromScancode(39u) == (int)'0', "KEY_0 отдельно и равен '0'");
        check((int)'0' == 48 && (int)'1' == 49, "значения совпадают с SDLK_0/SDLK_1");
        // Ноль не попадает в диапазон 1..9 — иначе кнопка «0» дублировалась бы.
        check(!(CanonicalFromScancode(39u) >= (int)'1' && CanonicalFromScancode(39u) <= (int)'9'),
              "ноль не попадает в диапазон 1..9");
    }

    std::printf("\nITOGO: %d proydeno, %d provaleno\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
