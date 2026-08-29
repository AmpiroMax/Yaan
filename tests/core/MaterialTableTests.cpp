/*
Module: tests
File: tests/core/MaterialTableTests.cpp

Responsibility:
- РЕЕСТР КАК ДАННЫЕ, взятый за четыре обещания волны 3:
  (1) он ЗАГРУЖАЕТСЯ — файл в assets/materials читается, и в нём есть то, что
      заказал владелец;
  (2) он ОТКАЗЫВАЕТ ВСЛУХ — неизвестное имя, неизвестное поле, клетка вне
      объявленного листа и не-число не превращаются молча во что-нибудь;
  (3) ЛИЧНОСТЬ ЗАПИСИ — ЕЁ СОДЕРЖИМОЕ: хэш не зависит от места записи в файле
      и меняется от любой правки поля;
  (4) ФИЗИКА И ОБЛИК — ОДИН СПИСОК: числа реестра совпадают с таблицей
      PhysicsSubstance.h до последнего разряда.

Key items:
- загрузка штатного файла и его состав;
- отказ на неизвестном имени (и контроль: известное имя находится);
- разбор — рукав, а не приговор: битая строка даёт находку, а не пустую таблицу;
- хэш записи: устойчив к перестановке, чувствителен к правке поля;
- сверка с core::SUBSTANCES — ворота против расползания двух реестров.

Dependencies:
- Uses: engine/core/materials/sources/MaterialRegistry.h,
  engine/core/materials/sources/PhysicsSubstance.h, doctest.
- Used by: tests/core.cmake (core_material_table).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; дизайн зоны — docs/design/MATERIALS.md.
- СЛУЧАЙ «ФИЗИКА И ОБЛИК — ОДИН СПИСОК» — ЭТО ВОРОТА, А НЕ УКРАШЕНИЕ. Пока
  зона физики берёт числа из своего массива, а зона облика — из файла, два
  списка разойдутся в первый же день, когда в один добавят вещество, а в
  другой забудут. Случай обязан жить до тех пор, пока PhysicsSubstance.h не
  начнёт спрашивать реестр; удалять его вместе с массивом, а не раньше.
*/

#include "engine/core/materials/sources/MaterialRegistry.h"
#include "engine/core/materials/sources/PhysicsSubstance.h"

#include <doctest/doctest.h>

#include <set>
#include <string>
#include <string_view>

using namespace dfn::core;

TEST_CASE("штатный файл реестра читается, и в нём есть заказанное") {
    std::vector<std::string> errors;
    const auto loaded =
        load_material_table(default_material_registry_path(), &errors);
    REQUIRE(loaded.has_value());
    // РАЗБОР ШТАТНОГО ФАЙЛА ОБЯЗАН БЫТЬ ЧИСТЫМ. Находка здесь — это ошибка в
    // данных, поставляемых с игрой, и её нельзя оставлять «до случая».
    for (const std::string& e : errors) {
        MESSAGE("находка разбора: " << e);
    }
    CHECK(errors.empty());

    const MaterialTable& table = *loaded;
    // ВЛАДЕЛЕЦ НАЗВАЛ ЭТИ ВЕЩЕСТВА ПОИМЁННО (заказ 28.08) плюс пять веществ
    // письма из ТЗ §7. Список — часть приёмки, а не украшение.
    for (const std::string_view wanted :
         {"oak-log", "pine-board", "brick-red", "granite", "plaster-white",
          "thatch", "linen", "iron", "gold-leaf", "birch-bark", "parchment",
          "rag-paper", "seal-wax", "ink"}) {
        CHECK(table.find(wanted) != MATERIAL_NONE);
    }

    // ЛИСТ ОБЪЯВЛЕН В ТЕХ ЖЕ ДАННЫХ. Ядро не знает ни одного атласа — оно
    // знает то, что сказал файл; ординал клетки вычисляется отсюда.
    const MaterialSheet* parts = table.sheet("parts");
    REQUIRE(parts != nullptr);
    CHECK(parts->columns.size() == 9);
    CHECK(parts->rows.size() == 4);

    // И КООРДИНАТА РАЗБИРАЕТСЯ В ЧИСЛА. Гранит — камень выветренного ряда;
    // это же число поедет в старую секцию HOUS.
    std::uint32_t col = 0;
    std::uint32_t row = 0;
    REQUIRE(table.cell_of(table.record(table.find("granite")), col, row));
    CHECK(col == 3); // stone
    CHECK(row == 3); // weathered
}

