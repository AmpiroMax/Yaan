/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 12:10:00
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
- 10:08:2026 - 12:10:00: Bevelled tapered prisms replace the boxes (user: more rounded); the torso stops at the shoulder line with a neck stub (user: too much chest); calf tapers to a real ankle so the closed stance cannot merge the legs.
*/

#include "engine/anim/sources/BodyMesh.h"

#include <algorithm>
#include <array>

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

// ROUNDNESS (user note 10:08:2026: «чуть геометрию тела загругленной сделать»).
// The segments were boxes, and a box has a four-sided silhouette that reads as
// a prop rather than as a person. These cut the four VERTICAL corners off each
// segment, turning the cross-section into an octagon — the biggest change in
// silhouette for the fewest triangles at 640x360, and it stops at "carved
// figure" rather than going on to "smooth mannequin" (the look is
// Daggerfall-chunky by user ruling). Fraction of the half-extent removed at
// each corner; 0 = the old box, 0.5 = a diamond.
constexpr float BEVEL_HEAD = 0.30f;
constexpr float BEVEL_LIMB = 0.30f;
constexpr float BEVEL_TORSO = 0.22f; // trunk stays the blockiest mass
constexpr float BEVEL_PELVIS = 0.25f;
constexpr float BEVEL_BOOT = 0.15f;  // boots are meant to look hard
// Distal/proximal width ratio along a limb: a forearm is not a prism.
constexpr float LIMB_TAPER = 0.85f;
// Calf -> ankle. Real leg widths run ~0.11 m at the knee to ~0.07 at the ankle;
// against leg_thickness*SHIN_TAPER this is that ratio, and it is what lets the
// closed stance exist at all (see the ShinL/R case).
constexpr float SHIN_ANKLE_TAPER = 0.58f;
// Foot breadth against thigh diameter (~0.095 m on a 1.8 m body).
constexpr float FOOT_WIDTH_RATIO = 0.62f;
// Trunk taper: the torso is a V, hips narrower than shoulders.
constexpr float TORSO_HIP_RATIO = 0.80f;
// Neck stub: fraction of head width, and it replaces the full-depth torso slab
// that used to run all the way up to the neck (see NECK_LINE below).
constexpr float NECK_WIDTH_RATIO = 0.62f;

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

void tri(BodySegmentMesh& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, uint32_t color) {
    const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    const auto base = static_cast<uint32_t>(m.vertices.size());
    for (const glm::vec3& pos : {a, b, c}) {
        m.vertices.push_back({pos, n, {0.0f, 0.0f}, color});
    }
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2});
}

// The eight XZ corners of one ring: a rectangle with its four corners cut.
// bevel is the fraction of each half-extent removed at the corner, so bevel 0
// reproduces the rectangle exactly (each corner degenerates to a repeated
// point, which the winding tolerates) and 0.5 gives a diamond.
[[nodiscard]] std::array<glm::vec2, 8> ring(float hx, float hz, float bevel) {
    const float cx = hx * bevel;
    const float cz = hz * bevel;
    return {{{hx - cx, hz},  {cx - hx, hz},  {-hx, hz - cz}, {-hx, cz - hz},
             {cx - hx, -hz}, {hx - cx, -hz}, {hx, cz - hz},  {hx, hz - cz}}};
}

// A bevelled, tapered prism spanning y0..y1, with its own half-extents at each
// end. This is the one primitive every segment is now built from: the old box
// is the bevel-0, taper-1 case of it.
void prism(BodySegmentMesh& m, float y0, float y1, float hx0, float hz0, float hx1,
           float hz1, float bevel, uint32_t c, float z_shift = 0.0f) {
    const auto lo = ring(hx0, hz0, bevel);
    const auto hi = ring(hx1, hz1, bevel);
    const auto at = [z_shift](const glm::vec2& p, float y) {
        return glm::vec3{p.x, y, p.y + z_shift};
    };
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t j = (i + 1) % 8;
        if (lo[i] == lo[j] && hi[i] == hi[j]) {
            continue; // degenerate seam of an unbevelled corner
        }
        quad(m, at(lo[i], y0), at(lo[j], y0), at(hi[j], y1), at(hi[i], y1), c);
    }
    // Caps as fans from corner 0; flat-shaded, so no vertices are shared.
    for (uint32_t i = 1; i + 1 < 8; ++i) {
        if (hi[0] != hi[i] && hi[i] != hi[i + 1]) {
            tri(m, at(hi[0], y1), at(hi[i], y1), at(hi[i + 1], y1), c);
        }
        if (lo[0] != lo[i] && lo[i] != lo[i + 1]) {
            tri(m, at(lo[0], y0), at(lo[i + 1], y0), at(lo[i], y0), c);
        }
    }
    m.bounds_min = glm::min(m.bounds_min,
                            glm::vec3{-std::max(hx0, hx1), std::min(y0, y1),
                                      -std::max(hz0, hz1) + z_shift});
    m.bounds_max = glm::max(m.bounds_max,
                            glm::vec3{std::max(hx0, hx1), std::max(y0, y1),
                                      std::max(hz0, hz1) + z_shift});
}

