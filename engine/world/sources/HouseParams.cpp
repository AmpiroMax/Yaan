/*
Module: engine/world
File: engine/world/sources/HouseParams.cpp

Responsibility:
- ЧИСЛА ЭЛЕМЕНТА: лексер строки стиля, таблица числовых полей (одна на
  лексер и слияние), слияние поле-поверх-стиля, вид заполнения, грани
  профиля.

Key items:
- parse_element_params / element_params_of / param_slots / fill_kind /
  profile_sides.

Dependencies:
- Uses: HouseMeshDetail.h
- Used by: сборка build_house_mesh (HouseMesh.cpp) и соседние модули постройки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/

#include "engine/world/sources/HouseMeshDetail.h"

// РЕЕСТР ВЕЩЕСТВ В ЯДРЕ. Правило 1 соблюдено: world смотрит в core и только
// в core; про атлас, программу и плитку он по-прежнему не знает ничего.
#include "engine/core/materials/sources/MaterialRegistry.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace dfn::world {

namespace {

/// Число из токена. Отказ на мусор, БЕЗ подмены нулём: молчаливый ноль в
/// радиусе — это бревно толщиной с волос, которое никто не связал бы с
/// опечаткой в свойствах.
bool parse_float_token(std::string_view s, float& out) {
    if (s.empty() || s.size() > 63) {
        return false;
    }
    char buf[64];
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    char* end = nullptr;
    const double v = std::strtod(buf, &end);
    if (end == buf || *end != '\0') {
        return false;
    }
    out = static_cast<float>(v);
    return true;
}

} // namespace

struct ParamSlot {
    std::string_view key;
    float ElementParams::*field;
};

/// ЧИСЛОВЫЕ ПОЛЯ ПАРАМЕТРОВ — ОДНА таблица на лексер строки и на слияние
/// поле-поверх-стиля (аудит #3: их было две, рукописная копия отставала).
std::span<const ParamSlot> param_slots() {
    static const ParamSlot slots[] = {
        {"radius", &ElementParams::radius},   {"length", &ElementParams::length},
        {"angle_x", &ElementParams::angle_x}, {"angle_y", &ElementParams::angle_y},
        {"angle_z", &ElementParams::angle_z}, {"thickness", &ElementParams::thickness},
        {"height", &ElementParams::height},   {"tex_deg", &ElementParams::tex_deg},
        {"clad", &ElementParams::clad},       {"windows", &ElementParams::windows},
        {"fill", &ElementParams::fill},       {"doors", &ElementParams::doors},
        {"stairs", &ElementParams::stairs},   {"wear", &ElementParams::wear},
        {"logends", &ElementParams::logends}, {"shutters", &ElementParams::shutters},
        {"porch", &ElementParams::porch},     {"plinth", &ElementParams::plinth},
        {"roof", &ElementParams::roof},       {"unsupported", &ElementParams::unsupported},
        {"open", &ElementParams::open},       {"beams", &ElementParams::beams},
        {"glow", &ElementParams::glow},       {"rise", &ElementParams::rise},
        {"nosing", &ElementParams::nosing}, {"riser", &ElementParams::riser},
    };
    return slots;
}

ElementParams parse_element_params(std::string_view style, std::vector<ParamIssue>* issues) {
    const auto slots = param_slots();

    ElementParams p;
    std::size_t pos = 0;
    bool first = true;
    while (pos <= style.size()) {
        const std::size_t sep = style.find(';', pos);
        const std::string_view tok =
            style.substr(pos, sep == std::string_view::npos ? std::string_view::npos : sep - pos);
        pos = sep == std::string_view::npos ? style.size() + 1 : sep + 1;
        if (tok.empty()) {
            first = false;
            continue;
        }
        const std::size_t eq = tok.find('=');
        if (eq == std::string_view::npos) {
            // Голое слово имеет смысл ТОЛЬКО первым: это имя стиля. Второе
            // голое слово — почти наверняка забытый ключ, и молчать про него
            // значит применить не то, что просили.
            if (first) {
                p.name = std::string(tok);
            } else if (issues != nullptr) {
                issues->push_back({std::string(tok), "свойство без имени ключа"});
            }
            first = false;
            continue;
        }
        first = false;
        const std::string_view key = tok.substr(0, eq);
        const std::string_view val = tok.substr(eq + 1);
        float number = 0.0f;
        const bool numeric = parse_float_token(val, number);

        if (key == "form") {
            if (val == "round") {
                p.form = LineForm::Round;
            } else if (val == "square") {
                p.form = LineForm::Square;
            } else if (val == "ngon") {
                p.form = LineForm::Ngon;
            } else if (val == "plank") {
                p.form = LineForm::Plank;
            } else if (issues != nullptr) {
                issues->push_back({std::string(tok),
                                   "форма бывает round, square, ngon или plank"});
            }
            continue;
        }
        if (key == "n" || key == "sides") {
            if (numeric) {
                p.sides = static_cast<int>(number);
                p.form = LineForm::Ngon;
            } else if (issues != nullptr) {
                issues->push_back({std::string(tok), "число граней не число"});
            }
            continue;
        }
        bool matched = false;
        for (const ParamSlot& s : slots) {
            if (key != s.key) {
                continue;
            }
            matched = true;
            if (numeric) {
                p.*(s.field) = number;
            } else if (issues != nullptr) {
                issues->push_back({std::string(tok), "значение не число"});
            }
            break;
        }
        if (!matched && issues != nullptr) {
            // СВОЙСТВА ДРУГИХ СЛОЁВ — НЕ ОШИБКА. Материал (mat/tone), дверь
            // (door/hinge) и запертость обхода читает редактор и отрисовка;
            // геометрию они не меняют, и построитель их не знает ПО ПРАВУ.
            // Ругаться на них значило бы печатать «неизвестное свойство» на
            // каждую дверь — и приучить всех не читать находки вовсе.
            // `portal` — ключ ПЕРЕХОДА (И15): створка с portal=1 входит в
            // коллайдер, то есть запечатывает оболочку. Читает его App при
            // заливке, как и door/hinge; геометрию он не меняет, и
            // построитель не знает его ПО ПРАВУ.
            // `material` — ИМЯ ВЕЩЕСТВА (волна 3 зоны МАТЕРИАЛЫ). Чужой ключ
            // для строителя геометрии по той же причине, что и `mat`: из
            // чего сделана деталь, её форму не меняет. Читает его
            // house_part_tile.
            const bool foreign = key == "mat" || key == "tone" || key == "door"
                              || key == "hinge" || key == "paint"
                              || key == "portal" || key == "material";
            if (!foreign) {
                issues->push_back({std::string(tok), "неизвестное свойство"});
            }
        }
    }
    return p;
}

WallFill fill_kind(const ElementParams& p) {
    switch (static_cast<int>(p.fill)) {
    case 2: return WallFill::Brick;
    case 3: return WallFill::Block;
    case 4: return WallFill::Logs;
    case 5: return WallFill::Parquet;
    case 6: return WallFill::Stairs;
    case 7: return WallFill::Shingle;
    case 8: return WallFill::Tile;
    default: return WallFill::Plain;
    }
}

int profile_sides(const ElementParams& p) {
    if (p.form == LineForm::Square || p.form == LineForm::Plank) {
        return 4;
    }
    return p.sides >= 3 ? p.sides : HOUSE_ROUND_SIDES;
}

// ---------------------------------------------------------------------------
// Плоскость контура
// ---------------------------------------------------------------------------

ElementParams element_params_of(const Element& e, std::vector<ParamIssue>* issues) {
    ElementParams p = parse_element_params(e.style, issues);
    for (const auto& kv : e.params) {
        // ТА ЖЕ ТАБЛИЦА, ЧТО У ЛЕКСЕРА (аудит #3, находка 5: рукописная
        // цепочка else-if дублировала slots[] и молча подменяла значение
        // ДЕФОЛТОМ, когда поле не было числом: style-радиус 0.30 при кривом
        // поле «абв» становился 0.12). Поле пишется только когда токен —
        // число; не-число — находка, прежнее значение живёт.
        const std::string tok = kv.first + "=" + kv.second;
        const ElementParams got = parse_element_params(tok, issues);
        bool matched = false;
        for (const auto& slot : param_slots()) {
            if (kv.first == slot.key) {
                float number = 0.0f;
                if (parse_float_token(kv.second, number)) {
                    p.*(slot.field) = got.*(slot.field);
                }
                matched = true;
                break;
            }
        }
        if (matched) {
            continue;
        }
        if (kv.first == "form") { p.form = got.form; }
        else if (kv.first == "sides" || kv.first == "n") { p.sides = got.sides; }
    }
    return p;
}

HousePartTile house_part_tile(const HouseGraph& graph, const Element& e,
                              int mat_override, int tone_override,
                              std::vector<ParamIssue>* issues) {
    HousePartTile t;
    const auto complain = [&](std::string token, std::string why) {
        if (issues != nullptr) {
            issues->push_back({std::move(token), std::move(why)});
        } else {
            // ВСЛУХ ДАЖЕ БЕЗ СБОРЩИКА НАХОДОК. Молчаливая подстановка — это
            // ровно тот дефект, ради которого волна завела имена: опечатка,
            // которую никто не увидит, живёт в принятых витринах годами.
            std::fprintf(stderr, "[дом] %s: %s\n", token.c_str(), why.c_str());
        }
    };
    // УМОЛЧАНИЕ ВЫВЕДЕНО ИЗ ВИДА ЭЛЕМЕНТА, а не назначено таблицей: прямая —
    // это брус (тёсаный, средний тон), поверхность — это стена (штукатурка,
    // светлая). Рецепт, не сказавший mat, получает то, чем эта деталь бывает
    // чаще всего.
    const bool beam = e.kind == ElementKind::Line;
    t.surface = beam ? 0u : 5u; // HewnTimber : Plaster
    t.tone = beam ? 1u : 0u;    // Mid : Light
    // ИМЯ ВЕЩЕСТВА — ПЕРВЫМ СЛОВОМ, координаты — запасным. Рецепт, сказавший
    // `material = brick-red`, называет вещество, а не клетку чужого атласа
    // (правило 5); реестр отдаёт координаты той же клетки, которую рецепт мог
    // бы написать числами, — то есть кадр от перехода на имя не меняется.
    const std::string named = graph.param(e.id, "material");
    if (!named.empty()) {
        const core::MaterialTable& table = core::material_registry();
        const core::MaterialId id = table.find(named);
        if (id == core::MATERIAL_NONE) {
            // ОТКАЗ ВСЛУХ НА НЕИЗВЕСТНОМ ИМЕНИ, и это половина смысла имён:
            // `mat=17` молча становился глухим окном, а «material = кирпыч»
            // обязан сказать о себе.
            complain("material=" + named, "неизвестное вещество — реестр его не знает");
        } else {
            std::uint32_t column = 0;
            std::uint32_t row = 0;
            if (table.cell_of(table.record(id), column, row)) {
                t.surface = column;
                t.tone = row;
                t.material = named;
            } else {
                // Вещество есть, но зерна у него нет (полотно, металл,
                // пергамент). Это не опечатка: клетку оставляем умолчанием
                // рода элемента, а ИМЯ несём дальше — рисовальщик возьмёт из
                // записи блик и оттенок, которых у пары нет вовсе.
                t.material = named;
            }
        }
    }
    const std::string m = graph.param(e.id, "mat");
    const std::string tn = graph.param(e.id, "tone");
    if (!m.empty()) {
        const int raw = std::atoi(m.c_str());
        if (raw < 0 || raw >= 9) {
            // ТОТ САМЫЙ ДЕФЕКТ, НАЗВАННЫЙ ЧИСЛОМ: `mat=17` становится
            // `17 % 9 = 8` = глухое окно, и стена застекляется без единого
            // сообщения. Остаток оставлен — менять его значило бы двигать
            // пиксели у любого рецепта, который на нём выехал, — но молчать
            // о нём больше нельзя.
            complain("mat=" + m,
                     "номер колонки вне листа (их 9); взят остаток "
                         + std::to_string(((raw % 9) + 9) % 9)
                         + " — назовите вещество ключом material");
        }
        t.surface = static_cast<std::uint32_t>(raw) % 9u;
    }
    if (!tn.empty()) {
        const int raw = std::atoi(tn.c_str());
        if (raw < 0 || raw >= 4) {
            complain("tone=" + tn, "номер ряда вне листа (их 4); взят остаток "
                                       + std::to_string(((raw % 4) + 4) % 4));
        }
        t.tone = static_cast<std::uint32_t>(raw) % 4u;
    }
    // СИЛЬНЫЙ ИЗНОС УВОДИТ ТОН В ВЫВЕТРЕННЫЙ РЯД атласа: серость и лишайник
    // НАРИСОВАНЫ там, а не выдумываются шейдером.
    const std::string w = graph.param(e.id, "wear");
    if (!w.empty() && std::strtof(w.c_str(), nullptr) >= 0.7f) {
        t.tone = 3u; // Weathered
    }
    // МАТЕРИАЛ КУСКА ПОВЕРХ МАТЕРИАЛА ЭЛЕМЕНТА — последним словом: стена
    // собирается ИЗ КУСКОВ (доска фахверка — брус, кирпич — глина, блок —
    // камень), а элемент по-прежнему один.
    if (mat_override >= 0) {
        t.surface = static_cast<std::uint32_t>(mat_override) % 9u;
    }
    if (tone_override >= 0) {
        t.tone = static_cast<std::uint32_t>(tone_override) % 4u;
    }
    return t;
}

} // namespace dfn::world
