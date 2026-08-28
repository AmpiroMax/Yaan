/*
Created: 28:08:2026 - 16:20:00
Last updated: 28:08:2026 - 16:20:00
Module: engine/core/materials
File: engine/core/materials/sources/MaterialRegistry.cpp

Responsibility:
- Разбор файла реестра (.dfmat), личность записи и таблица процесса.
  Устройство и причины — в MaterialRegistry.h.

Dependencies:
- Uses: MaterialRegistry.h, engine/core/serialization/ContentHash.h, stdlib.
- Used by: render, world, platform/render, app, инструменты, тесты.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; дизайн зоны — docs/design/MATERIALS.md.
- НИ ОДНОГО `switch` ПО МАТЕРИАЛУ. Разрешение идёт таблицей и только таблицей
  (MATERIALS.md §2.2); единственные switch здесь — по ключу разбора, то есть по
  грамматике файла, которую мы обязаны исчерпать.
*/
/*
UPD:
- 28:08:2026 - 16:20:00: Создан — волна 3 зоны МАТЕРИАЛЫ.
*/

#include "engine/core/materials/sources/MaterialRegistry.h"

#include "engine/core/serialization/sources/ContentHash.h"

#include <bit>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace dfn::core {
namespace {

/// Пробельная обрезка. Своя, потому что тянуть сюда чужой строковый набор ради
/// двух строк было бы дороже, чем написать (правило 1: core без зависимостей).
[[nodiscard]] std::string_view trim(std::string_view s) {
    const auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    while (!s.empty() && is_space(s.front())) {
        s.remove_prefix(1);
    }
    while (!s.empty() && is_space(s.back())) {
        s.remove_suffix(1);
    }
    return s;
}

/// Разбор числа. ОТКАЗ ВСЛУХ вместо тихого нуля: поле, которое не число, —
/// это находка данных, а подставленный ноль делает опечатку невидимой ровно
/// так же, как `mat=17 % 9` застекляет стену без единого сообщения.
[[nodiscard]] bool parse_float(std::string_view token, float& out) {
    const std::string text(token);
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    out = static_cast<float>(v);
    return true;
}

/// Три числа через пробел — цвет или свечение.
[[nodiscard]] bool parse_vec3(std::string_view value, glm::vec3& out) {
    std::istringstream in{std::string(value)};
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (!(in >> x >> y >> z)) {
        return false;
    }
    std::string extra;
    if (in >> extra) {
        return false;
    }
    out = glm::vec3{x, y, z};
    return true;
}

/// "parts:hewn-timber/dark" — лист, колонка, ряд.
[[nodiscard]] bool parse_cell(std::string_view value, std::string& sheet,
                              std::string& column, std::string& row) {
    const std::size_t colon = value.find(':');
    const std::size_t slash = value.find('/', colon == std::string_view::npos ? 0 : colon);
    if (colon == std::string_view::npos || slash == std::string_view::npos
        || slash <= colon + 1 || colon == 0 || slash + 1 >= value.size()) {
        return false;
    }
    sheet = std::string(value.substr(0, colon));
    column = std::string(value.substr(colon + 1, slash - colon - 1));
    row = std::string(value.substr(slash + 1));
    return true;
}

void hash_f32(serialization::Fnv1a64& h, float v) {
    h.update_u64(std::bit_cast<std::uint32_t>(v));
}

} // namespace

std::uint64_t material_record_hash(const MaterialRecord& rec) {
    // ПОРЯДОК ПОЛЕЙ — ЧАСТЬ КОНТРАКТА (шапка .h). Строки идут с длиной, числа
    // — БИТАМИ, а не значениями: личность обязана быть фактом о байтах, иначе
    // две одинаковые записи разошлись бы на машине с другим форматированием.
    serialization::Fnv1a64 h;
    h.update_length_prefixed(rec.name);
    h.update_u64(rec.tiled ? 1u : 0u);
    h.update_length_prefixed(rec.sheet);
    h.update_length_prefixed(rec.cell_column);
    h.update_length_prefixed(rec.cell_row);
    hash_f32(h, rec.tile_span_m);
    hash_f32(h, rec.tint.x);
    hash_f32(h, rec.tint.y);
    hash_f32(h, rec.tint.z);
    hash_f32(h, rec.roughness);
    hash_f32(h, rec.metalness);
    hash_f32(h, rec.emission.x);
    hash_f32(h, rec.emission.y);
    hash_f32(h, rec.emission.z);
    hash_f32(h, rec.wear);
    hash_f32(h, rec.wear_amp);
    hash_f32(h, rec.scatter_amp);
    h.update_u64(static_cast<std::uint64_t>(rec.motion));
    h.update_u64(static_cast<std::uint64_t>(rec.opacity));
    hash_f32(h, rec.density_kg_m3);
    hash_f32(h, rec.friction);
    hash_f32(h, rec.restitution);
    h.update_length_prefixed(rec.surface_tag);
    hash_f32(h, rec.brittle_kj_m2);
    return h.digest();
}

