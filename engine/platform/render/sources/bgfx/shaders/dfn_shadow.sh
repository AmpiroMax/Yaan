/*
Created: 09:08:2026 - 14:11:37
Last updated: 12:08:2026 - 23:08:22
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_shadow.sh

Responsibility:
- Shared shader include: sun shadow-map sampling for lit fragment shaders
  (terrain, prop). One hard-compared tap (user decision в1: hard pixel edges
  fit the art style; PCF off), normal-offset receiver + depth bias against
  acne at the coarse texel size.

Key items:
- s_shadowMap (stage 1, compare sampler), u_lightMtx, u_shadowParams,
  dfn_shadow_factor().

Dependencies:
- Uses: nothing (included after bgfx_shader.sh).
- Used by: fs_terrain, fs_prop.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- u_lightMtx / u_shadowParams packing is a contract with
  BgfxRenderer.cpp::update_shadow — change both together or not at all.
*/
/*
UPD:
- 09:08:2026 - 14:11:37: Dynamic sun shadows (feature-requests batch item 3).
- 12:08:2026 - 23:08:22: u_shadowParams.x IS NOW A DOSE, not a flag, so that
  DFN_SUN_SHADOW=0 is a true zero-dose control arm (Rule 48). At dose 1 the
  result is bit-identical to the flag version — mix(1.0, s, 1.0) == s — and
  the caster pass still runs at dose 0, so the ONLY thing that changes between
  the arms is the shadow term itself and everything else subtracts away (Rule
  47's structural cure). Two claims need exactly this arm and neither could be
  settled without it: R6b (the dapple is five times short of the reference, and
  its absolute number is unusable without a shadow-off control) and the user's
  two standing complaints about shadows, which live BETWEEN frames.
*/

#ifndef DFN_SHADOW_SH
#define DFN_SHADOW_SH

SAMPLER2DSHADOW(s_shadowMap, 1);

// World -> shadow-map uv/depth (crop * ortho-proj * light-view, CPU-side).
uniform mat4 u_lightMtx;
// x: DOSE (0 = no sun shadow at all, 1 = shipped), y: normal-offset in meters
// (~1.5 shadow texels), z: comparison depth bias (normalized depth units),
// w: unused.
//
// x IS A DOSE AND NOT A FLAG, and the reason is Rule 48. Every claim about the
// shadow — how much dapple the canopy lays down, whether the user's black
// stripe is the shadow map at all — needs an arm with the shadow OFF and
// EVERYTHING ELSE IDENTICAL, or its absolute number is measuring the ground
// texture and the terrain shading as much as the shadow. `DFN_SUN_SHADOW=0`
// gives that arm. It also keeps 0 an honest zero rather than "roughly no
// shadow": the sun is still up, the casters are still drawn into the map, the
// map is simply not read.
uniform vec4 u_shadowParams;

// 1.0 = fully sunlit, 0.0 = in shadow. `n` is the unit surface normal.
float dfn_shadow_factor(vec3 wpos, vec3 n)
{
    if (u_shadowParams.x <= 0.0) {
        return 1.0;
    }
    vec4 sc = mul(u_lightMtx, vec4(wpos + n * u_shadowParams.y, 1.0));
    // Orthographic light: w == 1, no perspective divide needed.
    vec2 uv = sc.xy;
    // Outside the shadow volume -> lit (the map covers the loaded chunk ring;
    // beyond it fog dominates anyway).
    vec2 border = max(abs(uv - vec2(0.5, 0.5)) - vec2(0.5, 0.5), vec2(0.0, 0.0));
    if (max(border.x, border.y) > 0.0) {
        return 1.0;
    }
    // At dose 1 this is exactly shadow2D(...) — mix(1.0, s, 1.0) == s — so the
    // shipped frame is bit-identical to the flag version this replaced.
    float s = shadow2D(s_shadowMap, vec3(uv, sc.z - u_shadowParams.z));
    return mix(1.0, s, min(u_shadowParams.x, 1.0));
}

#endif // DFN_SHADOW_SH
