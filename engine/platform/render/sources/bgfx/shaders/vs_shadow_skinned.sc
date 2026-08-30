$input a_position, a_indices, a_weight

// Sun-depth caster for skinned meshes (backend-internal "shadow_skinned").
// It exists because the ordinary "shadow" program reads a_position as the
// BIND pose: a walking body would have cast a standing shadow, which is the
// kind of defect that looks like a lighting bug for a week.

#include <bgfx_shader.sh>
#include "dfn_skin.sh"

void main()
{
	vec3 skinned = dfn_skin_point(a_position, a_indices, a_weight);
	gl_Position = mul(u_modelViewProj, vec4(skinned, 1.0));
}
