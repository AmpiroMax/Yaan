/*
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
// x — ближний exclude, метры: глубина ближе него читается как СОБСТВЕННАЯ
// ОСНАСТКА источника (борт лотка, дрова) и не заслоняет. Свой юниформ, а не
// компонента u_envParams: массив окружения объявляется ПОЗЖЕ этого включения,
// и ссылка на него отсюда не компилируется (поймано выпечкой шейдеров).
uniform vec4 u_psNear;

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
    // DIAGNOSTIC DOSES (u_pointShadowParams.w = DFN_PS_DEBUG, ships as 0.0 and
    // every branch below is then dead, so the zero-dose arm is the shipping
    // shader bit for bit). They split the "factor is 0 with clear air" defect
    // into its only three hiding places without a GPU debugger:
    //   3 = does uv land inside ITS OWN atlas tile? (1 = yes). The 4-column,
    //       3-row atlas shape is the same two-file contract as the rows above.
    //   2 = return the COMPARE value dist/radius (positive control: must show
    //       a radial gradient, or the debug channel itself is broken).
    //   1 = return the SAMPLED atlas value: ~1.0 everywhere means the tiles
    //       are empty-and-clear and the compare is at fault; ~0.0 means the
    //       tiles hold near-zero distances, i.e. the WRITER side is at fault.
    if (u_pointShadowParams.w > 2.5) {
        float tf = float(index * 6 + face);
        float rowf = floor((tf + 0.5) / 4.0);
        float colf = tf - 4.0 * rowf;
        vec2 lo = vec2(colf / 4.0, rowf / 3.0);
        vec2 hi = vec2((colf + 1.0) / 4.0, (rowf + 1.0) / 3.0);
        return (uv.x >= lo.x && uv.x <= hi.x && uv.y >= lo.y && uv.y <= hi.y)
                   ? 1.0 : 0.0;
    }
    if (u_pointShadowParams.w > 1.5) {
        return clamp(dist / radius, 0.0, 1.0);
    }
    if (u_pointShadowParams.w > 0.5) {
        return stored;
    }
    // Nothing was rendered into this texel -> cleared to 1.0 -> lit.
    float bias = u_pointShadowParams.z;
    // БЛИЖНИЙ ОККЛЮДЕР — СОБСТВЕННАЯ ОСНАСТКА ИСТОЧНИКА (23.08, круг 4 [N4]:
    // зал замка в полдень 19.3/255 при очаге радиусом 13; арма casts_shadow=0
    // дала +34% — свет душила кубовая тень СВОЕГО лотка, чьи борта пишут
    // глубину в сантиметрах от пламени; та же семья, что давний «пол под
    // ногами абсолютно чёрный» у факела). Глубина ближе u_psNearExclude
    // метров читается как своя оснастка и не заслоняет: борт лотка и дрова
    // не тень, а сам очаг. Мебель в метре и дальше тенит как раньше.
    // DFN_PS_NEAR_EXCLUDE, 0 = прежнее сравнение бит-в-бит.
    if (stored * radius < u_psNear.x) {
        return 1.0;
    }
    return (dist / radius) - bias <= stored ? 1.0 : 0.0;
}

#endif // DFN_POINTSHADOW_SH
