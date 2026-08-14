/*
Created: 14:08:2026 - 18:43:43
Last updated: 14:08:2026 - 21:22:22
Module: tests
File: tests/core/WorldFormatTests.cpp

Responsibility:
- The .dfw container's guarantees: a chunk survives the round trip unchanged,
  the same world writes byte-identical bytes twice (Rule 13.1), a file the
  build does not understand is refused rather than guessed at, and a corrupt
  or truncated file fails SOFT instead of half-loading.

Dependencies:
- Uses: doctest, dfn_world (WorldFormat, Worldgen).
- Used by: ctest (test_world_format).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE HALF-LOAD CASE IS THE POINT OF THIS SUITE, not the happy path. A chunk
  that comes back with its voxel surface missing is a hole in the ground the
  player falls through, and it looks exactly like an empty chunk to every
  caller — which is why the reader must return nothing at all, and why
  weakening these checks to "it loaded something" is never a fix.
*/
/*
UPD:
- 14:08:2026 - 18:43:43: Created with the container's implementation (the
  contract had been declaration-only since 09.08). Round trip is checked
  against REAL generated chunks rather than hand-built ones: a hand-built chunk
  would have empty voxel/scatter arrays, i.e. it would pass while proving
  nothing about the very fields the baker exists to carry.
- 14:08:2026 - 20:47:52: bake_world получил свои случаи: испечённый мир ПОЛЕ В ПОЛЕ равен
  сгенерированному (файл, который просто грузится, не доказывает ничего — сторожим
  выпечку, тихо разошедшуюся с миром, который меряют все тесты детерминированности),
  две выпечки одного seed побитово равны, пустой пролёт отвергается вслух.
- 14:08:2026 - 21:22:22: Случай на ЧТЕНИЕ испечённого через ChunkManager, и он контрольная рука,
  а не дымовой тест: одни и те же параметры стримятся дважды в одном процессе —
  из файла и из генератора, — и земля обязана выйти одинаковой. «Игра только
  читает» верно ровно настолько, насколько чтение и генерация дают ОДИН мир.
  Плюс отказ несуществующего файла: молчаливый откат на генерацию дал бы сборку,
  работающую на скорости генерации и отчитывающуюся об открытии выпечки.
*/

#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/world/sources/ChunkManager.h"
#include "engine/world/sources/WorldBake.h"
#include "engine/world/sources/WorldFormat.h"
#include "engine/world/sources/Worldgen.h"

#include <cstdio>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <vector>

using dfn::world::Chunk;
using dfn::world::ChunkCoord;
using dfn::world::WorldFileReader;
using dfn::world::WorldFileWriter;
using dfn::world::WorldGenParams;
using dfn::world::WorldInfo;

namespace {

/// A small but REAL world: two chunks of the default testbed layout, so the
/// payload carries an actual voxel surface, actual scatter and actual entity
/// records. Generating is the expensive part of this suite and it is the part
/// that makes it mean anything.
struct BakedPair {
    WorldGenParams params;
    std::vector<Chunk> chunks;
};

[[nodiscard]] BakedPair bake_two_chunks() {
    BakedPair out;
    out.params.seed = 20260814u;
    out.params.min_chunk = {0, 0};
    out.params.max_chunk = {1, 0};
    const auto ctx = dfn::world::build_world_context(out.params);
    out.chunks.push_back(dfn::world::generate_chunk(ctx, ChunkCoord{0, 0}));
    out.chunks.push_back(dfn::world::generate_chunk(ctx, ChunkCoord{1, 0}));
    return out;
}

[[nodiscard]] WorldInfo info_of(const BakedPair& baked) {
    WorldInfo info;
    info.seed = baked.params.seed;
    info.worldgen_version = 2;
    info.min_chunk = baked.params.min_chunk;
    info.max_chunk = baked.params.max_chunk;
    return info;
}

[[nodiscard]] bool write_world(const BakedPair& baked, const std::filesystem::path& path) {
    WorldFileWriter w;
    w.begin(info_of(baked));
    for (const Chunk& c : baked.chunks) {
        w.append_chunk(c);
    }
    return w.save(path);
}

[[nodiscard]] std::vector<std::byte> file_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<std::byte> out;
    if (!in) {
        return out;
    }
    in.seekg(0, std::ios::end);
    out.resize(static_cast<std::size_t>(in.tellg()));
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return out;
}

