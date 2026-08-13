$input v_texcoord0

// Upscale pass: point-sample the low-res internal target (Q9). Stage 3 adds
// the optional palette post (Q9b): ordered 4x4 Bayer dither in INTERNAL pixel
// space, then nearest-color quantization against the fixed 64-entry palette
// (u_palette, built by BgfxPalette.cpp). Enabled via u_postParams.x
// (RendererInitParams::palette_post <- DFN_PALETTE=1); OFF keeps the exact
// stage-2 passthrough.

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_postParams;
uniform vec4 u_blackFloor; // x: floor (0 = off), y: falloff exponent
uniform vec4 u_palette[64];

float bayer2(vec2 p) // [[0,2],[3,1]] as arithmetic: 2x + 3y - 4xy
{
    return 2.0 * p.x + 3.0 * p.y - 4.0 * p.x * p.y;
}

void main()
{
    vec4 src = texture2D(s_texColor, v_texcoord0);
    // BLACK FLOOR, and it goes BEFORE the palette on purpose: applied after
    // quantization it would produce colours the 64-entry palette does not
    // contain, and applied to the palette's own entries it would round back
    // into the black it is lifting off. The clamp guards pow() from a negative
    // base; the exponent keeps the day still (docs/NUMBERS.md).
    vec3 base = src.rgb + u_blackFloor.x
              * pow(clamp(vec3(1.0, 1.0, 1.0) - src.rgb, 0.0, 1.0),
                    vec3(u_blackFloor.y, u_blackFloor.y, u_blackFloor.y));
    base = clamp(base, 0.0, 1.0);
    vec4 result = vec4(base, src.a);
    if (u_postParams.x > 0.5)
    {
        // Dither computed per internal pixel so every upscaled block of the
        // same source pixel quantizes identically (pixels stay square).
        vec2 ip = floor(v_texcoord0 * u_postParams.zw);
        vec2 f1 = mod(ip, 2.0);
        vec2 f2 = mod(floor(ip * 0.5), 2.0);
        float bayer = (bayer2(f1) * 4.0 + bayer2(f2)) / 16.0; // 0..15/16
        vec3 c = base + (bayer - 0.46875) * 0.05;

        float best = 1.0e9;
        vec3 best_color = vec3(0.0, 0.0, 0.0);
        for (int i = 0; i < 64; ++i)
        {
            vec3 d = c - u_palette[i].rgb;
            float dist = dot(d * d, vec3(0.30, 0.59, 0.11)); // luma-weighted
            if (dist < best)
            {
                best = dist;
                best_color = u_palette[i].rgb;
            }
        }
        result = vec4(best_color, 1.0);
    }
    gl_FragColor = result;
}
