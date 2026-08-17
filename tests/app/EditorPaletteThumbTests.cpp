/*
Created: 18:08:2026 - 01:00:57
Last updated: 18:08:2026 - 01:00:57
Module: tests/app
File: tests/app/EditorPaletteThumbTests.cpp

Responsibility:
- THE PICTURE IN THE OBJECT MENU, measured. Every claim the thumbnail makes —
  that the part FILLS its tile whatever size it is, that the tile is white
  behind it, that the shape reads as a solid and not as a silhouette, that one
  part is drawn ONCE however many places show it, that a menu of 2412 rows
  neither stalls a frame nor eats a hundred megabytes — is a number here rather
  than a look at a screenshot.

WHY THIS SUITE EXISTS AND A FRAME DOES NOT DO ITS JOB. A screenshot of the menu
proves the tiles are drawn. It cannot show that a 0.25 m connector and a 4.6 m
beam are framed by the same rule (they look right either way at a glance), that
the second, third and fourth place a part appears reuse one texture, or that
scrolling the whole shelf does not accumulate 356 MB of them. Those are the
three ways this feature can be quietly wrong.

Dependencies:
- Uses: EditorPaletteThumb.h, engine/render (ProcMesh, ObjectRegistry,
  ProcTexture), doctest.
- Used by: ctest (app_editor_palette).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- EVERY ARM HERE HAS A CONTROL (Rule 30). The framing arms are paired with a
  deliberately unfitted frame, the shading arm with a symmetric light, the
  budget arm with an unbudgeted cache: an assertion that cannot go red is
  measuring nothing.
- AN ARM THAT SKIPS ON AN UNBAKED SHELF PROVES NOTHING. The one arm that reads
  a real .dfo has a hand-made box beside it holding the same property.
*/
/*
UPD:
- 18:08:2026 - 01:00:57: Создан — предпросмотр деталей и поверхностей (заказ 18.08).
*/

#include "engine/editor/sources/EditorPaletteThumb.h"

#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/render/sources/ProcTexture.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace dfn;
using namespace dfn::app;

namespace {

/// A closed box, the one shape whose shading and framing can be reasoned about
/// by hand — which is what makes it the right instrument here.
render::MeshData box_mesh(glm::vec3 lo, glm::vec3 hi, glm::vec3 color) {
    const std::uint32_t c = render::pack(color);
    render::MeshData m;
    const glm::vec3 a{lo.x, lo.y, lo.z};
    const glm::vec3 b{hi.x, lo.y, lo.z};
    const glm::vec3 d{hi.x, hi.y, lo.z};
    const glm::vec3 e{lo.x, hi.y, lo.z};
    const glm::vec3 f{lo.x, lo.y, hi.z};
    const glm::vec3 g{hi.x, lo.y, hi.z};
    const glm::vec3 h{hi.x, hi.y, hi.z};
    const glm::vec3 i{lo.x, hi.y, hi.z};
    render::quad(m, f, g, h, i, c); // +Z
    render::quad(m, b, a, e, d, c); // -Z
    render::quad(m, g, b, d, h, c); // +X
    render::quad(m, a, f, i, e, c); // -X
    render::quad(m, e, i, h, d, c); // +Y
    render::quad(m, a, b, g, f, c); // -Y
    return m;
}

std::vector<std::uint8_t> bake_box(glm::vec3 lo, glm::vec3 hi, glm::vec3 color, int px,
                                   float frame_scale = 1.0f) {
    const render::MeshData mesh = box_mesh(lo, hi, color);
    const render::MeshData* streams[] = {&mesh};
    ThumbFrame frame = thumb_frame(lo, hi);
    frame.half_extent *= frame_scale;
    std::vector<std::uint8_t> rgba;
    const bool ok = bake_thumbnail(std::span<const render::MeshData* const>(streams), frame, px,
                                   rgba);
    REQUIRE(ok);
    return rgba;
}

bool is_background(const std::vector<std::uint8_t>& rgba, int size, int x, int y) {
    const std::size_t at =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x))
        * 4u;
    return rgba[at] == 255 && rgba[at + 1] == 255 && rgba[at + 2] == 255;
}

int luma(const std::vector<std::uint8_t>& rgba, int size, int x, int y) {
    const std::size_t at =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x))
        * 4u;
    return (static_cast<int>(rgba[at]) + rgba[at + 1] + rgba[at + 2]) / 3;
}

