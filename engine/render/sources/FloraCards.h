/*
Created: 09:08:2026 - 20:21:13
Last updated: 09:08:2026 - 20:21:13
Module: engine/render
File: engine/render/sources/FloraCards.h

Responsibility:
- The alpha-cutout leaf CARD vocabulary: the procedurally generated leaf mask
  atlas (no image files, Q13) laid out as SHAPE x COLOUR, and the single card
  emitter that writes the foliage vertex contract render's "foliage" program
  reads.

Key items:
- FloraSeason, LeafAtlas, generate_leaf_atlas(), leaf_tile_uv(),
  leaf_tone_color(), LeafCardParams, emit_leaf_card().

Dependencies:
- Uses: glm (MeshData is forward-declared; ProcMesh.h is included by the .cpp).
- Used by: ProcFlora (card placement), RenderSystem (atlas upload),
  tests/render/ProcFloraTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md §3.8.
- PURE AND DETERMINISTIC. Same arguments -> byte-identical pixels and vertices.
- THE VERTEX COLOUR CONTRACT ON THE "foliage" PROGRAM IS NOT ALBEDO (render's
  vs_foliage/fs_foliage): r = sway weight, g = per-instance phase, b = per-card
  value jitter, a = sky visibility (render's interior-lighting channel — leave
  it at 1.0). Albedo comes from the atlas tile. Do not "fix" this to look like
  the opaque path.
- b is filled PER CARD, identically on all six vertices. Varying it per vertex
  turns the jitter into a gradient across the card, which is not what it is.
*/
/*
UPD:
- 09:08:2026 - 20:21:13: Created — leaf mask atlas + card emitter (user
  direction «листву плоскими прозрачными большими плоскими наборами
  листочков»; render's SHAPE x COLOUR atlas ruling).
*/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <vector>

namespace dfn::render {

/// Declared, not included: FloraSpecies.h needs this file's vocabulary
/// (LeafShape / LeafTone / FloraSeason) and must not drag in the whole mesh
/// header to get it.
struct MeshData;

/// Seasons are a DATA SWAP, never a world rebuild (LANDSCAPE §5.11): the mesh
/// stores an atlas tile, the atlas stores the colour. Changing season
/// regenerates ONE texture and re-uploads it — no mesh, no chunk, no baked
/// jitter is invalidated.
enum class FloraSeason : uint8_t {
    Summer = 0,
    Autumn = 1,
    Winter = 2,
};

/// Atlas geometry. Column = leaf SHAPE, row = leaf TONE, so one tile IS the
/// (shape, colour) pair: colour costs zero extra vertex bytes and is not welded
/// to shape — the same shape can appear light and dark in one crown, which is
/// what a crown that is 79-86 % leaf in its core needs to read as volume
/// (docs/specs/flora.md §3.10).
inline constexpr uint32_t LEAF_ATLAS_SHAPES = 4;
inline constexpr uint32_t LEAF_ATLAS_TONES = 8;
inline constexpr uint32_t LEAF_ATLAS_TILE_PX = 64;

/// Leaf shape columns. A species names the band it may draw from; the SHAPE is
/// silhouette work, the TONE is value work, and they are deliberately free to
/// vary independently.
enum class LeafShape : uint8_t {
    RoundLobed = 0, ///< broad lobed mass — the broadleaf default
    OvalSpray = 1,  ///< elongated leaning spray — clump and rim fill
    RaggedTip = 2,  ///< tapering wedge — branch tips and crown top
    NeedleFan = 3,  ///< conifer spray (reserved; pine still uses cone tiers)
};

/// Global foliage tone table. Rows are shared by every species in a chunk
/// because scatter bakes ONE merged buffer per chunk — all species, all cards,
/// one draw, one texture. Each species owns a contiguous band.
enum class LeafTone : uint8_t {
    OakMid = 0,
    OakDeep = 1,
    OakSunlit = 2,
    BirchLight = 3,
    BirchPale = 4,
    WillowDark = 5,
    WillowOlive = 6,
    ConiferDark = 7,
};

/// One generated atlas, ready for IRenderer::create_texture (RGBA8, row-major,
/// row 0 = top). `pixels.size() == width * height * 4`.
struct LeafAtlas {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t tile_px = 0;
    uint32_t shapes = LEAF_ATLAS_SHAPES;
    uint32_t tones = LEAF_ATLAS_TONES;
};

/// Generates the mask atlas. `tile_px` is the side of one tile (>= 16; 64 is
/// the default and gives a 256x512 image).
///
/// ALPHA IS BINARY (0 or 255) BY DESIGN: the material is an alpha TEST, the
/// target is 640x360 point-sampled, and a soft edge under the palette post
/// becomes dither, i.e. noise on few-pixel geometry (render's PALETTE SIGNAL
/// STRENGTH rule). Hard edges are also the project's look.
[[nodiscard]] LeafAtlas generate_leaf_atlas(uint32_t tile_px = LEAF_ATLAS_TILE_PX,
                                            FloraSeason season = FloraSeason::Summer);

/// uv rectangle of one tile: (u_min, v_min, u_max, v_max), inset by half a
/// texel so a point sampler can never straddle two tiles.
[[nodiscard]] glm::vec4 leaf_tile_uv(LeafShape shape, LeafTone tone,
                                     uint32_t tile_px = LEAF_ATLAS_TILE_PX);

/// The tone's base colour for a season. Exposed so the value-ORDER invariant
/// (design §5.11: species value order must hold in EVERY season) is testable
/// without decoding pixels.
[[nodiscard]] glm::vec3 leaf_tone_color(LeafTone tone, FloraSeason season);

/// True when a species with this tone still carries foliage in this season.
/// Winter costs one boolean: deciduous cards are simply not emitted, and the
/// skeleton is generated regardless (LANDSCAPE §5.11).
[[nodiscard]] bool leaf_tone_has_foliage(LeafTone tone, FloraSeason season);

/// Everything one card needs. All of it is per CARD — nothing here is looked up
/// from a global, and nothing varies per vertex except position and the sway
/// weight derived from it.
struct LeafCardParams {
    glm::vec3 center{0.0f};          ///< card centre, model space (m)
    glm::vec3 normal{0.0f, 0.0f, 1.0f}; ///< card facing; FIXED, never camera-facing
    float half_width = 1.0f;         ///< m, across the card
    float half_height = 1.0f;        ///< m, up the card
    float roll = 0.0f;               ///< rad, rotation inside the card plane
    LeafShape shape = LeafShape::RoundLobed;
    LeafTone tone = LeafTone::OakMid;
    float value_jitter = 0.5f;       ///< 0..1 -> vertex BLUE, one value per card
    float phase = 0.0f;              ///< 0..1 -> vertex GREEN, per instance
    glm::vec3 sway_origin{0.0f};     ///< the attachment: sway weight is 0 here
    float sway_span = 1.0f;          ///< m at which the sway weight reaches 1
    uint32_t tile_px = LEAF_ATLAS_TILE_PX;
};

/// Emits one card: a quad, two triangles, six vertices, both faces the same
/// plane normal (fs_foliage flips it toward the viewer — a flat card has no
/// meaningful back face).
void emit_leaf_card(MeshData& m, const LeafCardParams& p);

} // namespace dfn::render
