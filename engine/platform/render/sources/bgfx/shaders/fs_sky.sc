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

#include <bgfx_shader.sh>
#include "dfn_env.sh"

// The sheet fades out into the horizon haze band instead of shrinking to a
// vanishing-point moire; cumulus lives BELOW this, on the ring. The DIST pair
// is the same guard by distance: past ~8 km a 600 m cell is under 20 internal
// pixels and compresses into radial streaks (the "funnel" artifact the first
// shoot caught), so the sheet melts into haze there instead.
#define SHEET_HORIZON_FADE_LO 0.05
#define SHEET_HORIZON_FADE_HI 0.18
#define SHEET_FAR_FADE_LO_M   8000.0
#define SHEET_FAR_FADE_HI_M  16000.0
// Cumulus ring distance and angular height of the tallest tower.
#define CUMULUS_RING_M   6500.0
#define CUMULUS_TOP_Y    0.13
// Cumulus bases extend BELOW the apparent horizon (the terrain silhouette
// draws over the sky, so the base is clipped by real ridges, not by a fade
// that leaves towers floating — the first shoot's second artifact).
#define CUMULUS_BASE_Y  -0.03

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

    float sheet_fade = smoothstep(SHEET_HORIZON_FADE_LO,
                                  SHEET_HORIZON_FADE_HI, dir.y);
    if (sheet_fade > 0.0) {
        // Layer 1, the main sheet: view ray onto the low plane. Same field,
        // same offset the ground shadow projects — one authority (W4).
        float dist1 = (DFN_CLOUD_LAYER1_M - eye.y) / dir.y;
        vec2 p1 = eye.xz + dir.xz * dist1;
        float fade1 = sheet_fade
                    * (1.0 - smoothstep(SHEET_FAR_FADE_LO_M,
                                        SHEET_FAR_FADE_HI_M, dist1));
        float a1 = dfn_cloud_sheet_alpha(p1) * fade1;
        // Layer 2, high and thin: farther plane = slower apparent drift and
        // smaller cells — the parallax that makes the sky read as deep.
        float dist2 = (DFN_CLOUD_LAYER2_M - eye.y) / dir.y;
        vec2 p2 = eye.xz + dir.xz * dist2;
        float fade2 = sheet_fade
                    * (1.0 - smoothstep(SHEET_FAR_FADE_LO_M * 1.5,
                                        SHEET_FAR_FADE_HI_M * 1.5, dist2));
        float a2 = dfn_cloud_sheet2_alpha(p2) * fade2 * 0.65;

        // Denser core -> darker base: reuse the field so shading and shape
        // cannot disagree.
        float core1 = smoothstep(1.0 - u_cloudCover * 0.6, 1.0,
                                 dfn_cloud_field(p1 + u_cloudOffset));
        vec3 col1 = mix(cloud_bright, cloud_dark, core1);
        sky = mix(sky, cloud_bright, a2); // thin high sheet first (behind)
        sky = mix(sky, col1, a1);
    }

    // Cumulus impostors on the horizon ring (в10 third kind): anchored to
    // WORLD points on a far ring, drifting with the same offset, biased
    // UPWIND so the densest towers stand where the weather will come from
    // (W2.3 — the announcement). Terrain draws over the sky afterwards, so
    // every tower sits BEHIND the far ridges by construction, and the FLAT
    // BASE below the apparent horizon is clipped by real ground, never by a
    // fade (a fade left the first shoot's towers floating).
    if (u_cloudCumulus > 0.0 && dir.y < CUMULUS_TOP_Y + 0.03
        && dir.y > CUMULUS_BASE_Y) {
        vec2 dh = normalize(dir.xz + vec2(1e-5, 0.0));
        vec2 ring = eye.xz + dh * CUMULUS_RING_M;
        vec2 q = (ring + u_cloudOffset * 0.6) / (u_cloudWavelength * 2.4);
        float tower = dfn_cloud_vnoise(q);
        float detail = dfn_cloud_vnoise(q * 3.13 + vec2(9.0, 23.0));
        float upwind = 0.5 - 0.5 * dot(dh, u_windDir);
        float density = u_cloudCumulus * (0.30 + 0.90 * upwind);
        // DISTINCT towers: the gate keeps only the ridges of the ring noise,
        // so the horizon carries separate masses with sky between them
        // rather than a continuous rampart.
        float gate = smoothstep(0.60 - 0.28 * density, 0.76 - 0.28 * density,
                                tower);
        // Crenellated tops: the detail octave varies the summit line.
        float top_y = gate * (0.55 + 0.45 * detail) * CUMULUS_TOP_Y;
        // Solid body up to ~3/4 height, then a domed soft top.
        float hfrac = (dir.y - CUMULUS_BASE_Y)
                    / max(top_y - CUMULUS_BASE_Y, 0.001);
        float cum = gate * (1.0 - smoothstep(0.72, 1.0, hfrac));
        // Sunlit tops, shaded bases — the same hour palette as the sheets.
        vec3 cum_col = mix(cloud_dark * 0.90, cloud_bright,
                           clamp(0.25 + 0.75 * hfrac, 0.0, 1.0));
        sky = mix(sky, cum_col, cum);
    }

    // The sun disc is gated by its own colour, which apply_sky_time drives to
    // black below the horizon — no separate "is it day" flag needed. Drawn
    // after the clouds but attenuated by the sheet at ITS direction, so cover
    // dims the disc to a glow instead of the disc burning through a cloud.
    float sun_dot = max(dot(dir, u_sunDir), 0.0);
    float sun_occl = 1.0;
    if (u_sunDir.y > SHEET_HORIZON_FADE_LO) {
        vec2 ps = eye.xz
                + u_sunDir.xz * ((DFN_CLOUD_LAYER1_M - eye.y) / u_sunDir.y);
        sun_occl = 1.0 - 0.85 * dfn_cloud_sheet_alpha(ps);
    }
    sky += u_sunColor * ((pow(sun_dot, 900.0) * 0.85
                          + pow(sun_dot, 24.0) * 0.10) * sun_occl);

    gl_FragColor = vec4(sky, 1.0);
}
