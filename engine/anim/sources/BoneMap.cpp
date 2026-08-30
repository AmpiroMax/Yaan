/*
Module: engine/anim
File: engine/anim/sources/BoneMap.cpp

Responsibility:
- The synonym table itself and the two functions over it.

Dependencies:
- Uses: BoneMap.h.
- Used by: dfn_anim.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The table is DATA in code, and the entries are grouped by bone so a reader
  can see at a glance which naming families are covered.
*/

#include "engine/anim/sources/BoneMap.h"

#include <algorithm>
#include <cctype>

namespace dfn::anim {
namespace {

struct Synonym {
    const char* normalized;
    Bone bone;
    /// 0 = the ANATOMICALLY EXACT name for this bone, 1 = an acceptable
    /// stand-in. bind_skeleton prefers rank 0 wherever the skeleton offers
    /// both, and this is not tidiness: Rigify ships DEF-neck AND DEF-head,
    /// our Head bone's joint sits at the BASE OF THE SKULL (docs/RIG.md), and
    /// binding it to the neck instead measured the head as 0.187 of the
    /// figure — 5.35 heads tall, which the proportion judge correctly called
    /// a cartoon. One rank column, and the same model reads as a person.
    uint8_t rank = 0;
};

// Naming families covered, and where each came from:
//   * Mixamo / Blender Rigify        -- hips, spine, leftarm, leftupleg...
//   * Blender Rigify / Quaternius   -- DEF-hips, DEF-spine.001, DEF-neck,
//                                       DEF-upper_arm.L, DEF-thigh.L ("DEF-"
//                                       is stripped as an authoring prefix)
//   * KayKit (the reference knight)  -- hips, spine, chest, upperarm.l,
//                                       lowerarm.l, hand.l, upperleg.l,
//                                       lowerleg.l, foot.l
//   * Khronos sample assets          -- torso_joint_1, arm_joint_L_2,
//                                       leg_joint_R_3 (RiggedFigure, CesiumMan)
//   * the plain-English family       -- upperarml, thighr, lowerlegl
// A name that appears in none of them is not an error: it is a joint we have
// no bone for, and bind_skeleton leaves it unbound (see the header).
// "root" IS DELIBERATELY NOT A SYNONYM FOR THE PELVIS, and it cost a bug to
// know why. Half the rigs in circulation put a "root" bone on the FLOOR under
// the character, as the parent of the hips -- it exists to move the character,
// not to be its pelvis. bind_skeleton takes the ROOT-MOST match, so listing it
// would have handed the pelvis to the floor bone and left the real hips
// unbound: the body would rotate whole where it should have swayed.
constexpr Synonym SYNONYMS[] = {
    // --- centre line ---------------------------------------------------------
    {"hips", Bone::Pelvis},        {"hip", Bone::Pelvis},
    {"pelvis", Bone::Pelvis},      {"torsojoint1", Bone::Pelvis},
    {"spine", Bone::Torso},        {"spine1", Bone::Torso},
    {"spine01", Bone::Torso},      {"chest", Bone::Torso, 1},
    // Rigify numbers its spine chain "DEF-spine.001..003"; the root-most one
    // takes the Torso and the rest ride along (see the header).
    {"spine001", Bone::Torso},     {"spine002", Bone::Torso},
    {"spine003", Bone::Torso},
    {"upperchest", Bone::Torso, 1},   {"torso", Bone::Torso},
    {"torsojoint2", Bone::Torso},  {"abdomen", Bone::Torso},
    {"head", Bone::Head},          {"neck", Bone::Head, 1},
    {"neck1", Bone::Head, 1},         {"neckjoint1", Bone::Head, 1},
    {"skull", Bone::Head},
    // --- left arm ------------------------------------------------------------
    {"leftarm", Bone::UpperArmL},      {"upperarml", Bone::UpperArmL},
    {"lupperarm", Bone::UpperArmL},    {"leftupperarm", Bone::UpperArmL},
    {"armjointl1", Bone::UpperArmL},   {"larm", Bone::UpperArmL},
    {"leftforearm", Bone::ForearmL},   {"forearml", Bone::ForearmL},
    {"lforearm", Bone::ForearmL},      {"leftlowerarm", Bone::ForearmL},
    {"lowerarml", Bone::ForearmL},     {"armjointl2", Bone::ForearmL},
    {"lefthand", Bone::HandL},         {"handl", Bone::HandL},
    {"lhand", Bone::HandL},            {"armjointl3", Bone::HandL},
    // --- right arm -----------------------------------------------------------
    {"rightarm", Bone::UpperArmR},     {"upperarmr", Bone::UpperArmR},
    {"rupperarm", Bone::UpperArmR},    {"rightupperarm", Bone::UpperArmR},
    {"armjointr1", Bone::UpperArmR},   {"rarm", Bone::UpperArmR},
    {"rightforearm", Bone::ForearmR},  {"forearmr", Bone::ForearmR},
    {"rforearm", Bone::ForearmR},      {"rightlowerarm", Bone::ForearmR},
    {"lowerarmr", Bone::ForearmR},     {"armjointr2", Bone::ForearmR},
    {"righthand", Bone::HandR},        {"handr", Bone::HandR},
    {"rhand", Bone::HandR},            {"armjointr3", Bone::HandR},
    // --- left leg ------------------------------------------------------------
    {"leftupleg", Bone::ThighL},       {"leftthigh", Bone::ThighL},
    {"thighl", Bone::ThighL},          {"lthigh", Bone::ThighL},
    {"upperlegl", Bone::ThighL},       {"leftupperleg", Bone::ThighL},
    {"legjointl1", Bone::ThighL},
    {"leftleg", Bone::ShinL},          {"leftshin", Bone::ShinL},
    {"shinl", Bone::ShinL},            {"lshin", Bone::ShinL},
    {"leftcalf", Bone::ShinL},         {"calfl", Bone::ShinL},
    {"lowerlegl", Bone::ShinL},        {"leftlowerleg", Bone::ShinL},
    {"legjointl2", Bone::ShinL},
    {"leftfoot", Bone::FootL},         {"footl", Bone::FootL},
    {"lfoot", Bone::FootL},            {"leftankle", Bone::FootL},
    {"legjointl3", Bone::FootL},
    // --- right leg -----------------------------------------------------------
    {"rightupleg", Bone::ThighR},      {"rightthigh", Bone::ThighR},
    {"thighr", Bone::ThighR},          {"rthigh", Bone::ThighR},
    {"upperlegr", Bone::ThighR},       {"rightupperleg", Bone::ThighR},
    {"legjointr1", Bone::ThighR},
    {"rightleg", Bone::ShinR},         {"rightshin", Bone::ShinR},
    {"shinr", Bone::ShinR},            {"rshin", Bone::ShinR},
    {"rightcalf", Bone::ShinR},        {"calfr", Bone::ShinR},
    {"lowerlegr", Bone::ShinR},        {"rightlowerleg", Bone::ShinR},
    {"legjointr2", Bone::ShinR},
    {"rightfoot", Bone::FootR},        {"footr", Bone::FootR},
    {"rfoot", Bone::FootR},            {"rightankle", Bone::FootR},
    {"legjointr3", Bone::FootR},
};

// Authoring prefixes that carry no anatomy. Stripped from the FRONT only: a
// name is allowed to contain "def" in the middle of a real word.
constexpr const char* PREFIXES[] = {"mixamorig", "armature", "skeleton",
                                    "bip01", "bip", "def", "ctrl", "bone"};

} // namespace

std::string normalize_bone_name(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (const char c : raw) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) != 0) {
            out.push_back(static_cast<char>(std::tolower(uc)));
        }
    }
    // Strip prefixes repeatedly: "Armature_mixamorig:Hips" is a real name.
    bool stripped = true;
    while (stripped) {
        stripped = false;
        for (const char* p : PREFIXES) {
            const std::string_view pv{p};
            if (out.size() > pv.size() && out.compare(0, pv.size(), pv) == 0) {
                out.erase(0, pv.size());
                stripped = true;
                break;
            }
        }
    }
    return out;
}

