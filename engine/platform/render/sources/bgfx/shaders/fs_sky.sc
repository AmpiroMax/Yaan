$input v_dir

// Sky fragment (day/night, в1/в2 + clouds, в4/в10 / WEATHER.md W4):
// horizon->zenith gradient with a haze band at the horizon (matching the fog
// color so distant terrain melts into the sky at every hour), the sun disc
// and glow, a STAR FIELD, a MOON DISC WITH PHASE, and THREE KINDS OF CLOUD —
// two drifting parallax sheets (planes at DFN_CLOUD_DECK_{MID,HIGH}_M read through
// the ONE coverage field in dfn_env.sh, the same field the ground shadow
// projects) and cumulus impostors on the horizon ring, upwind-biased so they
// announce the weather that is coming (W2.3). Clouds blend over stars and
// moon, so night cover occludes the star field where it sits. All driven by
// RenderEnvironment via dfn_env.sh — no shader recompile to retune.
//
// UPD 10:08:2026 - 10:45:06: the sheet's horizon/distance fades DELETED (they carved a
// hard shelf across the sky at dir.y ~ 0.07 and emptied everything under it —
// the first shoot's "materialises only near the horizon"); the sheet now meets
// the horizon by converging to its own area average. Cumulus rebuilt on the
// same coverage field with an elevation-rising threshold, on a 20 km ring with
// real cloud altitudes, so the masses are rounded domes standing clear of the
// terrain line instead of trapezoids with their bodies under it.

// UPD 10:08:2026 - 20:10:49: THE SUN GAINED A BODY (W9). Two additive glow
// lobes replaced by a hard-edged disc composited inside a CAPPED halo. What
// read as a disc before was the RGBA8 clamp: measured at this vantage the old
// bright mass was 21.6 px across at its half-contour with a 16 px edge fall
// and 15.5 px of the red channel pegged at 255, i.e. no edge and a size set
// by saturation. It is now 9.24 px against SUN_ANGULAR_DIAMETER's 9.0 px, a
// 7 px fall, inside a glare that reaches ~36 px — so the bright FEATURE grew
// while acquiring a body, which is what "чуть больше" and "солнца не видно"
// asked for together.

// UPD 11:08:2026 - 14:40:43: THE HORIZON DOMES REMOVED (R3.1). The cumulus band now reads
// the coverage field in 3D on the ring instead of as a function of azimuth
// alone, and the height threshold goes back to LINEAR because the shape now
// comes from the field rather than from inverting a 1-D function. Measured on
// the cumulus probe: columns carrying a hole 0 -> 11, and 0 was the structural
// prediction, not a small number. Band also broader and thinner (363 columns
// with cloud against 196, on less total cloud).

// UPD 12:08:2026 - 22:45:00: R3.3 — THE HARD BRIGHT BAND AT THE HORIZON. The field's half of
// the fix is in dfn_env.sh; this file's half is the SHEET_HAZE_LO/HI hard cut,
// which was the next visible edge the moment the band above it got its
// structure back. It spanned 0.23-1.72 deg — about 12 px — and is replaced by
// an exponential extinction in the sheet's OWN distance, derived from the
// layer's geometric horizon. Measured on the cloud-only difference image:
// band rows SD 9.4 -> 30.5 with the mean 74.5 -> 47.8; the strip below the
// cumulus base keeps its low SD (7.6 -> 6.2) but loses two thirds of its
// amplitude (mean 62.7 -> 21.3), which is what a veil at 50 km should look
// like. The two DFN_CLOUD=0 arms came back BYTE-IDENTICAL before and after.

// UPD 12:08:2026 - 23:12:00: R3.2 — THREE DECKS, drawn BACK TO FRONT so the low
// one's holes are where the decks behind it show through, and a TONE LADDER in
// PALETTE UNITS: each rung is one PALETTE_SHADE_STEP_REF, and the load-bearing
// rung is the low deck's LIT tone sitting a full step BELOW the mid deck's
// SHADED tone, so no part of the near deck can be confused with any part of the
// far one. That is what "three strata" has to mean for a 64-colour palette —
// not three altitudes, three tones that survive quantisation. Measured on
// per-deck arms (DFN_DECK_ARM): the decks own 36.4 / 56.4 / 45.2 % of the sky
// box, low-vs-far overlap sits AT chance, holes read 1.74 palette steps
// brighter than the deck in front of them, and the shade-only difference image
// has SD 0.65 steps at a mean of -0.06 — a uniform darkening would score SD 0.

// UPD 13:08:2026 - 19:20:00: R3.4 — THE DECK ALTITUDES ARRIVE IN A UNIFORM, because the
// user asked for the ceiling's HEIGHT to be a field with a legal range: «они
// должны на разных высотах находиться, должен быть диапазон где им можно быть...
// на разных локациях будут на разных высотах, и в разную погоду на разных, будем
// таким образом погоду и климат отображать». DFN_CLOUD_DECK_{LOW,MID,HIGH}_M are
// now u_cloudDeck{Low,Mid,High} (slot 39), written per frame by
// engine/render's CloudModel from cloud_cover and the observer's place, range
// [400, 2000] m for the ceiling with the other two riding one multiplier so
// R3.2's derived 1 : 1.73 : 2.93 ladder survives.
//
// NOTHING ELSE IN THIS FILE HAD TO CHANGE, and that is the point worth keeping:
// the three decks were already drawn by intersecting the view ray with a plane
// at a real altitude, so an altitude that MOVES is expressible. A sky drawn as a
// function of view direction would have had nothing to move.

