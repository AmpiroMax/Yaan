/*
Module: engine/anim
File: engine/anim/sources/RestFit.h

Responsibility:
- THE REST POSE OF A BOUND MODEL, SOLVED AGAINST ITS OWN SKIN («поза покоя —
  строевая по швам», owner's order 02.09). Legs vertical under the hips, feet
  flat and forward, arms hanging along the sides with the elbow at
  REST_ELBOW_FLEX — and the arm abduction and the leg splay raised from zero
  until the body's own flesh clears itself by the REST_GAP_* rows, measured
  with the body-gap instrument on the skin.

Key items:
- RestFit / fit_rest_pose(): the solve — returns the rig, its binding, the
  skin-fitted hitboxes, the skin labels and the gaps it ended at.
- rest_rig_for(): the one-line form every reader of a model uses (the
  importer, the judge, the morph tool, the character, the screen, tests) so
  there is exactly one rest pose per body in the tree.

Dependencies:
- Uses: Rig, SkinnedBody (the retarget), Hitbox (fit_hitboxes_to_skin),
  BodyGaps (the instrument and the REST_GAP_* targets), core skeleton.
- Used by: engine/app (SkinnedCharacter, CharGenBody), tools (import_gltf,
  morph_tool, check_human_scale), tests.

Notes:
- WHY THE REST IS SOLVED AND NOT AUTHORED. The old rest converged the legs by
  an angle derived for the BOX RIG, whose hip joints sit on the skin 0.334 m
  apart; applied through the retarget to a model whose hip joints are 0.17 m
  apart, the same angle crossed the ankles INSIDE the body's axis — measured
  on the character screen: the legs 8.99 cm into each other, the hand 2.92 cm
  inside the thigh. A rest that is right for one skeleton is a number; a rest
  that is right for THIS skeleton is a measurement, and it has to be taken on
  the skin, because a joint has no radius (the lesson the hitbox wave paid
  for twice).
- THE SOLVE IS A LEVER, NOT A SCAN. A gap short by d centimetres at a band
  L metres below the joint the limb turns about wants asin(d / L) of turn;
  the legs share the deficit, d / 2 each. L is the distance from the joint
  to the WORST BAND of the gap (MeshGap::worst_y), not the whole limb: on
  the MPFB body the legs are tightest 6 cm under the hip (the crotch), and
  the whole-leg lever asked for a fifteenth of the angle. Each pass
  re-measures after the retarget (the model's own segments, not the
  canon's), so the small non-linearity is closed by iteration.
- LEGS FIRST, THEN ARMS, each lever-then-bisect. The splay carries the
  thighs toward the hanging hands (2 mm of hand-thigh gap per degree on
  this body); the abduction moves nothing the legs measure. Solving in that
  order closes the coupling in one direction.
- MONOTONE, from zero upward. The solver never pulls a limb IN: a body whose
  hands already clear the thighs at zero abduction keeps its arms dead
  vertical, which is the tightest «по швам» there is.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8, Rule 30): no ECS, no IO, no clock;
  deterministic — the same skin gives the same rest to the bit, which the
  morph targets and the baseline both depend on.
*/

#pragma once

#include "engine/anim/sources/BodyGaps.h"
#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <cstdint>
#include <span>

namespace dfn::anim {

/// How many lever passes ONE ANGLE may take before the solve reports what it
/// has. The legs and the arms are solved one after the other (legs first:
/// the splay moves the thighs toward the hands, the abduction moves nothing
/// the legs care about), each with its own budget; `RestFit::passes` counts
/// every rebuild of both, tightening included.
inline constexpr uint32_t REST_FIT_MAX_PASSES = 12;

struct RestFit {
    Rig rig;
    SkinnedRigBinding binding;
    HitboxSet boxes;
    SkinParts parts;
    /// The gaps at the rest the solve ended at, on the skin.
    BodyGaps gaps;
    BodyGapTargets targets;
    uint32_t passes = 0;
    /// True when every target was met AND the game's own boxes of the same
    /// pairs (thigh-thigh, hand-thigh) no longer intersect; false when the
    /// passes ran out or the model has no skin to measure (then `rig` is the
    /// unfitted attention rest and `gaps.valid` is false).
    bool met = false;
};

/// WHAT THE SOLVE CALLS CLEAR: the REST_GAP_* rows on the skin (gaps_meet)
/// and, on top of them, the hitboxes the game plays with — thigh against
/// thigh, hand against thigh — not touching. The boxes are wider than the
/// flesh at the hand's height (a thigh's box is as wide as its widest point,
/// the buttock, and the hand hangs beside its middle), so a rest that clears
/// the skin by the owner's 1.5 cm can still start every frame with the hand
/// INSIDE the box the arm layer steers by. One predicate for the solver and
/// the acceptance, so the two cannot disagree about what «по швам» means.
[[nodiscard]] bool rest_pose_clear(const BodyGaps& gaps, const BodyGapTargets& targets);

/// The solve. `legacy` = the box body's converged rest, unfitted — the
/// "before" arm of the owner's comparison (Rule 47: both arms from one
/// binary, differing by exactly the rest).
[[nodiscard]] RestFit fit_rest_pose(const RigProportions& p, const skel::Skeleton& skeleton,
                                    std::span<const platform::SkinnedVertex> skin,
                                    const BodyGapTargets& targets = BodyGapTargets::from_config(),
                                    bool legacy = false);

/// THE ONE REST POSE OF A BODY: config proportions, fitted to this skin.
/// Every reader of a model calls this and nothing else, so the importer's
/// grounding, the judge's silhouette, the morph targets' rest space, the
/// character's retarget and the screen's portrait are one pose.
[[nodiscard]] Rig rest_rig_for(const skel::Skeleton& skeleton,
                               std::span<const platform::SkinnedVertex> skin,
                               bool legacy = false);

} // namespace dfn::anim
