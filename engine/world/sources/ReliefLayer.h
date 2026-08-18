/*
Created: 17:08:2026 - 19:05:00
Last updated: 18:08:2026 - 12:06:09
Module: engine/world
File: engine/world/sources/ReliefLayer.h

Responsibility:
- THE HAND EDIT OF THE GROUND, as data: a sparse field of per-sample height
  deltas and authored surface classes that a composer painted with a brush,
  stored beside the .scene and applied LAST in the worldgen pass stack.

Key items:
- RELIEF_STEP_M / sample_index() / sample_world(): the lattice, and it is the
  WORLD's lattice, not any chunk's.
- ReliefLayer: the sparse field, with bilinear height sampling and nearest
  surface lookup.
- read_relief / write_relief: the sidecar text format (in git, diffable).

WHY THIS EXISTS (user, 17.08.2026): «в этом же инструменте необходима
возможность менять высоту ландшафта кистями разных размеров, выбирать что за
поверхность будет рисоваться». A brush edit that cannot be saved is a
demonstration, not a tool.

WHY A LAYER AND NOT MORE [pad] SECTIONS. A ScenePad is a STATEMENT — "here the
ground is this high, blending back over N metres" — a rectangle or a circle,
stamped with last-writer-wins semantics. Raise, lower and smooth are RELATIVE
and free-form: there is no pad that says "+1.4 m here and +0.9 m a metre away".
Expressing one stroke would take hundreds of overlapping pads, and pads do not
compose — an overlap yields the last one, not the sum. The composer would
reopen a file that no longer resembles what he painted.

The one exception is FLATTEN, which is a pad exactly: that mode emits a [pad]
into the .scene rather than samples into here, because a second way of saying
"here the ground is this high" is precisely the drift Rule 32 forbids.

WHERE IT ENTERS THE WORLD, and this is the whole design: compose_passes() is
the single statement of what the finished ground is — terrain_height (which the
scene JUDGE measures with) and generate_chunk (which the player's knees hit)
both go through it. Applying the layer there, and only there, is what makes
"the ground you walk on" and "the ground check_scene judges" one thing without
a line of coordination between them.

AN EMPTY LAYER IS A NO-OP BIT FOR BIT. Not by float luck — by an explicit
early-out at every application site. `x + 0.0f` is not `x` when x is -0.0f, and
the pinned testbed digest is worth more than one branch.

Dependencies:
- Uses: engine/core/{config,math}, glm, std.
- Used by: Worldgen (compose_passes, classify_surface), ChunkManager,
  engine/app (EditorBrush), tools.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure data + IO of its own file (Rule 1: world = core only).
- THE LATTICE IS THE WORLD'S, NOT A CHUNK'S. Sample indices are absolute, so a
  sample shared by two chunks decodes to the same float from both sides. A
  per-chunk lattice would put a hairline crack in every chunk seam, which is
  the same argument that made HEIGHT_QUANT_SCALE shared rather than per-chunk.
- WRITE ORDER IS A PROPERTY OF write_relief, NOT OF THE CONTAINER. A diff must
  show the samples that moved, never a rehash of a map.
*/
/*
UPD:
- 17:08:2026 - 19:05:00: Создан — слой авторских правок земли (заказ 17.08 про
  кисти рельефа). Добро лида на добавку в engine/world получено до написания.
- 18:08:2026 - 12:06:09: HEIGHTMAP_STEP 2.0 -> 1.0 м, и этот файл — ПРИЧИНА правки, а не её
  жертва. Заказ 18.08 («углы мне не нравятся») — про кисть из этого слоя, а
  верхний комментарий уже объяснял, почему углы неизбежны: решётка правки равна
  решётке хранения. Ни строки кода менять не пришлось, RELIEF_STEP_M выведен.
  ВАЖНО ДЛЯ ТОГО, КТО ДЕРЖИТ .relief В РАБОЧЕЙ КОПИИ: read_relief СВЕРЯЕТ
  объявленный шаг и откажет вслух — файл, написанный на 2 м, надо переписать
  (step 1, все индексы x и z удвоить). В git ни одного .relief нет, так что
  терять нечего; проверено find'ом по дереву.
*/

#pragma once

#include "engine/core/config/sources/Constants.h"
#include "engine/core/math/sources/SurfaceField.h"

#include <cstdint>
#include <filesystem>
#include <glm/vec2.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace dfn::world {

/// THE LATTICE A HAND EDIT LIVES ON — the heightmap's own step, and it cannot
/// be finer. The drawn terrain is a voxel surface built FROM the chunk's
/// decoded heightmap (VoxelVolume.h says so in as many words), so a delta
/// authored between two samples is a shape the world has no way to hold. The
/// brush's minimum radius follows from this and the tool says so out loud
/// rather than pretending to paint finer than the world can show.
///
/// THIS DERIVATION IS WHY THE STEP MOVED, 18.08.2026. The user painted with
/// the brush and said the corners were wrong; the sentence above is the reason
/// they were. HEIGHTMAP_STEP went 2.0 -> 1.0 m, so this lattice halved with it
/// and the minimum brush radius halved to 1 m — for free, because the number
/// was derived here rather than typed. It cannot usefully go below VOXEL_SIZE:
/// a delta at a sample no voxel node reads is a shape that still cannot be
/// drawn, which is the same wall one power of two down.
inline constexpr float RELIEF_STEP_M = static_cast<float>(config::HEIGHTMAP_STEP);

