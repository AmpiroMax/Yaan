/*
Created: 09:08:2026 - 20:28:29
Last updated: 09:08:2026 - 20:28:29
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_pointshadow.sh

Responsibility:
- Shared shader include: shadow lookup for CARRIED point lights (the torch).
  A point light shadows in every direction, so its map is a CUBE — stored here
  as six 90-degree faces packed into ONE 2D atlas, which keeps it to a single
  sampler stage and lets the face lookup be plain arithmetic instead of a
  cube-map sampling convention we would have to guess at.

Key items:
- s_pointShadow (stage 2), u_pointShadowRows[48] (12 faces x 4 matrix ROWS),
  u_pointShadowParams, dfn_point_shadow_factor().

Dependencies:
- Uses: nothing (included after bgfx_shader.sh, from dfn_env.sh).
- Used by: dfn_env.sh::dfn_surface_light — i.e. terrain, props and foliage.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The FACE ORDER (+X, -X, +Y, -Y, +Z, -Z) and the row-major matrix packing are
  a contract with BgfxRenderer.cpp::update_point_shadows — change both together
  or not at all. Face selection here is the major axis of the direction to the
  fragment, which is exactly the region each 90-degree face covers, so the two
  sides agree without either of them knowing a uv convention.
- The atlas stores LINEAR DISTANCE / radius, not projected depth: the compare
  is then a world-space metre comparison that needs no depth linearization and
  no per-face far-plane bookkeeping in the shader.
*/
/*
UPD:
- 09:08:2026 - 20:28:29: Created for interior lighting: cube shadows for the
  carried torch (up to MAX_SHADOW_POINT_LIGHTS lights).
*/

#ifndef DFN_POINTSHADOW_SH
#define DFN_POINTSHADOW_SH

SAMPLER2D(s_pointShadow, 2);

// 12 face matrices (2 lights x 6 faces), each stored as 4 ROWS so the shader
// can index dynamically without building a mat4 from a computed offset.
// Row r of face f lives at [f * 4 + r]. Already includes the atlas tile
// scale/offset, so xy/w lands directly in atlas uv.
uniform vec4 u_pointShadowRows[48];
// x: number of shadow-casting lights (they are packed FIRST in the light
// array), y: receiver normal offset in metres, z: comparison bias as a
// fraction of the light radius, w: unused.
uniform vec4 u_pointShadowParams;

// 1.0 = the light reaches this fragment, 0.0 = occluded.
//   index:  light slot (must be < u_pointShadowParams.x)
//   lpos:   light position, radius: the light's radius (the face far plane)
float dfn_point_shadow_factor(int index, vec3 wpos, vec3 n, vec3 lpos, float radius)
{
    vec3 to_frag = wpos - lpos;
    float dist = length(to_frag);
    if (dist <= 0.0001 || radius <= 0.0001) {
        return 1.0;
    }
    // Face = major axis of the direction. Each face is a 90-degree frustum, so
    // this is exactly the face that rasterized this fragment — no cube-map
    // convention involved on either side.
    vec3 ad = abs(to_frag);
    int face;
    if (ad.x >= ad.y && ad.x >= ad.z) {
        face = to_frag.x > 0.0 ? 0 : 1;
    } else if (ad.y >= ad.z) {
        face = to_frag.y > 0.0 ? 2 : 3;
    } else {
        face = to_frag.z > 0.0 ? 4 : 5;
    }

    // Receiver push-off along the normal: same trick as the sun map, and it
    // matters more here because the light sits ~0.35 m from the carrier and
    // grazing angles on a tunnel wall are the normal case, not the exception.
    vec4 p = vec4(wpos + n * u_pointShadowParams.y, 1.0);
    int base = (index * 6 + face) * 4;
    float px = dot(u_pointShadowRows[base + 0], p);
    float py = dot(u_pointShadowRows[base + 1], p);
    float pw = dot(u_pointShadowRows[base + 3], p);
    if (pw <= 0.0) {
        return 1.0;
    }
    vec2 uv = vec2(px, py) / pw;
    float stored = texture2D(s_pointShadow, uv).x;
    // Nothing was rendered into this texel -> cleared to 1.0 -> lit.
    float bias = u_pointShadowParams.z;
    return (dist / radius) - bias <= stored ? 1.0 : 0.0;
}

#endif // DFN_POINTSHADOW_SH
