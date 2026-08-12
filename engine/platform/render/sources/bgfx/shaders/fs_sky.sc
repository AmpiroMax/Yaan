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

// Horizon cumulus, sized against the distance it is READ from (Rule 33). A
// bank on a 20 km ring with a 1100 m base and 3400 m tops subtends dir.y
// 0.055..0.170 — a band about 6.6 deg tall, ~85 px of a 640x360 frame at the
// tour's 45 deg FOV, so an individual mass is legible rather than a pixel of
// texture. The previous ring was 6.5 km with a base BELOW the horizon, which
// put the whole body under the terrain line and left only the tapering tips
// showing: the "floating funnels" of the first shoot.
#define CUMULUS_RING_M   20000.0
#define CUMULUS_BASE_M    1100.0
#define CUMULUS_TOP_M     3800.0
#define CUMULUS_BASE_Y   (CUMULUS_BASE_M / CUMULUS_RING_M)
#define CUMULUS_TOP_Y    (CUMULUS_TOP_M / CUMULUS_RING_M)
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
float dfn_stars(vec3 dir)
{
    vec3 g = dir * 150.0;
    vec3 cell = floor(g);
    vec3 f = g - cell;
    float h = fract(sin(dot(cell, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
    if (h < 0.9860) {
        return 0.0;
    }
    vec3 p = vec3(fract(h * 17.0), fract(h * 31.0), fract(h * 57.0));
    float d = length(f - p);
    float brightness = 0.35 + 0.65 * fract(h * 91.0);
    return smoothstep(0.30, 0.0, d) * brightness;
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
        float dist2 = (DFN_CLOUD_DECK_HIGH_M - eye.y) / dir.y;
        vec2 p2 = eye.xz + dir.xz * dist2;
        float a2 = dfn_cloud_sheet2_alpha(p2, DFN_CLOUD_CELLS_PX(p2))
                 * exp(-dist2 / SHEET_EXTINCTION_M) * 0.55;

        // --- MID, 2600 m: the main sheet.
        float dist1 = (DFN_CLOUD_DECK_MID_M - eye.y) / dir.y;
        vec2 p1 = eye.xz + dir.xz * dist1;
        float cpx1 = DFN_CLOUD_CELLS_PX(p1);
        float a1 = dfn_cloud_sheet_alpha(p1, cpx1)
                 * exp(-dist1 / SHEET_EXTINCTION_M);
        vec2 q1 = p1 + u_cloudOffset;
        float f1 = dfn_cloud_field(q1, cpx1);
        // TWO shading terms and they answer different questions. Density: a
        // denser core is a thicker core and a thicker core is darker
        // underneath. Direction: the field's slope toward the sun, which is
        // what makes the deck read as MODELLED rather than as textured, and
        // the only one of the two that moves with the hour.
        // ADDED, never max()'d: the density term says how thick this column
        // is and the directional term says which way the light comes at it.
        // max() was measured and rejected — it floored the deck at half-shaded
        // and deleted the directional signal wherever density was the larger.
        float core1 = smoothstep(1.0 - u_cloudCover * 0.55, 1.0, f1);
        float shade1 = dfn_cloud_self_shade(q1, f1, cpx1);
        vec3 col1 = mix(cloud_bright * DECK_TONE_MID_LIT, cloud_dark,
                        clamp(core1 + shade1, 0.0, 1.0));

        // --- LOW, 1500 m: sparse, ragged, DARK, and IN FRONT. Its cells
        // subtend 1.73x the middle deck's, which is what says "nearer".
        float dist0 = (DFN_CLOUD_DECK_LOW_M - eye.y) / dir.y;
        vec2 p0 = eye.xz + dir.xz * dist0;
        float cpx0 = DFN_CLOUD_CELLS_PX(p0);
        float a0 = dfn_cloud_sheet_low_alpha(p0, cpx0)
                 * exp(-dist0 / SHEET_EXTINCTION_M);
        vec2 q0 = p0 + u_cloudOffset + DFN_CLOUD_DECK_LOW_SEED;
        float shade0 = dfn_cloud_self_shade(q0, dfn_cloud_field(q0, cpx0),
                                            cpx0);
        vec3 col0 = mix(cloud_bright * DECK_TONE_LOW_LIT,
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
        // its holes are where the two decks behind it show through.
        sky = mix(sky, cloud_bright, a2);
        sky = mix(sky, col1, a1);
        sky = mix(sky, col0, a0);
    }

    // Cumulus on the horizon ring (в10 third kind): the coverage field read in
    // 3D on a far ring, drifting with the same offset, against a threshold that
    // rises with elevation so the tops are rationed and the base is broad.
    // Biased UPWIND so the densest bank stands where the weather is coming from
    // (W2.3 — the announcement).
    //
    // THE ROUNDED DOMES THIS ONCE AIMED FOR WERE THE DEFECT, not the goal. Read
    // in 2-D the field is a function of AZIMUTH ALONE, so alpha was monotone in
    // height for every azimuth and the silhouette was a single-valued skyline:
    // no holes and no overlaps were POSSIBLE, provably. Inverting a squared
    // threshold then gave height ~ sqrt(field) — vertical where a lobe crosses
    // the threshold, flat at the lobe's peak — and vertical sides under a flat
    // top is a mushroom cap. Half a dozen of them sitting on the horizon is
    // what the lead reported, and he was right that it reads as breakage rather
    // than as style. The dimensionality was the defect; the exponent was not.
    if (u_cloudCumulus > 0.0 && dir.y < CUMULUS_TOP_Y + 0.02
        && dir.y > CUMULUS_BASE_Y - 0.004) {
        vec2 dh = normalize(dir.xz + vec2(1e-5, 0.0));
        float upwind = 0.5 - 0.5 * dot(dh, u_windDir);
        // THE TWO NUMBERS ARE COVERAGE FRACTIONS AT THE TWO ENDS OF THE BAND.
        // The field is remapped through its own CDF, so it is uniform on [0,1]
        // and a threshold of 1-c admits exactly c of the band. A bank of cumulus
        // at 20 km is nearly CONTINUOUS along the horizon; what varies is how
        // much of it survives with height. So the base is broad and only the
        // tops are rationed.
        float base_cover = clamp(u_cloudCumulus * (0.55 + 0.40 * upwind), 0.0, 0.92);
        float top_cover = base_cover * 0.10;
        float hn = clamp((dir.y - CUMULUS_BASE_Y)
                         / (CUMULUS_TOP_Y - CUMULUS_BASE_Y), 0.0, 1.0);
        // THE FIELD IS READ IN 3D NOW, and that is the whole fix for the domes.
        // The point is where this view ray meets the ring: horizontally the ring
        // position (continuous all the way round — no azimuth seam), vertically
        // the altitude it meets it at. Altitude is stretched by
        // CUMULUS_VERTICAL_STRETCH so one field cell is about as tall as the
        // band and about 1.6x wider, which is the proportion a bank of cumulus
        // actually has.
        vec3 ring3 = vec3(eye.x + dh.x * CUMULUS_RING_M + u_cloudOffset.x,
                          dir.y * CUMULUS_RING_M * CUMULUS_VERTICAL_STRETCH,
                          eye.z + dh.y * CUMULUS_RING_M + u_cloudOffset.y)
                   / (max(u_cloudWavelength, 1.0) * CUMULUS_SCALE);
        float F = dfn_cloud_field3(ring3);
        // LINEAR again, and it is allowed to be now. The squared exponent was
        // there to bend the shape of an inverted 1-D function; in 3D the shape
        // comes from the field itself, so the threshold only has to ration how
        // much cloud survives with height — which is what thins the tops.
        float T = mix(1.0 - base_cover, 1.0 - top_cover, hn);
        // The flat base stays a hard gate: a cumulus bank's defining line is
        // that every mass in it begins at the SAME altitude.
        float cum = smoothstep(T, T + 0.06, F)
                  * smoothstep(CUMULUS_BASE_Y - 0.004, CUMULUS_BASE_Y + 0.006,
                               dir.y);
        // SHADED BASES ARE WHAT SEPARATE A BANK FROM THE HAZE. A cumulus is lit
        // from above and its flat base is the darkest thing in the sky near the
        // horizon — and near the horizon the SKY is the pale horizon colour, so
        // a white-to-the-bottom mass has nothing to read against and dissolves
        // upward into a floating tooth. Held dark through the lower third, then
        // climbing. The field's own local density adds the second half of the
        // self-shadowing: a denser core is a thicker core, and a thicker core is
        // darker underneath.
        vec3 cum_col = mix(cloud_dark * 0.78, cloud_bright,
                           smoothstep(0.10, 0.75, hn));
        cum_col *= mix(1.0, 0.82, smoothstep(T + 0.06, T + 0.28, F)
                                      * (1.0 - hn));
        sky = mix(sky, cum_col, cum);
    }

    // The sun disc is gated by its own colour, which apply_sky_time drives to
    // black below the horizon — no separate "is it day" flag needed. Drawn
    // after the clouds but attenuated by the sheet at ITS direction, so cover
    // dims the disc to a glow instead of the disc burning through a cloud.
    float sun_dot = clamp(dot(dir, u_sunDir), -1.0, 1.0);
    float sun_occl = 1.0;
    if (u_sunDir.y > 0.03) {
        vec2 ps = eye.xz
                + u_sunDir.xz * ((DFN_CLOUD_DECK_MID_M - eye.y) / u_sunDir.y);
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
