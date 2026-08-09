$input a_position, a_normal, a_texcoord0, a_color0
$output v_texcoord0, v_wpos

// Water vertex shader (stage 3): flat plane at a given height; uv in
// water-tile units (world / tile size, wraps in the sampler).

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    v_texcoord0 = a_texcoord0;
}
