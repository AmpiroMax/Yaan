/*
Created: 28:08:2026 - 11:14:50
Last updated: 28:08:2026 - 17:10:00
Module: tests
File: tests/render/MaterialRegistryTests.cpp

Responsibility:
- Реестр веществ, взятый ровно за те обещания, ради которых он заведён:
  (1) он ПОЛОН — все 36 пар листа набора получают вещество, то есть полка из
  2544 .dfo и вся полка домов доезжают через него без перепечки;
  (2) он НЕ ТРОГАЕТ КАДР по умолчанию — вещество, которое ничего не сказало,
  это ламберт, то есть покадрово то же, что было до волны;
  (3) он РАЗЛИЧАЕТ ВЕЩЕСТВА — золото отражает не так, как штукатурка, и это
  утверждение проверяется числом, а не глазом;
  (4) он ХРАНИТ ПЛИТКУ пары — выведенное вещество безымянной пары кроется той
  же клеткой атласа, что и раньше.
- ПОСЛЕ ВОЛНЫ 3 — ещё и мост: раскладка листа объявлена В ДАННЫХ, а ординалы
  живут в перечислениях render, и совпадение этих двух порядков есть предмет
  отдельной проверки.

Key items:
- полнота отображения (36 пар), контроль на неполноту;
- умолчания == ламберт (контрольная рука кадра);
- разделение gold-leaf / plaster-white по шероховатости и металличности;
- имена: поиск по имени, отказ на пустом и на несуществующем;
- ИЗМЕРЕНИЕ БЛИКА: та же арифметика, что в fs_prop, на золоте против
  отвергнутого образца (тот же цвет ламбертом) — правило 45;
- СВЕРКА РАСКЛАДКИ данных с ординалами атласа.

Dependencies:
- Uses: PartsMaterial.h (мост), engine/core/materials (реестр), PartsAtlas.h,
  doctest.
- Used by: tests/render.cmake (render_material_registry).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; дизайн зоны — docs/design/MATERIALS.md.
- У КАЖДОГО ПОРОГА ЗДЕСЬ НАЗВАН ОТВЕРГНУТЫЙ ОБРАЗЕЦ (правило 45). У блика
  золота отвергнутый образец — сегодняшний герб: тот же цвет, шероховатость 1,
  металличность 0. Он обязан не пройти.
- ДВА СЛУЧАЯ ЗДЕСЬ ЛОМАЮТСЯ МОЛЧА, И ИМЕННО ПОЭТОМУ У НИХ ЕСТЬ ВОРОТА
  registry_is_loaded(). До волны 3 таблица была constexpr-массивом в C++ и не
  могла не приехать; теперь она ФАЙЛ, и не приехавший файл делает всё вещество
  «не названным». Тогда «умолчание == ламберт» прошёл бы на пустом реестре, а
  «покрыты все 36 пар» — на одних выведенных записях. Ворота обязаны стоять
  ПЕРВОЙ строкой обоих случаев.
*/
/*
UPD:
- 28:08:2026 - 11:14:50: Создан вместе с реестром — дизайн-волна зоны МАТЕРИАЛЫ
  (заказ владельца 28.08).
- 28:08:2026 - 17:10:00: ПЕРЕЕЗД НА РЕЕСТР В ДАННЫХ (волна 3). Случаи те же и
  проверяют то же; сменился только адрес вещества — вместо C++-таблицы в
  engine/render запись приходит из assets/materials/yaan.dfmat через
  engine/core/materials. Добавлены ворота невакуозности (см. Notice) и случай
  про раскладку: имя, растолкованное против другой раскладки листа, перекрасило
  бы полку молча.
*/

#include "engine/core/materials/sources/MaterialRegistry.h"
#include "engine/render/sources/PartsAtlas.h"
#include "engine/render/sources/PartsMaterial.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <string_view>

using namespace dfn::render;
using dfn::core::MATERIAL_NONE;
using dfn::core::MaterialId;
using dfn::core::MaterialRecord;
using dfn::core::material_registry;

