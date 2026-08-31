/*
Module: tests/character
File: tests/character/HitboxTests.cpp

Responsibility:
- Item 8 of the owner's 31.08 list: the body has PARTS, a ray can name which
  one it went through, the shapes follow the clip rather than the bind, and
  they cover the silhouette they are standing in for.

Key items:
- a_ray_names_the_part_it_went_through: head, chest and hand by aim, with a
  miss beside each as the control.
- the_boxes_follow_the_clip: the head's box moves between idle and crouch, and
  it moves WITH the head joint rather than by some other amount.
- the_boxes_cover_the_silhouette: projected coverage of the drawn body, with
  the empty set as the zero-dose control.

Dependencies:
- Uses: engine/anim (Hitbox, ClipPlayer, SkinnedBody, Rig), engine/render (the
  .dfo reader), the baked HumanBase.dfo (target dfn_characters).
- Used by: ctest (character_hitboxes).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 30: every claim gets a control. "A ray hit something" is true of a ray
  aimed anywhere near a body; only the miss beside it makes the hit a claim.
*/

#include <doctest/doctest.h>

#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>

using namespace dfn;

namespace {

constexpr const char* MODEL = "assets/objects/characters/HumanBase.dfo";

struct Model {
    render::RegistryObject obj;
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    anim::SkinnedRigBinding binding;
    anim::ClipLibrary lib;
    anim::HitboxSet set;
};

[[nodiscard]] bool load(Model& m) {
    if (!std::filesystem::exists(MODEL)) {
        return false;
    }
    auto o = render::read_object(MODEL);
    if (!o.has_value() || o->skeleton.empty() || o->clips.empty()) {
        return false;
    }
    m.obj = std::move(*o);
    m.binding = anim::bind_skinned_rig(m.rig, m.obj.skeleton);
    m.lib = anim::build_clip_library(m.rig, m.obj.skeleton, m.binding, m.obj.clips);
    m.set = anim::build_hitboxes(m.rig.proportions);
    return true;
}

/// The drawn pose of one role, through the same path the frame takes.
void pose_role(const Model& m, anim::ClipRole role, std::vector<anim::JointLocal>& out) {
    const anim::ClipEntry& e = m.lib[role];
    REQUIRE(e.present());
    anim::sample_clip_pose(m.obj.skeleton, m.obj.clips[static_cast<std::size_t>(e.clip)],
                           0.0f, out);
}

[[nodiscard]] glm::vec3 joint_of(const Model& m, anim::Bone b,
                                 std::span<const anim::JointLocal> sample) {
    std::vector<glm::mat4> local(m.obj.skeleton.size());
    std::vector<glm::mat4> model(m.obj.skeleton.size());
    for (std::size_t j = 0; j < m.obj.skeleton.size(); ++j) {
        local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                   * glm::mat4_cast(glm::normalize(sample[j].rotation))
                   * glm::scale(glm::mat4{1.0f}, sample[j].scale);
    }
    skel::skeleton_model_matrices(m.obj.skeleton, local, model);
    const int32_t j = m.binding.names.joint[anim::bone_index(b)];
    REQUIRE(j >= 0);
    return glm::vec3{model[static_cast<std::size_t>(j)][3]};
}

} // namespace

