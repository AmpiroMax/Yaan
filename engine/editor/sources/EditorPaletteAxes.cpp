/*
Created: 17:08:2026 - 21:00:14
Last updated: 17:08:2026 - 21:00:14
Module: engine/editor
File: engine/editor/sources/EditorPaletteAxes.cpp

Responsibility:
- THE FAMILY-FIRST CHOOSER. Pick a family, then turn its properties and watch
  the part change. Declared in EditorPalette.h; separated from the query half
  because they answer different questions and Rule 21 has a number.

WHY THIS EXISTS (user, 17.08.2026): «основное разбиение должно быть по
СЕМЕЙСТВУ, а не как сейчас. Разные свойства материала пусть для каждого
материала будут. Я выбрал что-то и могу поменять свойство и также видеть в
предпросмотре как меняется объект.»

THE ONE DESIGN DECISION THAT MATTERS: A NAME IS NEVER ASSEMBLED FROM THE
PROPERTIES. Building "wall-" + bond + "-" + material + ... would be a SECOND
grammar, written by hand, guessing at what the forge does — and it would be
wrong on the very first family that does not fit it. The stair proves the
point: its triple reads (going_u, width_u, STEPS) and the first number is a
placeholder, so an assembled name would confidently produce a part that does
not exist. Instead the chooser looks for a part whose ALREADY-PARSED facets
match the chosen positions. One dictionary, read in the other direction, and
the reverse operation cannot drift from the forward one because there is only
one of them.

THE SECOND DECISION, AND IT IS WHAT MAKES THE FEATURE USABLE: THE SHELF IS NOT
A FULL CROSS, so a position that leads nowhere is greyed BEFORE it is clicked.
Measured, not assumed: the wall family fills 4.3% of its own axis product (768
parts against 17820 combinations) because bond and material are tied — framex
only with clay and plaster, log only with timber and dark, ashlar only with
stone. And clay, tile and turf are baked in ONE wear out of three. A builder
turning wear on a tile roof would otherwise get an empty list and no reason.

Dependencies:
- Uses: EditorPalette.h, std.
- Used by: EditorPaletteView, App, tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DO NOT ADD A LIST OF AXES PER FAMILY. Which axes a family offers is measured
  from the shelf every time it is entered; a table here would be a second copy
  of the kit's design and would go stale the first time the kit grew.
- A REPAIRED AXIS MUST REACH THE PANEL. repaired_axes() is not diagnostics: a
  choice silently replaced sends the builder away with the wrong part.
*/
/*
UPD:
- 17:08:2026 - 21:00:14: Создан — выбор по семейству и свойствам (заказ пользователя 17.08, п.4).
*/

#include "engine/editor/sources/EditorPalette.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dfn::app {
namespace {

constexpr std::size_t AXIS_COUNT = static_cast<std::size_t>(PartAxis::Count);

/// THE ORDER IN WHICH AXES KEEP THEIR VALUE WHEN SOMETHING HAS TO GIVE.
/// Earlier is more precious. Material and bond describe WHAT the part is;
/// wear describes how tired it looks, and it is the cheapest thing to move —
/// which is exactly right for the case this was built for, where choosing tile
/// leaves only one wear on the shelf.
constexpr PartAxis REPAIR_ORDER[AXIS_COUNT] = {
    PartAxis::Material, PartAxis::Style,  PartAxis::Tags,   PartAxis::Faces,
    PartAxis::Diameter, PartAxis::Box,    PartAxis::Height, PartAxis::Length,
    PartAxis::Width,    PartAxis::Steps,  PartAxis::Wear,
};

/// Numbers in axis values are TEXT the panel shows, so they are formatted once,
/// here, and never twice.
[[nodiscard]] std::string metres(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    return buf;
}

} // namespace

