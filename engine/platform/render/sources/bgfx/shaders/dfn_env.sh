/*
Created: 09:08:2026 - 10:52:00
Last updated: 10:08:2026 - 20:10:49
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_env.sh

Responsibility:
- Shared shader include: the frame environment uniform block (RenderEnvironment
  -> u_envParams vec4 array) with named accessor macros. Single source of the
  index layout; BgfxRenderer.cpp packs the array in exactly this order.

Key items:
- u_envParams[33]; accessor #defines (sun, ambient, fog, sky, splat, water,
  moon, stars, point light) + dfn_surface_light() / dfn_fog_factor().

Dependencies:
- Uses: nothing (included after bgfx_shader.sh).
- Used by: fs_terrain, fs_water, fs_sky, vs shaders needing env values.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Index layout is a contract with BgfxRenderer.cpp::apply_environment — change
  both together or not at all.
*/
/*
UPD:
- 09:08:2026 - 10:52:00: Stage 3 — initial environment uniform layout.
- 09:08:2026 - 19:20:00: Day/night (в1/в2): moon + stars + carried point light
  slots (11..14) and the shared dfn_surface_light() used by terrain and props,
  so sun, moon, torch and the sky-visibility ambient are computed in ONE place.
- 09:08:2026 - 19:32:00: Light ARRAY (up to 8) replaces the single point
  light, plus authored u_ambientDarkness; env block 15 -> 32 vec4s.
- 09:08:2026 - 19:58:00: Wind slots [32] + shared dfn_wind_offset (foliage now,
  grass and cloth later); env block 32 -> 33 vec4s.
- 09:08:2026 - 20:31:00: Carried lights now SHADOW: dfn_pointshadow.sh included
  here (the light loop is here, so a torch cannot shadow on terrain and not on
  props), shadow-casting lights are packed first so the slot index is the cube
  index.
- 09:08:2026 - 21:06:00: dfn_screen_door + dfn_bayer4 (DrawParams::fade as an
  ordered dissolve, the LOD cross-fade's mechanism). The pixel coordinate is a
  PARAMETER: this header is included by vertex shaders, where gl_FragCoord does
  not exist, and naming it in the body broke every one of them.
- 10:08:2026 - 02:59:00: CLOUDS (WEATHER.md W4): env block 33 -> 35 vec4s
  (slots 33/34 = the weather/cloud state + drift offset), the ONE cloud
  coverage field (dfn_cloud_field / dfn_cloud_sheet_alpha / _sheet2_alpha)
  sampled by BOTH the sky sheet (fs_sky) and the ground shadow
  (dfn_cloud_sun_vis, applied to the sun term inside dfn_surface_light so
  terrain, props and foliage all darken together and cannot disagree).
- 10:08:2026 - 10:45:06: CLOUD FIELD FIXED, after reproducing it numerically (Rule 30b).
  (1) Rule 31: the octave sum is GAUSSIAN — measured over 400k samples it held
  98% of its mass in 0.200..0.797 of the [0,1] it declared, so cover 0.10 drew
  NOTHING and the default 0.45 drew 0.19. Remapped through its own CDF, so
  cover now means coverage within 0.024 across the whole range, both ends
  asserted. (2) Per-octave LOD on an ANISOTROPIC cells-per-pixel metric (the
  radial axis runs 20x the tangential one near the horizon — the streaks), and
  convergence to the area mean past the resolution limit, which replaced the
  distance/elevation fades that had been deleting 22% of the sky. (3) Rule 33:
  layers 1200/2200 -> 2600/4400 m, because at 1200 m the whole sky above 45 deg
  saw 10.8 cells of field AREA and mid-sky had nothing to draw at any
  threshold. The wavelength could not move — it is shared with the ground.
- 10:08:2026 - 20:10:49: env block 35 -> 36 vec4s. Slot 35 carries the SUN'S
  BODY (SUN_ANGULAR_DIAMETER / SUN_GLARE_ANGULAR_DIAMETER / SUN_DISC_LUMA /
  SUN_GLARE_LUMA_MAX) from the GENERATED header rather than as #defines here:
  design derives those rows and the shader measures the frame with them, so
  they have two consumers and may exist exactly once (Rule 35). Paired with
  apply_environment per this file's own contract notice.
*/

