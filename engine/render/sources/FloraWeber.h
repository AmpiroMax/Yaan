/*
Module: engine/render
File: engine/render/sources/FloraWeber.h

Responsibility:
- The Weber & Penn tree model ("Creation and Rendering of Realistic Trees",
  SIGGRAPH 1995): a recursive stem grammar with per-LEVEL laws for declination,
  rotation, length, taper, curvature and splitting. Produces a Skeleton; emits
  no triangles.

Key items:
- WeberLevel, WeberParams, weber_skeleton(), weber_shape_ratio().

Dependencies:
- Uses: FloraSkeleton.h (Skeleton, SkeletonNode), glm.
- Used by: ProcFlora (mesh emission), ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md,
  algorithm record docs/specs/flora_algorithms.md.
- PURE AND DETERMINISTIC: same (params, seed) -> byte-identical output.
- THE MODEL IS THE PAPER'S; THE SPECIES NUMBERS ARE OURS AND SAY SO. The
  published parameter tables (Quaking Aspen, Black Tupelo, CA Black Oak...)
  were not available to the agent that wrote this, so nothing here claims to be
  them. Every species row in FloraSpecies.cpp is derived from OUR design briefs
  and is labelled as ours. If those tables are ever obtained, they replace our
  rows wholesale — that is the entire point of using a published model.
*/

#pragma once

#include "engine/render/sources/FloraSkeleton.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