/// WHAT ONE PART ANSWERS ON ONE AXIS. "" is a legal answer and means "this part
/// has no bond / no tags", which is a position a builder can choose — a plain
/// plastered wall is not a wall with a missing property.
std::string PaletteModel::axis_value_of(std::size_t index, PartAxis axis, float& number) const {
    const PartFacets& f = part(index);
    number = 0.0f;
    switch (axis) {
    case PartAxis::Material: return f.material;
    case PartAxis::Style:    return f.style;
    case PartAxis::Tags: {
        std::string joined;
        for (const std::string& t : f.tags) {
            joined += (joined.empty() ? "" : "+") + t;
        }
        return joined;
    }
    case PartAxis::Faces:
        if (f.faces < 0) {
            return {};
        }
        number = static_cast<float>(f.faces);
        return f.faces == 0 ? std::string("r") : std::to_string(f.faces);
    case PartAxis::Diameter:
        number = f.diameter_m;
        return f.diameter_m > 0.0f ? metres(f.diameter_m) : std::string();
    case PartAxis::Height:
        number = f.height_m;
        return f.height_m > 0.0f ? metres(f.height_m) : std::string();
    case PartAxis::Length:
        number = f.length_m;
        return f.length_m > 0.0f ? metres(f.length_m) : std::string();
    case PartAxis::Width:
        number = f.width_m;
        return f.width_m > 0.0f ? metres(f.width_m) : std::string();
    case PartAxis::Steps:
        number = static_cast<float>(f.steps);
        return f.steps > 0 ? std::to_string(f.steps) : std::string();
    case PartAxis::Box:
        if (f.box_u[0] == 0 && f.box_u[1] == 0 && f.box_u[2] == 0) {
            return {};
        }
        // The kit's own notation, in grid units, because that is what the
        // builder counts with when parts have to meet.
        number = static_cast<float>(f.box_u[0] * f.box_u[1] * f.box_u[2]);
        return std::to_string(f.box_u[0]) + "x" + std::to_string(f.box_u[1]) + "x" +
               std::to_string(f.box_u[2]);
    case PartAxis::Wear:
        number = static_cast<float>(f.wear_pct);
        return f.wear_pct >= 0 ? std::to_string(f.wear_pct) : std::string();
    case PartAxis::Count: break;
    }
    return {};
}

bool PaletteModel::axis_match(std::size_t index, PartAxis axis, std::string_view value) const {
    float ignored = 0.0f;
    return axis_value_of(index, axis, ignored) == value;
}

void PaletteModel::choose_family(std::string_view family) {
    family_.assign(family);
    family_parts_.clear();
    for (std::size_t i = 0; i < part_count(); ++i) {
        if (part(i).family == family_) {
            family_parts_.push_back(i);
        }
    }
    for (std::size_t k = 0; k < AXIS_COUNT; ++k) {
        axis_set_[k] = false;
        axis_pick_[k].clear();
    }
    repaired_.clear();
    axes_dirty_ = true;
    if (family_.empty()) {
        return;
    }
    // SEED FROM THE PART IN HAND when it belongs here. Entering the chooser
    // must not throw away what the builder already picked — landing on the
    // shelf's first part every time would make the family view feel like a
    // reset rather than a way in.
    std::size_t seed = selected_index();
    if (seed >= part_count() || part(seed).family != family_) {
        seed = family_parts_.empty() ? part_count() : family_parts_.front();
    }
    if (seed < part_count()) {
        for (std::size_t k = 0; k < AXIS_COUNT; ++k) {
            float number = 0.0f;
            axis_pick_[k] = axis_value_of(seed, static_cast<PartAxis>(k), number);
            axis_set_[k] = true;
        }
        select(part(seed).name);
    }
}

void PaletteModel::rebuild_axes() const {
    if (!axes_dirty_) {
        return;
    }
    axes_dirty_ = false;
    for (std::size_t k = 0; k < AXIS_COUNT; ++k) {
        axis_values_[k].clear();
    }
    if (family_.empty()) {
        return;
    }
    for (std::size_t k = 0; k < AXIS_COUNT; ++k) {
        const PartAxis axis = static_cast<PartAxis>(k);
        for (const std::size_t at : family_parts_) {
            float number = 0.0f;
            const std::string v = axis_value_of(at, axis, number);
            const auto it = std::find_if(axis_values_[k].begin(), axis_values_[k].end(),
                                         [&v](const AxisValue& a) { return a.value == v; });
            if (it == axis_values_[k].end()) {
                axis_values_[k].push_back({v, number, 0, false});
            }
        }
        // AN AXIS WITH ONE POSITION IS NOT AN AXIS. Left in the list so the
        // panel can print it as a fact; axis_offered() is what says whether it
        // is a control.
        std::sort(axis_values_[k].begin(), axis_values_[k].end(),
                  [](const AxisValue& a, const AxisValue& b) {
                      if (a.number != b.number) {
                          return a.number < b.number;
                      }
                      return a.value < b.value;
                  });
        for (AxisValue& v : axis_values_[k]) {
            v.on = axis_set_[k] && axis_pick_[k] == v.value;
        }
    }

    // THE COUNTS, and each one answers "if I clicked this, how many parts would
    // be left" — with this axis' OWN pick left out, exactly like the flat
    // list's chips. A position reading zero is a dead end, and it is shown as
    // one before it is clicked rather than after.
    for (std::size_t k = 0; k < AXIS_COUNT; ++k) {
        const PartAxis axis = static_cast<PartAxis>(k);
        for (AxisValue& v : axis_values_[k]) {
            v.count = 0;
            for (const std::size_t at : family_parts_) {
                if (!axis_match(at, axis, v.value)) {
                    continue;
                }
                bool ok = true;
                for (std::size_t j = 0; ok && j < AXIS_COUNT; ++j) {
                    if (j == k || !axis_set_[j]) {
                        continue;
                    }
                    ok = axis_match(at, static_cast<PartAxis>(j), axis_pick_[j]);
                }
                if (ok) {
                    ++v.count;
                }
            }
        }
    }
}

