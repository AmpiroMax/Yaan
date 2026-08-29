/*
Module: engine/platform/anim
File: engine/platform/anim/interfaces/IAnim.h

Responsibility:
- The platform skeletal animation contract (Rule 0). Skeleton/clip loading,
  sampling and blending; ozz-animation lives only behind it.

Key items:
- IAnim: load skeleton/clip (runtime format), instances, evaluate -> skinning
  matrices written into a plain glm::mat4 span.
- AnimLayer: one (clip, time, weight) input to a blended evaluation.
- SkeletonHandle / ClipHandle / AnimInstanceHandle: opaque POD handles (0 = invalid).

Dependencies:
- Uses: C++ stdlib, glm (Rule 2). Nothing else — deliberately no IRenderer/bgfx
  dependency; the output is plain matrix spans.
- Used by: engine/anim (state machines, humanoid rig contract), engine/render
  (stage 3: palettes into the skinned-mesh submit, via contract sync), tests.

Notes:
- Runtime formats: backend-specific files produced by the offline asset pipeline
  (ozz backend: .ozz skeleton/animation files from gltf2ozz). Paths are given by
  the caller; the interface does not know the asset layout.
- Skinning matrix convention (agreed with render, stage-1 sync): column-major
  glm::mat4; each matrix = model-space joint transform * inverse bind pose
  (skinning-ready palette; mesh vertices in model space); palette ordered by
  skeleton joint index; span size >= joint_count(). Evaluation happens at the
  fixed tick (Rule 12); render-side palette interpolation is not planned.
- Blending: evaluate() samples every layer at its time and blends by weight;
  weights are normalized by the backend (all-zero weights = bind pose).
- Null backend (Rule 3 — a runnable mode): loads succeed with valid-but-inert
  handles, joint_count() returns 0, clip_duration() returns 0, evaluate() fills
  the whole output span with identity and returns true. Headless tours and tests
  therefore see bind-pose characters and never crash.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add ozz types, includes, or assumptions to this header.
- Contract frozen for stage 1 (Rule 26); changes only via group sync.
*/

#pragma once

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <span>
#include <string_view>

namespace dfn::platform {

// Opaque resource handles. id == 0 means "invalid / none".
struct SkeletonHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct ClipHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct AnimInstanceHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};

// One input to a blended evaluation. time_seconds is absolute clip time; looping
// policy (wrap vs clamp) is decided by the caller before evaluate().
struct AnimLayer {
    ClipHandle clip;
    float time_seconds = 0.0f;
    float weight = 0.0f;
};

class IAnim {
public:
    virtual ~IAnim() = default;

    // Lifecycle ----------------------------------------------------------------
    [[nodiscard]] virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Assets (runtime format files, produced offline) --------------------------
    [[nodiscard]] virtual SkeletonHandle load_skeleton(std::string_view path) = 0;
    virtual void unload_skeleton(SkeletonHandle skeleton) = 0;

    // A clip is bound to the skeleton it was authored for; mixing is an error.
    [[nodiscard]] virtual ClipHandle load_clip(SkeletonHandle skeleton,
                                               std::string_view path) = 0;
    virtual void unload_clip(ClipHandle clip) = 0;

    [[nodiscard]] virtual uint32_t joint_count(SkeletonHandle skeleton) const = 0;
    [[nodiscard]] virtual float clip_duration(ClipHandle clip) const = 0; // seconds

    // Instances (one per animated character; owns backend sampling caches) -----
    [[nodiscard]] virtual AnimInstanceHandle create_instance(SkeletonHandle skeleton) = 0;
    virtual void destroy_instance(AnimInstanceHandle instance) = 0;

    // Evaluation ---------------------------------------------------------------
    // Samples all layers, blends by (normalized) weight, writes one skinning
    // matrix per joint into out_skinning (see header notes for the convention).
    // Returns false if the instance is invalid or out_skinning is smaller than
    // joint_count(); the null backend writes identities and returns true.
    [[nodiscard]] virtual bool evaluate(AnimInstanceHandle instance,
                                        std::span<const AnimLayer> layers,
                                        std::span<glm::mat4> out_skinning) = 0;
};

} // namespace dfn::platform
