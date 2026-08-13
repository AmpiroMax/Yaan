/*
Created: 13:08:2026 - 19:35:00
Last updated: 13:08:2026 - 20:00:00
Module: engine/app
File: engine/app/sources/HudScreen.h

Responsibility:
- The IN-GAME screen furniture that is not text: today, the aiming point. The
  playing frame carried exactly one interface element -- the interaction prompt
  -- and it stood in the middle of the screen naming a verb for a thing the
  player had no marked point to aim at. This file draws that point.

Key items:
- HudFacts: what the app KNOWS (third person? map up?), never what it decides.
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
- 13:08:2026 - 19:35:00: Created -- the aiming point (user request: the playing
  screen had no interface at all beyond the prompt, and the prompt named a verb
  in the middle of a screen with nothing marking the middle).
- 13:08:2026 - 20:00:00: Прицел прячется в третьем лице и над картой, и условие
  живёт ЗДЕСЬ, а не в вызове: приложение передаёт факты, решение — зоны ui.
*/

#pragma once

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

// WHAT THE APP KNOWS, HANDED OVER AS FACTS. The app owns the camera mode and
// the map; the DECISION about what the aiming mark does in each is ui's, and it
// lives in draw_crosshair() rather than in an `if` at the call site. Two
// reasons, and the second is the one that matters: a condition written at the
// call site is invisible to everyone reading this file, so the next person to
// add a screen has no way to see that the rule exists.
struct HudFacts {
    bool third_person = false; // the camera is behind the character
    bool map_open = false;     // the full-screen map plate is up
};

// DFN_CROSSHAIR=0 removes the mark and changes nothing else: the counterfactual
// arm of the acceptance pair, so both frames come from ONE binary (Rule 47's
// caveat). Read once, like the plate's door -- a switch polled every frame lets
// two frames of one run disagree about what was measured.
[[nodiscard]] bool crosshair_enabled();

// Draws the aiming mark at the centre of `canvas` (the HUD layer, which the
// caller has already cleared). Returns whether anything was drawn, so the
// caller can keep the HUD layer hidden when it is empty -- a layer that is
// always "visible" makes every later "is anything on screen?" question lie.
//
// SILENT in third person and over the map, and both are the same rule rather
// than two: the mark names WHERE THE CAMERA RAY POINTS, and in third person the
// ray no longer starts at the eye the player is aiming with, while the map is a
// full-screen plate whose own centre is already the thing being looked at. A
// crosshair there is a second centre of attention on a screen that has one.
//
// THE DEFAULT ARGUMENT IS TEMPORARY AND IT IS HERE FOR THE TREE, NOT FOR THE
// DESIGN: the call site is the lead's file and lands one patch later, and a
// signature change that breaks six zones' builds for an hour is a worse thing
// than a default that shows the mark where the app has not yet said otherwise.
// It comes out the moment App.cpp passes the facts.
bool draw_crosshair(render::PixelCanvas& canvas, const HudFacts& facts = {});

} // namespace dfn::app
