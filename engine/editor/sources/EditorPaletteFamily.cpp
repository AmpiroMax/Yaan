/*
Module: engine/editor
File: engine/editor/sources/EditorPaletteFamily.cpp

Responsibility:
- THE MAIN PATH THROUGH THE MENU, as the user redrew it: eighteen families as
  pictures, then one family's PROPERTIES as dials, with the chosen part shown
  large and changing while the dials turn.

WHY THIS EXISTS (user, 17.08.2026): «основное разбиение должно быть по
СЕМЕЙСТВУ, а не как сейчас... Я выбрал что-то и могу поменять свойство и также
видеть в предпросмотре как меняется объект.»

WHAT THIS FILE IS NOT ALLOWED TO DECIDE: which axes exist, which positions are
reachable, and what a turn resolves to. All three are measured in
EditorPaletteAxes.cpp against the shelf. This file asks and draws.

Dependencies:
- Uses: EditorPaletteView.h, EditorPalette.h, EditorUi.h, Dear ImGui.
- Used by: EditorPaletteView.cpp (the panel calls in here when a family is open).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A MOVED AXIS MUST BE SAID OUT LOUD. repaired_axes() is drawn as a sentence,
  not swallowed: a choice silently replaced sends the builder to the map with a
  part he did not pick, and he finds out there.
- An unreachable position is DIMMED BUT STILL CLICKABLE. Making it dead would
  strand a builder who wants tile more than he wants that wear; clicking it
  moves the other axis and tells him. A wall the builder cannot argue with is
  worse than one that explains itself.
*/

#include "engine/editor/sources/EditorPaletteView.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace dfn::app {
namespace {

constexpr float FAMILY_TILE_PX = 110.0f;
constexpr float PREVIEW_PX = 192.0f;

/// The axes in the order a builder reads them: what it is made of, how it is
/// built, how big, and only then how worn.
constexpr PartAxis AXIS_ORDER[] = {
    PartAxis::Material, PartAxis::Style,  PartAxis::Tags,   PartAxis::Faces,
    PartAxis::Diameter, PartAxis::Box,    PartAxis::Height, PartAxis::Length,
    PartAxis::Width,    PartAxis::Steps,  PartAxis::Wear,
};

[[nodiscard]] const char* axis_key(PartAxis axis) {
    switch (axis) {
    case PartAxis::Material: return "palette.axis.material";
    case PartAxis::Style:    return "palette.axis.style";
    case PartAxis::Tags:     return "palette.axis.tags";
    case PartAxis::Faces:    return "palette.axis.faces";
    case PartAxis::Diameter: return "palette.axis.diameter";
    case PartAxis::Box:      return "palette.axis.box";
    case PartAxis::Height:   return "palette.axis.height";
    case PartAxis::Length:   return "palette.axis.length";
    case PartAxis::Width:    return "palette.axis.width";
    case PartAxis::Steps:    return "palette.axis.steps";
    case PartAxis::Wear:     return "palette.axis.wear";
    case PartAxis::Count:    break;
    }
    return "palette.axis.other";
}

/// The localization prefix an axis' RAW tokens go through. Numeric axes have
/// none: their positions are already numbers and a table row per diameter would
/// be a table nobody maintains.
[[nodiscard]] const char* axis_value_prefix(PartAxis axis) {
    switch (axis) {
    case PartAxis::Material: return "palette.material.";
    case PartAxis::Style:    return "palette.style.";
    case PartAxis::Tags:     return "palette.tag.";
    case PartAxis::Faces:    return "palette.faces.";
    case PartAxis::Wear:     return "palette.wear.";
    default:                 return nullptr;
    }
}

} // namespace

// EditorPaletteView.cpp owns these; declared here rather than in the header
// because they are the panel's private vocabulary, not the module's contract.
const char* palette_token_text(const char* prefix, const std::string& token);
bool palette_draw_part(PaletteModel& model, const PaletteHooks& hooks, std::size_t index,
                       bool grid, float thumb);

// ---------------------------------------------------------------------------

/// THE EIGHTEEN DOORS. Pictures rather than words, because the word "стойка"
/// and the word "столб" do not tell a builder apart what a glance does.
void draw_family_grid(PaletteModel& model, const PaletteHooks& hooks) {
    ImGui::TextDisabled("%s", EditorUi::tr("palette.families.hint"));
    const std::vector<FacetValue>& families = model.facet_values(FacetKind::Family);
    const float step = FAMILY_TILE_PX + ImGui::GetStyle().ItemSpacing.x;
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / step));
    int at = 0;
    for (const FacetValue& fam : families) {
        if (at % columns != 0) {
            ImGui::SameLine();
        }
        ++at;
        ImGui::PushID(fam.value.c_str());
        ImGui::BeginGroup();
        const std::size_t rep = model.first_of_family(fam.value);
        const EditorTexture tex =
            (hooks.thumbnail && rep < model.part_count())
                ? hooks.thumbnail(model.part(rep).name, static_cast<int>(FAMILY_TILE_PX))
                : 0;
        bool entered = false;
        if (tex != 0) {
            entered = EditorUi::image_button("##fam", tex, FAMILY_TILE_PX, FAMILY_TILE_PX);
        } else {
            entered = ImGui::Button(palette_token_text("palette.family.", fam.value),
                                    ImVec2(FAMILY_TILE_PX, FAMILY_TILE_PX));
        }
        ImGui::TextUnformatted(palette_token_text("palette.family.", fam.value));
        ImGui::TextDisabled("%zu", fam.count);
        ImGui::EndGroup();
        ImGui::PopID();
        if (entered) {
            model.choose_family(fam.value);
        }
    }
}

