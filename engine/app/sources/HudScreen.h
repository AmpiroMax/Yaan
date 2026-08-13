/*
Created: 13:08:2026 - 20:05:00
Last updated: 13:08:2026 - 20:05:00
Module: engine/app
File: engine/app/sources/HudScreen.h

Responsibility:
- The IN-GAME screen furniture that is not text: today, the aiming point. The
  playing frame carried exactly one interface element -- the interaction prompt
  -- and it stood in the middle of the screen naming a verb for a thing the
  player had no marked point to aim at. This file draws that point.

Key items:
- draw_crosshair(): the aiming mark, at the centre of the internal canvas.
- crosshair_enabled(): the dose door (DFN_CROSSHAIR=0), read once per run.

Dependencies:
- Uses: engine/render (PixelCanvas). No world knowledge and no input: it is
  handed a canvas and draws into it, so it is testable without a window.
- Used by: App (the HUD block), and the acceptance harness.

Notes:
- IT IS DRAWN IN INTERNAL PIXELS AND DOES NOT SCALE WITH THE WINDOW, because
  the internal grid IS the design grid: the whole look is a 640x360 (or
  320x180) picture integer-upscaled, and a crosshair sized in window pixels
  would be the one element on screen that is not made of the game's pixels.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone ui owns this file.
*/
/*
UPD:
- 13:08:2026 - 20:05:00: Created -- the aiming point (user request: the playing
  screen had no interface at all beyond the prompt, and the prompt named a verb
  in the middle of a screen with nothing marking the middle).
*/

#pragma once

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

// DFN_CROSSHAIR=0 removes the mark and changes nothing else: the counterfactual
// arm of the acceptance pair, so both frames come from ONE binary (Rule 47's
// caveat). Read once, like the plate's door -- a switch polled every frame lets
// two frames of one run disagree about what was measured.
[[nodiscard]] bool crosshair_enabled();

// Draws the aiming mark at the centre of `canvas` (the HUD layer, which the
// caller has already cleared). Silent when the door above is shut.
void draw_crosshair(render::PixelCanvas& canvas);

} // namespace dfn::app
