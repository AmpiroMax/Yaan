/*
Module: tests
File: tests/render/LodTerrainTests.cpp

Responsibility:
- Tests for the LOD DRAWING half: that a node is never drawn before its mesh
  exists, that a node core rebuilds does not restart its dissolve, that the
  fade reaches the renderer, and that meshes are actually released.

Key items:
- doctest cases over engine/render LodTerrain driven by the null renderer.

Dependencies:
- Uses: doctest, engine/render LodTerrain, engine/platform/render null backend.
- Used by: ctest (render_lod_terrain).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- GPU-free: the null renderer is a runnable mode, not a stub (Rule 3).
*/

#include "engine/render/sources/LodTerrain.h"

#include "engine/platform/render/sources/null/CreateNullRenderer.h"
#include "engine/render/sources/TerrainLod.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using dfn::render::LodNode;
using dfn::render::LodTerrain;

/// A coarse node as core will deliver it: 129 samples at the level's voxel
/// size, origin on the node grid.
struct NodeField {
    std::vector<uint16_t> heights;
    dfn::math::HeightFieldView view;
};

NodeField make_node_field(const LodNode& node) {
    NodeField f;
    const uint32_t res = dfn::render::LOD_NODE_VOXELS + 1;
    const float step = dfn::render::LOD_VOXEL_SIZE_M[node.level];
    const float size = dfn::render::lod_node_size_m(node.level);
    f.heights.assign(static_cast<size_t>(res) * res, 0);
    for (uint32_t z = 0; z < res; ++z) {
        for (uint32_t x = 0; x < res; ++x) {
            f.heights[static_cast<size_t>(z) * res + x] =
                static_cast<uint16_t>((x * 31u + z * 17u) % 4096u);
        }
    }
    f.view.chunk_coord = {node.x, node.z};
    f.view.origin = {static_cast<float>(node.x) * size,
                     static_cast<float>(node.z) * size};
    f.view.resolution = res;
    f.view.step = step;
    f.view.heights = f.heights;
    f.view.height_scale = 0.01f;
    f.view.height_offset = 0.0f;
    return f;
}

/// A frustum that accepts everything, so culling never confounds a residency
/// assertion. The cull itself is exercised by the case that needs it.
dfn::math::Frustum accept_all_frustum() {
    dfn::math::Frustum f{};
    for (dfn::math::Plane& plane : f.planes) {
        plane.normal = {0.0f, 1.0f, 0.0f};
        plane.d = 1.0e9f; // every point sits far on the inside of every plane
    }
    return f;
}

} // namespace

TEST_CASE("a node is never drawn before core has delivered its mesh") {
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer != nullptr);
    REQUIRE(renderer->init({}));

    LodTerrain lod;
    lod.set_world_bounds({0.0f, 0.0f}, {2048.0f, 2048.0f});
    lod.set_enabled(true);

    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};
    lod.update(eye, 0.0f);
    REQUIRE_FALSE(lod.to_load().empty());
    CHECK(lod.resident_count() == 0);
    // to_load is the per-frame DIFF, so it has to be captured now: the next
    // update clears it and the node is never announced twice.
    const LodNode first = lod.to_load()[0];

    const dfn::math::Frustum all = accept_all_frustum();
    // THE CONTROL for the no-holes guarantee: nodes are SELECTED and their
    // meshes are in flight. A renderer that drew them now would be drawing
    // nothing — the hole in the ground this whole state machine exists to
    // prevent. Even after a full fade's worth of time, zero draws.
    lod.update(eye, dfn::render::LOD_FADE_SECONDS * 2.0f);
    CHECK(lod.draw(*renderer, all, dfn::platform::ProgramHandle{1},
                   dfn::platform::TextureHandle{})
          == 0);

    // Deliver one node; only that one may draw.
    const NodeField field = make_node_field(first);
    lod.upload(*renderer, first, field.view, nullptr);
    CHECK(lod.resident_count() == 1);
    lod.update(eye, dfn::render::LOD_FADE_SECONDS);
    CHECK(lod.draw(*renderer, all, dfn::platform::ProgramHandle{1},
                   dfn::platform::TextureHandle{})
          == 1);

    lod.destroy_all(*renderer);
    renderer->shutdown();
}

