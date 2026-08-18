$input v_color0, v_normal, v_texcoord0, v_wpos

// Prop fragment (stage 3b): flat vertex-color albedo for placeholder meshes
// (scatter flora/stones, site structures) — faceted normals give the
// hard-edged low-poly read (LANDSCAPE §5/§6). Same lighting model, sun
// shadows (в1, dfn_shadow.sh) and fog as terrain so props sit in the scene
// instead of floating on it. Shares vs_terrain (PROGRAM_TABLE pairs
// "prop" = vs_terrain + fs_prop).

#include <bgfx_shader.sh>
#include "dfn_env.sh"
#include "dfn_shadow.sh"

SAMPLER2D(s_texColor, 0);

// x: texture bound, y: DrawParams::fade, z: highlight.
uniform vec4 u_params;

void main()
{
    dfn_screen_door(u_params.y, gl_FragCoord.xy); // LOD cross-fade / per-draw dissolve
    vec3 n = normalize(v_normal);
    float vis = dfn_shadow_factor(v_wpos, n);
    // АЛЬБЕДО: ПЛИТКА, ЕСЛИ ОНА ПРИВЯЗАНА, иначе плоский цвет вершины.
    // u_params.x бэкенд выставлял ВСЕГДА, читал его здесь никто — постройка
    // редактора (19.08) первая, кому плитка нужна: её uv считаны В МЕТРАХ, и
    // fract() повторяет узор столько раз, сколько метров у стены. Цвет вершины
    // остаётся МНОЖИТЕЛЕМ: у текстурируемых потоков он белый, а тонированные
    // меши без плитки не меняются ни на бит.
    vec3 albedo = v_color0.rgb;
    if (u_params.x > 0.5) {
        albedo *= texture2D(s_texColor, fract(v_texcoord0)).rgb;
    }
    // Vertex alpha is the sky-visibility channel (1.0 on everything built
    // above ground); dfn_surface_light adds moon and torch on top of sun.
    vec3 lit = albedo * dfn_surface_light(v_wpos, n, vis, v_color0.a);
    gl_FragColor = vec4(dfn_aerial(v_wpos, lit), 1.0);
}
