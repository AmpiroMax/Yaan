/*
Module: tests
File: tests/app/ModelConvertTests.cpp

Responsibility:
- THE DOWNLOAD CACHE AND THE SWITCH. Two claims a frame cannot make: that a
  real downloaded STL becomes a readable .dfo (and the second look costs
  nothing), and that walking the list creates and destroys GPU meshes in
  PAIRS — a leak is invisible in every screenshot ever taken of it.

Key items:
- The STL arm runs on a REAL file from artifacts/3D (Rule 30), not a fixture.
- The leak arm counts NullRenderer::live_meshes() across many switches, with a
  control: the count must actually be non-zero while a model is up, or the
  claim would hold for a viewer that never uploaded anything.

Dependencies:
- Uses: doctest, engine/app ModelConvert, engine/render (RenderSystem,
  ObjectRegistry), the null render backend.
- Used by: ctest (app_model_convert).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE CACHE IS WRITTEN INTO A TEMP ROOT, never into artifacts/3D: a suite that
  fills the tree's own cache changes what the next run of the game reads, and
  the two would then be measuring each other.
*/

#include "engine/app/sources/ModelConvert.h"

#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

/// THE SAMPLE THE ORDER NAMED: artifacts/3D/wizard-*/4-body.stl. Found rather
/// than spelled out in full, because the folder carries the download's own long
/// name and a wave that re-fetches it would rename the folder, not the file.
[[nodiscard]] fs::path find_wizard_body() {
    std::error_code ec;
    const fs::path root("artifacts/3D");
    if (!fs::is_directory(root, ec)) {
        return {};
    }
    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            break;
        }
        if (it->path().filename() == "4-body.stl") {
            return it->path();
        }
    }
    return {};
}

[[nodiscard]] fs::path scratch_root(const char* leaf) {
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec) / "dfn_model_convert" / leaf;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