MaterialTable::MaterialTable() {
    // ЗАПИСЬ 0 ЕСТЬ ВСЕГДА — «материала не названо». Безымянна нарочно:
    // find("") обязан не находить ничего.
    MaterialRecord none;
    none.name.clear();
    none.content_hash = material_record_hash(none);
    records_.push_back(std::move(none));
    reindex();
}

void MaterialTable::reindex() {
    serialization::Fnv1a64 h;
    for (MaterialRecord& rec : records_) {
        rec.content_hash = material_record_hash(rec);
        h.update_u64(rec.content_hash);
    }
    table_hash_ = h.digest();
}

MaterialId MaterialTable::find(std::string_view name) const {
    if (name.empty()) {
        return MATERIAL_NONE;
    }
    for (std::size_t i = 1; i < records_.size(); ++i) {
        if (records_[i].name == name) {
            return static_cast<MaterialId>(i);
        }
    }
    return MATERIAL_NONE;
}

const MaterialSheet* MaterialTable::sheet(std::string_view name) const {
    for (const MaterialSheet& s : sheets_) {
        if (s.name == name) {
            return &s;
        }
    }
    return nullptr;
}

bool MaterialTable::cell_of(const MaterialRecord& rec, std::uint32_t& column,
                            std::uint32_t& row) const {
    if (!rec.tiled) {
        return false;
    }
    const MaterialSheet* s = sheet(rec.sheet);
    if (s == nullptr) {
        return false;
    }
    bool found_column = false;
    for (std::size_t i = 0; i < s->columns.size(); ++i) {
        if (s->columns[i] == rec.cell_column) {
            column = static_cast<std::uint32_t>(i);
            found_column = true;
            break;
        }
    }
    bool found_row = false;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        if (s->rows[i] == rec.cell_row) {
            row = static_cast<std::uint32_t>(i);
            found_row = true;
            break;
        }
    }
    return found_column && found_row;
}

MaterialId MaterialTable::of_cell(std::string_view sheet_name, std::uint32_t column,
                                  std::uint32_t row) const {
    for (std::size_t i = 1; i < records_.size(); ++i) {
        const MaterialRecord& rec = records_[i];
        if (!rec.tiled || rec.sheet != sheet_name) {
            continue;
        }
        std::uint32_t c = 0;
        std::uint32_t r = 0;
        if (cell_of(rec, c, r) && c == column && r == row) {
            return static_cast<MaterialId>(i);
        }
    }
    return MATERIAL_NONE;
}