TEST_CASE("a_ray_names_the_part_it_went_through") {
    Model m;
    REQUIRE_MESSAGE(load(m), "bake the character first (target dfn_characters)");
    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    pose_role(m, anim::ClipRole::Idle, sample);
    const anim::HitboxPose pose =
        anim::hitbox_pose(m.set, m.obj.skeleton, m.binding, sample);
    // EVERY SLOT PLACED. A model missing a hand joint would quietly report
    // "none" for every hand shot, which reads exactly like a miss.
    CAPTURE(pose.count());
    CHECK(pose.count() == anim::HITBOX_COUNT);

    // THE AIM POINTS ARE THE JOINTS THEMSELVES, so the test does not carry a
    // second, hand-written idea of where a head is (Rule 35). The ray comes
    // from 3 m in FRONT of the figure — it faces -Z, so that is -Z of it —
    // and travels toward the joint.
    struct Shot {
        anim::Bone aim;
        anim::BodyPart want;
        const char* label;
    };
    const Shot shots[] = {
        {anim::Bone::Head, anim::BodyPart::Head, "head"},
        {anim::Bone::HandL, anim::BodyPart::HandL, "left hand"},
        {anim::Bone::HandR, anim::BodyPart::HandR, "right hand"},
        {anim::Bone::ShinL, anim::BodyPart::ShinL, "left shin"},
        {anim::Bone::FootR, anim::BodyPart::FootR, "right foot"},
    };
    for (const Shot& s : shots) {
        CAPTURE(std::string(s.label));
        glm::vec3 target = joint_of(m, s.aim, sample);
        if (s.aim == anim::Bone::Head) {
            // The head JOINT is the neck; the skull is half a head above it,
            // and that offset is the table's, not this test's guess.
            target.y += 0.5f * m.rig.proportions.head_height;
        }
        if (s.aim == anim::Bone::ShinL) {
            // ПРИЦЕЛ УВОДИТСЯ СО ШВА НА СЕРЕДИНУ ЗВЕНА, и это починка ПРИБОРА,
            // а не поблажка ему. Сустав голени — это КОЛЕНО, то есть ровно
            // граница между коробкой бедра и коробкой голени: на шве побеждает
            // та, что ближе на доли миллиметра, и любое шевеление пропорций в
            // сантиметр переворачивает ответ. Опыт соседней волны это и
            // предъявил: перевод BODY_SHOULDER_WIDTH_FRAC 0.259 -> 0.236 сводит
            // лодыжки на 3.5 см, нога наклоняется, и луч начинает называть
            // бедро — при совершенно исправных коробках.
            //
            // СЕРЕДИНА БЕРЁТСЯ У СКЕЛЕТА, А НЕ У ЭТОГО ФАЙЛА (правило 35): это
            // половина отрезка между коленом и лодыжкой, то есть те же два
            // сустава, которыми задана сама коробка голени. Своё «примерно
            // двадцать сантиметров ниже колена» было бы вторым описанием ноги.
            target = 0.5f * (target + joint_of(m, anim::Bone::FootL, sample));
        }
        if (s.aim == anim::Bone::FootR) {
            // The foot JOINT is the ankle and the foot hangs BELOW it — aim at
            // the ankle and the shin's box is what the ray meets first, which
            // is correct and is not what this row is asking about.
            target.y -= 0.5f * m.rig.proportions.ankle_height;
        }
        const glm::vec3 origin = target + glm::vec3{0.0f, 0.0f, -3.0f};
        const anim::HitboxHit hit =
            anim::hitbox_raycast(m.set, pose, origin, glm::vec3{0.0f, 0.0f, 1.0f}, 6.0f);
        CAPTURE(std::string(anim::body_part_name(hit.part)));
        CHECK(hit.part == s.want);
        // THE CONTROL BESIDE EVERY SHOT: the same ray, moved a metre out to
        // the side. A test that only ever fires at a body cannot tell a
        // hitbox set from a single box around the whole figure.
        const anim::HitboxHit miss = anim::hitbox_raycast(
            m.set, pose, origin + glm::vec3{1.5f, 0.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, 1.0f}, 6.0f);
        CHECK_FALSE(miss.hit());
    }
    // AND THE CHEST IS NOT THE HEAD. Aiming between the shoulders has to name
    // the trunk, or the three trunk boxes are one box with three names.
    // AIMED ALONG THE MODEL'S OWN SPINE, at the fraction the table gives the
    // chest. Adding "0.8 of OUR torso length" to the spine joint instead put
    // the shot 5 cm inside the skull: this asset's spine joint to head joint
    // is 0.472 m where our rig's torso is 0.612, and a test that carries its
    // own idea of where a chest is measures that idea (Rule 35).
    const glm::vec3 chest = glm::mix(joint_of(m, anim::Bone::Torso, sample),
                                     joint_of(m, anim::Bone::Head, sample), 0.775f);
    const anim::HitboxHit trunk = anim::hitbox_raycast(
        m.set, pose, chest + glm::vec3{0.0f, 0.0f, -3.0f}, glm::vec3{0.0f, 0.0f, 1.0f},
        6.0f);
    CAPTURE(std::string(anim::body_part_name(trunk.part)));
    CHECK(trunk.part == anim::BodyPart::Chest);
}