bool PaletteModel::axis_offered(PartAxis axis) const {
    rebuild_axes();
    return axis_values_[static_cast<std::size_t>(axis)].size() > 1;
}

const std::vector<AxisValue>& PaletteModel::axis_values(PartAxis axis) const {
    rebuild_axes();
    return axis_values_[static_cast<std::size_t>(axis)];
}

void PaletteModel::choose_axis(PartAxis axis, std::string_view value) {
    if (family_.empty()) {
        return;
    }
    const std::size_t fixed = static_cast<std::size_t>(axis);
    axis_pick_[fixed].assign(value);
    axis_set_[fixed] = true;
    repaired_.clear();

    // THE REPAIR, and it is a walk rather than a search: start from every part
    // of the family that answers the axis just turned, then apply the other
    // axes in order of how precious they are. An axis whose pick still leaves
    // something keeps it; one that would empty the set is MOVED to its nearest
    // reachable position and recorded. Because a constraint is only ever kept
    // when it is satisfiable given the ones before it, the set is never empty
    // at the end — there is no path from a legal click to a dead end.
    std::vector<std::size_t> live;
    for (const std::size_t at : family_parts_) {
        if (axis_match(at, axis, value)) {
            live.push_back(at);
        }
    }
    if (live.empty()) {
        // The position does not exist in this family at all. Nothing to repair
        // TO — leave the axes as they were and say nothing was resolved.
        axis_set_[fixed] = false;
        axes_dirty_ = true;
        return;
    }

    for (const PartAxis other : REPAIR_ORDER) {
        const std::size_t k = static_cast<std::size_t>(other);
        if (k == fixed || !axis_set_[k]) {
            continue;
        }
        std::vector<std::size_t> kept;
        for (const std::size_t at : live) {
            if (axis_match(at, other, axis_pick_[k])) {
                kept.push_back(at);
            }
        }
        if (!kept.empty()) {
            live.swap(kept);
            continue;
        }
        // NEAREST REACHABLE. For a numeric axis that is the closest number,
        // which is what a builder means by "the next one up"; for a word axis
        // there is no distance, so it is the first still standing.
        float want = 0.0f;
        (void)axis_value_of(live.front(), other, want);
        for (const AxisValue& v : axis_values_[k]) {
            if (v.value == axis_pick_[k]) {
                want = v.number;
                break;
            }
        }
        std::size_t best = live.front();
        float best_gap = -1.0f;
        for (const std::size_t at : live) {
            float number = 0.0f;
            (void)axis_value_of(at, other, number);
            const float gap = std::fabs(number - want);
            if (best_gap < 0.0f || gap < best_gap) {
                best_gap = gap;
                best = at;
            }
        }
        float number = 0.0f;
        axis_pick_[k] = axis_value_of(best, other, number);
        repaired_.push_back(other);
        std::vector<std::size_t> moved;
        for (const std::size_t at : live) {
            if (axis_match(at, other, axis_pick_[k])) {
                moved.push_back(at);
            }
        }
        live.swap(moved);
    }

    // Every remaining axis takes the value of what is left, so the chooser's
    // state always describes a REAL part rather than a wish.
    const std::size_t landed = live.front();
    for (std::size_t k = 0; k < AXIS_COUNT; ++k) {
        float number = 0.0f;
        axis_pick_[k] = axis_value_of(landed, static_cast<PartAxis>(k), number);
        axis_set_[k] = true;
    }
    axes_dirty_ = true;
    // THE PART IN HAND CHANGES HERE, which is what makes the ghost in the world
    // change in the same frame: the app's on_pick hangs off select().
    select(part(landed).name);
}

std::size_t PaletteModel::resolved_index() const {
    if (family_.empty()) {
        return part_count();
    }
    for (const std::size_t at : family_parts_) {
        bool ok = true;
        for (std::size_t k = 0; ok && k < AXIS_COUNT; ++k) {
            if (!axis_set_[k]) {
                continue;
            }
            ok = axis_match(at, static_cast<PartAxis>(k), axis_pick_[k]);
        }
        if (ok) {
            return at;
        }
    }
    return part_count();
}

} // namespace dfn::app