namespace {

/// ВОРОТА НЕВАКУОЗНОСТИ. См. Notice в шапке: реестр теперь файл, а не массив,
/// и утверждения «всё ламберт» / «все 36 пар покрыты» проходят на пустом
/// реестре, ничего при этом не значив.
///
/// Ворота проверяют ДВЕ вещи, и обе обязательны: таблица прочитана (записей
/// заметно больше нуля) И её раскладка совпадает с ординалами атласа. Второе
/// не менее важно первого: реестр, приехавший с переставленными колонками,
/// отвечает на каждый вопрос — и отвечает не про то вещество.
[[nodiscard]] bool registry_is_loaded() {
    const auto& table = material_registry();
    if (table.size() < 20) {
        return false;
    }
    std::string why;
    if (!parts_sheet_matches_atlas(table, &why)) {
        MESSAGE("раскладка реестра разошлась с атласом: " << why);
        return false;
    }
    return true;
}

/// БЛИК, СЧИТАННЫЙ ТОЙ ЖЕ АРИФМЕТИКОЙ, ЧТО В fs_prop.sc — и это осознанная
/// вторая копия, а не недосмотр правила 39. Шейдер компилируется в другом
/// языке и другим компилятором; тест, который позвал бы его, проверял бы
/// драйвер. Копия здесь ОБЯЗАНА совпадать формулой, и если fs_prop сменит
/// модель отражения, эта функция обязана поехать следом — о чём и написано
/// в fs_prop.sc над лепестком.
///
/// Возвращает яркость блика при заданной геометрии: n·h = ndoth.
///
/// НОРМИРОВКА ПО ПИКУ, как в шейдере: энергетическая (n+8)/(8π) выбивала блик
/// в белый на нашей LDR-цели и уничтожала цвет металла — измерено на кадре
/// герба, разбор в fs_prop.sc над `norm`. Проверки ниже это переживают без
/// правки, потому что все они об ОТНОШЕНИИ пика к склону, а отношение от
/// любой постоянной нормировки не зависит.
[[nodiscard]] float spec_lobe(float roughness, float ndoth) {
    const float a = std::max(roughness * roughness, 1e-3f);
    const float shininess = 2.0f / (a * a) - 2.0f;
    return std::pow(ndoth, shininess);
}

} // namespace

TEST_CASE("раскладка листа объявлена в данных и совпадает с атласом") {
    // САМОЕ ДЕШЁВОЕ МЕСТО, ГДЕ ЛОВИТСЯ САМАЯ ДОРОГАЯ ОШИБКА ВОЛНЫ. Реестр
    // называет клетку ИМЕНАМИ ("parts:stone/weathered"), ординал вычисляется
    // из объявленного в том же файле порядка колонок. Если этот порядок
    // разойдётся с PartSurface, каждое имя будет растолковано против чужой
    // раскладки — и вся полка перекрасится, не сменив ни одного байта и не
    // выдав ни одного сообщения.
    std::string why;
    const bool matches = parts_sheet_matches_atlas(material_registry(), &why);
    if (!matches) {
        MESSAGE("раскладка: " << why);
    }
    CHECK(matches);

    // Контроль прибора (правило 45): подсунутая таблица с переставленными
    // колонками обязана быть ОТВЕРГНУТА — иначе сверка выше проходила бы
    // всегда и не значила бы ничего.
    dfn::core::MaterialTable shifted;
    const std::string_view text =
        "sheet parts\n"
        "    columns = sawn-board hewn-timber end-grain stone fired-clay "
        "plaster thatch turf pane\n"
        "    rows = light mid dark weathered\n";
    (void)dfn::core::parse_material_table(text, "контроль", shifted, nullptr);
    CHECK_FALSE(parts_sheet_matches_atlas(shifted, nullptr));
}

