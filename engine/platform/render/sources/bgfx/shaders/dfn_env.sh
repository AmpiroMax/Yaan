/*
Created: 09:08:2026 - 10:52:00
Last updated: 09:08:2026 - 10:52:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_env.sh

Responsibility:
- Shared shader include: the frame environment uniform block (RenderEnvironment
  -> u_envParams vec4 array) with named accessor macros. Single source of the
  index layout; BgfxRenderer.cpp packs the array in exactly this order.

Key items:
- u_envParams[11]; accessor #defines (sun, ambient, fog, sky, splat, water).

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
*/

#ifndef DFN_ENV_SH
#define DFN_ENV_SH

uniform vec4 u_envParams[11];

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

// Distance fog shared by terrain and water: world-space distance from the eye.
float dfn_fog_factor(vec3 wpos)
{
    vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    return smoothstep(u_fogStart, u_fogEnd, length(wpos - eye));
}

#endif // DFN_ENV_SH
