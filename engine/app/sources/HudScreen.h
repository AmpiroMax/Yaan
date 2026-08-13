/*
Created: 13:08:2026 - 19:35:00
Last updated: 13:08:2026 - 20:55:00
Module: engine/app
File: engine/app/sources/HudScreen.h

Responsibility:
- The IN-GAME screen furniture: the aiming point, the direction ribbon along the
  top, and the three condition bars. The playing frame carried exactly one
  interface element -- the interaction prompt -- and it stood in the middle of a
  screen with nothing marking the middle, no way to tell which way you faced,
  and nothing saying how you were doing.

Key items:
- HudFacts: what the app KNOWS (facing, camera mode, map, condition), never
  what it decides.
- draw_crosshair(): the aiming mark, at the centre of the internal canvas.
- draw_compass_ribbon(): the sliding strip of directions along the top.
- draw_condition_bars(): health / stamina / magicka, bottom left.
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
- 13:08:2026 - 20:05:00: Временное умолчание аргумента снято — вызов в App.cpp
  передаёт факты (правка ведущего). Умолчание стояло ровно один коммит и ровно
  затем, чтобы дерево шести зон не собирало сломанное между двумя правками.
- 13:08:2026 - 20:40:00: ЛЕНТА-КОМПАС и ТРИ ПОЛОСЫ СОСТОЯНИЯ — дословный ответ
  пользователя («лента компас», «полосы здоровья/сил/магии», и ничего больше из
  предложенного). Лента растянута на СОБСТВЕННЫЙ угол обзора камеры, а не на
  условные 180°, чтобы метка на ленте стояла там же по горизонтали, где предмет
  на экране. Полосы полны и убыль не изображают: тратить их пока нечем.
- 13:08:2026 - 20:55:00: Двери дозы DFN_HUD_RIBBON=0 и DFN_HUD_BARS=0 — обе руки
  приёмки ленты и полос обязаны выходить из ОДНОГО бинарника, как у прицела.
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
    bool debug_readout = false; // the F3 readout is up (it owns the top-left)

    // WHERE THE PLAYER IS LOOKING. Sim's convention, named here because a
    // compass that guesses it is a compass that points the wrong way: yaw 0 is
    // -Z (north), positive turns clockwise seen from above, so +X is east. The
    // same convention the debug readout's compass word is derived from.
    float yaw_rad = 0.0f;
    // The camera's VERTICAL field of view. The horizontal one is derived from
    // it and the canvas's own aspect, so the ribbon needs no second number and
    // cannot be handed an aspect that disagrees with the picture it draws on.
    float fov_y_rad = 1.309f;

    // CONDITION, 0..1 each, and they default to FULL because that is the truth
    // today: there is no combat, no spell and no exhaustion in the game yet, so
    // nothing spends them. The bars are built for the future and DO NOT act out
    // a depletion that is not happening -- a bar that drains for show teaches
    // the player to read a number that means nothing (the user asked for them
    // knowing this; a full bar that is honestly full is worth more than an
    // animation).
    float health = 1.0f;
    float stamina = 1.0f;
    float magicka = 1.0f;
};

// DFN_CROSSHAIR=0 removes the mark and changes nothing else: the counterfactual
// arm of the acceptance pair, so both frames come from ONE binary (Rule 47's
// caveat). Read once, like the plate's door -- a switch polled every frame lets
// two frames of one run disagree about what was measured.
[[nodiscard]] bool crosshair_enabled();
// The same door for the other two elements, one each and read the same way.
// Three doors of one shape rather than one door with a syntax: the arm of an
// acceptance pair must be readable in a shell line by whoever repeats it, and
// `DFN_HUD_RIBBON=0` is readable in a way that `DFN_HUD=cb` is not.
[[nodiscard]] bool compass_ribbon_enabled();
[[nodiscard]] bool condition_bars_enabled();

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
bool draw_crosshair(render::PixelCanvas& canvas, const HudFacts& facts);

// THE DIRECTION RIBBON along the top (the user's request, in his words: "лента
// компас"). Cardinal marks slide across a strip as the head turns, exactly as
// the world does underneath: the strip spans the camera's own horizontal field
// of view, so a direction sitting a third of the way to the right edge of the
// RIBBON is the direction sitting a third of the way to the right edge of the
// SCREEN. That is the property that makes it readable without thinking, and it
// is why the ribbon takes the fov rather than a fixed 180 degrees.
//
// Silent over the map (it has north on it already) and silent while the debug
// readout is up -- the readout occupies the top and NAMES the direction in
// words, so there the ribbon would be a second answer to a question already
// answered, drawn over the answer.
bool draw_compass_ribbon(render::PixelCanvas& canvas, const HudFacts& facts);

// THE THREE CONDITION BARS (the user picked exactly these from four offers:
// health, stamina, magicka -- no clock, no held item). Bottom left, in a fixed
// order, because order is the only thing telling them apart for a player who
// does not separate the red from the green.
bool draw_condition_bars(render::PixelCanvas& canvas, const HudFacts& facts);

} // namespace dfn::app
