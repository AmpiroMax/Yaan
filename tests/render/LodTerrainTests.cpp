/*
Created: 09:08:2026 - 22:12:57
Last updated: 09:08:2026 - 22:12:57
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
/*
UPD:
- 09:08:2026 - 22:12:57: Created with the LOD drawing half.
*/

#include "engine/render/sources/LodTerrain.h"

#include "engine/platform/render/sources/null/CreateNullRenderer.h"
#include "engine/render/sources/TerrainLod.h"

#include <doctest/doctest.h>

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