TEST_CASE("неизвестное имя — это отказ, а не первая запись") {
    const MaterialTable& table = material_registry();
    // ГЛАВНЫЙ ДЕФЕКТ, РАДИ КОТОРОГО ЗАВЕДЕНЫ ИМЕНА: `mat=17` молча становится
    // `17 % 9 = 8` = глухое окно, то есть опечатка застекляет стену без
    // единого сообщения. Имя обязано вести себя иначе.
    CHECK(table.find("не-такого-вещества") == MATERIAL_NONE);
    CHECK(table.find("") == MATERIAL_NONE);
    CHECK(table.find("OAK-LOG") == MATERIAL_NONE); // регистр значим
    // Контроль (правило 45): отвергнутому образцу противопоставлено имя,
    // которое ОБЯЗАНО находиться, иначе проверка выше прошла бы у реестра,
    // не находящего вообще ничего.
    CHECK(table.find("oak-log") != MATERIAL_NONE);
}

TEST_CASE("разбор — рукав, а не приговор") {
    // Правило 29 (вечер заставы): прибор печатает ЧИСЛА, а не «прошло/не
    // прошло». Битая строка обязана дать НАХОДКУ и не отменить остальной файл:
    // реестр, падающий целиком от одной опечатки, заставит править вслепую.
    const std::string_view text =
        "sheet parts\n"
        "    columns = hewn-timber sawn-board end-grain stone fired-clay "
        "plaster thatch turf pane\n"
        "    rows = light mid dark weathered\n"
        "material good-one\n"
        "    cell = parts:stone/mid\n"
        "    roughness = 0.5\n"
        "material broken-one\n"
        "    roughness = не-число\n"
        "    неизвестное_поле = 1\n"
        "    cell = parts:нет-такой-колонки/mid\n";
    std::vector<std::string> errors;
    MaterialTable table;
    const bool clean = parse_material_table(text, "проба", table, &errors);
    CHECK_FALSE(clean);
    // Три находки: не-число, неизвестное поле, клетка вне листа.
    CHECK(errors.size() >= 3);
    // ...и при этом здоровая запись цела и разобрана до конца.
    const MaterialId good = table.find("good-one");
    REQUIRE(good != MATERIAL_NONE);
    CHECK(table.record(good).roughness == doctest::Approx(0.5f));
    // Контроль (правило 30): чистый файл обязан пройти БЕЗ находок, иначе
    // «находки есть» ничего не значило бы.
    std::vector<std::string> clean_errors;
    MaterialTable clean_table;
    CHECK(parse_material_table("material solo\n    roughness = 1.0\n", "проба2",
                               clean_table, &clean_errors));
    CHECK(clean_errors.empty());
}

TEST_CASE("клетка вне объявленного листа — находка, а не тихая перекраска") {
    // Это тот же дефект `mat=17 % 9`, только на новом языке: имя колонки,
    // которой в листе нет. Молчаливая подстановка нулевой колонки перекрасила
    // бы предмет и не сказала бы ни слова.
    const std::string_view text =
        "sheet parts\n"
        "    columns = hewn-timber stone\n"
        "    rows = light mid\n"
        "material ghost\n"
        "    cell = parts/выдумка\n"; // и разделитель тоже не тот
    std::vector<std::string> errors;
    MaterialTable table;
    CHECK_FALSE(parse_material_table(text, "проба", table, &errors));
    CHECK_FALSE(errors.empty());
    // Запись есть, но клетки у неё нет — и cell_of говорит об этом прямо.
    const MaterialId ghost = table.find("ghost");
    REQUIRE(ghost != MATERIAL_NONE);
    std::uint32_t col = 0;
    std::uint32_t row = 0;
    CHECK_FALSE(table.cell_of(table.record(ghost), col, row));
}

