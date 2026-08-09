$input v_color0, v_normal, v_texcoord0, v_wpos

// Terrain fragment shader v2 (stage 3): slope/height splat over the procedural
// 2x2 atlas (grass|rock / sand|dirt — layout contract with ProcTexture.h),
// directional lambert + ambient, distance fog into the sky horizon color.
// All thresholds are environment uniforms (dfn_env.sh) — tunable without
// recompiling shaders. u_params.x > 0.5 -> atlas bound; else vertex-tint
// fallback (stage-2 look, used when no texture is resident).

#include <bgfx_shader.sh>
#include "dfn_env.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_params;

vec3 atlas_sample(vec2 tiled_uv, vec2 cell)
{
    // Each atlas cell tiles independently: wrap inside the cell, then map the
    // fractional uv into the cell's quarter of the atlas.
    return texture2D(s_texColor, (cell + fract(tiled_uv)) * 0.5).rgb;
}

void main()
{
    vec3 n = normalize(v_normal);
    vec2 tuv = v_texcoord0 * u_terrainTiles;

    vec3 grass = atlas_sample(tuv, vec2(0.0, 0.0));
    vec3 rock  = atlas_sample(tuv, vec2(1.0, 0.0));
    vec3 sand  = atlas_sample(tuv, vec2(0.0, 1.0));
    vec3 dirt  = atlas_sample(tuv, vec2(1.0, 1.0));

    float slope = 1.0 - n.y;
    float rock_w = smoothstep(u_rockSlopeStart, u_rockSlopeEnd, slope);
    float sand_w = 1.0 - smoothstep(u_sandHeight, u_sandHeight + u_sandBlend, v_wpos.y);
    float dry = v_color0.a; // mesher noise: grass <-> dirt mottling

    vec3 ground = mix(mix(grass, dirt, dry), rock, rock_w);
    vec3 albedo = mix(ground, sand, sand_w);
    // Large-scale tint variation from the mesher breaks tiling repetition.
    albedo *= mix(vec3_splat(1.0), v_color0.rgb * 2.0, 0.35);
    // Untextured fallback: the stage-2 vertex tint look.
    albedo = mix(v_color0.rgb, albedo, step(0.5, u_params.x));

    float ndotl = max(dot(n, u_sunDir), 0.0);
    vec3 lit = albedo * (u_ambientColor + u_sunColor * ndotl);

    float fog = dfn_fog_factor(v_wpos);
    gl_FragColor = vec4(mix(lit, u_fogColor, fog), 1.0);
}
