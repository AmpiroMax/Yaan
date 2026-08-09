$input v_wpos

// Point-light shadow caster fragment: writes DISTANCE TO THE LIGHT, normalized
// by the light's radius, into the face's atlas tile. Storing a linear metre
// distance instead of projected depth means the receiver compares world
// distances (dfn_pointshadow.sh) and never has to linearize a depth buffer or
// know the face's near/far planes.

#include <bgfx_shader.sh>

// xyz: light position (world), w: light radius = the face far plane.
uniform vec4 u_pointCaster;

void main()
{
    float d = length(v_wpos - u_pointCaster.xyz) / max(u_pointCaster.w, 0.0001);
    gl_FragColor = vec4(clamp(d, 0.0, 1.0), 0.0, 0.0, 1.0);
}