/// Index of the lattice sample at or below `world_coord` on one axis.
[[nodiscard]] int32_t relief_index_floor(float world_coord);

/// World coordinate of lattice index `i` on one axis.
[[nodiscard]] float relief_world_of(int32_t i);

/// One painted sample. `surface` is authored only when `has_surface`; a stroke
/// that only raised ground leaves it unset, and a stroke that only painted
/// material leaves height_delta at zero. The two channels are independent on
/// purpose — a composer levels a market square long before he decides it is
/// sand.
struct ReliefSample {
    int32_t x = 0;
    int32_t z = 0;
    float height_delta = 0.0f;  ///< metres, added to the composed ground
    math::SurfaceClass surface = math::SurfaceClass::Grass;
    bool has_surface = false;
};

/// The sparse field of hand edits over one map.
///
/// Sparse because a brush touches only what it touched: a day's terracing is
/// thousands of samples out of the tens of thousands a map has, and storing the
/// untouched ones would turn "what did I change" into a question nobody can
/// answer from a diff.
class ReliefLayer {
public:
    [[nodiscard]] bool empty() const { return cells_.empty(); }
    [[nodiscard]] std::size_t size() const { return cells_.size(); }
    void clear() { cells_.clear(); }

    /// Height delta in metres at a world position, bilinear across the four
    /// surrounding lattice samples. Untouched samples read as exactly 0, so an
    /// edit fades to nothing at its own edge without anyone drawing a skirt.
    ///
    /// Callers still guard on empty() before adding this to a height — see the
    /// header note on -0.0f. Returning 0 here is correct; adding it is not free.
    [[nodiscard]] float height_delta_at(glm::vec2 world_xz) const;

    /// The authored surface class at a world position, NEAREST sample (a class
    /// is an enum and does not interpolate), or false when nobody painted here.
    [[nodiscard]] bool surface_at(glm::vec2 world_xz, math::SurfaceClass& out) const;

    /// Height delta at an exact lattice sample; 0 when untouched.
    [[nodiscard]] float delta_at(int32_t x, int32_t z) const;

    /// Sets the delta at a lattice sample. A delta of exactly 0 with no painted
    /// surface ERASES the sample rather than storing a zero: an edit undone
    /// must leave a file identical to the one before it, or every undone
    /// experiment survives forever as a line in a diff.
    void set_delta(int32_t x, int32_t z, float metres);

    /// Paints the surface class at a lattice sample.
    void set_surface(int32_t x, int32_t z, math::SurfaceClass surface);

    /// Removes any authored surface at a lattice sample (back to derived).
    void clear_surface(int32_t x, int32_t z);

    /// Every touched sample, in an UNSPECIFIED order — sort before writing.
    [[nodiscard]] std::vector<ReliefSample> samples() const;

    /// World-space bounds of every touched sample, padded by one lattice step
    /// (a delta at a sample reaches half a step either way through the bilinear
    /// filter, and a chunk that only touches the padding still has to rebuild).
    /// False when the layer is empty, and then the outputs are untouched.
    [[nodiscard]] bool bounds_xz(glm::vec2& min_xz, glm::vec2& max_xz) const;

private:
    struct Cell {
        float height_delta = 0.0f;
        math::SurfaceClass surface = math::SurfaceClass::Grass;
        bool has_surface = false;
    };
    /// Packed (x, z) -> cell. Unordered for the paint loop, which touches the
    /// same samples thousands of times in a stroke; ORDER IS IMPOSED ON WRITE.
    std::unordered_map<uint64_t, Cell> cells_;

    [[nodiscard]] static uint64_t key_of(int32_t x, int32_t z) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32)
             | static_cast<uint64_t>(static_cast<uint32_t>(z));
    }
    [[nodiscard]] const Cell* find(int32_t x, int32_t z) const;
};

/// Reads a .relief sidecar. Returns false and fills `error` (with the line
/// number) on a malformed number or an unknown lattice step.
///
/// A MISSING FILE IS AN ERROR HERE, not an empty layer. The caller asked for a
/// file by name because the .scene named it; answering "no edits" would make a
/// lost terrain edit look like a map that moved by itself, and the composer
/// would go looking for the bug in the generator.
[[nodiscard]] bool read_relief(const std::filesystem::path& path, ReliefLayer& out,
                               std::string& error);

/// Writes it back, atomically, sorted by z then x — so a diff shows the samples
/// that moved and never a rehash. Writing an EMPTY layer removes the file: an
/// edit fully undone must leave the tree as it was found.
[[nodiscard]] bool write_relief(const ReliefLayer& layer,
                                const std::filesystem::path& path);

} // namespace dfn::world
