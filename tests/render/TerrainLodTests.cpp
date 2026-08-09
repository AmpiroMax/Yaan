/*
Created: 09:08:2026 - 20:58:00
Last updated: 09:08:2026 - 22:12:57
Module: tests
File: tests/render/TerrainLodTests.cpp

Responsibility:
- Terrain LOD policy tests: the ladder's derived numbers, quadtree selection
  (finer near the eye, coarse far away, gap-free), the silhouette band, and the
  two-level fade window that keeps a level swap from popping.

Key items:
- doctest cases over engine/render TerrainLod (pure, GPU-free).

Dependencies:
- Uses: doctest, engine/render TerrainLod.
- Used by: ctest (render_terrain_lod).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep GPU-free; this suite is the reason LOD policy can be changed safely.
*/
/*
UPD:
- 09:08:2026 - 20:58:00: Created with the LOD selection module.
- 09:08:2026 - 22:12:57: Resident-rectangle carve-out (with the control that a
  rectangle-blind selection fails) and the skirt-depth derivation.
  TIMESTAMP CORRECTION (lead's catch, Rule 16): this entry and the file's
  'Last updated' were first written as 22:40:00 — a time COMPUTED forward from
  an earlier `date` call rather than read from one. Corrected to the real time
  of the edit, along with every other stamp in this changeset that had the same
  origin. The reason it matters is not tidiness: UPD stamps are the only
  cross-zone ordering record this project has, and a stamp half an hour early
  reorders history while looking exactly like a correct one.
*/

#include "engine/render/sources/TerrainLod.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>

using dfn::render::LodNode;
using dfn::render::LodResidency;

namespace {

// Highest level (coarsest) present in a selection.
uint8_t max_level(const std::vector<LodNode>& nodes) {
    uint8_t m = 0;
    for (const LodNode& n : nodes) {
        m = std::max(m, n.level);
    }
    return m;
}

bool contains(const std::vector<LodNode>& nodes, const LodNode& node) {
    return std::find(nodes.begin(), nodes.end(), node) != nodes.end();
}

} // namespace

TEST_CASE("the ladder's numbers are the derived ones, not tuned ones") {
    // A node is a constant triangle budget: same voxel count at every level.
    CHECK(dfn::render::lod_node_size_m(0) == doctest::Approx(128.0f));
    CHECK(dfn::render::lod_node_size_m(5) == doctest::Approx(8192.0f));
    // 110 metres of viewing distance per metre of edge (2.5 px per triangle at
    // 275 px per radian). A 1 m voxel is good enough from 110 m.
    CHECK(dfn::render::lod_split_distance_m(0) == doctest::Approx(110.0f));
    CHECK(dfn::render::lod_split_distance_m(1) == doctest::Approx(440.0f));
    CHECK(dfn::render::lod_split_distance_m(5) == doctest::Approx(7040.0f));
    // Every level is coarser than the one below it — a ladder that ever
    // repeats a size would make a level unreachable.
    for (uint32_t l = 1; l < dfn::render::LOD_LEVEL_COUNT; ++l) {
        CHECK(dfn::render::LOD_VOXEL_SIZE_M[l] > dfn::render::LOD_VOXEL_SIZE_M[l - 1]);
    }
}

TEST_CASE("footprint distance is zero inside a node and grows outside it") {
    const LodNode node{0, 1, 1}; // 128..256 m on both axes
    CHECK(dfn::render::lod_node_distance_m(node, {200.0f, 50.0f, 200.0f})
          == doctest::Approx(0.0f));
    CHECK(dfn::render::lod_node_distance_m(node, {128.0f - 30.0f, 0.0f, 200.0f})
          == doctest::Approx(30.0f));
    // Height is deliberately ignored: standing 500 m above the node is still
    // "inside" its footprint, which errs toward MORE detail, never less.
    CHECK(dfn::render::lod_node_distance_m(node, {200.0f, 500.0f, 200.0f})
          == doctest::Approx(0.0f));
}

