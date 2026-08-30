$input a_position, a_normal, a_texcoord0, a_color0, a_indices, a_weight
$output v_color0, v_normal, v_texcoord0, v_wpos

// Skinned vertex program (character wave, 30.08). Same OUTPUTS as vs_terrain
// on purpose: the fragment stage is fs_prop, unchanged, so a skinned body is
// lit, shadowed and fogged by exactly the code that lights everything else —
// PROGRAM_TABLE pairs "skinned" = vs_skinned + fs_prop, the way it already
// pairs "prop" = vs_terrain + fs_prop.

#include <bgfx_shader.sh>
#include "dfn_skin.sh"

void main()
{
	vec3 skinned = dfn_skin_point(a_position, a_indices, a_weight);
	// The normal is skinned by the same matrices WITHOUT their translation
	// (w = 0). Non-uniform bone scale would need the inverse transpose; the
	// importer refuses non-uniform joint scale for exactly that reason, so
	// this stays a rotation and renormalises.
	vec3 nrm = normalize(dfn_skin_vector(a_normal, a_indices, a_weight));
	gl_Position = mul(u_modelViewProj, vec4(skinned, 1.0));
	v_wpos = mul(u_model[0], vec4(skinned, 1.0)).xyz;
	v_normal = normalize(mul(u_model[0], vec4(nrm, 0.0)).xyz);
	v_color0 = a_color0;
	v_texcoord0 = a_texcoord0;
}
