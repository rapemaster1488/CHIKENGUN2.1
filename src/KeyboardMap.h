#pragma once

// KeyboardMap — раскладко-независимые клавиши.
//
// ЖАЛОБА: «ввод опять не работает, работает только ESCAPE, SPACE и ENTER».
// Это не совпадение. Urho3D отдаёт в событии E_KEYDOWN два разных значения:
//
//   P_KEY       — SDL_Keycode, символ С УЧЁТОМ РАСКЛАДКИ (SDLK_p = 'p');
//   P_SCANCODE  — физическое положение клавиши (SDL_SCANCODE_P = 19).
//
// При русской раскладке клавиша P приходит как кириллица 'п' (0x043F), и
// сравнение `key == KEY_P` не срабатывает НИКОГДА. А ESCAPE, SPACE, ENTER и
// стрелки — единственные клавиши, у которых P_KEY не зависит от раскладки
// (SDL_Keycode для них = управляющий ASCII). Отсюда ровно тот симптом,
// который видит игрок: «работает только эскейп, пробел и энтер».
//
// Лечение: все буквенные и цифровые привязки читаем по СКАНКОДУ, а значение
// приводим к каноническому ASCII-коду (SDLK_a == 'a'). Тогда биндинги не
// зависят ни от раскладки, ни от языковой панели.
//
// Модуль без Urho3D и без SDL: только числа. Поэтому он проверяется обычным
// unit-тестом (tests/test_keyboard_map.cpp), без окна и без GPU.

namespace Keys {

// Значения-«неизвестно». SDLK_UNKNOWN и SDL_SCANCODE_UNKNOWN оба равны 0,
// поэтому ноль означает «клавиша не опознана» и мы честно отдаём то, что
// прислал движок (или 0, если не прислал ничего).
constexpr int kUnknown = 0;

// Канонический код клавиши по физическому положению.
//
// USB HID usage, который SDL использует как SDL_Scancode:
//   4..29   — A..Z
//   30..38  — 1..9
//   39      — 0
//   40..56  — служебные и пунктуация
//
// Возвращаем ASCII-код (для букв — строчная латиница), потому что именно
// этими значениями оперирует Urho3D Key: KEY_A = SDLK_a = 'a'.
inline int CanonicalFromScancode(unsigned scan) {
    // Буквы: одна арифметика, без таблицы из 26 строк.
    if (scan >= 4u && scan <= 29u) return 'a' + (int)(scan - 4u);
    // Цифровой ряд: 1..9 идут подряд, ноль стоит последним.
    if (scan >= 30u && scan <= 38u) return '1' + (int)(scan - 30u);
    if (scan == 39u) return '0';
    switch (scan) {
        case 40u: return '\r';    // SCANCODE_RETURN
        case 41u: return 27;      // SCANCODE_ESCAPE
        case 42u: return '\b';    // SCANCODE_BACKSPACE
        case 43u: return '\t';    // SCANCODE_TAB
        case 44u: return ' ';     // SCANCODE_SPACE
        case 45u: return '-';
        case 46u: return '=';
        case 47u: return '[';
        case 48u: return ']';
        case 49u: return '\\';
        case 51u: return ';';
        case 52u: return '\'';
        case 53u: return '`';
        case 54u: return ',';
        case 55u: return '.';
        case 56u: return '/';
        default:  return kUnknown;
    }
}

// Итоговое значение клавиши для сравнения с KEY_*.
//
// Порядок предпочтения:
//   1) если сканкод опознан — берём канонический код (раскладка не важна);
//   2) иначе оставляем то, что прислал движок (нужно для F1..F12, стрелок,
//      модификаторов и клавиатур без нормальных сканкодов).
inline int Resolve(unsigned scan, unsigned key) {
    const int canonical = CanonicalFromScancode(scan);
    if (canonical != kUnknown) return canonical;
    return (int)key;
}

// Обратное преобразование: канонический код -> физический сканкод.
// Нужно для опроса удержания (Input::GetScancodeDown): GetKeyDown тоже
// раскладко-зависим и при русской раскладке не видит W/A/S/D.
// Возвращает kUnknown, если клавиша не имеет смысла удержания.
inline unsigned ScancodeFromCanonical(int canonical) {
    if (canonical >= 'a' && canonical <= 'z') return 4u + (unsigned)(canonical - 'a');
    if (canonical >= '1' && canonical <= '9') return 30u + (unsigned)(canonical - '1');
    if (canonical == '0') return 39u;
    switch (canonical) {
        case '\r': return 40u;   // RETURN
        case 27:   return 41u;   // ESCAPE
        case '\b': return 42u;   // BACKSPACE
        case '\t': return 43u;   // TAB
        case ' ':  return 44u;   // SPACE
        case '-':  return 45u;
        case '=':  return 46u;
        case '[':  return 47u;
        case ']':  return 48u;
        case '\\': return 49u;
        case ';':  return 51u;
        case '\'': return 52u;
        case '`':  return 53u;
        case ',':  return 54u;
        case '.':  return 55u;
        case '/':  return 56u;
        default:   return kUnknown;
    }
}

// Буква ли это (a..z) — удобно для отладочной телеметрии.
inline bool IsLetter(int canonical) { return canonical >= 'a' && canonical <= 'z'; }

// Является ли нажатие этой физической клавишей-буквой.
// wanted — строчная латиница ('p', 'j', ...).
inline bool Matches(unsigned scan, unsigned key, char wanted) {
    return Resolve(scan, key) == (int)wanted;
}

} // namespace Keys
