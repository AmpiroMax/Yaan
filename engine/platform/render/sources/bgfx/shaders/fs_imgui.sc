$input v_texcoord0, v_color0

// Dear ImGui fragment shader: vertex tint times the atlas sample.
//
// The font atlas is RGBA8 with white RGB and the coverage in alpha, so plain
// modulation covers both cases ImGui produces — glyphs and untextured filled
// rectangles (which sample a white texel).

#include <bgfx_shader.sh>

SAMPLER2D(s_imguiTex, 0);

void main()
{
    gl_FragColor = v_color0 * texture2D(s_imguiTex, v_texcoord0);
}