#ifndef DFN_ENV_SH
#define DFN_ENV_SH

// Cube shadows for the carried lights. Included here rather than by each
// fragment shader because the light LOOP lives here: a torch that shadowed in
// terrain but not in props would be worse than one that never shadowed.
#include "dfn_pointshadow.sh"

uniform vec4 u_envParams[36];

#define u_sunDir         (u_envParams[0].xyz)
#define u_sunColor       (u_envParams[1].xyz)
#define u_ambientColor   (u_envParams[2].xyz)
#define u_fogColor       (u_envParams[3].xyz)
#define u_fogStart       (u_envParams[4].x)
#define u_fogEnd         (u_envParams[4].y)
#define u_envTime        (u_envParams[4].z)
#define u_skyZenith      (u_envParams[5].xyz)
#define u_skyHorizon     (u_envParams[6].xyz)
#define u_sandHeight     (u_envParams[7].x)
#define u_sandBlend      (u_envParams[7].y)
#define u_rockSlopeStart (u_envParams[7].z)
#define u_rockSlopeEnd   (u_envParams[7].w)
#define u_terrainTiles   (u_envParams[8].x)
#define u_waterColor     (u_envParams[9])
#define u_waterScroll    (u_envParams[10].xy)
#define u_moonDir        (u_envParams[11].xyz)
#define u_moonPhase      (u_envParams[11].w)
#define u_moonColor      (u_envParams[12].xyz)
#define u_moonLight      (u_envParams[12].w)
#define u_starIntensity  (u_envParams[14].w)
// Authored darkness of the PLACE the player is in (0 = normal, 1 = the black
// void the user asked for in deep caves). Multiplies what survives the
// geometric sky-visibility term, and shortens carried lights.
#define u_ambientDarkness (u_envParams[15].x)
#define u_lightCount      (u_envParams[15].y)
// Point lights: [16+i] = position.xyz + radius, [24+i] = colour.xyz + flags.
#define DFN_MAX_LIGHTS 8
#define u_lightPosRad(i) (u_envParams[16 + (i)])
#define u_lightColor(i)  (u_envParams[24 + (i)])
// Wind: ONE wind for the world (foliage now, grass and cloth later).
#define u_windDir        (u_envParams[32].xy)
#define u_windStrength   (u_envParams[32].z)
#define u_windFlutter    (u_envParams[32].w)
// Clouds (WEATHER.md W4): the weather-state tuple's cloud slice plus the
// drift offset of the ONE coverage field. Both the sky sheet and the ground
// shadow sample the field through u_cloudOffset — never a second offset.
#define u_cloudCover      (u_envParams[33].x)
#define u_cloudCumulus    (u_envParams[33].y)
#define u_cloudShadow     (u_envParams[33].z)
#define u_cloudWavelength (u_envParams[33].w)
#define u_cloudOffset     (u_envParams[34].xy)
// The SUN'S BODY (W9). These are NUMBERS rows, not look-dev values, and they
// arrive through the uniform rather than as #defines here for one reason:
// design derives them and render measures the frame with them, so they have
// two consumers and may exist exactly once (Rule 35). The backend fills this
// slot straight from the generated header, so there is no second copy to
// drift. Radii, not diameters — the shader compares against an angle.
#define u_sunDiscRadius   (u_envParams[35].x)
#define u_sunGlareRadius  (u_envParams[35].y)
#define u_sunDiscLuma     (u_envParams[35].z)
#define u_sunGlareLumaMax (u_envParams[35].w)
// The quantiser's own luma weights (fs_upscale.sc). Every brightness rule in
// the sky is written in THIS metric and not in Euclidean RGB, because the
// palette pass weights the channels and a difference that lives in blue is
// nearly invisible to it (Rule 36's pipeline-metric clause).
#define DFN_LUMA_WEIGHTS vec3(0.30, 0.59, 0.11)

