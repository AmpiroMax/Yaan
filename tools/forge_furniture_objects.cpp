/*
Created: 27:08:2026 - 10:18:14
Last updated: 28:08:2026 - 17:31:25
Module: tools
File: tools/forge_furniture_objects.cpp

Responsibility:
- ПЕЧЬ ПОЛКИ МЕБЕЛИ (dfn_furn_objects): берёт ЧЕРТЁЖ предмета из
  assets/houses/furn-*.dfh и печёт его ОДИН РАЗ в объект реестра
  assets/objects/furniture/<имя>.dfo — тело, свет и материал уже посчитаны, и
  на месте остаётся ссылка «позиция + поворот».

ЗАЧЕМ (заказ владельца 27.08, дословно): «Кровати ощущаются не как один объект
запечённый, а что-то нарисованное по месту. Хочу видеть подтверждение
существования их в перечне наших объектов, чтобы точно знать, что они схожим
образом рисуются везде, и если их обновить — они везде обновятся при
необходимости». Ощущение было ВЕРНЫМ по устройству: .dfh — это РЕЦЕПТ, и
build_house_mesh + печка AO прогонялись на КАЖДУЮ кровать в каждой комнате.
Восемьдесят шесть локаций Вайтрана и Корнхолла несут 118 кроватей — сто
восемнадцать сборок одного и того же тела, и ни одной записи в перечне
объектов, по которой владелец мог бы проверить, что тело одно.

ЧТО ЗДЕСЬ НЕ МЕНЯЕТСЯ. Ни одно число геометрии. Тело печётся ТОЙ ЖЕ
build_house_mesh, свет — ТОЙ ЖЕ bake_house_sky_visibility, плитка листа —
ТОЙ ЖЕ world::house_part_tile, что зовёт загрузчик города. Задача печи —
ПЕРЕЛОЖИТЬ готовое в .dfo, а не пересчитать по-своему: пересчёт по-своему и
есть тот самый второй ответ, от которого владелец просит избавиться.

Usage:
    dfn_furn_objects [<out_dir>]        (умолчание assets/objects/furniture)
    dfn_furn_objects --list             (перечислить и остановиться, не пиша)
    dfn_furn_objects --houses <dir>     (черпать чертежи из другой папки)
    dfn_furn_objects --bevel <метры>    (ширина фаски; 0 — прежняя острая полка)

ЗАЧЕМ --houses. Проба «что будет, если у кровати сменить тон одеяла» не имеет
права править ЖИВУЮ полку домов: по ней в это же время ходят другие волны, и
чужой .dfh, изменившийся под руками, — это не проба, а поломка у соседа. С
этим ключом черновой чертёж лежит где угодно, печётся оттуда, а полка домов
не тронута ни байтом.

Dependencies:
- Uses: engine/world (HouseFile, HouseGraph, HouseMesh, house_part_tile),
  engine/render (ObjectRegistry).
- Used by: человек и агент мебели; артефакты читают сцена города и локация,
  перечень assets/objects/furniture/INDEX.md читает владелец.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КРУГОВОЙ ПРОГОН ИЛИ ОТКАЗ, как у dfn_forge и dfn_kit: всякий записанный
  объект читается обратно и его хэш сверяется до того, как печь отчитается об
  успехе. Реестр, чьи файлы не открываются, не индексирует ничего.
- ПЕЧЬ ОТКАЗЫВАЕТ ТОМУ, ЧЕГО НЕ УМЕЕТ ПОВТОРИТЬ, и говорит почему (см.
  refusal_of ниже). Молча испечь предмет, который в городе выглядит иначе, —
  худший исход из возможных: подмена была бы точной ровно до первого взгляда.
*/
/*
UPD:
- 27:08:2026 - 10:18:14: Создана. Кровать furn-bed — первый и пока единственный
  жилец полки: в текущих сценах Вайтрана и Корнхолла кровать РОВНО ОДНОГО вида
  (118 расстановок одного рецепта furn-bed.dfh, ни одной второй), и заводить
  «односпальную/двуспальную» про запас значило бы завести в перечне объект,
  которого нигде нет.
- 27:08:2026 - 10:34:00: МЕТКА ЗАПИСИ ВЫШЕ ИСПРАВЛЕНА НА РЕАЛЬНОЕ ВРЕМЯ (правило 16):
  стояло 15:xx при стенных 10:18 — я выбрал метку позже чужой записи 14:30 в
  AppHouse.cpp, лишь бы сошлась сверка хука, вместо того чтобы разобраться с
  чужой. Поставлено время коммита 9e2c8c9 (10:18:14). Текста записи и кода не
  трогал.
- 28:08:2026 - 11:52:00: ПОЛКА ВЫРОСЛА С ОДНОГО ПРЕДМЕТА ДО ДВАДЦАТИ — первая
  партия номенклатуры комнаты (заказ владельца 28.08 «достаточно объектов для
  повторения комнат Скайрима»). Шестнадцать новых тел из кузницы плюс три
  рецепта, которые лежали на полке домов и до сих пор строились ПО МЕСТУ в
  каждой комнате (стол, лавка, стеллаж) — ровно та болезнь, из-за которой эта
  печь и заведена: 118 кроватей были не одиноки, столов в двух городах ещё
  больше.
  ПРАВИЛО «ПРЕДМЕТ ПОПАДАЕТ СЮДА В ТОТ ДЕНЬ, КОГДА ЕГО РАССТАНОВКИ ПЕРЕВОДЯТ НА
  ССЫЛКУ» ВЫПОЛНЕНО, А НЕ ОБОЙДЕНО: партию ставит стенд
  assets/scenes/furniture-room.scene — комната по скрину image copy 9, тридцать
  пять расстановок, и каждая — [place] object=, а не [house] file=. Стенд и
  есть тот потребитель, ради которого печётся полка, и он же мерило владельца.
  Боевые города партию НЕ ставят: расстановка по домам — следующая волна.
  КОНТРОЛЬНАЯ РУКА, И ОНА ВАЖНЕЕ ПЕРЕЧНЯ: хэш кровати 09b01647bd52a00a НЕ
  ИЗМЕНИЛСЯ после того, как в перечень печи добавили девятнадцать соседей.
  Единственный прежний жилец полки — контроль на саму правку печи; сдвинься в
  общем коде что-нибудь, кровать показала бы это первой. Плюс перепечка в
  отдельный каталог: sha256 четырёх файлов совпали с боевыми побайтово.
  ЧТО ПЕЧЬ ОТВЕРГЛА, замерено, а не предсказано: furn-barrel — ОТКАЗ по износу
  0.45 на клёпке мельче 1.4 м (проба ставилась нарочно и откатана). Бочка и
  поленница остаются на стенде РЕЦЕПТАМИ, и это названная дыра партии.
- 28:08:2026 - 14:05:00: ПОЛКА 20 -> 35 — вторая партия номенклатуры (ярус выше 1.8 м,
  стены, снедь). Перечень вырос пятнадцатью строками; порядок тот же, что в кузнице.
  КОНТРОЛЬНАЯ РУКА — СОСЕДИ НЕ ДВИГАЮТ СОСЕДЕЙ, замерено: все двадцать строк первой
  партии в INDEX.md ИДЕНТИЧНЫ после добавления пятнадцати новых (ни один замер и ни один
  хэш не сдвинулся), а двадцать .dfo первой партии сошлись ПОБАЙТОВО с выпечкой, снятой
  до правки. Перепечка всех 35 в отдельный каталог дала те же файлы и тот же перечень.
  ОТКАЗОВ НЕТ НИ У ОДНОГО из пятнадцати: wear не поставлен нигде (печь отказывает куску
  с износом мельче 1.4 м — замер на бочке), поэтому прежняя дыра партии не выросла.
  ДЫРА ПЕРВОЙ ПАРТИИ ЧАСТИЧНО ЗАКРЫТА: на стенде стояла ДВОРОВАЯ поленница
  furn-woodpile рецептом — полтора метра кладки у стены сарая, поставленные в жилую
  комнату за неимением комнатной вязанки. Теперь её место занимает ссылка на
  furn-firewood (0.62). Рецептом в комнате осталась ОДНА бочка — та, которой печь
  отказывает; было пять рецептов, стало четыре.
- 28:08:2026 - 15:40:00: КЛЮЧ --bevel (ширина фаски, метры; умолчание —
  движковое HOUSE_BEVEL_W_DEFAULT). Заведён волной фаски РАДИ КОНТРОЛЬНОЙ
  РУКИ: сторож «фаска не двигает габарит» печёт полку дважды в одном прогоне —
  с нулевой шириной и со штатной — и сверяет колонки x/z/низ/верх. Сверка с
  перечнем прошлого коммита такой руки не даёт: она мерила бы заодно всё, что
  за сутки поменялось помимо фаски, и две одинаково неверные выпечки прошли бы
  её обе.
- 28:08:2026 - 17:31:25: кусок режется по ПАРЕ (клетка, имя вещества), и
  шаг повтора берётся из записи реестра (волна 3 зоны МАТЕРИАЛЫ). У полки, вещества не
  называющей, ключ ведёт себя как прежний: 35 файлов перепечены с
  отличием ровно в один байт версии контейнера, INDEX.md совпал.
*/