TEST_CASE("личность записи — её содержимое, а не место в файле") {
    // ЗАЧЕМ ЭТО ВООБЩЕ. Хэш записи — то, чем кэш плиток, запечённая полка и
    // сторож перепечки отвечают на вопрос «то же ли это вещество». Если бы он
    // зависел от порядка строк в файле, перестановка двух абзацев в текстовом
    // редакторе объявила бы всю полку протухшей.
    const std::string_view a =
        "material alpha\n    roughness = 0.5\n"
        "material beta\n    roughness = 0.9\n";
    const std::string_view b = // те же записи, порядок обратный
        "material beta\n    roughness = 0.9\n"
        "material alpha\n    roughness = 0.5\n";
    MaterialTable ta;
    MaterialTable tb;
    CHECK(parse_material_table(a, "a", ta, nullptr));
    CHECK(parse_material_table(b, "b", tb, nullptr));
    const std::uint64_t alpha_a = ta.record(ta.find("alpha")).content_hash;
    const std::uint64_t alpha_b = tb.record(tb.find("alpha")).content_hash;
    CHECK(alpha_a == alpha_b);
    // ...а номер при этом разный, и это ровно причина, по которой на диск
    // едет имя, а не номер.
    CHECK(ta.find("alpha") != tb.find("alpha"));

    // И ЧУВСТВИТЕЛЬНОСТЬ (правило 45, отвергнутый образец): правка ЛЮБОГО
    // поля обязана сменить личность. Иначе хэш, устойчивый к перестановке,
    // был бы устойчив и к смене вещества — то есть бесполезен.
    MaterialTable tc;
    CHECK(parse_material_table("material alpha\n    roughness = 0.51\n", "c", tc,
                               nullptr));
    CHECK(tc.record(tc.find("alpha")).content_hash != alpha_a);

    // Личность всей таблицы — свёртка личностей записей, и она отличает
    // таблицу из двух записей от таблицы из одной.
    CHECK(ta.table_hash() != tc.table_hash());
    // Контроль: пустая таблица (одна нулевая запись) не равна ни одной из них.
    CHECK(MaterialTable{}.table_hash() != ta.table_hash());
}

TEST_CASE("наследование физики: вещество письма весит своим сырьём") {
    const MaterialTable& table = material_registry();
    const MaterialId oak_log = table.find("oak-log");
    const MaterialId oak = table.find("oak");
    REQUIRE(oak_log != MATERIAL_NONE);
    REQUIRE(oak != MATERIAL_NONE);
    // Ключ substance — это НЕ копия чисел, а ссылка: дубовое бревно весит
    // ровно столько, сколько весит дуб, и второй таблицы плотностей нет.
    CHECK(table.record(oak_log).density_kg_m3
          == doctest::Approx(table.record(oak).density_kg_m3));
    CHECK(table.record(oak_log).surface_tag == table.record(oak).surface_tag);
    // Контроль (правило 30): вещество, ссылки не поставившее, наследования не
    // получает — иначе проверка выше прошла бы у реестра, где всё одинаково.
    // Позолота — ПОКРЫТИЕ, и плотности у неё нет нарочно (вопрос В3 отложен).
    const MaterialId gold = table.find("gold-leaf");
    REQUIRE(gold != MATERIAL_NONE);
    CHECK(table.record(gold).density_kg_m3
          != doctest::Approx(table.record(oak).density_kg_m3));
}