TEST_CASE("the fade a node draws with is the residency's, not a guess") {
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer->init({}));

    LodTerrain lod;
    lod.set_world_bounds({0.0f, 0.0f}, {2048.0f, 2048.0f});
    lod.set_enabled(true);
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};

    lod.update(eye, 0.0f);
    std::vector<LodNode> wanted(lod.to_load().begin(), lod.to_load().end());
    REQUIRE_FALSE(wanted.empty());
    for (const LodNode& n : wanted) {
        const NodeField f = make_node_field(n);
        lod.upload(*renderer, n, f.view, nullptr);
    }

    // A third of a fade in, every drawn node is PARTIALLY dissolved. A
    // renderer that ignored DrawParams::fade would show the incoming level at
    // full opacity over the outgoing one, which is the pop the dissolve
    // exists to remove — and would be invisible to a draw-count assertion, so
    // the fade value itself is what is checked.
    lod.update(eye, dfn::render::LOD_FADE_SECONDS / 3.0f);
    bool saw_partial = false;
    for (const auto& d : lod.residency_draws()) {
        CHECK(d.fade > 0.0f);
        CHECK(d.fade <= 1.0f);
        if (d.fade < 1.0f) {
            saw_partial = true;
        }
    }
    CHECK(saw_partial);

    lod.destroy_all(*renderer);
    renderer->shutdown();
}

TEST_CASE("re-uploading a node replaces its mesh and keeps its dissolve") {
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer->init({}));

    LodTerrain lod;
    lod.set_world_bounds({0.0f, 0.0f}, {2048.0f, 2048.0f});
    lod.set_enabled(true);
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};

    lod.update(eye, 0.0f);
    const LodNode node = lod.to_load()[0];
    const NodeField f = make_node_field(node);
    lod.upload(*renderer, node, f.view, nullptr);
    lod.update(eye, dfn::render::LOD_FADE_SECONDS / 4.0f);

    float fade_before = 0.0f;
    for (const auto& d : lod.residency_draws()) {
        if (d.node == node) {
            fade_before = d.fade;
        }
    }
    REQUIRE(fade_before > 0.0f);
    REQUIRE(fade_before < 1.0f);

    // CONTROL: a re-upload that reset the fade would make a node core rebuilt
    // dissolve back to nothing and reappear — a pop caused by the very
    // machinery that exists to prevent one. One node resident, one mesh.
    lod.upload(*renderer, node, f.view, nullptr);
    CHECK(lod.resident_count() == 1);
    lod.update(eye, 0.0f);
    for (const auto& d : lod.residency_draws()) {
        if (d.node == node) {
            CHECK(d.fade == doctest::Approx(fade_before));
        }
    }

    lod.destroy_all(*renderer);
    renderer->shutdown();
}

TEST_CASE("disabling LOD offers everything for release instead of stranding it") {
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer->init({}));

    LodTerrain lod;
    lod.set_world_bounds({0.0f, 0.0f}, {2048.0f, 2048.0f});
    lod.set_enabled(true);
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};

    lod.update(eye, 0.0f);
    std::vector<LodNode> wanted(lod.to_load().begin(), lod.to_load().end());
    for (const LodNode& n : wanted) {
        const NodeField f = make_node_field(n);
        lod.upload(*renderer, n, f.view, nullptr);
    }
    lod.update(eye, dfn::render::LOD_FADE_SECONDS);
    REQUIRE(lod.resident_count() == wanted.size());

    // CONTROL: skipping the residency update while disabled would leave every
    // mesh resident forever, with nothing ever offered to core's
    // release_coarse_node — a leak that no draw-count check would notice.
    lod.set_enabled(false);
    lod.update(eye, dfn::render::LOD_FADE_SECONDS * 2.0f);
    CHECK(lod.to_release().size() == wanted.size());
    for (const LodNode& n : lod.to_release()) {
        lod.drop(*renderer, n);
    }
    CHECK(lod.resident_count() == 0);

    lod.destroy_all(*renderer);
    renderer->shutdown();
}

