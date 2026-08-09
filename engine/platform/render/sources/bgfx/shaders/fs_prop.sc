$input v_color0, v_normal, v_texcoord0, v_wpos

// Prop fragment (stage 3b): flat vertex-color albedo for placeholder meshes
// (scatter flora/stones, site structures) — faceted normals give the
// hard-edged low-poly read (LANDSCAPE §5/§6). Same lighting model and fog as
// terrain so props sit in the scene instead of floating on it. Shares
// vs_terrain (PROGRAM_TABLE pairs "prop" = vs_terrain + fs_prop).

#include <bgfx_shader.sh>
#include "dfn_env.sh"

void main()
{
    vec3 n = normalize(v_normal);
    float ndotl = max(dot(n, u_sunDir), 0.0);
    vec3 lit = v_color0.rgb * (u_ambientColor + u_sunColor * ndotl);
    float fog = dfn_fog_factor(v_wpos);
    gl_FragColor = vec4(mix(lit, u_fogColor, fog), 1.0);
}