bool parse_material_table(std::string_view text, std::string_view source,
                          MaterialTable& out, std::vector<std::string>* errors) {
    out.records_.clear();
    out.sheets_.clear();
    out.source_ = std::string(source);
    MaterialRecord none;
    none.name.clear();
    out.records_.push_back(std::move(none));

    bool clean = true;
    std::size_t line_no = 0;
    // «Кто сейчас открыт»: -1 — никто, иначе номер записи либо листа.
    enum class Block { None, Material, Sheet };
    Block block = Block::None;
    std::size_t current = 0;
    // Наследование физики разбирается ВТОРЫМ проходом: запись вправе сослаться
    // на вещество, объявленное ниже, — файл читается человеком, а не машиной с
    // требованием «сначала объяви».
    std::vector<std::pair<std::size_t, std::string>> inherits;
    // Какие поля физики запись назвала САМА (наследование их не перебивает).
    std::vector<bool> own_density;
    std::vector<bool> own_friction;
    std::vector<bool> own_restitution;
    std::vector<bool> own_tag;
    std::vector<bool> own_brittle;
    const auto grow = [&] {
        own_density.resize(out.records_.size(), false);
        own_friction.resize(out.records_.size(), false);
        own_restitution.resize(out.records_.size(), false);
        own_tag.resize(out.records_.size(), false);
        own_brittle.resize(out.records_.size(), false);
    };
    grow();

    const auto fail = [&](std::string_view what) {
        clean = false;
        if (errors != nullptr) {
            errors->push_back(std::string(source) + ":" + std::to_string(line_no)
                              + ": " + std::string(what));
        }
    };

    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        std::string_view raw =
            text.substr(pos, nl == std::string_view::npos ? std::string_view::npos
                                                          : nl - pos);
        pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;
        ++line_no;
        if (const std::size_t hash = raw.find('#'); hash != std::string_view::npos) {
            raw = raw.substr(0, hash);
        }
        const std::string_view line = trim(raw);
        if (line.empty()) {
            continue;
        }

        // --- ЗАГОЛОВОК БЛОКА ------------------------------------------------
        if (line.starts_with("material ")) {
            const std::string_view name = trim(line.substr(9));
            if (name.empty()) {
                fail("material без имени");
                block = Block::None;
                continue;
            }
            if (out.find(name) != MATERIAL_NONE) {
                fail("имя материала повторяется: " + std::string(name));
            }
            MaterialRecord rec;
            rec.name = std::string(name);
            out.records_.push_back(std::move(rec));
            grow();
            current = out.records_.size() - 1;
            block = Block::Material;
            continue;
        }
        if (line.starts_with("sheet ")) {
            const std::string_view name = trim(line.substr(6));
            if (name.empty()) {
                fail("sheet без имени");
                block = Block::None;
                continue;
            }
            MaterialSheet s;
            s.name = std::string(name);
            out.sheets_.push_back(std::move(s));
            current = out.sheets_.size() - 1;
            block = Block::Sheet;
            continue;
        }

        // --- ПОЛЕ -----------------------------------------------------------
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            fail("строка без '=' и не заголовок блока: " + std::string(line));
            continue;
        }
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));
        if (block == Block::None) {
            fail("поле вне блока: " + std::string(key));
            continue;
        }
        if (block == Block::Sheet) {
            MaterialSheet& s = out.sheets_[current];
            std::vector<std::string>* target = nullptr;
            if (key == "columns") {
                target = &s.columns;
            } else if (key == "rows") {
                target = &s.rows;
            } else {
                fail("неизвестное поле листа: " + std::string(key));
                continue;
            }
            target->clear();
            std::istringstream in{std::string(value)};
            std::string token;
            while (in >> token) {
                target->push_back(token);
            }
            if (target->empty()) {
                fail("пустой список " + std::string(key));
            }
            continue;
        }

        MaterialRecord& rec = out.records_[current];
        float number = 0.0f;
        if (key == "cell") {
            if (!parse_cell(value, rec.sheet, rec.cell_column, rec.cell_row)) {
                fail("клетка не разобрана (ждали лист:колонка/ряд): "
                     + std::string(value));
                continue;
            }
            rec.tiled = true;
        } else if (key == "span_m") {
            if (!parse_float(value, number)) {
                fail("span_m не число: " + std::string(value));
                continue;
            }
            rec.tile_span_m = number;
        } else if (key == "tint") {
            if (!parse_vec3(value, rec.tint)) {
                fail("tint — не три числа: " + std::string(value));
            }
        } else if (key == "emission") {
            if (!parse_vec3(value, rec.emission)) {
                fail("emission — не три числа: " + std::string(value));
            }
        } else if (key == "roughness") {
            if (!parse_float(value, number)) {
                fail("roughness не число: " + std::string(value));
                continue;
            }
            rec.roughness = number;
        } else if (key == "metalness") {
            if (!parse_float(value, number)) {
                fail("metalness не число: " + std::string(value));
                continue;
            }
            rec.metalness = number;
        } else if (key == "wear") {
            if (!parse_float(value, number)) {
                fail("wear не число: " + std::string(value));
                continue;
            }
            rec.wear = number;
        } else if (key == "wear_amp") {
            if (!parse_float(value, number)) {
                fail("wear_amp не число: " + std::string(value));
                continue;
            }
            rec.wear_amp = number;
        } else if (key == "scatter_amp") {
            if (!parse_float(value, number)) {
                fail("scatter_amp не число: " + std::string(value));
                continue;
            }
            rec.scatter_amp = number;
        } else if (key == "motion") {
            if (value == "static") {
                rec.motion = MaterialMotion::Static;
            } else if (value == "foliage") {
                rec.motion = MaterialMotion::Foliage;
            } else {
                fail("motion: ждали static|foliage, получили " + std::string(value));
            }
        } else if (key == "opacity") {
            if (value == "opaque") {
                rec.opacity = MaterialOpacity::Opaque;
            } else if (value == "cutout") {
                rec.opacity = MaterialOpacity::Cutout;
            } else {
                fail("opacity: ждали opaque|cutout, получили " + std::string(value));
            }
        } else if (key == "density") {
            if (!parse_float(value, number)) {
                fail("density не число: " + std::string(value));
                continue;
            }
            rec.density_kg_m3 = number;
            own_density[current] = true;
        } else if (key == "friction") {
            if (!parse_float(value, number)) {
                fail("friction не число: " + std::string(value));
                continue;
            }
            rec.friction = number;
            own_friction[current] = true;
        } else if (key == "restitution") {
            if (!parse_float(value, number)) {
                fail("restitution не число: " + std::string(value));
                continue;
            }
            rec.restitution = number;
            own_restitution[current] = true;
        } else if (key == "surface") {
            rec.surface_tag = std::string(value);
            own_tag[current] = true;
        } else if (key == "brittle") {
            if (!parse_float(value, number)) {
                fail("brittle не число: " + std::string(value));
                continue;
            }
            rec.brittle_kj_m2 = number;
            own_brittle[current] = true;
        } else if (key == "substance") {
            inherits.emplace_back(current, std::string(value));
        } else {
            fail("неизвестное поле материала: " + std::string(key));
        }
    }

    // --- ВТОРОЙ ПРОХОД: НАСЛЕДОВАНИЕ ФИЗИКИ ---------------------------------
    // Одна ступень, а не цепочка: «дуб наследует у дуба, который наследует у
    // дерева» — это дерево наследования, у которого пришлось бы ловить циклы,
    // а поводов заводить его ещё ни одного.
    line_no = 0;
    for (const auto& [idx, parent_name] : inherits) {
        const MaterialId parent = out.find(parent_name);
        if (parent == MATERIAL_NONE) {
            clean = false;
            if (errors != nullptr) {
                errors->push_back(std::string(source) + ": материал \""
                                  + out.records_[idx].name
                                  + "\" ссылается на неизвестное вещество \""
                                  + parent_name + "\"");
            }
            continue;
        }
        const MaterialRecord src = out.records_[parent];
        MaterialRecord& dst = out.records_[idx];
        if (!own_density[idx]) {
            dst.density_kg_m3 = src.density_kg_m3;
        }
        if (!own_friction[idx]) {
            dst.friction = src.friction;
        }
        if (!own_restitution[idx]) {
            dst.restitution = src.restitution;
        }
        if (!own_tag[idx]) {
            dst.surface_tag = src.surface_tag;
        }
        if (!own_brittle[idx]) {
            dst.brittle_kj_m2 = src.brittle_kj_m2;
        }
    }

    // --- СВЕРКА КЛЕТОК С ЛИСТАМИ --------------------------------------------
    // Клетка, которой в объявленном листе нет, — это молчаливая перекраска:
    // именно так `mat=17 % 9` застекляет стену. Ловим здесь и вслух.
    for (std::size_t i = 1; i < out.records_.size(); ++i) {
        const MaterialRecord& rec = out.records_[i];
        if (!rec.tiled) {
            continue;
        }
        std::uint32_t c = 0;
        std::uint32_t r = 0;
        if (!out.cell_of(rec, c, r)) {
            clean = false;
            if (errors != nullptr) {
                errors->push_back(std::string(source) + ": материал \"" + rec.name
                                  + "\" называет клетку " + rec.sheet + ":"
                                  + rec.cell_column + "/" + rec.cell_row
                                  + ", которой нет в объявленном листе");
            }
        }
    }

    out.reindex();
    return clean;
}