TEST_CASE("the streamed rectangle removes nodes from the draw list entirely") {
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer->init({}));

    LodTerrain lod;
    lod.set_world_bounds({0.0f, 0.0f}, {2048.0f, 2048.0f});
    lod.set_enabled(true);
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};

    lod.update(eye, 0.0f);
    const size_t without_rect = lod.selected_count();
    REQUIRE(without_rect > 0);

    // CONTROL: with the chunk ring declared, the nodes covering it must be
    // gone from the SELECTION, not merely skipped at draw time — a node that
    // is still selected is still requested from core, still meshed and still
    // paid for.
    lod.set_resident_rect({768.0f, 768.0f}, {1280.0f, 1280.0f});
    lod.update(eye, 0.0f);
    CHECK(lod.selected_count() < without_rect);

    lod.destroy_all(*renderer);
    renderer->shutdown();
}

TEST_CASE("pending() is the standing set the ferry retries, not a one-shot diff") {
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer->init({}));

    LodTerrain lod;
    lod.set_world_bounds({0.0f, 0.0f}, {2048.0f, 2048.0f});
    lod.set_enabled(true);
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};

    lod.update(eye, 0.0f);
    const size_t announced = lod.to_load().size();
    REQUIRE(announced > 0);
    CHECK(lod.pending().size() == announced);

    // THE CONTROL, and it is the bug a ferry written against to_load() alone
    // would ship: to_load is a per-frame DIFF and names a node exactly once,
    // so by the next frame it is EMPTY — while core, which admits nodes under
    // a budget, has not answered yet. A ferry that only ever looked at
    // to_load() would request every node once, never collect one, and the
    // distant ground would simply never appear.
    lod.update(eye, 0.016f);
    CHECK(lod.to_load().empty());
    CHECK(lod.pending().size() == announced);

    // Delivering a node moves it out of pending, and only that one.
    const LodNode first = lod.pending()[0];
    const NodeField f = make_node_field(first);
    lod.upload(*renderer, first, f.view, nullptr);
    lod.update(eye, 0.016f);
    CHECK(lod.pending().size() == announced - 1);
    for (const LodNode& n : lod.pending()) {
        CHECK_FALSE(n == first);
    }

    lod.destroy_all(*renderer);
    renderer->shutdown();
}

TEST_CASE("a moved rectangle re-ships stale clips without ever dropping the draw") {
    // The straddle-ring fix's residency half. A node overlapping the resident
    // rectangle is meshed WITHOUT the overlapped cells; when the player
    // crosses a chunk boundary the rectangle moves, the old clip is wrong,
    // and the node must be re-uploaded — but its old mesh must keep drawing
    // until the replacement lands, because a frame with neither mesh is a
    // hole in the ground at 500 m.
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer->init({}));

    LodTerrain lod;
    lod.set_world_bounds({0.0f, 0.0f}, {2048.0f, 2048.0f});
    lod.set_enabled(true);
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};
    // The real streaming rectangle: 5 chunks of 256 m around the eye's chunk.
    lod.set_resident_rect({512.0f, 512.0f}, {1792.0f, 1792.0f});

    lod.update(eye, 0.0f);
    // {1,3,1} spans x 1536..2048, z 512..1024: straddles the max-x edge at
    // 1792 (a 1280 m rect always cuts the 512 m level-1 grid somewhere).
    // {1,0,0} spans 0..512 on both axes: wholly outside, clip empty.
    const LodNode straddler{1, 3, 1};
    const LodNode outsider{1, 0, 0};
    REQUIRE(std::find(lod.pending().begin(), lod.pending().end(), straddler)
            != lod.pending().end());
    REQUIRE(std::find(lod.pending().begin(), lod.pending().end(), outsider)
            != lod.pending().end());

    const NodeField sf = make_node_field(straddler);
    const NodeField of = make_node_field(outsider);
    lod.upload(*renderer, straddler, sf.view, nullptr);
    lod.upload(*renderer, outsider, of.view, nullptr);
    lod.update(eye, 0.016f);
    const auto in_pending = [&](const LodNode& n) {
        return std::find(lod.pending().begin(), lod.pending().end(), n)
               != lod.pending().end();
    };
    CHECK_FALSE(in_pending(straddler));
    CHECK_FALSE(in_pending(outsider));

    // The player crosses a chunk boundary: the rectangle moves one chunk east.
    lod.set_resident_rect({768.0f, 768.0f}, {2048.0f, 2048.0f});
    lod.update(eye, 0.016f);

    // The straddler's clipped region changed -> re-shipped via pending().
    CHECK(in_pending(straddler));
    // CONTROL: the outsider's clip is empty under both rectangles — a re-ship
    // list that names it would rebuild every node on every chunk crossing,
    // which is the churn this mechanism exists to avoid.
    CHECK_FALSE(in_pending(outsider));

    // NEVER A HOLE: while stale, the straddler still draws its old mesh.
    bool drawn = false;
    for (const auto& d : lod.residency_draws()) {
        if (d.node == straddler) {
            drawn = true;
        }
    }
    CHECK(drawn);

    // Re-upload (the ferry's next iteration) clears the staleness.
    lod.upload(*renderer, straddler, sf.view, nullptr);
    lod.update(eye, 0.016f);
    CHECK_FALSE(in_pending(straddler));

    lod.destroy_all(*renderer);
    renderer->shutdown();
}