#include "engine/core/materials/sources/MaterialRegistry.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseMesh.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using dfn::render::HouseSubmesh;
using dfn::render::RegistryObject;

/// ОДИН ЖИЛЕЦ ПОЛКИ: чертёж и имя объекта. Таблица, а не обход каталога:
/// на полке домов лежат ШЕСТНАДЦАТЬ рецептов furn-*, и печь их все в реестр
/// значило бы завести объекты, которых никто не ставит ссылкой. Предмет
/// попадает сюда в тот день, когда его расстановки переводят на ссылку.
struct Item {
    const char* house;  ///< путь к чертёжу от корня репозитория
    const char* name;   ///< имя объекта в реестре (без .dfo)
    const char* what;   ///< строка перечня — что это по-русски
};

const Item ITEMS[] = {
    {"assets/houses/furn-bed.dfh", "furn-bed",
     "кровать односпальная, рама на столбиках, дощатый настил, изголовье-щит"},
    // ПЕРВАЯ ПАРТИЯ НОМЕНКЛАТУРЫ КОМНАТЫ (28.08). Порядок — ПО ЧАСТОТЕ на
    // девятнадцати скринах интерьеров владельца (images_examples/houses_indoors,
    // подсчёт и таблица — docs/reports/furniture-nomenclature.html).
    //
    // ПОЧЕМУ ОНИ ЗДЕСЬ РАНЬШЕ, ЧЕМ ИХ КТО-ТО СТАВИТ. Правило полки, записанное
    // выше, гласит: «предмет попадает сюда в тот день, когда его расстановки
    // переводят на ссылку». Здесь он выполняется, а не обходится: партию
    // ставит СТЕНД assets/scenes/furniture-room.scene — комната по скрину
    // image copy 9, собранная СПИСКОМ ССЫЛОК [place] object=, а не списком
    // чертежей. Стенд и есть тот потребитель, ради которого печётся полка, и
    // он же — мерило владельца «наш набор повторяет их комнату». Боевые города
    // партию не ставят: расстановка по домам — следующая волна.
    //
    // ТОНА И ПЛИТКИ ЗДЕСЬ НЕ РЕШАЮТСЯ. Пары surface/tone приходят с чертежа и
    // станут маппингом в понятие Material (зона big-materials); словесный
    // материал каждого предмета — колонка отчёта, а не поле этой таблицы:
    // поле, которое ничего не меняет в выпечке, устареет молча.
    {"assets/houses/furn-chair.dfh", "furn-chair",
     "стул со спинкой, задние ножки насквозь, три доски спинки"},
    {"assets/houses/furn-table.dfh", "furn-table",
     "стол обеденный 1.8x0.9, столешница на 0.80, царги и Н-проножка"},
    {"assets/houses/furn-bowl.dfh", "furn-bowl",
     "миска глиняная d 0.23, утопленное нутро тёмного тона"},
    {"assets/houses/furn-jug.dfh", "furn-jug",
     "кувшин глиняный высотой 0.30, ручка коленом"},
    {"assets/houses/furn-candlestick.dfh", "furn-candlestick",
     "канделябр на три свечи, пламя самосветное (glow)"},
    {"assets/houses/furn-rug.dfh", "furn-rug",
     "ковёр 2.0x1.4, кайма и поле разных тонов"},
    {"assets/houses/furn-cupboard.dfh", "furn-cupboard",
     "комод-буфет 1.0x0.48, три ящика лицом, ручки-кольца"},
    {"assets/houses/furn-shelf.dfh", "furn-shelf",
     "стеллаж у стены 1.2x0.35, три полки, высота 2.05"},
    {"assets/houses/furn-shelf-wall.dfh", "furn-shelf-wall",
     "полка настенная 1.1x0.28 на косынках, начало координат — низ кронштейна"},
    {"assets/houses/furn-cauldron.dfh", "furn-cauldron",
     "котёл на треноге, верх жердей 1.20, тренога шире венца на 0.03"},
    {"assets/houses/furn-books.dfh", "furn-books",
     "стопка из четырёх книг, переплёты крашеные, блок страниц уступом"},
    {"assets/houses/furn-chest.dfh", "furn-chest",
     "сундук 0.9x0.5, крышка плитой с напуском, три полосы и накладка замка"},
    {"assets/houses/furn-bench.dfh", "furn-bench",
     "лавка 1.6x0.35, сиденье на 0.42, две щеки и проножка"},
    {"assets/houses/furn-basket.dfh", "furn-basket",
     "корзина d 0.48, три пояса плетения, обод поверху"},
    {"assets/houses/furn-hide.dfh", "furn-hide",
     "шкура на полу 1.7x1.25, контур с лапами, краска — белила"},
    {"assets/houses/furn-cup.dfh", "furn-cup",
     "кубок на ножке, высота 0.125"},
    {"assets/houses/furn-bottle.dfh", "furn-bottle",
     "бутыль высотой 0.325 с пробкой"},
    {"assets/houses/furn-stool.dfh", "furn-stool",
     "табурет 0.36x0.36, сиденье на 0.46, ножки развёрнуты наружу"},
    {"assets/houses/furn-sack.dfh", "furn-sack",
     "мешок высотой 0.62, горловина перевязана"},
    // ВТОРАЯ ПАРТИЯ (28.08): ЯРУС ВЫШЕ 1.8 м, СТЕНЫ И СНЕДЬ. Порядок — по
    // разделу 6 отчёта первой партии, где нехватки уже были расставлены по
    // частоте. Жердь-вешало идёт первой не по алфавиту: на всех восьми
    // скринах со снедью она — то, НА ЧЁМ снедь висит, и без неё связка
    // чеснока висела бы в пустоте.
    {"assets/houses/furn-rail.dfh", "furn-rail",
     "жердь-вешало под потолком 1.80 на двух подвесах"},
    {"assets/houses/furn-herbs.dfh", "furn-herbs",
     "пучок сухих трав высотой 0.40, семь стеблей веером от перевязи"},
    {"assets/houses/furn-garlic.dfh", "furn-garlic",
     "связка чеснока высотой 0.42, шесть головок стопкой по оси"},
    {"assets/houses/furn-ham.dfh", "furn-ham",
     "окорок на крюке высотой 0.50, тулово каплей"},
    {"assets/houses/furn-fish.dfh", "furn-fish",
     "три вяленых рыбины на шнуре, высота 0.46"},
    {"assets/houses/furn-chandelier.dfh", "furn-chandelier",
     "люстра-обод d 0.60 на восемь свечей, пламя самосветное (glow)"},
    {"assets/houses/furn-plaque.dfh", "furn-plaque",
     "плакетка на стену, круглый резной щит d 0.40"},
    {"assets/houses/furn-painting.dfh", "furn-painting",
     "картина в раме 0.62x0.46, полотно и рама на одних углах"},
    {"assets/houses/furn-tapestry.dfh", "furn-tapestry",
     "гобелен 0.90x1.24 на жерди, кайма понизу"},
    {"assets/houses/furn-beasthead.dfh", "furn-beasthead",
     "голова зверя на щите, высота 0.52, морда и рога из вершин щита"},
    {"assets/houses/furn-armchair.dfh", "furn-armchair",
     "кресло с подлокотниками 0.62x0.60, спинка 1.06, подушка"},
    {"assets/houses/furn-firewood.dfh", "furn-firewood",
     "вязанка дров комнатная, высота 0.62, семь поленьев в перевязь"},
    {"assets/houses/furn-bread.dfh", "furn-bread",
     "каравай d 0.26, верх куполом"},
    {"assets/houses/furn-cheese.dfh", "furn-cheese",
     "круг сыра d 0.28 с отрезанным клином"},
    {"assets/houses/furn-plant.dfh", "furn-plant",
     "растение в глиняной кадке, высота 0.56, пять стеблей"},
};

