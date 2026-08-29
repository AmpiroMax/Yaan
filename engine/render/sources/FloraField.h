/*
Module: engine/render
File: engine/render/sources/FloraField.h

Responsibility:
- FORWARDING HEADER ONLY. The clump field moved to core's zone
  (engine/core/math/sources/FloraField.h) the day WorldgenScatter became its
  consumer: `world` may not include `render`, so the field could not stay here
  and be placed from. Flora authored it, core reviewed and now owns it.

Key items:
- `using` declarations importing dfn::math's names into dfn::render, so flora's
  existing call sites and suite keep compiling against the ONE definition.

Dependencies:
- Uses: engine/core/math/sources/FloraField.h.
- Used by: FloraEdgeRules.h, ProcFlora, ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ADD NOTHING HERE. A second definition of any of these names is the exact
  failure this move exists to prevent — the field decides where cover bunches
  and core places against it, so a render-side copy would put flora's drifts
  and core's instances on different ground. Edit the core header.
- This file is a TRANSITION: it may be deleted the moment flora's includes
  point at engine/core/math/sources/FloraField.h directly.
*/

#pragma once

#include "engine/core/math/sources/FloraField.h"

namespace dfn::render {

using math::ClumpClass;
using math::ClumpParams;
using math::CLUMP_CLASS_COUNT;
using math::clump_field;
using math::clump_params;
using math::clump_raw;
using math::mushroom_ring_offsets;
/// The un-equalized noise, imported by name because flora's Rule 31 CONTROL is
/// built from it: the acceptance is "the raw field is uniform", and the case
/// that must FAIL it is the same noise without cdf_u. A control that cannot be
/// reached is not a control.
namespace clump_detail = math::clump_detail;

/// The tombstone flora left when clump_field_edged() was deleted, kept here
/// because this is where a reader looking for it will arrive.
///
/// `clump_field_edged()` computed its OWN edge ramp. Core also applies
/// PathSample::edge, which already carries BR-3's band shape and design's
/// per-class maintenance scoping — so the two together SQUARED the band and
/// moved its peak inward, and the symptom would have been «обочина жидковата»,
/// which nobody diagnoses as a units bug. The gradient is now applied exactly
/// once, by the caller, against plain clump_field().
///
/// Its two invariants became COMPOSITION-level properties and moved to core's
/// suite with their discriminating cases (WorldgenScatter's tests):
///   (1) the edge floor never SUBTRACTS — the composed value is >= the bare
///       clump value everywhere;
///   (2) a kept verge is not bare ground — at maintenance weight 0 the margin
///       falls back to the FIELD value, never to zero. The discriminating case
///       is ground where the field is ZERO: everywhere else "richness scales
///       the peak" and "richness scales the whole density" agree, and a test
///       on ordinary ground passes under both models while proving neither.

} // namespace dfn::render
