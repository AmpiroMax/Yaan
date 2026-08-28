/*
Created: 28:08:2026 - 16:55:00
Last updated: 28:08:2026 - 16:55:00
Module: engine/render
File: engine/render/sources/PartsMaterial.cpp

Responsibility:
- Словарь имён раскладки листа набора и перевод пары (поверхность, тон) в
  запись реестра. Устройство и причины — в PartsMaterial.h.

Dependencies:
- Uses: PartsMaterial.h, PartsAtlas.h, engine/core/materials.
- Used by: RenderSystem, engine/app, инструменты, тесты.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; дизайн зоны — docs/design/MATERIALS.md.
- ЗДЕСЬ НЕТ И НЕ БУДЕТ НИ ОДНОГО СВОЙСТВА ВЕЩЕСТВА. Цвет, блик, плотность —
  в данных. Появится здесь число про вещество — вернулись к двум реестрам.
*/
/*
UPD:
- 28:08:2026 - 16:55:00: Создан — волна 3 зоны МАТЕРИАЛЫ.
*/

#include "engine/render/sources/PartsMaterial.h"

#include <array>

namespace dfn::render {
namespace {

/// ПОРЯДОК — ЭТО ОРДИНАЛЫ PartSurface. Сверяется с данными (см. заголовок).
constexpr std::array<std::string_view, PARTS_ATLAS_SURFACES> SURFACE_NAMES{
    "hewn-timber", "sawn-board", "end-grain", "stone", "fired-clay",
    "plaster",     "thatch",     "turf",      "pane"};

/// ПОРЯДОК — ЭТО ОРДИНАЛЫ PartTone.
constexpr std::array<std::string_view, PARTS_ATLAS_TONES> TONE_NAMES{
    "light", "mid", "dark", "weathered"};

} // namespace

std::string_view part_surface_name(PartSurface surface) {
    const auto i = static_cast<std::size_t>(surface);
    return i < SURFACE_NAMES.size() ? SURFACE_NAMES[i] : std::string_view{};
}

std::string_view part_tone_name(PartTone tone) {
    const auto i = static_cast<std::size_t>(tone);
    return i < TONE_NAMES.size() ? TONE_NAMES[i] : std::string_view{};
}

bool part_surface_by_name(std::string_view name, PartSurface& out) {
    for (std::size_t i = 0; i < SURFACE_NAMES.size(); ++i) {
        if (SURFACE_NAMES[i] == name) {
            out = static_cast<PartSurface>(i);
            return true;
        }
    }
    return false;
}

bool part_tone_by_name(std::string_view name, PartTone& out) {
    for (std::size_t i = 0; i < TONE_NAMES.size(); ++i) {
        if (TONE_NAMES[i] == name) {
            out = static_cast<PartTone>(i);
            return true;
        }
    }
    return false;
}

core::MaterialId named_material_of(PartSurface surface, PartTone tone) {
    return core::material_registry().of_cell(PARTS_SHEET,
                                             static_cast<std::uint32_t>(surface),
                                             static_cast<std::uint32_t>(tone));
}

core::MaterialRecord material_of(PartSurface surface, PartTone tone) {
    const core::MaterialTable& table = core::material_registry();
    if (const core::MaterialId id = named_material_of(surface, tone);
        id != core::MATERIAL_NONE) {
        return table.record(id);
    }
    // ВЫВЕДЕННАЯ ЗАПИСЬ БЕЗЫМЯННОЙ ПАРЫ: та же клетка, тот же ряд, умолчания
    // по всему остальному — то есть в точности сегодняшний ламберт.
    core::MaterialRecord rec;
    rec.name.clear(); // безымянная по построению: имя есть не у всякого вещества
    rec.tiled = true;
    rec.sheet = std::string(PARTS_SHEET);
    rec.cell_column = std::string(part_surface_name(surface));
    rec.cell_row = std::string(part_tone_name(tone));
    rec.tile_span_m = PARTS_TILE_SPAN_M;
    // ИЗНОС ВЫВОДИТСЯ ИЗ РЯДА, а не берётся штатным: в этом листе тон и износ —
    // ОДНА ось (PartsAtlas.h), и ряд Weathered означает «выветренное».
    switch (tone) {
    case PartTone::Light:
        rec.wear = 0.10f;
        break;
    case PartTone::Mid:
        rec.wear = 0.35f;
        break;
    case PartTone::Dark:
        rec.wear = 0.50f;
        break;
    case PartTone::Weathered:
        rec.wear = 0.85f;
        break;
    }
    rec.content_hash = core::material_record_hash(rec);
    return rec;
}

bool parts_sheet_matches_atlas(const core::MaterialTable& table, std::string* why) {
    const auto say = [why](std::string text) {
        if (why != nullptr) {
            *why = std::move(text);
        }
        return false;
    };
    const core::MaterialSheet* sheet = table.sheet(PARTS_SHEET);
    if (sheet == nullptr) {
        return say("реестр не объявляет листа \"" + std::string(PARTS_SHEET) + "\"");
    }
    if (sheet->columns.size() != SURFACE_NAMES.size()) {
        return say("колонок в данных " + std::to_string(sheet->columns.size())
                   + ", в атласе " + std::to_string(SURFACE_NAMES.size()));
    }
    if (sheet->rows.size() != TONE_NAMES.size()) {
        return say("рядов в данных " + std::to_string(sheet->rows.size())
                   + ", в атласе " + std::to_string(TONE_NAMES.size()));
    }
    for (std::size_t i = 0; i < SURFACE_NAMES.size(); ++i) {
        if (sheet->columns[i] != SURFACE_NAMES[i]) {
            return say("колонка " + std::to_string(i) + ": в данных \""
                       + sheet->columns[i] + "\", в атласе \""
                       + std::string(SURFACE_NAMES[i]) + "\"");
        }
    }
    for (std::size_t i = 0; i < TONE_NAMES.size(); ++i) {
        if (sheet->rows[i] != TONE_NAMES[i]) {
            return say("ряд " + std::to_string(i) + ": в данных \"" + sheet->rows[i]
                       + "\", в атласе \"" + std::string(TONE_NAMES[i]) + "\"");
        }
    }
    return true;
}

std::string_view material_program(const core::MaterialRecord& rec) {
    // ДВЕ ПРОГРАММЫ, И НИ ОДНА ИЗ НИХ НЕ ЗНАЧИТ «ЭТО ЛИСТ». Качается и
    // вырезается — это свойства ВЕЩЕСТВА; сплошное и стоящее рисуется prop.
    if (rec.motion == core::MaterialMotion::Foliage
        || rec.opacity == core::MaterialOpacity::Cutout) {
        return "foliage";
    }
    return "prop";
}

} // namespace dfn::render
