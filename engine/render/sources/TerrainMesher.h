/*
Created: 09:08:2026 - 00:45:00
Last updated: 23:08:2026 - 00:30:00
Module: engine/render
File: engine/render/sources/TerrainMesher.h

Responsibility:
- Terrain meshing: turns a chunk's HeightFieldView (agreed core<->render
  contract) into the frozen IRenderer Vertex/index arrays, crack-free across
  chunk borders (shared edge rows).

Key items:
- TerrainMeshData; build_terrain_mesh().

Dependencies:
- Uses: engine/core/math (HeightFieldView), engine/platform/render (Vertex).
- Used by: RenderSystem::upload_terrain, tests (deterministic, GPU-free).

Notes:
- Vertex color carries SPLAT WEIGHTS since stage 3b (contract with
  fs_terrain.sc): R = sand, G = rock, B = water-bed, A = reserved (255).
  With a SurfaceFieldView the weights come from core's surface_class ONLY —
  the design truth (LANDSCAPE §3.3/§4); render never re-derives material
  bands from raw dist/height fields (design ruling, feature-requests batch —
  the removed dryness/dirt band painted 60 m brown washes over Grass).
  Without a surface view weights fall back to slope-only rock. Pure function
  of the inputs — deterministic, unit-tested.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this a pure function: no GPU calls, no ECS access.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial contract + implementation.
- 09:08:2026 - 11:57:20: Stage 3b — SurfaceFieldView overload; vertex color
  re-purposed from tint to splat weights (shader contract updated in step).
- 09:08:2026 - 14:11:37: Dryness/dirt channel removed (design ruling): splat
  keys off core's surface_class only; alpha reserved.
- 09:08:2026 - 22:01:04: LOD support. (1) UVs are WORLD-REFERENCED (world xz /
  CHUNK_SIZE) instead of 0..1 across the field. For a field whose origin sits
  on the 128 m node grid this is identical to the old formula — the difference
  is a whole number of tile repeats — but under the old formula a 1..8 km LOD
  node stretched one texture set across the entire node. A test pins the
  equality rather than asserting it in prose. (2) TerrainMeshOptions::
  skirt_depth_m appends a vertical apron to the four borders, which is what
  hides the T-junction crack between two adjacent LOD levels.
- 10:08:2026 - 01:47:53: TerrainMeshOptions::clip_min/clip_max — the mesh half
  of the straddle-ring fix (see TerrainLod.cpp UPD of the same date): cells
  wholly inside the chunk-streamed rectangle are not emitted and skirts hang
  along the cut boundary. Empty clip keeps the emission path bit-identical.
- 23:08:2026 - 00:30:00: PathClassField/pack_path_alpha — материал полотна в альфе вершины
  (2 бита класса + 6 бит износа), «нет тропы» остаётся 255. Одна упаковка
  на все мешеры кадра — контракт с разбором в fs_terrain.
*/

#pragma once

#include "engine/core/math/sources/HeightField.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <vector>

namespace dfn::render {

/// CPU-side terrain mesh for one chunk, ready for IRenderer::create_mesh.
struct TerrainMeshData {
    std::vector<platform::Vertex> vertices; // resolution^2, row-major like the field
    std::vector<uint32_t> indices;          // 6 * (resolution-1)^2
};

/// Triangulates `field` (world-space positions from origin/step, heights via
/// the agreed decode formula). Normals by central differences; UVs span 0..1
/// across the chunk; vertex colors encode the splat weights (see Notes).
/// Neighbor chunks share edge samples by contract, so meshes stitch without
/// cracks. `surface` (same chunk's SurfaceFieldView, may be nullptr) supplies
/// design-truth sand/rock/water-bed weights; nullptr keeps slope-only rock.
[[nodiscard]] TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field,
                                                 const math::SurfaceFieldView* surface);

/// Stage-2 compatible form: slope-only splat weights (no surface data).
[[nodiscard]] TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field);

