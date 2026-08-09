/*
Created: 09:08:2026 - 14:11:37
Last updated: 09:08:2026 - 14:11:37
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
*/

#ifndef DFN_SHADOW_SH
#define DFN_SHADOW_SH

SAMPLER2DSHADOW(s_shadowMap, 1);

// World -> shadow-map uv/depth (crop * ortho-proj * light-view, CPU-side).
uniform mat4 u_lightMtx;
// x: enabled (0/1), y: normal-offset in meters (~1.5 shadow texels),
// z: comparison depth bias (normalized depth units), w: unused.
uniform vec4 u_shadowParams;

// 1.0 = fully sunlit, 0.0 = in shadow. `n` is the unit surface normal.
float dfn_shadow_factor(vec3 wpos, vec3 n)
{
    if (u_shadowParams.x < 0.5) {
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
    return shadow2D(s_shadowMap, vec3(uv, sc.z - u_shadowParams.z));
}

#endif // DFN_SHADOW_SH
