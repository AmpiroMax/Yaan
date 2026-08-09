$input v_texcoord0

// Upscale pass: point-sample the low-res internal target (Q9). Stage 3 adds
// the optional palette post (Q9b): ordered 4x4 Bayer dither in INTERNAL pixel
// space, then nearest-color quantization against the fixed 64-entry palette
// (u_palette, built by BgfxPalette.cpp). Enabled via u_postParams.x
// (RendererInitParams::palette_post <- DFN_PALETTE=1); OFF keeps the exact
// stage-2 passthrough.

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_postParams; // x: palette on, y: palette size, zw: internal res
uniform vec4 u_palette[64];

float bayer2(vec2 p) // [[0,2],[3,1]] as arithmetic: 2x + 3y - 4xy
{
    return 2.0 * p.x + 3.0 * p.y - 4.0 * p.x * p.y;
}

void main()
{
    vec4 src = texture2D(s_texColor, v_texcoord0);
    vec4 result = src;
    if (u_postParams.x > 0.5)
    {
        // Dither computed per internal pixel so every upscaled block of the
        // same source pixel quantizes identically (pixels stay square).
        vec2 ip = floor(v_texcoord0 * u_postParams.zw);
        vec2 f1 = mod(ip, 2.0);
        vec2 f2 = mod(floor(ip * 0.5), 2.0);
        float bayer = (bayer2(f1) * 4.0 + bayer2(f2)) / 16.0; // 0..15/16
        vec3 c = src.rgb + (bayer - 0.46875) * 0.05;

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