TEST_CASE("selection is finest at the eye and coarsens with distance") {
    const glm::vec2 lo{0.0f, 0.0f};
    const glm::vec2 hi{2048.0f, 2048.0f}; // today's 2x2 km world
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};
    const auto nodes = dfn::render::select_lod_nodes(eye, lo, hi);
    REQUIRE_FALSE(nodes.empty());

    // The node under the eye is level 0: the eye is inside it, distance 0,
    // which is below every split distance.
    const auto under = std::find_if(nodes.begin(), nodes.end(), [&](const LodNode& n) {
        return dfn::render::lod_node_distance_m(n, eye) == 0.0f;
    });
    REQUIRE(under != nodes.end());
    CHECK(under->level == 0);

    // Every selected node satisfies its own rule: either it is the finest
    // level, or the eye is beyond its split distance. That invariant IS the
    // screen-error guarantee.
    for (const LodNode& n : nodes) {
        const bool ok = n.level == 0
                     || dfn::render::lod_node_distance_m(n, eye)
                            >= dfn::render::lod_split_distance_m(n.level);
        CHECK(ok);
    }

    // Detail actually falls off: the farthest node is coarser than the nearest.
    uint8_t level_near = 255;
    uint8_t level_far = 0;
    float far_dist = -1.0f;
    for (const LodNode& n : nodes) {
        const float d = dfn::render::lod_node_distance_m(n, eye);
        if (d == 0.0f) {
            level_near = std::min(level_near, n.level);
        }
        if (d > far_dist) {
            far_dist = d;
            level_far = n.level;
        }
    }
    CHECK(level_near == 0);
    CHECK(level_far > level_near);
}

TEST_CASE("selection covers the world without holes or overlaps") {
    const glm::vec2 lo{0.0f, 0.0f};
    const glm::vec2 hi{2048.0f, 2048.0f};
    const glm::vec3 eye{300.0f, 20.0f, 300.0f};
    const auto nodes = dfn::render::select_lod_nodes(eye, lo, hi);

    // Sample the world on a coarse grid; every sample must land in EXACTLY one
    // selected node. One node = a hole is impossible, exactly one = no z-fight
    // between two levels of the same ground.
    for (float z = 16.0f; z < 2048.0f; z += 128.0f) {
        for (float x = 16.0f; x < 2048.0f; x += 128.0f) {
            int hits = 0;
            for (const LodNode& n : nodes) {
                const float s = dfn::render::lod_node_size_m(n.level);
                const float nx = static_cast<float>(n.x) * s;
                const float nz = static_cast<float>(n.z) * s;
                if (x >= nx && x < nx + s && z >= nz && z < nz + s) {
                    ++hits;
                }
            }
            CHECK(hits == 1);
        }
    }
}

TEST_CASE("a 10x10 km world stays bounded and roots on a fixed grid") {
    const glm::vec3 eye{5000.0f, 30.0f, 5000.0f};
    const auto nodes =
        dfn::render::select_lod_nodes(eye, {0.0f, 0.0f}, {10000.0f, 10000.0f});
    REQUIRE_FALSE(nodes.empty());
    CHECK(nodes.size() < 4096); // the cap is a safety net, not the policy
    // The coarsest levels are not reachable inside a 10 km world and that is
    // CORRECT, not a gap: level 5 only survives beyond 7040 m of open ground,
    // which a world this size cannot offer from its own centre. The ladder is
    // sized for the far mountain at CAMERA_FAR, not for the playable extent.
    CHECK(max_level(nodes) >= 3);
    CHECK(max_level(nodes) <= dfn::render::LOD_LEVEL_COUNT - 1);

    // Node ids do not depend on world size: the same ground at the same level
    // keeps its id when the world grows, so core's cached nodes stay valid.
    const auto small =
        dfn::render::select_lod_nodes({500.0f, 30.0f, 500.0f}, {0.0f, 0.0f},
                                      {2048.0f, 2048.0f});
    const auto large =
        dfn::render::select_lod_nodes({500.0f, 30.0f, 500.0f}, {0.0f, 0.0f},
                                      {10000.0f, 10000.0f});
    const auto near_node = std::find_if(small.begin(), small.end(),
                                        [](const LodNode& n) { return n.level == 0; });
    REQUIRE(near_node != small.end());
    CHECK(contains(large, *near_node));
}

