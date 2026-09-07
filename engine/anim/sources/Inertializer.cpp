/*
Module: engine/anim
File: engine/anim/sources/Inertializer.cpp

Responsibility:
- Реализация инерциализации стыков (Inertializer.h): подгонка квинтики и
  разница ориентаций/смещений по суставам.

Key items:
- Quintic::fit(): коэффициенты по Болло; a0 зажат снизу нулём, t1 — до
  −5·x0/v0 при закрывающейся разнице.
- Inertializer::capture(): q_off = q_last · q_fresh⁻¹ (кратчайшая, w ≥ 0), ось и
  угол; скорость — по углу разницы позапрошлой позы на той же оси.

Dependencies:
- Uses: Inertializer.h, glm.
- Used by: SkinnedCharacter.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Разница накладывается СЛЕВА: q = angleAxis(x(t), axis) · q_new — так при
  t = 0 получается ровно q_last.
*/
#include "engine/anim/sources/Inertializer.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {

Quintic Quintic::fit(float x0, float v0, float t1) {
    Quintic q;
    q.x0 = x0;
    q.v0 = v0;
    if (t1 <= 0.0f) {
        q.t1 = 0.0f;
        return q;
    }
    if (v0 < 0.0f && x0 > 0.0f) {
        t1 = std::min(t1, -5.0f * x0 / v0);
    }
    q.t1 = t1;
    const float t2 = t1 * t1;
    const float t3 = t2 * t1;
    const float t4 = t3 * t1;
    const float t5 = t4 * t1;
    q.a0 = std::max(0.0f, (-8.0f * v0 * t1 - 20.0f * x0) / t2);
    q.a = -(q.a0 * t2 + 6.0f * v0 * t1 + 12.0f * x0) / (2.0f * t5);
    q.b = (3.0f * q.a0 * t2 + 16.0f * v0 * t1 + 30.0f * x0) / (2.0f * t4);
    q.c = -(3.0f * q.a0 * t2 + 12.0f * v0 * t1 + 20.0f * x0) / (2.0f * t3);
    return q;
}

float Quintic::at(float t) const {
    if (t1 <= 0.0f || t >= t1) {
        return 0.0f;
    }
    if (t <= 0.0f) {
        return x0;
    }
    const float t2 = t * t;
    const float t3 = t2 * t;
    return a * t3 * t2 + b * t2 * t2 + c * t3 + 0.5f * a0 * t2 + v0 * t + x0;
}

namespace {

/// Кратчайшая разница q_from · q_to⁻¹ (w ≥ 0).
glm::quat offset_of(const glm::quat& from, const glm::quat& to) {
    glm::quat q = glm::normalize(from) * glm::inverse(glm::normalize(to));
    if (q.w < 0.0f) {
        q = -q;
    }
    return q;
}

} // namespace

void Inertializer::capture(std::span<const JointLocal> older, std::span<const JointLocal> last,
                           std::span<const JointLocal> fresh, float dt, float blend) {
    const std::size_t n = std::min({older.size(), last.size(), fresh.size()});
    tracks.assign(n, InertialTrack{});
    t_s = 0.0f;
    blend_s = blend;
    if (blend <= 0.0f || dt <= 0.0f) {
        blend_s = 0.0f;
        return;
    }
    constexpr float EPS_ANGLE = 1.0e-4f; // рад
    constexpr float EPS_POS = 1.0e-5f;   // м
    for (std::size_t j = 0; j < n; ++j) {
        InertialTrack& tr = tracks[j];
        // ОРИЕНТАЦИЯ
        {
            const glm::quat off = offset_of(last[j].rotation, fresh[j].rotation);
            const glm::quat off_prev = offset_of(older[j].rotation, fresh[j].rotation);
            const glm::vec3 v{off.x, off.y, off.z};
            const glm::vec3 vp{off_prev.x, off_prev.y, off_prev.z};
            const float s = glm::length(v);
            const float sp = glm::length(vp);
            glm::vec3 axis{0.0f};
            if (s > 1.0e-6f) {
                axis = v / s;
            } else if (sp > 1.0e-6f) {
                axis = vp / sp;
            }
            if (glm::length(axis) > 0.5f) {
                const float x0 = 2.0f * std::atan2(glm::dot(v, axis), off.w);
                const float xm1 = 2.0f * std::atan2(glm::dot(vp, axis), off_prev.w);
                const float v0 = (x0 - xm1) / dt;
                if (std::abs(x0) > EPS_ANGLE || std::abs(v0) * blend > EPS_ANGLE) {
                    tr.axis = axis;
                    tr.rot = Quintic::fit(x0, v0, blend);
                    tr.has_rot = true;
                }
            }
        }
        // СМЕЩЕНИЕ
        {
            const glm::vec3 d = last[j].translation - fresh[j].translation;
            const glm::vec3 dp = older[j].translation - fresh[j].translation;
            const float x0 = glm::length(d);
            glm::vec3 dir{0.0f};
            if (x0 > 1.0e-7f) {
                dir = d / x0;
            } else if (glm::length(dp) > 1.0e-7f) {
                dir = glm::normalize(dp);
            }
            if (glm::length(dir) > 0.5f) {
                const float xm1 = glm::dot(dp, dir);
                const float v0 = (x0 - xm1) / dt;
                if (x0 > EPS_POS || std::abs(v0) * blend > EPS_POS) {
                    tr.dir = dir;
                    tr.pos = Quintic::fit(x0, v0, blend);
                    tr.has_pos = true;
                }
            }
        }
    }
}

float Inertializer::weight() const {
    if (!active()) {
        return 0.0f;
    }
    return Quintic::fit(1.0f, 0.0f, blend_s).at(t_s);
}

void Inertializer::apply(float t, std::span<JointLocal> pose) const {
    if (blend_s <= 0.0f || t >= blend_s) {
        return;
    }
    const std::size_t n = std::min(tracks.size(), pose.size());
    for (std::size_t j = 0; j < n; ++j) {
        const InertialTrack& tr = tracks[j];
        if (tr.has_rot) {
            const float x = tr.rot.at(t);
            if (x != 0.0f) {
                pose[j].rotation = glm::normalize(glm::angleAxis(x, tr.axis)
                                                  * glm::normalize(pose[j].rotation));
            }
        }
        if (tr.has_pos) {
            pose[j].translation += tr.dir * tr.pos.at(t);
        }
    }
}

} // namespace dfn::anim
