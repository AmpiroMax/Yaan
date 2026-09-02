$input a_position, a_texcoord0, a_indices, a_weight
$output v_texcoord0

// Sun-depth caster for SKINNED CUTOUT meshes (backend-internal
// "shadow_skinned_cutout"; волна «части персонажа»): a strand of hair, a
// lash, a brow. Posed by the palette like vs_shadow_skinned -- the plain
// cutout caster reads a_position as the BIND pose -- and it carries the
// texcoord through so fs_shadow_cutout can punch the mask's holes into the
// depth map: without that every hair card casts a solid rectangle and the
// head shadows like a pot.

#include <bgfx_shader.sh>
#include "dfn_skin.sh"

void main()
{
	vec3 skinned = dfn_skin_point(a_position, a_indices, a_weight);
	gl_Position = mul(u_modelViewProj, vec4(skinned, 1.0));
	v_texcoord0 = a_texcoord0;
}