TEST_CASE("реестр покрывает ВСЕ 36 пар листа набора") {
    // ВОРОТА НЕВАКУОЗНОСТИ ПЕРВОЙ СТРОКОЙ (см. Notice): без реестра ветка
    // выведенного вещества отвечает на все 36 пар сама, и случай прошёл бы,
    // не проверив ничего.
    REQUIRE(registry_is_loaded());

    // ЭТО И ЕСТЬ ОБРАТНАЯ СОВМЕСТИМОСТЬ ЧИСЛОМ. Секция HOUS формата v3 несёт
    // пару (surface, tone); в тот день, когда рисовальщик начнёт спрашивать
    // материал, любая пара без ответа — это кусок мебели, переставший
    // рисоваться. Полка из 2544 .dfo не может позволить себе дырку.
    int covered = 0;
    for (std::uint8_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        for (std::uint8_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            const auto surf = static_cast<PartSurface>(s);
            const auto tone = static_cast<PartTone>(t);
            const MaterialRecord m = material_of(surf, tone);
            // ПЛИТКА СОХРАНЕНА: выведенное вещество кроется ТОЙ ЖЕ клеткой.
            // Вещество, поменявшее клетку, перекрасило бы полку молча — ровно
            // та ловушка, о которой предупреждает ProcTexture.h про ординалы.
            CHECK(m.tiled);
            std::uint32_t col = 0;
            std::uint32_t row = 0;
            CHECK(material_registry().cell_of(m, col, row));
            CHECK(col == s);
            CHECK(row == t);
            ++covered;
        }
    }
    CHECK(covered == 36);

    // КОНТРОЛЬ НЕВАКУОЗНОСТИ (правило 30): проверка выше прошла бы и у реестра
    // из одних умолчаний. Требуем, чтобы ИМЕНОВАННЫЕ вещества среди этих 36
    // действительно нашлись — иначе «полнота» доказывала бы только то, что
    // функция всегда что-то возвращает.
    int named = 0;
    for (std::uint8_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        for (std::uint8_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            if (named_material_of(static_cast<PartSurface>(s),
                                  static_cast<PartTone>(t)) != MATERIAL_NONE) {
                ++named;
            }
        }
    }
    CHECK(named >= 6);
    // ...и чтобы имена НЕ покрывали всё: 28 пар безымянны по построению, и
    // если бы покрывали, ветка выведенного вещества не проверялась бы ничем.
    CHECK(named < 36);
}

TEST_CASE("материал, который ничего не сказал, — это сегодняшний ламберт") {
    // ВОРОТА НЕВАКУОЗНОСТИ ПЕРВОЙ СТРОКОЙ (см. Notice): на не приехавшем
    // реестре всё вещество безымянно и всё — ламберт, то есть случай прошёл
    // бы ровно тогда, когда доказывать нечего.
    REQUIRE(registry_is_loaded());

    // КОНТРОЛЬНАЯ РУКА ВСЕГО КАДРА (правило 47). fs_prop не входит в ветку
    // блика при roughness >= 0.999, а DrawParams умолчанием несёт материал
    // «не названо». Значит: MATERIAL_NONE и всякое выведенное вещество обязаны
    // давать 1/0, иначе «кадр бит-в-бит» — обещание, а не факт.
    const MaterialRecord& none = material_registry().record(MATERIAL_NONE);
    CHECK(none.roughness == doctest::Approx(1.0f));
    CHECK(none.metalness == doctest::Approx(0.0f));
    CHECK(none.emission.r == doctest::Approx(0.0f));
    CHECK_FALSE(none.tiled);
    CHECK(none.name.empty());

    // Тот же вопрос всем безымянным парам: они едут через вещество уже
    // сегодня (полка мебели), и любая из них с шероховатостью < 1 двинула бы
    // пиксели там, где волна обещала их не двигать.
    for (std::uint8_t s = 0; s < PARTS_ATLAS_SURFACES; ++s) {
        for (std::uint8_t t = 0; t < PARTS_ATLAS_TONES; ++t) {
            const auto surf = static_cast<PartSurface>(s);
            const auto tone = static_cast<PartTone>(t);
            if (named_material_of(surf, tone) != MATERIAL_NONE) {
                continue; // именованное вправе иметь блик — см. granite
            }
            const MaterialRecord m = material_of(surf, tone);
            CHECK(m.roughness == doctest::Approx(1.0f));
            CHECK(m.metalness == doctest::Approx(0.0f));
        }
    }

    // Идентификатор за пределами таблицы (файл из будущего) — тоже ламберт,
    // а не падение: read_object уже отказывает битым файлам.
    const MaterialRecord& far_future =
        material_registry().record(static_cast<MaterialId>(60000));
    CHECK(far_future.roughness == doctest::Approx(1.0f));
    CHECK(far_future.name.empty());
}

