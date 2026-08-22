/*
UPD:
- 17:08:2026 - 11:54:29: ТРОПА РИСУЕТСЯ САМОЙ ЗЕМЛЁЙ: альфа вершины несёт профиль износа
  (255 = нет тропы), протоптанное берёт текстуру земли и темнеет к голому
  центру. Небесная видимость здесь теперь 1.0 — канал, который её нёс,
  никогда не писался ничем, кроме 255.
- 22:08:2026 - 15:40:00: рельеф земли — s_texAux (стадия 4, лист нормалей), выбор клетки
  нормали теми же step()/bayer, что у альбедо, TBN из экранных производных
  (механизм fs_prop). u_params.w > 0.5 = лист подан; DFN_TERRAIN_NORMALS=0
  снимает подачу — плоская земля прежнего кадра.
- 22:08:2026 - 21:00:00: дизер 4x4 -> 8x8 (u_ditherFine, DFN_DITHER8; 0 = прежние 4x4 бит-в-бит): 16 порогов при FullHD читались шахматкой на стыках материалов.
- 22:08:2026 - 21:50:00: третья рука дизера — DFN_DITHER8=2 гладкое смешение без
  порога (DFN_SEL): палитра выключена и разрешение FullHD, исходное
  обоснование дизера могло истечь; решает кадр стыка тремя руками.
- 23:08:2026 - 00:30:00: материал полотна из альфы (2 бита класса + 6 бит износа) кладётся
  клеткой путевого атласа САМОЙ землёй; доза 0 — прежняя грязь бит-в-бит.
- 23:08:2026 - 04:10:00: кромка полотна уже спада износа (smoothstep 0.35..0.60): гладкое
  смешение красило камень полупрозрачной зеленью на метрах кромки ([N34]).
*/
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

// R5 (REFERENCE_FRAMES.md), two defects and two answers, both below:
//   (b) A READABLE TILE. Proven to BE the tile and not the dither or the
//       coverage AA by the DFN_TERRAIN_TILES 8/32/128 arms — the blob scale
//       moved by exactly the tiling factor in both directions. The answer is
//       the SECOND grass fetch at an incommensurate scale: the material still
//       repeats every 8 m, but no 8 m square is a copy of its neighbour any
//       more, and a repeat you cannot find twice is not readable.
//   (a) ONE TONE. Measured (tools/measure_ground_colour.py): our ground's
//       chroma spread is 5.7/3.6/1.8 at fine/mid/coarse scale against 9.3-21.5
//       / 7.3-17.7 / 2.3-7.5 in reference frames 01, 02 and 15 — lowest at
//       every scale, and worst at the coarse one. The answer is dfn_ground.sh.
#include <bgfx_shader.sh>
#include "dfn_env.sh"
#include "dfn_ground.sh"
#include "dfn_shadow.sh"

SAMPLER2D(s_texColor, 0);
// THE GROUND'S RELIEF SHEET (22.08): the terrain normal atlas, same 2x2
// layout as the splat atlas, baked from the same pre-ramp fields. Stage 4 is
// the aux-sheet contract (BgfxRendererSubmit binds a neutral 1x1 when no
// draw supplies one); u_params.w > 0.5 = a real sheet is bound.
SAMPLER2D(s_texAux, 4);
// ПУТЕВОЙ АТЛАС (стадия 5, DrawParams::aux2_texture): клетки материалов
// полотна троп в раскладке generate_path_atlas — (0,0) COBBLE, (1,0)
// PACKED_EARTH, (0,1) SCUFFED, (1,1) CUT_SLAB. Читается только при
// u_pathMatDose > 0.5 и ненулевом износе.
SAMPLER2D(s_texPath, 5);
uniform vec4 u_params;

vec3 atlas_sample(vec2 tiled_uv, vec2 cell)
{
    // Each atlas cell tiles independently: wrap inside the cell, then map the
    // fractional uv into the cell's quarter of the atlas.
    return texture2D(s_texColor, (cell + fract(tiled_uv)) * 0.5).rgb;
}

vec3 nrm_sample(vec2 tiled_uv, vec2 cell)
{
    return texture2D(s_texAux, (cell + fract(tiled_uv)) * 0.5).xyz * 2.0 - 1.0;
}

float bayer2(vec2 p) // [[0,2],[3,1]] as arithmetic: 2x + 3y - 4xy
{
    return 2.0 * p.x + 3.0 * p.y - 4.0 * p.x * p.y;
}

// ТРЕТЬЯ РУКА ДИЗЕРА (u_ditherFine >= 2): вовсе без порога — вес смешения
// напрямую. Дизер существовал ради 64-цветной палитры и правила «дизер, не
// градиент», написанных под 640x360; палитра выключена, разрешение FullHD —
// исходное обоснование могло истечь, и решает кадр стыка травы и мостовой,
// снятый тремя руками из одного бинарника (0 = 4x4, 1 = 8x8, 2 = гладко).
// Макрос, а не функция: `bayer` — локал main, в .sc его не пронести иначе.
#define DFN_SEL(w) (u_ditherFine >= 2.0 ? clamp(w, 0.0, 1.0) : step(bayer, w))

