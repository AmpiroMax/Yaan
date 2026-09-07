/*
Module: engine/anim
File: engine/anim/sources/LookLayer.h

Responsibility:
- СЛОЙ ВЗГЛЯДА (LOCOMOTION_GROUNDED.md §13.2, §14): разница «взгляд − корпус»
  раскладывается по цепочке позвоночник → шея → голова как поворот вокруг
  вертикали, чтобы стоящий человек оглядывался через плечо, а не стоял
  каменным до самого переступа. До TURN_FIRE_DEG разницу держит этот слой;
  дальше стреляет клип поворота, и слой отдаёт угол обратно по мере доворота.
- Веса по звеньям (LOOK_SHARE_*) — доли полного угла: грудь меньше, голова
  больше, сумма 1; предел LOOK_MAX_DEG, сглаживание LOOK_SMOOTH_S.

Key items:
- LookLayer / build_look_layer(): индексы звеньев по именам скелета.
- apply_look(): поворот звеньев вокруг вертикали в системе тела на доли угла;
  weight 0 — побитовое тождество (контрольная рука).

Dependencies:
- Uses: SkinnedBody.h (JointLocal), skeleton, glm.
- Used by: ClipPlayer (playback_sample), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Слой правит ТОЛЬКО ориентацию звеньев корпуса; ноги, таз, руки не трогает.
*/
#pragma once

#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include <array>
#include <cstdint>
#include <span>

namespace dfn::anim {

struct LookLayer {
    /// Звенья снизу вверх: spine.001, spine.002, spine.003, neck, head (−1 — нет).
    std::array<int32_t, 5> chain{-1, -1, -1, -1, -1};
    /// Доля полного угла на звено, сумма 1 (LOOK_SHARE_SPINE ×3, NECK, HEAD).
    std::array<float, 5> share{};
    [[nodiscard]] bool valid() const { return chain[4] >= 0; }
};

[[nodiscard]] LookLayer build_look_layer(const skel::Skeleton& skeleton);

/// Повернуть звенья корпуса вокруг вертикали системы тела на `yaw` (рад, +
/// по часовой, как рыск сим'а) по долям слоя, ослабленно `weight` (0..1).
void apply_look(const skel::Skeleton& skeleton, const LookLayer& layer, float yaw,
                float weight, std::span<JointLocal> sample);

/// Повернуть цепочку вокруг ПРОИЗВОЛЬНОЙ оси системы тела (единичной) на
/// `angle` по долям слоя — наклон корпуса по толчку (ярус 0 реакций), тот
/// же механизм, что взгляд; apply_look — частный случай с осью Y.
void apply_chain_rotation(const skel::Skeleton& skeleton, const LookLayer& layer,
                          const glm::vec3& axis_model, float angle, float weight,
                          std::span<JointLocal> sample);

} // namespace dfn::anim