/// One axis as a row of positions. Unreachable ones are dimmed and still
/// clickable — see the notice at the top of this file.
void draw_axis(PaletteModel& model, PartAxis axis) {
    const std::vector<AxisValue>& values = model.axis_values(axis);
    if (values.empty()) {
        return;
    }
    const char* prefix = axis_value_prefix(axis);
    const auto text_of = [prefix](const AxisValue& v) {
        if (v.value.empty()) {
            return EditorUi::tr("palette.axis.none");
        }
        return prefix == nullptr ? v.value.c_str() : palette_token_text(prefix, v.value);
    };

    if (!model.axis_offered(axis)) {
        // ONE POSITION IS A FACT, NOT A DIAL. Printed so the builder knows the
        // property exists and is settled, never as a control he can turn and
        // watch do nothing.
        ImGui::TextDisabled("%s: %s", EditorUi::tr(axis_key(axis)), text_of(values.front()));
        return;
    }

    ImGui::TextUnformatted(EditorUi::tr(axis_key(axis)));
    ImGui::PushID(static_cast<int>(axis));
    const float avail = ImGui::GetContentRegionAvail().x;
    float used = 0.0f;
    for (const AxisValue& v : values) {
        char label[160];
        std::snprintf(label, sizeof(label), "%s", text_of(v));
        const float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 4.0f;
        if (used > 0.0f && used + w < avail) {
            ImGui::SameLine();
        } else {
            used = 0.0f;
        }
        used += w + ImGui::GetStyle().ItemSpacing.x;
        const bool reachable = v.count > 0;
        ImGui::PushID(v.value.c_str());
        if (!reachable) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
        }
        if (ImGui::RadioButton(label, v.on)) {
            model.choose_axis(axis, v.value);
        }
        if (!reachable) {
            ImGui::PopStyleVar();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", EditorUi::tr("palette.axis.unreachable"));
            }
        }
        ImGui::PopID();
    }
    ImGui::PopID();
}

/// The family's own page: the way back, the preview, what had to move, and the
/// dials.
void draw_family_page(PaletteModel& model, const PaletteHooks& hooks) {
    if (ImGui::SmallButton(EditorUi::tr("palette.back"))) {
        model.choose_family("");
        return;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(palette_token_text("palette.family.", model.family()));

    // -- THE PREVIEW. Large, and it is the same part the ghost in the world is
    //    holding: both come from model.selected(), which choose_axis sets.
    const std::size_t at = model.resolved_index();
    if (at < model.part_count()) {
        const PartFacets& f = model.part(at);
        const PartMeasure& m = model.measure(at);
        const EditorTexture tex =
            hooks.thumbnail ? hooks.thumbnail(f.name, static_cast<int>(PREVIEW_PX)) : 0;
        if (tex != 0) {
            EditorUi::image(tex, PREVIEW_PX, PREVIEW_PX);
        } else {
            ImGui::Dummy(ImVec2(PREVIEW_PX, 8.0f));
            ImGui::TextDisabled("%s", EditorUi::tr("palette.preview.waiting"));
        }
        ImGui::TextWrapped("%s", f.name.c_str());
        if (m.known) {
            ImGui::TextDisabled("%.2f × %.2f × %.2f %s", static_cast<double>(m.width_m),
                                static_cast<double>(m.height_m), static_cast<double>(m.depth_m),
                                EditorUi::tr("palette.unit.m"));
        }
        bool fav = model.is_favourite(f.name);
        if (ImGui::Checkbox(EditorUi::tr("palette.only_fav.mark"), &fav)) {
            model.toggle_favourite(f.name);
        }
    }

    // -- WHAT HAD TO MOVE, in words. The user asked for a property dial; a dial
    //    that silently changes a NEIGHBOURING dial is the one thing that would
    //    make him distrust the whole panel.
    if (!model.repaired_axes().empty()) {
        std::string moved;
        for (const PartAxis axis : model.repaired_axes()) {
            moved += (moved.empty() ? "" : ", ");
            moved += EditorUi::tr(axis_key(axis));
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s %s", EditorUi::tr("palette.repaired"), moved.c_str());
    }

    ImGui::Separator();
    for (const PartAxis axis : AXIS_ORDER) {
        draw_axis(model, axis);
    }
}

} // namespace dfn::app
