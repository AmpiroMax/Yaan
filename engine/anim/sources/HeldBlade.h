/*
Module: engine/anim
File: engine/anim/sources/HeldBlade.h

Responsibility:
- THE SWORD IN THE HAND, as geometry. Until this file existed, "weapon drawn"
  was a pose and nothing else: the stance changed and the hand closed on air,
  so no frame could answer whether the blade was carried at the reference's
  30-40 degrees to the ground, and the comparison report called that its
  blocker.

Key items:
- HeldBlade / build_held_blade(): a plain arming sword — blade, crossguard,
  grip, pommel — as SKINNED vertices weighted 1.0 to the hand joint, so it
  rides the character's own palette and needs no second placement.
- HELD_BLADE_MESH_ID: the RenderMesh id it occupies.

Dependencies:
- Uses: Rig, SkinnedBody (the binding), core skeleton, platform's SkinnedVertex
  (the frozen layout), glm.
- Used by: the app (registration + the frame's second skinned draw), tests.

Notes:
- WHY ONE BONE AND NOT A PROP ENTITY WITH ITS OWN TRANSFORM. A prop would need
  the hand's world matrix ferried out of the pose every frame, interpolated by
  hand, and kept in step with the body across the tick/frame seam — three
  places to drift. A vertex weighted 1.0 to the hand joint is carried by the
  palette that is already being built, so the sword cannot be a frame behind
  the fist by construction. It costs one draw call and no state.
- AND THE VERTICES ARE IN BIND-MODEL SPACE, which is what makes that true. The
  palette is `model[j] * inverse_bind[j]`, so a point authored at
  `bind_model[hand] * offset` arrives at `model[hand] * offset` — i.e. exactly
  where the offset says, in the posed hand's own frame.
- WHY THE BLADE LIES ALONG THE KNUCKLES, and it is the second answer here. The
  first was "continue the forearm", which needs only two joints every rig has
  — and the frame showed what that costs: standing, the arm hangs straight
  down, so the sword hung with it and went a third of a metre into the grass.
  A fist holds a hilt ACROSS the palm. The knuckle line is found by geometry
  and not by bone name (the thumb is the hand-child that sits apart from the
  rest; the others lie on the line; the blade leaves on the thumb side), so
  the rule survives the next asset. A dedicated hand SLOT would be better
  still and the asset has none — KayKit's Knight does, HumanBase does not.
- IT IS PLACEHOLDER ASSET DATA (Rule 5's carve-out, the same standing as the
  box body's boxes and render's ProcMesh): primitives with flat vertex
  colours, authored here because there is no sword in the asset library and a
  dubious download is not an option.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure and deterministic: same skeleton in, same vertices out.
*/

#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <cstdint>
#include <span>
#include <vector>

namespace dfn::anim {

/// RenderMesh id for the held blade: 129, the id after the skinned character's
/// 128 and past every range render's ProcMesh.h hands out.
inline constexpr uint32_t HELD_BLADE_MESH_ID = 129;

struct HeldBlade {
    std::vector<platform::SkinnedVertex> vertices;
    std::vector<uint32_t> indices;
    /// The joint every vertex is weighted to, and the blade's own length —
    /// kept so a test can say where the point is without re-deriving the
    /// geometry, and so the app can print what it built.
    int32_t joint = -1;
    float length_m = 0.0f;
    [[nodiscard]] bool valid() const { return joint >= 0 && !vertices.empty(); }
};

/// Builds the blade for one bound model. Returns an invalid HeldBlade (and
/// says nothing — the caller reports) when the hand or the forearm did not
/// bind, because without both there is no line to lay the sword along.
///
/// `guard_pose` is the pose the sword is CARRIED in (the WeaponIdle clip's own
/// sample) and it is what the grip's cant is solved against, so the drawn
/// blade lands on STANCE_BLADE_TILT. Empty is allowed and gives the raw
/// knuckle line, which on this asset holds the blade level: worse, honest, and
/// what happens on a model whose guard clip is missing.
[[nodiscard]] HeldBlade build_held_blade(const skel::Skeleton& skeleton,
                                         const SkinnedRigBinding& binding,
                                         std::span<const JointLocal> guard_pose = {});

} // namespace dfn::anim
