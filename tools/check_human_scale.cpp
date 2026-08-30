/*
Module: tools
File: tools/check_human_scale.cpp

Responsibility:
- dfn_human_scale: measures an imported character's PROPORTIONS off its own
  skeleton and judges them against the canon in docs/design/HUMAN_SCALE.md.
  Prints every number it judged by; exits non-zero when any of them is outside
  the band.

Key items:
- main(): <file.dfo> [--tolerance FRAC] [--quiet].
- The measured landmarks: total height, head height (and heads-per-figure),
  shoulder / hip / knee / ankle heights, shoulder width, upper arm, forearm.

Dependencies:
- Uses: engine/render (.dfo reader), engine/anim (bone map, RigProportions),
  engine/core (skeleton FK).
- Used by: ctest (human_scale_* rows), the character wave's report.

Notes:
- THE CANON IS NOT RETYPED HERE. Every expected fraction is read from
  anim::RigProportions::from_config(), i.e. from the BODY_*_FRAC rows of
  NUMBERS.md, which docs/design/HUMAN_SCALE.md §"Наши строки против канона"
  certifies as EXACTLY the canonical values. A second copy of the table in
  this file would be the two-copies defect (Rule 35) sitting inside the very
  instrument that exists to catch proportion drift.
- IT MEASURES THE SKELETON, NOT THE PICTURE (Rule 47). Joint positions in the
  bind pose are geometry; a silhouette in a frame is a function of the camera,
  the light and the armour. The one thing taken from the MESH is the total
  height, because the crown of the head is not a joint.
- WHY A TOOL AND NOT ONLY A TEST. A test answers "did it break"; the owner's
  question on 30.08 was "what ARE the proportions" -- and that wants the table
  printed, on any file, without a rebuild of the suite.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Loud on every refusal; the numbers are printed even when the verdict is
  green, because a judge that prints only its verdict cannot be checked.
*/

#include "engine/anim/sources/BoneMap.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

struct Landmark {
    const char* name;
    float measured;  ///< fraction of the figure's total height
    float canon;
};

[[nodiscard]] glm::vec3 joint_pos(const std::vector<glm::mat4>& model, int32_t j) {
    return j >= 0 ? glm::vec3{model[static_cast<std::size_t>(j)][3]} : glm::vec3{0.0f};
}

} // namespace

