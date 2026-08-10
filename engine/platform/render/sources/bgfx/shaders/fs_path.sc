$input v_color0, v_normal, v_texcoord0, v_wpos

// Path surface fragment (LANDSCAPE §8.1 item 1): the trodden cross-section —
// worn centre -> pressed margins -> ground — over the 2x2 path atlas whose cell
// index IS core's PathClass ordinal (ProcTexture.h layout contract).
//
// Attribute contract with PathMesher.h (change both together):
//   v_texcoord0 = (across_m, along_m) on the tread frame; along is TRUE ARC
//                 LENGTH accumulated along the centreline, so the material does
//                 not stretch through a bend.
//   v_color0.r  = WEAR in [0,1]. Core's `path_wear_profile` sampled at the
//                 mesher's cross-section knots; linear between them, and the
//                 knot error is measured (PathMesher.h), not assumed.
//   v_color0.g  = (PathClass ordinal + 0.5) / 4.
//   v_color0.b  = BR-3 rich-edge weight. Carried, NOT painted — see below.
//   v_color0.a  = sky visibility (1.0 outdoors), as everywhere else.
//
// WHY THIS IS A GRADIENT AND NOT A DECAL RIBBON (§8.1 forbids the latter): the
// surface's COVERAGE falls with wear and is resolved by the same ordered 4x4
// Bayer threshold the terrain splat uses, on the internal-pixel grid. The
// ribbon mesh therefore has no visible boundary — it dissolves into whatever
// the terrain drew, which is the §4 "dither, not gradients" technique applied
// across a path's width instead of along a slope.
//
// WHY THE RICH EDGE IS NOT PAINTED HERE: BR-3's margin is richer VEGETATION,
// not a different soil. Painting a green-tinted band would be a render-side
// approximation of a world fact — the exact bug class that produced the
// invented 60 m brown wash (see the splat v4 ruling). The weight is passed
// through so flora's population can key off the same number.

#include <bgfx_shader.sh>
#include "dfn_env.sh"
#include "dfn_shadow.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_params; // x: atlas bound, y: fade, z: highlight, w: tiles per metre

float bayer2(vec2 p) // [[0,2],[3,1]] as arithmetic: 2x + 3y - 4xy
{
    return 2.0 * p.x + 3.0 * p.y - 4.0 * p.x * p.y;
}

void main()
{
    dfn_screen_door(u_params.y, gl_FragCoord.xy);

    float wear = clamp(v_color0.r, 0.0, 1.0);
    float cls = floor(clamp(v_color0.g, 0.0, 0.999) * 4.0);

    // HOW FAR THE MATERIAL HOLDS BEFORE IT DISSOLVES IS PER CLASS, and it is
    // design's maintenance column read as a surface rather than as planting
    // (§1.7 BR-3): a swept cobbled way is solid to its kerb, a hint-path is
    // BARELY a surface at all — it IS a partial baring of the ground, so its
    // coverage falls across the whole tread. Getting this the same for all four
    // would make the hint-path a narrow road, which is the one thing в7's four
    // classes exist to prevent.
    //   cobble 0.12 | dirt 0.35 | faint trail 0.85 | steps 0.10
    float hold = cls < 0.5 ? 0.12
               : (cls < 1.5 ? 0.35
               : (cls < 2.5 ? 0.85 : 0.10));
    float coverage = smoothstep(0.0, hold, wear);

    vec2 ip = floor(gl_FragCoord.xy);
    vec2 f1 = mod(ip, 2.0);
    vec2 f2 = mod(floor(ip * 0.5), 2.0);
    float bayer = (bayer2(f1) * 4.0 + bayer2(f2) + 0.5) / 16.0; // (0,1)
    if (coverage < bayer) {
        discard; // the ground the terrain already drew shows through
    }

    // Atlas cell = the PathClass ordinal, in the terrain atlas's own layout
    // (i -> {i & 1, i >> 1}), each cell wrapping inside its quarter.
    vec2 cell = vec2(mod(cls, 2.0), floor(cls * 0.5));
    vec2 tuv = v_texcoord0 * max(u_params.w, 0.0001);
    vec3 albedo = texture2D(s_texColor, (cell + fract(tuv)) * 0.5).rgb;

    // Untextured fallback (headless / atlas not resident): flat per-class
    // values in the same order, so a frame without the atlas still shows the
    // network rather than a magenta stripe.
    vec3 flat_albedo = cls < 0.5 ? vec3(0.44, 0.42, 0.37)
                     : (cls < 1.5 ? vec3(0.47, 0.39, 0.28)
                     : (cls < 2.5 ? vec3(0.39, 0.34, 0.22) : vec3(0.38, 0.39, 0.39)));
    albedo = mix(flat_albedo, albedo, step(0.5, u_params.x));

    // THE PRESSED MARGIN. Between the bare centre and the ground the surface is
    // trodden but not bared: compacted, dirtied, holding the litter the centre
    // has lost. That is a VALUE move on the same material, and it is the one
    // place a value move is right — the hue is already carried by which atlas
    // cell we are in, and the margin is not a different material.
    albedo *= 0.78 + 0.22 * wear;

    vec3 n = normalize(v_normal);
    float vis = dfn_shadow_factor(v_wpos, n);
    vec3 lit = albedo * dfn_surface_light(v_wpos, n, vis, v_color0.a);

    float fog = dfn_fog_factor(v_wpos);
    gl_FragColor = vec4(mix(lit, u_fogColor, fog), 1.0);
}
