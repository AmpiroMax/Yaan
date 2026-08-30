/*
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_skin.sh

Responsibility:
- Shared shader include: the bone palette uniform and linear blend skinning,
  used by BOTH skinned vertex programs so they cannot disagree about where a
  vertex ends up.

Key items:
- u_bones[64] (IRenderer::MAX_BONE_PALETTE); dfn_skin_point/dfn_skin_vector.

Dependencies:
- Used by: vs_skinned.sc (scene), vs_shadow_skinned.sc (sun depth).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The arithmetic here is the CPU reference's twin (anim::cpu_skin_position):
  transform by each bone, weight the RESULTS. Changing the form here without
  changing it there silently breaks the test that compares them.
*/

// Daggerfall N — shared skinning helper for the skinned vertex programs.
// Used by vs_skinned.sc (scene) and vs_shadow_skinned.sc (sun depth), so the
// two CANNOT disagree about where a vertex ends up: a body whose shadow is
// posed differently from the body is worse than a body with no shadow.
//
// THE PALETTE IS 64 mat4 (IRenderer::MAX_BONE_PALETTE). Slot 0 is bound to
// identity by the backend when a draw supplies no palette, so a mesh whose
// weights all point at slot 0 draws its BIND pose rather than collapsing.
//
// LINEAR BLEND SKINNING, deliberately: positions are transformed by each bone
// and the RESULTS are weighted, which is identical to weighting the matrices
// for affine transforms and is what the CPU reference in the tests does term
// for term (tests/character/SkinningTests.cpp compares the two at 1e-4).

#ifndef DFN_SKIN_SH
#define DFN_SKIN_SH

#define DFN_MAX_BONES 64
uniform mat4 u_bones[DFN_MAX_BONES];

vec3 dfn_skin_point(vec3 p, vec4 idx, vec4 w)
{
	vec4 p4 = vec4(p, 1.0);
	vec3 r = mul(u_bones[int(idx.x)], p4).xyz * w.x;
	r += mul(u_bones[int(idx.y)], p4).xyz * w.y;
	r += mul(u_bones[int(idx.z)], p4).xyz * w.z;
	r += mul(u_bones[int(idx.w)], p4).xyz * w.w;
	return r;
}

vec3 dfn_skin_vector(vec3 v, vec4 idx, vec4 w)
{
	vec4 v4 = vec4(v, 0.0);
	vec3 r = mul(u_bones[int(idx.x)], v4).xyz * w.x;
	r += mul(u_bones[int(idx.y)], v4).xyz * w.y;
	r += mul(u_bones[int(idx.z)], v4).xyz * w.z;
	r += mul(u_bones[int(idx.w)], v4).xyz * w.w;
	return r;
}

#endif // DFN_SKIN_SH
