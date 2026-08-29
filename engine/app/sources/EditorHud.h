/*
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
    // stamped it -- pass it in unmodified and let aim_entity_index() undo the
    // stamp, so the inversion has one home instead of one per caller.
    bool aim_hit = false;
    uint32_t aim_triangles = 0;
    float aim_distance_m = 0.0f;
    uint32_t aim_pick_id = 0;

    // МЫШЬ И УГОЛ, ОДНОЙ СТРОКОЙ НА ЭКРАНЕ. Заказ пользователя 18.08: «добавь
    // себе дебажного вывода о изменений координаты мыши и угле на который я
    // смотрю». Строка отвечает на вопрос, который иначе разбирается только
    // запуском игры руками: ПРИШЛО ли смещение (mouse_dx/dy) и ПОВЕРНУЛОСЬ ли
    // от него что-нибудь (yaw/pitch). Раньше эти две вещи были неразличимы, и
    // «камера не крутится» одинаково значило и «мышь не дошла», и «камера
    // проигнорировала» — три захода подряд их разделял человек, а не прибор.
    //
    // Показывается и состояние курсора: захвачен ли он окном (captured) и отдан
    // ли интерфейсу клавишей R (free). Именно захват и оказался поломкой:
    // App просил его каждым кадром, а платформа на каждый запрос сбрасывала
    // «предыдущее положение известно» и отдавала нулевое смещение.
    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;
    float yaw_deg = 0.0f;
    float pitch_deg = 0.0f;
    bool cursor_captured = false;
    bool cursor_free = false;
};

// THE PICK ID, UNDONE. engine/render stamps DrawParams::pick_id =
// EntityId.index + 1 (RenderSystem.cpp): the +1 keeps entity slot 0 -- a real
// slot -- from colliding with the "0 = unnamed" sentinel the IRenderer contract
// reserves. So the raw value is one MORE than the entity it names, and the
// overlay printed it raw: every object in the world was reported under the
// wrong number, and the number looked entirely plausible, which is why nobody
// caught it by reading the screen.
//
// Returns false for the sentinel, which is not an entity and must not be shown
// as one: terrain, sky and the LOD nodes all submit unnamed, and "объект -1"
// (or "объект 0", naming a real slot nobody is looking at) is worse than saying
// nothing. THIS IS WHY THE FUNCTION RETURNS A BOOL rather than a number -- an
// out-of-band value would be exactly the sentinel confusion one level up.
[[nodiscard]] bool aim_entity_index(uint32_t pick_id, uint32_t& out_index);

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
//
// ВЫСОТА КАДРА — ВТОРОЙ ДОВОД, И ОНА ЗДЕСЬ НЕ ДЛЯ СИММЕТРИИ. Строка «мышь и
// угол» нужна человеку, который выясняет, почему не крутится камера, но на
// 320x180 четвёртая строка выталкивает блок на подсказку снимка внизу — ровно
// то наложение, на которое он уже жаловался («кнопки сверху пересекаются с
// дебаг текстом»). Поэтому решение принимается ЗДЕСЬ и по числам: строка
// добавляется, только если блок с ней всё ещё расходится с подсказкой в самом
// высоком состоянии вывода (в воде он на строку выше). Вынести этот расчёт в
// App значило бы вернуть его туда, где его не достаёт ни один прибор.
[[nodiscard]] std::vector<std::string> editor_hud_lines(const EditorHudSnapshot& snap,
                                                        int width_px, int height_px);

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