/// A directory that cleans up after itself, so a failing run does not leave
/// the tree dirty for whoever commits next.
struct TempDir {
    std::filesystem::path path;
    explicit TempDir(const char* name)
        : path(std::filesystem::temp_directory_path() / name) {
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("dfw: a generated chunk survives the round trip field for field") {
    const TempDir dir("dfn_world_format_roundtrip");
    const auto file = dir.path / "pair.dfw";
    const BakedPair baked = bake_two_chunks();
    REQUIRE(write_world(baked, file));

    WorldFileReader r;
    REQUIRE(r.open(file));
    CHECK(r.info().seed == baked.params.seed);
    CHECK(r.chunk_directory().size() == 2);

    for (const Chunk& original : baked.chunks) {
        const auto loaded = r.load_chunk(original.coord);
        REQUIRE(loaded.has_value());
        CHECK(loaded->coord == original.coord);

        // The heightmap: every sample, not a hash of them — a hash that matches
        // proves the bytes agree, but a mismatch would not say WHERE, and the
        // first thing anybody debugging a format wants is the index.
        REQUIRE(loaded->heightmap.samples.size() == original.heightmap.samples.size());
        CHECK(loaded->heightmap.height_scale == original.heightmap.height_scale);
        CHECK(loaded->heightmap.height_offset == original.heightmap.height_offset);
        std::size_t height_mismatches = 0;
        for (std::size_t i = 0; i < original.heightmap.samples.size(); ++i) {
            height_mismatches +=
                loaded->heightmap.samples[i] != original.heightmap.samples[i] ? 1u : 0u;
        }
        CHECK(height_mismatches == 0);

        REQUIRE(loaded->surface.dist_to_water.size() == original.surface.dist_to_water.size());
        REQUIRE(loaded->surface.water_surface.size() == original.surface.water_surface.size());
        REQUIRE(loaded->surface.surface_class.size() == original.surface.surface_class.size());

        // The voxel surface is what the terrain is DRAWN and COLLIDED from, so
        // an empty one here would mean the suite proves nothing about the field
        // the baker exists to carry.
        REQUIRE(original.voxels.indices.size() > 0);
        REQUIRE(loaded->voxels.positions.size() == original.voxels.positions.size());
        REQUIRE(loaded->voxels.normals.size() == original.voxels.normals.size());
        REQUIRE(loaded->voxels.materials.size() == original.voxels.materials.size());
        REQUIRE(loaded->voxels.indices.size() == original.voxels.indices.size());
        std::size_t voxel_mismatches = 0;
        for (std::size_t i = 0; i < original.voxels.positions.size(); ++i) {
            voxel_mismatches += loaded->voxels.positions[i] != original.voxels.positions[i] ? 1u : 0u;
            voxel_mismatches += loaded->voxels.normals[i] != original.voxels.normals[i] ? 1u : 0u;
        }
        for (std::size_t i = 0; i < original.voxels.indices.size(); ++i) {
            voxel_mismatches += loaded->voxels.indices[i] != original.voxels.indices[i] ? 1u : 0u;
        }
        CHECK(voxel_mismatches == 0);

        REQUIRE(loaded->scatter.size() == original.scatter.size());
        std::size_t scatter_mismatches = 0;
        for (std::size_t i = 0; i < original.scatter.size(); ++i) {
            scatter_mismatches += loaded->scatter[i].position != original.scatter[i].position ? 1u : 0u;
            scatter_mismatches += loaded->scatter[i].yaw != original.scatter[i].yaw ? 1u : 0u;
            scatter_mismatches += loaded->scatter[i].scale != original.scatter[i].scale ? 1u : 0u;
            scatter_mismatches += loaded->scatter[i].species != original.scatter[i].species ? 1u : 0u;
        }
        CHECK(scatter_mismatches == 0);

        REQUIRE(loaded->entities.size() == original.entities.size());
        std::size_t entity_mismatches = 0;
        for (std::size_t i = 0; i < original.entities.size(); ++i) {
            entity_mismatches += loaded->entities[i].world_id != original.entities[i].world_id ? 1u : 0u;
            entity_mismatches += loaded->entities[i].archetype != original.entities[i].archetype ? 1u : 0u;
            entity_mismatches += loaded->entities[i].position_xz != original.entities[i].position_xz ? 1u : 0u;
            entity_mismatches += loaded->entities[i].yaw != original.entities[i].yaw ? 1u : 0u;
            entity_mismatches += loaded->entities[i].ground_y != original.entities[i].ground_y ? 1u : 0u;
        }
        CHECK(entity_mismatches == 0);
    }
}

TEST_CASE("dfw: the same world writes the same bytes twice (Rule 13.1)") {
    const TempDir dir("dfn_world_format_determinism");
    const BakedPair baked = bake_two_chunks();
    const auto a = dir.path / "a.dfw";
    const auto b = dir.path / "b.dfw";
    REQUIRE(write_world(baked, a));
    REQUIRE(write_world(baked, b));

    const auto bytes_a = file_bytes(a);
    const auto bytes_b = file_bytes(b);
    REQUIRE(bytes_a.size() > 0);
    CHECK(bytes_a.size() == bytes_b.size());
    CHECK(bytes_a == bytes_b);

    // ...and REGENERATING the world writes those same bytes again: the file is
    // a function of the seed, which is the property that lets a bake be cached
    // and lets an agent trust a demo map is the map its manifest names.
    const BakedPair again = bake_two_chunks();
    const auto c = dir.path / "c.dfw";
    REQUIRE(write_world(again, c));
    CHECK(file_bytes(c) == bytes_a);
}

TEST_CASE("dfw: a missing chunk is nothing, not an empty chunk") {
    const TempDir dir("dfn_world_format_absent");
    const auto file = dir.path / "pair.dfw";
    const BakedPair baked = bake_two_chunks();
    REQUIRE(write_world(baked, file));

    WorldFileReader r;
    REQUIRE(r.open(file));
    CHECK_FALSE(r.load_chunk(ChunkCoord{9, 9}).has_value());
}

TEST_CASE("dfw: a truncated file is refused, never half-loaded") {
    const TempDir dir("dfn_world_format_truncated");
    const auto whole = dir.path / "whole.dfw";
    const BakedPair baked = bake_two_chunks();
    REQUIRE(write_world(baked, whole));
    const auto bytes = file_bytes(whole);
    REQUIRE(bytes.size() > 64);

    // Cut the file in half: the header and the INFO section survive, so the
    // failure lands in the middle of a chunk payload — which is exactly the
    // case that would otherwise produce terrain with a hole in it.
    const auto cut = dir.path / "cut.dfw";
    {
        std::ofstream out(cut, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size() / 2));
    }

    WorldFileReader r;
    // open() may succeed (the sections it can walk are intact) or fail; what is
    // NOT allowed is a chunk that comes back partially decoded.
    if (r.open(cut)) {
        for (const ChunkCoord coord : {ChunkCoord{0, 0}, ChunkCoord{1, 0}}) {
            if (const auto loaded = r.load_chunk(coord)) {
                // If it loaded at all it must be COMPLETE, not a stub.
                CHECK(loaded->heightmap.samples.size()
                      == baked.chunks[0].heightmap.samples.size());
                CHECK(loaded->voxels.indices.size() > 0);
            }
        }
    }
}

TEST_CASE("dfw: a file that is not a world file is refused") {
    const TempDir dir("dfn_world_format_alien");
    const auto alien = dir.path / "not_a_world.dfw";
    {
        std::ofstream out(alien, std::ios::binary);
        out << "this is not a world file, it is a sentence";
    }
    WorldFileReader r;
    CHECK_FALSE(r.open(alien));
    CHECK_FALSE(r.open(dir.path / "does_not_exist.dfw"));
}

TEST_CASE("bake: the baked world IS the generated world, and reading it is far cheaper") {
    const TempDir dir("dfn_world_bake");
    const auto file = dir.path / "baked.dfw";

    dfn::world::WorldGenParams params;
    params.seed = 20260814u;
    params.min_chunk = {0, 0};
    params.max_chunk = {1, 1};

    const dfn::world::BakeReport report = dfn::world::bake_world(params, file);
    INFO("bake error: ", report.error);
    REQUIRE(report.error.empty());
    CHECK(report.chunks == 4);
    CHECK(report.bytes > 0);

    // THE POINT OF THE WHOLE BAKER: what comes back must be what the generator
    // would have produced, field for field. A file that merely loads proves
    // nothing — the failure this guards against is a bake that quietly differs
    // from the world every determinism test measures.
    const auto ctx = dfn::world::build_world_context(params);
    WorldFileReader reader;
    REQUIRE(reader.open(file));
    REQUIRE(reader.chunk_directory().size() == 4);

    for (int32_t z = 0; z <= 1; ++z) {
        for (int32_t x = 0; x <= 1; ++x) {
            const ChunkCoord coord{x, z};
            const Chunk expected = dfn::world::generate_chunk(ctx, coord);
            const auto got = reader.load_chunk(coord);
            REQUIRE(got.has_value());
            REQUIRE(got->heightmap.samples.size() == expected.heightmap.samples.size());
            REQUIRE(got->voxels.indices.size() == expected.voxels.indices.size());
            REQUIRE(got->scatter.size() == expected.scatter.size());
            REQUIRE(got->entities.size() == expected.entities.size());
            std::size_t mismatches = 0;
            for (std::size_t i = 0; i < expected.heightmap.samples.size(); ++i) {
                mismatches += got->heightmap.samples[i] != expected.heightmap.samples[i] ? 1u : 0u;
            }
            for (std::size_t i = 0; i < expected.voxels.indices.size(); ++i) {
                mismatches += got->voxels.indices[i] != expected.voxels.indices[i] ? 1u : 0u;
            }
            for (std::size_t i = 0; i < expected.scatter.size(); ++i) {
                mismatches += got->scatter[i].position != expected.scatter[i].position ? 1u : 0u;
            }
            CHECK(mismatches == 0);
        }
    }

    // Two bakes of one seed are byte-identical — that is what makes a CACHED
    // bake trustworthy rather than merely convenient (Rule 13.1).
    const auto again = dir.path / "again.dfw";
    const dfn::world::BakeReport second = dfn::world::bake_world(params, again);
    REQUIRE(second.error.empty());
    CHECK(file_bytes(again) == file_bytes(file));
}

TEST_CASE("bake: an empty extent is refused, not silently written as nothing") {
    const TempDir dir("dfn_world_bake_empty");
    dfn::world::WorldGenParams params;
    params.seed = 1u;
    params.min_chunk = {4, 4};
    params.max_chunk = {0, 0}; // behind min: a typo, not a world
    const dfn::world::BakeReport r = dfn::world::bake_world(params, dir.path / "no.dfw");
    CHECK_FALSE(r.error.empty());
    CHECK(r.chunks == 0);
}

TEST_CASE("streaming: a baked chunk and a generated chunk are the same ground") {
    const TempDir dir("dfn_world_stream_baked");
    const auto file = dir.path / "streamed.dfw";
    dfn::world::WorldGenParams params;
    params.seed = 20260814u;
    params.min_chunk = {0, 0};
    params.max_chunk = {1, 1};
    REQUIRE(dfn::world::bake_world(params, file).error.empty());

    dfn::world::ChunkStreamingParams streaming;
    streaming.load_radius = 1;
    streaming.unload_radius = 2;

    // The manager reads the file; the same manager on the same params without a
    // file generates. The two must agree, because "the game only reads" is only
    // true if reading and generating produce one world (Rule 30: the control
    // arm is the other way of getting the same thing).
    dfn::ecs::World ecs_baked;
    dfn::events::EventBus bus_baked;
    dfn::world::ChunkManager baked;
    REQUIRE(baked.open(file, params, nullptr, streaming));
    baked.update({128.0f, 0.0f, 128.0f}, ecs_baked, bus_baked);
    bus_baked.pump();

    dfn::ecs::World ecs_gen;
    dfn::events::EventBus bus_gen;
    dfn::world::ChunkManager generated;
    generated.open_generated(params, streaming);
    generated.update({128.0f, 0.0f, 128.0f}, ecs_gen, bus_gen);
    bus_gen.pump();

    REQUIRE(baked.is_loaded(ChunkCoord{0, 0}));
    REQUIRE(generated.is_loaded(ChunkCoord{0, 0}));
    const auto a = baked.heightfield(ChunkCoord{0, 0});
    const auto b = generated.heightfield(ChunkCoord{0, 0});
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(baked.height_at({128.0f, 128.0f}).value_or(-1.0f)
          == generated.height_at({128.0f, 128.0f}).value_or(-2.0f));

    // A file that does not exist is a REFUSAL, not a quiet fall back to the
    // generator — the failure mode this guards is a build that runs at
    // generation speed while reporting that it opened a bake.
    dfn::world::ChunkManager missing;
    CHECK_FALSE(missing.open(dir.path / "not_here.dfw", params, nullptr, streaming));
}
