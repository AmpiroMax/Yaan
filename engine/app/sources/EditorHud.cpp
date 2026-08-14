/*
Created: 14:08:2026 - 18:57:03
Last updated: 14:08:2026 - 18:57:03
Module: engine/app
File: engine/app/sources/EditorHud.cpp

Responsibility:
- Composition and layout of the editor viewer's overlay block. See the header
  for why it knows nothing about the renderer.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 14:08:2026 - 18:57:03: Создан вместе с заголовком — вынос блока из App.cpp и
  разведение с отладочным выводом. Тексты строк перенесены БЕЗ ИЗМЕНЕНИЙ:
  правка геометрическая, и смешивать её с правкой формулировок значило бы
  сдавать два изменения под одним доказательством.
*/

#include "engine/app/sources/EditorHud.h"

#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace dfn::app {
namespace {

// THE BLOCK'S CORNER. Left-aligned like the readout it stacks under: the
// alternative -- right-aligning it against the far edge -- was rejected because
// line 3 changes width every time the crosshair moves, and a right-aligned line
// that changes width makes the whole block slide sideways while the player is
// trying to read it.
constexpr int BLOCK_X = 4;
// A row of air between the readout's plate and this block's first line, so the
// two read as two panels rather than as one panel with a seam.
constexpr int BLOCK_GAP = 4;
constexpr int PLATE_PAD = 3; // draw_text_plate's default

[[nodiscard]] std::string loc_str(const char* key) {
    return std::string(localized(serialization::fnv1a64(key)));
}

// THE DOSE DOOR (Rule 47's one-binary clause): DFN_EDITOR_HUD_PINNED=1 puts the
// block back where it was -- nailed to (4, 4) on top of the readout -- so the
// before frame and the after frame come out of ONE build rather than two builds
// an hour apart in a tree six zones are compiling. Without it the "before" of
// this fix is unphotographable the moment the fix lands, and a layout claim
// with no before is a claim nobody can check.
//
// Read ONCE per process: a door polled every frame is a switch, and a switch
// inside an instrument lets two frames of one run disagree about what was
// tested. Same reasoning, same shape, as DFN_UI_PLATE next door.
[[nodiscard]] bool pinned_to_corner() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_EDITOR_HUD_PINNED");
        return e != nullptr && e[0] == '1';
    }();
    return on;
}

} // namespace

int editor_hud_row_h() { return render::FONT_CELL_H + 2; }

int editor_hud_x() { return BLOCK_X; }

int editor_hud_block_height_px(size_t line_count) {
    if (line_count == 0) {
        return 0;
    }
    // Each line carries its own plate, so the block's extent runs from the top
    // of the first plate to the bottom of the last one.
    return static_cast<int>(line_count - 1) * editor_hud_row_h()
         + render::FONT_INK_H + 2 * PLATE_PAD;
}

std::vector<std::string> editor_hud_lines(const EditorHudSnapshot& snap) {
    std::vector<std::string> lines;
    lines.reserve(3);

    char num[32];

    // LINE 1 -- the banner. Says which mode this is and how fast the free
    // camera moves, which are the two things that are not visible from the
    // picture itself.
    std::snprintf(num, sizeof(num), "%.1f", static_cast<double>(snap.fly_speed_mps));
    lines.push_back(loc_str("editor.banner") + " " + num + " "
                    + loc_str("editor.speed_unit"));

    // LINE 2 -- the frame's cost.
    std::string frame = loc_str("editor.hud.tris") + " "
                      + std::to_string(snap.frame_triangles) + "   "
                      + loc_str("editor.hud.draws") + " "
                      + std::to_string(snap.frame_draws);
    if (snap.wireframe) {
        frame += "   [" + loc_str("editor.hud.wire") + "]";
    }
    lines.push_back(std::move(frame));

    // LINE 3 -- what the crosshair is on.
    std::string aim = loc_str("editor.hud.aim") + ": ";
    if (snap.aim_hit) {
        std::snprintf(num, sizeof(num), "%.1f", static_cast<double>(snap.aim_distance_m));
        aim += std::to_string(snap.aim_triangles) + " " + loc_str("editor.hud.tris")
             + "   " + num + " " + loc_str("editor.hud.m");
        if (snap.aim_pick_id != 0) {
            aim += "   id " + std::to_string(snap.aim_pick_id);
        }
    } else {
        aim += loc_str("editor.hud.aim_none");
    }
    lines.push_back(std::move(aim));

    // NOT NARROWED, AND SAID OUT LOUD RATHER THAN LEFT TO BE ASSUMED. These
    // three lines are short enough today at every resolution the settings page
    // offers, which the test measures rather than asserts by eye -- but "short
    // enough" is a property of this wording, and the wording is about to
    // change. When it does, the short variants land with it and this is where
    // fits_width() goes.
    return lines;
}

int draw_editor_hud(render::PixelCanvas& canvas, const EditorHudSnapshot& snap,
                    int top_y) {
    const render::Color ink{244, 226, 160};
    const std::vector<std::string> lines = editor_hud_lines(snap);

    int y = top_y;
    for (const std::string& line : lines) {
        draw_text_plate(canvas, BLOCK_X, y, render::text_width_px(line),
                        render::FONT_INK_H);
        render::draw_text(canvas, BLOCK_X, y, line, ink, /*shadow=*/true);
        y += editor_hud_row_h();
    }
    return y - editor_hud_row_h() + render::FONT_INK_H + PLATE_PAD;
}

int editor_hud_top_y(int readout_bottom_y) {
    // The door's whole effect is here: the block either follows the readout or
    // ignores it, which is exactly the difference the two frames show.
    return pinned_to_corner() ? BLOCK_X : readout_bottom_y + BLOCK_GAP;
}

} // namespace dfn::app