// Cloud layer altitudes, meters ABOVE SEA LEVEL (world y). Look-dev pair for
// the two-sheet parallax: the sky intersects the view ray with these planes,
// the ground shadow projects along the sun to the SAME planes, so the sheet
// and its shadow line up by construction. Terrain tops out at ~400 m
// (WORLDGEN_MAX_HEIGHT), so both planes clear every landform.
//
// SIZED AGAINST THE VIEWING DISTANCE (Rule 33). These were 1200/2200 m, and at
// 1200 m the entire sky above 45 deg elevation mapped to a disc of 1.86
// wavelengths — 10.8 cells of AREA for the whole upper hemisphere. Mid-sky was
// therefore one or two cells and had nothing to draw at any threshold; it was
// a coin flip whether the frame caught a cloud overhead. The wavelength cannot
// move (it is NUMBERS' WIND_FIELD_WAVELENGTH, derived for the GROUND shadow's
// viewing distance and shared), so the SKY's distance to the field is what
// changes: 2600 m puts ~5 wavelengths of radius above 45 deg and ~9 above 30,
// which is a picture instead of a coin flip.
#define DFN_CLOUD_LAYER1_M 2600.0
#define DFN_CLOUD_LAYER2_M 4400.0
// Layer 2 samples the SAME field at a coarser scale and a fixed seed shift so
// the sheets decorrelate without inventing a second field or a second wind.
#define DFN_CLOUD_LAYER2_SCALE 0.47
#define DFN_CLOUD_LAYER2_SEED  vec2(310.0, -170.0)
// Softness of the coverage threshold, in UNIFORM field units (post-remap).
// Scale-free: after the remap the field's units are probability, so this is
// "the softest 10% of the distribution" at every cover value.
#define DFN_CLOUD_EDGE_U 0.10
// Mean and standard deviation of the raw octave sum, MEASURED over 400k
// samples (engine/render/sources/CloudModel.cpp is the reference side and
// tests/render/CloudModelTests.cpp asserts them). The remap below is this
// Gaussian's own CDF.
#define DFN_CLOUD_FIELD_MEAN 0.4980
#define DFN_CLOUD_FIELD_SD   0.1368

float dfn_cloud_hash(vec2 c)
{
    return fract(sin(dot(c, vec2(127.1, 311.7))) * 43758.5453);
}

float dfn_cloud_vnoise(vec2 p)
{
    vec2 c = floor(p);
    vec2 f = p - c;
    f = f * f * (3.0 - 2.0 * f);
    float a = dfn_cloud_hash(c);
    float b = dfn_cloud_hash(c + vec2(1.0, 0.0));
    float d = dfn_cloud_hash(c + vec2(0.0, 1.0));
    float e = dfn_cloud_hash(c + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(d, e, f.x), f.y);
}

// An octave whose cells have gone under about two pixels contributes nothing
// but aliasing. It is replaced by its MEAN, not scaled toward zero: scaling
// toward zero shrinks the field's spread and silently moves every coverage
// threshold with it — the Rule 31 defect re-introduced by the fix for it.
float dfn_cloud_octave_lod(float cells_px, float freq)
{
    return 1.0 - smoothstep(0.22, 0.75, cells_px * freq);
}

// THE coverage field (W4): one authority, UNIFORM on [0,1] by construction.
// `p` is world x/z ON A LAYER PLANE in meters, drift NOT yet applied — every
// sampler goes through the dfn_cloud_sheet*_alpha wrappers below, which add
// u_cloudOffset, so no call site can drift its own copy. `cells_px` is how
// much field one pixel covers here, in wavelengths, along the WORST screen
// axis (see dfn_cloud_cells_px).
//
// The octave sum is Gaussian, so thresholding it at 1-cover does NOT cover
// `cover`: measured, the sum occupied 0.045..0.945 with 98% of its mass in
// 0.200..0.797, cover 0.10 drew nothing at all and the shipped default 0.45
// drew 0.19 — the first shoot's empty sky, visible in the numbers before it
// was visible in a frame. Pushing the sum through its own CDF makes the field
// uniform, and then the threshold means what it says.
float dfn_cloud_field(vec2 p, float cells_px)
{
    vec2 q = p / max(u_cloudWavelength, 1.0);
    float l0 = dfn_cloud_octave_lod(cells_px, 1.00);
    float l1 = dfn_cloud_octave_lod(cells_px, 2.03);
    float l2 = dfn_cloud_octave_lod(cells_px, 4.07);
    float raw = 0.5
              + (dfn_cloud_vnoise(q) - 0.5) * 0.55 * l0
              + (dfn_cloud_vnoise(q * 2.03 + vec2(17.0, 31.0)) - 0.5) * 0.28 * l1
              + (dfn_cloud_vnoise(q * 4.07 + vec2(47.0, 89.0)) - 0.5) * 0.17 * l2;
    float z = (raw - DFN_CLOUD_FIELD_MEAN) / DFN_CLOUD_FIELD_SD;
    return 1.0 / (1.0 + exp(-1.702 * z)); // logistic ~= the normal CDF
}

