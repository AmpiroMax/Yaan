$input v_texcoord0

// Shadow-pass fragment for cutout casters: punch the mask's holes through the
// depth map. Without this every leaf card casts a solid rectangle and the
// canopy shadows like a box — worse than the shadowless trunks we just fixed.

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main()
{
    if (texture2D(s_texColor, v_texcoord0).a < 0.5) {
        discard;
    }
    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
