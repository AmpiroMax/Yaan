/*
Created: 14:08:2026 - 18:57:03
Last updated: 14:08:2026 - 19:05:56
Module: engine/app
File: engine/app/sources/EditorHud.h

Responsibility:
- The editor viewer's own overlay block: which lines it shows, what they say,
  how wide they are and where they sit relative to the debug readout. Data in,
  laid-out strings out.

Key items:
- EditorHudSnapshot: the numbers the block reports, as plain values.
- editor_hud_lines(): the block's text, localized and narrowed to a width.
- editor_hud_block_height_px() / draw_editor_hud(): its layout and its draw.

Dependencies:
- Uses: engine/app Localization + DebugOverlay (the shared text plate),
  engine/render (PixelCanvas, BitmapFont).
- Used by: App, and the editor panels that come after this one.

Notes:
- THIS MODULE KNOWS NOTHING ABOUT THE RENDERER, THE WINDOW OR THE ECS, and that
  is the constraint that makes it worth extracting rather than a tidier place
  to put the same code. The block used to be composed inline in App.cpp, which
  owns a window and therefore cannot be tested -- so the ONLY way to find out
  whether a line ran off the edge of a 640x360 frame was to run the game and
  look. That is how the overlap this file exists to fix survived: nothing could
  measure it. The snapshot is filled by the caller from RenderFrameStats /
  RenderPick, so a test builds the worst case by hand and the assertion is made
  against the same strings the frame draws.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- Every user-facing word here is a localization KEY (Rule 5). A literal string
  that reaches the screen is a violation, and this block is all screen.
*/
/*
UPD:
- 14:08:2026 - 18:57:03: Создан. Вынос редакторского блока из App.cpp (жалоба
  пользователя: «накладывается телеметрия рыжая с текстом трисс и та что
  открывается по кнопке 2»). Причина была не в оформлении, а в том, что углом
  владели двое: отладочный вывод рисуется в (3,3), а баннер редактора был
  прибит в (4,4) — оба верны поодиночке, вместе нечитаемы, а рабочий режим
  пользователя — оба сразу. Блок переехал ПОД вывод, отступ считается от его
  настоящей высоты (она непостоянна: в воде вывод на строку выше), и ширина
  каждой строки теперь ЗАМЕРЯЕТСЯ тестом, а не глазами.
- 14:08:2026 - 19:05:56: Строки названы по-человечески (жалоба пользователя: «с
  текстом трисс (что это такое)»). editor_hud_lines() снова принимает ширину:
  у каждой строки теперь ПОЛНАЯ форма-предложение и КОРОТКАЯ, и выбор между
  ними — замер собранной строки, а не ветка по разрешению. Полная существует
  потому, что на вопрос «что это такое» мнемоника не отвечает; короткая —
  потому, что 320x180 страница настроек предлагает соседней строкой.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

// WHAT THE EDITOR BLOCK REPORTS. Plain numbers, deliberately: the caller reads
// RenderFrameStats / RenderPick off the renderer and copies the fields in, so
// this module never includes the render interface and a test never needs one.
struct EditorHudSnapshot {
    // The free camera's wheel-driven speed. The banner's whole job is to say
    // "you are flying" and "how fast".
    float fly_speed_mps = 0.0f;

    // THE FRAME. Triangles submitted into the scene view and the backend's
    // all-view draw-call total -- the two halves of "why is this heavy".
    uint32_t frame_triangles = 0;
    uint32_t frame_draws = 0;
    bool wireframe = false;

    // WHAT THE CROSSHAIR IS ON. `aim_pick_id` is RAW, exactly as the renderer
    // stamped it; 0 is the contract's "unnamed" sentinel (terrain, sky and the
    // LOD nodes all submit without a name).
    bool aim_hit = false;
    uint32_t aim_triangles = 0;
    float aim_distance_m = 0.0f;
    uint32_t aim_pick_id = 0;
};

// THE BLOCK'S LINES, localized and narrowed to `width_px`, in draw order, top
// to bottom. Never empty.
//
// EACH LINE EXISTS IN A FULL AND A SHORT FORM, and which one comes back is
// decided by MEASURING the assembled string, not by branching on a resolution.
// The full form is a sentence because the user asked what the abbreviation
// meant; the short form exists because 320x180 is a rung the settings page
// offers one keypress away and a sentence does not fit in it. Measuring the
// assembled string rather than the wording is what makes the choice account
// for the numbers, which are unbounded and translated by nobody.
[[nodiscard]] std::vector<std::string> editor_hud_lines(const EditorHudSnapshot& snap,
                                                        int width_px);

// Row pitch and total height of a block of `line_count` lines, plate included.
// Published so the caller can ask whether the block clears what is under it
// without re-deriving the arithmetic that draws it.
[[nodiscard]] int editor_hud_row_h();
[[nodiscard]] int editor_hud_block_height_px(size_t line_count);

// WHERE THE BLOCK STARTS, given where the debug readout ended
// (debug_overlay_bottom_y). This is the whole fix for the overlap the user
// reported: the two blocks are laid out in SEQUENCE by one arithmetic instead
// of being pinned to the same corner by two.
[[nodiscard]] int editor_hud_top_y(int readout_bottom_y);

// The block's left edge. One pixel right of the readout's, which is not worth
// aligning: they no longer share a row.
[[nodiscard]] int editor_hud_x();

// Draws the block with its first line at `top_y`. Returns the y just below it,
// so a future panel can stack under this one the same way this one stacks under
// the readout -- rather than by guessing an offset, which is the defect this
// module was created to end.
int draw_editor_hud(render::PixelCanvas& canvas, const EditorHudSnapshot& snap,
                    int top_y);

} // namespace dfn::app
