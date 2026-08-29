/*
Module: engine/editor
File: engine/editor/sources/EditorPropsView.h

Responsibility:
- THE PROPERTIES COLUMN: what the thing under the crosshair IS, as numbers a
  builder can type over. Position, turn, size — and the judge's answer when a
  typed number is refused.

Key items:
- PropsModel: the selection, held as plain numbers the panel edits IN PLACE.
- PropsHooks: apply / delete / clear — the three verbs the panel cannot do
  itself, because doing them means touching the composition and the world.
- make_props_panel(): the EditorPanel declaration EditorUi is handed.

WHY THIS EXISTS (user, 17.08.2026): «справа должно окно рисоваться с его
характеристиками, я должен уметь их менять» — quoted in EditorUi.h as the
reason the whole ImGui layer was raised, and then not built. And the second
half, 17.08 evening: «состояние на R меняется, но инструменты не рисуются, не
понятно что сейчас я делаю и что». The select mode had nothing to show.

THE PANEL NEVER WRITES THE SCENE. It moves numbers in a struct and calls
`apply`; App runs them through EditorPlant::edit_placement, which RE-JUDGES and
puts the placement back on a refusal. That split is not tidiness: an editor that
applies what the judge forbids has taught the builder that red means nothing,
and from that moment it means nothing.

AND THE REFUSAL IS SHOWN, not swallowed. A typed number that quietly snaps back
is indistinguishable from a text field that does not work — the builder retypes
it twice and then stops trusting the panel.

Dependencies:
- Uses: EditorUi.h (the panel contract, tr()), Dear ImGui in the .cpp.
- Used by: engine/app (App wires it to the selection and to the judge).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO Begin/End HERE. EditorUi owns the window; this draws content.
- Every visible string goes through EditorUi::tr() (Rule 5).
*/

#pragma once

#include "engine/editor/sources/EditorUi.h"

#include <functional>
#include <string>

namespace dfn::app {

/// Stable id, so the panel can be opened by whoever owns the selection.
inline constexpr const char* PROPS_PANEL_ID = "props";

/// THE SELECTION, AS NUMBERS. App owns it and keeps it fresh; the panel edits
/// these fields in place and never holds a copy — a panel with its own copy is
/// a second set of coordinates, and the one that gets applied is whichever the
/// code reached first.
struct PropsModel {
    /// Empty = nothing is selected, and then the panel says so instead of
    /// showing a row of zeroes that look like a thing at the origin.
    std::string object;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    /// DEGREES here, radians in the world. The conversion lives at the seam in
    /// App because a builder types 90, not 1.5708 — and because a panel that
    /// showed radians would be a panel he does arithmetic in front of.
    float yaw_deg = 0.0f;
    float scale = 1.0f;
    /// What the judge said when the last edit was refused, in the builder's
    /// language. Empty means the last edit landed.
    std::string refusal;
    /// Measured extent of the object, for the size line. Zero = unknown.
    float width_m = 0.0f;
    float depth_m = 0.0f;
    float height_m = 0.0f;
    /// What this part belongs to, empty = it stands alone. Shown because it is
    /// the field that decides WHICH RULES JUDGE IT: a lone part must stand on
    /// the ground, a member of a building may rest on another member and must
    /// then answer to the joints instead.
    std::string group;
};

/// THE THREE VERBS THE PANEL CANNOT DO ITSELF. Each one touches the
/// composition, the judge and a baked tile — none of which this layer may see.
struct PropsHooks {
    /// Push the model's numbers into the world. Returns false and fills
    /// `model.refusal` when the judge refused; the model is then left showing
    /// what the builder typed, not what the world kept, so he can correct it.
    std::function<bool()> apply;
    /// Remove the selected placement.
    std::function<void()> remove;
};

/// Declares the properties column for EditorUi. `model` and `hooks` must
/// outlive the panel (App owns both). It starts CLOSED and is opened by the
/// selection: a column showing "nothing selected" is a column in the way.
[[nodiscard]] EditorPanel make_props_panel(PropsModel& model, PropsHooks hooks,
                                           EditorPanelSide side = EditorPanelSide::Right);

/// The content alone, for a caller that owns its window.
void draw_props_panel(PropsModel& model, const PropsHooks& hooks);

} // namespace dfn::app
