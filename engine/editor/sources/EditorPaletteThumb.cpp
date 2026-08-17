/*
Created: 18:08:2026 - 01:00:57
Last updated: 18:08:2026 - 01:00:57
Module: engine/editor
File: engine/editor/sources/EditorPaletteThumb.cpp

Responsibility:
- The rack, the framing and the software rasteriser declared in
  EditorPaletteThumb.h, plus the two ground swatches. Pixels only: the cache
  and its budget live in EditorPaletteThumbCache.cpp.

Dependencies:
- Uses: EditorPaletteThumb.h, engine/render (MeshData, ObjectRegistry,
  ProcTexture, Materials), glm.
- Used by: engine/app (through PaletteHooks), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PURE. No clock, no RNG, no file, no GPU: the same object gives the same bytes
  on every machine, which is the only reason a suite can assert on a pixel.
- TWO-SIDED SHADING IS DELIBERATE. The kit's parts are closed solids and the
  flora's cards are not; a back-facing card culled here would leave a hole in
  the crown, and a card lit by its back would be black. Facing the normal at the
  camera costs one dot product and removes both.
*/
/*
UPD:
- 18:08:2026 - 01:00:57: Создан вместе с EditorPaletteThumb.h.
*/

#include "engine/editor/sources/EditorPaletteThumb.h"

#include "engine/render/sources/Materials.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace dfn::app {
namespace {

constexpr float PI_F = 3.14159265358979323846f;

[[nodiscard]] float to_rad(float deg) {
    return deg * PI_F / 180.0f;
}

/// Unpacks the frozen 0xAABBGGRR vertex colour into 0..1 RGB. Alpha carries sky
/// visibility on the terrain path and means nothing here, so it is dropped
/// rather than multiplied in — a part shaded by somebody else's sky term would
/// darken for a reason no one could find.
[[nodiscard]] glm::vec3 unpack_rgb(std::uint32_t c) {
    return {static_cast<float>(c & 0xFFu) / 255.0f,
            static_cast<float>((c >> 8) & 0xFFu) / 255.0f,
            static_cast<float>((c >> 16) & 0xFFu) / 255.0f};
}

[[nodiscard]] std::uint8_t to_byte(float v) {
    return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/// The 4x4 ordered Bayer threshold fs_terrain.sc uses, in (0,1). Written as the
/// same arithmetic (2x + 3y - 4xy over two levels) rather than as a table, so
/// that the swatch and the ground dither the same way rather than merely
/// similarly.
[[nodiscard]] float bayer4(int x, int y) {
    const auto b2 = [](int px, int py) {
        const float fx = static_cast<float>(px);
        const float fy = static_cast<float>(py);
        return 2.0f * fx + 3.0f * fy - 4.0f * fx * fy;
    };
    const float coarse = b2(x & 1, y & 1);
    const float fine = b2((x >> 1) & 1, (y >> 1) & 1);
    return (coarse * 4.0f + fine + 0.5f) / 16.0f;
}

/// One cell of the terrain atlas as its own tile of `size` texels.
[[nodiscard]] std::vector<std::uint8_t> cell(render::ProcTextureKind kind, std::uint32_t size) {
    render::ProcTextureDesc desc;
    desc.kind = kind;
    desc.size = size;
    desc.seed = 1;
    return render::generate_proc_texture(desc);
}

/// The power-of-two tile the proc generator is asked for, given the pixels the
/// caller wants. The generator's feature scale is relative to the tile, so a
/// bigger tile is a SHARPER swatch and not a different material.
[[nodiscard]] std::uint32_t tile_size_for(int size_px) {
    std::uint32_t p = 32;
    while (p * 2 <= static_cast<std::uint32_t>(std::max(size_px, 32)) && p < 256) {
        p *= 2;
    }
    return p;
}

} // namespace

// ---------------------------------------------------------------------------
// the rack

ThumbBasis thumb_basis() {
    const float yaw = to_rad(THUMB_YAW_DEG);
    const float pitch = to_rad(THUMB_PITCH_DEG);
    ThumbBasis b;
    b.view = glm::normalize(glm::vec3(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch),
                                      -std::cos(yaw) * std::cos(pitch)));
    b.right = glm::normalize(glm::cross(b.view, glm::vec3(0.0f, 1.0f, 0.0f)));
    b.up = glm::cross(b.right, b.view);
    return b;
}

