/*
Module: engine/app
File: engine/app/sources/Peoples.h

Responsibility:
- НАРОДЫ ЯАНА КАК РАСПРЕДЕЛЕНИЯ: чтение каталога `assets/characters/peoples` (расширение .people),
  проверка народных краёв против судейских и ВЫБОРКА — один бросок даёт
  типаж, все ручки телосложения, имя и оттенки. Ни экрана, ни видеокарты, ни
  файлов сверх одного каталога.

Key items:
- People / PeopleArchetype / PeopleBand: народ, его края и его типажи.
- read_people() / read_peoples(): текстовый формат в духе .scene и .map.
- peoples_validate(): народный край ОБЯЗАН лежать внутри судейского; шире —
  громкий отказ, а не молчаливое обрезание.
- people_sample(): усечённая нормаль вокруг центра типажа, со скрытой связью
  «крупность». Принимает и ОДИН народ, и СМЕСЬ народов с весами.
- PeopleRng: своё зерно, потому что приёмка сравнивает прогон с прогоном.

Dependencies:
- Uses: только std. НАРОЧНО НИЧЕГО ИЗ app: единственный потребитель сегодня —
  экран создания персонажа, а завтра ими же кормится генератор населения, и
  он живёт слоем ниже. Файл, не тянущий за собой ни холста, ни рига, едет
  туда переносом пути.
- Used by: engine/app CharGen.cpp / AppCharGen.cpp, tests/app/PeoplesTests.cpp.

Notes:
- ПОЧЕМУ РАСПРЕДЕЛЕНИЕ, А НЕ ПРЕСЕТ (docs/design/CHARGEN_UI.md, Р8). Пресет
  народа точкой дал бы четыре одинаковых тела на четыре пояса и толпу
  близнецов. Народ несёт для каждой ручки КРАЯ, типаж — ЦЕНТР внутри них, и
  один и тот же файл служит обоим потребителям: игроку он даёт стартовую
  точку и сужает нарисованную дорожку, толпе — выборку.
- ОДНА СКРЫТАЯ СВЯЗЬ: «КРУПНОСТЬ». Ручки, брошенные независимо, дают
  коротышек с руками до колен. Один стандартный отсчёт бросается на тело и
  входит с весом PEOPLE_BULK_RHO в рост, плечи и длину рук; собственный
  отсчёт ручки входит с sqrt(1−ρ²). Так СОХРАНЯЕТСЯ КРАЕВОЕ РАСПРЕДЕЛЕНИЕ
  каждой ручки (сумма даёт ровно N(0,1)), то есть связь не портит ни один
  прибор, который меряет ручку поодиночке.
- НАРОДНЫЙ КРАЙ УЖЕ СУДЕЙСКОГО, И СПОР РЕШЁН В ПОЛЬЗУ СУДЬИ. Лоровед назвал
  венедам рост 150-200 см, судья мерил канон пропорций и дал 1.66-1.84.
  Данные несут усечение и запись о нём; читатель отказывает тому, кто попробует
  расширить полосу файлом.
- ПОЧЕМУ СВОЙ ГЕНЕРАТОР СЛУЧАЙНЫХ. `std::mt19937` одинаков везде, а вот
  `std::normal_distribution` — нет: стандарт не задаёт его алгоритм, и два
  прогона на двух библиотеках разошлись бы кадром при одном зерне (правило 13).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- ВИДИМЫЕ СЛОВА — КЛЮЧИ ЛОКАЛИЗАЦИИ (правило 5): народ и типаж несут
  `name_key`, а не подпись. Исключение ровно одно и оно же в чате и в имени
  игрока: СОБСТВЕННОЕ ИМЯ печатается дословно — «Ждан Кожемяка» не переводят.
- Числа полос и центров — СОДЕРЖИМОЕ ФАЙЛА. Ни одного из них здесь быть не
  должно: новый народ — это файл, а не правка кода (правило 6).
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dfn::app {

/// ГДЕ ЛЕЖАТ НАРОДЫ. Рядом с пресетами игрока, потому что это один и тот же
/// предмет с двух сторон: народ — распределение, пресет — его выборка.
inline constexpr const char* PEOPLES_DIR = "assets/characters/peoples";
inline constexpr const char* PEOPLES_EXT = ".people";

/// ДОЛЯ ПОЛОСЫ, КОТОРУЮ ЗАНИМАЕТ РАЗБРОС, если типаж не назвал свой. Четверть
/// народной полосы — число из дизайна (Р8, §4): две трети выборки садятся в
/// середину народа, а хвосты доходят до краёв.
inline constexpr float PEOPLE_SIGMA_FRAC = 0.25f;

/// ВЕС СКРЫТОЙ «КРУПНОСТИ» в росте, плечах и длине рук. Не подгонка: 0.7
/// значит, что половина дисперсии этих трёх ручек общая, то есть высокий
/// человек скорее широк в плечах и длиннорук, — и при этом краевое
/// распределение каждой из них остаётся ровно тем, что задал типаж.
inline constexpr float PEOPLE_BULK_RHO = 0.7f;

/// КАКИЕ ИМЕННО РУЧКИ ДЕРЖИТ «КРУПНОСТЬ». Имена целей, а не номера: номер
/// цели меняется при первой же новой строке в секции MORF.
inline constexpr const char* PEOPLE_BULK_KNOBS[] = {"stature", "shoulders",
                                                    "arm-length"};

/// ИМЯ РУЧКИ РОСТА В ФАЙЛЕ НАРОДА. То же слово, что в пресете и в дозе
/// DFN_MORPH (правило 32); здесь оно названо своим, потому что этот файл не
/// вправе включать CharGenBody.h — тот тянет за собой видеокарту.
inline constexpr const char* PEOPLE_STATURE_KNOB = "stature";

/// СКОЛЬКО РАЗ УСЕЧЁННАЯ НОРМАЛЬ ПЕРЕБРАСЫВАЕТ, прежде чем зажать. Отказ от
/// броска — честная усечённая нормаль; зажим на шестнадцатом — страховка от
/// вечного цикла у полосы, которую кто-то сделал уже сигмы.
inline constexpr int PEOPLE_TRUNCATION_TRIES = 16;

// --- ЧТО ЛЕЖИТ В ФАЙЛЕ ------------------------------------------------------

/// КРАЯ ОДНОЙ РУЧКИ У НАРОДА.
struct PeopleBand {
    std::string name;
    float lo = 0.0f;
    float hi = 0.0f;
};

/// ЦЕНТР И РАЗБРОС ОДНОЙ РУЧКИ У ТИПАЖА. Названа только та ручка, о которой
/// лор сказал словами; про остальные типаж МОЛЧИТ, и молчание значит «центр
/// народа», а не «ноль».
struct PeopleTrait {
    std::string name;
    float mu = 0.0f;
    float sigma = 0.0f;
};

struct PeopleArchetype {
    std::string id;
    std::string name_key;
    float frequency = 0.0f;  ///< вес выборки, проценты
    std::vector<PeopleTrait> traits;  ///< по имени ручки
};

/// ОДИН ОТТЕНОК ВЗВЕШЕННОГО СПИСКА (кожа, волосы, глаза, слои).
struct PeopleSwatch {
    std::string id;
    float weight = 0.0f;
};

/// ОБРЯДОВАЯ ОТМЕТИНА. `archetypes` пуст — значит у всего народа.
struct PeopleMark {
    std::string id;
    float chance = 0.0f;  ///< проценты; 0 значит «частоту назначает не народ»
    std::string place;
    std::vector<std::string> archetypes;
};

/// ПРАВИЛО ОБРАЗОВАНИЯ ИМЕНИ. У скельдов родовое имя может быть НЕ ИЗ СПИСКА,
/// а построено из мужского имени и суффикса — это и есть патроним.
struct PeopleNaming {
    std::string rule_key;
    std::string patronym_male;
    std::string patronym_female;
    float patronym_share = 0.0f;  ///< проценты родовых, которые строятся, а не берутся
};

/// ПОЛ. В первом выпуске базовый меш ОДИН, и пол честно меняет только имя:
/// второй пол — это второй меш, перенос всех целей заново и перемер всех
/// полос (CHARGEN_UI.md, Р9), то есть волна, а не поле здесь.
enum class PeopleSex : std::uint8_t { Male, Female };

struct People {
    std::string id;
    std::string name_key;
    std::string blurb_key;
    int order = 0;  ///< порядок на экране; каталог отдаёт народы по нему

    std::vector<PeopleBand> limits;  ///< по имени ручки
    std::vector<PeopleArchetype> archetypes;
    PeopleNaming naming;
    std::vector<std::string> male;
    std::vector<std::string> female;
    std::vector<std::string> family;
    std::vector<PeopleSwatch> skin;
    std::vector<PeopleSwatch> hair;
    std::vector<PeopleSwatch> eyes;
    std::vector<PeopleSwatch> layers;
    std::vector<PeopleMark> marks;

    [[nodiscard]] const PeopleBand* band(std::string_view knob) const;
    [[nodiscard]] const PeopleArchetype* archetype(std::string_view id) const;
    /// Номер типажа или archetypes.size(), если такого нет.
    [[nodiscard]] std::size_t archetype_index(std::string_view id) const;
    /// ЦЕНТР ТИПАЖА ПО РУЧКЕ — то, что видит игрок как стартовую точку. Если
    /// типаж про ручку молчит, это середина народной полосы.
    [[nodiscard]] float centre_of(std::size_t archetype, std::string_view knob) const;
};

// --- ЧТЕНИЕ -----------------------------------------------------------------

/// ОДИН НАРОД ИЗ ОДНОГО ФАЙЛА. False и причина словами: файл народа читается
/// на входе в экран, и «народов нет» игроку надо объяснить, а не показать
/// пустой список.
[[nodiscard]] bool read_people(const std::filesystem::path& in, People& out,
                               std::string& why);

/// ВСЕ НАРОДЫ КАТАЛОГА, отсортированные по `order`. Каталога нет или он пуст —
/// пустой ответ и жалоба в поток ошибок: это законное состояние дерева без
/// ассетов, а не отказ.
[[nodiscard]] std::vector<People> read_peoples(const std::filesystem::path& dir);

/// НАРОДНЫЙ КРАЙ ВНУТРИ СУДЕЙСКОГО, ЧАСТОТЫ В СУММЕ СТО, СПИСКИ НЕ ПУСТЫ.
/// `canon` — полосы, измеренные приёмкой (секция MORF плюс рост). Ручку, о
/// которой судья не знает, народ называть не вправе: это опечатка в имени
/// цели, и молчаливо принятая, она даёт ползунок, который никогда не двинется.
[[nodiscard]] bool peoples_validate(const People& people,
                                    const std::vector<PeopleBand>& canon,
                                    std::string& why);

// --- ВЫБОРКА ----------------------------------------------------------------

/// ЗЕРНО. Один uint64 и splitmix64: тот же рецепт с тем же зерном обязан
/// давать те же тела на любой стандартной библиотеке (правило 13), а
/// `std::normal_distribution` этого не обещает.
struct PeopleRng {
    std::uint64_t state = 0;
};

/// Равномерное в [0, 1).
[[nodiscard]] float people_uniform(PeopleRng& rng);
/// Стандартная нормаль. Бокс-Мюллер, оба хвоста, ни одной таблицы.
[[nodiscard]] float people_normal(PeopleRng& rng);

/// ОДИН ЧЕЛОВЕК: типаж, все ручки, имя, оттенки.
struct PeopleDraw {
    const People* people = nullptr;
    std::size_t archetype = 0;
    PeopleSex sex = PeopleSex::Male;
    /// Ручки ПО ИМЕНИ И ПО АЛФАВИТУ — тот же порядок, в котором их складывает
    /// бленд и пишет пресет: сложение float не ассоциативно, и порядок здесь
    /// это условие побайтовой воспроизводимости, а не аккуратность.
    std::vector<std::pair<std::string, float>> sliders;
    std::string name;
    std::string skin;
    std::string hair;
    std::string eyes;
};

/// ДОЛЯ ОДНОГО НАРОДА В СМЕСИ. Пограничья — не украшение лора, а вход
/// генератора населения: в столице и портах 10-15 % соседей, в Курганлы в
/// сезон до половины кайсаков. Экрану создания смесь не нужна, и он её не
/// зовёт; заложена она здесь, потому что переделывать вход генератора потом
/// дороже, чем принять его таким сразу.
struct PeopleMixEntry {
    const People* people = nullptr;
    float weight = 0.0f;
};
using PeopleMix = std::vector<PeopleMixEntry>;

/// ТИПАЖ ПО ЧАСТОТАМ.
[[nodiscard]] std::size_t people_pick_archetype(const People& people, PeopleRng& rng);
/// ТЕЛОСЛОЖЕНИЕ: усечённая нормаль вокруг центра типажа, в народных краях,
/// со скрытой «крупностью». Ручки по алфавиту.
[[nodiscard]] std::vector<std::pair<std::string, float>> people_sample_build(
    const People& people, std::size_t archetype, PeopleRng& rng);
/// ИМЯ ПО ПРАВИЛУ НАРОДА.
[[nodiscard]] std::string people_sample_name(const People& people, PeopleSex sex,
                                             PeopleRng& rng);
/// ОТТЕНОК ИЗ ВЗВЕШЕННОГО СПИСКА; пустой список — пустая строка.
[[nodiscard]] std::string people_pick_swatch(const std::vector<PeopleSwatch>& list,
                                             PeopleRng& rng);

/// ОДИН БРОСОК ОДНОГО НАРОДА.
[[nodiscard]] PeopleDraw people_sample(const People& people, PeopleSex sex,
                                       PeopleRng& rng);
/// ОДИН БРОСОК СМЕСИ: сперва народ по весам, дальше всё то же.
[[nodiscard]] PeopleDraw people_sample(const PeopleMix& mix, PeopleSex sex,
                                       PeopleRng& rng);

} // namespace dfn::app
