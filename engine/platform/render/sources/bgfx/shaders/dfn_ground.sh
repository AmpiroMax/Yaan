/*
 * Module: engine/platform/render
 * File: engine/platform/render/sources/bgfx/shaders/dfn_ground.sh
 *
 * Responsibility:
 * - THE GROUND'S COLOUR ABOVE THE TILE (REFERENCE_FRAMES.md R5): world-space
 *   value noise and the hue drift built on it. Everything here works in world
 *   METRES and has no period, which is the whole point — the material below it
 *   repeats every 8 m and this does not repeat at all.
 *
 * Key items:
 * - dfn_gnoise(): one octave of world-space value noise, metres in.
 * - dfn_ground_tint(): the R5 tint — hue moves, value barely does.
 *
 * Dependencies:
 * - Uses: nothing (pure arithmetic). Included by fs_terrain.sc.
 * - Used by: fs_terrain.sc.
 *
 * AI Agents Notice (must follow):
 * - Follow docs/ARCHITECTURE.md strictly.
 * - THIS FILE EXISTS BECAUSE OF A RULING IT MUST NOT BREAK. A previous
 *   render-side ground mottling was REMOVED by design (TerrainMesher UPD
 *   09:08:2026 - 14:11:37): it "painted large red-brown washes over ground core
 *   classifies as Grass", and a whole Grass sightline read as a 60 m brown
 *   flat. The failure was not the idea, it was the AMPLITUDE AND THE SCALE —
 *   one tone, big enough and different enough to become a second MATERIAL.
 *   The line between a tint and a material is not a matter of taste here, the
 *   project already owns it: §1.3b calls two regions SEPARATE at
 *   LANDMARK_SEPARATION_STEPS_MIN = 2 rulers of VALUE. So:
 *     hue may move freely; VALUE must stay far below the separation floor.
 *   Measured on the reference frames themselves (tools/measure_ground_colour.py,
 *   boxes in docs/acceptance/README.md), coarse-scale value spread in frames
 *   01/02/15 is 0.13-0.25 rulers — an order below the floor. The references are
 *   MULTI-HUE AND SINGLE-VALUE at the large scale, and that is the recipe.
 * - It also may not become a MATERIAL CLASS by the back door: no hard edges, no
 *   thresholds. Every mix here is smooth, so nothing in it can be mistaken for
 *   the shore mask or the slope rock, which are surface truth from core.
 */

#ifndef DFN_GROUND_SH
#define DFN_GROUND_SH

// Hash and value noise in WORLD METRES. Not the CPU ProcTexture hash: that one
// is an integer hash whose contract is byte-identical output across platforms,
// which a fragment shader cannot promise and does not need. What this one must
// be is CONTINUOUS (a discontinuity would print as a seam) and APERIODIC over
// the world, and it is both.
float dfn_ghash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float dfn_gnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep fade: C1, so no lattice creases
    float a = dfn_ghash(i);
    float b = dfn_ghash(i + vec2(1.0, 0.0));
    float c = dfn_ghash(i + vec2(0.0, 1.0));
    float d = dfn_ghash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// The R5 tint. `xz` is world position in metres, `amount` is the dose (0 = the
// zero-dose control arm, DFN_GROUND_TINT=0).
//
// THE TARGET HUES ARE READ OFF THE REFERENCE FRAMES, not invented: frame 02
// carries rust-red shrub over grey-brown earth, frame 01 pale tan, grey-green
// scrub and dark rock in one screen, frame 15 warm ash. They are expressed as
// MULTIPLIERS rather than colours so the tint rides whatever the material
// underneath is doing (a lit slope stays lit, a shaded hollow stays shaded) —
// a colour LERP toward a fixed rgb would flatten exactly the shading this
// change is supposed to be adding to.
//
// THREE SCALES, AND THE FINE ONE IS THERE BECAUSE THE REFERENCES PUT IT THERE.
// The first version of this function had only 19 m and 71 m, on the reasoning
// that the tile already owns everything smaller. Measured, that moved the
// frame's coarse chroma spread 1.80 -> 1.97 against a reference range of
// 2.3-7.5 and its fine spread not at all — because a 19 m field barely varies
// across the few metres of ground a walking eye actually sees. Frame 02's
// rust-red shrub patches are ONE TO THREE METRES; that is why its fine chroma
// spread is 21.5 against our 5.7. Hue has to live at the walking scale.
//
// THE PERIODS ARE NOT ROUND NUMBERS ON PURPOSE: 4.7 / 19 / 71 m, none of them
// a multiple of the material's 8 m tile, so no beat frequency lines up with
// the tile and prints as a new grid.
vec3 dfn_ground_tint(vec3 albedo, vec2 xz, float amount)
{
    float fine = dfn_gnoise(xz * (1.0 / 4.7) + vec2(5.1, 23.7));
    float meso = dfn_gnoise(xz * (1.0 / 19.0));
    float macro = dfn_gnoise(xz * (1.0 / 71.0) + vec2(37.2, 11.9));

    // Centred at zero so the tint has no mean: the average ground colour of the
    // world is unchanged, and only its VARIATION goes up. Without this the
    // change would read as "the grass got browner", which is a look decision
    // nobody made.
    // The big scale sets the REGION's character and the fine scale carries the
    // patchiness inside it, so a dry region gets rust patches rather than a
    // uniform rust wash — which is precisely the difference between frame 02
    // and the 60 m brown flat the removed mottling produced.
    float dry = ((macro - 0.5) * 1.1 + (fine - 0.5) * 0.9) * 2.0;
    float ash = ((meso - 0.5) * 1.0 + (fine - 0.5) * 0.7) * 2.0;

    // Per-channel gains. Red leads and blue trails on `dry` (that is what makes
    // ochre out of green); `ash` desaturates by lifting blue and dropping green.
    // The magnitudes are the HUE budget; the VALUE budget is enforced below.
    vec3 tint = vec3(1.0 + 0.30 * dry + 0.06 * ash,
                     1.0 + 0.02 * dry - 0.05 * ash,
                     1.0 - 0.14 * dry + 0.18 * ash);

    // THE VALUE CLAMP, AND IT IS THE RULING MADE ARITHMETIC. Renormalise the
    // gain so its luma (the quantiser's weights, the metric every brightness
    // rule here is written in) is exactly 1: whatever the hue does, the tinted
    // ground keeps the value the light gave it. A patch can therefore never
    // separate from its neighbour by the 2 rulers at which §1.3b says two
    // regions are different things — the only way this tint can produce a value
    // step at all is through the small residual of per-channel clipping.
    float w = dot(tint, vec3(0.30, 0.59, 0.11));
    tint /= max(w, 1e-4);

    // `amount` is NOT clamped above 1 on purpose: DFN_GROUND_TINT is how the
    // dose gets swept without a rebuild, and a dose sweep whose top end is the
    // shipped value cannot find out whether the shipped value is too small.
    return albedo * mix(vec3(1.0, 1.0, 1.0), tint, max(amount, 0.0));
}

#endif // DFN_GROUND_SH