// UPD 13:08:2026 - 19:49:07: THE CUMULUS STOPPED BEING A RING AND THE STARS STOPPED
// FLICKERING. Two user reports, two sampling defects, both measured before a
// pixel moved.
//
// (1) «я не приближаюсь к облакам» / «бело-серые кучки». The band read the
// field on a cylinder of radius 20 km CENTRED ON THE EYE. Its parallax was
// correct — for something 20 km away, which is a distance this world does not
// contain: the whole walkable testbed subtends 0.86 deg of bearing on it, and
// every cumulus pixel had the SAME range, so the bank could only ever translate
// rigidly. Re-anchoring a 20 km ring answers nothing; the masses had to move to
// where clouds are. They now live in a SLAB between the low and middle deck,
// four taps along the view ray, Beer-Lambert. Measured against the ring in one
// process (both arms, a zero-walk arm at exactly 0.00 %):
//     range to cloud, per cloud-bearing pixel   bearing swing per 300 m walked
//       RING  p10/median/p90  20.0/20.0/20.0 km    0.86 / 0.86 / 0.86 deg
//       SLAB  p10/median/p90   3.2/ 5.6/21.2 km    5.45 / 3.07 / 0.81 deg
// The ring had ONE number three times — no differential parallax anywhere in
// the frame, which is «стоят как декорация» stated as geometry. The slab's
// nearest tenth swings 6.7x the farthest, and cloud went from 2.8 % of the sky
// to 26.8 %. Path length does the rest for free: a ray at 2 deg crosses ~28x
// more slab than one straight up, so the horizon reads as a solid bank and the
// zenith as separate masses, from the same field and the same threshold.
// TWO OF MY OWN MISTAKES, both caught by the frame and both fixed here rather
// than left: the first cut had no LOD on the 3-D field and laid a speckled
// stripe over the horizon (fixed by converging to the field's own area average,
// 1-T, exactly as R3.3 does for the sheets), and the second applied the veil
// extinction INSIDE the integral, where path length saturates opacity faster
// than distance can fade it — a 40 km bank came out as an opaque grey WALL.
//
// (2) «звёзды дёргаются». Measured: the star field is a function of `dir` alone,
// so walking moves it by 0.00 %. TURNING moves it, and at 640x360 the disc was
// 0.55 px in RADIUS — smaller than the sample spacing, so one frame of a slow
// 30 deg/s look changed the WHOLE FIELD's brightness by -16.9 % and made
// individual stars swing 0.90 of full brightness. See dfn_stars for the sweep
// the 0.8 px radius is read off.

// UPD 13:08:2026 - 20:19:19: THE MIDDLE DECK HAS A THICKNESS — the last flat thing
// in the sky, and rule 52 in one line: a deck read at ONE plane intersection
// has a silhouette from below and no vertical dimension, so it can only be a
// lid. It is a SLAB now (u_deckThick, half a coverage cell = 300 m), read at
// three altitudes, with transmission (1-a)^n where n is the number of field
// CELLS the chord crosses — u_deckThick/dir.y divided by the cell width, which
// is 1 overhead and 12 at the skyline. Measured on the overcast probe, sky
// showing through the deck: 5.5 % -> 0.7 % near the horizon (the deck CLOSES
// along the layer, which is what overcast does and what a plane cannot do) and
// 15.6 % -> 11.5 % overhead (the ray crosses a third of a cell horizontally
// even straight up, so the three slices blend). ZERO-DOSE ARM: DFN_DECK_THICK=0
// reproduces the shipped sheet at 0.002 % of pixels, max 1/255 — `pow(x,1.0)`
// rounding and nothing else. THE CONTROL EARNED ITS KEEP: the first form wrote
// the path as clamp(1/dir.y, 1, 12), which does not contain the thickness at
// all, so the dose-0 arm still closed the deck and came back 54 % different
// from the shipped frame. A dose that does not appear in the formula is not a
// dose. The 2-D field is kept deliberately — dfn_cloud_field carries R3.3's
// per-octave LOD and the renormalisation that stopped the horizon band, and
// dfn_cloud_field3 has none; the vertical structure here comes from the
// SEPARATION of the samples, not from a third noise dimension.

// UPD 13:08:2026 - 20:27:38: THE NEAR DECK GETS ITS THICKNESS TOO, at a THIRD of the
// main sheet's. Not a taste: this is the ragged deck whose job in R3.2's tone
// ladder is to be sparse and let what is behind it through, and a near layer as
// deep as the main one loses its gaps the moment the chord runs along it — the
// holes ARE the feature here. A third of the cell also puts the near deck's
// aspect near 6:1 against the main sheet's 2:1, which is scud against
// stratocumulus. Same construction, same dose, and the same control: with
// DFN_DECK_THICK=0 the frame comes back at 0.001 % of pixels and max 1/255
// against the previous shader — the slab collapses to the plane it replaced.
// With it, 60.2 % of the frame moves at max 189/255 on the scattered probe.
// EVERY FLAT THING IN THIS FILE IS NOW ACCOUNTED FOR: the two lower decks are
// slabs, the cumulus are a slab, and the HIGH deck stays a plane on purpose —
// it is a cirrus veil at 4.4 km and cirrus have no depth to show.

// UPD 23:08:2026 - 02:05:56: Э5а+б (волна 23.08) — освещённое двухтоновое облако:
// потолок яркости по люме с сохранением оттенка (unit-luma hue вместо
// покомпонентного min) и тень подмесом зенита k=0.419. Доза DFN_CLOUD_LIT
// (слот 14.x), 0 — прежний кадр бит-в-бит.

// UPD 23:08:2026 - 02:07:35: Э4 — три места cells_px переведены на анизотропную
// меру (малая ось следа пикселя, DFN_CLOUD_CELLS_PX_ANISO): радиальное
// усреднение уже дают DECK_SLICES вдоль луча, а max() валил поле в среднее
// преждевременно — горизонтальная рябь рабочей зоны неба.

// UPD 23:08:2026 - 02:15:40: Э6 — потолок пути через ярус привязан к остаточному
// разбросу поля (mix(1, DECK_PATH_MAX, residual)): непрозрачность не копится
// там, где структура уже вернула площадное среднее. Доза DFN_CLOUD_PATHRES.

#include <bgfx_shader.sh>
#include "dfn_env.sh"

// HOW THE SHEET ENDS AT THE HORIZON, and it is an EXTINCTION IN DISTANCE, not
// a cut in elevation.
//
// What was here: `smoothstep(0.004, 0.030, dir.y)`. That spans 0.23-1.72 deg,
// about 12 px of a 640x360 frame — a hard cut, and it was invisible only
// because the band ABOVE it was already a flat wash (R3.3). The moment that
// band got its structure back this became the next visible edge, which is why
// it is fixed in the same change and not after it.
//
// The number is derived from the layer's own geometry rather than picked. A
// 2600 m deck seen from the valley floor physically ENDS at its geometric
// horizon, sqrt(2*R_earth*h) = 182 km; so an extinction length of a third of
// that puts the sheet at 1/e by 60 km, at 5 % by the distance where it ceases
// to exist at all, and at 96 % overhead — a fall that is continuous from the
// zenith outward and therefore has no edge anywhere to see.
//
// NOT dfn_aerial_transmittance, and the arithmetic is why: HAZE_SCALE_LENGTH
// is 600 m, calibrated so a ridge at 250-900 m reads its distance. Run over a
// 20 km sightline the same law gives an optical depth of 4.4 and erases the
// cumulus bank, and over the 2.6 km straight up it still takes a fifth off the
// zenith. The sheet is a SKY element: dfn_aerial's target colour IS the sky
// gradient, so aerial perspective applied to the sky is a term applied to
// itself. Terrain keeps that law; the sky gets its own length.
#define SHEET_EXTINCTION_M 60000.0

