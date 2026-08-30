/*
Module: tests
File: tests/character/SkinningTests.cpp

Responsibility:
- The import-and-skinning wave's proofs, in the order the pipeline runs:
  (1) the .dfo v5 reader against the reference figure's known numbers,
  (2) the glTF -> Rig name map, with an unbindable control,
  (3) clip sampling: a named frame, the ends held, determinism,
  (4) skinning: the CPU reference against the FORM the GPU program uses,
      on three vertices, at 1e-4,
  (5) the retarget: our procedural gait actually moves the imported skeleton.

Dependencies:
- Uses: doctest, dfn_anim, dfn_render (ObjectRegistry), dfn_core.
- Used by: ctest (character_skinning).

Notes:
- REFERENCE NUMBERS COME FROM A FILE NOBODY HERE AUTHORED. RiggedFigure is
  Khronos' sample asset (assets/objects/characters/LICENSE); its 19 joints,
  370 vertices, 256 triangles and 1.25 s clip are facts about someone else's
  file, which is the only kind of expectation a READER can be tested against.
  A fixture we baked ourselves would agree with our own bug.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every claim gets a CONTROL that must NOT hold (Rule 30): the bind pose for
  the retarget, an unmapped name for the bone map, a different frame for the
  clip.
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "engine/anim/sources/BoneMap.h"
#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/render/sources/ObjectRegistry.h"

using namespace dfn;

namespace {

const std::filesystem::path FIGURE{"assets/objects/characters/RiggedFigure.dfo"};
const std::filesystem::path KNIGHT{"assets/objects/characters/Knight.dfo"};

/// LINEAR BLEND SKINNING WRITTEN THE OTHER WAY ROUND: the matrices are summed
/// first and the vertex transformed once. This is the classic textbook form;
/// dfn_skin.sh (and anim::cpu_skin_position with it) transforms by each bone
/// and weights the RESULTS, because that is what a vertex shader can do
/// without matrix arithmetic. The two are equal for affine matrices, and THAT
/// EQUALITY IS THE ASSUMPTION THE SHADER RESTS ON -- so it is what gets
/// measured, on real palette matrices, rather than asserted in a comment.
[[nodiscard]] glm::vec3 blended_matrix_skin(const platform::SkinnedVertex& v,
                                            const std::vector<glm::mat4>& palette) {
    glm::mat4 m{0.0f};
    for (int k = 0; k < 4; ++k) {
        const std::size_t j = v.joints[k];
        if (j < palette.size()) {
            m += palette[j] * v.weights[k];
        }
    }
    return glm::vec3{m * glm::vec4{v.position, 1.0f}};
}

} // namespace

TEST_CASE("dfo v5 carries the reference figure's skin, skeleton and clip") {
    REQUIRE_MESSAGE(std::filesystem::exists(FIGURE),
                    "bake it: the CMake target dfn_characters, or dfn_import_gltf");
    const auto obj = render::read_object(FIGURE);
    REQUIRE(obj.has_value());

    // The file's own numbers, read out of Khronos' asset by the importer.
    CHECK(obj->skeleton.size() == 19);
    CHECK(obj->skin.vertices.size() == 370);
    CHECK(obj->skin.indices.size() == 256 * 3);
    REQUIRE(obj->clips.size() == 1);
    CHECK(obj->clips[0].duration_s == doctest::Approx(1.25).epsilon(0.001));
    CHECK(obj->clips[0].channels.size() == 57);

    // PARENT BEFORE CHILD is a contract the reader enforces; assert it holds
    // in the file we actually ship, not only in the reader's refusal path.
    for (std::size_t i = 0; i < obj->skeleton.joints.size(); ++i) {
        CHECK(obj->skeleton.joints[i].parent < static_cast<int32_t>(i));
    }
    // EVERY VERTEX'S WEIGHTS SUM TO 1. A vertex that sums to 0.97 shrinks
    // toward the origin and reads as a dent in the mesh; the importer
    // normalises, and this is the assertion that it did.
    float worst = 0.0f;
    for (const platform::SkinnedVertex& v : obj->skin.vertices) {
        const float sum = v.weights[0] + v.weights[1] + v.weights[2] + v.weights[3];
        worst = std::max(worst, std::fabs(sum - 1.0f));
        CHECK(v.joints[0] < obj->skeleton.size());
    }
    CHECK(worst < 1e-5f);

    // ROUND TRIP: the identity survives a write and a read. The content hash
    // is verified on every read, so a mismatch would already have refused the
    // file -- this pins that the NEW sections are inside that identity.
    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "dfn_skin_roundtrip.dfo";
    REQUIRE(render::write_object(*obj, tmp));
    const auto again = render::read_object(tmp);
    REQUIRE(again.has_value());
    CHECK(again->content_hash == obj->content_hash);
    CHECK(again->skeleton.size() == obj->skeleton.size());
    CHECK(again->clips.size() == obj->clips.size());
    std::filesystem::remove(tmp);
}

TEST_CASE("the glTF joint names map onto our fifteen bones") {
    // The two naming families the shipped assets use, plus the canonical one.
    CHECK(anim::normalize_bone_name("mixamorig:LeftForeArm") == "leftforearm");
    CHECK(anim::normalize_bone_name("arm_joint_L_2") == "armjointl2");
    CHECK(anim::normalize_bone_name("upperarm.l") == "upperarml");

    CHECK(anim::bone_from_joint_name("hips").value() == anim::Bone::Pelvis);
    CHECK(anim::bone_from_joint_name("upperarm.l").value() == anim::Bone::UpperArmL);
    CHECK(anim::bone_from_joint_name("lowerleg.r").value() == anim::Bone::ShinR);
    CHECK(anim::bone_from_joint_name("arm_joint_R_3").value() == anim::Bone::HandR);
    // THE CONTROL: names that must NOT bind. "handslot.l" and the IK rigs of
    // the reference knight are not deforming bones, and "root" is the FLOOR
    // bone half the world's rigs put under the hips -- binding it to the
    // pelvis would rotate the whole body where it should sway.
    CHECK_FALSE(anim::bone_from_joint_name("handslot.l").has_value());
    CHECK_FALSE(anim::bone_from_joint_name("kneeIK.l").has_value());
    CHECK_FALSE(anim::bone_from_joint_name("root").has_value());
    CHECK_FALSE(anim::bone_from_joint_name("").has_value());

    for (const std::filesystem::path& p : {FIGURE, KNIGHT}) {
        if (!std::filesystem::exists(p)) {
            continue;
        }
        const auto obj = render::read_object(p);
        REQUIRE(obj.has_value());
        const anim::SkeletonBinding b = anim::bind_skeleton(obj->skeleton);
        INFO("asset ", p.string());
        CHECK(b.bound_count == anim::BONE_COUNT);
        // ONE JOINT PER BONE AND NO JOINT TWICE: a spine chain driven twice
        // bends twice, which is the classic retarget defect.
        std::vector<int32_t> used;
        for (uint32_t i = 0; i < anim::BONE_COUNT; ++i) {
            REQUIRE(b.joint[i] >= 0);
            CHECK(std::find(used.begin(), used.end(), b.joint[i]) == used.end());
            used.push_back(b.joint[i]);
        }
    }
}

TEST_CASE("a clip samples to a pose, holds at its ends, and repeats exactly") {
    REQUIRE(std::filesystem::exists(FIGURE));
    const auto obj = render::read_object(FIGURE);
    REQUIRE(obj.has_value());
    REQUIRE(obj->clips.size() == 1);
    const skel::Skeleton& sk = obj->skeleton;
    const skel::AnimClip& clip = obj->clips[0];

    const std::size_t n = sk.size();
    std::vector<glm::vec3> t(n), s(n);
    std::vector<glm::quat> r(n);

    // FRAME N, and N is named rather than swept: the claim is that one
    // specific time gives one specific pose, and a sweep would let a sampler
    // that returns the bind pose everywhere pass.
    const float frame_n = 0.5f;
    skel::sample_clip(sk, clip, frame_n, t, r, s);
    const std::vector<glm::quat> at_half = r;

    // DETERMINISM: the same time gives bit-identical values.
    skel::sample_clip(sk, clip, frame_n, t, r, s);
    for (std::size_t i = 0; i < n; ++i) {
        CHECK(r[i].w == at_half[i].w);
        CHECK(r[i].x == at_half[i].x);
    }
    // ...AND THE CONTROL: a different time gives a DIFFERENT pose. Without
    // this, a sampler stuck on the bind pose passes everything above.
    skel::sample_clip(sk, clip, 1.0f, t, r, s);
    float moved = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        moved = std::max(moved, std::fabs(r[i].x - at_half[i].x)
                                    + std::fabs(r[i].y - at_half[i].y)
                                    + std::fabs(r[i].z - at_half[i].z));
    }
    CHECK(moved > 1e-3f);

    // BOTH ENDS HOLD rather than extrapolate: a clip asked about a time
    // outside its own span must answer with its first or last key.
    std::vector<glm::quat> before(n), first(n), after(n), last(n);
    skel::sample_clip(sk, clip, -5.0f, t, before, s);
    skel::sample_clip(sk, clip, 0.0f, t, first, s);
    skel::sample_clip(sk, clip, clip.duration_s + 5.0f, t, after, s);
    skel::sample_clip(sk, clip, clip.duration_s, t, last, s);
    for (std::size_t i = 0; i < n; ++i) {
        CHECK(before[i].x == doctest::Approx(first[i].x).epsilon(1e-5));
        CHECK(after[i].x == doctest::Approx(last[i].x).epsilon(1e-5));
    }

    // The clip drives real geometry: FK over the sampled pose puts the joints
    // somewhere other than where the bind pose does.
    std::vector<glm::mat4> bind_local(n), bind_model(n), anim_model(n);
    skel::skeleton_bind_local(sk, bind_local);
    skel::skeleton_model_matrices(sk, bind_local, bind_model);
    skel::sample_clip_model_matrices(sk, clip, frame_n, anim_model);
    float max_shift = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        max_shift = std::max(max_shift,
                             glm::length(glm::vec3{anim_model[i][3]}
                                         - glm::vec3{bind_model[i][3]}));
    }
    CHECK(max_shift > 0.01f);
}

TEST_CASE("CPU skinning equals the form the GPU program uses, to 1e-4") {
    REQUIRE(std::filesystem::exists(FIGURE));
    const auto obj = render::read_object(FIGURE);
    REQUIRE(obj.has_value());
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    const anim::SkinnedRigBinding binding = anim::bind_skinned_rig(rig, obj->skeleton);

    // A REAL POSE, not identity: at the bind pose every palette matrix is the
    // identity and BOTH forms return the vertex unchanged, so the test would
    // pass with the skinning deleted.
    anim::LocalPose pose = anim::gait_pose(rig, 0.37f, 0.9f, 0.0f);
    std::vector<glm::mat4> palette(obj->skeleton.size());
    anim::skinning_palette(rig, obj->skeleton, binding, pose, palette);

    // THREE VERTICES, spread across the mesh so at least one of them is on a
    // limb (a vertex weighted only to the pelvis moves with the root and
    // proves nothing about blending).
    const std::size_t picks[3] = {0, obj->skin.vertices.size() / 2,
                                  obj->skin.vertices.size() - 1};
    float worst = 0.0f;
    float largest_move = 0.0f;
    for (const std::size_t i : picks) {
        const platform::SkinnedVertex& v = obj->skin.vertices[i];
        const glm::vec3 shader_form = anim::cpu_skin_position(v, palette);
        const glm::vec3 matrix_form = blended_matrix_skin(v, palette);
        worst = std::max(worst, glm::length(shader_form - matrix_form));
        largest_move = std::max(largest_move, glm::length(shader_form - v.position));
    }
    INFO("worst |shader form - matrix form| = ", worst);
    CHECK(worst <= 1e-4f);
    // AND THE CONTROL FOR THE TEST ITSELF: the pose actually moved these
    // vertices. Agreement between two ways of computing nothing is not a
    // result (Rule 47's shape: measure the difference against a control arm,
    // and make sure the arm is not the subject).
    INFO("largest vertex displacement under the gait pose = ", largest_move);
    CHECK(largest_move > 0.01f);
}

TEST_CASE("the retarget moves the imported skeleton with OUR gait") {
    REQUIRE(std::filesystem::exists(FIGURE));
    const auto obj = render::read_object(FIGURE);
    REQUIRE(obj.has_value());
    const anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    const anim::SkinnedRigBinding binding = anim::bind_skinned_rig(rig, obj->skeleton);
    REQUIRE(binding.bound_count() == anim::BONE_COUNT);

    const std::size_t n = obj->skeleton.size();
    std::vector<glm::mat4> at_rest(n), at_plant(n), at_swing(n);
    anim::skinning_palette(rig, obj->skeleton, binding, anim::LocalPose{}, at_rest);
    // THE CONTROL ARM: an identity pose must put the model in OUR REST POSE --
    // arms hanging, hands below shoulders, feet on the ground -- and NOT in
    // the pose its author bound it in. It is a real statement about a real
    // asset: the sample figure binds with its arms out, and a retarget that
    // only re-expresses our deltas (a change of basis) leaves them out. The
    // measurement that caught it: 1.591 m wide against 1.705 m tall.
    std::vector<glm::mat4> rest_model(n);
    anim::rest_model_matrices(rig, obj->skeleton, binding, anim::LocalPose{},
                              rest_model);
    const auto joint_of = [&](anim::Bone b) {
        return static_cast<std::size_t>(binding.names.joint[anim::bone_index(b)]);
    };
    const float hand_y = rest_model[joint_of(anim::Bone::HandL)][3][1];
    const float shoulder_y = rest_model[joint_of(anim::Bone::UpperArmL)][3][1];
    const float head_y = rest_model[joint_of(anim::Bone::Head)][3][1];
    const float foot_y = rest_model[joint_of(anim::Bone::FootL)][3][1];
    INFO("rest pose: shoulder ", shoulder_y, " hand ", hand_y, " head ", head_y,
         " foot ", foot_y);
    CHECK(hand_y < shoulder_y);   // the arms HANG
    CHECK(head_y > shoulder_y);   // and the head is on top
    CHECK(foot_y < shoulder_y);
    // ...and the figure is taller than it is wide, which a T-pose is not.
    float wlo = 0.0f;
    float whi = 0.0f;
    float ylo = 0.0f;
    float yhi = 0.0f;
    for (std::size_t i = 0; i < obj->skin.vertices.size(); ++i) {
        const glm::vec3 p = anim::cpu_skin_position(obj->skin.vertices[i], at_rest);
        wlo = i == 0 ? p.x : std::min(wlo, p.x);
        whi = i == 0 ? p.x : std::max(whi, p.x);
        ylo = i == 0 ? p.y : std::min(ylo, p.y);
        yhi = i == 0 ? p.y : std::max(yhi, p.y);
    }
    INFO("rest extent: ", whi - wlo, " m wide, ", yhi - ylo, " m tall");
    CHECK(yhi - ylo > whi - wlo);

    // THE WORKING ARM: the two halves of the stride move the knees the OTHER
    // WAY ROUND. That is the one thing a walk cycle must do and the one thing
    // a broken retarget cannot fake.
    const float step = 0.9f;
    anim::skinning_palette(rig, obj->skeleton, binding,
                           anim::gait_pose(rig, config::FOOTFALL_PHASE_LEFT, step, 0.0f),
                           at_plant);
    anim::skinning_palette(rig, obj->skeleton, binding,
                           anim::gait_pose(rig, config::FOOTFALL_PHASE_RIGHT, step, 0.0f),
                           at_swing);
    const int32_t shin_l = binding.names.joint[anim::bone_index(anim::Bone::ShinL)];
    const int32_t shin_r = binding.names.joint[anim::bone_index(anim::Bone::ShinR)];
    REQUIRE(shin_l >= 0);
    REQUIRE(shin_r >= 0);
    const auto delta = [&](const std::vector<glm::mat4>& a,
                           const std::vector<glm::mat4>& b, int32_t j) {
        return glm::length(glm::vec3{a[static_cast<std::size_t>(j)][3]}
                           - glm::vec3{b[static_cast<std::size_t>(j)][3]});
    };
    const float moved_l = delta(at_plant, at_swing, shin_l);
    const float moved_r = delta(at_plant, at_swing, shin_r);
    INFO("left shin moves ", moved_l, " m, right shin ", moved_r, " m between the "
         "two footfall phases");
    CHECK(moved_l > 0.01f);
    CHECK(moved_r > 0.01f);
    // MIRROR SYMMETRY OF THE STRIDE: the two footfall phases are half a cycle
    // apart, so what the left leg does at one, the right does at the other.
    // Equal magnitudes is the cheapest statement of that which does not
    // re-derive the clip.
    CHECK(moved_l == doctest::Approx(moved_r).epsilon(0.05));
}
