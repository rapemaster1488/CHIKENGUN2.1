#pragma once
// ============================================================================
// BanyaSim.h — Симулятор парной «Кочет в баньке» для CHIKENGUN 2.3
// Полная C++17 реализация термодинамики парной, 4 действий со стрик-окнами,
// 57 загадок смотрящего (без повторов и с перемешиванием вариантов),
// всех 14 каноничных концовок, статистики захода/сессии и наград для Арены.
// Не зависит от заголовков рендера — напрямую покрыт тестами в tests/test_core.cpp.
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Banya {

constexpr float DURATION      = 180.0f; // 3:00 до выхода из бани
constexpr float COMFORT_MIN   = 66.0f;  // нижняя граница золотой середины
constexpr float COMFORT_MAX   = 96.0f;  // верхняя граница золотой середины
constexpr int   RIDDLE_GOAL   = 3;      // минимум загадок для зачёта
constexpr int   STEWARD_GOAL  = 6;      // минимум загадок на «смотрящего»
constexpr int   TOTAL_ENDINGS = 14;
constexpr int   ALL_ENDINGS_MASK = (1 << TOTAL_ENDINGS) - 1; // 16383
constexpr int   TOTAL_RIDDLES = 57;

enum Phase : int {
    PHASE_PLAY = 0,
    PHASE_OVER = 1
};

enum EndingId : int {
    END_NONE      = -1,
    END_FIRE      = 0,  // Баня сгорела (fire >= 100)
    END_BLAST     = 1,  // Печь рванула (поддача при temp >= 108)
    END_BURNED    = 2,  // Кочет сгорел (life <= 0 при temp >= 96)
    END_FROZEN    = 3,  // Кочет замёрз (life <= 0 при temp <= 66)
    END_EXHAUSTED = 4,  // Кочет сдался (life <= 0 при 66 < temp < 96)
    END_CHOKE     = 5,  // Кочет угорел (air <= 0)
    END_VAPOR     = 6,  // Кочет растворился в пару (steam > 0.9 непрерывно > 20 с)
    END_DROWN     = 7,  // Кочет утонул в шайке (4x «окатить» за 6 с)
    END_ESCAPE    = 8,  // Кочет сбежал в лес залуп (3x «проветрить» за 8 с)
    END_RAGE      = 9,  // Кочет озверел (4x «веником» за 6 с)
    END_SENTRY    = 10, // Смотрящий осерчал (3 промаха по загадкам подряд)
    END_NO_CREDIT = 11, // Вышел, но зачёта нет (3:00, загадок < 3)
    END_WIN       = 12, // Вышел с чистой совестью (3:00, загадок 3..5, +60 очков)
    END_STEWARD   = 13  // Кочет стал смотрящим (3:00, загадок >= 6, +120 очков)
};

struct EndingInfo {
    EndingId    id;
    const char* key;
    const char* title;
    const char* text;
    const char* hint;
    float       tintR, tintG, tintB, tintA;
    int         bonusScore;
};

inline const EndingInfo& GetEndingInfo(int endingId) {
    static const EndingInfo kTable[TOTAL_ENDINGS] = {
        { END_FIRE, "fire", "Баня сгорела",
          "Кочет поддавал и поддавал — печь не выдержала и занялась. Банька полыхнула так, что снег во дворе растаял, а кочет вылетел из двери с веником в клюве.",
          "Следи за шкалой «печь»: между поддачами ей нужно остывать. Окатить и проветрить тоже сбивают жар в печи.",
          1.00f, 0.42f, 0.12f, 0.34f, 0 },
        { END_BLAST, "blast", "Печь рванула",
          "Вода на камни, раскалённые докрасна, — и баньку разнесло по брёвнышку. Кочет улетел в сугроб вместе с веником и до сих пор не вернулся.",
          "Не поддавай пару выше 108 °C: камни уже на грани. Сначала окати или проветри, потом лей воду.",
          1.00f, 0.69f, 0.23f, 0.40f, 0 },
        { END_BURNED, "burned", "Кочет сгорел",
          "Слишком жарко: кочет распарился до красного гребешка, обмяк на полке и тихо скончался, как пельмень в кипятке.",
          "Красная зона — выше 100 °C. Держи жар в золотой середине: 66–96 °C.",
          1.00f, 0.35f, 0.12f, 0.24f, 0 },
        { END_FROZEN, "frozen", "Кочет замёрз",
          "Дверь была открыта слишком долго, пар ушёл, и кочет превратился в ледяную фигурку с гребешком. Красиво, но непоправимо.",
          "Проветривай короткими залпами: ниже 62 °C силы кочета тают. После проветривания сразу поддай пару.",
          0.66f, 0.85f, 0.93f, 0.50f, 0 },
        { END_EXHAUSTED, "exhausted", "Кочет сдался",
          "Ни жарко, ни холодно — просто силы кончились. Кочет слез с полка и уполз в предбанник, не дожидаясь выхода.",
          "В золотой середине (66–96 °C) силы восстанавливаются. Плюс загадки: верный ответ даёт силы.",
          0.42f, 0.35f, 0.29f, 0.28f, 0 },
        { END_CHOKE, "choke", "Кочет угорел",
          "Пар забил всю баньку под потолок, свежего воздуха не осталось — кочет обмяк и сполз с полка в шайку.",
          "Шкала «воздух» падает тем быстрее, чем больше пара. Проветривание возвращает её сразу — не жди последнего.",
          0.50f, 0.75f, 0.54f, 0.34f, 0 },
        { END_VAPOR, "vapor", "Кочет растворился в пару",
          "Пар держался на максимуме слишком долго. Когда облако рассеялось, на полке лежали только веник и мокрая войлочная шапка.",
          "Шкала «пар» на упоре двадцать секунд — и кочет уходит в облако. Проветривай раньше, чем она упрётся.",
          0.91f, 0.93f, 0.95f, 0.44f, 0 },
        { END_DROWN, "drown", "Кочет утонул в шайке",
          "Четвёртая шайка подряд — пол мокрый, лапы скользкие. Кочет оскользнулся и ушёл в воду с головой. С мылом.",
          "Окатить можно раз-два, потом дай кочету обсохнуть: четыре оката подряд — это уже не баня, а бассейн.",
          0.50f, 0.71f, 0.80f, 0.42f, 0 },
        { END_ESCAPE, "escape", "Кочет сбежал в лес залуп",
          "Три проветривания подряд, дверь настежь, сквозняк — и кочет, не будь дурак, ушёл налегке в лес залуп. Веник не взял, пара не попрощался.",
          "Проветривай по одному разу и закрывай дверь: три подряд — и он свалит.",
          0.60f, 0.71f, 0.78f, 0.38f, 0 },
        { END_RAGE, "rage", "Кочет озверел",
          "Четвёртый веник подряд — и кочет, распаренный до беспредела, вышиб дверь вместе со смотрящим. Баньку закрыли на ремонт до зимы.",
          "Веник — не автомат: давай кочету передышку между парениями.",
          1.00f, 0.48f, 0.35f, 0.30f, 0 },
        { END_SENTRY, "sentry", "Смотрящий осерчал",
          "Три загадки подряд не в масть — смотрящий вылил шайку, погасил печь и выставил кочета за дверь. Зачёт не закрыт, банька не по тебе.",
          "Не ответил или ответил неверно — считается. Три промаха подряд — и всё, разговор окончен.",
          0.54f, 0.35f, 0.23f, 0.36f, 0 },
        { END_NO_CREDIT, "noCredit", "Вышел, но зачёта нет",
          "Дверь открылась, кочет вышел распаренный — а смотрящий покачал головой: загадок мало. Банька была, а зачёта не будет.",
          "Нужно минимум три верных загадки за заход, иначе выход не считается.",
          0.54f, 0.48f, 0.41f, 0.26f, 0 },
        { END_WIN, "win", "Вышел с чистой совестью",
          "Три минуты в парной — и кочет выкатился на снег распаренный, счастливый и с веником под крылом. Хорошая была банька.",
          "Три верных загадки — зачёт закрыт. Шесть — и ты уже смотрящий.",
          1.00f, 0.82f, 0.48f, 0.22f, 60 },
        { END_STEWARD, "steward", "Кочет стал смотрящим",
          "Дожил до выхода, разгадал шесть загадок — и смотрящий отдал ему ковш. Теперь кочет тут главный: ковш, веник, уважение и свой полок.",
          "Лучшая концовка: три минуты и минимум шесть верных загадок.",
          1.00f, 0.86f, 0.54f, 0.30f, 120 }
    };
    int idx = (endingId >= 0 && endingId < TOTAL_ENDINGS) ? endingId : 0;
    return kTable[idx];
}

struct RiddleDef {
    const char* q;
    const char* o[4];
    int         a;
    const char* e;
};

inline const RiddleDef (&GetAllRiddles())[TOTAL_RIDDLES] {
    static const RiddleDef kRiddles[TOTAL_RIDDLES] = {
        // --- банные, с подвохом (25) ---
        { "Что входит в баню сухим, а выходит мокрым и распаренным?",
          { "Веник", "Ковш", "Полотенце", "Сам кочет" }, 0,
          "Веник запаривают: был сухой пучок веток — стал мокрый и душистый." },
        { "Чем больше поддаёшь — тем жарче и тем меньше видно. Что это?",
          { "Пар", "Дым", "Пыль", "Туман" }, 0,
          "Пар: больше воды на камнях — гуще облако." },
        { "Что значит «поддать пару»?",
          { "Плеснуть воды на камни", "Открыть дверь", "Побить веником", "Подкинуть дров" }, 0,
          "Именно плеснуть: вода на раскалённые камни — и пар пошёл." },
        { "Сколько раз можно поддать пару одним ковшом?",
          { "Один", "Два", "Три", "Пока не кончится" }, 0,
          "Ковш — на один заход. Второй раз — это уже пустой ковш." },
        { "Что нельзя унести из бани, даже если очень хочется?",
          { "Пар", "Веник", "Шайку", "Кочета" }, 0,
          "Пар уходит в щёлочку, а веник — унеси на здоровье." },
        { "Что тяжелее: мокрый веник или сухой?",
          { "Мокрый", "Сухой", "Одинаково", "Зависит от листьев" }, 0,
          "Мокрый тяжелее — в нём вода. И бьёт он тоже тяжелее." },
        { "Что не тонет в шайке?",
          { "Сухой веник", "Мокрый веник", "Ковш", "Кочет" }, 0,
          "Сухой веник плавает, мокрый тонет. Как и люди." },
        { "Что будет, если поддать пару и сразу проветрить?",
          { "Пар уйдёт", "Пар станет гуще", "Печь рванёт", "Ничего" }, 0,
          "Сначала пар, потом проветривание — иначе всё в щёлочку." },
        { "Куда девается пар, когда открываешь дверь?",
          { "В щёлочку", "В лес залуп", "В море говна", "В кочета" }, 0,
          "В щёлочку и уходит. Поэтому проветривать надо коротко." },
        { "Что делать, если в бане кончился пар, а веник ещё сухой?",
          { "Плеснуть воды на камни", "Дышать глубже", "Ждать", "Обмахивать веником" }, 0,
          "Плеснуть: сухой веник — это дрова, мокрый — инструмент." },
        { "Чем больше берёшь, тем меньше остаётся. Что это?",
          { "Вода в шайке", "Веники", "Пар", "Огонь" }, 0,
          "Вода: берёшь ковшом — шайка пустеет." },
        { "Что нужно сделать, прежде чем сесть на полок?",
          { "Постелить полотенце", "Плеснуть воды", "Позвать смотрящего", "Снять веник" }, 0,
          "Полотенце: полок горячий, а попа одна." },
        { "Сколько минут длится «пять минут в бане»?",
          { "Пока не выгонят", "Ровно пять", "Десять", "Три" }, 0,
          "Пока не выгонят. Это же баня, а не электричка." },
        { "Кто первым выйдет из бани?",
          { "Тот, кто первый вспотел", "Смотрящий", "Кочет", "Тот, кто ближе к двери" }, 0,
          "Кто первый вспотел — тот первый и на снег." },
        { "Что нельзя увидеть в бане, даже если очень париться?",
          { "Свой затылок", "Пар", "Полок", "Ковш" }, 0,
          "Свой затылок. Остальное — сколько угодно." },
        { "В бане жарко, в шайке холодно, в голове пусто. Что это?",
          { "Нормальная баня", "Перегрев", "Угар", "Простуда" }, 0,
          "Нормальная баня. Если в голове ещё и мысли — это уже перегар." },
        { "Что общего у веника и загадки?",
          { "Бьют по мозгам", "Оба из берёзы", "Оба мокрые", "Ничего" }, 0,
          "Веник по спине, загадка по мозгам. Эффект один." },
        { "Что нужно, чтобы стать смотрящим?",
          { "Ковш", "Веник", "Ключ", "Слово" }, 0,
          "Ковш. Кто держит ковш — тот и главный." },
        { "Что делает смотрящий, когда все парятся?",
          { "Смотрит", "Парится", "Спит", "Топит" }, 0,
          "Смотрит. Поэтому он и смотрящий." },
        { "Что делает кочет, когда смотрящий отвернулся?",
          { "Пьёт воду из шайки", "Поёт", "Спит", "Парится" }, 0,
          "Пьёт. Париться он и при смотрящем может." },
        { "Сколько зэков влезет в шайку?",
          { "Ни одного", "Один", "Трое", "Сколько влезет" }, 0,
          "Шайка — для воды. Зэк в шайке — это уже отдельная концовка." },
        { "Три зэка мылись в одной шайке. Сколько было шайок?",
          { "Одна", "Три", "Ни одной", "Пять" }, 0,
          "Одна. В этом и вся арифметика." },
        { "Что нужно кочету для полного счастья?",
          { "Веник, пар и ковш", "Море говна", "Лес залуп", "Холодная шайка" }, 0,
          "Веник, пар и ковш. Остальное — от лукавого." },
        { "Что будет, если плеснуть в печь бензина?",
          { "Баня сгорит", "Пар станет гуще", "Ничего", "Печь остынет" }, 0,
          "Полыхнёт так, что кочета найдут в сугробе." },
        { "Что делать, если кочет угорел?",
          { "Проветрить и вынести на снег", "Ещё пару", "Веником", "Ждать" }, 0,
          "Свежий воздух и снег. Паром угар не лечат." },

        // --- тюремно-абсурдные (14) ---
        { "Летишь на самолёте, движок отказал. Внизу — лес залуп и море говна. Куда десантируешься?",
          { "В лес залуп", "В море говна", "В шайку к кочету", "Останусь в самолёте" }, 1,
          "В море говна: плюх — и мягко, как в парную с головой. А лес залуп тебя сам встретит." },
        { "Летишь на самолёте, движок отказал, а внизу только банька с кочетом. Куда прыгаешь?",
          { "На полок", "На камни", "В шайку", "На кочета" }, 0,
          "На полок: там пар, веник и кочет. Даже падать приятно." },
        { "Движок отказал: слева лес залуп, справа море говна, впереди баня с кочетом. Куда?",
          { "В баню", "В лес залуп", "В море говна", "Вниз" }, 0,
          "В баню: везде плохо, а в бане хоть пар, веник и кочет." },
        { "Просыпаешься в бане: в одной руке веник из крапивы, в другой — ковш кипятка. Что отпустишь первым?",
          { "Веник", "Ковш", "Оба", "Ничего" }, 1,
          "Ковш: кипяток — это к доктору, а крапива — это к здоровью. Веник держи." },
        { "На выбор: месяц без бани или месяц в бане с закрытыми глазами. Что берёшь?",
          { "Без бани", "В бане с закрытыми глазами", "Полтора месяца без бани", "Уехать к морю говна" }, 1,
          "В бане глаза всё равно не нужны: пар, веник, темнота и благодать." },
        { "У тебя две шайки: одна с водой, другая с говном. Ты окатился. Как узнать, что взял не ту?",
          { "По запаху", "По цвету", "По весу", "Никак" }, 0,
          "По запаху. Остальные признаки в парной врут." },
        { "Тебе предложили: век не париться или раз в год попариться в бане с говном. Что берёшь?",
          { "Век не париться", "Раз в год в бане с говном", "Оба", "Ни то, ни другое" }, 1,
          "Раз в год — это терпимо. Век без пара — нет." },
        { "В бане три шайки: пустая, полная и с говном. В какой париться?",
          { "В пустой", "В полной", "В третьей", "Ни в какой" }, 1,
          "В полной — там вода. Остальные варианты даже обсуждать не будем." },
        { "Сколько веников можно унести из бани, чтобы никто не заметил?",
          { "Один", "Два", "Сколько унесёшь", "Ни одного" }, 0,
          "Один: второй веник всегда шуршит громче первого." },
        { "Сколько нужно кочетов, чтобы получился один смотрящий?",
          { "Ни одного", "Один", "Три", "Пять" }, 0,
          "Смотрящий не из кочетов делается, а из ковша." },
        { "Что делает веник, когда его не парят?",
          { "Сохнет и ждёт", "Падает", "Плачет", "Уходит" }, 0,
          "Сохнет и ждёт своего часа. Как все мы." },
        { "Что делает пар, когда его не видно?",
          { "Всё равно есть", "Исчезает", "Превращается в воду", "Уходит в печь" }, 0,
          "Он есть, просто невидимый. Как и проблемы." },
        { "Сколько загадок можно разгадать за один заход?",
          { "Сколько успеешь", "Три", "Шесть", "Ни одной" }, 0,
          "Сколько успеешь: загадки не повторяются, но и не бесконечны." },
        { "Кто в бане всех громче?",
          { "Тот, кому плеснули кипятком", "Смотрящий", "Кочет", "Веник" }, 0,
          "Кипяток на полок — и самый молчаливый становится солистом." },

        // --- счёт и логика (18) ---
        { "Кочет начал с 70 °C и трижды поддал пару, каждый раз +7 °C. Что на градуснике?",
          { "84 °C", "91 °C", "98 °C", "77 °C" }, 1,
          "70 + 7 × 3 = 91 °C." },
        { "Кирпич весит килограмм и полкирпича. Сколько весит кирпич?",
          { "2 кг", "1,5 кг", "1 кг", "2,5 кг" }, 0,
          "Полкирпича — килограмм, значит целый — два." },
        { "Ковш — 2 литра, шайка — 10. Сколько ковшей до полной?",
          { "5", "4", "10", "2" }, 0,
          "10 : 2 = 5. Только воду не разлей." },
        { "У кочета два крыла, у зэка две руки. В бане три кочета и два зэка. Сколько крыльев?",
          { "6", "4", "10", "5" }, 0,
          "Крылья только у кочетов: 3 × 2 = 6." },
        { "В бане четыре угла, в каждом углу по кочету. Напротив каждого — ещё по три кочета. Сколько всего кочетов?",
          { "4", "8", "12", "16" }, 0,
          "Их четверо: каждый видит трёх остальных." },
        { "Три кочета парились три часа. Сколько часов парился каждый?",
          { "По 3 часа", "6 часов на двоих", "Кочет не парится", "По-разному" }, 0,
          "Парились-то одновременно, каждый свои три часа." },
        { "Сколько яиц можно съесть натощак?",
          { "Одно", "Три", "Пять", "Сколько влезет" }, 0,
          "Только одно: после первого завтрак уже не натощак." },
        { "Что тяжелее — килограмм пара или килограмм говна?",
          { "Пар", "Говно", "Одинаково", "Зависит от бани" }, 2,
          "Килограмм есть килограмм. Просто одно хотя бы не пахнет." },
        { "В одной бане 100 °C. Сколько градусов в двух таких банях?",
          { "100", "200", "50", "150" }, 0,
          "Температура не складывается: в каждой свои 100." },
        { "Два отца и два сына вышли из бани, а всего их трое. Как так?",
          { "Дед, отец и сын", "Один был маленький", "Один остался внутри", "Так не бывает" }, 0,
          "Дед — отец отца, отец — сын деда и отец внука." },
        { "На полке лежали пять веников. Один ты взял и попарился. Сколько веников осталось лежать на полке?",
          { "4", "5", "3", "Ни одного" }, 0,
          "Четыре: взятый веник уже не лежит на полке." },
        { "Четыре зэка в бане, у каждого веник. Напротив каждого — по три веника. Сколько веников в бане?",
          { "4", "8", "12", "16" }, 0,
          "Четыре: каждый видит три чужих, а веники считаются один раз." },
        { "Логика парной: каждое проветривание сбивает пар вдвое. Сколько проветриваний нужно, чтобы от 100 % осталось меньше 13 %?",
          { "Два", "Три", "Четыре", "Одно" }, 1,
          "100 → 50 → 25 → 12,5 %. Три проветривания." },
        { "Сколько раз надо поддать пару подряд, чтобы баня занялась?",
          { "Пять, если не думать", "Два", "Никогда", "Двадцать" }, 0,
          "Печь копит жар: пять быстрых поддач подряд — и она занимается." },
        { "Из какого дерева веник, если у него белая кора и серёжки?",
          { "Берёза", "Дуб", "Осина", "Ель" }, 0,
          "Берёза: белая кора и серёжки — её и парят." },
        { "Сколько зэков нужно, чтобы попарить одного кочета?",
          { "Один", "Два", "Три", "Ни одного" }, 1,
          "Два: один парит, второй следит, чтобы кочет не угорел." },
        { "Кто в бане главный, если все равны?",
          { "Тот, кто топит", "Тот, кто парит", "Тот, кто держит ковш", "Все равны" }, 2,
          "Ковш — это власть. Без воды камни молчат, и никакой бани нет." },
        { "Самолёт падает, под тобой баня. Прыгаешь в баню или в самолёте сидишь?",
          { "В баню", "В самолёте", "В море говна", "Звоню смотрящему" }, 0,
          "В баню: там хоть пар, веник и кочет. В самолёте париться нечем." }
    };
    return kRiddles;
}

struct ActiveRiddle {
    bool        active      = false;
    bool        settled     = false;
    int         sourceIdx   = 0;
    const char* q           = "";
    const char* opts[4]     = { "", "", "", "" };
    int         correctIdx  = 0;
    int         chosenIdx   = -1;
    const char* explanation = "";
    float       timeLeft    = 15.0f;
    float       closeTimer  = 0.0f;
};

struct RunStats {
    float maxTemp    = 72.0f;
    float minTemp    = 72.0f;
    float secComfort = 0.0f;
    float secHot     = 0.0f;
    float secCold    = 0.0f;
    int   pours      = 0;
    int   veniks     = 0;
    int   rinses     = 0;
    int   vents      = 0;
    int   asked      = 0;
    int   correct    = 0;
    int   wrong      = 0;
    int   missed     = 0;
    int   streak     = 0;
    int   bestStreak = 0;
    float minAir     = 100.0f;
    float maxFire    = 18.0f;
};

struct LifetimeStats {
    int runs            = 0;
    int wins            = 0;
    int bestScore       = 0;
    int correct         = 0;
    int wrong           = 0;
    int bestStreak      = 0;
    int totalSeconds    = 0;
    int bestRiddles     = 0;
    int seenEndingsMask = 0;
    int endingCounts[TOTAL_ENDINGS] = {};
};

inline int CountSeenEndings(int mask) {
    int m = mask & ALL_ENDINGS_MASK;
    int cnt = 0;
    for (int i = 0; i < TOTAL_ENDINGS; ++i) {
        if (m & (1 << i)) ++cnt;
    }
    return cnt;
}

inline int ComputeScore(float elapsed, int riddles, EndingId ending) {
    int base = (int)std::lround(std::min(std::max(0.0f, elapsed), DURATION)) + riddles * 15;
    int bonus = (ending >= 0 && ending < TOTAL_ENDINGS) ? GetEndingInfo(ending).bonusScore : 0;
    return base + bonus;
}

// Награда в кибер-кредитах (₵) для Арсенала/Перков CHIKENGUN 2.3
inline int ComputeCreditsReward(int banyaScore, EndingId ending) {
    int base = std::max(15, (int)std::lround(banyaScore * 0.5));
    if (ending == END_STEWARD) base += 80;
    else if (ending == END_WIN) base += 40;
    return base;
}

// Боевой бафф после парной: 0 = нет, 1 = Банный закал (win), 2 = Ковш Смотрящего (steward)
inline int BuffTierForEnding(EndingId ending) {
    if (ending == END_STEWARD) return 2;
    if (ending == END_WIN)     return 1;
    return 0;
}

class Simulator {
public:
    Phase        phase          = PHASE_PLAY;
    EndingId     ending         = END_NONE;
    bool         paused         = false;

    float        temp           = 72.0f;
    float        steam          = 0.28f;
    float        fire           = 18.0f;
    float        air            = 100.0f;
    float        life           = 100.0f;
    float        elapsed        = 0.0f;
    float        left           = DURATION;

    int          riddles        = 0;
    int          wrongStreak    = 0;
    float        steamMax       = 0.0f;
    float        ventRemaining  = 0.0f;
    float        blissRemaining = 0.0f;
    float        nextRiddle     = 12.0f;
    float        riddleCooldown = 0.0f;

    ActiveRiddle  riddle;
    RunStats      run;
    LifetimeStats lifetime;

    explicit Simulator(uint32_t seed = 0xC0C4E7u) : rngState_(seed ? seed : 1u) {
        Reset();
    }

    void Seed(uint32_t s) { rngState_ = s ? s : 1u; }

    void Reset() {
        phase          = PHASE_PLAY;
        ending         = END_NONE;
        paused         = false;
        temp           = 72.0f;
        steam          = 0.28f;
        fire           = 18.0f;
        air            = 100.0f;
        life           = 100.0f;
        elapsed        = 0.0f;
        left           = DURATION;
        riddles        = 0;
        wrongStreak    = 0;
        steamMax       = 0.0f;
        ventRemaining  = 0.0f;
        blissRemaining = 0.0f;
        nextRiddle     = 12.0f;
        riddleCooldown = 0.0f;
        clockNow_      = 0.0f;

        riddle = ActiveRiddle{};
        run = RunStats{};
        run.maxTemp = temp;
        run.minTemp = temp;
        run.minAir  = air;
        run.maxFire = fire;

        waterHist_.clear();
        ventHist_.clear();
        venikHist_.clear();
        RefillRiddlePool();
    }

    void SetPaused(bool p) { paused = p; }

    int CurrentScore() const {
        return ComputeScore(elapsed, riddles, ending);
    }

    const char* CurrentMood() const {
        if (blissRemaining > 0.0f) return "блаженство";
        if (temp > 104.0f)         return "красный, как гребешок!";
        if (temp > 96.0f)          return "жарковато, пора окатиться";
        if (temp >= COMFORT_MIN)   return "золотая середина";
        if (temp > 58.0f)          return "свежо, прохладненько";
        return "холодно, пёрышки дыбом";
    }

    // --- 4 основных действия ---
    bool ActSteam() {
        if (phase != PHASE_PLAY || paused) return false;
        if (temp >= 108.0f) {
            Finish(END_BLAST);
            return true;
        }
        run.pours++;
        temp = Clamp(temp + 7.5f, 36.0f, 120.0f);
        fire = Clamp(fire + 22.0f, 0.0f, 100.0f);
        air  = Clamp(air  - 4.0f,  0.0f, 100.0f);
        blissRemaining = std::max(blissRemaining, 1.4f);
        DeathCheck();
        return true;
    }

    bool ActVenik() {
        if (phase != PHASE_PLAY || paused) return false;
        temp = Clamp(temp + 3.0f, 36.0f, 120.0f);
        fire = Clamp(fire + 5.0f, 0.0f, 100.0f);
        air  = Clamp(air  - 3.0f, 0.0f, 100.0f);
        life = Clamp(life + 3.0f, 0.0f, 100.0f);
        run.veniks++;
        blissRemaining = 2.6f;
        if (PushStreak(venikHist_, 6.0f) >= 4) {
            Finish(END_RAGE);
            return true;
        }
        DeathCheck();
        return true;
    }

    bool ActWater(bool* outWasCold = nullptr) {
        if (phase != PHASE_PLAY || paused) return false;
        bool wasCold = (temp < 60.0f);
        if (outWasCold) *outWasCold = wasCold;
        run.rinses++;
        temp = Clamp(temp - 8.0f,  36.0f, 120.0f);
        fire = Clamp(fire - 18.0f, 0.0f,  100.0f);
        air  = Clamp(air  + 4.0f,  0.0f,  100.0f);
        if (wasCold) life = Clamp(life - 10.0f, 0.0f, 100.0f);
        blissRemaining = 0.0f;
        if (PushStreak(waterHist_, 6.0f) >= 4) {
            Finish(END_DROWN);
            return true;
        }
        DeathCheck();
        return true;
    }

    bool ActVent(bool* outWasCold = nullptr) {
        if (phase != PHASE_PLAY || paused) return false;
        bool wasCold = (temp < 72.0f);
        if (outWasCold) *outWasCold = wasCold;
        run.vents++;
        ventRemaining = 3.4f;
        air  = Clamp(air  + 55.0f, 0.0f, 100.0f);
        fire = Clamp(fire - 10.0f, 0.0f, 100.0f);
        if (wasCold) life = Clamp(life - 5.0f, 0.0f, 100.0f);
        if (PushStreak(ventHist_, 8.0f) >= 3) {
            Finish(END_ESCAPE);
            return true;
        }
        DeathCheck();
        return true;
    }

    // --- Загадки смотрящего ---
    bool OpenRiddle(bool forced) {
        if (phase != PHASE_PLAY || paused || riddle.active) return false;
        if (!forced && riddleCooldown > 0.0f) return false;
        riddle = TakeShuffledRiddle();
        run.asked++;
        return true;
    }

    bool AnswerRiddle(int optIdx) {
        if (phase != PHASE_PLAY || !riddle.active || riddle.settled) return false;
        riddle.settled   = true;
        riddle.chosenIdx = optIdx;
        bool ok = (optIdx == riddle.correctIdx);
        if (ok) {
            life = Clamp(life + 12.0f, 0.0f, 100.0f);
            air  = Clamp(air  + 8.0f,  0.0f, 100.0f);
            riddles++;
            wrongStreak = 0;
            run.correct++;
            run.streak++;
            run.bestStreak = std::max(run.bestStreak, run.streak);
            riddle.closeTimer = 1.8f;
        } else {
            bool missed = (optIdx < 0);
            wrongStreak++;
            run.streak = 0;
            if (missed) run.missed++; else run.wrong++;
            life = Clamp(life - (missed ? 16.0f : 14.0f), 0.0f, 100.0f);
            air  = Clamp(air  - (missed ? 8.0f  : 6.0f),  0.0f, 100.0f);
            riddle.closeTimer = 3.1f;
        }
        return true;
    }

    // Мгновенно закрыть решённую карточку загадки (используется и по таймеру, и в тестах)
    void FlushSettledRiddle() {
        if (!riddle.active || !riddle.settled) return;
        riddle.active   = false;
        riddle.settled  = false;
        nextRiddle      = elapsed + 26.0f;
        riddleCooldown  = 2.5f;
        if (wrongStreak >= 3) Finish(END_SENTRY);
        else DeathCheck();
    }

    // --- Шаг симуляции ---
    void Step(float dt) {
        if (phase != PHASE_PLAY || paused || dt <= 0.0f) return;

        elapsed += dt;
        left = std::max(0.0f, DURATION - elapsed);
        clockNow_ += dt;
        ventRemaining  = std::max(0.0f, ventRemaining  - dt);
        blissRemaining = std::max(0.0f, blissRemaining - dt);
        riddleCooldown = std::max(0.0f, riddleCooldown - dt);

        run.maxTemp = std::max(run.maxTemp, temp);
        run.minTemp = std::min(run.minTemp, temp);
        if (temp >= COMFORT_MIN && temp <= COMFORT_MAX) run.secComfort += dt;
        else if (temp > 100.0f) run.secHot += dt;
        else if (temp < 62.0f)  run.secCold += dt;
        run.minAir  = std::min(run.minAir,  air);
        run.maxFire = std::max(run.maxFire, fire);

        float cool = (temp - 42.0f) * 0.0085f + (ventRemaining > 0.0f ? 1.9f : 0.0f);
        temp = Clamp(temp - cool * dt, 36.0f, 120.0f);

        float targetSteam = Clamp((temp - 52.0f) / 58.0f, 0.0f, 1.0f);
        steam += (targetSteam - steam) * std::min(1.0f, dt * 1.1f);

        air  = Clamp(air  - (0.35f + steam * 2.0f) * dt, 0.0f, 100.0f);
        fire = Clamp(fire - 9.0f * dt, 0.0f, 100.0f);

        if (steam > 0.9f) steamMax += dt;
        else steamMax = 0.0f;
        if (steamMax > 20.0f) {
            Finish(END_VAPOR);
            return;
        }

        float lifeDelta = -0.2f;
        if (temp > 100.0f)       lifeDelta = -(temp - 100.0f) * 0.55f;
        else if (temp < 62.0f)   lifeDelta = -(62.0f - temp)  * 0.5f;
        else if (temp >= COMFORT_MIN && temp <= COMFORT_MAX) lifeDelta = 1.3f;
        life = Clamp(life + lifeDelta * dt, 0.0f, 100.0f);

        DeathCheck();
        if (phase == PHASE_OVER) return;

        if (riddle.active) {
            if (!riddle.settled) {
                riddle.timeLeft = std::max(0.0f, riddle.timeLeft - dt);
                if (riddle.timeLeft <= 0.0f) AnswerRiddle(-1);
            } else {
                riddle.closeTimer -= dt;
                if (riddle.closeTimer <= 0.0f) FlushSettledRiddle();
            }
        } else if (phase == PHASE_PLAY && elapsed >= nextRiddle) {
            OpenRiddle(true);
        }

        if (phase == PHASE_PLAY && left <= 0.0f) {
            Finish(riddles >= STEWARD_GOAL ? END_STEWARD : (riddles >= RIDDLE_GOAL ? END_WIN : END_NO_CREDIT));
        }
    }

    ActiveRiddle TakeShuffledRiddle() {
        if (riddlePool_.empty()) RefillRiddlePool();
        int ridx = riddlePool_.back();
        riddlePool_.pop_back();
        const RiddleDef& base = GetAllRiddles()[ridx];

        int perm[4] = { 0, 1, 2, 3 };
        for (int i = 3; i > 0; --i) {
            int j = (int)(NextRand() % (uint32_t)(i + 1));
            std::swap(perm[i], perm[j]);
        }

        ActiveRiddle out;
        out.active      = true;
        out.settled     = false;
        out.sourceIdx   = ridx;
        out.q           = base.q;
        out.explanation = base.e;
        out.timeLeft    = 15.0f;
        out.correctIdx  = 0;
        for (int i = 0; i < 4; ++i) {
            out.opts[i] = base.o[perm[i]];
            if (perm[i] == base.a) out.correctIdx = i;
        }
        return out;
    }

private:
    uint32_t rngState_ = 0xC0C4E7u;
    float    clockNow_ = 0.0f;
    std::vector<int>   riddlePool_;
    std::vector<float> waterHist_;
    std::vector<float> ventHist_;
    std::vector<float> venikHist_;

    static float Clamp(float v, float lo, float hi) {
        return std::min(hi, std::max(lo, v));
    }

    uint32_t NextRand() {
        uint32_t x = rngState_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rngState_ = x ? x : 1u;
        return rngState_;
    }

    void RefillRiddlePool() {
        riddlePool_.resize(TOTAL_RIDDLES);
        for (int i = 0; i < TOTAL_RIDDLES; ++i) riddlePool_[i] = i;
        for (int i = TOTAL_RIDDLES - 1; i > 0; --i) {
            int j = (int)(NextRand() % (uint32_t)(i + 1));
            std::swap(riddlePool_[i], riddlePool_[j]);
        }
    }

    int PushStreak(std::vector<float>& hist, float windowSec) {
        hist.push_back(clockNow_);
        while (!hist.empty() && (clockNow_ - hist.front()) > windowSec) {
            hist.erase(hist.begin());
        }
        return (int)hist.size();
    }

    void DeathCheck() {
        if (phase == PHASE_OVER) return;
        if (fire >= 100.0f) { Finish(END_FIRE);  return; }
        if (air  <= 0.0f)   { Finish(END_CHOKE); return; }
        if (life <= 0.0f) {
            EndingId cause = (temp >= 96.0f) ? END_BURNED : (temp <= 66.0f ? END_FROZEN : END_EXHAUSTED);
            Finish(cause);
        }
    }

    void Finish(EndingId kind) {
        if (phase == PHASE_OVER || kind < 0 || kind >= TOTAL_ENDINGS) return;
        phase  = PHASE_OVER;
        ending = kind;
        riddle.active  = false;
        riddle.settled = false;

        lifetime.seenEndingsMask |= (1 << (int)kind);
        lifetime.runs++;
        if (kind == END_WIN || kind == END_STEWARD) lifetime.wins++;
        lifetime.endingCounts[(int)kind]++;
        lifetime.bestScore    = std::max(lifetime.bestScore, ComputeScore(elapsed, riddles, kind));
        lifetime.correct     += run.correct;
        lifetime.wrong       += run.wrong + run.missed;
        lifetime.bestStreak   = std::max(lifetime.bestStreak, run.bestStreak);
        lifetime.totalSeconds += (int)std::lround(std::min(elapsed, DURATION));
        lifetime.bestRiddles  = std::max(lifetime.bestRiddles, riddles);
    }
};

} // namespace Banya