// THE THREE DECKS' TONE LADDER (R3.2), and every rung is ONE PALETTE SHADE
// STEP. `PALETTE_SHADE_STEP_REF` is 0.0784 of the range, and this file's own
// PALETTE SIGNAL STRENGTH rule says a brightness step is the WEAKEST signal
// available and anything under one step becomes dither. So the ladder is built
// on exactly that unit rather than on taste:
//
//   high deck            1.0000   (the brightest thing in the sky)
//   mid deck, lit        0.9216   = 1 - 1 x 0.0784
//   mid deck, shaded     0.5800   (cloud_dark, unchanged)
//   low deck, lit        0.5000   = 0.58 - 1 x 0.0784
//   low deck, shaded     0.3000
//
// The load-bearing pair is the middle one: the low deck's LIT tone sits a full
// step BELOW the mid deck's SHADED tone, so no part of the near deck can be
// confused with any part of the deck behind it. That is what "three strata"
// has to mean for a 64-colour palette — not three altitudes, three tones that
// survive quantisation.
#define DECK_TONE_MID_LIT  0.9216
#define DECK_TONE_LOW_LIT  0.5000
#define DECK_TONE_LOW_DARK 0.3000

// PER-DECK ACCEPTANCE ARMS (Rule 27/30). "The sky looks deeper" is not a
// criterion; a deck is PRESENT iff the pixels that change when it ALONE is
// switched off are a substantial set, and the three sets must be largely
// disjoint. Set to 1/2/3 to drop the low/mid/high deck, rebuild (~14 s), shoot
// the same vantage, and read the arms with
// `tools/measure_aerial.py decks <FULL> <ARM...> <box>`. 0 is the shipped sky.
#define DFN_DECK_ARM 0

// CUMULUS ARE A SLAB NOW, NOT A RING AROUND THE EYE, and that is the fix for
// «я не приближаюсь к облакам» / «бело-серые кучки, стоят как декорация».
//
// WHAT WAS WRONG, and it was NOT the anchoring. The band sampled the field at
// `eye.xz + dh * 20000`, so its parallax response was exactly that of something
// 20 km away — which is CORRECT geometry for a thing 20 km away, and that is
// the whole problem: 20 km is a distance this world does not contain. The
// testbed is about 1 km across, so the entire walkable world subtends 3 deg of
// bearing change on a 20 km bank. Nothing the player can do moves it. The
// complaint is about DISTANCE, and no amount of re-anchoring a 20 km ring
// answers it.
//
// So the masses go where clouds actually are: in a SLAB between the low and the
// middle deck, sampled along the view ray. The geometry then hands the whole
// look back for free, because distance becomes a function of where you look:
//   straight up      the slab is 0.9-1.6 km away — a cloud you are UNDER
//   45 degrees       ~1.3-2.3 km
//   6 deg elevation  ~9-16 km
//   2 deg            ~27-46 km — the far bank on the horizon
// Walking 500 m swings the bearing of the nearest masses by 30 deg and the far
// bank by 1 deg, which is what an approach IS: differential parallax between
// near and far cloud. The ring had one distance and therefore no differential.
//
// AND THE PATH LENGTH IS WHAT MAKES THE HORIZON A BANK. A ray at 2 deg crosses
// 20 km of slab and a ray straight up crosses 700 m, so opacity accumulates ~28x
// faster toward the horizon: the same field, the same threshold, and the
// horizon still reads as a solid bank while the zenith reads as separate
// masses. That was the ring's job and it is now a consequence rather than a
// setting.
//
// The slab is bracketed by the two decks (u_cloudDeckLow..u_cloudDeckMid) so it
// moves with the ceiling (R3.4) and cannot drift into or through them.
#define CUMULUS_TAPS 4
// The mid deck's own vertical sampling. THREE slices, because two cannot show a
// core (only a top and a bottom) and four buys a fourth field tap for a
// structure the 640x360 raster cannot resolve. The path cap is where the chord
// through the slab reaches the layer's own geometric horizon: beyond it the
// extinction owns the pixel anyway, and an uncapped 1/dir.y is a division by
// nothing at the skyline. DECK_PATH_REF is the chord at 30 degrees of
// elevation.
#define DECK_SLICES    3
// The near deck is a THIRD as deep as the main one — see its block for why.
#define DECK_LOW_THICK_FRAC 0.3333
#define DECK_PATH_MAX  12.0
// Extinction per slab-thickness of FULLY dense cloud. Derived from the one
// case the slab has to get right on its own: looking straight up through a
// solid column the ray crosses exactly one thickness, and a cumulus directly
// overhead should be nearly opaque but not black — 0.85 of the way there, so
// 1 - exp(-k) = 0.85 and k = 1.90. Everything else follows from the path
// length, which is geometry: at 2 deg of elevation the same law reaches full
// opacity on a fifth of that density, which is what a horizon bank is.
#define CUMULUS_DENSITY 1.90
// The ONE field, read coarser on the ring. THE NUMBER IS AN ASPECT RATIO, not
// a taste: the band is CUMULUS_TOP_Y - CUMULUS_BASE_Y = 0.135 of a radian tall,
// i.e. 7.7 deg, and a cumulus mass is WIDER THAN IT IS TALL — a fair-weather
// cumulus is 2-5 km across against 2-3 km of depth, and at 20 km that is a
// silhouette whose lobes are 6-14 deg wide. At scale 2.5 one 600 m field cell
// subtended 4.3 deg: NARROWER than the band was tall, which is what made every
// mass a vertical tooth. At 7.0 a cell subtends 12 deg — about 1.6 times the
// band's height, which is the proportion a bank of cumulus actually has.
#define CUMULUS_SCALE     7.0
// Altitude stretch for the 3-D read. One field cell is CUMULUS_SCALE * the
// wavelength across (~4.2 km on the ring, ~12 deg of azimuth); the band is
// CUMULUS_TOP_M - CUMULUS_BASE_M = 2700 m tall. Stretching altitude by 1.6
// makes a cell about one band tall, i.e. masses ~1.6x wider than high — the
// proportion of a real fair-weather cumulus (2-5 km across, 2-3 km deep).
#define CUMULUS_VERTICAL_STRETCH 1.6