glm::vec3 thumb_light() {
    // From above and over the shoulder, and — the part that is a DECISION —
    // with unequal +X and +Z, so a box's two visible sides get two shades.
    // The three faces land at 0.836 / 0.635 / 0.496 of full scale, i.e. 51 and
    // 36 bytes apart on a white part, which is what the suite asserts.
    //
    // WHAT WAS MEASURED, because the first version of this comment named the
    // wrong failure and the counterfactual refuted it (Rule 30b):
    //  - light along the VIEW axis does NOT flatten the part. Shading here is
    //    two-sided, so the shade already falls off with the angle to the
    //    camera and the three faces stay 20+ bytes apart. The suite stayed
    //    green through that break, and the claim was struck rather than kept.
    //  - light with x == z DOES flatten it: the two visible sides collapse to
    //    one shade (the gap fell 36 -> 12 bytes) and the box reads as an L.
    //  - no shading at all is the worst of the three and the one a user would
    //    report: a white part on the white background the user asked for
    //    becomes literally 0 visible pixels.
    return glm::normalize(glm::vec3(-0.50f, -0.82f, -0.28f));
}

// ---------------------------------------------------------------------------
// the framing

ThumbFrame thumb_frame(const glm::vec3& lo, const glm::vec3& hi) {
    ThumbFrame f;
    const ThumbBasis b = thumb_basis();
    const glm::vec3 mid = (lo + hi) * 0.5f;

    float u_lo = std::numeric_limits<float>::max();
    float u_hi = -std::numeric_limits<float>::max();
    float v_lo = u_lo;
    float v_hi = u_hi;
    for (int corner = 0; corner < 8; ++corner) {
        const glm::vec3 p{(corner & 1) != 0 ? hi.x : lo.x, (corner & 2) != 0 ? hi.y : lo.y,
                          (corner & 4) != 0 ? hi.z : lo.z};
        const glm::vec3 d = p - mid;
        const float u = glm::dot(d, b.right);
        const float v = glm::dot(d, b.up);
        u_lo = std::min(u_lo, u);
        u_hi = std::max(u_hi, u);
        v_lo = std::min(v_lo, v);
        v_hi = std::max(v_hi, v);
    }

    // THE CENTRE IS THE PROJECTED BOX'S CENTRE, not the box's. They differ:
    // a box seen from above and to the side projects into a hexagon whose
    // middle is not the middle of the solid, and centring on the solid leaves a
    // visibly uneven margin — which reads as a part standing off-centre in its
    // tile, i.e. as a defect in the PART.
    f.center = mid + b.right * ((u_lo + u_hi) * 0.5f) + b.up * ((v_lo + v_hi) * 0.5f);
    const float span = std::max(u_hi - u_lo, v_hi - v_lo);
    f.half_extent = span * 0.5f * (1.0f + THUMB_MARGIN);
    f.valid = f.half_extent > 1e-5f;
    if (!f.valid) {
        f.half_extent = 1e-5f; // never divide by it, whatever the caller does
    }
    return f;
}

ThumbFrame thumb_frame_of(const render::ObjectExtent& extent) {
    return thumb_frame(glm::vec3(extent.lo.x, extent.bottom, extent.lo.y),
                       glm::vec3(extent.hi.x, extent.top, extent.hi.y));
}

// ---------------------------------------------------------------------------
// the rasteriser