std::optional<Bone> bone_from_joint_name(std::string_view raw) {
    const std::string key = normalize_bone_name(raw);
    for (const Synonym& s : SYNONYMS) {
        if (key == s.normalized) {
            return s.bone;
        }
    }
    return std::nullopt;
}

uint8_t joint_name_rank(std::string_view raw) {
    const std::string key = normalize_bone_name(raw);
    for (const Synonym& s : SYNONYMS) {
        if (key == s.normalized) {
            return s.rank;
        }
    }
    return 0xFF;
}

SkeletonBinding bind_skeleton(const skel::Skeleton& skeleton) {
    SkeletonBinding binding;
    std::array<uint8_t, BONE_COUNT> best_rank{};
    best_rank.fill(0xFF);
    // ROOT-MOST WINS, and the joints are stored parent-before-child, so a
    // single forward pass with "first writer keeps it" IS root-most: the
    // second joint of a three-joint spine never overwrites the first.
    for (std::size_t i = 0; i < skeleton.joints.size(); ++i) {
        const auto bone = bone_from_joint_name(skeleton.joints[i].name);
        if (!bone.has_value()) {
            continue;
        }
        const uint32_t bi = bone_index(*bone);
        const uint8_t rank = joint_name_rank(skeleton.joints[i].name);
        if (binding.joint[bi] >= 0 && rank >= best_rank[bi]) {
            continue; // already bound by an equal or better name, root-most
        }
        if (binding.joint[bi] < 0) {
            ++binding.bound_count;
        }
        binding.joint[bi] = static_cast<int32_t>(i);
        best_rank[bi] = rank;
    }
    return binding;
}

} // namespace dfn::anim