/// ПОЧЕМУ ЭТОТ КУСОК НЕЛЬЗЯ ЗАПЕЧЬ — пусто, если можно. Три причины, и все
/// три об одном: запечённый объект НЕ ЗНАЕТ, ГДЕ ОН БУДЕТ СТОЯТЬ, а эти три
/// вещи загрузчик считает по месту.
///
///  1. МОХ И ГРЯЗЬ. Слой считается по МИРОВОЙ точке вершины (шум пятен) и по
///     высоте над низом ПОСТРОЙКИ. У кровати он не включается никогда (условие
///     загрузчика — износ > 0 И габарит куска меньше 1.4 м, а настил кровати
///     2.0 м по диагонали), но у бочки или поленницы включится, и запечённое
///     пятно мха поехало бы по городу одинаковым штампом.
///  2. СТВОРКА (door=1). Дверное полотно живёт своим списком, качается на
///     петле и в коллайдер входит по особому правилу. Предмет мебели створок
///     не имеет; если появится — это не предмет, это постройка.
///  3. ЧИСТО ФИЗИЧЕСКАЯ ЧАСТЬ (collider_only). Невидимый пандус под открытыми
///     ступенями: он в коллайдере и не в картинке, а у .dfo такого разделения
///     нет — секция HOUS несёт то, что рисуется, и из него же берётся тело.
std::string refusal_of(const dfn::world::HouseGraph& graph,
                       const dfn::world::HouseMesh& built,
                       const dfn::world::MeshPart& part,
                       const dfn::world::Element& e) {
    if (part.collider_only) {
        return "часть только для коллайдера (невидимый пандус) — .dfo такого "
               "разделения не носит";
    }
    if (graph.param(e.id, "door") == "1") {
        return "створка (door=1) — она качается и живёт своим списком, "
               "предметом реестра ей не быть";
    }
    const std::string w = graph.param(e.id, "wear");
    const float wear = w.empty() ? 0.0f : std::strtof(w.c_str(), nullptr);
    if (wear > 0.0f) {
        glm::vec3 lo{1e9f};
        glm::vec3 hi{-1e9f};
        for (std::uint32_t i = 0; i < part.index_count; ++i) {
            const auto& q = built.vertices[built.indices[part.index_begin + i]].pos;
            lo = glm::min(lo, q);
            hi = glm::max(hi, q);
        }
        // ТОТ ЖЕ ПОРОГ 1.4, ЧТО У ЗАГРУЗЧИКА (AppHouse.cpp, «органика только на
        // мелкой грануляции»). Названо здесь числом, а не позаимствовано: у
        // загрузчика оно решает, красить ли мхом, здесь — печь ли вообще, и
        // склеивать два разных решения одной константой было бы хуже.
        if (glm::length(hi - lo) < 1.4f) {
            return "кусок носит мох и грязь ПО МЕСТУ (износ " + w
                 + ", габарит меньше 1.4 м) — запечённый штамп поехал бы по "
                   "всему городу одинаковым пятном";
        }
    }
    return {};
}