TEST_CASE("золото отражает не так, как штукатурка — и это число") {
    const MaterialId gold = material_registry().find("gold-leaf");
    const MaterialId plaster = material_registry().find("plaster-white");
    REQUIRE(gold != MATERIAL_NONE);
    REQUIRE(plaster != MATERIAL_NONE);

    const MaterialRecord& g = material_registry().record(gold);
    const MaterialRecord& p = material_registry().record(plaster);

    // МЕТАЛЛИЧНОСТЬ — ТО, ЧТО ОТЛИЧАЕТ ЗОЛОТО ОТ ЖЁЛТОЙ КРАСКИ. У краски блик
    // белый (f0 = 0.04), у золота — золотой (f0 = альбедо). Заказ владельца
    // назвал именно этот случай: «для золота/металла уже есть нужда — герб».
    CHECK(g.metalness == doctest::Approx(1.0f));
    CHECK(p.metalness == doctest::Approx(0.0f));

    // У ЗОЛОТА ЛЕПЕСТОК ЕСТЬ, У ШТУКАТУРКИ ЕГО НЕТ ВООБЩЕ, и мерить это надо
    // ПОЛУШИРИНОЙ, а не отношением в наугад взятой точке.
    //
    // ПОЧЕМУ НЕ ОТНОШЕНИЕМ «пик / 12 градусов», как здесь стояло сначала.
    // Порог «золото собраннее в 10 раз» был выведен при шероховатости 0.22 и
    // молча оказался утверждением ПРО ЭТО ЧИСЛО, а не про вещество: когда
    // развёртка по кадрам подняла золото до 0.60 (лепесток обязан быть шире
    // точечного источника), проверка покраснела, хотя ничего верного не
    // сломалось. Проверка, привязанная к значению параметра, стареет вместе с
    // ним; проверка про УСТРОЙСТВО — нет.
    //
    // Полуширина — угол, на котором лепесток падает вдвое от пика. Она
    // безразмерна, не зависит ни от яркости солнца, ни от нормировки, и у
    // ламберта её НЕ СУЩЕСТВУЕТ: при шероховатости 1 показатель равен нулю,
    // лепесток тождественно равен единице и не падает никогда.
    const auto half_angle_deg = [](float roughness) {
        for (int deg = 1; deg <= 90; ++deg) {
            const float c = std::cos(static_cast<float>(deg) * 3.14159265f / 180.0f);
            if (spec_lobe(roughness, c) < 0.5f) {
                return static_cast<float>(deg);
            }
        }
        return 180.0f;
    };

    const float gold_half = half_angle_deg(g.roughness);
    const float plaster_half = half_angle_deg(p.roughness);

    // У ЗОЛОТА ПОЛУШИРИНА ЕСТЬ И ОНА УЗКАЯ: блик — пятно, а не заливка.
    CHECK(gold_half < 30.0f);
    // У ШТУКАТУРКИ ЕЁ НЕТ ВОВСЕ — ламберт не собирает свет никуда.
    CHECK(plaster_half == doctest::Approx(180.0f));

    // ОТВЕРГНУТЫЙ ОБРАЗЕЦ — СЕГОДНЯШНИЙ ГЕРБ (правило 45): тот же золотой
    // цвет, но шероховатость 1 и металличность 0, то есть ровно то, чем герб
    // нарисован на полке. Он обязан НЕ пройти проверку выше — иначе она
    // прошла бы и для мира без материалов, и не значила бы ничего.
    CHECK_FALSE(half_angle_deg(1.0f) < 30.0f);

    // И ЦВЕТ ЗОЛОТА ВЗЯТ С САМОГО ГЕРБА, а не выдуман: доминирующий цвет
    // вершин oak-seal-relief.dfo — rgb(199,153,56) на 72039 вершинах.
    // Волна про БЛИК, и перекрасить принятый владельцем предмет она не смеет.
    // ПЕРЕЕЗД В ДАННЫЕ ЭТО ЧИСЛО ТОЖЕ ПРОВЕРЯЕТ: файл записал его десятичной
    // дробью, и если бы разбор потерял точность, здесь бы это и всплыло.
    CHECK(g.tint.r == doctest::Approx(199.0f / 255.0f));
    CHECK(g.tint.g == doctest::Approx(153.0f / 255.0f));
    CHECK(g.tint.b == doctest::Approx(56.0f / 255.0f));
}

