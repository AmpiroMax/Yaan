/*
Module: tests/render
File: tests/render/GroundTuftsTests.cpp

Responsibility:
- The ground-tuft layer's invariants, and each one is a defect that would ship
  silently otherwise: tufts that move between frames (a shimmer source built
  into the fix for flatness), tufts on rock, tufts on a cliff, tufts identical
  to each other, and tufts drawn past the Rule 33 distance at which they are no
  longer objects.

Dependencies:
- Uses: doctest, engine/render GroundTufts.
- Used by: ctest target render_ground_tufts.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- EVERY CASE HERE HAS A CONTROL ARM (Rule 30). A test that only asserts the
  good case passes on a function that returns a constant.
*/

#include <doctest/doctest.h>

#include "engine/render/sources/GroundTufts.h"

#include <glm/geometric.hpp>

#include <set>
#include <vector>

using namespace dfn;

namespace {

// A flat grass plate: `n` x `n` metres of ground as two triangles per cell,
// every vertex Grass. `tilt` leans the whole plate about the x axis so the
// slope filter can be aimed at it.
struct Plate {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint8_t> materials;
    std::vector<uint32_t> indices;
};

Plate make_plate(int n, math::VoxelMaterial material, float slope = 0.0f) {
    Plate p;
    for (int z = 0; z <= n; ++z) {
        for (int x = 0; x <= n; ++x) {
            p.positions.push_back({static_cast<float>(x),
                                   static_cast<float>(z) * slope,
                                   static_cast<float>(z)});
            p.normals.push_back({0.0f, 1.0f, 0.0f});
            p.materials.push_back(static_cast<uint8_t>(material));
        }
    }
    const auto w = static_cast<uint32_t>(n + 1);
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            const auto i = static_cast<uint32_t>(z) * w + static_cast<uint32_t>(x);
            p.indices.insert(p.indices.end(), {i, i + w, i + 1,
                                               i + 1, i + w, i + w + 1});
        }
    }
    return p;
}

math::VoxelMeshView view_of(const Plate& p) {
    math::VoxelMeshView v;
    v.positions = p.positions;
    v.normals = p.normals;
    v.materials = p.materials;
    v.indices = p.indices;
    return v;
}

render::GroundTuftParams params(float density = 0.5f) {
    render::GroundTuftParams p;
    p.density_per_m2 = density;
    p.view_distance_m = 12.0f;
    p.height_max_m = 0.4f;
    p.slope_max_rad = 0.52f;
    p.seed = 0x67C5u;
    return p;
}

} // namespace

TEST_CASE("tuft density lands near the requested rate, and zero means zero") {
    const Plate plate = make_plate(40, math::VoxelMaterial::Grass);
    const auto spots = render::harvest_tuft_spots(view_of(plate), params(0.5f));
    // 40x40 m at 0.5/m^2. Planting is a per-triangle coin flip, so the count is
    // a binomial draw around 800 -- a generous window, because the point is
    // that the rate is the requested one and not, say, one per triangle (3200)
    // or one per cell (1600), which are the two ways this goes wrong.
    CHECK(spots.size() > 650);
    CHECK(spots.size() < 950);

    // THE CONTROL. Zero density plants nothing; without it the case above
    // passes on any function that returns a plausible number of spots.
    CHECK(render::harvest_tuft_spots(view_of(plate), params(0.0f)).empty());
}

TEST_CASE("the same world plants the same tufts -- twice, exactly") {
    // A tuft that moves between two builds of the same chunk IS shimmer, and it
    // would be shimmer manufactured by the layer whose purpose is to make the
    // ground look better.
    const Plate plate = make_plate(24, math::VoxelMaterial::Grass);
    const auto a = render::harvest_tuft_spots(view_of(plate), params());
    const auto b = render::harvest_tuft_spots(view_of(plate), params());
    REQUIRE(a.size() == b.size());
    REQUIRE(!a.empty());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].position.x == doctest::Approx(b[i].position.x));
        CHECK(a[i].position.z == doctest::Approx(b[i].position.z));
        CHECK(a[i].seed == b[i].seed);
    }
}

