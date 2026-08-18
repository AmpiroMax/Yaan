/*
Created: 10:08:2026 - 12:11:29
Last updated: 18:08:2026 - 12:06:07
Module: engine/render
File: engine/render/sources/PathMesher.h

Responsibility:
- The §8.1 PATH SURFACE (в7/в24: «core generates, render draws»): turns core's
  path stations into ribbon meshes carrying the trodden CROSS-SECTION — worn
  centre, pressed margins, and the dissolve into the ground — plus the class
  each piece is made of.

Key items:
- PATH_CROSS_KNOTS / PATH_EDGE_MARGIN_M / PATH_PIECE_STATIONS: the three
  numbers the geometry is built from, each derived below.
- PathPiece: one drawable run of ONE class, with its bounds.
- build_path_pieces(): the whole network -> pieces, pure and GPU-free.

Dependencies:
- Uses: ProcMesh.h (MeshData), engine/core/math (PathStation, the wear profile,
  Aabb), glm.
- Used by: RenderSystem::set_path_surface, tests/render/PathMesherTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions: no GPU calls, no ECS access; deterministic, unit-tested.
- THE CROSS-SECTION HAS EXACTLY ONE DEFINITION AND IT IS core's
  (`math::path_wear_profile`). It is called HERE, on the CPU, at the knots
  below — it is deliberately NOT reimplemented in fs_path.sc. A copy of the
  formula in GLSL would keep the old curve the day core retunes it, silently,
  and the two zones would disagree about where a path ends while both passed
  their own tests. What the shader receives is the SAMPLED profile.
- THE ATLAS CELL IS core's PathClass ORDINAL. See ProcTexture.h's ordinal
  warning; render is on core's pin (world::PathClassTests).

Notes on why this is geometry and not a terrain splat channel — the question a
successor will ask first, because a splat channel is obviously cheaper:

  The terrain lattice is 1 m (HEIGHTMAP_STEP; it was 2 m until 18:08:2026).
  A dirt tread is 2.2 m wide and the three bands live INSIDE that. Core
  already ran this experiment one power of two up: baking dist/class/width
  into the 4 m routing grid measured wear 0.46 at the centreline instead of
  1.0, because the nearest cell centre was 0.8 m off the line, and the whole
  cross-section collapsed into one value.
  THE 1 M LATTICE HALVES THAT OBJECTION, and this block says so rather than
  keeping the old arithmetic: the tread now spans ~3 samples across instead
  of 1-2, and the worst distance from a cell centre to the line falls with
  the cell, 1.0 m -> 0.5 m. What is left is still thin — three samples have
  to carry worn centre, pressed margin AND the dissolve, one per band, with
  nothing over for the gradient inside a band — so the argument is weaker,
  not answered.
  WHAT THAT DOES NOT LICENCE: it does not follow that the ribbon is now
  unnecessary. Nobody has measured a 1 m splat channel against this geometry;
  that is a separate decision, and until someone runs it the honest state is
  "open", not "settled either way".
  So the cross-section stays on its own geometry — where the across
  coordinate is a vertex attribute and the profile is exact per pixel.

And why a ribbon does not violate §8.1's «a GRADIENT, never a decal ribbon»:
the piece's material COVERAGE falls with wear and is resolved by the same
ordered 4x4 Bayer threshold the terrain splat uses (fs_path.sc). The mesh's
outer boundary sits where wear is already 0, so it is fully discarded there and
there is no edge to see — the tread dissolves into whatever the terrain drew.
The prohibition is on a hard-edged stamped strip, which this is the opposite of.
*/
/*
UPD:
- 10:08:2026 - 12:11:29: Created — the path surface splat.
- 18:08:2026 - 12:06:07: The "why geometry, not a splat channel" argument
  re-costed against the 1 m chunk lattice (HEIGHTMAP_STEP 2.0 -> 1.0,
  NUMBERS). The objection this file rests on is now roughly HALF as strong: a
  2.2 m tread gets ~3 samples across where it used to get 1-2, and the worst
  centreline offset halves with the cell. Written down as a weakening rather
  than quietly left standing, because a successor who checks the arithmetic
  will find it and trust the rest of the block less. What is deliberately NOT
  written is the conclusion that the ribbon is therefore unnecessary: three
  samples still have to carry three bands, and nobody has measured a 1 m
  splat channel against this geometry. That decision stays open and owned by
  whoever runs the measurement.
*/