// Coverage -> opacity. cover 0 = empty sky (alpha exactly 0 everywhere: the
// Rule 30 control — DFN_CLOUD=0 must erase sheet AND shadows in one move);
// cover 1 = solid, with no holes left by an edge hanging off the end of the
// range (the other of the two assertions a range is).
float dfn_cloud_alpha(vec2 p, float cover, float cells_px)
{
    if (cover <= 0.0) {
        return 0.0;
    }
    float u = dfn_cloud_field(p, cells_px);
    float edge = min(DFN_CLOUD_EDGE_U, min(cover, 1.0 - cover));
    float a = smoothstep(1.0 - cover - edge, 1.0 - cover + edge, u);
    // Past the resolution limit a point sample is noise; the honest value is
    // the area average, which for a uniform field thresholded at 1-cover is
    // `cover`. Converging there turns the far sheet into a haze veil instead
    // of a speckle band, and needs no distance cut-off to hide the aliasing —
    // the cut-off is what carved the hard shelf across the first shoot's sky.
    return mix(a, cover, smoothstep(0.20, 0.60, cells_px));
}

// How much field one pixel covers, in wavelengths, at a sampled layer point.
// Taken from the SCREEN DERIVATIVES of the point itself, which is anisotropic
// for free: tangentially a pixel spans dist/px_per_rad, but RADIALLY the plane
// runs away as dist/dir_y — near the horizon that is twenty times more field
// per pixel, and it is the radial axis, not the tangential one, that smeared
// the first shoot's horizon into streaks. The worst axis wins.
//
// Derived rather than computed from the projection: the analytic version read
// px_per_rad off u_proj/u_viewRect, which came back as garbage in the sky pass
// and drove the ENTIRE sky past the resolution limit — every pixel converged
// to the area mean and the sky went out as a flat 45% wash. Caught by the
// cover-0 control, which was clean blue beside it. FRAGMENT SHADERS ONLY.
#define DFN_CLOUD_CELLS_PX(p) \
    (max(length(dFdx(p)), length(dFdy(p))) / max(u_cloudWavelength, 1.0))

// The two sheets, as the ONLY two ways to read the field. Layer 1 is the main
// sheet (full cover weight); layer 2 is the high thin sheet (reduced cover).
float dfn_cloud_sheet_alpha(vec2 p_on_layer1, float cells_px)
{
    return dfn_cloud_alpha(p_on_layer1 + u_cloudOffset, u_cloudCover, cells_px);
}

float dfn_cloud_sheet2_alpha(vec2 p_on_layer2, float cells_px)
{
    return dfn_cloud_alpha((p_on_layer2 + u_cloudOffset)
                               * DFN_CLOUD_LAYER2_SCALE
                           + DFN_CLOUD_LAYER2_SEED,
                           u_cloudCover * 0.75,
                           cells_px * DFN_CLOUD_LAYER2_SCALE);
}

