/*
Module: engine/anim
File: engine/anim/sources/LookLayer.cpp

Responsibility:
- Реализация слоя взгляда (LookLayer.h): поиск звеньев, поворот вокруг
  вертикали в системе тела через модельные матрицы цепочки.

Key items:
- build_look_layer(): по именам DEF-spine.001…003, DEF-neck, DEF-head.
- apply_look(): для каждого звена локальная ориентация = R_parent⁻¹ · Ry(δ) ·
  R_parent · local, где δ — доля угла; матрицы родителей пересчитываются
  после каждого звена, чтобы верхние звенья крутились в уже повёрнутой цепи.

Dependencies:
- Uses: LookLayer.h, Constants.h, glm.
- Used by: ClipPlayer.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Рыск сим'а растёт по часовой, glm::rotate(+θ, Y) — против: знак обратный.
*/
#include "engine/anim/sources/LookLayer.h"

#include "engine/core/config/sources/Constants.h"

#include <cmath>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {

LookLayer build_look_layer(const skel::Skeleton& skeleton) {
    LookLayer layer;
    const char* names[5] = {"DEF-spine.001", "DEF-spine.002", "DEF-spine.003", "DEF-neck",
                            "DEF-head"};
    for (std::size_t i = 0; i < 5; ++i) {
        layer.chain[i] = skeleton.find(names[i]);
    }
    const float spine = static_cast<float>(config::LOOK_SHARE_SPINE);
    layer.share = {spine, spine, spine, static_cast<float>(config::LOOK_SHARE_NECK),
                   static_cast<float>(config::LOOK_SHARE_HEAD)};
    // звенья, которых нет, отдают долю голове — сумма остаётся единицей
    float lost = 0.0f;
    for (std::size_t i = 0; i < 4; ++i) {
        if (layer.chain[i] < 0) {
            lost += layer.share[i];
            layer.share[i] = 0.0f;
        }
    }
    layer.share[4] += lost;
    return layer;
}

namespace {

void model_rotations(const skel::Skeleton& skeleton, std::span<const JointLocal> sample,
                     std::vector<glm::quat>& out) {
    out.assign(skeleton.size(), glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
    for (std::size_t j = 0; j < skeleton.size() && j < sample.size(); ++j) {
        const int32_t p = skeleton.joints[j].parent;
        const glm::quat local = glm::normalize(sample[j].rotation);
        out[j] = p >= 0 ? glm::normalize(out[static_cast<std::size_t>(p)] * local) : local;
    }
}

} // namespace

void apply_look(const skel::Skeleton& skeleton, const LookLayer& layer, float yaw, float weight,
                std::span<JointLocal> sample) {
    if (!layer.valid() || sample.size() < skeleton.size() || weight <= 0.0f
        || std::abs(yaw) < 1.0e-5f) {
        return;
    }
    std::vector<glm::quat> model;
    for (std::size_t i = 0; i < 5; ++i) {
        const int32_t j = layer.chain[i];
        if (j < 0 || layer.share[i] <= 0.0f) {
            continue;
        }
        model_rotations(skeleton, sample, model);
        const int32_t p = skeleton.joints[static_cast<std::size_t>(j)].parent;
        const glm::quat parent = p >= 0 ? model[static_cast<std::size_t>(p)]
                                        : glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
        // рыск сим'а по часовой = glm-поворот на −угол вокруг +Y
        const glm::quat turn = glm::angleAxis(-yaw * layer.share[i] * weight,
                                              glm::vec3{0.0f, 1.0f, 0.0f});
        JointLocal& jl = sample[static_cast<std::size_t>(j)];
        jl.rotation = glm::normalize(glm::inverse(parent) * turn * parent
                                     * glm::normalize(jl.rotation));
    }
}

} // namespace dfn::anim
