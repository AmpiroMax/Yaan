/*
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
- ReliefPath / relief_path_polyline(): A PATH AS A CURVE, not as a mask of
  painted squares — the control points a composer put down and the arc that
  runs through them.
- ReliefLayer's PATH WEAR CHANNEL: the per-sample [0,1] field the curves are
  decomposed into, which is what the ground actually draws.

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

/// THE NARROWEST PATH WORTH DRAWING, as a half-width. Same argument as
/// BRUSH_MIN_RADIUS_M and the same lattice: a tread narrower than one sample
/// either lands on samples or misses them depending on where it runs, so the
/// path would fade in and out along its own length.
inline constexpr float PATH_MIN_HALF_WIDTH_M = RELIEF_STEP_M;

/// THE NARROWEST FADE BAND, in metres — and this number is the whole reason
/// the user's «не по квадратам» works at all, so it is measured rather than
/// chosen (Rule 45).
///
/// The ground draws wear as a per-sample value carried in the vertex and
/// LINEARLY INTERPOLATED across the triangle (TerrainMesher/VoxelMesher put it
/// in the vertex alpha, fs_terrain.sc dithers against it). A fade band wider
/// than the lattice therefore lands on several samples and the visible edge is
/// the interpolated isoline — a straight diagonal. A band narrower than one
/// sample is a 0/1 field again, and 0/1 on a lattice is a STAIRCASE, which is
/// exactly what painting a path with a surface class does today.
///
/// MEASURED, both arms, on the 0.5 isoline of a 60 m run at seven angles
/// (tests/app/EditorPathTests.cpp reproduces it): band 1.0 m -> at most 0.12 m
/// off the true straight line; band 0.25 m -> 0.42 m; the 0/1 class arm ->
/// 0.49 m, which is half a lattice cell and the definition of a staircase.
inline constexpr float PATH_MIN_FADE_M = RELIEF_STEP_M;

/// A PATH THE COMPOSER DREW, as its control points and its cross-section.
///
/// THE CURVE IS THE STATEMENT AND THE WEAR IS DERIVED FROM IT — that is why
/// the sidecar stores these points and not the samples they stamp. A path
/// stored as painted samples cannot be re-shaped afterwards: the composer
/// would be editing the FOOTPRINT of his own decision instead of the decision,
/// which is the difference between a tool and a rubber stamp.
struct ReliefPath {
    /// The points the composer put down, in world XZ, in the order he put
    /// them. The arc runs THROUGH them (centripetal Catmull-Rom, which is a
    /// cubic Bezier written so that the control points are ON the curve) —
    /// a Bezier whose handles are not on the curve would make him aim at
    /// places the path does not go.
    std::vector<glm::vec2> points;
    /// Half the worn width, metres. Clamped to PATH_MIN_HALF_WIDTH_M on use.
    float half_width_m = 1.5f;
    /// HOW SOFT THE EDGE IS, [0,1]: the fraction of the half-width over which
    /// the wear fades from full to none. 1 means the fade starts at the centre
    /// line, which is EXACTLY math::path_wear_profile — the cross-section the
    /// generated network already uses. Smaller values flatten the top and
    /// narrow the fade; the fade is floored at PATH_MIN_FADE_M in metres,
    /// because below that the field is 0/1 and the staircase comes back.
    float edge_softness = 1.0f;
    /// МАТЕРИАЛ ПОЛОТНА (22.08, владелец: «тропинки опять плитами кладутся, а
    /// не тропинкой каменной»). Порядковый номер клетки путевого атласа —
    /// core PathClass: 0 мостовая (COBBLE), 1 укатанный грунт (PACKED_EARTH),
    /// 2 протоптанная стёжка (SCUFFED), 3 тёсаные плиты (CUT_SLAB). До этого
    /// поля материал чертежа ВЫБРАСЫВАЛСЯ на этой самой структуре — генератор
    /// писал stone/gravel/dirt, сюда доезжали только ширина и износ, и шейдер
    /// красил всё одной грязью; город от безысходности мостили штучными
    /// плитами. Умолчание 1 — прежние тропы полян ближе всего к укатанному
    /// грунту; файл без ключа читается как раньше.
    int path_class = 1;
};

/// The arc through the control points, sampled at most `max_step_m` apart.
/// Fewer than two points gives back the points themselves: a path of one point
/// is a place, not a path, and inventing an arc through it would be inventing.
[[nodiscard]] std::vector<glm::vec2> relief_path_polyline(const ReliefPath& path,
                                                          float max_step_m = 0.5f);

/// Wear at `dist_m` from the centre line of `path`, [0,1]. THE CROSS-SECTION IS
/// math::path_wear_profile AND NOT A SECOND FORMULA (SurfaceField.h says so in
/// as many words): softness moves where the fade STARTS, exactly as the brush's
/// hardness does, so the softest setting IS the generated network's profile
/// rather than something that resembles it.
[[nodiscard]] float relief_path_wear(const ReliefPath& path, float dist_m);

/// World box the path's wear can reach, padded by one lattice step. False for
/// a path with no points.
[[nodiscard]] bool relief_path_bounds(const ReliefPath& path, glm::vec2& min_xz,
                                      glm::vec2& max_xz);

/// The control point under `aim_xz`, or `points.size()` for none. NEAREST
/// within `grab_m`, so two points closer together than the grab radius still
/// resolve to the one the pointer is actually on.
[[nodiscard]] std::size_t relief_path_pick(const ReliefPath& path, glm::vec2 aim_xz,
                                           float grab_m);

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
    void clear() {
        cells_.clear();
        wear_.clear();
        paths_.clear();
    }

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

    // -- THE PATH CHANNEL ---------------------------------------------------
    //
    // A SEPARATE MAP FROM cells_, on purpose. cells_ answers "did a hand touch
    // the HEIGHT here", and an emptiness claim rests on it (see the header on
    // -0.0f); a path that added cells there would switch on the height branch
    // for a map where no height was ever touched. The channels are also
    // independent in fact: a path is worn ground, not lowered ground.

    /// Authored wear at a world position, [0,1], bilinear across the four
    /// surrounding samples — and BILINEAR IS THE POINT. The renderer carries
    /// this number per vertex and interpolates it, so a wear field sampled on
    /// the lattice draws its edge as an isoline of the interpolation: a
    /// diagonal comes out diagonal. Nearest lookup here (what a surface CLASS
    /// must do, since an enum has no midpoint) is what draws a staircase.
    [[nodiscard]] float path_wear_at(glm::vec2 world_xz) const;

    /// Wear at an exact lattice sample; 0 when untouched.
    [[nodiscard]] float path_wear_of(int32_t x, int32_t z) const;

    /// Sets it. Zero ERASES the sample, for the same reason set_delta does.
    /// Public because the bake calls it and a test measures it; the composer's
    /// route to it is a ReliefPath.
    void set_path_wear(int32_t x, int32_t z, float wear);

    /// Is there any authored wear at all? The early-out every reader needs, and
    /// the reason a map with no paths stays bit-identical.
    [[nodiscard]] bool has_path_wear() const { return !wear_.empty(); }
    [[nodiscard]] std::size_t path_wear_size() const { return wear_.size(); }

    /// World box of the wear channel, padded by one lattice step. False when
    /// nothing is worn.
    [[nodiscard]] bool path_wear_bounds_xz(glm::vec2& min_xz, glm::vec2& max_xz) const;

    // -- THE CURVES THEMSELVES ----------------------------------------------

    [[nodiscard]] const std::vector<ReliefPath>& paths() const { return paths_; }
    /// Appends and stamps it; returns its index.
    std::size_t add_path(const ReliefPath& path);
    /// Replaces one and RE-BAKES THE WHOLE CHANNEL. Not "unstamp then stamp":
    /// two paths that crossed share their worn samples, and unstamping one
    /// would erase the crossing out of the other. Re-baking is O(paths) and
    /// happens when a hand lets go of a point, which is not a hot loop.
    void set_path(std::size_t index, const ReliefPath& path);
    void erase_path(std::size_t index);
    /// Clears the wear channel and stamps every path into it again. The one
    /// definition of "what the curves mean", called after any change to them
    /// and after reading a file.
    void rebake_paths();

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
    /// Packed (x, z) -> wear, [0,1]. DERIVED from paths_ and never written to
    /// the sidecar: the file carries the curves, and the samples are what the
    /// curves mean. Two records of one decision is the drift Rule 32 forbids,
    /// and here it would show as a path that moved on screen but not in the
    /// file the composer re-opened.
    std::unordered_map<uint64_t, float> wear_;
    std::vector<ReliefPath> paths_;

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
