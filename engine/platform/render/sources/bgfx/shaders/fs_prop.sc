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
SAMPLER2D(s_texAux, 4); // DrawParams::aux_texture — лист нормалей материала

// x: texture bound, y: DrawParams::fade, z: highlight, w: aux sheet bound.
uniform vec4 u_params;

void main()
{
    dfn_screen_door(u_params.y, gl_FragCoord.xy); // LOD cross-fade / per-draw dissolve
    vec3 n = normalize(v_normal);
    // --- РЕЛЬЕФ МАТЕРИАЛА (u_params.w = привязан лист нормалей). Заказ 19.08:
    // «у стен и столбов текстуры объём, как у собранных деталей» — детали носят
    // рельеф через ЭТОТ ЖЕ механизм в fs_foliage (тангенс из экранных
    // производных, Грам-Шмидт, вырожденный градиент падает в геометрическую
    // нормаль). Скопировано оттуда сознательно и с тем же устройством: у
    // шейдеров нет общего include под это, а два РАЗНЫХ способа считать базис
    // дали бы стенам рельеф, наклонённый иначе, чем у деталей рядом.
    if (u_params.w > 0.5) {
        vec3 dp1 = dFdx(v_wpos);
        vec3 dp2 = dFdy(v_wpos);
        vec2 du1 = dFdx(v_texcoord0);
        vec2 du2 = dFdy(v_texcoord0);
        float det = du1.x * du2.y - du2.x * du1.y;
        if (abs(det) > 1e-12) {
            vec3 tangent = normalize((dp1 * du2.y - dp2 * du1.y) / det);
            tangent = normalize(tangent - n * dot(n, tangent));
            vec3 bitangent = cross(n, tangent);
            vec3 tn = texture2D(s_texAux, fract(v_texcoord0)).xyz * 2.0 - 1.0;
            n = normalize(tangent * tn.x + bitangent * tn.y + n * tn.z);
        }
    }
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
        // МАКРО-ВАРИАЦИЯ ПРОТИВ РЯБИ (заказ 20.08: «текстуры везде одинаковые,
        // повторяются — видно, что объект искусственный»). Плитка метровая, и
        // на большой стене её период читается глазом; два слоя ценностного
        // шума по МИРОВЫМ координатам (периоды ~7 м и ~2.3 м, амплитуда ±12%)
        // рассинхронизируют повторы. По миру, а не по uv — сосед-кусок кладки
        // получает своё пятно, и стык не палит сетку.
        vec2 wp = v_wpos.xz + v_wpos.yy * 0.37;
        vec2 g1 = floor(wp * 0.14);
        vec2 f1 = fract(wp * 0.14);
        f1 = f1 * f1 * (3.0 - 2.0 * f1);
        #define DFN_H(q) fract(sin(dot(q, vec2(127.1, 311.7))) * 43758.5453)
        float n1 = mix(mix(DFN_H(g1), DFN_H(g1 + vec2(1.0, 0.0)), f1.x),
                       mix(DFN_H(g1 + vec2(0.0, 1.0)), DFN_H(g1 + vec2(1.0, 1.0)), f1.x),
                       f1.y);
        vec2 g2 = floor(wp * 0.43);
        vec2 f2 = fract(wp * 0.43);
        f2 = f2 * f2 * (3.0 - 2.0 * f2);
        float n2 = mix(mix(DFN_H(g2), DFN_H(g2 + vec2(1.0, 0.0)), f2.x),
                       mix(DFN_H(g2 + vec2(0.0, 1.0)), DFN_H(g2 + vec2(1.0, 1.0)), f2.x),
                       f2.y);
        albedo *= 0.88 + 0.16 * n1 + 0.08 * n2;
    }
    // САМОСВЕТНЫЙ ДРО (u_params.z < -0.5, UPD 23:08:2026 - 18:10:31): альбедо без
    // освещения и без теней — пламя и стекло фонаря видны с любого
    // расстояния и с любой грани, независимо от бюджета точечных светов.
    // Воздух поверх остаётся: дальний огонь тает в дымке, как всё в мире;
    // скотопик яркое не серит.
    if (u_params.z < -0.5) {
        gl_FragColor = vec4(dfn_aerial(v_wpos, albedo), 1.0);
        return;
    }
    // Vertex alpha is the sky-visibility channel (1.0 on everything built
    // above ground); dfn_surface_light adds moon and torch on top of sun.
    vec3 lit = albedo * dfn_surface_light(v_wpos, n, vis, v_color0.a);
    gl_FragColor = vec4(dfn_aerial(v_wpos, lit), 1.0);
}