#pragma once

#include "engine/core/math/sources/Aabb.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/render/sources/ProcMesh.h"

#include <cstdint>
#include <span>
#include <vector>

namespace dfn::render {

/// Cross-section knots per SIDE of the centreline, uniformly spaced in
/// u = |across| / worn_half_width over [0,1].
///
/// DERIVED, not picked. `math::path_wear_profile` is 1 - u^2, so linear
/// interpolation between knots spaced h apart has a maximum error of
/// |f''| h^2 / 8 = h^2 / 4. At 5 intervals (h = 0.2) that is 0.010 of wear —
/// a fortieth of one 64-colour palette shade step, i.e. below anything the
/// pipeline can express. A test measures it against core's function rather
/// than trusting this arithmetic, so a change to the profile's CURVATURE
/// (say, to a cosine) reds here instead of quietly widening the error.
inline constexpr int PATH_CROSS_KNOTS = 5;

/// How far past the worn edge the mesh runs (m). Wear is already 0 there, so
/// every pixel of it is discarded; it exists only so that the boundary of the
/// GEOMETRY is never the boundary of the VISIBLE surface — a rasteriser edge
/// and a dither edge alias differently, and the first would be a decal edge.
inline constexpr float PATH_EDGE_MARGIN_M = 0.15f;

/// Stations per drawable piece (~4 m each, so ~128 m of tread).
///
/// This is a CULLING number, not a memory one. A whole 600 m route is one
/// bounding sphere ~300 m across: it is on screen essentially always, and the
/// frustum can never reject it. At 128 m a piece is comparable to a terrain
/// chunk, which is the granularity everything else in this renderer culls at.
inline constexpr uint32_t PATH_PIECE_STATIONS = 32;

/// One drawable run of the network: a strip of ONE PathClass.
///
/// Pieces never span a class change. Interpolating the class across the seam
/// would paint the ordinals BETWEEN the two — a cobble-to-hint-path transition
/// would grow a two-metre band of dirt that core never routed, because 1 lies
/// between 0 and 2. The seam is where the paving actually ends.
struct PathPiece {
    MeshData mesh;
    math::Aabb bounds{};
    uint8_t path_class = 0; ///< world::PathClass ordinal (pinned contract)
};

/// Builds the drawable pieces of one path network.
///
/// `stations` and `route_offsets` are ChunkManager::PathSurface's spans: route
/// i occupies [route_offsets[i], route_offsets[i+1]), and route_offsets ends
/// with the total. Returns an empty vector for an empty network — a stand with
/// no paths is a valid stand, not a failure (Rule 32).
///
/// Vertex attribute contract with fs_path.sc (change both together):
///   position   world space, y = station.tread_height (core sinks the ground to
///              tread_height - PATH_GROOVE_DEPTH, so the ribbon sits proud of
///              it and cannot z-fight)
///   normal     the tread's frame: flat ACROSS, following the longitudinal
///              profile ALONG
///   uv         (across_m, arc_length_m) — TRUE arc length, so the material
///              does not stretch through a bend
///   color.r    wear, sampled from math::path_wear_profile at the knots
///   color.g    (path_class + 0.5) / 4  -> the atlas cell
///   color.b    0 (reserved)
///   color.a    1 (sky visibility, as everywhere else)
[[nodiscard]] std::vector<PathPiece>
build_path_pieces(std::span<const math::PathStation> stations,
                  std::span<const uint32_t> route_offsets);

} // namespace dfn::render
