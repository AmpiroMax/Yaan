/*
Module: engine/editor
File: engine/editor/sources/EditorPaletteView.h

Responsibility:
- THE OBJECT MENU AS A PANEL. Turns PaletteModel into the thing the user asked
  for: a window he opens with a key, stands still in front of, and clicks a
  block out of. Nothing here decides anything — every question it asks is
  answered by the model, which is why the model is testable and this is not.

Key items:
- PaletteHooks: the two things the panel cannot get for itself — a thumbnail
  and a measurement. Both are the app's business (they need the registry and an
  offscreen target), both are asked for ONLY for rows about to be drawn.
- make_parts_panel(): the EditorPanel declaration App hands to EditorUi.

WHY A SEPARATE FILE FROM THE MODEL: this one includes ImGui and cannot be
instantiated without a context; the model must be provable without a window.
Keeping them together would make the tested half untestable.

Dependencies:
- Uses: EditorPalette.h, EditorUi.h (the panel contract), Dear ImGui (in the
  .cpp only).
- Used by: engine/app (App).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE PANEL NEVER OPENS ITS OWN WINDOW. EditorUi owns Begin/End, the position
  and the size; this draws CONTENT. Three agents write panels and the editor has
  to look like one tool.
- Every string goes through EditorUi::tr() (Rule 5). The one exception is a
  part's own tokens (family, material, tag) when the table has no row for them:
  those fall back to the RAW TOKEN, because a material baked this morning must
  read as "clay" rather than as a placeholder nobody can act on. That is data
  passing through, not interface text written in C++.
- ASK FOR A THUMBNAIL ONLY FOR VISIBLE ROWS. 2411 offscreen renders is a hitch
  nobody asked for, and the list is clipped precisely so it never happens.
*/

#pragma once

#include "engine/editor/sources/EditorPalette.h"
#include "engine/editor/sources/EditorUi.h"

#include <functional>
#include <string>

namespace dfn::app {

/// WHAT THE PANEL CANNOT ANSWER ITSELF. Both are optional: an empty hook makes
/// the panel fall back to a name-only row, which is a poorer menu and still a
/// working one.
struct PaletteHooks {
    /// A picture of this part, or 0 while it is not ready.
    ///
    /// `size_px` is a WISH, not a demand: the panel asks for what it is about
    /// to draw and will scale whatever it gets. Cache by NAME alone — caching
    /// by (name, size) would put one part in the atlas three times, at 44 in
    /// the strips, 96 in the tiles and 192 in the preview.
    ///
    /// RETURNING 0 IS NORMAL, not an error: it means "not ready yet", and the
    /// panel asks again next frame. Keep a per-frame budget on the app side —
    /// the list is clipped, so only visible rows ask, but the first frame after
    /// the menu opens has two or three dozen of them visible at once, and that
    /// is exactly the frame a builder is watching.
    std::function<EditorTexture(const std::string& name, int size_px)> thumbnail;

    /// THE BUDGET'S CLOCK. Called once, at the top of the panel's draw, so
    /// whoever answers `thumbnail` knows a new frame has begun and may refill
    /// its allowance. It lives here rather than in the app's frame loop for a
    /// reason worth keeping: the only frames in which a thumbnail is asked for
    /// are the frames this panel draws, so this is the exact place "per frame"
    /// means something. Optional, like the rest.
    std::function<void()> begin_frame;

    /// The part's measured extent and triangle count (render::measure_object).
    /// Same contract as PaletteModel::MeasureFn; wired straight through.
    PaletteModel::MeasureFn measure;

    /// Called when the builder picks a part, AFTER the model has recorded it.
    /// This is how the build hand learns what the ghost should be.
    std::function<void(const std::string& name)> on_pick;
};

/// Declares the object menu for EditorUi. `model` and `hooks` must outlive the
/// panel (App owns both). `side` is a parameter rather than a constant because
/// the user asked for the menu on the RIGHT and the properties column wants the
/// same wall — whoever lays the editor out decides, in one place.
[[nodiscard]] EditorPanel make_parts_panel(PaletteModel& model, PaletteHooks hooks,
                                           EditorPanelSide side = EditorPanelSide::Right);

/// The panel's content, exposed for the rare case App wants it inside its own
/// window (the capture door draws the menu without the editor frame). Normal
/// callers use make_parts_panel().
void draw_parts_panel(PaletteModel& model, const PaletteHooks& hooks);

} // namespace dfn::app
