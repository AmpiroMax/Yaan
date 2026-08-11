$input v_color0, v_normal, v_texcoord0, v_wpos

// Terrain fragment v4: surface-truth splat over the procedural 2x2 atlas
// (grass|rock / sand|dirt — ProcTexture.h layout contract). Vertex color
// carries splat weights baked by TerrainMesher from core's SurfaceFieldView:
//   r = sand (shore mask), g = rock, b = water bed, a = reserved.
// Design ruling (feature-requests batch): material bands come from core's
// surface_class ONLY — the legacy height-based sand band and the render-side
// dirt "dryness" mottling are gone (they painted 60 m brown washes core never
// classified). In-shader slope rock still augments the class weight with the
// SAME design thresholds (§4 rules 2-3, visual == gameplay truth), giving the
// GrassRockBlend band its ordered grass<->rock dither. Transitions are
// ordered-dithered in internal-pixel space (§4: dither, not gradients).
// Dynamic sun shadows (в1): one hard tap, dfn_shadow.sh.
// u_params.x < 0.5 -> flat-color fallback (no atlas resident).

#include <bgfx_shader.sh>
#include "dfn_env.sh"
#include "dfn_shadow.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_params;

vec3 atlas_sample(vec2 tiled_uv, vec2 cell)
{
    // Each atlas cell tiles independently: wrap inside the cell, then map the
    // fractional uv into the cell's quarter of the atlas.
    return texture2D(s_texColor, (cell + fract(tiled_uv)) * 0.5).rgb;
}

float bayer2(vec2 p) // [[0,2],[3,1]] as arithmetic: 2x + 3y - 4xy
{
    return 2.0 * p.x + 3.0 * p.y - 4.0 * p.x * p.y;
}

void main()
{
    // LOD cross-fade: the outgoing level dissolves as the incoming one
    // appears, both drawn, neither blended (dfn_env.sh).
    dfn_screen_door(u_params.y, gl_FragCoord.xy);
    vec3 n = normalize(v_normal);
    vec2 tuv = v_texcoord0 * u_terrainTiles;

    vec3 grass = atlas_sample(tuv, vec2(0.0, 0.0));
    vec3 rock  = atlas_sample(tuv, vec2(1.0, 0.0));
    vec3 sand  = atlas_sample(tuv, vec2(0.0, 1.0));
    vec3 dirt  = atlas_sample(tuv, vec2(1.0, 1.0));

    float slope = 1.0 - n.y;
    float rock_w = max(v_color0.g,
                       smoothstep(u_rockSlopeStart, u_rockSlopeEnd, slope));
    float sand_w = v_color0.r; // shore mask from core's surface_class only
    float bed_w = v_color0.b;

    // Ordered 4x4 Bayer threshold; the scene view renders at INTERNAL_RES, so
    // gl_FragCoord is already in internal pixels (blocks stay square).
    vec2 ip = floor(gl_FragCoord.xy);
    vec2 f1 = mod(ip, 2.0);
    vec2 f2 = mod(floor(ip * 0.5), 2.0);
    float bayer = (bayer2(f1) * 4.0 + bayer2(f2) + 0.5) / 16.0; // (0,1)

    // §4 priority via paint order (later mix wins): grass -> bed -> rock ->
    // sand on top. The blend band is a two-material dither: grass and rock
    // texels only, never a third color (§4 rule 3).
    vec3 albedo = grass;
    albedo = mix(albedo, dirt * 0.68, step(bayer, bed_w)); // dark wet bed
    albedo = mix(albedo, rock, step(bayer, rock_w));
    albedo = mix(albedo, sand, step(bayer, sand_w));

    // Untextured fallback: flat splat palette (headless / atlas not resident).
    vec3 flat_albedo = vec3(0.33, 0.43, 0.22);
    flat_albedo = mix(flat_albedo, vec3(0.42, 0.40, 0.38), step(bayer, rock_w));
    flat_albedo = mix(flat_albedo, vec3(0.72, 0.65, 0.44), step(bayer, sand_w));
    albedo = mix(flat_albedo, albedo, step(0.5, u_params.x));

    float vis = dfn_shadow_factor(v_wpos, n);
    // Vertex alpha carries sky visibility on voxel meshes (0 = sealed cave),
    // and is the reserved 1.0 on heightfield terrain — so this is a no-op
    // above ground and the interior falloff the moment core writes it.
    vec3 lit = albedo * dfn_surface_light(v_wpos, n, vis, v_color0.a);

    gl_FragColor = vec4(dfn_aerial(v_wpos, lit), 1.0);
}