// Angular half-size of the moon disc. The true moon is ~0.26 deg, which at
// 640x360 is ONE pixel — invisible and, worse, a flickering pixel. Ours is
// deliberately a Daggerfall moon: big enough that the phase is legible as a
// silhouette, which is the whole point of having phases.
#define MOON_COS_INNER 0.99939 // ~2.0 deg half-angle
#define MOON_COS_OUTER 0.99908 // ~2.45 deg, soft limb

// Static star field. Direction is quantized to a coarse 3D cell grid, the cell
// is hashed, and only the rare high hashes become stars, at a pseudo-random
// point inside their cell. NO time term: a twinkle at 640x360 under the
// 64-colour palette reads as sensor noise, not as sky.
//
// THE STARS DID NOT MOVE AND STILL FLICKERED, and this is the fix for the
// user's «звёзды дёргаются». Two measured facts, from a CPU mirror of this
// function (one process, a zero-rotation arm that comes out identically 0):
//
//  1. The star field is a function of `dir` ALONE, so WALKING cannot move it —
//     measured 0.00 % of pixels changed. What moves it is TURNING, and turning
//     is what a first-person player does constantly: 30 deg/s at 120 fps is
//     1.2 px of image per frame.
//  2. At 640x360 the shipped disc is 0.55 px in RADIUS. A point smaller than
//     the sample spacing is captured or missed depending on where the pixel
//     centre happens to fall inside it, so ONE FRAME of that slow turn moved
//     the WHOLE FIELD's brightness by -16.9 %, with individual stars swinging
//     0.90 of full brightness — appearing and vanishing between adjacent
//     frames. That is the flicker, and it is a sampling defect, not a twinkle.
//
// The cure for a sub-pixel point is not "antialias its edge" — there is no
// edge to antialias, the whole star is inside one sample. It is to spread the
// SAME ENERGY over at least one pixel, so sub-pixel motion redistributes light
// between neighbours instead of switching it on and off. So the radius is now
// in PIXELS (from the screen derivative, exactly as the sun disc below takes
// its own AA) and the brightness is divided by the area, which holds each
// star's emitted light fixed: this is a change of SHAPE, not of dose.
//
// 0.8 px IS READ OFF THE SWEEP, not chosen. Whole-field swing per 1.2 px turn,
// energy held constant, radius in pixels:
//     0.55 px (shipped) -16.93 %   |  133 px at >=1 palette step, 97 at >=2
//     0.83 px            -1.34 %   |  360                       , 153
//     1.10 px            +0.86 %   |  442                       ,  69
//     1.83 px            +0.49 %   |   26                       ,   0
// The swing falls 12.6x from 0.55 to 0.83 px and only 1.6x more by 1.10, while
// the BRIGHTEST stars (the ones that survive two palette steps) start
// collapsing past 0.83 — 153 -> 69 -> 0. Both curves are still good at 0.83 px
// and one of them is not past it: that is the knee, and it is where this sits.
// Note the second column: the field gets MORE visible, not less. The widening
// recovers light the raster was throwing away (captured 43.3 -> 76.8 units of
// an emitted 167), so the stars are steadier AND there are more of them.
//
// AND THE 3x3x3 READ IS PART OF THE SAME FIX. A disc of radius R spills into
// the neighbouring cells, and the single-cell read CLIPPED it — measured, that
// clipping alone cost 17 % of the field's light and contributed 5.4 points of
// the swing (a star crossing a cell boundary was being cut into a crescent).
// Night-only: the caller gates on u_starIntensity.
#define STAR_CELLS       150.0
#define STAR_RADIUS_BASE 0.30
#define STAR_RADIUS_PX   0.80

