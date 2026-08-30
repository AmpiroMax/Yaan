/*
Module: engine/core
File: engine/core/skeleton/sources/Skeleton.cpp

Responsibility:
- Implements the two pure functions of the imported-skeleton contract:
  local-TRS forward kinematics and keyframe sampling.

Dependencies:
- Uses: Skeleton.h, glm.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Sampling is LINEAR by contract (glTF's default interpolation). CUBICSPLINE
  inputs are converted at IMPORT time, never guessed at here: a sampler that
  silently reads spline tangents as values produces motion that is wrong in a
  way no test of this file can see.
*/

#include "engine/core/skeleton/sources/Skeleton.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace dfn::skel {
namespace {

/// Where `t` falls in a strictly increasing key list: the index of the key at
/// or before it, plus the fraction to the next. Clamped at both ends, so a
/// clip queried outside its own span HOLDS rather than extrapolating.
struct KeySpan {
    std::size_t lo = 0;
    std::size_t hi = 0;
    float frac = 0.0f;
};

[[nodiscard]] KeySpan locate(const std::vector<float>& times, float t) {
    KeySpan s;
    if (times.size() <= 1) {
        return s;
    }
    if (t <= times.front()) {
        return s;
    }
    if (t >= times.back()) {
        s.lo = times.size() - 1;
        s.hi = s.lo;
        return s;
    }
    const auto it = std::upper_bound(times.begin(), times.end(), t);
    s.hi = static_cast<std::size_t>(it - times.begin());
    s.lo = s.hi - 1;
    const float span = times[s.hi] - times[s.lo];
    s.frac = span > 0.0f ? (t - times[s.lo]) / span : 0.0f;
    return s;
}

} // namespace

void skeleton_bind_local(const Skeleton& skeleton, std::span<glm::mat4> out) {
    const std::size_t n = std::min(skeleton.joints.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        const SkeletonJoint& j = skeleton.joints[i];
        out[i] = glm::translate(glm::mat4{1.0f}, j.bind_translation)
                 * glm::mat4_cast(j.bind_rotation)
                 * glm::scale(glm::mat4{1.0f}, j.bind_scale);
    }
}

void skeleton_model_matrices(const Skeleton& skeleton, std::span<const glm::mat4> local,
                             std::span<glm::mat4> out) {
    const std::size_t n =
        std::min({skeleton.joints.size(), local.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const int32_t parent = skeleton.joints[i].parent;
        // Parents precede children by contract, so the parent's model matrix
        // is already final. A file that breaks that ordering would read its
        // own uninitialised slot -- refused at import, never patched here.
        out[i] = parent >= 0 && static_cast<std::size_t>(parent) < i
                     ? out[static_cast<std::size_t>(parent)] * local[i]
                     : local[i];
    }
}

void sample_clip(const Skeleton& skeleton, const AnimClip& clip, float time_s,
                 std::span<glm::vec3> translation, std::span<glm::quat> rotation,
                 std::span<glm::vec3> scale) {
    const std::size_t n = std::min({skeleton.joints.size(), translation.size(),
                                    rotation.size(), scale.size()});
    for (std::size_t i = 0; i < n; ++i) {
        translation[i] = skeleton.joints[i].bind_translation;
        rotation[i] = skeleton.joints[i].bind_rotation;
        scale[i] = skeleton.joints[i].bind_scale;
    }
    for (const AnimChannel& ch : clip.channels) {
        if (ch.joint >= n || ch.times.empty()
            || ch.times.size() != ch.values.size()) {
            continue;
        }
        const KeySpan s = locate(ch.times, time_s);
        const glm::vec4 a = ch.values[s.lo];
        const glm::vec4 b = ch.values[s.hi];
        switch (ch.path) {
        case AnimPath::Translation:
            translation[ch.joint] = glm::vec3{a} + (glm::vec3{b} - glm::vec3{a}) * s.frac;
            break;
        case AnimPath::Scale:
            scale[ch.joint] = glm::vec3{a} + (glm::vec3{b} - glm::vec3{a}) * s.frac;
            break;
        case AnimPath::Rotation: {
            // glTF stores (x,y,z,w); glm::quat's constructor takes (w,x,y,z).
            const glm::quat qa{a.w, a.x, a.y, a.z};
            const glm::quat qb{b.w, b.x, b.y, b.z};
            rotation[ch.joint] = glm::normalize(glm::slerp(qa, qb, s.frac));
            break;
        }
        }
    }
}

void sample_clip_model_matrices(const Skeleton& skeleton, const AnimClip& clip,
                                float time_s, std::span<glm::mat4> out) {
    const std::size_t n = skeleton.joints.size();
    std::vector<glm::vec3> t(n);
    std::vector<glm::quat> r(n);
    std::vector<glm::vec3> s(n);
    sample_clip(skeleton, clip, time_s, t, r, s);
    std::vector<glm::mat4> local(n);
    for (std::size_t i = 0; i < n; ++i) {
        local[i] = glm::translate(glm::mat4{1.0f}, t[i]) * glm::mat4_cast(r[i])
                   * glm::scale(glm::mat4{1.0f}, s[i]);
    }
    skeleton_model_matrices(skeleton, local, out);
}

} // namespace dfn::skel