TEST_CASE("grass grows on grass and on the blend, and on nothing else") {
    const auto p = params();
    CHECK(!render::harvest_tuft_spots(
              view_of(make_plate(16, math::VoxelMaterial::Grass)), p).empty());
    CHECK(!render::harvest_tuft_spots(
              view_of(make_plate(16, math::VoxelMaterial::GrassRockBlend)), p).empty());
    // The controls: the same plate, the same density, a material the splat
    // draws as something else. A tuft standing on drawn rock is the visual
    // half of gameplay truth disagreeing with the picture (§4).
    CHECK(render::harvest_tuft_spots(
              view_of(make_plate(16, math::VoxelMaterial::Rock)), p).empty());
    CHECK(render::harvest_tuft_spots(
              view_of(make_plate(16, math::VoxelMaterial::Sand)), p).empty());
}

TEST_CASE("SLOPE_GRASS_MAX is honoured, and it is the shader's own threshold") {
    const auto p = params();
    // 0.52 rad is 29.8 deg; tan of that is 0.573. A plate at 0.30 is inside,
    // one at 0.90 (42 deg) is well outside, and the two must disagree.
    CHECK(!render::harvest_tuft_spots(
              view_of(make_plate(16, math::VoxelMaterial::Grass, 0.30f)), p).empty());
    CHECK(render::harvest_tuft_spots(
              view_of(make_plate(16, math::VoxelMaterial::Grass, 0.90f)), p).empty());
}

TEST_CASE("the view distance is a hard cut, in metres, around the eye") {
    const Plate plate = make_plate(60, math::VoxelMaterial::Grass);
    const auto spots = render::harvest_tuft_spots(view_of(plate), params());
    REQUIRE(!spots.empty());
    auto p = params();
    p.view_distance_m = 6.0f;
    const render::MeshData near_mesh =
        render::build_ground_tufts(spots, glm::vec3{30.0f, 0.0f, 30.0f}, p);
    p.view_distance_m = 12.0f;
    const render::MeshData far_mesh =
        render::build_ground_tufts(spots, glm::vec3{30.0f, 0.0f, 30.0f}, p);
    CHECK(!near_mesh.vertices.empty());
    // Four times the area, so several times the geometry. The inequality is the
    // claim; the exact ratio is a binomial draw and is not asserted.
    CHECK(far_mesh.vertices.size() > near_mesh.vertices.size() * 2);

    // Every vertex actually inside the radius, plus the tuft's own height and
    // lean. Nothing may be drawn out past the Rule 33 cut.
    const glm::vec3 eye{30.0f, 0.0f, 30.0f};
    for (const platform::Vertex& v : far_mesh.vertices) {
        const glm::vec2 d{v.position.x - eye.x, v.position.z - eye.z};
        CHECK(glm::length(d) < p.view_distance_m + p.height_max_m);
    }

    // THE CONTROL: an eye far away from the plate grows nothing at all.
    CHECK(render::build_ground_tufts(spots, glm::vec3{500.0f, 0.0f, 500.0f}, p)
              .vertices.empty());
}

TEST_CASE("tufts are not one clump in copies -- several shapes and several tones") {
    // The user asked for «мелкую РАЗНУЮ траву», and a stamp repeated is the
    // defect the ground colour was just fixed for. Both axes are checked,
    // because a set of four shapes all in one colour would pass a shape-only
    // test and still read as one plant.
    const Plate plate = make_plate(40, math::VoxelMaterial::Grass);
    const auto spots = render::harvest_tuft_spots(view_of(plate), params());
    REQUIRE(spots.size() > 100);
    std::set<uint32_t> tones;
    std::set<size_t> sizes;
    for (size_t i = 0; i < 60; ++i) {
        const render::MeshData one = render::build_ground_tufts(
            std::span<const render::TuftSpot>(&spots[i], 1),
            spots[i].position, params());
        REQUIRE(!one.vertices.empty());
        sizes.insert(one.vertices.size()); // blade count -> vertex count
        for (const platform::Vertex& v : one.vertices) {
            tones.insert(v.color_rgba);
        }
    }
    CHECK(sizes.size() >= 3);  // at least three of the four blade counts appear
    CHECK(tones.size() >= 6);  // four tones x root/tip, so >= 6 distinct colours
}