TEST_CASE("the_boxes_follow_the_clip") {
    Model m;
    REQUIRE(load(m));
    std::vector<anim::JointLocal> idle(m.obj.skeleton.size());
    std::vector<anim::JointLocal> crouch(m.obj.skeleton.size());
    pose_role(m, anim::ClipRole::Idle, idle);
    pose_role(m, anim::ClipRole::CrouchIdle, crouch);
    const anim::HitboxPose a = anim::hitbox_pose(m.set, m.obj.skeleton, m.binding, idle);
    const anim::HitboxPose b =
        anim::hitbox_pose(m.set, m.obj.skeleton, m.binding, crouch);

    const auto centre = [&](const anim::HitboxPose& p, anim::BodyPart want) {
        for (uint32_t i = 0; i < anim::HITBOX_COUNT; ++i) {
            if (m.set.slot[i].part == want && p.valid[i] != 0) {
                return glm::vec3{p.frame[i][3]};
            }
        }
        REQUIRE(false);
        return glm::vec3{0.0f};
    };
    const glm::vec3 head_idle = centre(a, anim::BodyPart::Head);
    const glm::vec3 head_crouch = centre(b, anim::BodyPart::Head);
    CAPTURE(head_idle.y);
    CAPTURE(head_crouch.y);
    // A CROUCHING HEAD IS LOWER, and by a lot: the bodies are kinematic and
    // follow the pose, which is the whole claim of item 8's third bullet.
    CHECK(head_idle.y - head_crouch.y > 0.10f);
    // AND IT MOVED BY WHAT THE HEAD MOVED BY, not by some other amount. This
    // is the control that separates "the boxes are posed" from "the boxes are
    // posed CORRECTLY": a table applied to the wrong joint would still move.
    const float joint_drop =
        joint_of(m, anim::Bone::Head, idle).y - joint_of(m, anim::Bone::Head, crouch).y;
    CAPTURE(joint_drop);
    CHECK(head_idle.y - head_crouch.y == doctest::Approx(joint_drop).epsilon(0.25));
}