/// МАТЕРИАЛ ПОЛОТНА ПО МЕСТУ (22.08, владелец: «тропинки опять плитами
/// кладутся, а не тропинкой каменной»). Композиция знает, ГДЕ идёт мостовая,
/// а где стёжка; ядро несёт только износ. Поле — полилинии с полушириной и
/// классом путевого атласа (0 мостовая / 1 укатанный грунт / 2 стёжка /
/// 3 тёсаные плиты); мешер спрашивает класс ТОЛЬКО там, где износ ненулевой.
struct PathClassStroke {
    std::vector<glm::vec2> points; ///< уже растянутая полилиния (дуга снаружи)
    float half_width_m = 1.5f;
    uint8_t path_class = 1;
};
struct PathClassField {
    std::vector<PathClassStroke> strokes;
    /// Класс ближайшего мазка, чьё полотно накрывает точку (последний
    /// нарисованный выигрывает — поздний мазок ложится поверх); мимо всех —
    /// fallback (сеть ядра — укатанный грунт).
    [[nodiscard]] uint8_t class_at(glm::vec2 xz, uint8_t fallback = 1) const;
};

/// АЛЬФА ТРОПЫ С МАТЕРИАЛОМ: 2 бита класса + 6 бит обратного износа.
/// «Нет тропы» остаётся 255 (класс 3, износ 0 — шейдер рисует ноль), поэтому
/// меш без сети байт-в-байт прежний. Обе решётки — эта упаковка и разбор в
/// fs_terrain — обязаны меняться вместе (контракт).
[[nodiscard]] inline uint8_t pack_path_alpha(float wear, uint8_t cls) {
    if (wear <= 0.0f) {
        return 255;
    }
    const int wi = std::clamp(
        static_cast<int>(std::lround((1.0f - wear) * 63.0f)), 0, 62);
    return static_cast<uint8_t>((static_cast<int>(cls & 3u) << 6) | wi);
}

/// Extra meshing choices. Everything here defaults to the chunk behaviour, so
/// the two calls above are exactly `build_terrain_mesh(field, surface, {})`.
struct TerrainMeshOptions {
    /// Metres of vertical apron hung from the four border edges, 0 = none.
    /// A skirt exists ONLY to hide the T-junction crack where this mesh meets
    /// a neighbour meshed on a different lattice — it is never visible ground,
    /// so it is deliberately allowed to be too deep rather than too shallow.
    /// Derive it with lod_skirt_depth_m(); chunks share a sample lattice with
    /// their neighbours by contract and need none.
    float skirt_depth_m = 0.0f;

    /// World-space xz rectangle to EXCLUDE from the mesh (empty when
    /// clip_max <= clip_min on either axis = no clip, the default). Cells
    /// whose footprint lies WHOLLY inside are not emitted; cells cut by the
    /// rectangle are kept whole (the safe direction — at most one cell of
    /// overlap, and zero for the real caller: every LOD voxel size divides
    /// the 256 m chunk grid, so a chunk-aligned rectangle lands exactly on
    /// cell boundaries at every level). This is the straddle-ring fix's
    /// mesh half: a LOD node overlapping the chunk-streamed rectangle is
    /// SELECTED whole at its distance-correct level and clipped here, instead
    /// of being force-split to level 0. Skirts (skirt_depth_m) hang along the
    /// cut boundary exactly as along the outer border — the cut is a seam
    /// against differently-latticed chunk meshes and cracks the same way.
    glm::vec2 clip_min{0.0f};
    glm::vec2 clip_max{0.0f};

    /// Поле классов полотна; nullptr = прежняя 8-битная упаковка износа.
    /// НЕ nullptr — новая упаковка pack_path_alpha, и она обязана быть ОДНОЙ
    /// на всех мешерах кадра (LOD-кольцо включительно): шейдер разбирает
    /// альфу одним правилом на весь кадр.
    const PathClassField* path_classes = nullptr;
};

/// Full form. Skirt vertices are appended AFTER the resolution^2 grid vertices,
/// so `vertices[z * resolution + x]` keeps addressing the surface and existing
/// callers that index the grid are unaffected.
[[nodiscard]] TerrainMeshData build_terrain_mesh(const math::HeightFieldView& field,
                                                 const math::SurfaceFieldView* surface,
                                                 const TerrainMeshOptions& options);

/// The largest height difference between two ADJACENT samples along the four
/// border rows of `field`, in metres. This is the measurement that sizes a
/// skirt (see lod_skirt_depth_m) — measured from the field the node was built
/// from, never assumed.
[[nodiscard]] float terrain_border_max_step_m(const math::HeightFieldView& field);

} // namespace dfn::render