int main(int argc, char** argv) {
    std::string path;
    // 5 % is the owner's own band (order of 30.08: "если доли модели расходятся
    // с каноном > 5 %"). Kept as a flag so a stricter reading can be asked for
    // without editing the judge -- but never defaulted looser.
    float tolerance = 0.05f;
    bool quiet = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--tolerance" && i + 1 < argc) {
            tolerance = std::strtof(argv[++i], nullptr);
        } else if (a == "--quiet") {
            quiet = true;
        } else if (path.empty()) {
            path = a;
        }
    }
    if (path.empty()) {
        std::fprintf(stderr, "dfn_human_scale <character.dfo> [--tolerance 0.05] "
                             "[--quiet]\n");
        return 2;
    }
    const auto obj = dfn::render::read_object(path);
    if (!obj.has_value()) {
        std::fprintf(stderr, "[scale] cannot read \"%s\"\n", path.c_str());
        return 1;
    }
    if (obj->skeleton.empty() || obj->skin.vertices.empty()) {
        std::fprintf(stderr, "[scale] \"%s\" is not a character (no SKEL/SKIN)\n",
                     path.c_str());
        return 1;
    }
    const dfn::anim::SkeletonBinding bind = dfn::anim::bind_skeleton(obj->skeleton);
    if (bind.bound_count < dfn::anim::BONE_COUNT) {
        std::fprintf(stderr,
                     "[scale] \"%s\": only %u of %u rig bones bind by name — the "
                     "proportions of an unbound skeleton cannot be judged\n",
                     path.c_str(), bind.bound_count, dfn::anim::BONE_COUNT);
        return 1;
    }

    // TOTAL HEIGHT COMES FROM THE MESH: the crown of the head is not a joint,
    // and every fraction below is a fraction OF IT.
    //
    // MEASURED ON THE REST-POSED MESH, NOT ON THE STORED BIND VERTICES, and
    // that distinction is the whole difference between an instrument and a
    // decoration. A model the importer fitted to the canon has a bind pose
    // its palette no longer maps to the identity -- the mesh is CARRIED to its
    // new proportions by the skinning. Reading the stored vertices there
    // measures the model as it arrived from its author, i.e. the one state the
    // fit was supposed to change (Rule 47: the instrument must not lose its
    // subject exactly when the effect is present). First run of this judge did
    // exactly that and reported a 25.68-head figure.
    const dfn::anim::Rig rig =
        dfn::anim::Rig::build(dfn::anim::RigProportions::from_config());
    const dfn::anim::SkinnedRigBinding skinned =
        dfn::anim::bind_skinned_rig(rig, obj->skeleton);
    std::vector<glm::mat4> palette(obj->skeleton.size());
    dfn::anim::skinning_palette(rig, obj->skeleton, skinned, dfn::anim::LocalPose{},
                                palette);
    float lo = 0.0f;
    float hi = 0.0f;
    for (std::size_t i = 0; i < obj->skin.vertices.size(); ++i) {
        const float y = dfn::anim::cpu_skin_position(obj->skin.vertices[i], palette).y;
        lo = i == 0 ? y : std::min(lo, y);
        hi = i == 0 ? y : std::max(hi, y);
    }
    const float height = hi - lo;
    if (height < 1e-3f) {
        std::fprintf(stderr, "[scale] \"%s\" has no height\n", path.c_str());
        return 1;
    }

    // JOINTS IN THE REST FRAME, for the same reason the mesh is: a T-posed
    // model's bind-pose shoulders sit where its arms were bound, not where
    // the figure stands.
    const std::size_t n = obj->skeleton.size();
    std::vector<glm::mat4> model(n);
    dfn::anim::rest_model_matrices(rig, obj->skeleton, skinned, dfn::anim::LocalPose{},
                                   model);

    using dfn::anim::Bone;
    using dfn::anim::bone_index;
    const auto at = [&](Bone b) {
        return joint_pos(model, bind.joint[bone_index(b)]);
    };
    const glm::vec3 neck = at(Bone::Head);
    const glm::vec3 shoulder_l = at(Bone::UpperArmL);
    const glm::vec3 shoulder_r = at(Bone::UpperArmR);
    const glm::vec3 elbow_l = at(Bone::ForearmL);
    const glm::vec3 wrist_l = at(Bone::HandL);
    const glm::vec3 hip = at(Bone::Pelvis);
    const glm::vec3 knee_l = at(Bone::ShinL);
    const glm::vec3 ankle_l = at(Bone::FootL);

    const dfn::anim::RigProportions canon = dfn::anim::RigProportions::from_config();
    const float H = static_cast<float>(dfn::config::PLAYER_CAPSULE_HEIGHT);
    const std::vector<Landmark> rows{
        // Heights from the SOLE (lo), as fractions of the figure's height.
        {"head height (crown - neck)", (hi - neck.y) / height, canon.head_height / H},
        {"neck height", (neck.y - lo) / height, canon.neck_height / H},
        {"shoulder height", ((shoulder_l.y + shoulder_r.y) * 0.5f - lo) / height,
         canon.shoulder_height / H},
        {"hip height", (hip.y - lo) / height, canon.hip_height / H},
        {"knee height", (knee_l.y - lo) / height, canon.knee_height / H},
        {"ankle height", (ankle_l.y - lo) / height, canon.ankle_height / H},
        // Segment lengths, joint to joint.
        {"upper arm", glm::length(elbow_l - shoulder_l) / height,
         canon.upper_arm_length / H},
        {"forearm", glm::length(wrist_l - elbow_l) / height, canon.forearm_length / H},
        {"shoulder width", glm::length(shoulder_l - shoulder_r) / height,
         canon.shoulder_width / H},
    };

    const float heads = height / std::max(hi - neck.y, 1e-4f);
    int failures = 0;
    if (!quiet) {
        std::printf("[scale] \"%s\": %s\n", path.c_str(), obj->name.c_str());
        std::printf("[scale] figure height %.3f m (model units), %.2f heads "
                    "(canon 7.5-8.0)\n",
                    static_cast<double>(height), static_cast<double>(heads));
        std::printf("        %-28s %8s %8s %9s\n", "landmark", "model", "canon",
                    "delta");
    }
    for (const Landmark& r : rows) {
        const float rel = r.canon > 1e-6f ? (r.measured - r.canon) / r.canon : 0.0f;
        const bool bad = std::fabs(rel) > tolerance;
        failures += bad ? 1 : 0;
        if (!quiet) {
            std::printf("        %-28s %8.3f %8.3f %+8.1f%% %s\n", r.name,
                        static_cast<double>(r.measured), static_cast<double>(r.canon),
                        static_cast<double>(rel * 100.0f), bad ? "<-- OUT" : "");
        }
    }
    // HEADS-PER-FIGURE IS JUDGED SEPARATELY and its band is the artistic one
    // (HUMAN_SCALE.md: 7.5-8 heads), not a percentage of a fraction: it is the
    // single number a person reads off a silhouette, and the stylised chibi
    // that started this rule fails HERE first and by a mile.
    const bool heads_bad = heads < 7.5f || heads > 8.0f;
    failures += heads_bad ? 1 : 0;
    if (heads_bad && !quiet) {
        std::printf("        %-28s %8.2f %8s %9s <-- OUT\n", "heads per figure",
                    static_cast<double>(heads), "7.5-8.0", "");
    }
    if (failures > 0) {
        std::fprintf(stderr,
                     "[scale] %d landmark(s) outside +-%.0f%% of the canon "
                     "(docs/design/HUMAN_SCALE.md) — REFUSED as a VISIBLE "
                     "character. A model may still be a fine test fixture.\n",
                     failures, static_cast<double>(tolerance * 100.0f));
        return 1;
    }
    if (!quiet) {
        std::printf("[scale] every landmark within +-%.0f%% of the canon\n",
                    static_cast<double>(tolerance * 100.0f));
    }
    return 0;
}
