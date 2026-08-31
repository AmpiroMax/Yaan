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

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <vector>
#include <cmath>
#include <limits>
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
constexpr float GRIP_THICK = 0.030f;  // m
constexpr float POMMEL = 0.042f;      // m, disc diameter
// СКОЛЬКО ГАРДА СТОИТ ЗА ЛИНИЕЙ КОСТЯШЕК, метры. Гарда упирается в
// указательный палец — не пронзает его и не висит отдельно от него, — и
// половина её собственной толщины плюс миллиметр это ровно и означает.
constexpr float GUARD_PAST_KNUCKLES = 0.5f * GUARD_THICK + 0.001f;
// СКОЛЬКО НАВЕРШИЕ ВЫСТУПАЕТ ЗА ПЯТКУ ЛАДОНИ, метры. Навершие затем и
// существует, чтобы рука не соскальзывала с рукояти: оно обязано быть ЗА
// краем ладони, а не внутри неё.
constexpr float POMMEL_PAST_HEEL = 0.010f;
// THE HAMMER GRIP'S CANT, as a fraction of the forearm direction mixed into
// the knuckle line. A hilt does not lie ALONG the knuckles: in the grip a
// swordsman actually uses it runs diagonally across the palm, from the base of
// the index finger to the heel of the hand, which tips the blade toward the
// forearm. 0.5 is tan(26.6 degrees) of that cant, and it is what puts the
// carried blade in the reference's own band — measured on this asset, the
// drawn idle went from 7.3 degrees off the ground (a blade held out level, and
// nobody carries a sword that way) to 33.
/// The band the cant is searched over, and how finely. A cant is a ratio, and
/// past about 1.5 the "hilt" has stopped crossing the palm at all.
constexpr float CANT_MAX = 1.5f;
constexpr int CANT_STEPS = 96;

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
                           const SkinnedRigBinding& binding,
                           std::span<const JointLocal> guard_pose) {
    HeldBlade out;
    const int32_t hand = binding.names.joint[bone_index(Bone::HandR)];
    const int32_t forearm = binding.names.joint[bone_index(Bone::ForearmR)];
    if (hand < 0 || forearm < 0 || static_cast<std::size_t>(hand) >= skeleton.size()
        || static_cast<std::size_t>(forearm) >= skeleton.size()
        || hand > 255) {
        return out; // the caller reports; a silent empty sword is not a sword
    }
    // BIND-MODEL SPACE, which is the frame the palette undoes (header).
    const auto bind_of = [&](int32_t j) {
        return glm::inverse(skeleton.joints[static_cast<std::size_t>(j)].inverse_bind);
    };
    const glm::vec3 hand_p{bind_of(hand)[3]};
    const glm::vec3 arm_p{bind_of(forearm)[3]};

    // THE HILT LIES ALONG THE KNUCKLES, and this is the second answer to
    // "which way does the sword point". The first — continue the forearm —
    // was written because it needs nothing but two joints every rig has, and
    // the frame showed what it costs: standing, the arm hangs, so the blade
    // hung with it and went 0.4 m into the grass. A real fist holds a hilt
    // ACROSS the palm, and the line the hilt lies on is the line through the
    // bases of the fingers.
    //
    // FOUND BY GEOMETRY AND NOT BY NAME. The hand's direct children are the
    // finger bases on any rig; the THUMB is the one that sits apart from the
    // others (it is the odd point, and that is what "opposable" means), so it
    // is the child furthest from the mean of the rest. The remaining bases lie
    // on the knuckle line, whose direction is their principal axis, and the
    // blade leaves the fist on the THUMB side. No substring of "index" or
    // "pinky" appears anywhere in this, so the rule survives the next asset.
    std::vector<glm::vec3> base;
    for (std::size_t j = 0; j < skeleton.size(); ++j) {
        if (skeleton.joints[j].parent == hand) {
            base.push_back(glm::vec3{bind_of(static_cast<int32_t>(j))[3]});
        }
    }
    glm::vec3 along{0.0f};
    glm::vec3 finger_dir{0.0f};
    if (base.size() >= 4) {
        glm::vec3 mean{0.0f};
        for (const glm::vec3& p : base) {
            mean += p;
        }
        mean /= float(base.size());
        std::size_t thumb = 0;
        float worst = -1.0f;
        for (std::size_t i = 0; i < base.size(); ++i) {
            const float d = glm::length(base[i] - mean);
            if (d > worst) {
                worst = d;
                thumb = i;
            }
        }
        std::vector<glm::vec3> knuckle;
        for (std::size_t i = 0; i < base.size(); ++i) {
            if (i != thumb) {
                knuckle.push_back(base[i]);
            }
        }
        // The principal axis of four nearly-collinear points is the widest
        // spread among them, which needs no eigen solve: the pair furthest
        // apart IS the line.
        float span = 0.0f;
        for (std::size_t i = 0; i < knuckle.size(); ++i) {
            for (std::size_t j = i + 1; j < knuckle.size(); ++j) {
                const float d = glm::length(knuckle[j] - knuckle[i]);
                if (d > span) {
                    span = d;
                    along = knuckle[j] - knuckle[i];
                }
            }
        }
        if (span > 1.0e-4f) {
            along = glm::normalize(along);
            // Point it at the thumb: that is the side a blade comes out of.
            if (glm::dot(base[thumb] - hand_p, along) < 0.0f) {
                along = -along;
            }
            glm::vec3 knuckle_mean{0.0f};
            for (const glm::vec3& p : knuckle) {
                knuckle_mean += p;
            }
            finger_dir = knuckle_mean / float(knuckle.size()) - hand_p;
        } else {
            along = glm::vec3{0.0f};
        }
    }
    if (glm::length(along) < 1.0e-4f) {
        // NO FINGERS: fall back to the forearm's line and say so by drawing a
        // sword that hangs down the arm. It is worse and it is not nothing.
        along = hand_p - arm_p;
        if (glm::length(along) < 1.0e-4f) {
            return out;
        }
        along = glm::normalize(along);
    }
    // AND THE CANT, SOLVED AGAINST THE POSE THE SWORD IS CARRIED IN. The
    // knuckle line alone holds the blade level, because a hanging arm's
    // knuckles face forward; a real grip runs diagonally across the palm and
    // tips the blade toward the forearm. HOW FAR it tips is the one free
    // number, and it is not guessable: a cant that looks anatomical in the
    // BIND pose lands somewhere else entirely in the guard, which is a
    // different pose — measured, an eyeballed 0.5 gave 15 degrees standing and
    // 7 walking, on either side of the 30-40 the reference states. So the cant
    // is scanned until the blade sits at STANCE_BLADE_TILT in the guard pose,
    // and the row is then the only thing said about the angle at all.
    if (glm::length(hand_p - arm_p) > 1.0e-4f) {
        const glm::vec3 arm_dir = glm::normalize(hand_p - arm_p);
        // THE TURN FROM BIND TO GUARD, which is all a rigid attachment can
        // see: the sword is one bone, so its world direction is the hand's
        // rotation applied to whatever direction we author here.
        glm::quat to_guard{1.0f, 0.0f, 0.0f, 0.0f};
        if (guard_pose.size() >= skeleton.size()) {
            std::vector<glm::mat4> local(skeleton.size(), glm::mat4{1.0f});
            std::vector<glm::mat4> model(skeleton.size(), glm::mat4{1.0f});
            for (std::size_t j = 0; j < skeleton.size(); ++j) {
                local[j] = glm::translate(glm::mat4{1.0f}, guard_pose[j].translation)
                           * glm::mat4_cast(glm::normalize(guard_pose[j].rotation))
                           * glm::scale(glm::mat4{1.0f}, guard_pose[j].scale);
            }
            skel::skeleton_model_matrices(skeleton, local, model);
            to_guard = model_rotation(model[static_cast<std::size_t>(hand)])
                       * glm::conjugate(model_rotation(bind_of(hand)));
        }
        const auto want = static_cast<float>(config::STANCE_BLADE_TILT);
        float best_err = std::numeric_limits<float>::max();
        glm::vec3 best = along;
        for (int i = 0; i <= CANT_STEPS; ++i) {
            const float cant = CANT_MAX * float(i) / float(CANT_STEPS);
            const glm::vec3 trial = glm::normalize(along + cant * arm_dir);
            const glm::vec3 posed = to_guard * trial;
            // Positive = the point is DOWN, which is how a sword is carried.
            const float tilt = std::asin(std::clamp(-posed.y, -1.0f, 1.0f));
            const float err = std::abs(tilt - want);
            if (err < best_err) {
                best_err = err;
                best = trial;
            }
        }
        along = best;
    }
    // THE EDGE FOLLOWS THE FINGERS. A blade held in a fist has its edge on the
    // side the fingertips point to; with no fingers the roll is the wrist's
    // own, which is the best the bind can say.
    glm::vec3 edge = glm::length(finger_dir) > 1.0e-4f
                         ? finger_dir
                         : model_rotation(bind_of(hand)) * glm::vec3{0.0f, 1.0f, 0.0f};
    edge -= along * glm::dot(edge, along);
    if (glm::length(edge) < 1.0e-4f) {
        edge = std::abs(along.y) < 0.9f ? glm::cross(along, glm::vec3{0.0f, 1.0f, 0.0f})
                                        : glm::vec3{1.0f, 0.0f, 0.0f};
    }
    edge = glm::normalize(edge);

    out.joint = hand;
    Frame f;
    // НАЧАЛО — ЦЕНТР КУЛАКА, А НЕ СУСТАВ ЗАПЯСТЬЯ, и это половина починки
    // пункта 5 («меч торчит из кисти, не лежит в руке»). Сустав кисти стоит в
    // ЗАПЯСТЬЕ, а рукоять зажата в ЛАДОНИ — между запястьем и костяшками; ось,
    // проведённая через сустав, проходит МИМО кулака. Замерено на этом ассете:
    // центр кулака отстоял от оси рукояти на 5.2 см при радиусе кулака 3.6 —
    // то есть рукоять шла рядом с рукой, снаружи неё.
    const glm::vec3 fist = 0.5f * (hand_p + (glm::length(finger_dir) > 1.0e-4f
                                                 ? hand_p + finger_dir
                                                 : hand_p));
    f.origin = fist;
    f.along = along;
    f.edge = edge;
    f.flat = glm::normalize(glm::cross(edge, along));

    // И ДЛИНА РУКОЯТИ ВЫВОДИТСЯ ИЗ РУКИ, А НЕ ЗАДАЁТСЯ. Гарда стоит за линией
    // костяшек (у указательного пальца), навершие — за пяткой ладони, а
    // рукоять — это ровно то, что между ними, то есть ДЛИНА ЛАДОНИ. Число
    // 0.105, стоявшее здесь раньше, было верным для чьей-то руки и неверным
    // для этой (её ладонь 0.116): гарда садилась на 4.9 см НЕ ДОХОДЯ до
    // костяшек, то есть в середину ладони.
    const float knuckle_at =
        glm::length(finger_dir) > 1.0e-4f
            ? glm::dot(hand_p + finger_dir - f.origin, f.along)
            : 0.5f * 0.116f;
    const float heel_at = glm::dot(hand_p - f.origin, f.along);
    const float guard_at = knuckle_at + GUARD_PAST_KNUCKLES;
    const float pommel_face = heel_at - POMMEL_PAST_HEEL;
    const float grip_len = std::max(0.02f, guard_at - pommel_face);
    box(f, guard_at - 0.5f * grip_len, 0.5f * grip_len, 0.5f * GRIP_THICK,
        0.5f * GRIP_THICK, LEATHER, out);
    box(f, pommel_face - 0.5f * POMMEL, 0.5f * POMMEL, 0.5f * POMMEL,
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
    out.length_m = BLADE_LEN + grip_len + POMMEL;
    return out;
}

} // namespace dfn::anim
