$input a_position

// Shadow-pass vertex shader (backend-internal "shadow" program): renders
// caster depth from the sun's orthographic view. The shadow view's transform
// is the light view/proj, so the standard modelViewProj does all the work.

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