// Sun visibility through the cloud sheets at a WORLD point: project along the
// sun to each layer plane and read the same alphas the sky draws. Applied to
// the sun term of dfn_surface_light, so terrain, props and foliage darken
// together as the shadow crawls (the "мир живёт" frame). Fades out at low
// sun: near the horizon a crawling shadow degenerates into kilometers of
// smear, and dusk attenuation belongs to the state's sun_attenuation, not to
// this projection.
float dfn_cloud_sun_vis(vec3 wpos)
{
    if (u_cloudShadow <= 0.0 || u_cloudCover <= 0.0) {
        return 1.0;
    }
    float sun_y = u_sunDir.y;
    float low_sun = smoothstep(0.08, 0.20, sun_y);
    if (low_sun <= 0.0) {
        return 1.0;
    }
    vec2 p1 = wpos.xz
            + u_sunDir.xz * ((DFN_CLOUD_LAYER1_M - wpos.y) / sun_y);
    vec2 p2 = wpos.xz
            + u_sunDir.xz * ((DFN_CLOUD_LAYER2_M - wpos.y) / sun_y);
    // Fully resolved: the shadow is read at a GROUND point, where one pixel
    // covers metres, not the kilometres a grazing sky ray covers. The sheet's
    // LOD exists for the sky's perspective and would only blur the shadow.
    // The high sheet is thin: half occlusion weight.
    float transmit = (1.0 - dfn_cloud_sheet_alpha(p1, 0.0))
                   * (1.0 - 0.5 * dfn_cloud_sheet2_alpha(p2, 0.0));
    return 1.0 - u_cloudShadow * low_sun * (1.0 - transmit);
}

// Sway offset for a wind-affected vertex, in WORLD space.
//   sway_weight: 0 at the attachment (branch/ground), 1 at the free edge.
//   phase:       per-INSTANCE, so a stand ripples instead of pulsing as one.
// u_windStrength already contains the CPU-side gust envelope, which is what
// lets audio and gameplay read the same number the leaves are moving to; this
// function only adds per-instance and per-place variation on top of it.
vec3 dfn_wind_offset(vec3 wpos, float sway_weight, float phase)
{
    if (u_windStrength <= 0.0 || sway_weight <= 0.0) {
        return vec3(0.0, 0.0, 0.0);
    }
    float tau = 6.2831853;
    // Gusts TRAVEL along the wind direction: without this term every tree in
    // a stand peaks at the same instant and the forest breathes as one object.
    float travel = dot(wpos.xz, u_windDir) * 0.06;
    float sway = sin(u_envTime * 1.1 + phase * tau + travel);
    float flutter = sin(u_envTime * 4.3 + phase * tau * 2.0) * 0.35 * u_windFlutter;
    float amp = u_windStrength * sway_weight;
    vec2 horizontal = u_windDir * (amp * (sway + flutter) * 0.6);
    // A pushed card also DIPS. Pure horizontal translation reads as the card
    // sliding; the dip is what sells it as bending about its attachment.
    float dip = -abs(amp * sway) * 0.15;
    return vec3(horizontal.x, dip, horizontal.y);
}

// SCREEN-DOOR FADE (DrawParams::fade, arriving in u_params.y — passed as an
// argument because each fragment shader declares its own u_params).
//
// A LOD cross-fade draws the SAME GROUND at two levels for a moment. Alpha
// blending would need sorting and would double-darken; a dissolve costs one
// discard and needs neither. It also happens to be the only fade that survives
// this project's 64-colour post: a half-transparent surface would land between
// palette entries and be dithered anyway, so we do the dithering ourselves, at
// the internal-pixel grid, where it reads as texture rather than as mush.
//
// The 4x4 ordered matrix is computed rather than tabled (no dynamic array
// indexing on any backend): M4 = 4 * M2(high bits) + M2(low bits), with
// M2(x,y) = 2x + 3y - 4xy reproducing [[0,2],[3,1]] exactly.
float dfn_m2(float x, float y)
{
    return 2.0 * x + 3.0 * y - 4.0 * x * y;
}

float dfn_bayer4(vec2 p)
{
    vec2 q = mod(floor(p), 4.0);
    float xl = mod(q.x, 2.0);
    float yl = mod(q.y, 2.0);
    float xh = floor(q.x * 0.5);
    float yh = floor(q.y * 0.5);
    return (4.0 * dfn_m2(xh, yh) + dfn_m2(xl, yl)) / 16.0;
}

