$input a_position, a_normal, a_texcoord0, a_color0
$output v_color0, v_normal, v_texcoord0, v_wpos

// Terrain vertex shader v2 (stage 3): world-space mesh; passes world position
// for the fragment splat (height) and distance fog.

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    v_normal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
    v_color0 = a_color0;
    v_texcoord0 = a_texcoord0;
}