TEST_CASE("the silhouette band starts where design says it does") {
    const glm::vec3 eye{0.0f, 0.0f, 0.0f};
    CHECK_FALSE(dfn::render::lod_node_is_silhouette({0, 0, 0}, eye));   // under the eye
    CHECK_FALSE(dfn::render::lod_node_is_silhouette({0, 5, 0}, eye));   // 640 m out
    CHECK(dfn::render::lod_node_is_silhouette({0, 8, 0}, eye));         // 1024 m out
}

TEST_CASE("a level swap draws BOTH levels until the new one is fully in") {
    LodResidency res;
    const std::vector<LodNode> coarse{{1, 0, 0}};
    const std::vector<LodNode> fine{{0, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}};

    res.update(coarse, 0.0f);
    REQUIRE(res.to_load().size() == 1);
    CHECK(res.to_draw().empty()); // requested but not uploaded yet: nothing drawn
    res.mark_resident(coarse[0]);
    res.update(coarse, dfn::render::LOD_FADE_SECONDS);
    REQUIRE(res.to_draw().size() == 1);
    CHECK(res.to_draw()[0].fade == doctest::Approx(1.0f));

    // Walk closer: the four fine children are requested, and while they arrive
    // the coarse node is STILL drawn. Releasing it now is what a naive streamer
    // does, and it is a guaranteed hole in the ground.
    res.update(fine, 0.016f);
    CHECK(res.to_load().size() == 4);
    CHECK(res.to_release().empty());
    REQUIRE(res.to_draw().size() == 1);
    CHECK(res.to_draw()[0].node == coarse[0]);
    CHECK(res.to_draw()[0].fade < 1.0f);

    for (const LodNode& n : fine) {
        res.mark_resident(n);
    }
    // Half a fade later both levels are on screen — that overlap IS the
    // cross-fade window, and it only exists because core agreed a node may be
    // resident at two levels at once.
    res.update(fine, dfn::render::LOD_FADE_SECONDS * 0.4f);
    CHECK(res.to_draw().size() == 5);
    CHECK(res.to_release().empty());

    // Once the fade completes the coarse node is released — exactly once.
    res.update(fine, dfn::render::LOD_FADE_SECONDS);
    REQUIRE(res.to_release().size() == 1);
    CHECK(res.to_release()[0] == coarse[0]);
    CHECK(res.to_draw().size() == 4);
    for (const auto& d : res.to_draw()) {
        CHECK(d.fade == doctest::Approx(1.0f));
    }
    res.update(fine, 0.016f);
    CHECK(res.to_release().empty()); // no double release
    CHECK(res.resident_count() == 4);
}

TEST_CASE("re-selecting a node mid-fade-out keeps it and fades it back in") {
    LodResidency res;
    const std::vector<LodNode> a{{1, 0, 0}};
    res.update(a, 0.0f);
    res.mark_resident(a[0]);
    res.update(a, dfn::render::LOD_FADE_SECONDS);
    REQUIRE(res.to_draw()[0].fade == doctest::Approx(1.0f));

    res.update({}, dfn::render::LOD_FADE_SECONDS * 0.5f); // deselected, fading
    CHECK(res.to_draw().size() == 1);
    CHECK(res.to_release().empty());

    // Turning on the spot must not cost a reload: the mesh never left.
    res.update(a, dfn::render::LOD_FADE_SECONDS * 0.5f);
    CHECK(res.to_load().empty());
    CHECK(res.is_resident(a[0]));
    CHECK(res.to_draw()[0].fade == doctest::Approx(1.0f));
}

