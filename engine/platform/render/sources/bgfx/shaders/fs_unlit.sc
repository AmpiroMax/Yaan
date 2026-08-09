$input v_color0, v_texcoord0

// Unlit fragment shader. u_params.x > 0.5 -> modulate by s_texColor.

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_params;

void main()
{
    vec4 tex = texture2D(s_texColor, v_texcoord0);
    vec4 textured = v_color0 * tex;
    gl_FragColor = mix(v_color0, textured, step(0.5, u_params.x));
}
