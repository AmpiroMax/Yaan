/*
Created: 09:08:2026 - 14:11:37
Last updated: 13:08:2026 - 16:10:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_shadow.sh

Responsibility:
- Shared shader include: sun shadow-map sampling for lit fragment shaders
  (terrain, prop). One hard-compared tap (user decision в1: hard pixel edges
  fit the art style; PCF off), normal-offset receiver + depth bias against
  acne at the coarse texel size.

Key items:
- s_shadowMap (stage 1, compare sampler), u_lightMtx, u_shadowParams,
  s_shadowMapNear (stage 3) + u_lightMtxNear (the near cascade),
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
- 13:08:2026 - 16:10:00: THE NEAR CASCADE (s_shadowMapNear / u_lightMtxNear /
  u_shadowParams.w). R6b's dose arms proved the defect is BANDWIDTH and not
  amount: the far map's 0.156 m texel is 2-3x coarser than the leaf mask's own
  texel, so the canopy reaches the ground through a 0.31 m low-pass and only
  the blob survives — which is why our shadow's contribution RISES with block
  size (+0.034 at 8 px, +0.402 at 40 px) while reference 03's dapple FALLS.
  The near map is 0.0195 m and is consulted FIRST, never blended with the far
  one: blending would put the 0.31 m low-pass straight back on top of it.
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

// THE NEAR CASCADE (R6b — the dapple's GRAIN, not its amount). Same light, same
// depth bracket, 8x the texel density over a 40 m box around the eye.
SAMPLER2DSHADOW(s_shadowMapNear, 3);
uniform mat4 u_lightMtxNear;

// 1.0 = fully sunlit, 0.0 = in shadow. `n` is the unit surface normal.
float dfn_shadow_factor(vec3 wpos, vec3 n)
{
    if (u_shadowParams.x <= 0.0) {
        return 1.0;
    }
    // THE NEAR CASCADE FIRST, and the ORDER is the contract: wherever the fine
    // map covers the fragment it is strictly better information about the same
    // casters, so the coarse map is not consulted at all rather than blended
    // in. Blending the two would put a 0.156 m-cutoff low-pass BACK on top of
    // the 0.0195 m one over the whole near ground, i.e. it would average away
    // exactly the grain this cascade exists to deliver.
    //
    // The margin is why there is no seam. The test rejects the outer texel-ish
    // rim of the near map, so a fragment only takes the near answer when its
    // whole neighbourhood is inside; the handover happens at 40 m, where the
    // two maps disagree about EDGE SHARPNESS and never about presence — both
    // are fed by the same caster list and the same depth bracket.
    vec4 scn = mul(u_lightMtxNear, vec4(wpos + n * u_shadowParams.w, 1.0));
    vec2 uvn = scn.xy;
    vec2 bordern = max(abs(uvn - vec2(0.5, 0.5)) - vec2(0.498, 0.498),
                       vec2(0.0, 0.0));
    if (max(bordern.x, bordern.y) <= 0.0) {
        float sn = shadow2D(s_shadowMapNear, vec3(uvn, scn.z - u_shadowParams.z));
        return mix(1.0, sn, min(u_shadowParams.x, 1.0));
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