TEST_CASE("имена — рукоятка, порядковый — личность") {
    REQUIRE(registry_is_loaded());
    // Имя есть у каждой записи, кроме нулевой, и все имена РАЗНЫЕ: два
    // вещества под одним именем сделали бы поиск по имени случайным.
    std::set<std::string_view> seen;
    const auto table = material_registry().records();
    REQUIRE(table.size() >= 2);
    CHECK(table[MATERIAL_NONE].name.empty());
    for (std::size_t i = 1; i < table.size(); ++i) {
        CHECK_FALSE(table[i].name.empty());
        CHECK(seen.insert(table[i].name).second);
        // Поиск по имени обязан вернуть ЭТУ ЖЕ запись: реестр, у которого
        // имя и порядковый расходятся, индексирует не то, что думает.
        CHECK(material_registry().find(table[i].name) == static_cast<MaterialId>(i));
    }

    // Пустое имя и небывалое имя — оба «не названо», а не первая запись:
    // опечатка в рецепте не имеет права молча стать дубом.
    CHECK(material_registry().find("") == MATERIAL_NONE);
    CHECK(material_registry().find("не-такого-вещества") == MATERIAL_NONE);

    // ВЛАДЕЛЕЦ НАЗВАЛ ЭТИ ВЕЩЕСТВА ПОИМЁННО (заказ 28.08). Список — часть
    // приёмки волны, а не украшение: реестр без них не отвечает на заказ.
    for (const std::string_view wanted :
         {"oak-log", "brick-red", "granite", "plaster-white", "thatch", "iron",
          "linen"}) {
        CHECK(material_registry().find(wanted) != MATERIAL_NONE);
    }
}

TEST_CASE("вещества письма заведены с первого дня") {
    REQUIRE(registry_is_loaded());
    // ТЗ владельца §7 требует их именно так: «в реестр с первого дня». Причина
    // не в книгах — читаемость и письмо игрока это отдельная зона, — а в том,
    // что вещество, которого нет, заставляет предмет ПРИТВОРЯТЬСЯ. На полке
    // мебели такое притворство уже стоит девяти обходов (стекло ходит глухим
    // окном, мешковина соломой), и каждый из девяти придётся не дорабатывать,
    // а ОТМЕНЯТЬ.
    for (const std::string_view wanted :
         {"birch-bark", "parchment", "rag-paper", "seal-wax", "ink"}) {
        CHECK(material_registry().find(wanted) != MATERIAL_NONE);
    }

    // НИ У ОДНОГО ИЗ НИХ НЕТ ПЛИТКИ, и это утверждение, а не умолчание: у листа
    // набора девять колонок, и ни одна из них не пергамент.
    for (const std::string_view wanted :
         {"birch-bark", "parchment", "rag-paper", "seal-wax", "ink"}) {
        CHECK_FALSE(
            material_registry().record(material_registry().find(wanted)).tiled);
    }

    // ВОСК ПЕЧАТИ СОБИРАЕТ БЛИК, БУМАГА — НЕТ. Контроль (правило 30): проверка
    // выше прошла бы и у пяти одинаковых записей, эта — нет.
    CHECK(material_registry().record(material_registry().find("seal-wax")).roughness
          < material_registry().record(material_registry().find("rag-paper")).roughness);

    // И НИ ОДНО ИЗ ПЯТИ НЕ МЕТАЛЛ: воск блестит по-диэлектрически (белый блик),
    // а не как золото. Металличность здесь — та самая разница.
    for (const std::string_view wanted :
         {"birch-bark", "parchment", "rag-paper", "seal-wax", "ink"}) {
        CHECK(material_registry().record(material_registry().find(wanted)).metalness
              == doctest::Approx(0.0f));
    }
}

