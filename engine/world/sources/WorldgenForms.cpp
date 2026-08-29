/*
Module: engine/world
File: engine/world/sources/WorldgenForms.cpp

Responsibility:
- Implementation of the bench/riser operator (§10.1.3 F7's subject).

Key items:
- terrace_forms.

Dependencies:
- Uses: WorldgenForms.h, WorldgenMacro.h (stream ids), WorldgenNoise.h.
- Used by: Worldgen.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic and position-based (Rule 13.1).
*/

#include "engine/world/sources/WorldgenForms.h"

#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace dfn::world {

namespace {

/// REQUESTED NUMBERS ROWS (Rule 35 — stated here beside the term each one
/// scales, measured before they are asked for). Every one of them is DERIVED
/// against §10.1.3's own geometry, which is the only arithmetic in this file
/// that matters:
///
///   a pocket of hidden ground opens behind a crest at distance d exactly when
///   the ground beyond it falls faster than atan(EYE / d) — 9.7 deg at 10 m,
///   4.9 deg at 20 m, 3.2 deg at 30 m, 1.6 deg at 60 m.
///
/// So a riser must clear ~10 deg to work at the near end of the 5-60 m band,
/// and the numbers below are chosen to put the TYPICAL riser at 12-20 deg on
/// the ground this world actually has.
constexpr float STEP_MIN = 0.9f;   ///< bench-to-bench rise (m), low end
constexpr float STEP_MAX = 1.8f;   ///< ...and high end. A step under ~0.8 m is
                                   ///< eaten by the 2 m heightmap the world is
                                   ///< DRAWN from; a step over ~2 m starts
                                   ///< building walls out of the lowland.
constexpr float STEP_CELL = 320.0f; ///< the step height drifts on this cell, so
                                    ///< the country changes character slowly
constexpr float DATUM_CELL = 512.0f; ///< where the level pile is anchored. Same
                                     ///< cell as WORLDGEN_OCTAVE1, so the datum
                                     ///< drifts with the landform rather than
                                     ///< against it; over any 60 m sight line it
                                     ///< is effectively constant, which is what
                                     ///< keeps a riser on ONE contour
constexpr float STRENGTH_MIN = 0.45f; ///< how much of the bench's gradient is
constexpr float STRENGTH_MAX = 0.88f; ///< moved into its riser
constexpr float STRENGTH_CELL = 112.0f;
constexpr float RISER_MIN = 0.26f;  ///< riser share of one level band. 0.26 is
constexpr float RISER_MAX = 0.52f;  ///< a 3.8x gradient multiplier, 0.52 a 2.4x
constexpr float RISER_CELL = 144.0f;
/// THE RAMPS. A scarp with no way up it is a fence, and the reference frames
/// (01, 06) are full of benches you can walk onto. This field opens the riser
/// back out to the plain slope over ~15% of its length — measured as area, so
/// it is a property of the world and not of one crossing.
constexpr float BREACH_CELL = 88.0f;
constexpr float BREACH_LO = 0.10f;
constexpr float BREACH_HI = 0.30f;

/// THE DRAWS (промоины) — REQUESTED NUMBERS ROWS, same standing as above.
///
/// Why a second form exists at all, and it is a measurement rather than a
/// preference: the bench operator is a MONOTONE TRANSFER, so it can only
/// re-shape relief that is already there. Measured at A1 with the terraces
/// alone, the frame's per-column counts came out as one contiguous failing
/// SECTOR — the bearings along which the country has ~2 m of relief in 60 m —
/// while every other bearing reached 2-3. No setting of step, riser or
/// strength moved that sector (24-point sweep), and the settings that moved
/// the CONTINUOUS field (a 0.35 m step reads p5 = 3) read p5 = 0 on the 2 m
/// heightmap the world is actually drawn from. So the shortfall is not a
/// tuning of the first form; it is ground with nothing in it to re-shape.
/// TWO SPACINGS, NOT ONE, and the reason is a shape rather than a number: a
/// single cell draws PARALLEL channels at one pitch, which is a washboard and
/// reads as manufactured from any standpoint. Two incommensurate pitches on the
/// same axis field interleave into long channels with short tributaries between
/// them, which is what a dissected slope looks like and what frame 03 shows.
constexpr float DRAW_CELL = 14.0f;     ///< across-grain spacing, the main set
constexpr float DRAW_CELL_2 = 8.75f;   ///< ...and the tributary set (the RATIO
                                       ///< is what carries the interleaving, so
                                       ///< it is held while the pitch moves)
constexpr float DRAW_MIX_2 = 0.45f;    ///< its share
constexpr float DRAW_STRETCH = 9.0f;  ///< along-grain compression: a draw is an
                                      ///< order of magnitude longer than wide,
                                      ///< which is what makes it a FORM and not
                                      ///< a dimple — the run is the point
constexpr float DRAW_THRESHOLD = 0.70f; ///< how much of the band is channel
constexpr float DRAW_DEPTH_MIN = 1.2f;  ///< incision (m). The floor is set by
constexpr float DRAW_DEPTH_MAX = 2.6f;  ///< the 2 m heightmap and the grazing
                                        ///< geometry: a 0.7 m cut with a ~5 m
                                        ///< bank is 8 deg, which opens a pocket
                                        ///< beyond 12 m; below that it is a
                                        ///< texture, not a landform
constexpr float DRAW_DEPTH_CELL = 208.0f;
constexpr float DRAW_DENSITY_CELL = 272.0f; ///< dissected country comes in tracts,
constexpr float DRAW_DENSITY_LO = 0.18f;    ///< not uniformly
constexpr float DRAW_DENSITY_HI = 0.52f;
/// ...BUT IT NEVER REACHES ZERO, and that floor is the contract rather than a
/// taste. §10.1.2 binds on the FLATTEST LEGAL GROUND — the complaint is about
/// the flat places — so a form that switches off over tracts leaves exactly the
/// ground the contract is about with nothing on it. Measured before the floor
/// went in: over the world's twelve flattest legal standpoints the pocket count
/// read p5 0 / median 1, and the zeros were tracts, not bearings. Variety
/// survives as a difference of DEGREE (0.4x to 1x), which is what varies in
/// country anyway.
constexpr float DRAW_DENSITY_FLOOR = 0.40f;

/// THE WANDER, and it answers the defect this pass's OWN first frames named
/// rather than one an instrument found: at a tight pitch the channels read as a
/// WASHBOARD — long, parallel, evenly spaced — and regularity is exactly how a
/// generated world gives itself away. Real dissection has a pitch that wanders,
/// tributaries that enter at whatever angle the ground gives them, and talwegs
/// that stop before they arrive.
///
/// It is a BOUNDED DOMAIN WARP: the channel field is sampled at a position
/// displaced by a slow vector field. Two properties make that the right tool
/// rather than "vary the cell":
///   * A cell that varies with position is not a lattice at all — value noise
///     read at a position-dependent cell tears, because two neighbouring points
///     no longer index the same corners. A warp keeps ONE lattice and moves the
///     query, so the field stays continuous and seamless by construction.
///   * The LOCAL PITCH is the cell divided by (1 + the warp's derivative along
///     the cross-axis), so warping IS varying the pitch — smoothly, and by an
///     amount you can bound. At WANDER_AMP over WANDER_CELL the derivative is
///     bounded by ~2.6 * AMP / CELL, which at 26 m over 190 m is +-36 %: the
///     pitch breathes between roughly 10 and 22 m around a nominal 14.
/// §2.1 rejected a position-varying ROTATION for its |world| * grad(theta)
/// distortion; a position-varying TRANSLATION carries no such term — the
/// displacement is bounded and does not grow with distance from the origin.
constexpr float WANDER_AMP = 26.0f;
constexpr float WANDER_CELL = 190.0f;

/// ...AND THE TALWEGS END. A channel that runs the full width of the world is
/// as manufactured as an even pitch. The threshold that decides what counts as
/// channel is itself a slow field, so along any one draw the section narrows,
/// pinches out and picks up again — which is what a head-water network does,
/// and it costs one noise sample.
constexpr float THRESHOLD_SWING = 0.16f;
constexpr float THRESHOLD_CELL = 118.0f;

/// THE TRIBUTARY BEARING, and it is the half of "washboard" that a spacing
/// measure cannot see. Measured: the gap between draws has a coefficient of
/// variation of 0.573 shipped against 0.567 with the wander switched off —
/// IDENTICALLY IRREGULAR, i.e. the spacing was never the defect. What the frame
/// actually shows is PARALLELISM: every channel on one bearing, which reads as
/// corduroy however unevenly the lines are spaced. So the tributary set is read
/// off the same axis lattice at an OFFSET BEARING, and the offset itself drifts,
/// so tributaries meet their trunks at a range of angles instead of one.
constexpr float TRIB_BEARING_MIN = 0.34f; ///< rad (~19 deg): below this they
constexpr float TRIB_BEARING_MAX = 0.72f; ///< read as parallel; above ~45 deg
                                          ///< they stop reading as tributaries
                                          ///< of THIS trunk at all
constexpr float TRIB_BEARING_CELL = 240.0f;

/// THE CUT BANK (подрез) — the channel's section is ASYMMETRIC, and which side
/// is cut alternates along its length.
///
/// It is the one thing in this file that is a landform rather than a field: a
/// stream undercuts the outer bank of every bend and leaves a slip-off slope
/// opposite, so a real channel is steep on one side and gentle on the other,
/// and the sides SWAP at every inflection. Two reasons it earns its place here
/// beyond looking right — both about the eye rather than about the metre:
///   * a steep rim holds a longer shadow than a symmetric groove of the same
///     depth, because what a pocket costs is the angle at the CREST;
///   * the gentle side gives the eye a lit face against the shaded one, which
///     is what makes a channel read as a channel from across a field instead of
///     as a dark line.
/// The side comes from the sign of the channel field about its own axis, and
/// the swap from a slow field along it, so it needs no new direction source —
/// it is the channel's own geometry read one derivative further.
constexpr float CUTBANK_STEEP = 0.55f; ///< exponent on the cut side (<1 = the
                                       ///< section rises fast off the floor)
constexpr float CUTBANK_GENTLE = 1.9f; ///< ...and on the slip-off side
constexpr float CUTBANK_CELL = 132.0f; ///< how often the cut side swaps

/// THE BACK-TILTED BENCH (оползневая ступень), and it is aimed at a hole the
/// pocket histogram found rather than at a look.
///
/// Pockets by distance at the pinned standpoint read 55/37/44/1/34/9 across the
/// 5-60 m band: the hole is 35-45 m, where that ground turns and begins to
/// RISE. On ground below the eye that rises, nothing can hide — the sight line
/// falls away from it faster than it climbs — and no amount of relief laid on
/// top changes that, because every form so far is a MONOTONE transfer of
/// elevation and a monotone transfer of a rising profile is still rising.
///
/// A rotational slump does not leave a flat bench: the block tilts BACK into
/// the hill, so behind the lip the ground dips before it climbs again. That
/// single sign change is the mechanism the histogram is asking for, and it is
/// the one thing a monotone operator cannot produce — so this term is
/// deliberately NON-monotone, applied to the bench only and bounded well under
/// the step so the surface never folds.
constexpr float SLUMP_SAG = 0.22f;  ///< sag as a fraction of the step
constexpr float SLUMP_CELL = 156.0f;
constexpr float SLUMP_FRAC = 0.55f; ///< how many benches are back-tilted

float env_float(const char* name, float lo, float hi, float fallback) {
    if (const char* e = std::getenv(name)) {
        const float v = std::strtof(e, nullptr);
        if (v >= lo && v <= hi) return v;
    }
    return fallback;
}

/// Drifting field in [lo, hi] on `cell`.
float drift(uint64_t seed, uint32_t stream, float cell, glm::vec2 world, float lo, float hi) {
    return lo + noise::value_noise(seed, stream, cell, world) * (hi - lo);
}

} // namespace

