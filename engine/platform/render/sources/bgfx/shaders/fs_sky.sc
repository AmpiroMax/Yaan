$input v_dir

// Sky fragment (day/night, в1/в2 + clouds, в4/в10 / WEATHER.md W4):
// horizon->zenith gradient with a haze band at the horizon (matching the fog
// color so distant terrain melts into the sky at every hour), the sun disc
// and glow, a STAR FIELD, a MOON DISC WITH PHASE, and THREE KINDS OF CLOUD —
// two drifting parallax sheets (planes at DFN_CLOUD_LAYER1/2_M read through
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

#include <bgfx_shader.sh>
#include "dfn_env.sh"

// The sheet meets the horizon by CONVERGING TO ITS OWN AREA AVERAGE (the LOD
// in dfn_cloud_alpha), which is what an unresolvable sheet honestly looks
// like — a veil. It used to be cut instead: an elevation fade over 0.05..0.18
// and a distance fade over 8..16 km, which between them deleted 22.4% of the
// sky's pixels and left a hard horizontal SHELF at dir.y ~ 0.07 with empty sky
// under it. That shelf, not the projection, is what the first shoot read as
// "the sheet only materialises near the horizon". All that is left here is the
// last degree or so, blended into the haze band the sky already draws.
#define SHEET_HAZE_LO 0.004
#define SHEET_HAZE_HI 0.030

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
    float up = clamp(dir.y, 0.0, 1.0);
    float horizon_band = pow(1.0 - up, 3.0);
    vec3 sky = mix(u_skyZenith, u_skyHorizon, horizon_band);

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

    // Layer 1, the main sheet: view ray onto the low plane. Same field, same
    // offset the ground shadow projects — one authority (W4). No elevation or
    // distance gate: the LOD inside dfn_cloud_alpha handles the horizon.
    if (dir.y > 0.0005) {
        float dist1 = (DFN_CLOUD_LAYER1_M - eye.y) / dir.y;
        vec2 p1 = eye.xz + dir.xz * dist1;
        float cpx1 = DFN_CLOUD_CELLS_PX(p1);
        float haze = smoothstep(SHEET_HAZE_LO, SHEET_HAZE_HI, dir.y);
        float a1 = dfn_cloud_sheet_alpha(p1, cpx1) * haze;
        // Layer 2, high and thin: farther plane = slower apparent drift and
        // smaller cells — the parallax that makes the sky read as deep.
        float dist2 = (DFN_CLOUD_LAYER2_M - eye.y) / dir.y;
        vec2 p2 = eye.xz + dir.xz * dist2;
        float a2 = dfn_cloud_sheet2_alpha(p2, DFN_CLOUD_CELLS_PX(p2))
                 * haze * 0.55;

        // Denser core -> darker base: reuse the field so shading and shape
        // cannot disagree.
        float core1 = smoothstep(1.0 - u_cloudCover * 0.55, 1.0,
                                 dfn_cloud_field(p1 + u_cloudOffset, cpx1));
        vec3 col1 = mix(cloud_bright, cloud_dark, core1);
        sky = mix(sky, cloud_bright, a2); // thin high sheet first (behind)
        sky = mix(sky, col1, a1);
    }

    // Cumulus on the horizon ring (в10 third kind): THE SAME coverage field,
    // read on a far ring and drifting with the same offset, against a
    // threshold that RISES with elevation. Where the field is strong the mass
    // climbs high, where it is weak it stays a low lump — so the silhouette is
    // a set of rounded domes with the field's own octaves for a cauliflower
    // rim. The shipped version multiplied one saturating gate into both the
    // presence and the height, which made every mass the same flat-topped
    // trapezoid. Biased UPWIND so the densest bank stands where the weather is
    // coming from (W2.3 — the announcement).
    if (u_cloudCumulus > 0.0 && dir.y < CUMULUS_TOP_Y + 0.02
        && dir.y > CUMULUS_BASE_Y - 0.004) {
        vec2 dh = normalize(dir.xz + vec2(1e-5, 0.0));
        vec2 ring = (eye.xz + dh * CUMULUS_RING_M + u_cloudOffset)
                  / CUMULUS_SCALE;
        float upwind = 0.5 - 0.5 * dot(dh, u_windDir);
        // THE TWO NUMBERS ARE COVERAGE FRACTIONS AT THE TWO ENDS OF THE BAND,
        // and stating them that way is the correction. The field is remapped
        // through its own CDF, so it is uniform on [0,1] and a threshold of
        // 1-c admits exactly c of all azimuths. The shipped form folded base
        // and top into one `dens`, which made the BASE as sparse as the tops
        // were rare: at cumulus 0.5 only ~35% of azimuths carried any cloud at
        // all, and each of those was the narrow peak of a field lobe. A bank of
        // cumulus at 20 km is nearly CONTINUOUS along the horizon; what varies
        // is how high each mass climbs. So the base is broad and only the tops
        // are rationed.
        float base_cover = clamp(u_cloudCumulus * (0.55 + 0.40 * upwind), 0.0, 0.92);
        float top_cover = base_cover * 0.06;
        float F = dfn_cloud_field(ring, 0.02);
        float hn = clamp((dir.y - CUMULUS_BASE_Y)
                         / (CUMULUS_TOP_Y - CUMULUS_BASE_Y), 0.0, 1.0);
        // THE THRESHOLD RISES WITH hn SQUARED, and the reason is the SHAPE OF
        // THE INVERSE. F is a function of AZIMUTH ALONE, so a column stands
        // from the base up to where T(hn) reaches F: the skyline IS hn_max(F).
        // Squared gives hn_max ~ sqrt(F), which is steep at the flanks and
        // FLAT at the top — a cumulus profile. Linear was tried and gives
        // hn_max ~ F, an affine image of a field whose lobes are conical, so
        // every mass came out a straight-sided TENT and the horizon read as a
        // distant mountain range.
        //
        // (The vertical teeth this pass started from were NOT this exponent's
        // fault, which is why the linear experiment was worth running: they
        // came from CUMULUS_SCALE making a mass narrower than the band is
        // tall. Fix the aspect ratio and the square root is right again.)
        float T = mix(1.0 - base_cover, 1.0 - top_cover, hn * hn);
        float cum = smoothstep(T, T + 0.05, F)
                  * smoothstep(CUMULUS_BASE_Y - 0.004, CUMULUS_BASE_Y + 0.006,
                               dir.y);
        // SHADED BASES ARE WHAT SEPARATE A BANK FROM THE HAZE. A cumulus is
        // lit from above and its flat base is the darkest thing in the sky
        // near the horizon — and near the horizon the SKY is the pale horizon
        // colour, so a white-to-the-bottom mass has nothing to read against
        // and dissolves upward into a floating tooth. The ramp therefore holds
        // the dark through the lower third and only then climbs.
        vec3 cum_col = mix(cloud_dark * 0.78, cloud_bright,
                           smoothstep(0.10, 0.75, hn));
        sky = mix(sky, cum_col, cum);
    }

    // The sun disc is gated by its own colour, which apply_sky_time drives to
    // black below the horizon — no separate "is it day" flag needed. Drawn
    // after the clouds but attenuated by the sheet at ITS direction, so cover
    // dims the disc to a glow instead of the disc burning through a cloud.
    float sun_dot = max(dot(dir, u_sunDir), 0.0);
    float sun_occl = 1.0;
    if (u_sunDir.y > 0.03) {
        vec2 ps = eye.xz
                + u_sunDir.xz * ((DFN_CLOUD_LAYER1_M - eye.y) / u_sunDir.y);
        sun_occl = 1.0 - 0.85 * dfn_cloud_sheet_alpha(ps, 0.0);
    }
    sky += u_sunColor * ((pow(sun_dot, 900.0) * 0.85
                          + pow(sun_dot, 24.0) * 0.10) * sun_occl);

    gl_FragColor = vec4(sky, 1.0);
}