TEST_CASE("the selection's triangle budget is known, not hoped for") {
    // MEASURED, so a successor inherits the number instead of re-deriving it:
    // 2x2 km world, eye at the centre -> 76 nodes, 64 of them level 0.
    // 10x10 km -> 184 nodes, 128 at level 0. Level 0 covers 0..440 m because
    // the ladder's first jump is 1 -> 4 m: nothing between those is available,
    // so the fine level has to reach the coarse level's competence distance.
    // At ~33k triangles per node that is ~2.1M triangles of terrain before
    // frustum culling, which is the NEXT lever (a 75-degree frustum keeps
    // roughly a third of them) and is not built yet.
    const auto small =
        dfn::render::select_lod_nodes({1024.0f, 20.0f, 1024.0f}, {0.0f, 0.0f},
                                      {2048.0f, 2048.0f});
    const auto large =
        dfn::render::select_lod_nodes({5000.0f, 30.0f, 5000.0f}, {0.0f, 0.0f},
                                      {10000.0f, 10000.0f});
    CHECK(small.size() == 76);
    CHECK(large.size() == 184);
    // The world growing 25x in area must not grow the draw list anywhere near
    // as fast — that is the entire point of the ladder.
    CHECK(large.size() < small.size() * 4);
}

// --- The resident rectangle: where chunk streaming ends and LOD begins ------

namespace {

// World rectangle a node occupies, for overlap checks.
struct NodeRect {
    float x0, z0, x1, z1;
};
NodeRect rect_of(const LodNode& n) {
    const float s = dfn::render::lod_node_size_m(n.level);
    const float x0 = static_cast<float>(n.x) * s;
    const float z0 = static_cast<float>(n.z) * s;
    return {x0, z0, x0 + s, z0 + s};
}
bool overlaps(const NodeRect& a, const dfn::render::LodRect& b) {
    return a.x0 < b.max.x && a.x1 > b.min.x && a.z0 < b.max.y && a.z1 > b.min.y;
}
bool covers(const LodNode& n, float x, float z) {
    const NodeRect r = rect_of(n);
    return x >= r.x0 && x < r.x1 && z >= r.z0 && z < r.z1;
}

} // namespace

TEST_CASE("the streamed rectangle is carved out of the selection") {
    const glm::vec2 lo{0.0f, 0.0f};
    const glm::vec2 hi{2048.0f, 2048.0f};
    const glm::vec3 eye{1024.0f, 20.0f, 1024.0f};
    // What core streams today: Chebyshev CHUNK_LOAD_RADIUS around the focus
    // chunk, i.e. a chunk-aligned square. 768..1280 is the 2-chunk ring around
    // the chunk containing the eye.
    const dfn::render::LodRect resident{{768.0f, 768.0f}, {1280.0f, 1280.0f}};

    const auto blind = dfn::render::select_lod_nodes(eye, lo, hi);
    const auto carved = dfn::render::select_lod_nodes(eye, lo, hi, resident);

    // THE CONTROL. The node the eye stands in is selected when nothing is
    // streamed — and MUST NOT be selected once chunks own that ground, because
    // a level-0 node is 1 m voxels where the chunk heightfield is 2 m and the
    // two surfaces would interleave per pixel. A selection that ignored the
    // rectangle passes every other case in this file; this is the one that
    // rejects it.
    const auto under = std::find_if(blind.begin(), blind.end(), [&](const LodNode& n) {
        return covers(n, eye.x, eye.z);
    });
    REQUIRE(under != blind.end());
    CHECK(contains(blind, *under));
    CHECK_FALSE(contains(carved, *under));

    // No selected node overlaps the rectangle AT ALL — not merely "mostly".
    for (const LodNode& n : carved) {
        CHECK_FALSE(overlaps(rect_of(n), resident));
    }

    // And nothing outside the rectangle is lost in the process: every sample
    // outside it is still covered by exactly one node. A carve-out that leaves
    // a ring of missing ground is worse than the overlap it fixed.
    for (float z = 16.0f; z < 2048.0f; z += 128.0f) {
        for (float x = 16.0f; x < 2048.0f; x += 128.0f) {
            const bool inside = x >= resident.min.x && x < resident.max.x
                             && z >= resident.min.y && z < resident.max.y;
            if (inside) {
                continue;
            }
            int hits = 0;
            for (const LodNode& n : carved) {
                if (covers(n, x, z)) {
                    ++hits;
                }
            }
            CHECK(hits == 1);
        }
    }
}

