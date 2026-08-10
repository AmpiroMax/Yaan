$input v_texcoord0
/*
UPD:
- 10:08:2026 - 23:26:06: Mip 0 pinned explicitly (texture2DLod). The mask
  gained a mip chain for the colour pass's coverage antialiasing, and an
  averaged alpha against this hard 0.5 threshold would have thinned the
  canopy's SHADOW as a side effect. The shadow pass has no coverage to
  spend, so it keeps the full-detail mask.
*/

// Shadow-pass fragment for cutout casters: punch the mask's holes through the
// depth map. Without this every leaf card casts a solid rectangle and the
// canopy shadows like a box — worse than the shadowless trunks we just fixed.

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main()
{
    // MIP 0 EXPLICITLY. The mask now carries a mip chain (it is what makes the
    // treeline stop flickering in the colour pass), and a mip level averaged
    // against this hard 0.5 threshold would thin the canopy's SHADOW as the
    // caster recedes — a second, different defect arriving as a side effect.
    // The shadow pass has no coverage to spend, so it keeps the full-detail
    // mask and the cutout it was written for.
    if (texture2DLod(s_texColor, v_texcoord0, 0.0).a < 0.5) {
        discard;
    }
    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
