/*
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_shadow.sh

Responsibility:
- Shared shader include: sun shadow-map sampling for lit fragment shaders
  (terrain, prop). 3x3 PCF soft edge (owner decision 22.08.2026, overturning
  в1's hard single tap; DFN_SHADOW_SOFT=0 restores в1 bit-for-bit),
  normal-offset receiver + depth bias against acne.

Key items:
- s_shadowMap (stage 1, compare sampler), u_lightMtx, u_shadowParams,
  s_shadowMapNear (stage 3) + u_lightMtxNear (the near cascade),
  u_shadowSoft (PCF spread + uv-per-texel), dfn_shadow_factor().

Dependencies:
- Uses: nothing (included after bgfx_shader.sh).
- Used by: fs_terrain, fs_prop.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- u_lightMtx / u_shadowParams packing is a contract with
  BgfxRenderer.cpp::update_shadow — change both together or not at all.
*/

#ifndef DFN_SHADOW_SH
#define DFN_SHADOW_SH

SAMPLER2DSHADOW(s_shadowMap, 1);

// World -> shadow-map uv/depth (crop * ortho-proj * light-view, CPU-side).
uniform mat4 u_lightMtx;
// x: DOSE (0 = no sun shadow at all, 1 = shipped), y: normal-offset in meters
// (far map), z: comparison depth bias (normalized depth units), w:
// normal-offset in meters for the NEAR cascade (its own number — a far-texel
// push-off would erode eight near texels of every hole).
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

// THE SOFT EDGE (owner decision 22.08.2026). x: PCF tap spacing in TEXELS of
// the map being sampled — 0 disables the kernel and the single-tap path below
// runs bit-identically (the в1 control arm, DFN_SHADOW_SOFT=0). y: uv per
// texel of the far map (1/SHADOW_MAP_SIZE), z: uv per texel of the near map
// (1/SHADOW_NEAR_MAP_SIZE), w: unused. Spread rides with its own uv factors so
// the near cascade is softened in ITS texels and keeps the grain R6b bought.
uniform vec4 u_shadowSoft;

// Nine hardware-compared taps in a (2 x spread) texel box. Each tap is
// bilinear-compare already, so the sum is a smooth ramp; the loop bounds are
// compile-time constants and unroll.
#define DFN_SHADOW_PCF9(map, uv, depth, step_uv, out_sum)                     \
    {                                                                         \
        out_sum = 0.0;                                                        \
        for (int _dy = -1; _dy <= 1; ++_dy) {                                 \
            for (int _dx = -1; _dx <= 1; ++_dx) {                             \
                out_sum += shadow2D(map,                                      \
                    vec3(uv + vec2(float(_dx), float(_dy)) * step_uv, depth));\
            }                                                                 \
        }                                                                     \
        out_sum *= (1.0 / 9.0);                                               \
    }

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
        float sn;
        if (u_shadowSoft.x > 0.0) {
            // Taps reach at most spread x z uv past the rim; the 0.002 uv
            // margin above is ~8 near texels, so no tap leaves the map.
            DFN_SHADOW_PCF9(s_shadowMapNear, uvn, scn.z - u_shadowParams.z,
                            u_shadowSoft.x * u_shadowSoft.z, sn);
        } else {
            sn = shadow2D(s_shadowMapNear, vec3(uvn, scn.z - u_shadowParams.z));
        }
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
    // At dose 1 and spread 0 this is exactly shadow2D(...) — the frame the two
    // knobs' zero arms restore is the shipped pre-soft, pre-dose frame.
    float s;
    if (u_shadowSoft.x > 0.0) {
        DFN_SHADOW_PCF9(s_shadowMap, uv, sc.z - u_shadowParams.z,
                        u_shadowSoft.x * u_shadowSoft.y, s);
    } else {
        s = shadow2D(s_shadowMap, vec3(uv, sc.z - u_shadowParams.z));
    }
    return mix(1.0, s, min(u_shadowParams.x, 1.0));
}

#endif // DFN_SHADOW_SH