/// The share of the tile's WIDER axis the drawn part actually occupies. This is
/// the number the framing exists to control.
float drawn_coverage(const std::vector<std::uint8_t>& rgba, int size) {
    int x_lo = size;
    int x_hi = -1;
    int y_lo = size;
    int y_hi = -1;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (is_background(rgba, size, x, y)) {
                continue;
            }
            x_lo = std::min(x_lo, x);
            x_hi = std::max(x_hi, x);
            y_lo = std::min(y_lo, y);
            y_hi = std::max(y_hi, y);
        }
    }
    if (x_hi < 0) {
        return 0.0f;
    }
    const int span = std::max(x_hi - x_lo + 1, y_hi - y_lo + 1);
    return static_cast<float>(span) / static_cast<float>(size);
}

float drawn_share(const std::vector<std::uint8_t>& rgba, int size) {
    int on = 0;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            on += is_background(rgba, size, x, y) ? 0 : 1;
        }
    }
    return static_cast<float>(on)
           / static_cast<float>(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
}

/// Mean colour of a swatch, as three floats — enough to tell two ground
/// materials apart without asserting on any single texel.
glm::vec3 mean_rgb(const std::vector<std::uint8_t>& rgba) {
    glm::vec3 sum{0.0f};
    const std::size_t n = rgba.size() / 4u;
    for (std::size_t i = 0; i < n; ++i) {
        sum.r += static_cast<float>(rgba[i * 4 + 0]);
        sum.g += static_cast<float>(rgba[i * 4 + 1]);
        sum.b += static_cast<float>(rgba[i * 4 + 2]);
    }
    return n == 0 ? sum : sum / static_cast<float>(n);
}

constexpr int TILE = 96;

} // namespace

// ===========================================================================
// THE FRAMING
// ===========================================================================

