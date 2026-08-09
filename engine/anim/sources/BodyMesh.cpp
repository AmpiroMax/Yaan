/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 01:56:45
Module: engine/anim
File: engine/anim/sources/BodyMesh.cpp

Responsibility:
- Builds the per-bone segment boxes (flat-shaded, hard edges, low-res read).

Dependencies:
- Uses: BodyMesh.h, glm.
- Used by: app ferry, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep pure/deterministic; sizes only from RigProportions (which come from
  NUMBERS rows) — no literal meters here except authored color/inset ratios.
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial implementation.
*/

#include "engine/anim/sources/BodyMesh.h"

#include <algorithm>

namespace dfn::anim {

namespace {

// Placeholder palette (ASSET data; flat colors read well after quantization).
constexpr glm::vec3 SKIN{0.78f, 0.62f, 0.48f};
constexpr glm::vec3 TUNIC{0.42f, 0.34f, 0.24f};   // worn leather brown
constexpr glm::vec3 SLEEVE{0.48f, 0.40f, 0.30f};  // a shade lighter than tunic
constexpr glm::vec3 TROUSER{0.30f, 0.30f, 0.34f}; // grey wool
constexpr glm::vec3 BOOT{0.20f, 0.16f, 0.12f};

// Shape ratios (asset data): forearm/shin taper, hand/foot slimming.
constexpr float FOREARM_TAPER = 0.85f;
constexpr float SHIN_TAPER = 0.80f;
constexpr float HAND_WIDTH_RATIO = 0.9f;
constexpr float HAND_DEPTH_RATIO = 0.5f;
constexpr float FOOT_HEEL_RATIO = 0.25f; // of foot length behind the ankle
constexpr float PELVIS_HEIGHT_RATIO = 0.09f; // of torso length, half up half down
constexpr float TORSO_GAP_RATIO = 0.06f;     // of torso length above the pelvis box
constexpr float HEAD_DEPTH_RATIO = 1.15f;    // skull is deeper than wide

[[nodiscard]] uint32_t pack(const glm::vec3& c) {
    const auto to8 = [](float v) {
        return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return 0xFF000000u | (to8(c.b) << 16) | (to8(c.g) << 8) | to8(c.r);
}

void quad(BodySegmentMesh& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
          uint32_t color) {
    // Flat-shaded: no shared vertices; normal from the winding (CCW outside).
    const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    const auto base = static_cast<uint32_t>(m.vertices.size());
    for (const glm::vec3& pos : {a, b, c, d}) {
        m.vertices.push_back({pos, n, {0.0f, 0.0f}, color});
    }
    m.indices.insert(m.indices.end(),
                     {base, base + 1, base + 2, base, base + 2, base + 3});
}

// Axis-aligned box [lo, hi] with CCW-outside faces.
void box(BodySegmentMesh& m, glm::vec3 lo, glm::vec3 hi, uint32_t c) {
    const glm::vec3 v000{lo.x, lo.y, lo.z}, v100{hi.x, lo.y, lo.z};
    const glm::vec3 v010{lo.x, hi.y, lo.z}, v110{hi.x, hi.y, lo.z};
    const glm::vec3 v001{lo.x, lo.y, hi.z}, v101{hi.x, lo.y, hi.z};
    const glm::vec3 v011{lo.x, hi.y, hi.z}, v111{hi.x, hi.y, hi.z};
    quad(m, v001, v101, v111, v011, c); // +Z
    quad(m, v100, v000, v010, v110, c); // -Z
    quad(m, v101, v100, v110, v111, c); // +X
    quad(m, v000, v001, v011, v010, c); // -X
    quad(m, v011, v111, v110, v010, c); // +Y
    quad(m, v000, v100, v101, v001, c); // -Y
    m.bounds_min = glm::min(m.bounds_min, lo);
    m.bounds_max = glm::max(m.bounds_max, hi);
}

// A limb box hanging down -Y from the origin joint, square cross-section.
void limb(BodySegmentMesh& m, float length, float thickness, uint32_t c,
          float depth_scale = 1.0f) {
    const float h = thickness * 0.5f;
    const float hz = h * depth_scale;
    box(m, {-h, -length, -hz}, {h, 0.0f, hz}, c);
}

} // namespace

BodySegmentMesh build_body_segment_mesh(Bone bone, const RigProportions& p) {
    BodySegmentMesh m;
    m.bounds_min = glm::vec3{1e9f};
    m.bounds_max = glm::vec3{-1e9f};

    const float torso_len = p.torso_length();
    switch (bone) {
    case Bone::Pelvis: {
        const float hh = torso_len * PELVIS_HEIGHT_RATIO;
        box(m, {-p.hip_width * 0.5f, -hh, -p.torso_depth * 0.45f},
            {p.hip_width * 0.5f, hh, p.torso_depth * 0.45f}, pack(TROUSER));
        break;
    }
    case Bone::Torso: {
        const float y0 = torso_len * (PELVIS_HEIGHT_RATIO + TORSO_GAP_RATIO);
        box(m, {-p.shoulder_width * 0.5f, y0, -p.torso_depth * 0.5f},
            {p.shoulder_width * 0.5f, torso_len, p.torso_depth * 0.5f}, pack(TUNIC));
        break;
    }
    case Bone::Head: {
        const float hw = p.head_width * 0.5f;
        const float hd = hw * HEAD_DEPTH_RATIO;
        box(m, {-hw, 0.0f, -hd}, {hw, p.head_height, hd}, pack(SKIN));
        break;
    }
    case Bone::UpperArmL:
    case Bone::UpperArmR:
        limb(m, p.upper_arm_length, p.arm_thickness, pack(SLEEVE));
        break;
    case Bone::ForearmL:
    case Bone::ForearmR:
        limb(m, p.forearm_length, p.arm_thickness * FOREARM_TAPER, pack(SKIN));
        break;
    case Bone::HandL:
    case Bone::HandR: {
        const float w = p.arm_thickness * FOREARM_TAPER * HAND_WIDTH_RATIO;
        limb(m, p.hand_length, w, pack(SKIN), HAND_DEPTH_RATIO);
        break;
    }
    case Bone::ThighL:
    case Bone::ThighR:
        limb(m, p.thigh_length(), p.leg_thickness, pack(TROUSER));
        break;
    case Bone::ShinL:
    case Bone::ShinR:
        limb(m, p.shin_length(), p.leg_thickness * SHIN_TAPER, pack(BOOT));
        break;
    case Bone::FootL:
    case Bone::FootR: {
        // Origin at the ankle; sole at -ankle_height; toes forward (-Z).
        const float h = p.leg_thickness * SHIN_TAPER * 0.5f;
        const float heel = p.foot_length * FOOT_HEEL_RATIO;
        box(m, {-h, -p.ankle_height, -(p.foot_length - heel)},
            {h, 0.0f, heel}, pack(BOOT));
        break;
    }
    }
    return m;
}

} // namespace dfn::anim