void main()
{
    // LOD cross-fade: the outgoing level dissolves as the incoming one
    // appears, both drawn, neither blended (dfn_env.sh).
    dfn_screen_door(u_params.y, gl_FragCoord.xy);
    vec3 n = normalize(v_normal);
    vec2 tuv = v_texcoord0 * u_terrainTiles;

    // THE TILE, BROKEN WITHOUT BEING REMOVED. Two fetches of the SAME grass
    // cell at scales whose ratio is not a simple fraction, chosen between by a
    // field whose own period (11 m) is not a multiple of the tile's 8 m. The
    // pattern at any point is still one of two known stamps, so nothing is
    // invented and nothing needs new memory; but which stamp, and at what
    // phase, changes across the ground, so the eye has nothing to lock onto.
    // The mix is smooth: a threshold would trade a repeating tile for a
    // repeating BLOTCH EDGE, which is the same defect with a longer period.
    float bomb = dfn_gnoise(v_wpos.xz * (1.0 / 11.0));
    vec3 grass = mix(atlas_sample(tuv, vec2(0.0, 0.0)),
                     atlas_sample(tuv * 0.41 + vec2(0.19, 0.63), vec2(0.0, 0.0)),
                     smoothstep(0.30, 0.70, bomb) * clamp(u_groundTint, 0.0, 1.0));
    grass = dfn_ground_tint(grass, v_wpos.xz, u_groundTint);
    vec3 rock  = atlas_sample(tuv, vec2(1.0, 0.0));
    vec3 sand  = atlas_sample(tuv, vec2(0.0, 1.0));
    vec3 dirt  = atlas_sample(tuv, vec2(1.0, 1.0));

    float slope = 1.0 - n.y;
    float rock_w = max(v_color0.g,
                       smoothstep(u_rockSlopeStart, u_rockSlopeEnd, slope));
    float sand_w = v_color0.r; // shore mask from core's surface_class only
    float bed_w = v_color0.b;

    // Ordered Bayer threshold; the scene view renders at INTERNAL_RES, so
    // gl_FragCoord is already in internal pixels (blocks stay square).
    //
    // 4x4 -> 8x8 (22.08, приёмка круга 2 [N6]): порог 16 уровней задумывался
    // под 640x360, где клетка дизера была крупным элементом стиля; при FullHD
    // та же решётка читается «грубой шахматкой по всем стыкам травы, грунта и
    // мостовой» — полоса перехода квантуется в 16 ступеней, и внутри каждой
    // ступени глаз ловит правильную решётку. Третий уровень рекурсии Байера
    // даёт 64 порога: решётка та же, ступени вчетверо мельче. Это всё ещё
    // ordered dither в пикселях сцены (§4: дизер, не градиенты).
    // u_ditherFine — доза (слот 40.w): 0 = прежние 4x4 бит-в-бит.
    vec2 ip = floor(gl_FragCoord.xy);
    vec2 f1 = mod(ip, 2.0);
    vec2 f2 = mod(floor(ip * 0.5), 2.0);
    vec2 f3 = mod(floor(ip * 0.25), 2.0);
    float bayer4 = (bayer2(f1) * 4.0 + bayer2(f2) + 0.5) / 16.0;
    float bayer8 = (bayer2(f1) * 16.0 + bayer2(f2) * 4.0 + bayer2(f3) + 0.5) / 64.0;
    float bayer = mix(bayer4, bayer8, clamp(u_ditherFine, 0.0, 1.0)); // (0,1)

    // §4 priority via paint order (later mix wins): grass -> bed -> rock ->
    // sand on top. The blend band is a two-material dither: grass and rock
    // texels only, never a third color (§4 rule 3).
    vec3 albedo = grass;
    albedo = mix(albedo, dirt * 0.68, DFN_SEL(bed_w)); // dark wet bed
    albedo = mix(albedo, rock, DFN_SEL(rock_w));
    albedo = mix(albedo, sand, DFN_SEL(sand_w));

    // Untextured fallback: flat splat palette (headless / atlas not resident).
    vec3 flat_albedo = vec3(0.33, 0.43, 0.22);
    flat_albedo = mix(flat_albedo, vec3(0.42, 0.40, 0.38), DFN_SEL(rock_w));
    flat_albedo = mix(flat_albedo, vec3(0.72, 0.65, 0.44), DFN_SEL(sand_w));
    albedo = mix(flat_albedo, albedo, step(0.5, u_params.x));

    // THE PATH IS THE GROUND, not a ribbon over it. Vertex alpha carries the
    // §8.1 wear profile inverted (255 = no path), so a world with no network
    // renders byte-identically and a trodden sample darkens and bares itself
    // exactly where core wore the terrain down. The user asked for precisely
    // this on 17.08 — «тропинки должны быть свойством земли, а не поверх
    // нарисованной текстурой» — after paths kept hovering: two surfaces built
    // from one field will always disagree somewhere, and ground cannot hover
    // over itself.
    // МАТЕРИАЛ ПОЛОТНА В САМОЙ ЗЕМЛЕ (22.08, владелец: «тропинки опять
    // плитами кладутся, а не тропинкой каменной»). При дозе > 0.5 альфа несёт
    // 2 бита класса + 6 бит обратного износа (контракт pack_path_alpha,
    // TerrainMesher.h): мостовая, укатанный грунт, стёжка и тёсаные плиты —
    // клетки путевого атласа, положенные ЗЕМЛЁЙ, а не настилом поверх неё.
    // Грунтовые классы темнеют к протоптанному центру, как прежняя тропа;
    // камень — нет: у мостовой нет «голой середины». Доза 0 — прежний
    // 8-битный износ и прежняя грязь бит-в-бит.
    float path_w;
    vec3 path_col;
    if (u_pathMatDose > 0.5) {
        float a8 = floor(v_color0.a * 255.0 + 0.5);
        float cls = floor(a8 / 64.0);
        float wi = a8 - cls * 64.0;
        path_w = 1.0 - wi / 63.0;
        vec2 pcell = vec2(mod(cls, 2.0), floor(cls * 0.5));
        path_col = texture2D(s_texPath,
                             (pcell + fract(v_wpos.xz * u_pathTiles)) * 0.5).rgb;
        float earthy = (cls == 1.0 || cls == 2.0) ? 1.0 : 0.0;
        path_col *= mix(1.0, mix(1.0, 0.72, path_w), earthy);
    } else {
        path_w = 1.0 - v_color0.a;
        // The trodden surface is the DIRT texel, darkened toward bare earth at
        // the worn centre. Painted last so a path crosses sand and rock as a
        // path — a trail over a shingle bank is still a trail.
        path_col = dirt * mix(1.0, 0.62, path_w);
    }
    // КРОМКА ПОЛОТНА УЖЕ, ЧЕМ СПАД ИЗНОСА (круг 4, [N34]: «зелень красится
    // поверх камня» — гладкое смешение по всему спаду делало камень
    // полупрозрачно-зелёным на метрах кромки, и былинки читались сквозь
    // плиту). Камень либо лежит, либо нет: полоса смешения сжата в
    // 0.35..0.60 износа — визуально ~0.3-0.5 м кромки вместо метров.
    float path_mix = u_pathMatDose > 0.5
        ? smoothstep(0.35, 0.60, path_w)
        : DFN_SEL(path_w);
    albedo = mix(albedo, path_col, path_mix);

    // THE GROUND'S RELIEF (22.08, «земля — крашеный ковёр рядом со стеной с
    // рельефом»). The normal is picked by the SAME step()/bayer selections
    // the albedo used — the texel whose colour you see is the texel whose
    // slope shades, one groove in both sheets. TBN from screen derivatives:
    // fs_prop's mechanism copied deliberately (no shared include exists, and
    // two DIFFERENT basis derivations would tilt the ground's relief against
    // the wall standing on it).
    if (u_params.w > 0.5) {
        vec3 tn = nrm_sample(tuv, vec2(0.0, 0.0));
        tn = mix(tn, nrm_sample(tuv, vec2(1.0, 1.0)), DFN_SEL(bed_w));
        tn = mix(tn, nrm_sample(tuv, vec2(1.0, 0.0)), DFN_SEL(rock_w));
        tn = mix(tn, nrm_sample(tuv, vec2(0.0, 1.0)), DFN_SEL(sand_w));
        tn = mix(tn, nrm_sample(tuv, vec2(1.0, 1.0)), DFN_SEL(path_w));
        vec3 dp1 = dFdx(v_wpos);
        vec3 dp2 = dFdy(v_wpos);
        vec2 du1 = dFdx(tuv);
        vec2 du2 = dFdy(tuv);
        float det = du1.x * du2.y - du2.x * du1.y;
        if (abs(det) > 1e-12) {
            vec3 tangent = normalize((dp1 * du2.y - dp2 * du1.y) / det);
            tangent = normalize(tangent - n * dot(n, tangent));
            vec3 bitangent = cross(n, tangent);
            tn = normalize(tn);
            n = normalize(tangent * tn.x + bitangent * tn.y + n * tn.z);
        }
    }

    float vis = dfn_shadow_factor(v_wpos, n);
    // Sky visibility is 1 here: the channel that used to carry it was never
    // written with anything but 255, and the interior falloff it was reserved
    // for arrives with its own signal when core supplies one.
    vec3 lit = albedo * dfn_surface_light(v_wpos, n, vis, 1.0);

    gl_FragColor = vec4(dfn_aerial(v_wpos, lit), 1.0);
}
