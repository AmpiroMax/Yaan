$input v_color0

// Debug line fragment shader: pass-through color.

#include <bgfx_shader.sh>

void main()
{
    gl_FragColor = v_color0;
}
