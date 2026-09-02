/*
Module: engine/anim
File: engine/anim/sources/BodyGaps.cpp

Responsibility:
- Implements the body-gap instrument: skin labelling by rig bone, the signed
  lateral gap by height bands, the vertex-pair minimum and the box pairs.

Dependencies:
- Uses: BodyGaps.h, Hitbox.h (hitbox_pose, hitbox_pair_distance), SkinnedBody
  (cpu_skin_position, sample_palette), generated constants (REST_GAP_*).
- Used by: dfn_anim (RestFit), engine/app, tests, tools.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here reads a clock, a file or the ECS.
*/

#include "engine/anim/sources/BodyGaps.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dfn::anim {
namespace {

/// ШИРИНА ПОЛОСЫ ВЫСОТЫ бокового замера. Сантиметр: вершины бедра на этом
/// теле стоят в 1-3 см друг от друга, и полоса уже сантиметра часто пуста с
/// одной из сторон; шире двух — и край, взятый по полосе, сглаживает
/// сантиметровый заход одной части в другую.
constexpr float BAND_M = 0.01f;

/// НАСКОЛЬКО ЧАСТИ ДОЛЖНЫ ПЕРЕКРЫВАТЬСЯ ПО ГЛУБИНЕ в полосе, чтобы полоса
/// считалась «бок о бок». Отрицательный допуск — разрешить сантиметровый
/// просвет по z: кисть, висящая чуть впереди бедра, всё ещё стоит рядом с ним.
constexpr float DEPTH_SLACK_M = 0.01f;

struct Extremes {
    float min_x = std::numeric_limits<float>::infinity();
    float max_x = -std::numeric_limits<float>::infinity();
    float min_z = std::numeric_limits<float>::infinity();
    float max_z = -std::numeric_limits<float>::infinity();
    [[nodiscard]] bool any() const { return min_x <= max_x; }
};

/// БОКОВОЙ ЗНАКОВЫЙ ЗАЗОР: `inner` стоит ближе к оси тела, `outer` — дальше
/// по +X. `sign` = +1, когда наружная часть справа (+X), -1 — слева.
[[nodiscard]] MeshGap lateral_gap(const std::vector<glm::vec3>& pos,
                                  const std::vector<uint32_t>& inner,
                                  const std::vector<uint32_t>& outer, float sign) {
    MeshGap g;
    if (inner.empty() || outer.empty()) {
        return g;
    }
    float lo = std::numeric_limits<float>::infinity();
    float hi = -std::numeric_limits<float>::infinity();
    for (const uint32_t i : inner) {
        lo = std::min(lo, pos[i].y);
        hi = std::max(hi, pos[i].y);
    }
    float lo2 = std::numeric_limits<float>::infinity();
    float hi2 = -std::numeric_limits<float>::infinity();
    for (const uint32_t i : outer) {
        lo2 = std::min(lo2, pos[i].y);
        hi2 = std::max(hi2, pos[i].y);
    }
    lo = std::max(lo, lo2);
    hi = std::min(hi, hi2);
    if (hi <= lo) {
        return g; // no shared height at all
    }
    const int bands = static_cast<int>((hi - lo) / BAND_M) + 1;
    std::vector<Extremes> a(static_cast<std::size_t>(bands));
    std::vector<Extremes> b(static_cast<std::size_t>(bands));
    const auto fill = [&](const std::vector<uint32_t>& idx, std::vector<Extremes>& out) {
        for (const uint32_t i : idx) {
            const glm::vec3 p = pos[i];
            if (p.y < lo || p.y > hi) {
                continue;
            }
            const int k = std::clamp(static_cast<int>((p.y - lo) / BAND_M), 0, bands - 1);
            Extremes& e = out[static_cast<std::size_t>(k)];
            // X is read in the OUTER side's sense, so "inner edge of the outer
            // part" is always its minimum and the maths below has one sign.
            const float x = p.x * sign;
            e.min_x = std::min(e.min_x, x);
            e.max_x = std::max(e.max_x, x);
            e.min_z = std::min(e.min_z, p.z);
            e.max_z = std::max(e.max_z, p.z);
        }
    };
    fill(inner, a);
    fill(outer, b);
    float worst = std::numeric_limits<float>::infinity();
    for (int k = 0; k < bands; ++k) {
        const Extremes& ia = a[static_cast<std::size_t>(k)];
        const Extremes& ob = b[static_cast<std::size_t>(k)];
        if (!ia.any() || !ob.any()) {
            continue;
        }
        // Side by side means overlapping in depth; a hand held out in front
        // of the thigh shares its height and is not beside it.
        if (ia.max_z < ob.min_z - DEPTH_SLACK_M || ob.max_z < ia.min_z - DEPTH_SLACK_M) {
            continue;
        }
        const float gap = ob.min_x - ia.max_x;
        if (gap < worst) {
            worst = gap;
            g.worst_y = lo + (float(k) + 0.5f) * BAND_M;
        }
        ++g.bands;
    }
    if (g.bands > 0) {
        g.lateral_m = worst;
    }
    return g;
}

[[nodiscard]] float pair_min(const std::vector<glm::vec3>& pos,
                             const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b) {
    float best = std::numeric_limits<float>::infinity();
    for (const uint32_t i : a) {
        const glm::vec3 p = pos[i];
        for (const uint32_t j : b) {
            const glm::vec3 d = pos[j] - p;
            best = std::min(best, glm::dot(d, d));
        }
    }
    return std::sqrt(best);
}

[[nodiscard]] std::vector<uint32_t> join(const std::vector<uint32_t>& a,
                                         const std::vector<uint32_t>& b) {
    std::vector<uint32_t> out;
    out.reserve(a.size() + b.size());
    out.insert(out.end(), a.begin(), a.end());
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

} // namespace

float MeshGap::judged_m() const {
    return std::isnan(lateral_m) ? pair_m : lateral_m;
}

float BodyGaps::hand_thigh_worst_m() const {
    return std::min(hand_thigh[0].judged_m(), hand_thigh[1].judged_m());
}

float BodyGaps::forearm_trunk_worst_m() const {
    return std::min(forearm_trunk[0].judged_m(), forearm_trunk[1].judged_m());
}

SkinParts label_skin_parts(const skel::Skeleton& skeleton,
                           const SkinnedRigBinding& binding,
                           std::span<const platform::SkinnedVertex> vertices) {
    SkinParts parts;
    const std::size_t n = skeleton.size();
    if (n == 0 || vertices.empty()) {
        return parts;
    }
    std::vector<int> bone_of(n, -1);
    for (uint32_t b = 0; b < BONE_COUNT; ++b) {
        const int32_t j = binding.names.joint[b];
        if (j >= 0 && static_cast<std::size_t>(j) < n) {
            bone_of[static_cast<std::size_t>(j)] = static_cast<int>(b);
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        if (bone_of[j] < 0) {
            const int32_t par = skeleton.joints[j].parent;
            if (par >= 0 && static_cast<std::size_t>(par) < j) {
                bone_of[j] = bone_of[static_cast<std::size_t>(par)];
            }
        }
    }
    parts.bone_of_vertex.assign(vertices.size(), -1);
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const platform::SkinnedVertex& v = vertices[i];
        int best = -1;
        float bw = -1.0f;
        for (int k = 0; k < 4; ++k) {
            if (v.weights[k] > bw) {
                bw = v.weights[k];
                best = static_cast<int>(v.joints[k]);
            }
        }
        const int bone = best >= 0 && static_cast<std::size_t>(best) < n
                             ? bone_of[static_cast<std::size_t>(best)]
                             : -1;
        parts.bone_of_vertex[i] = static_cast<int8_t>(bone);
        if (bone >= 0) {
            parts.by_bone[static_cast<std::size_t>(bone)].push_back(static_cast<uint32_t>(i));
        }
    }
    return parts;
}

BodyGaps measure_body_gaps(const skel::Skeleton& skeleton,
                           const SkinnedRigBinding& binding, const HitboxSet& boxes,
                           std::span<const platform::SkinnedVertex> vertices,
                           const SkinParts& parts, std::span<const JointLocal> sample) {
    BodyGaps g;
    const std::size_t n = skeleton.size();
    if (n == 0 || vertices.empty() || !parts.valid() || sample.size() < n
        || parts.bone_of_vertex.size() != vertices.size()) {
        return g;
    }
    std::vector<glm::mat4> palette(n);
    sample_palette(skeleton, sample, palette);
    std::vector<glm::vec3> pos(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        pos[i] = cpu_skin_position(vertices[i], palette);
    }
    const auto of = [&](Bone b) -> const std::vector<uint32_t>& {
        return parts.by_bone[bone_index(b)];
    };
    const std::vector<uint32_t> leg_l = join(of(Bone::ThighL), of(Bone::ShinL));
    const std::vector<uint32_t> leg_r = join(of(Bone::ThighR), of(Bone::ShinR));
    const std::vector<uint32_t> trunk = join(of(Bone::Pelvis), of(Bone::Torso));

    // THE LEFT LEG IS AT -X (docs/RIG.md), so the right leg is the outer part.
    g.legs = lateral_gap(pos, leg_l, leg_r, 1.0f);
    g.legs.pair_m = pair_min(pos, leg_l, leg_r);
    // The hand hangs OUTSIDE the thigh; on the left side outside is -X.
    g.hand_thigh[0] = lateral_gap(pos, of(Bone::ThighL), of(Bone::HandL), -1.0f);
    g.hand_thigh[0].pair_m = pair_min(pos, of(Bone::HandL), of(Bone::ThighL));
    g.hand_thigh[1] = lateral_gap(pos, of(Bone::ThighR), of(Bone::HandR), 1.0f);
    g.hand_thigh[1].pair_m = pair_min(pos, of(Bone::HandR), of(Bone::ThighR));
    g.forearm_trunk[0] = lateral_gap(pos, trunk, of(Bone::ForearmL), -1.0f);
    g.forearm_trunk[0].pair_m = pair_min(pos, of(Bone::ForearmL), trunk);
    g.forearm_trunk[1] = lateral_gap(pos, trunk, of(Bone::ForearmR), 1.0f);
    g.forearm_trunk[1].pair_m = pair_min(pos, of(Bone::ForearmR), trunk);

    const HitboxPose hp = hitbox_pose(boxes, skeleton, binding, sample);
    using P = BodyPart;
    const auto d = [&](P a, P b) { return hitbox_pair_distance(boxes, hp, a, b); };
    g.legs_box_m = std::min(d(P::ThighL, P::ThighR), d(P::ShinL, P::ShinR));
    g.hand_thigh_box_m = {d(P::HandL, P::ThighL), d(P::HandR, P::ThighR)};
    g.hand_hips_box_m = {d(P::HandL, P::Hips), d(P::HandR, P::Hips)};
    g.forearm_abdomen_box_m = {d(P::ForearmL, P::Abdomen), d(P::ForearmR, P::Abdomen)};
    g.valid = true;
    return g;
}

BodyGapTargets BodyGapTargets::from_config() {
    BodyGapTargets t;
    t.legs_m = static_cast<float>(config::REST_GAP_LEGS);
    t.hand_thigh_m = static_cast<float>(config::REST_GAP_HAND_THIGH);
    t.forearm_trunk_m = static_cast<float>(config::REST_GAP_FOREARM_TRUNK);
    return t;
}

bool gaps_meet(const BodyGaps& gaps, const BodyGapTargets& targets) {
    return gaps.valid && gaps.legs.judged_m() >= targets.legs_m
           && gaps.hand_thigh_worst_m() >= targets.hand_thigh_m
           && gaps.forearm_trunk_worst_m() >= targets.forearm_trunk_m;
}

std::string describe_gaps(const BodyGaps& g) {
    if (!g.valid) {
        return "зазоры: нет замера";
    }
    const auto cm = [](float m) {
        char b[24];
        if (std::isnan(m)) {
            std::snprintf(b, sizeof(b), "не рядом");
        } else if (std::isinf(m)) {
            std::snprintf(b, sizeof(b), "нет части");
        } else {
            std::snprintf(b, sizeof(b), "%+.2f", static_cast<double>(100.0f * m));
        }
        return std::string(b);
    };
    std::string s = "МЕШ, см: нога-нога " + cm(g.legs.lateral_m) + " (пары "
                    + cm(g.legs.pair_m) + "), кисть-бедро L " + cm(g.hand_thigh[0].lateral_m)
                    + " R " + cm(g.hand_thigh[1].lateral_m) + " (пары "
                    + cm(g.hand_thigh[0].pair_m) + " / " + cm(g.hand_thigh[1].pair_m)
                    + "), предплечье-корпус L " + cm(g.forearm_trunk[0].lateral_m) + " R "
                    + cm(g.forearm_trunk[1].lateral_m) + " (пары "
                    + cm(g.forearm_trunk[0].pair_m) + " / " + cm(g.forearm_trunk[1].pair_m)
                    + ") | КОРОБКИ, см: нога-нога " + cm(g.legs_box_m) + ", кисть-бедро "
                    + cm(g.hand_thigh_box_m[0]) + " / " + cm(g.hand_thigh_box_m[1])
                    + ", кисть-таз " + cm(g.hand_hips_box_m[0]) + " / "
                    + cm(g.hand_hips_box_m[1]) + ", предплечье-живот "
                    + cm(g.forearm_abdomen_box_m[0]) + " / " + cm(g.forearm_abdomen_box_m[1]);
    return s;
}

void bind_pose_sample(const skel::Skeleton& skeleton, std::span<JointLocal> out) {
    const std::size_t n = std::min(skeleton.size(), out.size());
    for (std::size_t j = 0; j < n; ++j) {
        out[j].translation = skeleton.joints[j].bind_translation;
        out[j].rotation = skeleton.joints[j].bind_rotation;
        out[j].scale = skeleton.joints[j].bind_scale;
    }
}

} // namespace dfn::anim