TEST_CASE("a node straddling the streamed border splits instead of overlapping") {
    // The rectangle sits in the world corner and the eye is 2 km away, so the
    // screen-error rule alone would put a single COARSE node over that whole
    // corner — and that node would redraw ground the chunk ring already owns.
    const glm::vec2 lo{0.0f, 0.0f};
    const glm::vec2 hi{2048.0f, 2048.0f};
    const glm::vec3 eye{2000.0f, 20.0f, 2000.0f};
    const dfn::render::LodRect resident{{0.0f, 0.0f}, {256.0f, 256.0f}};

    const auto blind = dfn::render::select_lod_nodes(eye, lo, hi);
    const auto carved = dfn::render::select_lod_nodes(eye, lo, hi, resident);

    // CONTROL: without the rectangle the corner is one coarse node...
    const auto coarse_corner =
        std::find_if(blind.begin(), blind.end(), [](const LodNode& n) {
            return n.level > 0 && covers(n, 10.0f, 10.0f);
        });
    REQUIRE(coarse_corner != blind.end());
    // ...and with it, that node is gone: it straddled the border, so it was
    // split rather than accepted. Accepting it is the failure this rejects.
    CHECK_FALSE(contains(carved, *coarse_corner));

    // The split runs all the way to level 0, which is the level at which
    // inside/outside is exact (128 m nodes against a 256 m chunk grid).
    // 256..384 m is outside the rectangle and must be a level-0 node.
    CHECK(contains(carved, LodNode{0, 2, 0}));
    // 0..128 m is inside it and must not exist at any level.
    for (const LodNode& n : carved) {
        CHECK_FALSE(covers(n, 64.0f, 64.0f));
    }
}

TEST_CASE("skirt depth is measured from the ground, not picked") {
    // Flat ground: no measured step, so the floor applies — one voxel of the
    // node's own level. A zero skirt still shows a hairline where two lattices
    // meet, which is why the floor is not zero.
    CHECK(dfn::render::lod_skirt_depth_m(0, 0.0f) == doctest::Approx(1.0f));
    CHECK(dfn::render::lod_skirt_depth_m(5, 0.0f) == doctest::Approx(64.0f));

    // A cliff edge: the measurement dominates, scaled by the ladder's largest
    // jump (1 -> 4 m), because the neighbour that disagrees may be four times
    // coarser.
    CHECK(dfn::render::lod_skirt_depth_m(0, 12.0f) == doctest::Approx(48.0f));

    // CONTROL — the thing this rejects is a CONSTANT skirt. No single value
    // can satisfy both of these: 1 m on flat level-0 ground is right and would
    // be a 12-fold hole under a cliff; 48 m would be right at the cliff and is
    // absurd everywhere else. The two cases must disagree by more than an
    // order of magnitude, and they do.
    CHECK(dfn::render::lod_skirt_depth_m(0, 12.0f)
          > dfn::render::lod_skirt_depth_m(0, 0.0f) * 10.0f);
    // Negative or nonsense measurements never produce a negative skirt (a
    // skirt that pointed UP would be visible geometry floating in the air).
    CHECK(dfn::render::lod_skirt_depth_m(2, -5.0f) > 0.0f);
}