namespace dfn::render {

/// The paper's eight crown SHAPES. They exist to answer one question — how long
/// is a level-1 branch as a function of where it leaves the trunk — and that
/// single function is most of what separates a poplar from an oak. It is NOT a
/// clip: nothing is cut to fit it. The silhouette emerges because the branches
/// that make it are the right lengths, which is the difference between a tree
/// that HAS a shape and a tree that has been PUT IN one.
enum class WeberShape : uint8_t {
    Conical = 0,      ///< 0.2 + 0.8 * ratio
    Spherical = 1,    ///< 0.2 + 0.8 * sin(pi * ratio)
    Hemispherical = 2,///< 0.2 + 0.8 * sin(0.5 * pi * ratio)
    Cylindrical = 3,  ///< 1.0
    TaperedCyl = 4,   ///< 0.5 + 0.5 * ratio
    Flame = 5,        ///< ratio <= 0.7 : ratio/0.7 ; else (1-ratio)/0.3
    InverseConical = 6, ///< 1 - 0.8 * ratio
    TendFlame = 7,    ///< ratio <= 0.7 : 0.5 + 0.5*ratio/0.7 ; else 0.5 + 0.5*(1-ratio)/0.3
};

/// `ratio` is 0 at the top of the trunk and 1 at the bottom of the branched
/// region, exactly as in the paper.
[[nodiscard]] float weber_shape_ratio(WeberShape shape, float ratio);

/// One level of the recursion. The paper indexes these 0..3 (trunk, main
/// branches, secondary, tertiary) and every field is per level because that is
/// the observation the model is built on: a tree's levels do not obey the same
/// law at different scales, they obey DIFFERENT laws. That is why a pure
/// self-similar fractal reads as a fractal and not as a tree.
struct WeberLevel {
    /// Declination from the PARENT's axis, degrees. The single most
    /// recognisable number in the model: 30 deg is a poplar, 90 is an oak limb
    /// leaving horizontally.
    float down_angle = 60.0f;
    /// Variation on it. NEGATIVE has the paper's special meaning: the
    /// declination then depends on WHERE the child sits on its parent, so
    /// branches near the trunk's top point up and those near its base point
    /// out. That one sign is worth more than any amount of jitter.
    float down_angle_v = 0.0f;
    float rotate = 140.0f;  ///< deg around the parent axis between consecutive children
    float rotate_v = 0.0f;
    /// Length as a fraction: of the tree's height at level 0, of the parent's
    /// length at every level below.
    float length = 0.4f;
    float length_v = 0.0f;
    /// 0 = cylinder, 1 = cone, 2 = spherical end, 3 = periodic (the paper's
    /// higher values model bumpy stems; we use 0..1 in practice).
    float taper = 1.0f;
    float curve = 0.0f;      ///< total deg the stem turns over its length
    float curve_back = 0.0f; ///< non-zero: the stem turns one way then the other (S)
    float curve_v = 0.0f;    ///< deg of random wander, per segment
    uint32_t curve_res = 3;  ///< segments the stem is drawn in
    /// Splits per segment, FRACTIONAL on purpose. 0.2 does not mean "a fifth of
    /// a split"; it means one segment in five splits, and the paper's error
    /// accumulation is what makes that exact rather than probabilistic. Stem
    /// splitting is how a broadleaf gets its forked, non-dominant crown, and
    /// it is the mechanism our previous grower had no equivalent of.
    float seg_splits = 0.0f;
    float split_angle = 0.0f;
    float split_angle_v = 0.0f;
    uint32_t branches = 20; ///< children carried by ONE stem of the level above
};

struct WeberParams {
    uint32_t levels = 3;
    /// Tree height in metres. Ours, not the paper's `Scale`: our species height
    /// band is a cross-zone contract, so the size comes from the contract and
    /// the model supplies proportion.
    float height = 20.0f;
    /// Fraction of the trunk carrying NO branches. This is the clear bole, so
    /// it is also what CANOPY_CLEARANCE_MIN is expressed through.
    float base_size = 0.35f;
    float ratio = 0.02f;       ///< trunk radius / trunk length
    float ratio_power = 1.2f;  ///< r_child = r_parent * (len_child/len_parent)^this
    float flare = 0.6f;        ///< root flare; the mesh side already draws one
    /// Upward bias applied to every segment beyond level 1, the paper's
    /// AttractionUp. It is what makes lower branches sweep up at their ends and
    /// is the difference between a live limb and a dead one.
    float attraction_up = 0.5f;
    WeberShape shape = WeberShape::Spherical;
    /// SPLITS AT THE VERY BASE OF THE TRUNK (the paper's `0BaseSplits`), and it
    /// is not a variant of seg_splits. This is the trunk arriving out of the
    /// ground as two or three axes rather than one — `CA Black Oak` carries 2,
    /// and it is where an oak's several main limbs come from instead of one
    /// bole with branches on it. The two-lobed veteran silhouette the user
    /// described («как сиськи») is this parameter, not an envelope: the shape
    /// comes from STRUCTURE, which is what docs/GIANT_OAKS.md §4 argued from
    /// first principles and this published table settles.
    uint32_t base_splits = 0;
    WeberLevel level[4];
    /// Hard ceiling on skeleton size, ours: the paper is not spending a
    /// triangle budget and we are. Growth stops cleanly with what it has.
    uint32_t max_nodes = 400;
    /// Everything below is OUR integration, not the paper's model.
    glm::vec3 base{0.0f};        ///< where the trunk starts, tree-local
    glm::vec2 lean{0.0f};        ///< XZ drift of the whole tree (wind lean)
    float top_y = 1e9f;          ///< absolute ceiling for any node
    float max_radius = 1e9f;     ///< m from the crown axis
    glm::vec2 axis{0.0f};        ///< the crown axis the radius is measured from
    /// Crown shyness, as in FractalParams: a stem that would cross a
    /// neighbour's boundary stops and carries its foliage at the stop.
    const void* crowd = nullptr; ///< FloraShape::CrownEdge[]
    uint32_t crowd_count = 0;
    glm::vec2 crowd_origin{0.0f};
    float crowd_jitter = 0.0f;
    float crowd_inset = 0.0f;
    float crowd_floor = 0.0f;
    /// THE AUTHORED BOLE — ours, and it is the fix for the two-trunk defect
    /// (13.08.2026). The mesh side draws a swept, leaning bole (build_trunk)
    /// and then this model grew its own level-0 axes from the base: 94-100 %
    /// of first-order branch bases measured OUTSIDE the drawn bole's surface
    /// (oak mean 5.30 m, worst 13.78 m off) — «ветки своими углами из
    /// основания торчат» is that number seen from below. When `bole` is set,
    /// level 0 WALKS this polyline instead of growing its own: the trunk the
    /// branches hang on and the trunk the eye sees are one object. Past the
    /// polyline's end the stem continues free — and `base_splits` fork THERE,
    /// as DRAWN leaders (trunk=false), instead of fanning invisibly from the
    /// ground: a bole that splits into leaders at the crown is what an oak
    /// does, and our clear-bole contract (CANOPY_CLEARANCE_MIN) never allowed
    /// ground-level splits anyway. The caller passes it only through its dose
    /// door (flora_united_bole_arm); this struct stays env-free and pure.
    const glm::vec3* bole = nullptr;
    uint32_t bole_count = 0;
};

/// Grows the whole tree into `sk`, which must be empty. The trunk's nodes are
/// marked `trunk` so the mesh side can skip drawing them twice, and every
/// terminal stem of the deepest level contributes a leaf site anchored to
/// itself — so the attachment invariant of FloraSkeleton.h holds here by
/// construction, exactly as it does for the other growers.
void weber_skeleton(Skeleton& sk, const WeberParams& p, uint64_t seed);

} // namespace dfn::render