float dfn_star_hash(vec3 cell)
{
    return fract(sin(dot(cell, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

float dfn_stars(vec3 dir)
{
    vec3 g = dir * STAR_CELLS;
    // How many cells one pixel spans, from the raster itself. A constant would
    // be right at exactly one internal resolution and wrong at the other, and
    // this project ships two (640x360 and 320x180).
    float cells_px = max(length(fwidth(g)), 1e-4);
    float R = clamp(max(STAR_RADIUS_BASE, STAR_RADIUS_PX * cells_px),
                    STAR_RADIUS_BASE, 1.0);
    // Energy, not peak: a wider disc carries the same total light.
    float scale = (STAR_RADIUS_BASE * STAR_RADIUS_BASE) / (R * R);

    vec3 c0 = floor(g);
    float sum = 0.0;
    for (int k = -1; k <= 1; ++k) {
    for (int j = -1; j <= 1; ++j) {
    for (int i = -1; i <= 1; ++i) {
        vec3 cell = c0 + vec3(float(i), float(j), float(k));
        float h = dfn_star_hash(cell);
        if (h < 0.9860) {
            continue;
        }
        vec3 p = cell + vec3(fract(h * 17.0), fract(h * 31.0), fract(h * 57.0));
        float d = length(g - p);
        float brightness = 0.35 + 0.65 * fract(h * 91.0);
        sum += smoothstep(R, 0.0, d) * brightness * scale;
    }}}
    return sum;
}

// Moon disc with a phase terminator. Builds a basis on the moon direction,
// reads the fragment's position on the disc, lifts it onto the sphere, and
// lights that point with a direction derived from the phase: 0 = new (lit
// side faces away), 0.5 = full (lit side faces us).
vec3 dfn_moon(vec3 dir)
{
    float md = dot(dir, u_moonDir);
    if (md < MOON_COS_OUTER) {
        return vec3(0.0, 0.0, 0.0);
    }
    vec3 world_up = abs(u_moonDir.y) > 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(world_up, u_moonDir));
    vec3 up = cross(u_moonDir, right);

    // Disc coordinates in [-1,1] (small-angle: the tangent plane is fine here).
    float radius = sqrt(max(1.0 - MOON_COS_INNER * MOON_COS_INNER, 1e-8));
    vec2 uv = vec2(dot(dir, right), dot(dir, up)) / radius;
    float r2 = dot(uv, uv);
    float disc = smoothstep(1.06, 0.94, sqrt(r2)); // soft limb, still hard-ish

    // Sphere normal of that point, +z toward the viewer.
    vec3 n = vec3(uv, sqrt(max(1.0 - min(r2, 1.0), 0.0)));
    float a = u_moonPhase * 6.28318530718;
    vec3 light = vec3(sin(a), 0.0, -cos(a));
    float lit = smoothstep(-0.06, 0.10, dot(n, light));

    // The unlit part keeps a trace of earthshine so a crescent still reads as
    // a sphere and not as a floating sliver.
    return u_moonColor * (disc * (lit * 0.95 + 0.05));
}

// The cloud palette for this hour, derived from the SAME sun/ambient the
// ground is lit by: at noon the sheets are white-grey, at sunset u_sunColor
// is red so the sheets redden with the sky, at night both terms collapse and
// the clouds go darker than the moonlit blue around them. Moonlight adds a
// cold rim so a full-moon night keeps its clouds legible.
vec3 dfn_cloud_bright()
{
    vec3 c = u_ambientColor * 1.9 + clamp(u_sunColor, 0.0, 1.0) * 0.50
           + u_moonColor * (u_moonLight * 0.22);
    // Э5а (волна 23.08): потолок по ЛЮМЕ с сохранением оттенка. Днём все
    // три канала выше 1.12, и покомпонентный min выдавал ровно
    // (1.12,1.12,1.12) — нейтрально-серое облако БЕЗ ПРАВА на тёплую
    // макушку по построению (замер: зенитная облачность #8e8f90 при
    // SD 3.27/255). Механизм тот же, что у диска солнца ниже: unit-luma
    // hue. Доза DFN_CLOUD_LIT, 0 — прежний min бит-в-бит.
    if (u_cloudLitDose > 0.5) {
        float cl = dot(c, DFN_LUMA_WEIGHTS);
        if (cl > 1.12) {
            c *= 1.12 / cl;
        }
        return c;
    }
    return min(c, vec3(1.12, 1.12, 1.12));
}

void main()
{
    vec3 dir = normalize(v_dir);
    // THE GRADIENT MOVED TO dfn_env.sh and did not change: the air in front
    // of the terrain (dfn_aerial, R1) has to fade into exactly this, so it
    // may exist once and only once.
    vec3 sky = dfn_sky_gradient(dir);

    // Stars sit BEHIND everything else and only above the horizon; they fade
    // near it so the field does not collide with the horizon haze.
    float star_fade = smoothstep(0.02, 0.22, dir.y);
    sky += vec3(1.0, 1.0, 1.0) * (dfn_stars(dir) * u_starIntensity * star_fade);

    sky += dfn_moon(dir);

    // ---- Clouds (W4). Alphas first; the blends AFTER stars and moon are
    // what occludes them where cover sits — no separate star gating.
    vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 cloud_bright = dfn_cloud_bright();
    vec3 cloud_dark = cloud_bright * 0.58;
    // Э5б: тень облака — ПОДМЕШИВАНИЕ НЕБА ЗЕНИТА, не скаляр. Скалярное
    // *0.58 давало серый низ того же оттенка (перепад 1.59x против
    // эталонных 2.2x, холодное днище недостижимо). k = 0.419 выведен из
    // эталона верх/низ 2.2x (0.9216/2.2). Ночью зенит темнее облака — низ
    // сам становится темнее просвета из той же формулы, без ночного флага.
    if (u_cloudLitDose > 0.5) {
        cloud_dark = mix(cloud_bright, u_skyZenith, 0.419);
    }
    // THE LOW DECK IS COMPOSITED AFTER THE CUMULUS, so its alpha and colour are
    // carried out of the deck block. The cumulus slab STARTS at the low deck's
    // own altitude, so the deck is the nearest thing in the sky and has to be
    // able to occlude the bank behind it; drawn in the deck block it could not.
    float a0 = 0.0;
    vec3 col0 = vec3(0.0, 0.0, 0.0);

    // THREE DECKS, drawn BACK TO FRONT (R3.2). Same field, same drift offset
    // the ground shadow projects — one authority (W4). No elevation or
    // distance gate: the LOD inside dfn_cloud_alpha handles the horizon and
    // the extinction above handles the last degree.
    if (dir.y > 0.0005) {
        // --- HIGH, 4400 m: thin, flat, and THE BRIGHTEST OF THE THREE.
        // Brightest on purpose and it is the whole point of the ladder: a hole
        // in the dark low deck then shows the bright high deck behind it,
        // which is reference 12's "holes that show brighter sky" made literal
        // instead of approximated. It gets no self-shadowing — a thin veil at
        // 4.4 km has no depth to shade.
        float dist2 = (u_cloudDeckHigh - eye.y) / dir.y;
        vec2 p2 = eye.xz + dir.xz * dist2;
        float a2 = dfn_cloud_sheet2_alpha(p2, DFN_CLOUD_CELLS_PX_ANISO(p2))
                 * exp(-dist2 / SHEET_EXTINCTION_M) * 0.55;

        // --- MID: the main sheet, AND IT HAS A THICKNESS NOW.
        //
        // This is the last flat thing in the sky and it was rule 52 in one
        // line: a deck read at ONE intersection has a silhouette from below
        // and no vertical dimension at all, so it can only ever be a lid. It
        // is now a SLAB of thickness u_deckThick, read at three altitudes, and
        // two things fall out of that which no amount of shading a plane could
        // produce:
        //
        //   1. PATH. The chord through a slab at elevation th is T/sin(th), so
        //      the same deck is 1 thickness deep overhead and 11 at 5 degrees.
        //      Opacity follows: 1-(1-a)^path. Overhead the holes stay open and
        //      near the horizon the deck closes — which is what an overcast
        //      sky actually does, and it used to be faked by nothing at all.
        //   2. STRUCTURE. The three altitudes are met at three world points
        //      separated by T/(3*dir.y) horizontally, so overhead they are the
        //      same cloud (the coverage is unchanged, by construction) and at
        //      a grazing angle they are DIFFERENT cloud — the ray passes
        //      through several cells of the layer and the silhouette gets an
        //      inside.
        //
        // WHAT IS AND IS NOT UNCHANGED OVERHEAD, and the first version of this
        // comment got it wrong in the direction that flatters the change, so it
        // is corrected rather than deleted. The OPACITY LAW is the old one at
        // high elevation — 300 m of thickness against a 600 m cell puts the
        // path under 1 above about 30 deg, where it clamps, and 1-(1-a)^1 = a.
        // The SAMPLES are not: even straight up the ray crosses 200 m of the
        // layer horizontally between the first slice and the last, a third of a
        // cell, so the three do not coincide and their mean is smoother than one
        // sample. Measured on the overcast probe, sky showing through the deck:
        // 15.6 % -> 11.5 % overhead (the layer blending its own depth) against
        // 5.5 % -> 0.7 % near the horizon (the path closing it). Both are the
        // change working; only the second is the headline.
        // The zero-dose arm (DFN_DECK_THICK=0) IS the shipped sheet, and that
        // one is exact: measured 0.002 % of pixels differing by at most 1/255,
        // which is `pow(x, 1.0)` rounding.
        //
        // The 2-D field is kept ON PURPOSE and the 3-D one is not used here:
        // dfn_cloud_field carries R3.3's per-octave LOD and its renormalisation
        // onto the surviving mean and spread, which is what stopped the hard
        // bright band at the horizon. dfn_cloud_field3 has no LOD at all. The
        // vertical structure here comes from the SEPARATION of the samples, not
        // from a third noise dimension, so nothing is given back for it.
        float dist1 = (u_cloudDeckMid - eye.y) / dir.y;
        vec2 p1 = eye.xz + dir.xz * dist1;
        float cpx1 = DFN_CLOUD_CELLS_PX_ANISO(p1);
        // THE PATH IS A COUNT OF CELLS CROSSED, not 1/dir.y, and the zero-dose
        // arm is what forced the correction. The first form read
        // `clamp(1/dir.y, 1, 12)`, which does not contain the thickness at all
        // — so at DFN_DECK_THICK=0 the deck still closed toward the horizon and
        // the control came back 54.4 % different from the shipped frame instead
        // of identical. A dose that does not appear in the formula is not a
        // dose.
        //
        // The honest quantity: the field's cells are u_cloudWavelength wide, the
        // chord through the slab travels u_deckThick/dir.y HORIZONTALLY, so the
        // ray meets that distance divided by the cell width in independent
        // cells, each cloud with probability `cover`. Transmission is therefore
        // (1-a)^n. It is 1 at the zenith by arithmetic (300 m of travel against
        // a 600 m cell) and 1 at zero thickness by construction, so both the old
        // frame and the control fall out of the same expression.
        // Э6 (волна 23.08): потолок пути — ПО ОСТАТОЧНОМУ РАЗБРОСУ. Жёсткий
        // DECK_PATH_MAX = 12 копил непрозрачность у горизонта там, где поле
        // уже вернуло площадное среднее — структуры нет, а крышка есть; на
        // разрешимом небе потолок остаётся прежним. Доза DFN_CLOUD_PATHRES
        // (слот 38.w), 0 — прежний жёсткий потолок бит-в-бит.
        float pmax1 = u_cloudPathResDose > 0.5
            ? mix(1.0, DECK_PATH_MAX, dfn_cloud_lod_residual(cpx1))
            : DECK_PATH_MAX;
        float path1 = clamp((u_deckThick / max(dir.y, 0.001))
                            / max(u_cloudWavelength, 1.0),
                            1.0, pmax1);
        float a1_mean = 0.0;
        float f1 = 0.0;
        for (int si = 0; si < DECK_SLICES; ++si) {
            float sf = (float(si) + 0.5) / float(DECK_SLICES);
            float alt = u_cloudDeckMid + u_deckThick * sf;
            vec2 ps = eye.xz + dir.xz * ((alt - eye.y) / dir.y);
            a1_mean += dfn_cloud_sheet_alpha(ps, cpx1);
            f1 += dfn_cloud_field(ps + u_cloudOffset, cpx1);
        }
        a1_mean /= float(DECK_SLICES);
        f1 /= float(DECK_SLICES);
        vec2 q1 = p1 + u_cloudOffset;
        float a1 = (1.0 - pow(max(1.0 - a1_mean, 0.0), path1))
                 * exp(-dist1 / SHEET_EXTINCTION_M);
        // TWO shading terms and they answer different questions. Density: a
        // denser core is a thicker core and a thicker core is darker
        // underneath. Direction: the field's slope toward the sun, which is
        // what makes the deck read as MODELLED rather than as textured, and
        // the only one of the two that moves with the hour.
        // ADDED, never max()'d: the density term says how thick this column
        // is and the directional term says which way the light comes at it.
        // max() was measured and rejected — it floored the deck at half-shaded
        // and deleted the directional signal wherever density was the larger.
        // The density term is UNCHANGED, and that is deliberate after the
        // control: the first cut multiplied it by path/2, which is 0.5 at the
        // zenith — it would have made the deck LIGHTER overhead than the sheet
        // it replaced, under a claim of adding depth. The slab already speaks
        // through the alpha and through f1 being a three-slice mean; a second
        // knob on the same effect is how two copies of one number start
        // disagreeing.
        float core1 = smoothstep(1.0 - u_cloudCover * 0.55, 1.0, f1);
        float shade1 = dfn_cloud_self_shade(q1, f1, cpx1);
        vec3 col1 = mix(cloud_bright * DECK_TONE_MID_LIT, cloud_dark,
                        clamp(core1 + shade1, 0.0, 1.0));

        // --- LOW, 1500 m: sparse, ragged, DARK, and IN FRONT. Its cells
        // subtend 1.73x the middle deck's, which is what says "nearer".
        //
        // IT HAS A THICKNESS TOO, and it is a THIRD of the middle deck's. Not a
        // taste: this is the RAGGED deck, the one whose job in R3.2's ladder is
        // to be sparse and let the sky behind it through, and a layer as deep as
        // the main sheet stops having gaps once the chord runs along it — the
        // holes are the feature here. A third of the cell also keeps the near
        // deck's aspect nearer 6:1, which is what scud looks like against
        // stratocumulus at 2:1.
        float dist0 = (u_cloudDeckLow - eye.y) / dir.y;
        vec2 p0 = eye.xz + dir.xz * dist0;
        float cpx0 = DFN_CLOUD_CELLS_PX_ANISO(p0);
        float thick0 = u_deckThick * DECK_LOW_THICK_FRAC;
        float pmax0 = u_cloudPathResDose > 0.5
            ? mix(1.0, DECK_PATH_MAX, dfn_cloud_lod_residual(cpx0))
            : DECK_PATH_MAX;
        float path0 = clamp((thick0 / max(dir.y, 0.001))
                            / max(u_cloudWavelength, 1.0), 1.0, pmax0);
        float a0_mean = 0.0;
        for (int sj = 0; sj < DECK_SLICES; ++sj) {
            float sf0 = (float(sj) + 0.5) / float(DECK_SLICES);
            vec2 ps0 = eye.xz
                     + dir.xz * ((u_cloudDeckLow + thick0 * sf0 - eye.y) / dir.y);
            a0_mean += dfn_cloud_sheet_low_alpha(ps0, cpx0);
        }
        a0_mean /= float(DECK_SLICES);
        a0 = (1.0 - pow(max(1.0 - a0_mean, 0.0), path0))
           * exp(-dist0 / SHEET_EXTINCTION_M);
        vec2 q0 = p0 + u_cloudOffset + DFN_CLOUD_DECK_LOW_SEED;
        float shade0 = dfn_cloud_self_shade(q0, dfn_cloud_field(q0, cpx0),
                                            cpx0);
        col0 = mix(cloud_bright * DECK_TONE_LOW_LIT,
                   cloud_bright * DECK_TONE_LOW_DARK,
                   clamp(0.5 + shade0 * 0.5, 0.0, 1.0));

        // The acceptance arms: each drops exactly one deck and nothing else.
#if DFN_DECK_ARM == 1
        a0 = 0.0;
#elif DFN_DECK_ARM == 2
        a1 = 0.0;
#elif DFN_DECK_ARM == 3
        a2 = 0.0;
#endif
        // BACK TO FRONT. Order is the feature: the low deck is drawn LAST so
        // its holes are where everything behind it shows through — and "behind
        // it" now includes the cumulus slab, so its composite waits below.
        sky = mix(sky, cloud_bright, a2);
        sky = mix(sky, col1, a1);
    }

    // CUMULUS (в10's third kind): the coverage field read in 3D through a SLAB
    // between the low and the middle deck, drifting with the same offset, with
    // a threshold that rises through the slab so the tops are rationed and the
    // base is broad. Biased UPWIND so the densest bank stands where the weather
    // is coming from (W2.3 — the announcement).
    //
    // THE 3-D READ IS KEPT AND IT IS LOAD-BEARING (R3.1). Read in 2-D the field
    // was a function of AZIMUTH ALONE, so alpha was monotone in height for every
    // azimuth and the silhouette was a single-valued skyline: no holes and no
    // overlaps were POSSIBLE, provably, and inverting the threshold gave
    // vertical sides under a flat top — the mushroom caps the lead reported.
    // WHAT CHANGED IS WHERE THE THIRD COORDINATE COMES FROM: it used to be
    // dir.y * 20000, the altitude at which the ray crossed a ring GLUED TO THE
    // EYE, and it is now the real altitude of a real world point on the ray.
    // THE GATE IS WHERE THE VEIL HAS ALREADY GONE, not where it is still worth
    // drawing, and the first cut of this got it wrong: at dir.y 0.004 the
    // converged veil is still 0.83 opaque, so the gate WAS the horizon line —
    // a dead straight edge across the frame. The extinction does the fade on
    // its own once it is allowed to finish: entry distance is 845/dir.y metres,
    // so at 0.002 the veil is 0.10 and by 0.001 it is 0.0002. The gate now sits
    // below that, where it can only ever remove a zero.
    if (u_cloudCumulus > 0.0 && dir.y > 0.0008) {
        vec2 dh = normalize(dir.xz + vec2(1e-5, 0.0));
        float upwind = 0.5 - 0.5 * dot(dh, u_windDir);
        // THE TWO NUMBERS ARE COVERAGE FRACTIONS AT THE TWO ENDS OF THE SLAB.
        // The field is remapped through its own CDF, so it is uniform on [0,1]
        // and a threshold of 1-c admits exactly c of the volume. A cumulus deck
        // is broad at its base and rationed at its tops, which is the one thing
        // that makes a cauliflower rather than a brick.
        float base_cover = clamp(u_cloudCumulus * (0.55 + 0.40 * upwind), 0.0, 0.92);
        float top_cover = base_cover * 0.10;

        float slab_base = u_cloudDeckLow;
        float slab_top = u_cloudDeckMid;
        float t0 = (slab_base - eye.y) / dir.y;
        float t1 = (slab_top - eye.y) / dir.y;
        // Path through the slab, in units of its own THICKNESS. 1 looking
        // straight up, ~28 at 2 degrees of elevation — this ratio is the whole
        // reason the horizon reads as a solid bank and the zenith as separate
        // masses, and it is geometry rather than a second set of constants.
        float path = (t1 - t0) / max(slab_top - slab_base, 1.0);

        // THE FIELD RUNS OUT OF RESOLUTION BEFORE THE SLAB RUNS OUT OF SKY, and
        // the first frame of this said so loudly: a speckled stripe a few
        // degrees tall sat above the horizon, where one pixel spans kilometres
        // ALONG the ray and consecutive taps land in unrelated cells. Same
        // defect the sheets had (R3.3) and the same cure — converge to the
        // honest area average once there is nothing left to resolve. The field
        // is uniform on [0,1] by construction, so the average above a threshold
        // T is exactly 1-T and no second constant is needed.
        //
        // The derivative is taken ONCE, at the middle of the slab, and outside
        // the tap loop: screen derivatives inside a loop are the kind of thing
        // that is fine until a driver disagrees.
        vec3 w_mid = eye + dir * mix(t0, t1, 0.5);
        float cum_cells_px = max(length(dFdx(w_mid)), length(dFdy(w_mid)))
                           / (max(u_cloudWavelength, 1.0) * CUMULUS_SCALE);
        float cum_dead = smoothstep(0.35, 0.75, cum_cells_px);

        float transmit = 1.0;
        float lit_sum = 0.0;
        float dens_sum = 0.0;
        for (int i = 0; i < CUMULUS_TAPS; ++i) {
            float f = (float(i) + 0.5) / float(CUMULUS_TAPS);
            float t = mix(t0, t1, f);
            vec3 w = eye + dir * t;
            // The SAME field, at the real world point, with altitude stretched
            // so one cell is about as tall as the slab and ~1.6x wider — the
            // proportion a fair-weather cumulus actually has.
            vec3 q3 = vec3(w.x + u_cloudOffset.x,
                           w.y * CUMULUS_VERTICAL_STRETCH,
                           w.z + u_cloudOffset.y)
                    / (max(u_cloudWavelength, 1.0) * CUMULUS_SCALE);
            float F = dfn_cloud_field3(q3);
            float T = mix(1.0 - base_cover, 1.0 - top_cover, f);
            float d = mix(smoothstep(T, T + 0.06, F), 1.0 - T, cum_dead);
            // Beer-Lambert through this segment: the segment is path/N slab
            // thicknesses long.
            float seg = d * (path / float(CUMULUS_TAPS));
            float take = transmit * (1.0 - exp(-CUMULUS_DENSITY * seg));
            // The colour is accumulated with the SAME weights the opacity is,
            // so a mass shows the tone of the part of it that is actually in
            // front — a base-lit sample cannot tint a body the ray never
            // reached.
            lit_sum += take * f;
            dens_sum += take * smoothstep(T + 0.06, T + 0.28, F) * (1.0 - f);
            transmit *= exp(-CUMULUS_DENSITY * seg);
        }
        // THE VEIL LAW GOES ON THE FINISHED ALPHA, not inside the integral, and
        // the frame that made me move it showed why: with the extinction applied
        // per segment, the path length near the horizon saturates the opacity
        // faster than the distance can fade it, so a 40 km bank came out as an
        // opaque grey WALL standing on the horizon instead of melting into it.
        // Applied once, on the slab's own entry distance, it is the same
        // 60 km law the two sheets use and the bank fades into the sky the way
        // everything else at that range does.
        float cum = (1.0 - transmit) * exp(-t0 / SHEET_EXTINCTION_M);
        if (cum > 0.001) {
            float hn = clamp(lit_sum / max(cum, 1e-4), 0.0, 1.0);
            // SHADED BASES ARE WHAT SEPARATE A BANK FROM THE HAZE. A cumulus is
            // lit from above and its flat base is the darkest thing in the sky
            // near the horizon — and near the horizon the SKY is the pale
            // horizon colour, so a white-to-the-bottom mass has nothing to read
            // against and dissolves upward into a floating tooth.
            vec3 cum_col = mix(cloud_dark * 0.78, cloud_bright,
                               smoothstep(0.10, 0.75, hn));
            cum_col *= mix(1.0, 0.82, clamp(dens_sum / max(cum, 1e-4), 0.0, 1.0));
            sky = mix(sky, cum_col, cum);
        }
    }

    // ...and NOW the low deck, in front of everything including the bank.
    sky = mix(sky, col0, a0);

    // The sun disc is gated by its own colour, which apply_sky_time drives to
    // black below the horizon — no separate "is it day" flag needed. Drawn
    // after the clouds but attenuated by the sheet at ITS direction, so cover
    // dims the disc to a glow instead of the disc burning through a cloud.
    float sun_dot = clamp(dot(dir, u_sunDir), -1.0, 1.0);
    float sun_occl = 1.0;
    if (u_sunDir.y > 0.03) {
        vec2 ps = eye.xz
                + u_sunDir.xz * ((u_cloudDeckMid - eye.y) / u_sunDir.y);
        sun_occl = 1.0 - 0.85 * dfn_cloud_sheet_alpha(ps, 0.0);
    }
    // THE SUN HAS A BODY (W9). What stood here was two glow lobes,
    // pow(dot,900)*0.85 + pow(dot,24)*0.10, ADDED to the sky — and what the
    // player read as a disc was the RGBA8 clamp cutting that glow off at 1.0.
    // Measured at noon by design, per channel: red saturated out to 1.40 deg,
    // green to 1.95, blue to 2.82. So the "sun" was a ~5.6 deg white smear
    // with coloured fringes, it had no edge anywhere, and its apparent SIZE
    // drifted through the day with u_sunColor and the sky ramp. Nothing in
    // the code named it. "Ярко есть, солнца не видно" is a precise report.
    float sun_ang = acos(sun_dot);
    float sun_luma = dot(u_sunColor, DFN_LUMA_WEIGHTS);
    if (sun_luma > 0.001) {
        // Unit-luma hue: the rows below fix BRIGHTNESS in the quantiser's
        // metric, so the colour has to carry the hue and nothing else, or the
        // separations design derived are separations of a different number.
        // This also gates the sun without a day flag — apply_sky_time drives
        // u_sunColor to black under the horizon and the luma goes with it.
        vec3 hue = u_sunColor / sun_luma;
        // THE GLARE, AND ITS CEILING IS THE ACTUAL FIX. The sky beside the
        // sun used to reach the top of the range, so a disc had nowhere above
        // it to stand and "make the sun brighter" was a no-op — that is why
        // this is a halo with a CAP rather than a bigger, brighter blob.
        // u_sunGlareLumaMax sits two quantiser steps below the top, not one,
        // because one step IS the quantisation cell and a threshold equal to
        // the instrument's resolution has no margin at all (Rule 30a).
        float glare = 1.0 - smoothstep(0.0, u_sunGlareRadius, sun_ang);
        glare *= glare; // bright core, long tail, still zero at the rim
        sky = mix(sky, hue * u_sunGlareLumaMax, clamp(glare * sun_occl, 0.0, 1.0));
        // THE DISC. Antialiased over ONE PIXEL of sky, taken from the
        // derivative rather than from a constant: at 640x360 the disc is ~9 px
        // and a hard threshold rasterises it as a diamond, and the 320x180
        // preset would need a different constant. fwidth is right at both.
        float aa = max(fwidth(sun_ang), 1e-5);
        float disc = 1.0 - smoothstep(u_sunDiscRadius - aa,
                                      u_sunDiscRadius + aa, sun_ang);
        // COMPOSITE, NEVER ADD. `sky += disc` clamps to white, which destroys
        // every luma relation the sky rules are written in and is exactly how
        // the old glow ended up defining its own size by saturation.
        sky = mix(sky, hue * u_sunDiscLuma, clamp(disc * sun_occl, 0.0, 1.0));
    }

    gl_FragColor = vec4(sky, 1.0);
}
