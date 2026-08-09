// Shadow-pass fragment shader: depth-only (color writes are disabled by the
// render state; the output value is never consumed).

#include <bgfx_shader.sh>

void main()
{
    gl_FragColor = vec4_splat(0.0);
}
