$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

// Dear ImGui vertex shader (editor interface only, never the game's HUD).
//
// The positions arrive in INTERFACE PIXELS, origin top-left, and u_modelViewProj
// carries the orthographic map from those pixels to clip space. That is what
// lets the same draw lists be submitted twice — once to the window's backbuffer
// and once, with a different ortho, into the internal-sized capture target —
// without touching a single vertex.

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position.xy, 0.0, 1.0));
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}