std::optional<MaterialTable> load_material_table(const std::filesystem::path& path,
                                                 std::vector<std::string>* errors) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string text = buf.str();
    MaterialTable table;
    (void)parse_material_table(text, path.string(), table, errors);
    return table;
}

const char* default_material_registry_path() { return "assets/materials/yaan.dfmat"; }

namespace {

MaterialTable& process_table() {
    static MaterialTable table = [] {
        const char* env = std::getenv("DFN_MATERIALS");
        const std::filesystem::path path =
            (env != nullptr && *env != '\0') ? std::filesystem::path(env)
                                             : std::filesystem::path(
                                                   default_material_registry_path());
        std::vector<std::string> errors;
        std::optional<MaterialTable> loaded = load_material_table(path, &errors);
        if (!loaded.has_value()) {
            // ВСЛУХ И ОДИН РАЗ. Без реестра всё вещество — «не названо», то
            // есть докатериальный кадр; молча уехать в умолчания значило бы
            // не заметить, что данные не приехали.
            std::fprintf(stderr,
                         "[materials] реестр не открылся (%s): всё вещество "
                         "будет «не названо» — кадр как до материалов\n",
                         path.string().c_str());
            return MaterialTable{};
        }
        for (const std::string& e : errors) {
            std::fprintf(stderr, "[materials] %s\n", e.c_str());
        }
        return std::move(*loaded);
    }();
    return table;
}

} // namespace

const MaterialTable& material_registry() { return process_table(); }

bool reload_material_registry(const std::filesystem::path& path,
                              std::vector<std::string>* errors) {
    std::optional<MaterialTable> loaded = load_material_table(path, errors);
    if (!loaded.has_value()) {
        return false;
    }
    process_table() = std::move(*loaded);
    return true;
}

} // namespace dfn::core
