/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
Module: engine/platform/anim
File: engine/platform/anim/sources/null/NullAnim.cpp

Responsibility:
- Null IAnim backend (Rule 3): loading always succeeds with inert handles;
  evaluate() writes identity matrices so consumers render bind poses.

Key items:
- NullAnim (file-local) + create_null_anim() factory.

Dependencies:
- Uses: interfaces/IAnim.h, C++ stdlib.
- Used by: engine/app wiring, tests, headless tours.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Semantics here are contract (IAnim.h notes); keep them in sync.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial null backend implementation.
*/

#include "engine/platform/anim/sources/null/CreateNullAnim.h"

namespace dfn::platform {
namespace {

class NullAnim final : public IAnim {
public:
    bool init() override { return true; }
    void shutdown() override {}

    SkeletonHandle load_skeleton(std::string_view path) override {
        (void)path;
        return SkeletonHandle{next_id_++};
    }
    void unload_skeleton(SkeletonHandle skeleton) override { (void)skeleton; }

    ClipHandle load_clip(SkeletonHandle skeleton, std::string_view path) override {
        (void)skeleton;
        (void)path;
        return ClipHandle{next_id_++};
    }
    void unload_clip(ClipHandle clip) override { (void)clip; }

    uint32_t joint_count(SkeletonHandle skeleton) const override {
        (void)skeleton;
        return 0; // contract: null skeletons expose no joints
    }
    float clip_duration(ClipHandle clip) const override {
        (void)clip;
        return 0.0f;
    }

    AnimInstanceHandle create_instance(SkeletonHandle skeleton) override {
        (void)skeleton;
        return AnimInstanceHandle{next_id_++};
    }
    void destroy_instance(AnimInstanceHandle instance) override { (void)instance; }

    bool evaluate(AnimInstanceHandle instance, std::span<const AnimLayer> layers,
                  std::span<glm::mat4> out_skinning) override {
        (void)instance;
        (void)layers;
        // Contract: fill the whole span with identity (bind pose) and succeed.
        for (auto& matrix : out_skinning) {
            matrix = glm::mat4{1.0f};
        }
        return true;
    }

private:
    uint32_t next_id_ = 1; // 0 is the invalid handle
};

} // namespace

std::unique_ptr<IAnim> create_null_anim() {
    return std::make_unique<NullAnim>();
}

} // namespace dfn::platform