// A limb hanging down -Y from the origin joint, tapering toward the distal end.
void limb(BodySegmentMesh& m, float length, float thickness, uint32_t c,
          float depth_scale = 1.0f, float bevel = BEVEL_LIMB,
          float taper = LIMB_TAPER) {
    const float h = thickness * 0.5f;
    prism(m, -length, 0.0f, h * taper, h * taper * depth_scale, h,
          h * depth_scale, bevel, c);
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
        const float hx = p.hip_width * 0.5f;
        const float hz = p.torso_depth * 0.45f;
        prism(m, -hh, hh, hx, hz, hx, hz, BEVEL_PELVIS, pack(TROUSER));
        break;
    }
    case Bone::Torso: {
        // THE CHEST STOPS AT THE COLLARBONE (user note 10:08:2026: «сейчас
        // сильно много груди видно»). The slab used to run at full depth all
        // the way to the NECK — 0.134 m below the eye — so looking down met a
        // full-depth wall where a real body has a neck rising from a shoulder
        // line. Ending it at BODY_SHOULDER_HEIGHT_FRAC and continuing with a
        // narrow neck stub drops the visible chest edge from 79.0 to 83.5 deg
        // of depression and takes the foot's visibility threshold from 0.430 m
        // of swing to 0.294 m against a maximum reach of 0.486 — a foot on
        // screen across 59 % of the stride where it used to be 31 %.
        const float y0 = torso_len * (PELVIS_HEIGHT_RATIO + TORSO_GAP_RATIO);
        const float neck_line = p.shoulder_height - p.hip_height;
        const float sx = p.shoulder_width * 0.5f;
        const float sz = p.torso_depth * 0.5f;
        prism(m, y0, neck_line, sx * TORSO_HIP_RATIO, sz * TORSO_HIP_RATIO, sx, sz,
              BEVEL_TORSO, pack(TUNIC));
        const float nx = p.head_width * NECK_WIDTH_RATIO * 0.5f;
        prism(m, neck_line, torso_len, nx, nx, nx, nx, BEVEL_LIMB, pack(SKIN));
        break;
    }
    case Bone::Head: {
        const float hw = p.head_width * 0.5f;
        const float hd = hw * HEAD_DEPTH_RATIO;
        // Jaw slightly narrower than the crown; the bevel does the rest.
        prism(m, 0.0f, p.head_height, hw * 0.88f, hd * 0.94f, hw, hd, BEVEL_HEAD,
              pack(SKIN));
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
        // Calf into ankle. This taper is load-bearing, not decoration: with
        // the stance closed to BODY_STANCE_WIDTH_FRAC the ankles are 0.117 m
        // apart, and a shin drawn at a constant 0.80 of THIGH diameter is
        // 0.122 m wide — the two legs would intersect. Real calf-to-ankle is
        // about 0.11 m down to 0.07; SHIN_ANKLE_TAPER puts the distal end at
        // 0.071 m, so the legs close to a real stance without merging.
        limb(m, p.shin_length(), p.leg_thickness * SHIN_TAPER, pack(BOOT), 1.0f,
             BEVEL_BOOT, SHIN_ANKLE_TAPER);
        break;
    case Bone::FootL:
    case Bone::FootR: {
        // Origin at the ankle; sole at -ankle_height; toes forward (-Z).
        const float h = p.leg_thickness * FOOT_WIDTH_RATIO * 0.5f;
        const float heel = p.foot_length * FOOT_HEEL_RATIO;
        const float hz = p.foot_length * 0.5f;
        prism(m, -p.ankle_height, 0.0f, h, hz, h, hz, BEVEL_BOOT, pack(BOOT),
              heel - hz);
        break;
    }
    }
    return m;
}

} // namespace dfn::anim