bool bake_thumbnail(std::span<const render::MeshData* const> streams, const ThumbFrame& frame,
                    int size_px, std::vector<std::uint8_t>& rgba) {
    const int size = std::clamp(size_px, 8, 512);
    rgba.assign(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4u,
                THUMB_BACKGROUND);
    if (!frame.valid) {
        return false;
    }

    const int super = std::max(1, THUMB_SUPERSAMPLE);
    const int w = size * super;
    const std::size_t pixels = static_cast<std::size_t>(w) * static_cast<std::size_t>(w);
    // THE TWO BUFFERS ARE REUSED BETWEEN BAKES, and that is not micro-tuning:
    // at 192 px with x2 supersampling they are 1.8 MB of colour plus 0.6 MB of
    // depth, and allocating them per part cost MORE than drawing the part
    // (measured: 1.23 ms for an 80-triangle beam on the cold allocation, 0.59
    // on the warm one). Six of those a frame is most of a 8.3 ms frame spent
    // on malloc. thread_local rather than static: the frame thread bakes, and
    // a second thread taking this path must not share the canvas.
    thread_local std::vector<glm::vec3> color;
    thread_local std::vector<float> depth;
    color.assign(pixels, glm::vec3(1.0f));
    depth.assign(pixels, std::numeric_limits<float>::max());

    const ThumbBasis b = thumb_basis();
    const glm::vec3 light = thumb_light();
    const float half = frame.half_extent;
    const float to_px = static_cast<float>(w) * 0.5f / half;
    const float mid = static_cast<float>(w) * 0.5f;
    bool drew = false;

    for (const render::MeshData* stream : streams) {
        if (stream == nullptr) {
            continue;
        }
        const std::size_t tris = stream->indices.size() / 3;
        for (std::size_t t = 0; t < tris; ++t) {
            const platform::Vertex& v0 = stream->vertices[stream->indices[t * 3 + 0]];
            const platform::Vertex& v1 = stream->vertices[stream->indices[t * 3 + 1]];
            const platform::Vertex& v2 = stream->vertices[stream->indices[t * 3 + 2]];

            glm::vec3 n = glm::cross(v1.position - v0.position, v2.position - v0.position);
            const float n_len = glm::length(n);
            if (!(n_len > 1e-12f)) {
                continue; // a degenerate triangle has no face to light
            }
            n /= n_len;
            if (glm::dot(n, b.view) > 0.0f) {
                n = -n; // two-sided: see the file header
            }
            const float lambert = std::max(0.0f, glm::dot(n, -light));
            const float shade = THUMB_AMBIENT + THUMB_DIFFUSE * lambert;
            const glm::vec3 albedo =
                (unpack_rgb(v0.color_rgba) + unpack_rgb(v1.color_rgba)
                 + unpack_rgb(v2.color_rgba)) / 3.0f;
            const glm::vec3 lit = albedo * shade;

            // Screen coordinates. Orthographic, so depth is linear in the view
            // axis and a plain barycentric interpolation of it is exact.
            glm::vec3 sx{0.0f};
            glm::vec3 sy{0.0f};
            glm::vec3 sz{0.0f};
            const platform::Vertex* verts[3] = {&v0, &v1, &v2};
            for (int i = 0; i < 3; ++i) {
                const glm::vec3 d = verts[i]->position - frame.center;
                sx[i] = mid + glm::dot(d, b.right) * to_px;
                sy[i] = mid - glm::dot(d, b.up) * to_px;
                sz[i] = glm::dot(d, b.view);
            }

            const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
            if (std::fabs(area) < 1e-9f) {
                continue;
            }
            const int x0 = std::max(0, static_cast<int>(std::floor(std::min({sx[0], sx[1], sx[2]}))));
            const int x1 = std::min(w - 1, static_cast<int>(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
            const int y0 = std::max(0, static_cast<int>(std::floor(std::min({sy[0], sy[1], sy[2]}))));
            const int y1 = std::min(w - 1, static_cast<int>(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
            const float inv_area = 1.0f / area;

            for (int y = y0; y <= y1; ++y) {
                const float py = static_cast<float>(y) + 0.5f;
                for (int x = x0; x <= x1; ++x) {
                    const float px = static_cast<float>(x) + 0.5f;
                    float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) * inv_area;
                    float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) * inv_area;
                    float w2 = 1.0f - w0 - w1;
                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                        continue;
                    }
                    const float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                    const std::size_t at =
                        static_cast<std::size_t>(y) * static_cast<std::size_t>(w)
                        + static_cast<std::size_t>(x);
                    if (z >= depth[at]) {
                        continue;
                    }
                    depth[at] = z;
                    color[at] = lit;
                    drew = true;
                }
            }
        }
    }

    if (!drew) {
        return false; // nothing landed in the frame; the caller keeps its placeholder
    }

    const float inv_samples = 1.0f / static_cast<float>(super * super);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            glm::vec3 sum{0.0f};
            for (int sy2 = 0; sy2 < super; ++sy2) {
                for (int sx2 = 0; sx2 < super; ++sx2) {
                    sum += color[static_cast<std::size_t>(y * super + sy2)
                                     * static_cast<std::size_t>(w)
                                 + static_cast<std::size_t>(x * super + sx2)];
                }
            }
            sum *= inv_samples;
            const std::size_t out = (static_cast<std::size_t>(y) * static_cast<std::size_t>(size)
                                     + static_cast<std::size_t>(x)) * 4u;
            rgba[out + 0] = to_byte(sum.r);
            rgba[out + 1] = to_byte(sum.g);
            rgba[out + 2] = to_byte(sum.b);
            rgba[out + 3] = 255;
        }
    }
    return true;
}

