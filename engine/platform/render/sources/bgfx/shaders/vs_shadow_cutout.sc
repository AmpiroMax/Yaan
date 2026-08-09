$input a_position, a_texcoord0, a_color0
$output v_texcoord0

// Shadow-pass vertex shader for alpha-cutout casters. MUST apply the identical
// wind sway to vs_foliage: if the shadow geometry does not sway with the
// leaves, the canopy's shadow stands still while the canopy moves, which reads
// worse than no wind at all.

#include <bgfx_shader.sh>
#include "dfn_env.sh"

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    wpos += dfn_wind_offset(wpos, a_color0.r, a_color0.g);
    // u_viewProj is the CURRENT view's matrix, and this shader only ever runs
    // on the shadow view, so this is the light's projection.
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
    v_texcoord0 = a_texcoord0;
}
