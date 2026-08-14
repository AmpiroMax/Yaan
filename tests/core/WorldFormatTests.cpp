/*
Created: 14:08:2026 - 18:43:43
Last updated: 14:08:2026 - 18:43:43
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
*/

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