bool bake_object_thumbnail(const render::RegistryObject& obj, int size_px,
                           std::vector<std::uint8_t>& rgba) {
    const render::MeshData* streams[] = {&obj.wood, &obj.bark, &obj.cards, &obj.ground};
    return bake_thumbnail(std::span<const render::MeshData* const>(streams),
                          thumb_frame_of(render::measure_object(obj)), size_px, rgba);
}

// ---------------------------------------------------------------------------
// the ground

bool bake_surface_swatch(math::SurfaceClass surface, int size_px, std::vector<std::uint8_t>& rgba) {
    const int size = std::clamp(size_px, 8, 512);
    const std::uint32_t tile = tile_size_for(size);
    const std::vector<std::uint8_t> grass = cell(render::ProcTextureKind::GRASS, tile);
    const std::vector<std::uint8_t> rock = cell(render::ProcTextureKind::ROCK, tile);
    const std::vector<std::uint8_t> sand = cell(render::ProcTextureKind::SAND, tile);
    const std::vector<std::uint8_t> dirt = cell(render::ProcTextureKind::DIRT, tile);
    const std::size_t need = static_cast<std::size_t>(tile) * tile * 4u;
    if (grass.size() < need || rock.size() < need || sand.size() < need || dirt.size() < need) {
        return false;
    }

    // THE SAME WEIGHTS THE MESHERS BAKE INTO THE VERTEX COLOUR. Reading them
    // from render rather than restating them here is the whole reason this
    // swatch is worth showing: a class re-weighted tomorrow moves the ground
    // and the picture of the ground together.
    const render::SplatWeights sw = render::splat_weights_of(surface);

    rgba.assign(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4u, 255);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const std::uint32_t tx =
                static_cast<std::uint32_t>(x) * tile / static_cast<std::uint32_t>(size);
            const std::uint32_t ty =
                static_cast<std::uint32_t>(y) * tile / static_cast<std::uint32_t>(size);
            const std::size_t src = (static_cast<std::size_t>(ty) * tile + tx) * 4u;
            const float threshold = bayer4(x, y);
            // fs_terrain's paint order: grass, then the wet bed, then rock,
            // then sand on top. Later wins; each step is a THRESHOLD, never a
            // mix, so no texel of a third colour can appear.
            const std::uint8_t* pick = &grass[src];
            float darken = 1.0f;
            if (threshold <= sw.bed) {
                pick = &dirt[src];
                darken = 0.68f; // the shader's wet-bed factor
            }
            if (threshold <= sw.rock) {
                pick = &rock[src];
                darken = 1.0f;
            }
            if (threshold <= sw.sand) {
                pick = &sand[src];
                darken = 1.0f;
            }
            const std::size_t out = (static_cast<std::size_t>(y) * static_cast<std::size_t>(size)
                                     + static_cast<std::size_t>(x)) * 4u;
            for (int c = 0; c < 3; ++c) {
                rgba[out + static_cast<std::size_t>(c)] =
                    to_byte(static_cast<float>(pick[c]) / 255.0f * darken);
            }
            rgba[out + 3] = 255;
        }
    }
    return true;
}

bool bake_path_swatch(render::ProcTextureKind kind, int size_px, std::vector<std::uint8_t>& rgba) {
    const int size = std::clamp(size_px, 8, 512);
    const std::uint32_t tile = tile_size_for(size);
    const std::vector<std::uint8_t> src = cell(kind, tile);
    const std::size_t need = static_cast<std::size_t>(tile) * tile * 4u;
    if (src.size() < need) {
        return false;
    }
    rgba.assign(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4u, 255);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const std::uint32_t tx =
                static_cast<std::uint32_t>(x) * tile / static_cast<std::uint32_t>(size);
            const std::uint32_t ty =
                static_cast<std::uint32_t>(y) * tile / static_cast<std::uint32_t>(size);
            const std::size_t at = (static_cast<std::size_t>(ty) * tile + tx) * 4u;
            const std::size_t out = (static_cast<std::size_t>(y) * static_cast<std::size_t>(size)
                                     + static_cast<std::size_t>(x)) * 4u;
            rgba[out + 0] = src[at + 0];
            rgba[out + 1] = src[at + 1];
            rgba[out + 2] = src[at + 2];
            rgba[out + 3] = 255;
        }
    }
    return true;
}

} // namespace dfn::app