/// A cube, as three streams' worth of triangles, for the leak arm. Its SIZE is
/// what matters there, not its shape: the arm counts meshes, not pixels.
[[nodiscard]] dfn::render::MeshData box_mesh(float s, std::uint32_t colour) {
    dfn::render::MeshData m;
    const glm::vec3 c[8] = {{0, 0, 0}, {s, 0, 0}, {s, s, 0}, {0, s, 0},
                            {0, 0, s}, {s, 0, s}, {s, s, s}, {0, s, s}};
    const int faces[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                             {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    for (const auto& f : faces) {
        dfn::render::quad(m, c[f[0]], c[f[1]], c[f[2]], c[f[3]], colour);
    }
    return m;
}

} // namespace

TEST_CASE("a real downloaded STL is read into triangles") {
    const fs::path stl = find_wizard_body();
    REQUIRE_MESSAGE(!stl.empty(), "artifacts/3D/wizard-*/4-body.stl not on this tree");

    const auto mesh = dfn::app::read_stl(stl);
    REQUIRE(mesh.has_value());
    CHECK(mesh->triangle_count() > 1000);
    CHECK(mesh->vertices.size() == mesh->triangle_count() * 3);
    CHECK(mesh->indices.size() == mesh->triangle_count() * 3);

    glm::vec3 lo{1e9f};
    glm::vec3 hi{-1e9f};
    for (const dfn::platform::Vertex& v : mesh->vertices) {
        CHECK(std::isfinite(v.position.x));
        CHECK(std::isfinite(v.position.y));
        CHECK(std::isfinite(v.position.z));
        // A NORMAL THAT IS NOT A UNIT VECTOR IS A HOLE IN THE LIT FRAME, and
        // printable STLs are full of zero and denormalised facet normals —
        // which is exactly why the reader recomputes them from the winding
        // instead of trusting the file.
        CHECK(std::fabs(glm::length(v.normal) - 1.0f) < 1e-3f);
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    CHECK(hi.x > lo.x);
    CHECK(hi.y > lo.y);
    CHECK(hi.z > lo.z);
}

TEST_CASE("a garbage file is refused rather than half-read") {
    const fs::path dir = scratch_root("garbage");
    const fs::path bad = dir / "not-a-model.stl";
    {
        std::FILE* f = std::fopen(bad.string().c_str(), "wb");
        REQUIRE(f != nullptr);
        const char junk[] = "this is not an STL and never was";
        std::fwrite(junk, 1, sizeof(junk), f);
        std::fclose(f);
    }
    CHECK_FALSE(dfn::app::read_stl(bad).has_value());

    const auto conv = dfn::app::convert_model(bad, dir, {});
    CHECK(conv.dfo.empty());
    CHECK_FALSE(conv.reason.empty()); // loud, with a sentence
}

TEST_CASE("the conversion caches, and the cached .dfo reads back") {
    const fs::path stl = find_wizard_body();
    REQUIRE_MESSAGE(!stl.empty(), "artifacts/3D/wizard-*/4-body.stl not on this tree");
    const fs::path root = scratch_root("cache");

    const auto first = dfn::app::convert_model(stl, root, {});
    REQUIRE_MESSAGE(!first.dfo.empty(), first.reason);
    CHECK_FALSE(first.from_cache);
    CHECK(first.triangles > 1000);
    CHECK(fs::exists(first.dfo));
    // The cache lives under the downloads root and is hidden by name, because
    // the scan skips it by that name (ModelViewer.cpp).
    CHECK(first.dfo.parent_path() == dfn::app::model_cache_dir(root));

    // SECOND LOOK COSTS A FILE OPEN. That is the whole point of a cache, and
    // "from_cache" is the only way to see it happen.
    const auto second = dfn::app::convert_model(stl, root, {});
    REQUIRE(!second.dfo.empty());
    CHECK(second.from_cache);
    CHECK(second.dfo == first.dfo);

    // And what was cached is a real registry object the game can read.
    const auto obj = dfn::render::read_object(first.dfo);
    REQUIRE(obj.has_value());
    CHECK(obj->kind == "download");
    CHECK(obj->wood.triangle_count() == first.triangles);
    const auto mesh = dfn::app::read_stl(stl);
    REQUIRE(mesh.has_value());
    CHECK(obj->wood.triangle_count() == mesh->triangle_count());

    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST_CASE("the cache key changes when the file does") {
    const fs::path dir = scratch_root("key");
    const fs::path a = dir / "same-name.stl";
    {
        std::FILE* f = std::fopen(a.string().c_str(), "wb");
        REQUIRE(f != nullptr);
        std::vector<char> bytes(200, 0);
        std::fwrite(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
    }
    const std::string k1 = dfn::app::cache_name_for(a);
    {
        std::FILE* f = std::fopen(a.string().c_str(), "wb");
        REQUIRE(f != nullptr);
        std::vector<char> bytes(400, 0);
        std::fwrite(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
    }
    const std::string k2 = dfn::app::cache_name_for(a);
    // A WAVE IS FILLING artifacts/3D WHILE THIS ONE RUNS. A cache keyed by the
    // name alone would show yesterday's figure under today's file for ever,
    // and nothing would say so.
    CHECK(k1 != k2);
    CHECK(k1.rfind("same-name-", 0) == 0);
    CHECK(k2.size() > 4);
    CHECK(k2.compare(k2.size() - 4, 4, ".dfo") == 0);
    // Stable when nothing changed.
    CHECK(dfn::app::cache_name_for(a) == k2);

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE("a baked .dfo is passed through, never copied into the cache") {
    // Two copies of one object under one name is how the two start drifting;
    // the shelves need no conversion at all.
    const fs::path shelf("assets/objects/trees");
    std::error_code ec;
    fs::path any;
    if (fs::is_directory(shelf, ec)) {
        for (const auto& e : fs::directory_iterator(shelf, ec)) {
            if (e.path().extension() == ".dfo") {
                any = e.path();
                break;
            }
        }
    }
    REQUIRE_MESSAGE(!any.empty(), "no .dfo on assets/objects/trees");
    const fs::path root = scratch_root("passthrough");
    const auto conv = dfn::app::convert_model(any, root, {});
    CHECK(conv.dfo == any);
    CHECK(conv.from_cache);
    CHECK_FALSE(fs::exists(dfn::app::model_cache_dir(root)));
    fs::remove_all(root, ec);
}

TEST_CASE("switching models creates and destroys meshes in pairs") {
    dfn::platform::NullRenderer renderer;
    dfn::platform::RendererInitParams params;
    params.framebuffer_width = 320;
    params.framebuffer_height = 180;
    REQUIRE(renderer.init(params));
    CHECK(renderer.live_meshes() == 0);

    dfn::render::RenderSystem rs;
    constexpr glm::ivec2 KEY{2000, 2000}; // the stand's own key (AppViewer.cpp)

    // THE CONTROL FIRST: one model up must actually occupy meshes, or "no
    // growth" below would hold for a viewer that never uploaded anything.
    rs.upload_prebuilt_scatter(renderer, KEY, box_mesh(1.0f, 0xFFFFFFFFu),
                               box_mesh(0.5f, 0xFF00FF00u));
    const std::uint32_t one_model = renderer.live_meshes();
    CHECK(one_model == 2); // one wood stream, one foliage stream

    // NOW WALK THE LIST. Sixty switches, and the count must not creep by one:
    // upload_prebuilt_scatter drops the key before it uploads, which is what
    // makes create and destroy a pair BY CONSTRUCTION rather than by a rule
    // somebody has to remember.
    for (int i = 0; i < 60; ++i) {
        const float s = 0.5f + static_cast<float>(i % 7) * 0.3f;
        rs.upload_prebuilt_scatter(renderer, KEY, box_mesh(s, 0xFFFFFFFFu),
                                   box_mesh(s * 0.5f, 0xFF00FF00u));
        CHECK(renderer.live_meshes() == one_model);
    }

    // A model with no foliage stream (an STL, a kit part) holds ONE mesh, and
    // the switch back to a two-stream model still does not accumulate.
    rs.upload_prebuilt_scatter(renderer, KEY, box_mesh(1.0f, 0xFFFFFFFFu), {});
    CHECK(renderer.live_meshes() == 1);
    rs.upload_prebuilt_scatter(renderer, KEY, box_mesh(1.0f, 0xFFFFFFFFu),
                               box_mesh(0.5f, 0xFF00FF00u));
    CHECK(renderer.live_meshes() == 2);

    // LEAVING THE STAND RETURNS EVERYTHING. This is the arm for the line
    // unload_world() gained: the stand's key is not a chunk coordinate, so
    // nobody else's bookkeeping would ever have dropped it.
    rs.drop_scatter(renderer, KEY);
    CHECK(renderer.live_meshes() == 0);
    // And dropping twice is not a double free.
    rs.drop_scatter(renderer, KEY);
    CHECK(renderer.live_meshes() == 0);

    rs.shutdown(renderer);
    renderer.shutdown();
}
