/*
Module: tests/character
File: tests/character/ClipTestModel.h

Responsibility:
- The model the imported-clip tests measure on, loaded ONE way: the baked
  HumanBase.dfo, the rest pose solved on its skin (RestFit), the skinned
  binding and the clip library — plus sim's step model restated for the
  tests, so the two suites that read it (ClipPlayerTests.cpp,
  ClipSlideTests.cpp) cannot drift apart in what they call "the body".

Key items:
- Model / load(): the object, rig, binding, library.
- step_length(): sim's length(v) from the two registry rows, not a copy.
- clip_name_of(), pelvis_half_width(), pose_distance(): the small readers.

Dependencies:
- Uses: engine/anim (ClipPlayer, RestFit, Rig, SkinnedBody), engine/render
  (.dfo reader), generated constants.
- Used by: tests/character/ClipPlayerTests.cpp, tests/character/ClipSlideTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Test-only header: an anonymous namespace, one copy per translation unit, no
  engine code includes it.
*/

#pragma once

#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/RestFit.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace dfn;

constexpr const char* MODEL = "assets/objects/characters/HumanBase.dfo";

/// SIM'S OWN STEP MODEL, restated here and NOT included: gameplay sits ABOVE
/// anim in the DAG, and a character test that links it would be the first
/// edge that makes the graph a cycle. The two rows it reads
/// (STEP_LENGTH_BASE, STEP_LENGTH_PER_MPS) are generated constants, so this is
/// one reader of a registry row rather than a second copy of a number.
[[nodiscard]] float step_length(float speed) {
    return static_cast<float>(config::STEP_LENGTH_BASE)
           + static_cast<float>(config::STEP_LENGTH_PER_MPS) * speed;
}

struct Model {
    render::RegistryObject obj;
    /// THE RIG THE GAME SHIPS, built from the NUMBERS rows, and NOT a
    /// default-constructed one.
    ///
    /// A `Rig{}` has zero proportions and identity rest rotations, so the
    /// retarget carries the model into a T-POSE and calls it our rest pose:
    /// measured, the hand then hangs 0.821 m from the pelvis centre instead of
    /// 0.230, which is the very number item 3 of the owner's list is about.
    /// Every measurement below is against the rest pose, so an unbuilt rig is
    /// an instrument calibrated on a body nobody draws (Rule 47).
    anim::Rig rig = anim::Rig::build(anim::RigProportions::from_config());
    anim::SkinnedRigBinding binding;
    anim::ClipLibrary lib;
};

/// `feet_drive` = true строит библиотеку НОВОГО шва (часы клипа в anim,
/// перемещение от стопы, docs/design/LOCOMOTION_GROUNDED.md); по умолчанию —
/// ПРЕЖНИЙ шов (фаза сим'а, стрид-скейл): наборы слоёв (стойка, обход рук,
/// зеркало, IK стоп) снимают позы НА ЗАДАННОЙ ФАЗЕ через drive.stride_phase, и
/// это их предмет; шов проверяют ClipSlideTests и app_grounded_locomotion.
/// role_overrides — «Walk=Walk_Loop,…»: прибор, характеризующий КОНКРЕТНЫЙ клип
/// (зеркало, спина, контрольная рука подгонки шага), называет его по имени и
/// не зависит от ролей по умолчанию (с 04.09 ходьба/трусца — Mixamo).
/// `transitions` — одноразовые клипы перехода (§13). Прибор, характеризующий
/// САМ ЦИКЛ (размах, снос, темп, крест стоп), выключает их: иначе первые
/// полсекунды каждого прогона — клип старта, и мерился бы он.
[[nodiscard]] bool load(Model& m, bool feet_drive = false, std::string_view role_overrides = {},
                        bool transitions = false) {
    if (!std::filesystem::exists(MODEL)) {
        return false;
    }
    auto o = render::read_object(MODEL);
    if (!o.has_value() || o->skeleton.empty() || o->clips.empty()) {
        return false;
    }
    m.obj = std::move(*o);
    // ОДНА РЕСТ-ПОЗА НА ТЕЛО (RestFit.h): та же, которой тело рисуют экран
    // создания и мир — иначе стенд судил бы позу, которой никто не видит.
    m.rig = anim::rest_rig_for(m.obj.skeleton, m.obj.skin.vertices);
    m.binding = anim::bind_skinned_rig(m.rig, m.obj.skeleton);
    m.lib = anim::build_clip_library(m.rig, m.obj.skeleton, m.binding, m.obj.clips,
                                     m.obj.skin.vertices, feet_drive, role_overrides);
    m.lib.transitions = transitions;
    return true;
}

[[nodiscard]] std::string clip_name_of(const Model& m, anim::ClipRole r) {
    const anim::ClipEntry& e = m.lib[r];
    return e.present() ? m.obj.clips[static_cast<std::size_t>(e.clip)].name
                       : std::string{};
}

/// THE BODY'S OWN PELVIS HALF-WIDTH, metres: the hip joint's offset from the
/// body axis in the rest pose this file measures everything else in.
///
/// WHY THIS NUMBER EXISTS AT ALL (owner's decision, 01.09). The visible
/// HumanBase now ships RAW — no --fit-canon, no --reshape — because the owner
/// compared the raw asset with the one the game baked and kept the raw one.
/// Its skeleton is its author's, not the canon's: the hip joints sit 0.085 m
/// off the axis where the canon-fitted body had them at 0.145. Every band in
/// this file that was written in METRES off the canon therefore broke at once,
/// and the honest repair is not a smaller number — it is to ask the body how
/// wide it is. A band in the model's own units survives the next body too.
[[nodiscard]] float pelvis_half_width(const Model& m) {
    std::vector<glm::mat4> model(m.obj.skeleton.size());
    anim::rest_model_matrices(m.rig, m.obj.skeleton, m.binding, anim::LocalPose{},
                              model);
    const auto at = [&](anim::Bone b) {
        const int32_t j = m.binding.names.joint[anim::bone_index(b)];
        return j >= 0 ? glm::vec3{model[static_cast<std::size_t>(j)][3]}
                      : glm::vec3{0.0f};
    };
    return std::abs(at(anim::Bone::ThighL).x - at(anim::Bone::Pelvis).x);
}

/// The largest distance any joint moved between two samples — a one-number
/// answer to "is this the same pose".
[[nodiscard]] float pose_distance(std::span<const anim::JointLocal> a,
                                  std::span<const anim::JointLocal> b) {
    float worst = 0.0f;
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const float d = 1.0f - std::abs(glm::dot(a[i].rotation, b[i].rotation));
        worst = std::max(worst, d);
        worst = std::max(worst, glm::length(a[i].translation - b[i].translation));
    }
    return worst;
}

} // namespace