TEST_CASE("кровать с полки читается материалами, и один её кусок безымянен") {
    REQUIRE(registry_is_loaded());
    // ПРОВЕРКА НА НАСТОЯЩЕМ ПРЕДМЕТЕ. assets/objects/furniture/furn-bed.dfo —
    // единственный .dfo с непустой секцией HOUS, и его три куска это
    // (HewnTimber, Dark), (SawnBoard, Mid) и (SawnBoard, Dark). Первые два
    // обязаны иметь ИМЯ — иначе рецепт по-прежнему не сможет сказать «дубовое
    // бревно» и волна не сдвинула заказ; третий обязан НЕ иметь — иначе
    // ветка выведенного вещества не проверяется здесь ничем.
    CHECK(named_material_of(PartSurface::HewnTimber, PartTone::Dark)
          == material_registry().find("oak-log"));
    CHECK(named_material_of(PartSurface::SawnBoard, PartTone::Mid)
          == material_registry().find("pine-board"));
    CHECK(named_material_of(PartSurface::SawnBoard, PartTone::Dark)
          == MATERIAL_NONE);

    // И безымянный кусок всё равно кроется своей клеткой и своим износом:
    // тёмный ряд — это выветренное дерево, а не «материал не найден».
    const MaterialRecord derived = material_of(PartSurface::SawnBoard, PartTone::Dark);
    CHECK(derived.tiled);
    CHECK(derived.cell_column == part_surface_name(PartSurface::SawnBoard));
    CHECK(derived.cell_row == part_tone_name(PartTone::Dark));
    CHECK(derived.wear > material_of(PartSurface::SawnBoard, PartTone::Light).wear);
}

TEST_CASE("тайлинг — свойство вещества, а не константа набора") {
    REQUIRE(registry_is_loaded());
    // ПЕРВОЕ ПОЛЕ, РАДИ КОТОРОГО tile_span_m ОБЯЗАН ДОЕХАТЬ ДО .dfo. До волны 3
    // он жил в PartSkin у кузницы и до файла не доходил вовсе, поэтому
    // соломенная кровля повторяется с тем же метровым шагом, что дощатая
    // стена, — и читается как ковёр. Солома обязана быть КРУПНЕЕ доски.
    const MaterialRecord& thatch =
        material_registry().record(material_registry().find("thatch"));
    const MaterialRecord& board =
        material_registry().record(material_registry().find("pine-board"));
    CHECK(thatch.tile_span_m < board.tile_span_m);
    // Контроль (правило 30): у доски шаг — ровно набора, то есть волна не
    // трогала того, о чём не говорила.
    CHECK(board.tile_span_m == doctest::Approx(PARTS_TILE_SPAN_M));
}

TEST_CASE("программу выбирает ВЕЩЕСТВО, а не поток файла") {
    REQUIRE(registry_is_loaded());
    // §2.4 дизайна: «материал решает всё» вместо «поток решает программу».
    // До волны 3 куда попадёт геометрия решалось тем, в какой поток .dfo её
    // положила кузница, — и потому, чтобы брус стал кирпичом, надо было
    // перепечь меш. Здесь проверяется, что решение принимается ПО ЗАПИСИ.
    MaterialRecord stone;
    CHECK(material_program(stone) == "prop");

    MaterialRecord leaf;
    leaf.motion = dfn::core::MaterialMotion::Foliage;
    CHECK(material_program(leaf) == "foliage");

    // ВЫРЕЗАЕМОЕ — ТОЖЕ СВОЙСТВО ВЕЩЕСТВА, а не «это лист»: решётка и ставня
    // вырезаются, не качаясь. Контроль (правило 30): если бы программу
    // выбирало одно только качание, этот случай вернул бы prop.
    MaterialRecord grate;
    grate.opacity = dfn::core::MaterialOpacity::Cutout;
    CHECK(material_program(grate) == "foliage");

    // И ОТВЕРГНУТЫЙ ОБРАЗЕЦ ВСЕЙ ЗАТЕИ (правило 45): вещество из полки —
    // дубовое бревно — обязано ехать СПЛОШНОЙ программой, хотя до волны его
    // клетка жила в атласе, который читает foliage. Программа перестала
    // значить «это лист».
    CHECK(material_program(material_of(PartSurface::HewnTimber, PartTone::Dark))
          == "prop");
}
