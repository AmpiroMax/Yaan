/*
Created: 13:08:2026 - 20:08:00
Last updated: 13:08:2026 - 20:08:00
Module: engine/app
File: engine/app/sources/HudScreen.cpp

Responsibility:
- The aiming mark's shape, and the reasoning for every number in it.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone ui owns this file.
*/
/*
UPD:
- 13:08:2026 - 20:08:00: Created (user request). Four ticks with a hole in the
  middle, outlined; the hole and the outline are each a measured decision, see
  the comments at the constants.
*/

#include "engine/app/sources/HudScreen.h"

#include <cstdlib>

#include "engine/render/sources/PixelCanvas.h"

namespace dfn::app {

namespace {

// THE INK is the interface's own, the same value the prompt and the readout
// are drawn in: the aiming mark is not a different kind of object from the
// words that appear next to it, and a second near-white would be a colour
// nobody chose.
constexpr render::Color INK{232, 228, 214};
// THE OUTLINE IS BLACK, one pixel all round. The mark is drawn over WHATEVER
// the player is facing -- sky, snow, torchlight -- and that is the same
// problem the text had, answered the way a LINE has to answer it. A plate
// cannot be the answer here: an opaque rectangle in the exact middle of the
// screen would cover the thing being aimed at, which is the one place on
// screen that must stay visible. The measurement is in
// docs/acceptance/README.md (ui-crosshair-*): ink-vs-covered fails the
// two-step rule over bright ground, ink-vs-what-it-abuts passes everywhere,
// because what it abuts is its own outline.
constexpr render::Color OUTLINE{0, 0, 0};

// THE HOLE IN THE MIDDLE, and it is the whole design. A dot marks the point
// and HIDES it: at 640x360 a distant door handle is a pixel, and a crosshair
// that covers the pixel you are aiming at answers "where am I pointing" by
// deleting the answer. Four ticks pointing inward name the same point and
// leave it visible. Morrowind's mark (reference frame 07) is this shape.
constexpr int GAP = 3;    // clear pixels between the centre and each tick
constexpr int TICK = 4;   // length of each tick, in internal pixels
constexpr int THICK = 1;  // one pixel: it is a mark, not a widget

} // namespace

bool crosshair_enabled() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_CROSSHAIR");
        return !(e != nullptr && e[0] == '0');
    }();
    return on;
}

void draw_crosshair(render::PixelCanvas& canvas) {
    if (!crosshair_enabled()) {
        return;
    }
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    if (w <= 0 || h <= 0) {
        return;
    }
    // THE CENTRE IS THE CENTRE OF THE CAMERA RAY, and on an even-sized grid
    // that falls BETWEEN pixels. Rounding down puts the mark half a pixel up
    // and left of true centre; the ticks are symmetric about the same rounded
    // point, so the shape stays symmetric even where the grid cannot be.
    const int cx = w / 2;
    const int cy = h / 2;

    // Outline first, then ink over it: the outline is the ink's rectangle
    // grown by one pixel on every side, so no ink pixel is ever left abutting
    // the world directly. Written as two passes rather than a per-pixel
    // dilation because the shape is four rectangles and a dilation would be a
    // clever way to draw the same twelve rows.
    const struct {
        int x, y, w, h;
    } ticks[4] = {
        {cx - GAP - TICK, cy, TICK, THICK},          // left
        {cx + GAP + 1, cy, TICK, THICK},             // right
        {cx, cy - GAP - TICK, THICK, TICK},          // up
        {cx, cy + GAP + 1, THICK, TICK},             // down
    };
    for (const auto& t : ticks) {
        canvas.fill_rect(t.x - 1, t.y - 1, t.w + 2, t.h + 2, OUTLINE);
    }
    for (const auto& t : ticks) {
        canvas.fill_rect(t.x, t.y, t.w, t.h, INK);
    }
}

} // namespace dfn::app
