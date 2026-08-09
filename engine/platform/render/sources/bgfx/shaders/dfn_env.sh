/*
Created: 09:08:2026 - 10:52:00
Last updated: 09:08:2026 - 20:40:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_env.sh

Responsibility:
- Shared shader include: the frame environment uniform block (RenderEnvironment
  -> u_envParams vec4 array) with named accessor macros. Single source of the
  index layout; BgfxRenderer.cpp packs the array in exactly this order.

Key items:
- u_envParams[32]; accessor #defines (sun, ambient, fog, sky, splat, water,
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
- 09:08:2026 - 20:40:00: Light ARRAY (up to 8) replaces the single point
  light, plus authored u_ambientDarkness; env block 15 -> 32 vec4s.
*/

#ifndef DFN_ENV_SH
#define DFN_ENV_SH

uniform vec4 u_envParams[32];

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
    light += u_sunColor * (max(dot(n, u_sunDir), 0.0) * sun_vis);
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
        light += u_lightColor(i).rgb * (atten * atten
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