float draw_forms(uint64_t seed, glm::vec2 world, float mask) {
    if (mask <= 0.0f) return 0.0f;
    // THE COMB IS RETIRED, AND IT IS RETIRED RATHER THAN DELETED ON PURPOSE.
    //
    // This lattice bought the first non-zero GROUND_OCCLUSION_COUNT this project
    // ever read, and the price is now measured: a fixed 14 m pitch put 9.2 % of
    // the ground's relief energy into the 8-20 m band where real land carries
    // 0.00-0.05 %, which is the "scratched with claws" the user is looking at.
    // Its replacement is WorldgenFlow: valleys cut where the landform actually
    // sends its water, at a spacing that comes out of a channel-head support
    // area instead of being named.
    //
    // DEFAULT IS NOW OFF, and `DFN_DRAW_DEPTH=1` brings it back through this
    // same code path. That is deliberate under Rule 51: the REJECTED SAMPLE has
    // to stay reproducible, or the next zone to re-derive a threshold will have
    // nothing to fail against. Everything below this line is the comb exactly as
    // it shipped at 9888746 -- do not tidy it, it is evidence.
    const float depth_scale = env_float("DFN_DRAW_DEPTH", 0.0f, 6.0f, 0.0f);
    if (depth_scale == 0.0f) return 0.0f; // the pass's own named control

    // WHERE A DRAW RUNS, AND IT IS NOT A NEW DIRECTION FIELD. The channel
    // field is sampled through §2.1's OWN per-valley axis lattice — the same
    // frames, the same blending, the same seamlessness argument the ridgelet
    // octave uses — only compressed harder along the axis. So a draw runs
    // along the grain of the land it is cut into, which is where water would
    // run, and it inherits §2.1's coherence rather than inventing a second
    // opinion about which way this country lies.
    //
    // STATED PLAINLY BECAUSE IT MATTERS LATER: this gives a draw a DIRECTION
    // and a RUN, which is what §10.1.3 needs and what a noise field cannot
    // have. It does NOT give it CONNECTIVITY TO A DRAINAGE — these channels do
    // not know where the river is, and on a stand that declares the LF-8
    // erosion pass that pass is the better source. The testbed declares
    // `erosion: false`, so nothing else in this world is producing incision at
    // all, and a draw that runs along the valley axis is the honest half of
    // the answer rather than a claimed gully.
    const float cell = env_float("DFN_DRAW_CELL", 8.0f, 200.0f, DRAW_CELL);
    const float stretch = env_float("DFN_DRAW_STRETCH", 1.0f, 40.0f, DRAW_STRETCH);
    // THE WANDER (see above). DFN_DRAW_WANDER=0 is its own named control — the
    // same code path with the displacement at zero, which is the washboard this
    // exists to break, kept reachable so the difference can be measured rather
    // than admired.
    const float wander = env_float("DFN_DRAW_WANDER", 0.0f, 3.0f, 1.0f) * WANDER_AMP;
    const glm::vec2 warped =
        world
        + glm::vec2{noise::value_noise(seed, STREAM_DRAW_WANDER, WANDER_CELL, world) - 0.5f,
                    noise::value_noise(seed, STREAM_DRAW_WANDER + 1, WANDER_CELL,
                                       world + glm::vec2{311.7f, -97.3f})
                        - 0.5f}
              * (2.0f * wander);
    const float line = aniso_octave_sample(seed, STREAM_DRAW_LINE, cell, warped, stretch);
    const float trib_bearing =
        env_float("DFN_DRAW_TRIB_BEARING", 0.0f, 1.6f,
                  drift(seed, STREAM_DRAW_BEARING, TRIB_BEARING_CELL, world, TRIB_BEARING_MIN,
                        TRIB_BEARING_MAX))
        * (noise::value_noise(seed, STREAM_DRAW_BEARING + 1, TRIB_BEARING_CELL * 1.7f, world)
                   < 0.5f
               ? -1.0f
               : 1.0f); // tributaries enter from both sides, not all from one
    const float line2 = aniso_octave_sample(seed, STREAM_DRAW_LINE + 1,
                                            cell * (DRAW_CELL_2 / DRAW_CELL), warped, stretch,
                                            trib_bearing);
    const float threshold =
        DRAW_THRESHOLD
        + (noise::value_noise(seed, STREAM_DRAW_THRESHOLD, THRESHOLD_CELL, warped) - 0.5f)
              * 2.0f * THRESHOLD_SWING;
    // Which side is undercut here, and it swaps along the channel's length.
    const float swap = noise::value_noise(seed, STREAM_DRAW_CUTBANK, CUTBANK_CELL, warped) < 0.5f
                           ? -1.0f
                           : 1.0f;
    const auto channel = [threshold, swap](float v) {
        const float ridge = 1.0f - std::fabs(2.0f * v - 1.0f); // 1 on the channel axis
        const float across = std::clamp((ridge - threshold) / (1.0f - threshold), 0.0f, 1.0f);
        // The section: smoothstep across, so the banks are the steep part and
        // the floor is flat — a channel, not a groove. Then the two sides are
        // given different exponents, which is the cut bank / slip-off pair.
        const float sym = noise::smoothstep01(across) * noise::smoothstep01(across);
        const float side = (v - 0.5f) * swap; // >0 on the undercut side
        const float e = side > 0.0f ? CUTBANK_STEEP : CUTBANK_GENTLE;
        return std::pow(sym, e);
    };
    // The two sets combine as the DEEPER of the two, never as a sum: channels
    // MEET AND MERGE where they cross, they do not add their depths. The
    // tributary set is the shallower one, which is the same statement about
    // stream order made in metres.
    const float section = std::max(channel(line), DRAW_MIX_2 * channel(line2));
    if (section <= 0.0f) return 0.0f;

    const float dissected =
        DRAW_DENSITY_FLOOR
        + (1.0f - DRAW_DENSITY_FLOOR)
              * noise::smoothstep01(std::clamp(
                  (noise::value_noise(seed, STREAM_DRAW_DENSITY, DRAW_DENSITY_CELL, world)
                   - DRAW_DENSITY_LO)
                      / (DRAW_DENSITY_HI - DRAW_DENSITY_LO),
                  0.0f, 1.0f));
    const float depth = drift(seed, STREAM_DRAW_DEPTH, DRAW_DEPTH_CELL, world, DRAW_DEPTH_MIN,
                              DRAW_DEPTH_MAX)
                      * (depth_scale > 0.0f ? depth_scale : 1.0f);

    return -depth * section * dissected * mask;
}

