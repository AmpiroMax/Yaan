/*
Created: 17:08:2026 - 20:06:53
Last updated: 17:08:2026 - 20:06:53
Module: engine/editor
File: engine/editor/sources/EditorBrushView.h

Responsibility:
- THE BRUSH PANEL: what the sculptor sets before he touches the ground — the
  mode, the size, the strength, the surface he is painting — and what he sets
  before he plants — the species, how many, how varied.

Key items:
- BrushHooks: the two things the panel cannot answer for itself.
- make_brush_panel(): the EditorPanel declaration EditorUi is handed.
- draw_brush_panel(): the content alone, for a caller with its own window.

WHY THE PANEL OWNS NO STATE. It edits a TerrainBrush and a PlantBrush the app
owns, in place. A panel holding its own copy would be a second set of settings,
and the stroke would come out at the size of whichever copy the code reached
first — a defect that looks like the slider not working.

THE ONE RULE THIS PANEL EXISTS TO RESPECT: a dab must never fire while the
pointer is over these widgets. That is not enforced here — EditorUi::wants_mouse
answers it and the caller obeys it — but it is why the panel is declared through
EditorUi rather than drawn wherever it likes. «Настроил кисть и случайно выкопал
яму» happens on the first afternoon, and it happens once per builder.

Dependencies:
- Uses: EditorBrush.h (the settings it edits), EditorUi.h, Dear ImGui (allowed
  in engine/editor and nowhere else).
- Used by: engine/app (App wires it into EditorUi).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE PANEL NEVER OPENS ITS OWN WINDOW. EditorUi owns Begin/End, the position
  and the size; this draws CONTENT. Three agents write panels and the editor has
  to look like one tool.
- Every string goes through EditorUi::tr() (Rule 5). A Russian literal here is
  both a rule violation and the first line that renders without glyphs.
*/
/*
UPD:
- 17:08:2026 - 20:06:53: Создан — панель кисти на контракте каркаса.
*/

#pragma once

#include "engine/editor/sources/EditorBrush.h"
#include "engine/editor/sources/EditorUi.h"

#include <functional>
#include <string>
#include <vector>

namespace dfn::app {

/// WHAT THE PANEL CANNOT ANSWER ITSELF.
struct BrushHooks {
    /// The species the shelves actually carry, for the planting list. Called
    /// once per frame while the planting section is open; the app is expected
    /// to keep the vector, since reading a directory per frame is a directory
    /// read per frame.
    std::function<const std::vector<std::string>&()> species;

    /// THE STROKE'S OWN NUMBERS, for the readout: how many lattice samples the
    /// last dab moved and by how much. It is shown because "the ground looks
    /// higher" is what a builder can see for himself, and "31 samples, worst
    /// 0.42 m" is what he can act on — and because a brush that has silently
    /// stopped biting looks exactly like a brush aimed at nothing.
    std::function<void(int& samples, float& worst_m)> last_dab;
};

/// Declares the brush panel for EditorUi. `terrain`, `plant` and `hooks` must
/// outlive the panel (App owns them). It starts CLOSED: a panel the builder has
/// to ask for is a panel that is not in his way while he is doing something
/// else.
[[nodiscard]] EditorPanel make_brush_panel(TerrainBrush& terrain, PlantBrush& plant,
                                           BrushHooks hooks,
                                           EditorPanelSide side = EditorPanelSide::Right);

/// The content alone, for a caller that owns its window (the capture door).
/// Normal callers use make_brush_panel().
void draw_brush_panel(TerrainBrush& terrain, PlantBrush& plant, const BrushHooks& hooks);

} // namespace dfn::app