TEST_CASE("a healthy ring reads as ZERO on pending() — which counter a readout "
          "asks decides what it can see") {
    // EARNED, NOT HYPOTHETICAL. The debug overlay showed "lod 0" taken from
    // pending(), and two zones independently read it as "the far-detail ring
    // never populates". One of them nearly published it as a defect in a
    // subsystem that turned out to be healthy, and the other had the same
    // number from the same field, so the two observations agreeing was not
    // confirmation — both instruments had the same blind spot. pending() is
    // the AWAITING-UPLOAD list: it reads as ABSENCE exactly when everything
    // has arrived.
    auto renderer = dfn::platform::create_null_renderer();
    REQUIRE(renderer != nullptr);
    REQUIRE(renderer->init({}));

    LodTerrain lod;
    lod.set_world_bounds({0.0f, 0.0f}, {2048.0f, 2048.0f});
    lod.set_enabled(true);
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};

    lod.update(eye, 0.0f);
    REQUIRE_FALSE(lod.to_load().empty());
    std::vector<LodNode> announced(lod.to_load().begin(), lod.to_load().end());
    for (const LodNode& n : announced) {
        const NodeField field = make_node_field(n);
        lod.upload(*renderer, n, field.view, nullptr);
    }
    lod.update(eye, dfn::render::LOD_FADE_SECONDS);

    const dfn::math::Frustum all = accept_all_frustum();
    const size_t drawn = lod.draw(*renderer, all, dfn::platform::ProgramHandle{1},
                                  dfn::platform::TextureHandle{});

    // THE STATE THE READOUT MISREPORTED: everything delivered, everything
    // drawing, and pending() empty. Zero here means healthy.
    CHECK(lod.pending().empty());
    CHECK(lod.selected_count() > 0);
    CHECK(lod.resident_count() > 0);
    CHECK(lod.last_draw_count() == drawn);
    CHECK(lod.last_draw_count() > 0);

    // THE CONTROL, and it is the whole reason last_draw_count() is the counter
    // worth showing: cull everything. A ring that is CULLED and a ring that is
    // MISSING are different situations, and pending() gives the same answer to
    // both — so any readout built on it cannot tell them apart. The draw count
    // separates them, which is the property being asserted.
    dfn::math::Frustum none{};
    for (dfn::math::Plane& plane : none.planes) {
        plane.normal = {0.0f, 1.0f, 0.0f};
        plane.d = -1.0e9f; // every point sits far OUTSIDE every plane
    }
    CHECK(lod.draw(*renderer, none, dfn::platform::ProgramHandle{1},
                   dfn::platform::TextureHandle{})
          == 0);
    CHECK(lod.last_draw_count() == 0);
    CHECK(lod.pending().empty());       // unchanged — it cannot see the cull
    CHECK(lod.resident_count() > 0);    // the meshes are still there

    lod.destroy_all(*renderer);
    renderer->shutdown();
}
