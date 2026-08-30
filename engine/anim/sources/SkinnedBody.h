/*
Module: engine/anim
File: engine/anim/sources/SkinnedBody.h

Responsibility:
- RETARGETING: drive an imported skeleton with the rig's own LocalPose, and
  hand the renderer the skinning-matrix palette that results. This is what
  makes the procedural gait -- written for 15 boxes -- move a bought model.

Key items:
- JointLocal: one imported joint's local TRS -- the base a clip sample lays
  under the rig's own pose.
- SkinnedRigBinding: the naming bind plus the per-bone frame CORRECTION and
  the import scale check.
- bind_skinned_rig(): built once per model, from rig + skeleton.
- skinning_palette(): LocalPose -> one mat4 per joint, MODEL space.
- cpu_skin_position(): the reference the GPU program is tested against.

Dependencies:
- Uses: Rig.h, Pose.h, BoneMap.h, core skeleton, platform render (SkinnedVertex
  layout only -- the same interface-header-only dependency BodyMesh already has).
- Used by: engine/app (per-frame palette), tests.

Notes:
- THE ONE PIECE OF MATHS WORTH READING TWICE. Our LocalPose stores a rotation
  RELATIVE TO OUR REST POSE, expressed in OUR bone's frame. The imported joint
  has its own bind orientation, which is not ours -- and on a T-posed model it
  is not even close. So the bind is first CARRIED INTO our rest, once, at
  bind time, in a single forward pass over the joints:
      R_ours(b) = the bone's model-space rotation in OUR rest pose (FK of an
                  identity LocalPose)
      D(j)      = inverse(R_model_corrected(parent(j)) * R_bind(j)) * R_ours(b)
      local(j)  = T_bind * (R_bind * D(j) * delta_ours(b)) * S_bind
  The forward pass matters: D(j) is written against the parent's ALREADY
  corrected model rotation, so a chain of corrections composes instead of
  fighting. At an identity pose every bound joint then sits at exactly
  R_ours(b) -- the model stands in our rest pose -- and `delta_ours` lands in
  the bone's own frame, which is where our clips author it.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions (Rule 30 testability): no ECS, no IO, no renderer here.
*/

#pragma once

#include "engine/anim/sources/BoneMap.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <array>
#include <glm/mat4x4.hpp>
#include <span>

namespace dfn::anim {

/// ONE JOINT'S LOCAL TRANSFORM in the imported skeleton's own terms — the
/// form skel::sample_clip produces and the form the palette below accepts as
/// its BASE. It lives here, next to the retarget, because the retarget is the
/// only thing that ever has to hold both this and a LocalPose at once.
struct JointLocal {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct SkinnedRigBinding {
    SkeletonBinding names;
    /// THE REST DELTA: the local rotation that carries the imported joint from
    /// its own BIND orientation to OUR REST orientation. Applied before the
    /// pose's own rotation, so an identity LocalPose puts the model in OUR
    /// rest pose -- arms down, legs converged -- and not in whatever pose its
    /// author happened to bind it in.
    ///
    /// IT IS NOT A CHANGE OF BASIS, and the difference is a whole wave's worth
    /// of wrong. A conjugation C^-1 * delta * C leaves the model IN ITS BIND
    /// POSE at identity: the reference base binds in a T-pose, so the
    /// character stood with its arms straight out and every clip swung them
    /// from there. Measured: 1.591 m wide against 1.705 m tall.
    std::array<glm::quat, BONE_COUNT> rest_delta{};
    /// How tall the imported model stands, in metres, measured from the bind
    /// pose at import time. Zero when unknown.
    float model_height_m = 0.0f;

    SkinnedRigBinding() { rest_delta.fill(glm::quat{1.0f, 0.0f, 0.0f, 0.0f}); }
    [[nodiscard]] uint32_t bound_count() const { return names.bound_count; }
};

/// Built ONCE per model (the bind pose does not change). `rig` supplies our
/// rest frames, `skeleton` the imported bind frames.
[[nodiscard]] SkinnedRigBinding bind_skinned_rig(const Rig& rig,
                                                 const skel::Skeleton& skeleton);

/// The palette for one pose: `out[j]` = posed joint in MODEL space x the
/// joint's inverse bind, i.e. exactly what a skinned vertex program wants.
/// out.size() must be >= skeleton.size(); joints past the rig's reach keep
/// their bind pose and ride with their parents.
void skinning_palette(const Rig& rig, const skel::Skeleton& skeleton,
                      const SkinnedRigBinding& binding, const LocalPose& pose,
                      std::span<glm::mat4> out);

/// THE IMPORTED SKELETON'S JOINTS IN OUR REST POSE, model space -- the same
/// matrices skinning_palette builds, one multiplication before it folds in the
/// inverse binds. It exists because MEASURING A MODEL IN ITS BIND POSE AND
/// SIZING IT BY ITS REST POSE IS TWO FRAMES IN ONE SUM: the importer's canon
/// fit did exactly that for one revision and asked for a skeleton 6.6e12
/// metres tall. Anything that measures proportions reads joints from here.
void rest_model_matrices(const Rig& rig, const skel::Skeleton& skeleton,
                         const SkinnedRigBinding& binding, const LocalPose& pose,
                         std::span<glm::mat4> out);

/// THE HALF OF rest_model_matrices THAT IS NOT FK: our pose written out as the
/// imported skeleton's own LOCAL transforms. rest_model_matrices calls it and
/// then runs FK, so there is exactly one copy of the composition
/// `T_bind * (R_bind * rest_delta * ours) * S_bind` in the tree (Rule 35).
///
/// IT EXISTS BECAUSE A CLIP AND A POSE HAVE TO MEET SOMEWHERE. An imported
/// clip already speaks in these locals; our procedural poses speak in
/// LocalPose. Blending a bought walk with a crouch, or crossfading a clip into
/// the procedural sit, means putting both into ONE currency first — and this
/// is the currency, because it is the only one of the two that can express
/// what a clip says about a joint no rig bone maps to.
/// out.size() must be >= skeleton.size().
void pose_local_transforms(const Rig& rig, const skel::Skeleton& skeleton,
                           const SkinnedRigBinding& binding, const LocalPose& pose,
                           std::span<JointLocal> out);

/// FK + inverse binds for a set of locals, i.e. the palette for a pose that
/// did NOT come through our rig. Same output as skinning_palette, one step
/// earlier in the pipe.
void sample_palette(const skel::Skeleton& skeleton, std::span<const JointLocal> local,
                    std::span<glm::mat4> out);

/// LINEAR BLEND SKINNING ON THE CPU, term for term what dfn_skin.sh does on
/// the GPU. It exists so the shader has something to be WRONG AGAINST: a
/// skinning bug on the GPU is invisible to every test that does not have this.
[[nodiscard]] glm::vec3 cpu_skin_position(const platform::SkinnedVertex& v,
                                          std::span<const glm::mat4> palette);

} // namespace dfn::anim