TEST_CASE("the_boxes_cover_the_silhouette") {
    Model m;
    REQUIRE(load(m));
    std::vector<anim::JointLocal> sample(m.obj.skeleton.size());
    pose_role(m, anim::ClipRole::Idle, sample);
    const anim::HitboxPose pose =
        anim::hitbox_pose(m.set, m.obj.skeleton, m.binding, sample);

    // THE SKINNED BODY AS IT IS DRAWN, on the CPU, through the same palette
    // the GPU gets. Nothing here is about the boxes yet: this is the shape
    // they are standing in for.
    std::vector<glm::mat4> palette(m.obj.skeleton.size());
    anim::sample_palette(m.obj.skeleton, sample, palette);
    std::vector<glm::vec3> posed;
    posed.reserve(m.obj.skin.vertices.size());
    for (const platform::SkinnedVertex& v : m.obj.skin.vertices) {
        posed.push_back(anim::cpu_skin_position(v, palette));
    }
    REQUIRE(posed.size() > 100);
    glm::vec3 lo = posed[0];
    glm::vec3 hi = posed[0];
    for (const glm::vec3& p : posed) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    // A PROJECTION, which is what the order asks for and is the right question
    // for a hitbox: a shot comes from a direction, and what it can hit is the
    // shape it sees. The grid is one centimetre, the front view (the figure
    // faces -Z, so the camera looks along +Z).
    constexpr float CELL_M = 0.01f;
    const int nx = std::max(1, static_cast<int>((hi.x - lo.x) / CELL_M) + 2);
    const int ny = std::max(1, static_cast<int>((hi.y - lo.y) / CELL_M) + 2);
    std::vector<uint8_t> mesh(static_cast<std::size_t>(nx * ny), 0);
    const auto cell_of = [&](float x, float y) {
        const int cx = std::clamp(static_cast<int>((x - lo.x) / CELL_M), 0, nx - 1);
        const int cy = std::clamp(static_cast<int>((y - lo.y) / CELL_M), 0, ny - 1);
        return static_cast<std::size_t>(cy * nx + cx);
    };
    // TRIANGLE RASTERISATION, not "mark the cell each VERTEX falls in". A
    // 13 744-triangle mesh has vertices metres apart on a limb and would leave
    // a silhouette full of holes, which the boxes would then be credited with
    // not needing to cover.
    for (std::size_t i = 0; i + 2 < m.obj.skin.indices.size(); i += 3) {
        const glm::vec3& a = posed[m.obj.skin.indices[i]];
        const glm::vec3& b = posed[m.obj.skin.indices[i + 1]];
        const glm::vec3& c = posed[m.obj.skin.indices[i + 2]];
        const float minx = std::min({a.x, b.x, c.x});
        const float maxx = std::max({a.x, b.x, c.x});
        const float miny = std::min({a.y, b.y, c.y});
        const float maxy = std::max({a.y, b.y, c.y});
        const int x0 = std::clamp(static_cast<int>((minx - lo.x) / CELL_M), 0, nx - 1);
        const int x1 = std::clamp(static_cast<int>((maxx - lo.x) / CELL_M), 0, nx - 1);
        const int y0 = std::clamp(static_cast<int>((miny - lo.y) / CELL_M), 0, ny - 1);
        const int y1 = std::clamp(static_cast<int>((maxy - lo.y) / CELL_M), 0, ny - 1);
        const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        if (std::abs(area) < 1.0e-12f) {
            continue;
        }
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                const float px = lo.x + (float(cx) + 0.5f) * CELL_M;
                const float py = lo.y + (float(cy) + 0.5f) * CELL_M;
                const float w0 =
                    ((b.x - px) * (c.y - py) - (c.x - px) * (b.y - py)) / area;
                const float w1 =
                    ((c.x - px) * (a.y - py) - (a.x - px) * (c.y - py)) / area;
                const float w2 = 1.0f - w0 - w1;
                if (w0 >= -1.0e-4f && w1 >= -1.0e-4f && w2 >= -1.0e-4f) {
                    mesh[static_cast<std::size_t>(cy * nx + cx)] = 1;
                }
            }
        }
    }
    (void)cell_of;
    std::size_t drawn = 0;
    std::size_t covered = 0;
    for (int cy = 0; cy < ny; ++cy) {
        for (int cx = 0; cx < nx; ++cx) {
            if (mesh[static_cast<std::size_t>(cy * nx + cx)] == 0) {
                continue;
            }
            ++drawn;
            const glm::vec3 origin{lo.x + (float(cx) + 0.5f) * CELL_M,
                                   lo.y + (float(cy) + 0.5f) * CELL_M, lo.z - 2.0f};
            const anim::HitboxHit hit = anim::hitbox_raycast(
                m.set, pose, origin, glm::vec3{0.0f, 0.0f, 1.0f}, 8.0f);
            covered += hit.hit() ? 1u : 0u;
        }
    }
    REQUIRE(drawn > 1000); // the instrument saw a body at all
    const float ratio = float(covered) / float(drawn);
    CAPTURE(drawn);
    CAPTURE(covered);
    CAPTURE(ratio);
    MESSAGE("silhouette coverage " << 100.0f * ratio << " % of " << drawn
                                   << " cells (1 cm, front view, idle)");
    CHECK(ratio >= 0.90f);
    // THE ZERO-DOSE CONTROL (Rule 48): the same measurement with no boxes at
    // all has to read zero. A coverage instrument that credits an empty set is
    // measuring the mesh and calling it the hitboxes.
    const anim::HitboxPose empty;
    std::size_t control = 0;
    for (int cy = 0; cy < ny; ++cy) {
        for (int cx = 0; cx < nx; ++cx) {
            if (mesh[static_cast<std::size_t>(cy * nx + cx)] == 0) {
                continue;
            }
            const glm::vec3 origin{lo.x + (float(cx) + 0.5f) * CELL_M,
                                   lo.y + (float(cy) + 0.5f) * CELL_M, lo.z - 2.0f};
            control += anim::hitbox_raycast(m.set, empty, origin,
                                            glm::vec3{0.0f, 0.0f, 1.0f}, 8.0f)
                               .hit()
                           ? 1u
                           : 0u;
        }
    }
    CHECK(control == 0);
}
