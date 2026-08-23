/*
Created: 23:08:2026 - 20:48:17
Last updated: 24:08:2026 - 01:25:00
Module: tools
File: tools/house_manifest.h

Responsibility:
- МАНИФЕСТ ПОЛКИ ГОТОВЫХ ПОСТРОЕК (assets/houses/INDEX.txt). Обе кузницы —
  dfn_houses и dfn_furniture — после выпечки зовут write_house_manifest, и та
  ЗАМЕРЯЕТ каждый .dfh на полке: габарит графа, габарит построенного меша,
  верх тела, пятно стен, дверь, балки перекрытия, верх половой плиты.

Key items:
- write_house_manifest: обходит полку, пишет INDEX.txt.
- recipe_family: пакет рецепта — префикс имени до первого дефиса.
- ManifestRow: одна замеренная строка (для проверок из тестов).

Dependencies:
- Uses: engine/world (HouseFile, HouseGraph, HouseMesh).
- Used by: tools/forge_houses.cpp, tools/forge_furniture.cpp; читает
  генератор города (tools/gen_city.py) вместо собственного разбора .dfh.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЗАМЕР, А НЕ ТАБЛИЦА. Числа в манифесте берутся с самих файлов полки каждым
  прогоном кузницы. Любая цифра габарита, выписанная руками во второе место,
  — будущая ложь: полку перепекают каждую волну, а таблицу поправить забудут.
  Полка уже платила за это (city-house-s значился 7x7 при факте 4.5x6.0).
- ОДИН РАЗБОРЩИК ФОРМАТА. Манифест существует ровно затем, чтобы .dfh читал
  только C++ (read_house). Появится второй разборщик на другом языке — они
  разойдутся, и разойдутся молча.
- ЧИСЛА КВАНТУЮТСЯ ДО ТОЧНОСТИ ФАЙЛА (DFH_DECIMALS). write_house печатает
  координаты как %.4f; float, прочитанный обратно, отстоит от этого десятичного
  числа на 1e-8, и напечатанный как %.9g он дал бы 0.351000011 там, где в файле
  стоит 0.3510. Читатель манифеста обязан получать РОВНО то, что стоит в .dfh,
  иначе манифест и файл — два разных источника правды.

ГРАНИЦА МАНИФЕСТА (названа вслух, чтобы «единственный источник» не звучал
шире, чем он есть):
- ЗДЕСЬ ТОЛЬКО ГАБАРИТ И ПОСАДКА, а не внутренняя геометрия детали. Два
  читателя .dfh в генераторе города пережили волну И2 сознательно:
  arc_kit() (хорда и поворот собственной оси гнутого прогона — ломаная стенок
  height=6.00) и bridge_deck() (профиль настила, шесть наклонных плит). Из
  коробки ни то, ни другое не выводится, и подставить туда bbox было бы той же
  подменой, от которой манифест разводит g* (пятно) и m* (объём со свесами).
- ЕСЛИ ЭТИ ЧИСЛА ПОНАДОБЯТСЯ — ИМ МЕСТО В РЕЕСТРЕ КУЗНИЦЫ, А НЕ ЗДЕСЬ. Кузница
  знает поворот куска В МОМЕНТ ВЫПЕЧКИ (forge_wall_arc печатает R, поворот и
  длину оси прямо в журнал); обратный замер по файлу восстанавливал бы то, что
  и так было в руках. Замер и паспорт в одной строке смешивать не стоит.
  Требования снимающей стороны (волна И1, agent epoch-i1), чтобы их не
  восстанавливали с нуля: на кусок стены — chord, turn, arc; на мост — профиль
  настила списком пар (x, y).
КАК ЧИТАТЬ door_axis (замер cornhall2-forge, 24.08, 166 строк полки):
- ОСЬ, НЕ СОВПАВШАЯ С РУКОЙ РЕЦЕПТА, — ПОВОД МЕРИТЬ g-ГАБАРИТ, А НЕ МЕНЯТЬ
  ЭВРИСТИКУ. Ось берётся по БОЛЬШЕМУ смещению двери от центра g-габарита, и
  дважды показалось, что она врёт (ратуша, весовая палата). Оба раза врал не
  замер, а тело: ряд столбов начинался с назначенного x вместо выведенного из
  длины, вылезал за угол на 1.8 и 0.2 м, габарит растягивался, центр уезжал —
  и колонка честно об этом сообщила. Показание прибора приняли за поломку
  прибора.
- ПРЕДЛАГАВШАЯСЯ ЗАМЕНА («ось по БЛИЖАЙШЕЙ грани габарита») ОТВЕРГНУТА ЗАМЕРОМ,
  а не приёмкой. Дверь есть у 35 рецептов из 166; две эвристики расходятся
  ровно на ОДНОМ — cornhall-windmill, — и там ближайшая грань даёт -X при
  верном +Z: габарит растянут КРЫЛЬЯМИ на пять метров по X, и до западной
  грани от двери ближе, чем до южной стены, в которой она стоит. То есть
  лекарство ломается ровно на том классе, ради которого предлагалось, —
  на постройке с далеко вылетающей частью (крылья, эркер, портик), — а
  нынешняя эвристика на нём устояла.
- ЗНАК turn ЗНАЧИМ ОТНОСИТЕЛЬНО ОБХОДА КОЛЬЦА, А НЕ САМ ПО СЕБЕ. У чертежа
  Вайтрана знаковая площадь -7417 и все 14 изломов отрицательные — и это тот
  порядок вершин, при котором кладка смотрит бугром НАРУЖУ. У города с обходом
  в другую сторону тот же рецепт встанет лицом внутрь. Поэтому кузница обязана
  писать поворот В СВОЕЙ системе (бугор в локальный +Z), а разворот выводит
  генератор — как выводит и сейчас.
*/
/*
UPD:
- 23:08:2026 - 20:48:17: Создан — волна И2 эпохи «12 городов»: манифест полки
  вместо python-разборщика .dfh в генераторе города. Формат согласован с
  волной И1 (agent epoch-i1): колоночный, с объявлением колонок в шапке,
  разбор dict(zip(cols, line.split())), числа %.9g.
- 23:08:2026 - 21:25:00: ГРАНИЦА МАНИФЕСТА записана в шапке — что он НЕ несёт
  (внутреннюю геометрию детали), какие два читателя .dfh пережили волну
  сознательно (arc_kit, bridge_deck) и почему таким числам место в реестре
  кузницы, а не в замере. Знание жило в переписке двух волн, а переписку
  следующая волна не прочтёт; знак turn относительно обхода кольца — оттуда же.
  Правка только в комментарии: полка и INDEX.txt перепеклись байт-в-байт.
- 23:08:2026 - 21:34:00: НАЗВАНА ЦЕНА sort ПОСЛЕ directory_iterator. Строка
  стояла голой, а генератор города обходит семейства выпечки В ПОРЯДКЕ СТРОК
  МАНИФЕСТА, и при равной пригодности из этого порядка выбирается рецепт: её
  снятие сдвинуло бы посадку марша террасы, а не оформление файла. Замечание
  волны И1 — у неё этот обход в цепочке, и она о нём знала, а кузница нет.
  Голая строка с далёким следствием — приглашение её «прибрать».
  Правка только в комментарии: INDEX.txt не изменился ни на байт.
- 24:08:2026 - 19:00:00: КОЛОНКА face_off — ВЫНОС ЛИЦА ЗА ДВЕРНУЮ ТОЧКУ
  (волна Г эпохи, находка пилота Корнхолла №9). Пилот держал таблицу этих
  чисел РУКАМИ в чертеже своего города: генератор сажает дом ДВЕРНОЙ гранью,
  а на красной линии сплошного фасадного ряда стоит ЛИЦО, и разница у каждого
  рецепта своя — 0.77 м у рядового дома, 6.90 у купеческой усадьбы. Замер
  детали, переписанный в композицию, — ровно тот класс числа, ради которого
  манифест и заводили: полку перепекают каждую волну, а таблицу поправить
  забудут. Число выводится из g-габарита и точки двери (полуглубина минус
  door_off), но выводить его НА МЕСТЕ значило бы завести второй замер той же
  детали. Колонка дописана В КОНЕЦ поимённого блока двери, перед beams: у
  читателя разбор по именам колонок, но порядок «дверные поля рядом» — часть
  читаемости файла глазами. Полка перепеклась байт-в-байт (149 рецептов), в
  INDEX.txt изменилась ровно одна колонка.
- 24:08:2026 - 23:10:00: В ШАПКУ ЗАПИСАНО, КАК ЧИТАТЬ door_axis. Замер
  cornhall2-forge по всем 166 строкам полки: дверь есть у 35 рецептов, а
  предлагавшаяся замена эвристики («ось по ближайшей грани габарита» вместо
  нынешней «по большему смещению от центра») расходится с ней ровно на ОДНОМ
  и в худшую сторону — на ветряке, у которого габарит растянут крыльями.
  Замечание отозвано автором; в бэклог оно не пошло, потому что задачи нет.
  Осталось ЧТЕНИЕ: оба случая «неверной оси» были раздутым телом (столбы за
  углом на 1.8 и 0.2 м), и колонка сработала прибором. Правка только в
  комментарии: полка и INDEX.txt перепеклись байт-в-байт.
- 24:08:2026 - 00:37:57: ось двери — правило чтения: несогласие = «измерь тело», оба исхода законны; windmill расходится по паспорту всегда (просьба cities-style, замер cornhall2-forge).
- 24:08:2026 - 00:38:53: снят мой дубль блока об оси двери (писался параллельно с 1bfe295 и разорвал фразу заголовка границы) — остаётся полный блок «КАК ЧИТАТЬ door_axis» Г-волны.
- 24:08:2026 - 00:41:00: СКЛЕЕНА РАЗОРВАННАЯ ЗАПИСЬ UPD (моя, 23:10:00):
  две записи соседа легли ВНУТРЬ неё — параллельная запись в общий файл,
  та же цена, что у общего индекса в правиле 29. Ни одна запись не
  удалена (правило 17): обе перенесены целиком и стоят ниже, в порядке
  своего времени. Правка только в комментарии.
- 24:08:2026 - 01:05:00: ИСПРАВЛЕНИЕ К ЗАПИСИ ВЫШЕ — И ПРИЧИНА, И АВТОР ТАМ
  НАЗВАНЫ НЕВЕРНО (запись не удаляю, правило 17; поправка идёт отдельной).
  АВТОР. «Две записи соседа» — не соседа: cornhall2-forge этот файл не
  открывал ни разу, `git show --name-only` по обоим его коммитам (60201be,
  a4fe027) даёт только его 16 .dfh, INDEX.txt, forge_houses.cpp и
  pack_cornhall.h. Записи 00:37:57 и 00:38:53 пришли из 176833d — это main,
  положивший строку по просьбе cities-style и сославшийся на его замер.
  Различить это по коммитам НЕЛЬЗЯ: все агенты пишут под одним git-автором,
  так что авторство не сообщает ничего, а единственная зацепка — текст самой
  записи («просьба cities-style, замер cornhall2-forge»). Вывод «раз замер
  его, то и запись его» был доступный, но неверный, и цена ему — ложное
  обвинение в общем файле.
  ПРИЧИНА. Не «параллельная запись в общий файл» (это следствие), а ДВОЙНАЯ
  МАРШРУТИЗАЦИЯ ОДНОЙ НАХОДКИ: кузница послала отзыв своего замечания ДВУМ
  адресатам — сюда и cities-style, — и ни в одном письме не назвала, КТО
  кладёт строку. cities-style репозитория эпохи не правит и передала дальше
  main; вторая рука, получив ту же находку напрямую, положила её сама.
  Коммиты 1bfe295 (00:37:39) и 176833d (00:38:04): двадцать пять секунд.
  ЧЕМ ЭТО НЕ ЛОВИТСЯ: ни блокировкой файла, ни порядком коммитов, ни
  аккуратностью — обе руки делали правильную работу по правильной просьбе.
  Ловится ОДНОЙ ФРАЗОЙ НА ВХОДЕ: если находка идёт больше чем одному, в том
  же письме назван тот, кто её кладёт.
  И ЭТО ТРЕТИЙ ЭТАЖ ОДНОЙ ЛЕСТНИЦЫ (формулировка кузницы, и она точная):
  правило 29 — про общий ИНДЕКС, запись 00:41:00 — про общий ФАЙЛ, эта — про
  общего АДРЕСАТА. Разделяемый ресурс, о котором никто не думает как о
  ресурсе, — это распределение работы между агентами.
- 24:08:2026 - 01:25:00: КОЛОНКА wpoly — КОНТУР ПЯТНА СТЕН (правка по
  претензии владельца 24.08: «много домов, у которых фундамент СИЛЬНО ШИРЕ
  самого дома»).
  ЧТО ОКАЗАЛОСЬ. Прямоугольник w* честно отвечает на свой вопрос — «сколько
  места занять», — и генератор города правильно берёт его для посадки, пада и
  приёмки утопленности. Но ЛЕНТЕ ФУНДАМЕНТА он отвечал на другой вопрос, «где
  стоит стена», и врал тем сильнее, чем сложнее план. Замер по всей полке:
  city-house-l 10.00x8.00 стен против 10.00x10.35 охвата (+2.35 — два столба
  крыльца r=0.10 и марш, попавшие в нижний венец), city-temple +2.20
  (паперть), city-shop +2.00 (навес), city-mill +3.80 (ось водяного колеса),
  city-barn +1.50 (въездная аппарель), city-keep-s — зал 15x8 и два крыла 5x6
  внутри охвата 25.0x13.2, то есть лента обводила ВОЗДУХ МЕЖДУ КРЫЛЬЯМИ.
  ПОЧЕМУ ЗАМЕР ЗДЕСЬ, А НЕ В ГЕНЕРАТОРЕ. Кольца выводятся из графа — из
  цепочек-стен нижнего венца, — а граф разбирает только read_house. Второй
  разборщик .dfh в питоне и был бы ровно тем, о чём предупреждает шапка:
  разошлись бы молча. Обход граней плоского графа (в каждой вершине первый
  поворот по часовой) выбран не из любви к алгоритму, а по факту полки: у
  усадьбы и у ратуши в одной вершине сходятся ТРИ стены (к корпусу примыкает
  дворовая ограда), и наивная цепочка уходит в ограду, теряя корпус. Он же
  даром отдаёт РАЗБИЕНИЕ ПО КОРПУСАМ: у city-keep девять колец (зал, два
  крыла, шесть башен), у cornhall-inn два, у усадьбы одно из десяти вершин.
  ДВЕ ВЕТКИ, БЕЗ КОТОРЫХ КОЛЕЦ НЕ БЫЛО БЫ У ШЕСТИ РЕЦЕПТОВ. Первая:
  ЗАМКНУТАЯ ПАНЕЛЬ, чья проекция на план вырождается в отрезок, — это стена
  (щипцовые торцы амбара построены контуром, а не цепочкой). Вторая:
  РАЗОМКНУТАЯ ЦЕПЬ С ДВУМЯ КОНЦАМИ ЗАМЫКАЕТСЯ (аркада ратуши и устье печи —
  стены с проёмом во всю сторону; фундамент под аркадой есть, кольца не было).
  Без колец остаются три рецепта, и это честно: cornhall-granary на сваях,
  city-stall на ножках, city-gate — проём. Читателю там остаётся w*.
  Полка перепеклась БАЙТ-В-БАЙТ (163 прежних .dfh, shasum сошёлся), в
  INDEX.txt добавилась одна колонка в конец.
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseMesh.h"

namespace dfn::forge {

/// Знаков после запятой у координаты в .dfh (write_house печатает %.4f).
inline constexpr double DFH_DECIMALS = 1e4;

/// Координата, приведённая к тому десятичному числу, которое СТОИТ В ФАЙЛЕ.
/// См. «ЧИСЛА КВАНТУЮТСЯ» в шапке: без этого манифест и .dfh расходятся в
/// восьмом знаке, а генератор города считает по ним посадку дома.
[[nodiscard]] inline double q(float v) {
    return std::round(static_cast<double>(v) * DFH_DECIMALS) / DFH_DECIMALS;
}

/// ПАКЕТ РЕЦЕПТА — ПРЕФИКС ЕГО ИМЕНИ до первого дефиса: city-house-s -> city,
/// furn-lamp-post -> furn. Правило выбрано так, чтобы городской пакет
/// будущего города (solitude-*.dfh) получил своё семейство САМ, без правки
/// здесь: любая таблица «имя -> пакет» на двенадцать городов протухнет.
///
/// Пять повторов демки старше правила и дефис у них значит другое
/// (log-replica — не пакет «log»). Их пакет назван явно и списком, потому что
/// список закрыт: новых имён без префикса на полке не заводят.
[[nodiscard]] inline std::string recipe_family(const std::string& name) {
    static const std::map<std::string, std::string> legacy = {
        {"log-replica", "demo"},   {"frame-replica", "demo"},
        {"stone-replica", "demo"}, {"l-house", "demo"},
        {"u-house", "demo"},
    };
    const auto it = legacy.find(name);
    if (it != legacy.end()) {
        return it->second;
    }
    const std::size_t dash = name.find('-');
    return dash == std::string::npos ? name : name.substr(0, dash);
}

/// Замер одной постройки. Поля названы так же, как колонки INDEX.txt.
struct ManifestRow {
    std::string name;
    std::string file;
    std::string family;
    std::size_t elems = 0;
    std::size_t verts = 0;
    std::size_t tris = 0;
    /// Габарит по вершинам ГРАФА (пятно застройки без свеса кровли по x/z —
    /// то, из чего генератор берёт центр и размер посадочного места).
    double g_lo[3] = {0.0, 0.0, 0.0};
    double g_hi[3] = {0.0, 0.0, 0.0};
    /// Габарит по вершинам ПОСТРОЕННОГО МЕША — истинный занимаемый объём с
    /// толщиной стен, свесом кровли и скатами.
    double m_lo[3] = {0.0, 0.0, 0.0};
    double m_hi[3] = {0.0, 0.0, 0.0};
    /// Верх тела С УЧЁТОМ height у поверхностей: у стенки верх лежит не на
    /// вершине, а на «вершина + height».
    double top = 0.0;
    /// Пятно СТЕН: прямоугольник вершин нижнего венца (y == 0).
    double w_lo[2] = {0.0, 0.0};
    double w_hi[2] = {0.0, 0.0};
    bool has_door = false;
    std::string door_axis = "-";
    double door_off = 0.0;
    double door_pt[2] = {0.0, 0.0};
    /// ВЫНОС ЛИЦА ЗА ДВЕРНУЮ ТОЧКУ: сколько метров тела стоит ВПЕРЕДИ той
    /// точки, куда генератор ставит дверь — от двери до наружной грани
    /// g-габарита по оси двери. Это и есть разница между «дверь на красной
    /// линии» и «ФАСАД на красной линии», и она у каждого рецепта своя: у
    /// city-house-s 0.77 м, у city-manor 6.90. Посадка сплошного фасадного
    /// ряда без этого числа выносит стену усадьбы на семь метров в проезжую
    /// часть; пилот Корнхолла держал таблицу этих чисел РУКАМИ в чертеже
    /// города, то есть замер детали жил у композиции. Арифметически это
    /// «полуглубина габарита минус door_off», но выводить его на месте значит
    /// заводить второй замер той же детали — а манифест затем и есть, чтобы
    /// замер был один.
    double face_off = 0.0;
    /// КОНТУР ПЯТНА СТЕН — замкнутые кольца наземной кладки, по одному на
    /// КОРПУС постройки. Прямоугольник w_lo/w_hi отвечает на вопрос «сколько
    /// места занять», и для посадки этого хватает; ЛЕНТЕ ФУНДАМЕНТА нужен
    /// другой ответ — «где именно стоит стена», и прямоугольник тут врёт тем
    /// сильнее, чем сложнее план. У Г-образной усадьбы он обводит дворик
    /// снаружи стен (154 кв.м пятна против 210 габаритных), у замка с
    /// крыльями — воздух между крыльями, у мельницы — ось водяного колеса,
    /// у половины полки — крыльцо и вылет кровли, попавшие в нижний венец
    /// одной вершиной. Отсюда и берётся «фундамент сильно шире дома».
    /// Кольцо обходится ПРОТИВ ЧАСОВОЙ (площадь по формуле шнурков > 0), то
    /// есть наружная нормаль стороны (a -> b) равна (dz, -dx)/|ab|.
    std::vector<std::vector<std::array<double, 2>>> wpoly;
    /// Низ каждой несомой балки (surface beams=1), по возрастанию.
    std::vector<double> beams;
    bool has_floor = false;
    double floor = 0.0;
};

namespace detail {

/// Значение параметра элемента; nullptr — параметра нет.
[[nodiscard]] inline const std::string* param(const dfn::world::Element& e,
                                              const char* key) {
    for (const auto& kv : e.params) {
        if (kv.first == key) {
            return &kv.second;
        }
    }
    return nullptr;
}

[[nodiscard]] inline double param_num(const dfn::world::Element& e, const char* key) {
    const std::string* s = param(e, key);
    return s == nullptr ? 0.0 : std::strtod(s->c_str(), nullptr);
}

[[nodiscard]] inline bool flag(const dfn::world::Element& e, const char* key) {
    const std::string* s = param(e, key);
    return s != nullptr && *s == "1";
}

/// Число для манифеста: девять значащих. Просьба волны И1 — при %.3f разрыв
/// в 5e-4 на выносе двери меняет, на какой итерации сработает разведение тел
/// в раскладке, и дом уезжает на 0.4 м.
inline std::string fnum(double v) {
    char buf[40];
    // -0 печатается как «-0» и сбивает глаз на нулевой кромке.
    std::snprintf(buf, sizeof(buf), "%.9g", v == 0.0 ? 0.0 : v);
    return buf;
}

/// НАЗЕМНЫЙ ОТРЕЗОК КЛАДКИ в плане: (x,z) начала и конца.
using PlanSeg = std::array<double, 4>;

/// ОТРЕЗКИ НАЗЕМНОЙ КЛАДКИ рецепта. Стеной считается ровно то, что стоит НА
/// ЗЕМЛЕ и несёт толщину:
///  - цепочка (surface из двух вершин) с height > 0, обе вершины на y == 0;
///  - ЗАМКНУТАЯ панель, чья проекция на план ВЫРОЖДАЕТСЯ В ОТРЕЗОК и чей низ
///    лежит на земле — так собраны щипцовые торцы амбара: снаружи это стена,
///    а в графе — контур, и без этой ветки у амбара кольца нет вовсе.
/// Створка двери (door=1) и дощатая мелочь тоньше 0.15 отброшены: под ними
/// фундамента не бывает, а в нижний венец они попадают наравне со стеной.
[[nodiscard]] inline std::vector<PlanSeg> ground_wall_segs(
        const dfn::world::HouseGraph& g,
        const std::map<dfn::world::VertexId, std::array<double, 3>>& pos) {
    using namespace dfn::world;
    std::vector<PlanSeg> out;
    for (const Element& e : g.elements()) {
        if (e.kind != ElementKind::Surface || flag(e, "door")) {
            continue;
        }
        if (param_num(e, "thickness") < 0.15) {
            continue;
        }
        std::vector<const std::array<double, 3>*> refs;
        for (const VertexId id : e.refs) {
            const auto it = pos.find(id);
            if (it != pos.end()) {
                refs.push_back(&it->second);
            }
        }
        if (refs.size() < 2) {
            continue;
        }
        if (e.closed) {
            double ylo = std::numeric_limits<double>::infinity();
            double xlo = ylo;
            double zlo = ylo;
            double xhi = -ylo;
            double zhi = -ylo;
            for (const auto* p : refs) {
                ylo = std::min(ylo, (*p)[1]);
                xlo = std::min(xlo, (*p)[0]);
                xhi = std::max(xhi, (*p)[0]);
                zlo = std::min(zlo, (*p)[2]);
                zhi = std::max(zhi, (*p)[2]);
            }
            const double dx = xhi - xlo;
            const double dz = zhi - zlo;
            if (ylo > 1e-6 || std::min(dx, dz) > 1e-4 || std::max(dx, dz) < 0.1) {
                continue;
            }
            out.push_back({xlo, zlo, xhi, zhi});
            continue;
        }
        if (refs.size() != 2 || param_num(e, "height") <= 0.01) {
            continue;
        }
        if (std::fabs((*refs[0])[1]) > 1e-6 || std::fabs((*refs[1])[1]) > 1e-6) {
            continue;
        }
        out.push_back({(*refs[0])[0], (*refs[0])[2], (*refs[1])[0], (*refs[1])[2]});
    }
    return out;
}

/// ЗАМКНУТЫЕ КОЛЬЦА из отрезков кладки — обходом граней плоского графа.
///
/// ПОЧЕМУ ОБХОД ГРАНЕЙ, А НЕ «СОБРАТЬ ЦЕПОЧКУ ПО СОСЕДЯМ». У ратуши и у
/// усадьбы в одной вершине сходятся ТРИ стены (к корпусу примыкает дворовая
/// ограда), и наивный обход уходит в ограду, теряя корпус. Обход граней
/// (в каждой вершине берётся первый поворот ПО ЧАСОВОЙ от входящего ребра)
/// разбирает такой узел правильно и заодно отдаёт КАЖДЫЙ корпус своим
/// кольцом: у замка их девять — зал, два крыла и шесть башен.
///
/// РАЗОМКНУТАЯ ЦЕПЬ ЗАМЫКАЕТСЯ. Ратушная аркада и устье обжиговой печи —
/// стены с проёмом во всю сторону: концов у цепи два, кольца нет, а
/// фундамент под аркадой есть. Если у связной части ровно два конца, они
/// соединяются; если больше — часть остаётся без кольца и рецепт уходит на
/// прямоугольник (амбар на сваях, рыночный лоток).
[[nodiscard]] inline std::vector<std::vector<std::array<double, 2>>> wall_rings(
        std::vector<PlanSeg> segs) {
    using Pt = std::array<double, 2>;
    const auto key = [](double x, double z) {
        return std::make_pair(static_cast<long long>(std::llround(x * 10000.0)),
                              static_cast<long long>(std::llround(z * 10000.0)));
    };
    std::map<std::pair<long long, long long>, Pt> node;
    std::map<std::pair<long long, long long>, std::set<std::pair<long long, long long>>> adj;
    for (const PlanSeg& s : segs) {
        const auto a = key(s[0], s[1]);
        const auto b = key(s[2], s[3]);
        if (a == b) {
            continue;
        }
        node[a] = {s[0], s[1]};
        node[b] = {s[2], s[3]};
        adj[a].insert(b);
        adj[b].insert(a);
    }
    // Замыкание разомкнутых цепей: по связной части, и только если концов два.
    std::set<std::pair<long long, long long>> seen;
    for (const auto& kv : adj) {
        if (seen.count(kv.first) != 0) {
            continue;
        }
        std::vector<std::pair<long long, long long>> comp;
        std::vector<std::pair<long long, long long>> st{kv.first};
        seen.insert(kv.first);
        while (!st.empty()) {
            const auto v = st.back();
            st.pop_back();
            comp.push_back(v);
            for (const auto& w : adj[v]) {
                if (seen.insert(w).second) {
                    st.push_back(w);
                }
            }
        }
        std::vector<std::pair<long long, long long>> ends;
        for (const auto& v : comp) {
            if (adj[v].size() == 1) {
                ends.push_back(v);
            }
        }
        if (ends.size() == 2 && adj[ends[0]].count(ends[1]) == 0) {
            adj[ends[0]].insert(ends[1]);
            adj[ends[1]].insert(ends[0]);
        }
    }
    std::set<std::pair<std::pair<long long, long long>, std::pair<long long, long long>>> used;
    std::vector<std::vector<Pt>> rings;
    for (const auto& kv : adj) {
        for (const auto& to : kv.second) {
            auto start = std::make_pair(kv.first, to);
            if (used.count(start) != 0) {
                continue;
            }
            std::vector<Pt> pts;
            auto cur = start;
            bool ok = true;
            while (used.count(cur) == 0) {
                used.insert(cur);
                pts.push_back(node[cur.first]);
                const Pt a = node[cur.first];
                const Pt b = node[cur.second];
                const double in_ang = std::atan2(a[1] - b[1], a[0] - b[0]);
                double best = 0.0;
                bool have = false;
                std::pair<long long, long long> next{};
                for (const auto& c : adj[cur.second]) {
                    const Pt p = node[c];
                    double d = std::atan2(p[1] - b[1], p[0] - b[0]) - in_ang;
                    while (d <= 1e-9) {
                        d += 2.0 * 3.14159265358979323846;
                    }
                    while (d > 2.0 * 3.14159265358979323846 + 1e-9) {
                        d -= 2.0 * 3.14159265358979323846;
                    }
                    if (!have || d < best) {
                        best = d;
                        next = c;
                        have = true;
                    }
                }
                if (!have) {
                    ok = false;
                    break;
                }
                cur = std::make_pair(cur.second, next);
                if (cur == start) {
                    break;
                }
            }
            if (!ok || pts.size() < 3) {
                continue;
            }
            double a2 = 0.0;
            for (std::size_t i = 0; i < pts.size(); ++i) {
                const Pt& p0 = pts[i];
                const Pt& p1 = pts[(i + 1) % pts.size()];
                a2 += p0[0] * p1[1] - p1[0] * p0[1];
            }
            // Грань обхода с ПОЛОЖИТЕЛЬНОЙ площадью — внутренность корпуса;
            // отрицательная — та же связная часть снаружи, её кольцом не
            // считаем. Порог 1 кв.м отсекает щели между примыкающими телами.
            if (a2 / 2.0 < 1.0) {
                continue;
            }
            // Снять точки, лежащие на прямой между соседями: сторона стены —
            // это сторона, а не цепочка вершин, доставшихся от проёмов.
            std::vector<Pt> flat;
            for (std::size_t i = 0; i < pts.size(); ++i) {
                const Pt& p = pts[i];
                const Pt& q = pts[(i + pts.size() - 1) % pts.size()];
                const Pt& r = pts[(i + 1) % pts.size()];
                const double cr = (p[0] - q[0]) * (r[1] - p[1])
                                  - (p[1] - q[1]) * (r[0] - p[0]);
                const double dt = (p[0] - q[0]) * (r[0] - p[0])
                                  + (p[1] - q[1]) * (r[1] - p[1]);
                if (std::fabs(cr) < 1e-7 && dt > 0.0) {
                    continue;
                }
                flat.push_back(p);
            }
            if (flat.size() >= 3) {
                rings.push_back(flat);
            }
        }
    }
    // Кольцо ВНУТРИ другого кольца — это комната, а не корпус: под внутренней
    // перегородкой фундамент есть, но он под полом и ленте наружу не нужен.
    std::sort(rings.begin(), rings.end(),
              [](const std::vector<Pt>& a, const std::vector<Pt>& b) {
                  const auto area = [](const std::vector<Pt>& r) {
                      double s = 0.0;
                      for (std::size_t i = 0; i < r.size(); ++i) {
                          s += r[i][0] * r[(i + 1) % r.size()][1]
                               - r[(i + 1) % r.size()][0] * r[i][1];
                      }
                      return s;
                  };
                  const double aa = area(a);
                  const double ab = area(b);
                  if (aa != ab) {
                      return aa > ab;
                  }
                  return a < b;
              });
    const auto inside = [](const Pt& p, const std::vector<Pt>& poly) {
        bool c = false;
        for (std::size_t i = 0; i < poly.size(); ++i) {
            const Pt& p1 = poly[i];
            const Pt& p2 = poly[(i + 1) % poly.size()];
            if ((p1[1] > p[1]) != (p2[1] > p[1])
                && p[0] < (p2[0] - p1[0]) * (p[1] - p1[1]) / (p2[1] - p1[1]) + p1[0]) {
                c = !c;
            }
        }
        return c;
    };
    std::vector<std::vector<Pt>> keep;
    for (const auto& r : rings) {
        double cx = 0.0;
        double cz = 0.0;
        for (const Pt& p : r) {
            cx += p[0];
            cz += p[1];
        }
        cx /= static_cast<double>(r.size());
        cz /= static_cast<double>(r.size());
        bool nested = false;
        for (const auto& k : keep) {
            if (inside({cx, cz}, k)) {
                nested = true;
                break;
            }
        }
        if (!nested) {
            keep.push_back(r);
        }
    }
    // КАНОНИЧЕСКОЕ НАЧАЛО КОЛЬЦА — младшая точка: обход граней стартует с
    // произвольной дуги, и без этого одна и та же кладка печаталась бы
    // разными строками от прогона к прогону.
    for (auto& r : keep) {
        const auto it = std::min_element(r.begin(), r.end());
        std::rotate(r.begin(), it, r.end());
    }
    return keep;
}

} // namespace detail

/// Замер одной постройки по УЖЕ ПРОЧИТАННОМУ графу. Отдельно от обхода полки,
/// чтобы тест мог замерить один файл, не поднимая всю библиотеку.
[[nodiscard]] inline ManifestRow measure_house(const std::string& name,
                                               const std::string& file,
                                               const dfn::world::HouseGraph& g) {
    using namespace dfn::world;
    ManifestRow r;
    r.name = name;
    r.file = file;
    r.family = recipe_family(name);
    r.elems = g.element_count();
    r.verts = g.vertex_count();

    std::map<VertexId, std::array<double, 3>> pos;
    const double inf = std::numeric_limits<double>::infinity();
    r.g_lo[0] = r.g_lo[1] = r.g_lo[2] = inf;
    r.g_hi[0] = r.g_hi[1] = r.g_hi[2] = -inf;
    for (const Vertex& v : g.vertices()) {
        // Вершина НА ОСИ координат не носит: её место считает построитель
        // меша. На полке таких нет (кузницы ставят только Free), но манифест
        // обязан говорить это вслух, а не молча приписать ей начало координат.
        if (v.anchoring == Anchoring::OnEdge) {
            continue;
        }
        const std::array<double, 3> p = {q(v.local.x), q(v.local.y), q(v.local.z)};
        pos[v.id] = p;
        for (int i = 0; i < 3; ++i) {
            r.g_lo[i] = std::min(r.g_lo[i], p[i]);
            r.g_hi[i] = std::max(r.g_hi[i], p[i]);
        }
    }
    if (pos.empty()) {
        r.g_lo[0] = r.g_lo[1] = r.g_lo[2] = 0.0;
        r.g_hi[0] = r.g_hi[1] = r.g_hi[2] = 0.0;
    }

    // ПЯТНО СТЕН — вершины НИЖНЕГО ВЕНЦА (y == 0), а не минимальный y: у
    // мельницы колесо уходит на -1.65, и «нижний венец по минимуму» дал бы
    // отпечаток в две вершины оси колеса. Меньше четырёх — венца нет
    // (плита, бордюр), тогда пятно совпадает с габаритом.
    std::vector<const std::array<double, 3>*> low;
    for (const auto& kv : pos) {
        if (std::fabs(kv.second[1]) < 1e-6) {
            low.push_back(&kv.second);
        }
    }
    if (low.size() >= 4) {
        r.w_lo[0] = r.w_lo[1] = inf;
        r.w_hi[0] = r.w_hi[1] = -inf;
        for (const auto* p : low) {
            r.w_lo[0] = std::min(r.w_lo[0], (*p)[0]);
            r.w_hi[0] = std::max(r.w_hi[0], (*p)[0]);
            r.w_lo[1] = std::min(r.w_lo[1], (*p)[2]);
            r.w_hi[1] = std::max(r.w_hi[1], (*p)[2]);
        }
    } else {
        r.w_lo[0] = r.g_lo[0];
        r.w_hi[0] = r.g_hi[0];
        r.w_lo[1] = r.g_lo[2];
        r.w_hi[1] = r.g_hi[2];
    }

    // Элементы — по возрастанию имени: тот же порядок, в котором они стоят в
    // файле, и значит «последняя дверь побеждает» значит то же самое здесь и
    // при чтении файла глазами.
    std::vector<const Element*> els;
    els.reserve(g.elements().size());
    for (const Element& e : g.elements()) {
        els.push_back(&e);
    }
    std::sort(els.begin(), els.end(),
              [](const Element* a, const Element* b) { return a->id < b->id; });

    r.top = 0.0;
    for (const auto& kv : pos) {
        r.top = std::max(r.top, kv.second[1]);
    }
    double best_area = -1.0;
    for (const Element* e : els) {
        std::vector<const std::array<double, 3>*> refs;
        for (const VertexId id : e->refs) {
            const auto it = pos.find(id);
            if (it != pos.end()) {
                refs.push_back(&it->second);
            }
        }
        if (refs.empty()) {
            continue;
        }
        if (e->kind == ElementKind::Surface) {
            double hi = -inf;
            for (const auto* p : refs) {
                hi = std::max(hi, (*p)[1]);
            }
            r.top = std::max(r.top, hi + detail::param_num(*e, "height"));
        }
        if (detail::flag(*e, "door")) {
            double sx = 0.0;
            double sz = 0.0;
            for (const auto* p : refs) {
                sx += (*p)[0];
                sz += (*p)[2];
            }
            r.has_door = true;
            r.door_pt[0] = sx / static_cast<double>(refs.size());
            r.door_pt[1] = sz / static_cast<double>(refs.size());
        }
        if (e->kind == ElementKind::Surface && detail::flag(*e, "beams")) {
            double lo = inf;
            for (const auto* p : refs) {
                lo = std::min(lo, (*p)[1]);
            }
            r.beams.push_back(lo - detail::param_num(*e, "thickness") / 2.0);
        }
        // ВЕРХ ПОЛОВОЙ ПЛИТЫ — самая широкая горизонтальная замкнутая плита,
        // ЦЕЛИКОМ лежащая в пятне стен. Отбор по площади без этого условия
        // берёт у замка плиты парадного двора (255 кв.м на 0.12) вместо пола
        // зала (120 на 1.66) — двор шире стен, пол по построению не шире.
        // Толщина плиты симметрична срединной плоскости (HousePlate.cpp),
        // поэтому верх — это «вершина + thickness/2».
        if (e->kind == ElementKind::Surface && e->closed && refs.size() >= 3) {
            double ylo = inf;
            double yhi = -inf;
            double xlo = inf;
            double xhi = -inf;
            double zlo = inf;
            double zhi = -inf;
            for (const auto* p : refs) {
                ylo = std::min(ylo, (*p)[1]);
                yhi = std::max(yhi, (*p)[1]);
                xlo = std::min(xlo, (*p)[0]);
                xhi = std::max(xhi, (*p)[0]);
                zlo = std::min(zlo, (*p)[2]);
                zhi = std::max(zhi, (*p)[2]);
            }
            const bool horizontal = yhi - ylo <= 1e-6;
            const bool inside = xlo >= r.w_lo[0] - 1e-6 && xhi <= r.w_hi[0] + 1e-6
                                && zlo >= r.w_lo[1] - 1e-6 && zhi <= r.w_hi[1] + 1e-6;
            if (horizontal && inside) {
                double a2 = 0.0;
                for (std::size_t i = 0; i < refs.size(); ++i) {
                    const auto& p0 = *refs[i];
                    const auto& p1 = *refs[(i + 1) % refs.size()];
                    a2 += p0[0] * p1[2] - p1[0] * p0[2];
                }
                const double area = std::fabs(a2) / 2.0;
                const double top = ylo + detail::param_num(*e, "thickness") / 2.0;
                if (area > best_area || (area == best_area && top < r.floor)) {
                    best_area = area;
                    r.floor = top;
                    r.has_floor = true;
                }
            }
        }
    }
    std::sort(r.beams.begin(), r.beams.end());
    r.wpoly = detail::wall_rings(detail::ground_wall_segs(g, pos));

    if (r.has_door) {
        const double cx = (r.g_lo[0] + r.g_hi[0]) / 2.0;
        const double cz = (r.g_lo[2] + r.g_hi[2]) / 2.0;
        const double dx = r.door_pt[0] - cx;
        const double dz = r.door_pt[1] - cz;
        if (std::fabs(dz) >= std::fabs(dx)) {
            r.door_axis = dz > 0 ? "+Z" : "-Z";
            r.door_off = std::fabs(dz);
            r.face_off = dz > 0 ? r.g_hi[2] - r.door_pt[1]
                                : r.door_pt[1] - r.g_lo[2];
        } else {
            r.door_axis = dx > 0 ? "+X" : "-X";
            r.door_off = std::fabs(dx);
            r.face_off = dx > 0 ? r.g_hi[0] - r.door_pt[0]
                                : r.door_pt[0] - r.g_lo[0];
        }
    }

    const HouseMesh mesh = build_house_mesh(g);
    r.tris = mesh.triangle_count();
    r.m_lo[0] = r.m_lo[1] = r.m_lo[2] = inf;
    r.m_hi[0] = r.m_hi[1] = r.m_hi[2] = -inf;
    for (const HouseVertex& v : mesh.vertices) {
        const double p[3] = {static_cast<double>(v.pos.x), static_cast<double>(v.pos.y),
                             static_cast<double>(v.pos.z)};
        for (int i = 0; i < 3; ++i) {
            r.m_lo[i] = std::min(r.m_lo[i], p[i]);
            r.m_hi[i] = std::max(r.m_hi[i], p[i]);
        }
    }
    if (mesh.vertices.empty()) {
        r.m_lo[0] = r.m_lo[1] = r.m_lo[2] = 0.0;
        r.m_hi[0] = r.m_hi[1] = r.m_hi[2] = 0.0;
    }
    return r;
}

/// ОБЪЯВЛЕНИЕ КОЛОНОК — часть формата, а не украшение: читатель разбирает
/// строку как dict(zip(cols, line.split())), и новая колонка дописывается
/// ТОЛЬКО В КОНЕЦ, чтобы старые позиции не поехали.
inline constexpr const char* MANIFEST_COLUMNS =
    "name file family elems verts tris "
    "gx0 gy0 gz0 gx1 gy1 gz1 "
    "mx0 my0 mz0 mx1 my1 mz1 "
    "top wx0 wz0 wx1 wz1 "
    "door_axis door_off dpx dpz face_off beams floor wpoly";

/// Обходит полку, замеряет КАЖДЫЙ .dfh и пишет dir/INDEX.txt.
///
/// ЗАМЕР ИДЁТ С ФАЙЛОВ, А НЕ С ГРАФА В ПАМЯТИ. Так манифест остаётся полным и
/// верным после выпечки одного рецепта ключом --only: переписать строку только
/// испечённого значило бы держать в INDEX.txt числа, снятые в разное время с
/// разных версий полки.
[[nodiscard]] inline bool write_house_manifest(const std::string& dir) {
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& it : fs::directory_iterator(dir, ec)) {
        if (it.path().extension() == ".dfh") {
            files.push_back(it.path().filename().string());
        }
    }
    if (ec) {
        std::fprintf(stderr, "manifest: не открылась полка %s: %s\n", dir.c_str(),
                     ec.message().c_str());
        return false;
    }
    // ПОРЯДОК СТРОК — ЧАСТЬ ДОГОВОРА, А НЕ ОФОРМЛЕНИЕ, И КЛИЕНТ У НЕГО НЕ ОДИН.
    // directory_iterator отдаёт имена в порядке файловой системы: он зависит от
    // того, в каком порядке файлы создавались, и на другой машине другой. Без
    // этой строки манифест перестал бы совпадать сам с собой между прогонами —
    // и это ещё не худшее.
    //
    // Худшее в том, что генератор города обходит семейства выпечки В ПОРЯДКЕ
    // СТРОК МАНИФЕСТА (ростовки марша и кромки, столбы, куски мощения), а из
    // порядка обхода выбирается рецепт ПРИ РАВНОЙ ПРИГОДНОСТИ. То есть уехавшая
    // строка сдвинет не «оформление манифеста», а посадку марша террасы, и
    // связь между причиной и следствием никто не восстановит. Замечание волны
    // И1 (agent epoch-i1), у которой этот обход в цепочке.
    std::sort(files.begin(), files.end());

    std::string out;
    out += "# assets/houses/INDEX.txt — МАНИФЕСТ ПОЛКИ ГОТОВЫХ ПОСТРОЕК.\n";
    out += "# Пишется кузницами dfn_houses и dfn_furniture после выпечки; руками\n";
    out += "# не править — следующий прогон перепишет. Числа ЗАМЕРЕНЫ с самих .dfh.\n";
    out += "#\n";
    out += "# Это ЕДИНСТВЕННЫЙ источник габаритов для генератора города: .dfh\n";
    out += "# разбирает только C++ (read_house), второго разборщика формата нет.\n";
    out += "#\n";
    out += "# Строка на рецепт, поля через пробел, порядок строк — по имени файла.\n";
    out += "# Разбор: dict(zip(cols, line.split())). \"-\" — величины нет.\n";
    out += "#\n";
    out += "#   family      пакет рецепта: префикс имени до дефиса (city, furn,\n";
    out += "#               будущие городские пакеты — своим именем)\n";
    out += "#   elems verts элементов и вершин в ГРАФЕ\n";
    out += "#   tris        треугольников в построенном меше\n";
    out += "#   g*          габарит по вершинам ГРАФА: посадочное пятно без свеса\n";
    out += "#   m*          габарит по вершинам ПОСТРОЕННОГО МЕША: занимаемый объём\n";
    out += "#               со стенами по толщине, свесами и скатами\n";
    out += "#   top         верх тела с учётом height у поверхностей\n";
    out += "#   w*          пятно СТЕН (вершины нижнего венца, y == 0), в плане\n";
    out += "#   door_*      ось наружной нормали двери и её вынос от центра g-габарита\n";
    out += "#   dpx dpz     точка двери в локали рецепта\n";
    out += "#   face_off    вынос ЛИЦА за дверную точку: от двери до наружной\n";
    out += "#               грани g-габарита по оси двери. Разница между\n";
    out += "#               «дверь на красной линии» и «фасад на красной линии»\n";
    out += "#   beams       низы несомых балок (surface beams=1) через запятую\n";
    out += "#   floor       верх САМОЙ ШИРОКОЙ горизонтальной замкнутой плиты,\n";
    out += "#               целиком лежащей в пятне стен. У дома это пол; у\n";
    out += "#               прогона стены — боевой ход, у бордюра — его верх\n";
    out += "#   wpoly       КОНТУР пятна стен: замкнутые кольца наземной кладки,\n";
    out += "#               кольцо на корпус. Кольца через ';', точки через ',',\n";
    out += "#               x и z через '/'. Обход ПРОТИВ ЧАСОВОЙ: наружная\n";
    out += "#               нормаль стороны (a -> b) равна (dz, -dx)/|ab|.\n";
    out += "#               Прямоугольник w* говорит, СКОЛЬКО МЕСТА занять;\n";
    out += "#               wpoly — ГДЕ ИМЕННО СТОИТ СТЕНА, и у Г-образного\n";
    out += "#               плана, у корпуса с крыльями и у мельницы с колесом\n";
    out += "#               это разные ответы. '-' — колец нет (амбар на сваях,\n";
    out += "#               лоток), читателю остаётся прямоугольник\n";
    out += "#\n";
    out += "# columns: ";
    out += MANIFEST_COLUMNS;
    out += "\n";

    std::size_t rows = 0;
    for (const std::string& fname : files) {
        const fs::path path = fs::path(dir) / fname;
        std::ifstream in(path, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        dfn::world::HouseGraph g;
        const auto io = dfn::world::read_house(text, g);
        if (!io.ok) {
            // ПОЛКА С НЕЧИТАЕМЫМ ФАЙЛОМ — НЕ ПОЛКА. Пропустить строку значило
            // бы отдать генератору манифест, в котором рецепта просто нет, и
            // тот встал бы городом без этого дома, не сказав почему.
            std::fprintf(stderr, "manifest: %s не прочёлся: %s (строка %d)\n",
                         path.string().c_str(), io.why.c_str(), io.line);
            return false;
        }
        const std::string name = fs::path(fname).stem().string();
        const ManifestRow r = measure_house(name, fname, g);
        std::string line = r.name;
        line += ' ';
        line += r.file;
        line += ' ';
        line += r.family;
        line += ' ' + std::to_string(r.elems);
        line += ' ' + std::to_string(r.verts);
        line += ' ' + std::to_string(r.tris);
        for (int i = 0; i < 3; ++i) {
            line += ' ' + detail::fnum(r.g_lo[i]);
        }
        for (int i = 0; i < 3; ++i) {
            line += ' ' + detail::fnum(r.g_hi[i]);
        }
        for (int i = 0; i < 3; ++i) {
            line += ' ' + detail::fnum(r.m_lo[i]);
        }
        for (int i = 0; i < 3; ++i) {
            line += ' ' + detail::fnum(r.m_hi[i]);
        }
        line += ' ' + detail::fnum(r.top);
        line += ' ' + detail::fnum(r.w_lo[0]);
        line += ' ' + detail::fnum(r.w_lo[1]);
        line += ' ' + detail::fnum(r.w_hi[0]);
        line += ' ' + detail::fnum(r.w_hi[1]);
        line += ' ' + r.door_axis;
        line += ' ' + (r.has_door ? detail::fnum(r.door_off) : std::string("-"));
        line += ' ' + (r.has_door ? detail::fnum(r.door_pt[0]) : std::string("-"));
        line += ' ' + (r.has_door ? detail::fnum(r.door_pt[1]) : std::string("-"));
        line += ' ' + (r.has_door ? detail::fnum(r.face_off) : std::string("-"));
        if (r.beams.empty()) {
            line += " -";
        } else {
            line += ' ';
            for (std::size_t i = 0; i < r.beams.size(); ++i) {
                if (i != 0) {
                    line += ',';
                }
                line += detail::fnum(r.beams[i]);
            }
        }
        line += ' ' + (r.has_floor ? detail::fnum(r.floor) : std::string("-"));
        if (r.wpoly.empty()) {
            line += " -";
        } else {
            line += ' ';
            for (std::size_t i = 0; i < r.wpoly.size(); ++i) {
                if (i != 0) {
                    line += ';';
                }
                for (std::size_t j = 0; j < r.wpoly[i].size(); ++j) {
                    if (j != 0) {
                        line += ',';
                    }
                    line += detail::fnum(r.wpoly[i][j][0]);
                    line += '/';
                    line += detail::fnum(r.wpoly[i][j][1]);
                }
            }
        }
        line += '\n';
        out += line;
        ++rows;
    }

    const fs::path index = fs::path(dir) / "INDEX.txt";
    std::ofstream f(index, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::fprintf(stderr, "manifest: не открылся %s\n", index.string().c_str());
        return false;
    }
    f << out;
    if (!f) {
        std::fprintf(stderr, "manifest: не записался %s\n", index.string().c_str());
        return false;
    }
    std::fprintf(stderr, "manifest: %zu рецептов -> %s\n", rows,
                 index.string().c_str());
    return true;
}

} // namespace dfn::forge