TEST_CASE("thumbnail: the part fills its tile, and the margin is the stated one") {
    const std::vector<std::uint8_t> tile = bake_box({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                                                    {0.55f, 0.42f, 0.30f}, TILE);
    const float coverage = drawn_coverage(tile, TILE);
    // 6% of margin means the fitted box spans 1 / 1.06 = 94.3% of the tile.
    CHECK(coverage > 0.90f);
    CHECK(coverage <= 1.0f);

    // THE CONTROL. The same box in a frame three times too wide occupies a
    // third of the tile — which is what a menu with no fitting looks like, and
    // it is what the arm above must be able to see.
    const std::vector<std::uint8_t> loose = bake_box({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                                                     {0.55f, 0.42f, 0.30f}, TILE, 3.0f);
    CHECK(drawn_coverage(loose, TILE) < 0.40f);
}

TEST_CASE("thumbnail: a 0.25 m connector and a 4.6 m beam get the SAME picture") {
    // The whole reason the frame is fitted rather than fixed. Same shape, one
    // eighteen times the other: byte-identical tiles, or the small parts of the
    // shelf are four pixels each.
    const std::vector<std::uint8_t> small = bake_box({0.0f, 0.0f, 0.0f}, {0.25f, 0.25f, 0.25f},
                                                     {0.5f, 0.5f, 0.5f}, TILE);
    const std::vector<std::uint8_t> large = bake_box({0.0f, 0.0f, 0.0f}, {4.6f, 4.6f, 4.6f},
                                                     {0.5f, 0.5f, 0.5f}, TILE);
    CHECK(small == large);

    // THE CONTROL: a box of a DIFFERENT SHAPE at the same size must not match,
    // or the comparison above would pass for a rasteriser that draws nothing.
    const std::vector<std::uint8_t> flat = bake_box({0.0f, 0.0f, 0.0f}, {4.6f, 1.0f, 4.6f},
                                                    {0.5f, 0.5f, 0.5f}, TILE);
    CHECK(flat != large);
}

TEST_CASE("thumbnail: where a part sits in the world does not change its picture") {
    const std::vector<std::uint8_t> at_origin = bake_box({0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.5f},
                                                         {0.4f, 0.5f, 0.6f}, TILE);
    const std::vector<std::uint8_t> far_away = bake_box({317.0f, -48.0f, 902.0f},
                                                        {319.0f, -47.0f, 902.5f},
                                                        {0.4f, 0.5f, 0.6f}, TILE);
    CHECK(at_origin == far_away);
}

// ===========================================================================
// THE WHITE BACKGROUND, AND WHAT SURVIVES IT
// ===========================================================================

TEST_CASE("thumbnail: the ground is white and a WHITE part is still visible on it") {
    // The user asked for a white background («на белом фоне сам объект»), and
    // the kit's plaster is nearly white. If a lit face were allowed to reach
    // 255, a plastered wall would be an empty tile — the exact opposite of the
    // request, and invisible to any arm that only checks "is the background
    // white".
    const std::vector<std::uint8_t> tile = bake_box({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                                                    {1.0f, 1.0f, 1.0f}, TILE);
    const float share = drawn_share(tile, TILE);
    CHECK(share > 0.35f); // a fitted cube covers most of its tile
    CHECK(share < 0.95f); // and never all of it: the margin is real

    int brightest = 0;
    for (int y = 0; y < TILE; ++y) {
        for (int x = 0; x < TILE; ++x) {
            if (!is_background(tile, TILE, x, y)) {
                brightest = std::max(brightest, luma(tile, TILE, x, y));
            }
        }
    }
    // 0.32 + 0.63 = 0.95 of full scale, and the gap to 255 is what separates
    // a white part from the paper it is drawn on.
    CHECK(brightest <= 245);
    CHECK(brightest >= 200); // and it is not a grey smudge either
}

TEST_CASE("thumbnail: a box reads as a SOLID — three faces, three shades") {
    const std::vector<std::uint8_t> tile = bake_box({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
                                                    {1.0f, 1.0f, 1.0f}, TILE);
    std::map<int, int> levels;
    for (int y = 0; y < TILE; ++y) {
        for (int x = 0; x < TILE; ++x) {
            if (!is_background(tile, TILE, x, y)) {
                ++levels[luma(tile, TILE, x, y)];
            }
        }
    }
    std::vector<std::pair<int, int>> by_count(levels.begin(), levels.end());
    std::sort(by_count.begin(), by_count.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    REQUIRE(by_count.size() >= 3);
    std::vector<int> faces{by_count[0].first, by_count[1].first, by_count[2].first};
    std::sort(faces.begin(), faces.end());
    // Predicted by hand from thumb_light(): 0.496 / 0.635 / 0.836 of full
    // scale, i.e. 36 and 51 bytes apart on a white part.
    CHECK(faces[1] - faces[0] >= 20);
    CHECK(faces[2] - faces[1] >= 20);
}

// ===========================================================================
// A REAL PART OFF THE SHELF
// ===========================================================================

TEST_CASE("thumbnail: a real baked part draws, and it is framed like the box") {
    const std::filesystem::path shelf = "assets/objects/parts";
    std::error_code ec;
    if (!std::filesystem::is_directory(shelf, ec)) {
        MESSAGE("shelf not baked; the hand-made arms above hold the same properties");
        return;
    }
    int checked = 0;
    for (const char* name : {"beam-dark-12x1x1-w03", "joint-timber-d50-n4-h13-w03",
                             "wall-log-timber-12x1x13-blind-w03"}) {
        const auto obj = render::read_object(shelf / (std::string(name) + ".dfo"));
        if (!obj) {
            continue;
        }
        std::vector<std::uint8_t> rgba;
        // THE BUDGET'S PREMISE, MEASURED RATHER THAN ASSERTED. Six bakes a
        // frame is only a sane number if one bake is a fraction of a
        // millisecond; the header says so, and this is where that sentence is
        // checked against the machine instead of being believed.
        const auto started = std::chrono::steady_clock::now();
        REQUIRE(bake_object_thumbnail(*obj, THUMB_BAKE_PX, rgba));
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - started).count();
        MESSAGE("«" << std::string(name) << "»: " << obj->wood.triangle_count() + obj->bark.triangle_count()
                    << " треугольников, выпечка " << ms << " мс");
        // A whole frame is 8.3 ms at 120 fps; the budget spends at most six of
        // these. Ten milliseconds for ONE tile would mean the number is wrong,
        // not that the machine is slow.
        CHECK(ms < 10.0);
        CHECK(rgba.size()
              == static_cast<std::size_t>(THUMB_BAKE_PX) * THUMB_BAKE_PX * 4u);
        // The same fitting rule as the box, on a mesh nobody wrote by hand.
        CHECK(drawn_coverage(rgba, THUMB_BAKE_PX) > 0.90f);
        // AND IT IS NOT A BLANK TILE. A rasteriser that misses would still pass
        // a coverage test computed over zero pixels, which is why the share is
        // asserted too.
        CHECK(drawn_share(rgba, THUMB_BAKE_PX) > 0.02f);
        ++checked;
    }
    CHECK(checked > 0);
}

// ===========================================================================
// THE CACHE: BUDGET, KEY, CEILING, GIVING UP
// ===========================================================================

namespace {

/// A cache wired to counters instead of to a GPU. Bakes a one-pixel picture,
/// hands out increasing handles, and remembers what was dropped.
struct FakeGpu {
    int bakes = 0;
    int uploads = 0;
    int drops = 0;
    std::uint64_t next = 1;
    bool bake_ok = true;

    void wire(ThumbCache& cache) {
        cache.set_bake([this](const std::string&, int px, std::vector<std::uint8_t>& rgba) {
            ++bakes;
            if (!bake_ok) {
                return false;
            }
            rgba.assign(static_cast<std::size_t>(px) * static_cast<std::size_t>(px) * 4u, 200);
            return true;
        });
        cache.set_upload([this](int, const std::uint8_t*) {
            ++uploads;
            return next++;
        });
        cache.set_drop([this](std::uint64_t) { ++drops; });
    }
};

std::string row(int i) {
    return "part-" + std::to_string(i);
}

} // namespace

TEST_CASE("thumbnail cache: a frame draws at most the budget, and the rest arrive later") {
    ThumbCache cache;
    FakeGpu gpu;
    gpu.wire(cache);
    cache.set_budget(THUMB_FRAME_BUDGET);
    cache.set_bake_size(16);

    cache.begin_frame();
    int ready = 0;
    for (int i = 0; i < 24; ++i) {
        ready += cache.get(row(i), 96) != 0 ? 1 : 0;
    }
    CHECK(cache.baked_this_frame() == THUMB_FRAME_BUDGET);
    CHECK(ready == THUMB_FRAME_BUDGET);
    CHECK(cache.deferred() == 24u - THUMB_FRAME_BUDGET);

    // A handful more frames and the whole visible page is drawn. This is the
    // "menu fills in" the budget is FOR, and it has to TERMINATE — the count is
    // derived from the budget rather than typed, so lowering the budget moves
    // the arm instead of breaking it.
    const int frames_needed = (24 + THUMB_FRAME_BUDGET - 1) / THUMB_FRAME_BUDGET;
    for (int frame = 1; frame < frames_needed; ++frame) {
        cache.begin_frame();
        for (int i = 0; i < 24; ++i) {
            (void)cache.get(row(i), 96);
        }
    }
    cache.begin_frame();
    ready = 0;
    for (int i = 0; i < 24; ++i) {
        ready += cache.get(row(i), 96) != 0 ? 1 : 0;
    }
    CHECK(ready == 24);
    CHECK(gpu.bakes == 24);

    // THE CONTROL. Without a budget the same page draws all 24 in one frame —
    // the hitch the budget exists to prevent, and the proof this arm can go red.
    ThumbCache greedy;
    FakeGpu greedy_gpu;
    greedy_gpu.wire(greedy);
    greedy.set_budget(1000);
    greedy.set_bake_size(16);
    greedy.begin_frame();
    for (int i = 0; i < 24; ++i) {
        (void)greedy.get(row(i), 96);
    }
    CHECK(greedy.baked_this_frame() == 24);
}

TEST_CASE("thumbnail cache: one part is one picture, at every size it is shown") {
    ThumbCache cache;
    FakeGpu gpu;
    gpu.wire(cache);
    cache.set_bake_size(16);
    cache.begin_frame();
    // The three places a part appears: the 44 px strip, the 96 px tile, the
    // 192 px family preview. Caching by (name, size) would bake three.
    const std::uint64_t a = cache.get("beam", 44);
    const std::uint64_t b = cache.get("beam", 96);
    const std::uint64_t c = cache.get("beam", 192);
    CHECK(a != 0);
    CHECK(a == b);
    CHECK(b == c);
    CHECK(gpu.bakes == 1);
    CHECK(gpu.uploads == 1);
}

TEST_CASE("thumbnail cache: the ceiling holds, and what it throws out is dropped") {
    ThumbCache cache;
    FakeGpu gpu;
    gpu.wire(cache);
    cache.set_bake_size(16);
    cache.set_capacity(8);
    cache.set_budget(1);
    // Scrolling the shelf: forty distinct parts, one per frame.
    for (int i = 0; i < 40; ++i) {
        cache.begin_frame();
        (void)cache.get(row(i), 96);
    }
    CHECK(cache.size() <= 8u);
    CHECK(gpu.uploads == 40);
    // EVERY texture the cache no longer holds went back through the drop hook.
    // A cache that merely forgot them would leak 356 MB over the whole shelf,
    // and nothing on screen would look wrong while it did.
    CHECK(gpu.uploads - gpu.drops == static_cast<int>(cache.size()));
    CHECK(cache.evictions() == 32u);
}

TEST_CASE("thumbnail cache: a name that cannot be drawn is given up on, not retried for ever") {
    ThumbCache cache;
    FakeGpu gpu;
    gpu.wire(cache);
    gpu.bake_ok = false;
    cache.set_bake_size(16);
    for (int frame = 0; frame < 10; ++frame) {
        cache.begin_frame();
        CHECK(cache.get("no-such-part", 96) == 0u);
    }
    // Three attempts, then silence: without the cap this one row would spend a
    // budget slot every frame and the parts behind it would never be drawn.
    CHECK(gpu.bakes == THUMB_MAX_ATTEMPTS);
    CHECK(cache.given_up() == 1u);
}

TEST_CASE("thumbnail cache: with no source wired the menu still answers") {
    // Rule 3 in its smallest form: no bake, no upload, no window — the panel
    // gets 0 for everything and draws its name-only rows.
    ThumbCache cache;
    cache.begin_frame();
    CHECK(cache.get("beam", 96) == 0u);
    CHECK(cache.size() == 0u);
}

// ===========================================================================
// THE GROUND: SURFACES AND PATHS
// ===========================================================================

TEST_CASE("swatch: the four paintable surfaces are told apart by their pictures") {
    const math::SurfaceClass rows[] = {math::SurfaceClass::Grass,
                                       math::SurfaceClass::GrassRockBlend,
                                       math::SurfaceClass::Rock, math::SurfaceClass::Sand};
    std::vector<glm::vec3> means;
    for (math::SurfaceClass s : rows) {
        std::vector<std::uint8_t> rgba;
        REQUIRE(bake_surface_swatch(s, 64, rgba));
        CHECK(rgba.size() == 64u * 64u * 4u);
        means.push_back(mean_rgb(rgba));
    }
    for (std::size_t a = 0; a < means.size(); ++a) {
        for (std::size_t b = a + 1; b < means.size(); ++b) {
            const glm::vec3 d = means[a] - means[b];
            const float dist = std::sqrt(d.r * d.r + d.g * d.g + d.b * d.b);
            // A swatch a builder cannot tell from its neighbour is a caption
            // with extra steps — which is the state the user complained about.
            CHECK(dist > 8.0f);
        }
    }

    // THE CONTROL: the same class twice is the same picture, so the separation
    // above is measuring the CLASS and not the noise.
    std::vector<std::uint8_t> twice_a;
    std::vector<std::uint8_t> twice_b;
    REQUIRE(bake_surface_swatch(math::SurfaceClass::Rock, 64, twice_a));
    REQUIRE(bake_surface_swatch(math::SurfaceClass::Rock, 64, twice_b));
    CHECK(twice_a == twice_b);
}

TEST_CASE("swatch: a path is drawn from the SAME texture the road is") {
    // «для дорожек и других троп тоже предпросмотр сделать». The four §8.1
    // surfaces, and the swatch is the generator's own bytes rather than a
    // second idea of what cobble looks like — the two could not drift apart if
    // somebody wanted them to.
    const render::ProcTextureKind kinds[] = {
        render::ProcTextureKind::COBBLE, render::ProcTextureKind::PACKED_EARTH,
        render::ProcTextureKind::SCUFFED, render::ProcTextureKind::CUT_SLAB};
    std::vector<glm::vec3> means;
    for (render::ProcTextureKind k : kinds) {
        std::vector<std::uint8_t> rgba;
        REQUIRE(bake_path_swatch(k, 64, rgba));

        render::ProcTextureDesc desc;
        desc.kind = k;
        desc.size = 64;
        desc.seed = 1;
        const std::vector<std::uint8_t> source = render::generate_proc_texture(desc);
        REQUIRE(source.size() == rgba.size());
        CHECK(rgba == source);
        means.push_back(mean_rgb(rgba));
    }
    for (std::size_t a = 0; a < means.size(); ++a) {
        for (std::size_t b = a + 1; b < means.size(); ++b) {
            const glm::vec3 d = means[a] - means[b];
            CHECK(std::sqrt(d.r * d.r + d.g * d.g + d.b * d.b) > 4.0f);
        }
    }
}
