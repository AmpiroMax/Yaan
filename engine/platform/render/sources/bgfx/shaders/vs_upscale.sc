$input a_position, a_texcoord0
$output v_texcoord0

// Upscale pass: fullscreen-in-viewport quad in clip space (the integer-scaled
// destination rect is expressed via the view rect, Q9).

#include <bgfx_shader.sh>

void main()
{
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    v_texcoord0 = a_texcoord0;
}