TEST_CASE("физика и облик — ОДИН список веществ") {
    // ВОРОТА ПРОТИВ ДВУХ РЕЕСТРОВ (см. Notice). До волны 3 существовали два
    // ответа на вопрос «какие у нас есть вещества»: PhysicsSubstance.h в core
    // и таблица облика в render. Волна свела их в один файл данных; пока
    // массив PhysicsSubstance.h ещё стоит на месте (его читает зона физики, и
    // переключать её чужой волной нельзя), единственное, что держит их
    // вместе, — эта проверка.
    const MaterialTable& table = material_registry();
    REQUIRE(table.size() > 20);

    // Нулевая запись — «не сказано» с обеих сторон: числа Jolt по умолчанию.
    const PhysicsSubstance& def = substance(SUBSTANCE_DEFAULT);
    const MaterialRecord& none = table.record(MATERIAL_NONE);
    CHECK(none.density_kg_m3 == doctest::Approx(def.density_kg_m3));
    CHECK(none.friction == doctest::Approx(def.friction));
    CHECK(none.restitution == doctest::Approx(def.restitution));
    CHECK(none.surface_tag == def.surface_tag);

    int checked = 0;
    for (std::size_t i = 1; i < substances().size(); ++i) {
        const PhysicsSubstance& s = substances()[i];
        // ОДНО ИМЯ РАЗОШЛОСЬ НАРОЧНО, И ЭТО ЗАПИСАНО В ДАННЫХ: физическое
        // "parchment" (сырьё) против вещества письма "parchment" (выделанный
        // лист) — одно имя на два разных ответа было бы той самой подделкой,
        // ради отмены которой волна и затевалась. В реестре сырьё зовётся
        // parchment-stock, и лист письма ссылается на него.
        const std::string_view name =
            s.name == "parchment" ? std::string_view{"parchment-stock"} : s.name;
        const MaterialId id = table.find(name);
        INFO("вещество физики: " << s.name);
        REQUIRE(id != MATERIAL_NONE);
        const MaterialRecord& rec = table.record(id);
        CHECK(rec.density_kg_m3 == doctest::Approx(s.density_kg_m3));
        CHECK(rec.friction == doctest::Approx(s.friction));
        CHECK(rec.restitution == doctest::Approx(s.restitution));
        CHECK(rec.surface_tag == s.surface_tag);
        CHECK(rec.brittle_kj_m2 == doctest::Approx(s.brittle_kj_m2));
        ++checked;
    }
    // Контроль невакуозности (правило 30): цикл выше прошёл бы и по пустому
    // списку. Веществ физики двадцать шесть сверх нулевого.
    CHECK(checked == 26);
}

TEST_CASE("шкала полей вещества не ограничена вкусом пояса") {
    // ГРАНИЦА СТИЛЯ (§2.14): «зеркальный хром в Яане не встречается» — строка
    // в паспорте пояса; «зеркального хрома не бывает» — дефект движка. Запрет,
    // встроенный в движок, придётся ломать в первый же день, когда понадобится
    // волшебный предмет или чужая земля.
    const std::string_view text =
        "material chrome\n"
        "    roughness = 0.0\n"
        "    metalness = 1.0\n"
        "    emission = 4.0 4.0 4.0\n";
    MaterialTable table;
    std::vector<std::string> errors;
    CHECK(parse_material_table(text, "проба", table, &errors));
    CHECK(errors.empty());
    const MaterialRecord& chrome = table.record(table.find("chrome"));
    CHECK(chrome.roughness == doctest::Approx(0.0f));
    CHECK(chrome.metalness == doctest::Approx(1.0f));
    CHECK(chrome.emission.r == doctest::Approx(4.0f)); // без потолка
}

TEST_CASE("разброс экземпляра — амплитуда у вещества, значение не здесь") {
    // Р3 (записка №5): «разброс» полем-ЗНАЧЕНИЕМ отрицал бы критерий К6.
    // Кузница детерминирована, личность объекта — хэш содержимого; держи
    // реестр само значение, все четыре стула прочли бы одно число и остались
    // побитово одинаковыми. Реестр несёт АМПЛИТУДУ, значение берётся из семени
    // сцены. Здесь проверяется, что поле именно амплитуда и по умолчанию — 0,
    // то есть сегодняшний детерминированный кадр.
    const MaterialTable& table = material_registry();
    for (const MaterialRecord& rec : table.records()) {
        CHECK(rec.scatter_amp >= 0.0f);
        CHECK(rec.wear_amp >= 0.0f);
    }
    CHECK(table.record(MATERIAL_NONE).scatter_amp == doctest::Approx(0.0f));
    // Контроль: поле ЧИТАЕТСЯ из данных, а не всегда ноль.
    MaterialTable probe;
    CHECK(parse_material_table("material weathered-board\n    scatter_amp = 0.12\n",
                               "проба", probe, nullptr));
    CHECK(probe.record(probe.find("weathered-board")).scatter_amp
          == doctest::Approx(0.12f));
}
