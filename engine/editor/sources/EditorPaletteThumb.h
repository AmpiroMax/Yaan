/*
Created: 18:08:2026 - 01:00:57
Last updated: 18:08:2026 - 01:00:57
Module: engine/editor
File: engine/editor/sources/EditorPaletteThumb.h

Responsibility:
- THE PICTURE IN THE OBJECT MENU. A part is drawn — on white, from one shared
  rack and one shared light, framed by its own MEASURED box — into plain RGBA8
  bytes, plus the cache that keeps those pictures by name and spends a fixed
  budget of them per frame. Everything here is data in / data out: not one line
  needs a window, which is why the whole thing is provable in a suite that has
  no ImGui in it at all (Rule 3).

Key items:
- thumb_frame(): the framing decision — where the tile's centre lands and how
  many metres it spans — from the box render::measure_object hands over.
- bake_thumbnail() / bake_object_thumbnail(): the software rasteriser. Yes,
  software: see WHY NOT THE GPU below.
- bake_surface_swatch() / bake_path_swatch(): the same idea for GROUND, so the
  surface picker can show what a path looks like instead of naming it.
- ThumbCache: name -> texture, a per-frame bake budget and an LRU cap.

WHY THIS EXISTS (user, 18.08.2026): «у объектов в меню объектов нет всё ещё
предпросмотра что это за объекты, только название, нужно чтобы там на белом
фоне сам объект был нарисован», and right after it «для дорожек и других троп
тоже предпросмотр сделать».

WHY NOT THE GPU. An offscreen pass would need a render target, a pass id, a
program and a submit — four things that live in engine/platform behind a
backend, none of which can be instantiated in a test. The parts are a few
thousand flat-shaded triangles at 192 px; a CPU rasteriser draws one in half a
millisecond (0.50-0.72 ms measured, and the suite prints the figure rather than
promising it), and in exchange every decision in it — the rack, the framing, the
shading, the budget — is a number a suite can read back out of the pixels. The
day a part needs the real material chain, this becomes the fallback rather than
the mechanism.

ONE RACK FOR EVERY PART, AND THAT IS THE POINT. Thumbnails are read as a GRID:
the builder is comparing them to each other, so the only thing that may differ
between two tiles is the part. Same yaw, same pitch, same light, same margin —
and the frame FITTED to each part, so a 0.25 m connector and a 4.6 m beam both
fill their tile. A fixed scale would show the connector as four pixels, which
is the same as showing nothing.

Dependencies:
- Uses: engine/render (MeshData, RegistryObject, measure_object, ProcTexture,
  Materials), engine/core/math (SurfaceClass), glm, std. NOT ImGui, NOT
  EditorUi: the uploader arrives as a function so this file stays testable.
- Used by: EditorPaletteView / EditorPaletteFamily (through PaletteHooks),
  engine/app (wires the bake and the upload), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- RETURNING 0 FROM THE CACHE IS NORMAL. It means "not drawn yet"; the panel
  draws its placeholder and asks again next frame. Never block a frame on a
  bake, and never raise the budget to make a first frame look better — the
  first frame after the menu opens is the one a builder is watching, and a
  stutter there is worse than a tile arriving one frame late.
- NO SECOND SET OF GROUND COLOURS. The surface swatch composes the SAME cells
  render::generate_proc_texture() bakes into the terrain atlas, weighted by the
  SAME render::splat_weights_of() the meshers use. A hand-picked green here
  would drift from the world silently, and the swatch's whole job is to say
  what the ground will look like.
*/
/*
UPD:
- 18:08:2026 - 01:00:57: Создан — предпросмотр детали и поверхности (заказ 18.08). Крючок
  thumbnail в PaletteHooks существовал с 17.08 и не был подключён НИКЕМ: панель
  честно спрашивала картинку каждый кадр и каждый кадр получала 0, то есть меню
  из 2412 подписей. Здесь появляется то, что на этот вопрос отвечает.
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/render/sources/ProcTexture.h"

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace dfn::app {

// -- the shared rack ---------------------------------------------------------

/// THE ONE ANGLE. 35° around the part and 22° down: enough to see three faces
/// of a box (top, front, side), shallow enough that a wall panel still reads as
/// a wall rather than as a plan view. Both are shared by every tile — see the
/// note in the header about why a thumbnail grid may only vary in ONE thing.
inline constexpr float THUMB_YAW_DEG = 35.0f;
inline constexpr float THUMB_PITCH_DEG = 22.0f;

/// Empty band left around the fitted box, as a fraction of the tile. Small,
/// because the tile is 96 px and every percent given away is silhouette lost;
/// non-zero, because a part touching the tile's edge reads as cropped.
inline constexpr float THUMB_MARGIN = 0.06f;

/// THE ONE SIZE A PART IS BAKED AT, whatever size it is drawn at. Caching by
/// (name, size) would put one part in the atlas three times — 44 px in the
/// strips, 96 in the tiles, 192 in the family preview — for one part, which is
/// the arithmetic the hook's contract already refuses (EditorPaletteView.h).
/// 192 is the largest of the three, so the other two are downscales and never
/// blur-ups.
inline constexpr int THUMB_BAKE_PX = 192;

/// Rasterise at this multiple and box-filter down. A kit part is mostly long
/// thin edges at a shallow angle, and those are exactly what a hard-edged
/// rasteriser turns into a staircase; x2 costs four times almost nothing.
inline constexpr int THUMB_SUPERSAMPLE = 2;

/// How many parts may be drawn in ONE frame. The list is clipped, so only
/// visible rows ask — but the first frame after the menu opens has two or three
/// dozen of them visible at once, and that is the frame a builder is watching.
///
/// FOUR RATHER THAN THE SIX THIS STARTED AT, and the difference is a
/// measurement rather than a preference: one 192 px bake takes 0.50-0.72 ms on
/// this machine (an 80-triangle beam and a post, measured in the suite and
/// printed by it), and the frame it lands in is 8.3 ms long at 120 fps. Six
/// would spend 3.0-4.3 ms — over a third of the frame — on the exact frames a
/// builder is scrolling. Four spends 2.0-2.9 ms and fills a 24-tile page in
/// six frames, i.e. in a twentieth of a second.
inline constexpr int THUMB_FRAME_BUDGET = 4;

/// How many pictures the cache keeps. 128 tiles of 192x192 RGBA8 is 18.9 MB —
/// the whole shelf would be 356 MB, which is why this is a cap and not a hope.
inline constexpr std::size_t THUMB_CACHE_MAX = 128;

/// How many times a name whose mesh cannot be found is retried before it is
/// given up on. Without a cap a single bad name eats a budget slot EVERY frame,
/// for ever, and the parts behind it never get drawn.
inline constexpr int THUMB_MAX_ATTEMPTS = 3;

/// The tile's white ground, as a byte. Written for every pixel no triangle
/// reaches; the shading below never reaches it, so "background" and "a lit face
/// of a white part" are always distinguishable.
inline constexpr std::uint8_t THUMB_BACKGROUND = 255;

/// Ambient + diffuse, and their SUM IS BELOW 1 on purpose (0.95): a plastered
/// part is nearly white, and a fully lit face allowed to reach 255 would be
/// literally invisible against the white the user asked for.
inline constexpr float THUMB_AMBIENT = 0.32f;
inline constexpr float THUMB_DIFFUSE = 0.63f;

/// The screen basis of the shared rack: `right` and `up` span the tile,
/// `view` is the direction the camera looks ALONG (depth grows with it).
struct ThumbBasis {
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 view{0.0f, 0.0f, -1.0f};
};
[[nodiscard]] ThumbBasis thumb_basis();

/// The direction the light travels. From above and over the shoulder, with
/// UNEQUAL x and z, so a box's three visible faces get three different shades
/// and the tile reads as a solid rather than as an L. The .cpp records what was
/// measured about the two ways this can be got wrong — including one this
/// header first claimed and the counterfactual refuted.
[[nodiscard]] glm::vec3 thumb_light();

// -- the framing -------------------------------------------------------------

/// WHERE THE TILE LOOKS AND HOW WIDE. `center` is the model-space point that
/// lands in the middle of the picture; `half_extent` is half the metres the
/// picture spans, in both axes (the tile is square).
struct ThumbFrame {
    glm::vec3 center{0.0f};
    float half_extent = 0.0f;
    bool valid = false; ///< false for a box with no extent: nothing to draw
};

/// Fits the axis-aligned box [lo, hi] into the tile under the shared rack.
/// The box is the MEASURED one (render::measure_object), never the name's
/// promise: the two disagree, and the tile has to match the mesh it draws.
[[nodiscard]] ThumbFrame thumb_frame(const glm::vec3& lo, const glm::vec3& hi);

/// The same, straight from a measured object.
[[nodiscard]] ThumbFrame thumb_frame_of(const render::ObjectExtent& extent);

// -- the pictures ------------------------------------------------------------

/// Draws `streams` into `rgba` (size_px * size_px * 4, alpha 255) on white.
/// False means nothing was drawn — no triangle landed in the frame — and the
/// caller must NOT upload the result: a blank white tile reads as a broken part
/// where a placeholder reads as "still drawing".
[[nodiscard]] bool bake_thumbnail(std::span<const render::MeshData* const> streams,
                                  const ThumbFrame& frame, int size_px,
                                  std::vector<std::uint8_t>& rgba);

/// One registry object: measure it, frame it, draw every stream it carries.
[[nodiscard]] bool bake_object_thumbnail(const render::RegistryObject& obj, int size_px,
                                         std::vector<std::uint8_t>& rgba);

/// A GROUND SWATCH for the surface picker, composed exactly the way the terrain
/// fragment composes it: the grass cell as the base, the sand / rock / dirt
/// cells laid over it through a 4x4 ordered Bayer threshold against
/// render::splat_weights_of(class). The dither is not decoration — §4 rule 3
/// says the blend band is two materials' texels and never a third colour, so a
/// linear mix here would show the builder a green-grey that exists nowhere in
/// the world.
[[nodiscard]] bool bake_surface_swatch(math::SurfaceClass surface, int size_px,
                                       std::vector<std::uint8_t>& rgba);

/// A PATH SWATCH — «дорожки и другие тропы». `kind` is one of the four §8.1
/// path surfaces (COBBLE, PACKED_EARTH, SCUFFED, CUT_SLAB), whose order in the
/// path atlas IS core's PathClass ordinal (ProcTexture.h). Straight from
/// generate_proc_texture, so the swatch and the road are one texture.
[[nodiscard]] bool bake_path_swatch(render::ProcTextureKind kind, int size_px,
                                    std::vector<std::uint8_t>& rgba);

// -- the cache ---------------------------------------------------------------

/// NAME -> PICTURE, WITH A BUDGET AND A CEILING. The panel asks for a texture
/// every frame for every visible row; this answers instantly from the map, bakes
/// at most THUMB_FRAME_BUDGET new ones per frame, and never holds more than
/// THUMB_CACHE_MAX at once.
///
/// It owns no GPU type: the bake, the upload and the drop all arrive as
/// functions, which is what lets a suite drive the whole policy — budget,
/// eviction, retry cap — with counters instead of pixels.
class ThumbCache {
public:
    /// Fills `rgba` for a name. False = "cannot be drawn (yet)"; see attempts.
    using BakeFn = std::function<bool(const std::string& name, int size_px,
                                      std::vector<std::uint8_t>& rgba)>;
    /// Hands the bytes to the interface layer and returns its handle; 0 = failed.
    using UploadFn = std::function<std::uint64_t(int size_px, const std::uint8_t* rgba)>;
    using DropFn = std::function<void(std::uint64_t texture)>;

    void set_bake(BakeFn fn) { bake_ = std::move(fn); }
    void set_upload(UploadFn fn) { upload_ = std::move(fn); }
    void set_drop(DropFn fn) { drop_ = std::move(fn); }
    void set_budget(int per_frame) { budget_ = per_frame < 0 ? 0 : per_frame; }
    void set_capacity(std::size_t entries) { capacity_ = entries < 1 ? 1 : entries; }
    void set_bake_size(int px) { bake_px_ = px < 8 ? 8 : px; }

    /// Opens a frame: the budget refills. Called once per frame by whoever owns
    /// the loop — not by get(), because "per frame" is the loop's word.
    void begin_frame();

    /// The picture for a name, or 0 while it is not ready. `size_px` is the
    /// caller's WISH and is deliberately ignored for the cache key: one part is
    /// one picture (EditorPaletteView.h's hook contract).
    [[nodiscard]] std::uint64_t get(const std::string& name, int size_px);

    /// Drops every texture through the drop hook and empties the map.
    void clear();

    // -- what the suite reads (and what a live session can print) -------------
    [[nodiscard]] std::size_t size() const { return entries_.size(); }
    [[nodiscard]] int baked_this_frame() const { return baked_frame_; }
    [[nodiscard]] std::size_t bakes() const { return bakes_; }
    [[nodiscard]] std::size_t evictions() const { return evictions_; }
    /// Names given up on after THUMB_MAX_ATTEMPTS failed bakes.
    [[nodiscard]] std::size_t given_up() const { return given_up_; }
    /// Requests that had to answer 0 because the budget was spent.
    [[nodiscard]] std::size_t deferred() const { return deferred_; }

private:
    struct Entry {
        std::uint64_t texture = 0;
        std::uint64_t used = 0; ///< the tick of the last get(), for the LRU
        int attempts = 0;
        bool given_up = false;
    };

    void evict_one();

    std::unordered_map<std::string, Entry> entries_;
    BakeFn bake_;
    UploadFn upload_;
    DropFn drop_;
    std::vector<std::uint8_t> scratch_; ///< one buffer, not one per bake
    int budget_ = THUMB_FRAME_BUDGET;
    int bake_px_ = THUMB_BAKE_PX;
    std::size_t capacity_ = THUMB_CACHE_MAX;
    int baked_frame_ = 0;
    std::uint64_t tick_ = 0;
    std::size_t bakes_ = 0;
    std::size_t evictions_ = 0;
    std::size_t given_up_ = 0;
    std::size_t deferred_ = 0;
};

} // namespace dfn::app
