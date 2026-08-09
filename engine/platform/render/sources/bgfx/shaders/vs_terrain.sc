$input a_position, a_normal, a_texcoord0, a_color0
$output v_color0, v_normal, v_texcoord0

// Terrain vertex shader: world-space mesh, per-vertex ground tint from the
// mesher; normal to world space for the fragment lambert.

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
    v_color0 = a_color0;
    v_texcoord0 = a_texcoord0;
}
