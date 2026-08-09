$input a_position, a_normal, a_texcoord0, a_color0
$output v_color0, v_normal, v_texcoord0, v_wpos

// Foliage vertex shader (alpha-cutout leaf cards, user request «листву плоскими
// прозрачными большими плоскими наборами листочков ... чтобы она якобы
// перемещалась»). Applies the shared wind sway in WORLD space.
//
// VERTEX COLOUR IS REPURPOSED on this program: leaf cards take their albedo
// from the mask texture, so rgb is free for wind data.
//   r = sway weight (0 at the branch attachment, 1 at the card's outer edge)
//   g = per-instance phase
//   b = spare (per-card stiffness / mask variant)
//   a = sky visibility (UNCHANGED — foliage darkens in caves like everything)

#include <bgfx_shader.sh>
#include "dfn_env.sh"

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    wpos += dfn_wind_offset(wpos, a_color0.r, a_color0.g);
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
    v_wpos = wpos;
    v_normal = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}
