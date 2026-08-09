$input v_texcoord0

// Upscale pass: point-sample the low-res internal target (Q9). The palette
// post-process flag (Q9, optional) will hook into THIS shader at stage 3.

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main()
{
    gl_FragColor = texture2D(s_texColor, v_texcoord0);
}
