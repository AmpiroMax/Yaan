/*
Module: engine/render
File: engine/render/sources/WaterMesher.h

Responsibility:
- Per-body water meshes (stage 3b): a lake ellipse plane from math::LakePlane
  and a river ribbon strip from an ordered math::RiverStation polyline (width
  per station, surface height descending source -> mouth). Replaces the single
  global debug plane.

Key items:
- build_lake_mesh(); build_river_mesh().

Dependencies:
- Uses: ProcMesh.h (MeshData), engine/core/math (LakePlane/RiverStation), glm.
- Used by: RenderSystem::set_water_bodies, tests/render/WaterMesherTests.cpp.

Notes:
- Both meshes are world-space and rendered with the "water" program (alpha
  blend, read-only depth, submitted after opaques). Edge margins push the
  water under the carved banks; the overlap is hidden by the depth test
  against terrain, so shorelines never show gaps.
- UVs are world-position / uv_tile_m like the debug plane, so the scrolled
  water texture is continuous across bodies.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions: no GPU calls, no ECS access; deterministic, unit-tested.
- River stations arrive ordered source -> mouth with monotonically
  non-increasing surface heights (core invariant) — never re-sort them.
*/

#pragma once

#include "engine/render/sources/ProcMesh.h"

#include <span>

namespace dfn::render {

/// Flat ellipse fan at lake.surface_height covering the lake footprint plus
/// `edge_margin_m` (hidden under the banks by depth test). `segments` rim
/// points; uv = world xz / uv_tile_m.
[[nodiscard]] MeshData build_lake_mesh(const math::LakePlane& lake, float uv_tile_m,
                                       float edge_margin_m, uint32_t segments = 48);

/// Ribbon strip along one river segment's stations: left/right rim offset by
/// half_width + edge_margin_m perpendicular to the local flow direction,
/// y = the station's water surface (descending downhill by the core
/// invariant). Returns an empty mesh for fewer than 2 stations.
[[nodiscard]] MeshData build_river_mesh(std::span<const math::RiverStation> stations,
                                        float uv_tile_m, float edge_margin_m);

} // namespace dfn::render
