$input a_position, a_texcoord0
$output v_dir

// Sky background pass (stage 3): fullscreen clip-space quad; the world-space
// view ray is reconstructed by unprojecting two depths — convention-agnostic
// (any two points on the ray give its direction). Drawn first in the scene
// view (sequential mode), no depth test/write; terrain covers it.

#include <bgfx_shader.sh>

void main()
{
    gl_Position = vec4(a_position.xy, 0.5, 1.0); // depth irrelevant: no test
    vec4 p_far = mul(u_invViewProj, vec4(a_position.xy, 1.0, 1.0));
    vec4 p_near = mul(u_invViewProj, vec4(a_position.xy, 0.0, 1.0));
    v_dir = p_far.xyz / p_far.w - p_near.xyz / p_near.w;
}