// Discards the fragment if this pixel is not part of the surviving pattern.
// fade >= 1 keeps everything (the maximum threshold is 15/16), fade <= 0 keeps
// nothing. The pixel coordinate is a PARAMETER, not gl_FragCoord read inside:
// this header is included by vertex shaders too (dfn_wind_offset), and
// gl_FragCoord does not exist there — naming it in the body fails the build of
// every vertex shader that includes this file.
void dfn_screen_door(float fade, vec2 pixel)
{
    if (fade < dfn_bayer4(pixel) + 0.03125) {
        discard;
    }
}

// Ground brightness of a FULL moon, as a fraction of moon_color. A full moon
// is ~400,000x dimmer than the sun; the art value that reads as "navigable
// night, not a second daylight" is this. Look-dev — the only such number in a
// shader, and it lives here because it pairs with the u_moonLight contract.
#define DFN_MOON_GROUND_MAX 0.30

// Surface lighting shared by terrain and props, so night, moonlight and the
// carried torch can never disagree between them.
//   sun_vis: sun shadow-map visibility (dfn_shadow_factor), 1 = unshadowed.
//   sky_vis: how much sky the surface sees, 0 = sealed interior, 1 = open.
//            Voxel meshes carry it in vertex ALPHA (core writes it at build
//            time); surfaces without the data pass 1.0.
vec3 dfn_surface_light(vec3 wpos, vec3 n, float sun_vis, float sky_vis)
{
    // Ambient is SKY light: an enclosed volume must not receive it, which is
    // what stops caves from reading as flatly daylit. Two independent terms
    // gate it — the GEOMETRIC one (sky_vis, from the voxel mesh) and the
    // AUTHORED one (u_ambientDarkness, from the darkness zone the player is
    // in). Geometry cannot express "this place is unnaturally dark", and
    // authoring should not have to describe a cave's shape.
    float dark = clamp(u_ambientDarkness, 0.0, 1.0);
    float sky = sky_vis * (1.0 - dark);
    vec3 light = u_ambientColor * sky;
    // Cloud shadow (W4): the same coverage field the sky draws, projected
    // along the sun. Lives HERE so every surface-lit thing — terrain, props,
    // foliage — darkens under the same crawling shadow (Rule 32).
    light += u_sunColor * (max(dot(n, u_sunDir), 0.0) * sun_vis
                           * dfn_cloud_sun_vis(wpos));
    // Moonlight: directional and unshadowed — the shadow map belongs to the
    // sun, and a second cascade for the moon is not worth the frame.
    light += u_moonColor * (u_moonLight * DFN_MOON_GROUND_MAX
                            * max(dot(n, u_moonDir), 0.0) * sky);
    // Point lights (torch, braziers, lit windows). Radius 0 = off. Smooth
    // quadratic falloff. Authored darkness SHORTENS them, which is what makes
    // a torch light "лишь мелкий клочок" in a black-void place instead of
    // simply making the room grey.
    float reach = 1.0 - 0.55 * dark;
    for (int i = 0; i < DFN_MAX_LIGHTS; ++i)
    {
        if (float(i) >= u_lightCount) {
            break;
        }
        vec4 pos_rad = u_lightPosRad(i);
        vec3 to_light = pos_rad.xyz - wpos;
        float dist = length(to_light);
        float atten = clamp(1.0 - dist / max(pos_rad.w * reach, 0.0001), 0.0, 1.0);
        // Shadow-casting lights are packed FIRST (BgfxRenderer orders them), so
        // the slot index IS the cube-atlas index and no second lookup table
        // exists to fall out of sync.
        float occl = 1.0;
        if (float(i) < u_pointShadowParams.x && atten > 0.0) {
            occl = dfn_point_shadow_factor(i, wpos, n, pos_rad.xyz, pos_rad.w);
        }
        light += u_lightColor(i).rgb * (atten * atten * occl
                    * max(dot(n, to_light / max(dist, 0.0001)), 0.0));
    }
    return light;
}

// Distance fog shared by terrain and water: world-space distance from the eye.
float dfn_fog_factor(vec3 wpos)
{
    vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    return smoothstep(u_fogStart, u_fogEnd, length(wpos - eye));
}

#endif // DFN_ENV_SH
