/*
Module: engine/anim
File: engine/anim/sources/BodyGaps.h

Responsibility:
- ЗАЗОРЫ МЕЖДУ ЧАСТЯМИ ОДНОГО ТЕЛА, в одной позе, двумя мерами: по МЕШУ (то,
  что видит владелец) и по КОРОБКАМ (то, чем пользуется игра). Один прибор на
  экран создания, смотровую, стенд, решатель рест-позы и приёмку — чтобы
  «на экране ноги слиплись» и «на стенде 0 пересечений» не оказались двумя
  разными измерениями двух разных поз (правило 35, правило 47).

Key items:
- SkinParts / label_skin_parts(): какая вершина скина к какой кости рига
  относится (ближайший связанный предок самой тяжёлой кости — то же правило,
  что у подгонки коробок и у судьи пропорций).
- BodyGaps / measure_body_gaps(): пары нога↔нога, кисть↔бедро, предплечье↔
  корпус по мешу (ЗНАКОВЫЙ боковой зазор по полосам высоты плюс минимум по
  парам вершин) и те же пары по коробкам (hitbox_pair_distance).
- BodyGapTargets / gaps_meet(): пороги владельца (строки REST_GAP_*) и вердикт.
- describe_gaps(): строка для журнала и отчёта; bind_pose_sample(): поза
  привязки как сэмпл (то, что рисует смотровая с единичной палитрой).

Dependencies:
- Uses: Rig, SkinnedBody (JointLocal, binding), Hitbox, core skeleton,
  platform render (SkinnedVertex), generated constants (REST_GAP_*).
- Used by: RestFit (решатель), engine/app (экран создания, смотровая,
  журнал), tests (character_stance, app_character_path), tools.

Notes:
- ПОЧЕМУ БОКОВОЙ ЗАЗОР ПО ПОЛОСАМ, А НЕ ТОЛЬКО МИНИМУМ ПО ПАРАМ ВЕРШИН.
  Минимум расстояния между вершинами двух мешей НЕ УМЕЕТ БЫТЬ ОТРИЦАТЕЛЬНЫМ:
  два бедра, вошедшие друг в друга на три сантиметра, дают положительное
  число — расстояние между ближайшими вершинами двух пересекающихся
  поверхностей. Именно так «ноги слиплись» проходило прибор, у которого
  «зазор 3.2 см». Боковой зазор считается по полосам высоты: в каждой полосе,
  где обе части присутствуют и перекрываются по глубине, берётся внутренний
  край наружной части минус наружный край внутренней; отрицательное число —
  это глубина взаимопроникновения, и оно печатается со знаком.
- МИНИМУМ ПО ПАРАМ ВЕРШИН ОСТАЁТСЯ для поз, где части не стоят бок о бок
  (кисть, ушедшая вперёд на махе): там полос перекрытия нет, и боковой зазор
  честно отвечает «не рядом», а пары вершин дают расстояние.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8, Rule 30): no ECS, no IO, no clock.
- Пороги — строки NUMBERS (REST_GAP_*), не литералы здесь: их читают решатель
  и приёмка, то есть двое (правило 35).
*/

#pragma once

#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace dfn::anim {

/// КАКАЯ ВЕРШИНА К КАКОЙ КОСТИ РИГА ОТНОСИТСЯ. Считается один раз на тело:
/// веса скина от позы не зависят.
struct SkinParts {
    /// Кость рига на вершину, -1 — ни к одной (вершина без веса или сустав,
    /// не имеющий связанного предка).
    std::vector<int8_t> bone_of_vertex;
    /// Индексы вершин по костям, в порядке возрастания.
    std::array<std::vector<uint32_t>, BONE_COUNT> by_bone{};
    [[nodiscard]] bool valid() const { return !bone_of_vertex.empty(); }
};

[[nodiscard]] SkinParts label_skin_parts(const skel::Skeleton& skeleton,
                                         const SkinnedRigBinding& binding,
                                         std::span<const platform::SkinnedVertex> vertices);

/// ОДИН ЗАЗОР ПО МЕШУ: боковой знаковый по полосам и минимум по парам вершин.
struct MeshGap {
    /// Боковой зазор, метры, ОТРИЦАТЕЛЬНЫЙ при взаимопроникновении. NaN, если
    /// части нигде не стоят бок о бок (ни одной полосы высоты с перекрытием по
    /// глубине) — тогда единственный ответ даёт `pair_m`.
    float lateral_m = std::numeric_limits<float>::quiet_NaN();
    /// Минимум расстояния между вершинами двух частей, метры. Всегда ≥ 0.
    float pair_m = std::numeric_limits<float>::infinity();
    /// Сколько полос высоты участвовало в боковом замере.
    uint32_t bands = 0;
    /// ЧЕМ СУДИТЬ: боковым, где он есть, иначе парами вершин.
    [[nodiscard]] float judged_m() const;
};

struct BodyGaps {
    // --- МЕШ --------------------------------------------------------------
    /// Левая нога (бедро + голень) против правой.
    MeshGap legs;
    /// Кисть против бедра своей стороны. [0] левая.
    std::array<MeshGap, 2> hand_thigh{};
    /// Предплечье против корпуса (таз + туловище). [0] левое.
    std::array<MeshGap, 2> forearm_trunk{};
    // --- КОРОБКИ (hitbox_pair_distance, 0 = пересечение) --------------------
    float legs_box_m = 0.0f;          ///< min(бедро-бедро, голень-голень)
    std::array<float, 2> hand_thigh_box_m{};
    std::array<float, 2> hand_hips_box_m{};
    std::array<float, 2> forearm_abdomen_box_m{};
    bool valid = false;

    [[nodiscard]] float hand_thigh_worst_m() const;
    [[nodiscard]] float forearm_trunk_worst_m() const;
};

/// Зазоры одной позы. `parts` — от label_skin_parts на тех же вершинах;
/// `boxes` — таблица, УЖЕ подогнанная по коже (fit_hitboxes_to_skin), иначе
/// половина коробок описывает канон, а не это тело.
[[nodiscard]] BodyGaps measure_body_gaps(const skel::Skeleton& skeleton,
                                         const SkinnedRigBinding& binding,
                                         const HitboxSet& boxes,
                                         std::span<const platform::SkinnedVertex> vertices,
                                         const SkinParts& parts,
                                         std::span<const JointLocal> sample);

/// ПОРОГИ ВЛАДЕЛЬЦА (02.09): нога↔нога ≥ 2 см, кисть↔бедро ≥ 1.5 см,
/// предплечье↔корпус ≥ 2 см — по МЕШУ. Читаются из строк реестра.
struct BodyGapTargets {
    float legs_m = 0.0f;
    float hand_thigh_m = 0.0f;
    float forearm_trunk_m = 0.0f;
    [[nodiscard]] static BodyGapTargets from_config();
};

/// Все три пары не ниже порога.
[[nodiscard]] bool gaps_meet(const BodyGaps& gaps, const BodyGapTargets& targets);

/// ОДНА СТРОКА ЖУРНАЛА/ОТЧЁТА: все пары в сантиметрах, мешем и коробками.
[[nodiscard]] std::string describe_gaps(const BodyGaps& gaps);

/// ПОЗА ПРИВЯЗКИ МОДЕЛИ как JointLocal — то, что показывает смотровая, когда
/// рисует скин с единичной палитрой. out.size() >= skeleton.size().
void bind_pose_sample(const skel::Skeleton& skeleton, std::span<JointLocal> out);

} // namespace dfn::anim
