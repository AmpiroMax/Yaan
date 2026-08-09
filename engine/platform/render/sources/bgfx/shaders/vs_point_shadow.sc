$input a_position
$output v_wpos

// Point-light shadow caster (backend-internal "point_shadow" program): renders
// one of the six 90-degree faces of a carried light's cube map. The face's
// view/proj is the view transform, so modelViewProj does the projection; the
// world position is forwarded because the FRAGMENT stores linear distance to
// the light, not projected depth (see dfn_pointshadow.sh).

#include <bgfx_shader.sh>

void main()
{
    vec4 wpos = mul(u_model[0], vec4(a_position, 1.0));
    v_wpos = wpos.xyz;
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
