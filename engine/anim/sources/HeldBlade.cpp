/*
Module: engine/anim
File: engine/anim/sources/HeldBlade.cpp

Responsibility:
- Implements the one-bone sword: four boxes in the posed hand's own frame.

Dependencies:
- Uses: HeldBlade.h, core skeleton, glm.
- Used by: the app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure and deterministic; no clock, no IO, no ECS.
*/

#include "engine/anim/sources/HeldBlade.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {
namespace {

// --- Placeholder ASSET data (Rule 5's carve-out; see the header) ------------
// AN ARMING SWORD, the plainest one-handed shape there is: a straight blade
// about as long as the forearm-plus-upper-arm, a short straight crossguard, a
// one-hand grip and a disc pommel. Dimensions are the museum norm for a
// XIII-century type XIV rounded to the centimetre, which is what makes them
// asset data and not numbers anyone else has to agree with.
constexpr float BLADE_LEN = 0.78f;    // m, ricasso to point
constexpr float BLADE_WIDE = 0.048f;  // m, at the guard
constexpr float BLADE_TIP = 0.016f;   // m, at the point
constexpr float BLADE_THICK = 0.009f; // m
constexpr float GUARD_SPAN = 0.20f;   // m, tip to tip
constexpr float GUARD_THICK = 0.018f; // m
constexpr float GRIP_LEN = 0.105f;    // m
constexpr float GRIP_THICK = 0.030f;  // m
constexpr float POMMEL = 0.042f;      // m, disc diameter
// HOW FAR THE GUARD SITS FROM THE HAND JOINT along the grip. The hand joint is
// at the WRIST on every rig this project has met, and a fist closes about a
// palm's width past it.
constexpr float WRIST_TO_FIST = 0.055f;

constexpr uint32_t STEEL = 0xFFC8C4BEu;  // 0xAABBGGRR
constexpr uint32_t GUARD_C = 0xFF6E6A64u;
constexpr uint32_t LEATHER = 0xFF2A3A55u; // dark brown, packed BGR
constexpr uint32_t BRASS = 0xFF3E86A8u;

struct Frame {
    glm::vec3 origin{0.0f};
    glm::vec3 along{0.0f, 1.0f, 0.0f};  ///< down the blade, hilt -> point
    glm::vec3 flat{1.0f, 0.0f, 0.0f};   ///< across the flat of the blade
    glm::vec3 edge{0.0f, 0.0f, 1.0f};   ///< the thin direction
};

/// One box, given its centre along the blade axis and its half-extents in the
/// frame's three directions. Flat-shaded: every face gets its own four
/// vertices so the normal is the face's and not a smoothed average.
void box(const Frame& f, float centre, float half_along, float half_flat,
         float half_edge, uint32_t colour, HeldBlade& out) {
    const glm::vec3 c = f.origin + f.along * centre;
    const glm::vec3 a = f.along * half_along;
    const glm::vec3 b = f.flat * half_flat;
    const glm::vec3 e = f.edge * half_edge;
    struct Face {
        glm::vec3 normal;
        glm::vec3 u;
        glm::vec3 v;
        glm::vec3 offset;
    };
    const Face faces[6] = {
        {f.along, b, e, a},   {-f.along, e, b, -a},
        {f.flat, e, a, b},    {-f.flat, a, e, -b},
        {f.edge, a, b, e},    {-f.edge, b, a, -e},
    };
    for (const Face& face : faces) {
        const auto base = static_cast<uint32_t>(out.vertices.size());
        const glm::vec3 o = c + face.offset;
        const glm::vec3 corner[4] = {o - face.u - face.v, o + face.u - face.v,
                                     o + face.u + face.v, o - face.u + face.v};
        const glm::vec2 uv[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        for (int i = 0; i < 4; ++i) {
            platform::SkinnedVertex v{};
            v.position = corner[i];
            v.normal = glm::normalize(face.normal);
            v.uv = uv[i];
            v.color_rgba = colour;
            v.joints[0] = static_cast<uint8_t>(out.joint);
            v.weights[0] = 1.0f;
            out.vertices.push_back(v);
        }
        out.indices.insert(out.indices.end(),
                           {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

/// A four-sided tapered blade: the same six faces as a box, but the far end is
/// narrower. Written out rather than folded into `box` because a taper needs
/// per-end extents and a box that took eight of them would stop being a box.
void taper(const Frame& f, float from, float to, float half_flat_from,
           float half_flat_to, float half_edge, uint32_t colour, HeldBlade& out) {
    const glm::vec3 a0 = f.origin + f.along * from;
    const glm::vec3 a1 = f.origin + f.along * to;
    const glm::vec3 b0 = f.flat * half_flat_from;
    const glm::vec3 b1 = f.flat * half_flat_to;
    const glm::vec3 e = f.edge * half_edge;
    const glm::vec3 ring0[4] = {a0 - b0 - e, a0 + b0 - e, a0 + b0 + e, a0 - b0 + e};
    const glm::vec3 ring1[4] = {a1 - b1 - e, a1 + b1 - e, a1 + b1 + e, a1 - b1 + e};
    const auto quad = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                          const glm::vec3& p3) {
        const auto base = static_cast<uint32_t>(out.vertices.size());
        const glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p3 - p0));
        const glm::vec3 p[4] = {p0, p1, p2, p3};
        const glm::vec2 uv[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        for (int i = 0; i < 4; ++i) {
            platform::SkinnedVertex v{};
            v.position = p[i];
            v.normal = n;
            v.uv = uv[i];
            v.color_rgba = colour;
            v.joints[0] = static_cast<uint8_t>(out.joint);
            v.weights[0] = 1.0f;
            out.vertices.push_back(v);
        }
        out.indices.insert(out.indices.end(),
                           {base, base + 1, base + 2, base, base + 2, base + 3});
    };
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) % 4;
        quad(ring0[i], ring0[j], ring1[j], ring1[i]);
    }
    quad(ring0[3], ring0[2], ring0[1], ring0[0]); // the butt, facing the hilt
    quad(ring1[0], ring1[1], ring1[2], ring1[3]); // the point
}

[[nodiscard]] glm::quat model_rotation(const glm::mat4& m) {
    glm::mat3 r{m};
    for (int c = 0; c < 3; ++c) {
        const float len = glm::length(r[c]);
        r[c] = len > 1.0e-8f ? r[c] / len : glm::vec3{c == 0, c == 1, c == 2};
    }
    return glm::normalize(glm::quat_cast(r));
}

} // namespace

HeldBlade build_held_blade(const skel::Skeleton& skeleton,
                           const SkinnedRigBinding& binding) {
    HeldBlade out;
    const int32_t hand = binding.names.joint[bone_index(Bone::HandR)];
    const int32_t forearm = binding.names.joint[bone_index(Bone::ForearmR)];
    if (hand < 0 || forearm < 0 || static_cast<std::size_t>(hand) >= skeleton.size()
        || static_cast<std::size_t>(forearm) >= skeleton.size()
        || hand > 255) {
        return out; // the caller reports; a silent empty sword is not a sword
    }
    // BIND-MODEL SPACE, which is the frame the palette undoes (header).
    const glm::mat4 hand_bind =
        glm::inverse(skeleton.joints[static_cast<std::size_t>(hand)].inverse_bind);
    const glm::mat4 arm_bind =
        glm::inverse(skeleton.joints[static_cast<std::size_t>(forearm)].inverse_bind);
    const glm::vec3 hand_p{hand_bind[3]};
    const glm::vec3 arm_p{arm_bind[3]};
    glm::vec3 along = hand_p - arm_p;
    if (glm::length(along) < 1.0e-4f) {
        return out;
    }
    along = glm::normalize(along);
    // THE ROLL OF THE FLAT COMES FROM THE WRIST, not from a world axis: the
    // hand is the one joint that knows which way its palm faces, and a blade
    // whose flat ignored it would turn edge-on as soon as the wrist did.
    glm::vec3 flat = model_rotation(hand_bind) * glm::vec3{1.0f, 0.0f, 0.0f};
    flat -= along * glm::dot(flat, along);
    if (glm::length(flat) < 1.0e-4f) {
        flat = std::abs(along.y) < 0.9f ? glm::cross(along, glm::vec3{0.0f, 1.0f, 0.0f})
                                        : glm::vec3{1.0f, 0.0f, 0.0f};
    }
    flat = glm::normalize(flat);

    out.joint = hand;
    Frame f;
    f.origin = hand_p;
    f.along = along;
    f.flat = flat;
    f.edge = glm::normalize(glm::cross(along, flat));

    // The grip runs BACK from the fist (negative along) and the blade forward,
    // so the hand closes around the middle of the hilt the way a hand does.
    const float guard_at = WRIST_TO_FIST;
    box(f, guard_at - 0.5f * GRIP_LEN, 0.5f * GRIP_LEN, 0.5f * GRIP_THICK,
        0.5f * GRIP_THICK, LEATHER, out);
    box(f, guard_at - GRIP_LEN - 0.5f * POMMEL, 0.5f * POMMEL, 0.5f * POMMEL,
        0.5f * POMMEL, BRASS, out);
    // The crossguard's long axis is the FLAT direction: a guard lies in the
    // plane of the blade, which is what makes it a cross and not a T.
    {
        const glm::vec3 c = f.origin + f.along * guard_at;
        Frame g;
        g.origin = c;
        g.along = f.flat;
        g.flat = f.along;
        g.edge = f.edge;
        box(g, 0.0f, 0.5f * GUARD_SPAN, 0.5f * GUARD_THICK, 0.5f * GUARD_THICK,
            GUARD_C, out);
    }
    taper(f, guard_at + 0.5f * GUARD_THICK, guard_at + BLADE_LEN, 0.5f * BLADE_WIDE,
          0.5f * BLADE_TIP, 0.5f * BLADE_THICK, STEEL, out);
    out.length_m = BLADE_LEN + GRIP_LEN + POMMEL;
    return out;
}

} // namespace dfn::anim