float terrace_forms(uint64_t seed, glm::vec2 world, float h_in, float mask) {
    if (mask <= 0.0f) return 0.0f;

    const float step =
        env_float("DFN_TERRACE_STEP", 0.2f, 8.0f,
                  drift(seed, STREAM_TERRACE_STEP, STEP_CELL, world, STEP_MIN, STEP_MAX));

    // The RAMP field multiplies the strength, so a breach is the operator
    // relaxing toward identity rather than a hole cut in the ground: the
    // surface stays continuous across it and you walk up the plain slope.
    const float breach = std::clamp(
        (noise::value_noise(seed, STREAM_TERRACE_BREACH, BREACH_CELL, world) - BREACH_LO)
            / (BREACH_HI - BREACH_LO),
        0.0f, 1.0f);
    const float strength =
        env_float("DFN_TERRACE_STRENGTH", 0.0f, 0.95f,
                  drift(seed, STREAM_TERRACE_STRENGTH, STRENGTH_CELL, world, STRENGTH_MIN,
                        STRENGTH_MAX))
        * noise::smoothstep01(breach) * mask;
    if (strength <= 0.0f) return 0.0f;

    const float riser =
        env_float("DFN_TERRACE_RISER", 0.05f, 0.95f,
                  drift(seed, STREAM_TERRACE_RISER, RISER_CELL, world, RISER_MIN, RISER_MAX));

    // The level pile: a datum that drifts slowly, so the world is not one
    // machined stack of world-height contours, and does not drift fast, so a
    // riser stays on its contour for as far as the eye follows it.
    const float datum =
        drift(seed, STREAM_TERRACE_DATUM, DATUM_CELL, world, -0.5f, 0.5f) * step;

    const float u = (h_in - datum) / step;
    const float f = u - std::floor(u);

    // G: the monotone step. Flat over both benches, one smoothstep across the
    // riser, centred in the band so the delta is odd about the middle and the
    // operator neither raises nor lowers the country on average.
    const float lo = 0.5f - riser * 0.5f;
    const float g = noise::smoothstep01(std::clamp((f - lo) / riser, 0.0f, 1.0f));

    // THE BACK-TILT, and it is the only non-monotone term in this file. It sags
    // the bench BEHIND the riser (f past the riser's top) by a parabola whose
    // peak is SLUMP_SAG of the step, so the profile behind a lip goes down
    // before it goes up. Bounded well under the step, so the transfer's
    // derivative stays positive except across the sag itself — the surface dips,
    // it never folds.
    const float hi = lo + riser;
    float sag = 0.0f;
    if (f > hi
        && noise::value_noise(seed, STREAM_TERRACE_SLUMP, SLUMP_CELL, world) < SLUMP_FRAC) {
        const float u = (f - hi) / std::max(1.0f - hi, 1e-3f);
        sag = env_float("DFN_TERRACE_SAG", 0.0f, 0.6f, SLUMP_SAG) * 4.0f * u * (1.0f - u);
    }

    // The delta: a MONOTONE TRANSFER h -> h + step*strength*(g(f) - f), plus the
    // back-tilt above. The transfer alone is monotone because
    // d/dh = 1 - strength + strength*G'(f) >= 1 - strength > 0 for strength < 1:
    // no overhang, no inverted drainage, contours preserved.
    return step * (strength * (g - f) - sag);
}

} // namespace dfn::world