/// КРАСКА ЭЛЕМЕНТА — вершинным цветом, ровно как у загрузчика: плитка
/// материала умножается на него в шейдере, 0xFFFFFFFF оставляет её как есть.
std::uint32_t paint_of(const dfn::world::HouseGraph& graph,
                       const dfn::world::Element& e) {
    const std::string c = graph.param(e.id, "paint");
    if (c.empty()) {
        return 0xFFFFFFFFu;
    }
    const int idx = std::clamp(std::atoi(c.c_str()), 0,
                               dfn::world::HOUSE_PAINT_COUNT - 1);
    const glm::vec3 rgb = dfn::world::HOUSE_PAINT_RGB[idx];
    const auto b = [](float f) {
        return static_cast<std::uint32_t>(std::lround(f * 255.0f));
    };
    return 0xFF000000u | (b(rgb.z) << 16) | (b(rgb.y) << 8) | b(rgb.x);
}

/// Один предмет: чертёж -> объект. Пусто (nullopt) — печь отказала и СКАЗАЛА
/// почему.
bool bake_item(const Item& item, const fs::path& houses_dir, float bevel_m,
               RegistryObject& out, std::string& why) {
    // ИМЯ ФАЙЛА ИЗ ТАБЛИЦЫ, ПАПКА — ИЗ КЛЮЧА: так проба и боевая печь читают
    // ОДИН И ТОТ ЖЕ чертёж по имени, и подмена не может случиться молча.
    const fs::path house = houses_dir / fs::path(item.house).filename();
    std::ifstream f(house, std::ios::binary);
    if (!f) {
        why = "не открылся чертёж " + house.string();
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    dfn::world::HouseGraph graph;
    const dfn::world::HouseIoResult io = dfn::world::read_house(ss.str(), graph);
    if (!io.ok) {
        why = std::string("чертёж не читается: ") + io.why;
        return false;
    }

    // ТО ЖЕ ТЕЛО И ТОТ ЖЕ СВЕТ, ЧТО У ЗАГРУЗЧИКА. Обе функции детерминированы
    // по построению (см. их заголовки), поэтому «запечено» и «построено по
    // месту» — это побайтово одно и то же, а не два похожих ответа.
    const dfn::world::HouseMesh built = dfn::world::build_house_mesh(graph, bevel_m);
    const std::vector<std::uint8_t> sky = dfn::world::bake_house_sky_visibility(built);
    for (const dfn::world::MeshFinding& mf : built.findings) {
        std::fprintf(stderr, "[мебель] %s e%u: %s\n", item.name,
                     static_cast<unsigned>(mf.element), mf.what.c_str());
    }

    out = RegistryObject{};
    out.name = item.name;
    // ВИД ОБЪЕКТА — "furniture", и это не украшение имени: россыпь сцены
    // смотрит на kind, чтобы решить, какое тело выдать объекту, а запечённая
    // постройка вообще мимо россыпи. Читателю достаточно НЕПУСТОГО списка
    // house, но вид говорит человеку в перечне, что он держит в руках.
    out.kind = "furniture";
    {
        char src[192];
        std::snprintf(src, sizeof(src), "furn:%s parts=%zu tris=%zu",
                      house.filename().string().c_str(),
                      built.parts.size(), built.triangle_count());
        out.source = src;
    }

    // Куски складываются по ПЛИТКЕ (поверхность, тон, самосвечение) — по тому
    // же ключу, по которому их складывает загрузчик в партию отрисовки. Карта,
    // а не вектор: ключ упорядочен, и две выпечки одного чертежа дают
    // побайтово один файл.
    // КЛЮЧ КУСКА — ПАРА (клетка, ИМЯ ВЕЩЕСТВА). Имя пусто у всего, что
    // вещества не назвало, и тогда ключ ведёт себя ровно как прежний
    // uint64: полка, испечённая без имён, режется на те же куски.
    std::map<std::pair<std::uint64_t, std::string>, std::size_t> at;
    for (const dfn::world::MeshPart& part : built.parts) {
        const dfn::world::Element* e = graph.element(part.element);
        if (e == nullptr) {
            continue;
        }
        if (const std::string no = refusal_of(graph, built, part, *e); !no.empty()) {
            why = "элемент e" + std::to_string(static_cast<unsigned>(e->id))
                + ": " + no;
            return false;
        }
        const dfn::world::HousePartTile tile =
            dfn::world::house_part_tile(graph, *e, part.mat_override,
                                        part.tone_override);
        // КУСОК РЕЖЕТСЯ ПО ВЕЩЕСТВУ, а не только по паре (волна 3). Пока
        // рецепт вещества не назвал, имя пусто и ключ — прежний, побитово:
        // хэш ключа обязан совпасть с тем, которым испечена сегодняшняя
        // полка, иначе куски разложились бы иначе и 35 файлов сменили бы
        // личность, не изменившись ни на вершину. Имя входит в ключ ОТДЕЛЬНО
        // от пары, потому что два вещества вправе делить одну клетку
        // (крашеная доска и некрашеная — та же плитка, разный блик).
        const std::uint64_t key = (static_cast<std::uint64_t>(tile.surface) << 8)
                                | (part.emissive ? (1ull << 16) : 0ull) | tile.tone;
        auto it = at.find({key, tile.material});
        if (it == at.end()) {
            HouseSubmesh sub;
            sub.surface = tile.surface;
            sub.tone = tile.tone;
            sub.emissive = part.emissive;
            sub.material = tile.material;
            // ШАГ ПОВТОРА — ИЗ ЗАПИСИ ВЕЩЕСТВА, и это первое место, где он
            // доезжает до файла. До волны 3 span_m жил в PartSkin, тратился
            // НА ВЫПЕЧКЕ и в .dfo не попадал вовсе: соломенная кровля
            // повторялась с шагом дощатой стены. 0 — «штатный шаг вещества»,
            // то есть ровно прежнее поведение у всего, что вещества не
            // назвало.
            if (!tile.material.empty()) {
                const dfn::core::MaterialTable& table = dfn::core::material_registry();
                const dfn::core::MaterialId id = table.find(tile.material);
                if (id != dfn::core::MATERIAL_NONE) {
                    sub.span_m = table.record(id).tile_span_m;
                }
            }
            out.house.push_back(std::move(sub));
            it = at.emplace(std::pair{key, tile.material}, out.house.size() - 1).first;
        }
        dfn::render::MeshData& mesh = out.house[it->second].mesh;
        const std::uint32_t colour = paint_of(graph, *e);
        // ДЕДУП В ПРЕДЕЛАХ ЧАСТИ, как у загрузчика: у одной части вершина
        // общая, у соседней — своя (у них разные нормали и разный материал).
        std::map<std::uint32_t, std::uint32_t> remap;
        for (std::uint32_t i = 0; i < part.index_count; ++i) {
            const std::uint32_t vi = built.indices[part.index_begin + i];
            auto rit = remap.find(vi);
            if (rit == remap.end()) {
                const dfn::world::HouseVertex& hv = built.vertices[vi];
                dfn::platform::Vertex pv{};
                pv.position = hv.pos;   // МЕСТНЫЕ координаты чертежа: объект
                pv.normal = hv.normal;  // не знает, где он будет стоять
                pv.uv = hv.uv;
                // ЗАПЕЧЁННАЯ НЕБЕСНАЯ ВИДИМОСТЬ — В АЛЬФУ, полной дозой.
                // ЧЕСТНО ВСЛУХ: дверь-доза DFN_HOUSE_AO ослабляет затемнение
                // построек в кадре, а запечённого предмета уже не достанет —
                // его альфа лежит в файле. Это цена выпечки, и она названа:
                // доза заведена, чтобы мерить «стало хуже» на ГОРОДЕ, а
                // предмет мебели ни одного её прогона не был предметом спора.
                const std::uint32_t a = vi < sky.size() ? sky[vi] : 255u;
                pv.color_rgba = (colour & 0x00FFFFFFu) | (a << 24);
                mesh.vertices.push_back(pv);
                rit = remap.emplace(vi,
                    static_cast<std::uint32_t>(mesh.vertices.size() - 1)).first;
            }
            mesh.indices.push_back(rit->second);
        }
    }
    if (out.house.empty()) {
        why = "чертёж не дал ни одного куска — имя, указывающее в пустоту";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    bool list_only = false;
    fs::path out_dir = "assets/objects/furniture";
    fs::path houses_dir = "assets/houses";
    // ШИРИНА ФАСКИ, МЕТРЫ. Ключ заведён РАДИ КОНТРОЛЬНОЙ РУКИ, а не ради
    // удобства: «фаска не двигает габарит» доказывается только двумя
    // выпечками в одном прогоне — нулевой и штатной, — а не сверкой с
    // перечнем прошлого коммита. Перечень прошлого коммита стареет вместе с
    // деревом (правило 54) и молчит о том, что за сутки поменялось помимо
    // фаски.
    float bevel_m = dfn::world::HOUSE_BEVEL_W_DEFAULT;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) {
            list_only = true;
        } else if (std::strcmp(argv[i], "--houses") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "[мебель] --houses просит папку — ОТКАЗ\n");
                return 2;
            }
            houses_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--bevel") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "[мебель] --bevel просит ширину в метрах — ОТКАЗ\n");
                return 2;
            }
            bevel_m = std::strtof(argv[++i], nullptr);
        } else if (argv[i][0] == '-') {
            std::fprintf(stderr, "[мебель] неизвестный ключ \"%s\" — ОТКАЗ\n", argv[i]);
            return 2;
        } else {
            out_dir = argv[i];
        }
    }
    if (list_only) {
        for (const Item& it : ITEMS) {
            std::printf("%-14s %s  <- %s\n", it.name, it.what, it.house);
        }
        std::printf("[мебель] %zu предмет(ов) в перечне печи\n",
                    sizeof(ITEMS) / sizeof(ITEMS[0]));
        return 0;
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "[мебель] не создаётся %s: %s\n",
                     out_dir.string().c_str(), ec.message().c_str());
        return 1;
    }

    // ПЕРЕЧЕНЬ ПОЛКИ — ЭТО ПРИЁМКА ВЛАДЕЛЬЦА («вижу в перечне»), поэтому он
    // пишется ЗДЕСЬ и только здесь, из ИЗМЕРЕННЫХ величин (measure_object), а
    // не из чисел рецепта: размер, переписанный рукой рядом с предметом, —
    // первое, что устареет молча при следующей перепечке.
    std::string index;
    index += "# ПОЛКА МЕБЕЛИ РЕЕСТРА ОБЪЕКТОВ (assets/objects/furniture)\n";
    index += "#\n";
    index += "# Предмет обстановки, запечённый ОДИН РАЗ: тело, свет и плитки\n";
    index += "# материала посчитаны печью dfn_furn_objects, а город и локация\n";
    index += "# ставят ССЫЛКУ — имя, позиция, поворот. Правка предмета — это\n";
    index += "# правка ЧЕРТЕЖА (assets/houses/furn-*.dfh) и перепечка этой\n";
    index += "# полки; править .dfo руками нельзя, он двоичный и хэшируется.\n";
    index += "#\n";
    index += "# ГАБАРИТЫ ЗАМЕРЕНЫ С САМОГО ОБЪЕКТА (render::measure_object),\n";
    index += "# метры, от начала координат предмета (северо-западный угол\n";
    index += "# пятна на полу). x/z — пятно, low/high — низ и верх.\n";
    index += "#\n";
    index += "# имя | x | z | низ | верх | треуг. | кусков | хэш | чертёж\n";

    std::size_t written = 0;
    for (const Item& item : ITEMS) {
        RegistryObject obj;
        std::string why;
        if (!bake_item(item, houses_dir, bevel_m, obj, why)) {
            std::fprintf(stderr, "[мебель] %s — ОТКАЗ: %s\n", item.name, why.c_str());
            return 1;
        }
        const fs::path path = out_dir / (std::string(item.name) + ".dfo");
        if (!dfn::render::write_object(obj, path)) {
            std::fprintf(stderr, "[мебель] не пишется %s\n", path.string().c_str());
            return 1;
        }
        // КРУГОВОЙ ПРОГОН ИЛИ ОТКАЗ.
        const std::uint64_t expect = dfn::render::object_content_hash(obj);
        const auto back = dfn::render::read_object(path);
        if (!back || back->content_hash != expect) {
            std::fprintf(stderr, "[мебель] %s не читается обратно — ОТКАЗ\n",
                         path.string().c_str());
            return 1;
        }
        const dfn::render::ObjectExtent ext = dfn::render::measure_object(*back);
        std::size_t tris = 0;
        for (const HouseSubmesh& s : back->house) {
            tris += s.mesh.indices.size() / 3;
        }
        char row[512];
        std::snprintf(row, sizeof(row),
                      "%-14s %5.2f %5.2f %6.2f %6.2f %6zu %3zu %016llx  %s\n",
                      item.name, static_cast<double>(ext.hi.x - ext.lo.x),
                      static_cast<double>(ext.hi.y - ext.lo.y),
                      static_cast<double>(ext.bottom), static_cast<double>(ext.top),
                      tris, back->house.size(),
                      static_cast<unsigned long long>(expect), item.what);
        index += row;
        std::printf("[мебель] %s -> %s\n"
                    "[мебель]   пятно %.2f x %.2f, низ %.2f, верх %.2f, "
                    "%zu треуг. в %zu куске(ах), хэш %016llx\n",
                    item.name, path.string().c_str(),
                    static_cast<double>(ext.hi.x - ext.lo.x),
                    static_cast<double>(ext.hi.y - ext.lo.y),
                    static_cast<double>(ext.bottom), static_cast<double>(ext.top),
                    tris, back->house.size(),
                    static_cast<unsigned long long>(expect));
        ++written;
    }

    const fs::path index_path = out_dir / "INDEX.md";
    std::ofstream idx(index_path, std::ios::binary | std::ios::trunc);
    if (!idx) {
        std::fprintf(stderr, "[мебель] не пишется %s\n", index_path.string().c_str());
        return 1;
    }
    idx << index;
    std::printf("[мебель] %zu предмет(ов) на полке, перечень в %s\n", written,
                index_path.string().c_str());
    return 0;
}
