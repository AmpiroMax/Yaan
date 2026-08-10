/*
Created: 10:08:2026 - 21:21:55
Last updated: 10:08:2026 - 21:21:55
Module: tests
File: tests/sim/SaveDeltaTests.cpp

Responsibility:
- The .dfs codec end to end: a real file on disk, written and read back with the
  world-owned sections and the gameplay sections that register with it. Plus the
  three guarantees that are easy to lose and expensive to lose silently — the
  seed guard, the refusal of a save with no META, and verbatim preservation of
  sections this build does not understand.

Key items:
- The forward-compatibility case: a save round-tripped THROUGH a build that does
  not know one of its sections must come out the other side with that section's
  data intact.

Dependencies:
- Uses: doctest, dfn_world (SaveDeltaCodec), dfn_gameplay (sections), dfn_core.
- Used by: ctest (sim_save_delta).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Zone note: the codec is engine/world (core's zone); sim implemented it under a
  lead carve, so its suite lives here. Move to tests/core/ when core takes it
  back — the assertions carry over unchanged.
*/
/*
UPD:
- 10:08:2026 - 21:21:55: Created with the SaveDeltaCodec implementation.
*/

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "engine/core/ecs/sources/World.h"
#include "engine/core/serialization/sources/BinaryReader.h"
#include "engine/core/serialization/sources/BinaryWriter.h"
#include "engine/gameplay/sources/GameplaySave.h"
#include "engine/gameplay/sources/Inventory.h"
#include "engine/world/sources/SaveDelta.h"

namespace {

namespace world = dfn::world;
namespace gameplay = dfn::gameplay;
namespace serialization = dfn::serialization;
using dfn::ecs::EntityId;

// A tag no module owns, used to play the part of "a section written by a build
// newer than this one".
constexpr serialization::SectionTag TAG_FUTURE =
    serialization::make_tag('F', 'U', 'T', 'R');

struct TempDir {
    std::filesystem::path path;
    explicit TempDir(const char* name)
        : path(std::filesystem::temp_directory_path() / name) {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~TempDir() { std::filesystem::remove_all(path); }
};

[[nodiscard]] world::SaveDelta make_delta(uint64_t seed, float yaw, uint32_t spawn_id) {
    world::SaveDelta delta;
    delta.world_seed = seed;
    world::ChunkDelta chunk;
    chunk.coord = {3, -4};
    chunk.entity_deltas.push_back(
        {12345ull, world::EntityDelta::Kind::Destroyed, {0.0f, 0.0f}, 0.0f});
    chunk.entity_deltas.push_back(
        {67890ull, world::EntityDelta::Kind::Moved, {11.5f, -22.25f}, yaw});
    chunk.dynamic_spawns.push_back(
        {spawn_id, 0xABCD'1234'5678'9F01ull, {3, -4}, {1.5f, 2.5f}, 0.75f});
    delta.chunks.push_back(std::move(chunk));

    world::ChunkDelta other;
    other.coord = {0, 0};
    other.entity_deltas.push_back(
        {7ull, world::EntityDelta::Kind::StateChanged, {0.0f, 0.0f}, 0.0f});
    delta.chunks.push_back(std::move(other));
    return delta;
}

} // namespace

TEST_CASE("a delta survives a real file, and a different delta comes back different") {
    TempDir dir("dfn_save_delta_round_trip");
    dfn::ecs::World ecs;

    // Rule 30's control for a round trip is a SECOND payload: a codec that
    // returned a fixed delta would satisfy either one of these alone.
    auto round_trip = [&](const world::SaveDelta& in, const char* file) {
        world::SaveDeltaCodec codec;
        REQUIRE(codec.write_save(dir.path / file, in, ecs));
        world::SaveDelta out;
        world::SaveDeltaCodec loader;
        REQUIRE(loader.read_save(dir.path / file, out, ecs));
        return out;
    };

    const world::SaveDelta first = make_delta(0xFEEDull, 1.25f, 900u);
    const world::SaveDelta second = make_delta(0xFEEDull, -3.0f, 901u);
    const world::SaveDelta out_first = round_trip(first, "a.dfs");
    const world::SaveDelta out_second = round_trip(second, "b.dfs");

    CHECK(out_first.world_seed == 0xFEEDull);
    REQUIRE(out_first.chunks.size() == 2);

    const world::ChunkDelta* touched = out_first.chunk_delta({3, -4});
    REQUIRE(touched != nullptr);
    REQUIRE(touched->entity_deltas.size() == 2);
    CHECK(touched->entity_deltas[0].world_id == 12345ull);
    CHECK(touched->entity_deltas[0].kind == world::EntityDelta::Kind::Destroyed);
    CHECK(touched->entity_deltas[1].kind == world::EntityDelta::Kind::Moved);
    CHECK(touched->entity_deltas[1].position_xz.x == doctest::Approx(11.5f));
    CHECK(touched->entity_deltas[1].position_xz.y == doctest::Approx(-22.25f));
    CHECK(touched->entity_deltas[1].yaw == doctest::Approx(1.25f));
    REQUIRE(touched->dynamic_spawns.size() == 1);
    CHECK(touched->dynamic_spawns[0].dynamic_id == 900u);
    CHECK(touched->dynamic_spawns[0].archetype == 0xABCD'1234'5678'9F01ull);
    CHECK(touched->dynamic_spawns[0].chunk == world::ChunkCoord{3, -4});

    // An untouched chunk has no bucket at all — that is what makes this a delta
    // rather than a world copy (Rule 7).
    CHECK(out_first.chunk_delta({99, 99}) == nullptr);

    // The control: the second payload came back as itself.
    const world::ChunkDelta* second_touched = out_second.chunk_delta({3, -4});
    REQUIRE(second_touched != nullptr);
    CHECK(second_touched->entity_deltas[1].yaw == doctest::Approx(-3.0f));
    CHECK(second_touched->dynamic_spawns[0].dynamic_id == 901u);
    CHECK(touched->entity_deltas[1].yaw != second_touched->entity_deltas[1].yaw);
}

TEST_CASE("registered gameplay sections travel in the same file") {
    TempDir dir("dfn_save_delta_sections");
    const std::filesystem::path file = dir.path / "game.dfs";

    dfn::ecs::World ecs;
    const EntityId player = ecs.spawn();
    ecs.add(player, gameplay::Inventory{{{gameplay::ItemId{0xC01Dull}, 42}}});

    world::SaveDeltaCodec codec;
    gameplay::register_gameplay_save_sections(codec);
    world::SaveDelta delta;
    delta.world_seed = 99;
    REQUIRE(codec.write_save(file, delta, ecs));

    // Corrupt the live state so a "restore" that does nothing cannot pass.
    ecs.get<gameplay::Inventory>(player)->stacks.clear();

    world::SaveDeltaCodec loader;
    gameplay::register_gameplay_save_sections(loader);
    world::SaveDelta loaded;
    REQUIRE(loader.read_save(file, loaded, ecs));

    const auto& stacks = ecs.get<gameplay::Inventory>(player)->stacks;
    REQUIRE(stacks.size() == 1);
    CHECK(stacks[0].item.value == 0xC01Dull);
    CHECK(stacks[0].count == 42);
}

TEST_CASE("a section this build does not understand survives a load and a re-save") {
    // THE FORWARD COMPATIBILITY CASE. An old build opening a new save must not
    // quietly delete the systems it has never heard of. Skip-unknown keeps the
    // LOAD working; copying the bytes back out on the next WRITE is what keeps
    // the data alive, and only this case can tell the two apart.
    TempDir dir("dfn_save_delta_preserve");
    const std::filesystem::path newer = dir.path / "newer.dfs";
    const std::filesystem::path resaved = dir.path / "resaved.dfs";
    dfn::ecs::World ecs;

    // A save written by a build that has a FUTR section, faked by writing the
    // container directly (this build has no such module, by construction).
    {
        serialization::BinaryWriter w;
        w.begin_file(world::SAVE_MAGIC, world::SAVE_FORMAT_VERSION);
        w.begin_section(world::save_section::META, 1);
        w.write_u64(0x5EEDull);
        w.end_section();
        w.begin_section(TAG_FUTURE, 4);
        w.write_u64(0xDEFACEDull);
        w.write_string("progress in a system this build has never heard of");
        w.end_section();
        REQUIRE(w.ok());
        REQUIRE(w.save_to_file(newer));
    }

    world::SaveDeltaCodec old_build;
    world::SaveDelta loaded;
    REQUIRE(old_build.read_save(newer, loaded, ecs)); // unknown tag is not an error
    CHECK(loaded.world_seed == 0x5EEDull);
    REQUIRE(old_build.write_save(resaved, loaded, ecs));

    // The re-saved file must still contain FUTR, byte for byte, at its own
    // version — which the old build could not have reconstructed.
    serialization::BinaryReader r;
    REQUIRE(r.open_file(resaved, world::SAVE_MAGIC));
    bool found = false;
    while (const auto section = r.next_section()) {
        if (section->tag != TAG_FUTURE) {
            continue;
        }
        found = true;
        CHECK(section->version == 4);
        CHECK(r.read_u64() == 0xDEFACEDull);
        CHECK(r.read_string() == "progress in a system this build has never heard of");
    }
    CHECK(found);
    CHECK(r.ok());

    // CONTROL, and it is the arm that makes this a test rather than a
    // description: a codec that never LOADED the newer file has nothing to
    // preserve, so writing from it must NOT produce a FUTR section. Without
    // this, "the tag is present" would also pass on an implementation that
    // emitted FUTR unconditionally from somewhere else.
    const std::filesystem::path fresh = dir.path / "fresh.dfs";
    world::SaveDeltaCodec never_loaded;
    REQUIRE(never_loaded.write_save(fresh, loaded, ecs));
    serialization::BinaryReader r2;
    REQUIRE(r2.open_file(fresh, world::SAVE_MAGIC));
    bool found_in_fresh = false;
    while (const auto section = r2.next_section()) {
        found_in_fresh = found_in_fresh || section->tag == TAG_FUTURE;
    }
    CHECK_FALSE(found_in_fresh);
}

TEST_CASE("a save written for another world is refused") {
    TempDir dir("dfn_save_delta_seed");
    const std::filesystem::path file = dir.path / "other_world.dfs";
    dfn::ecs::World ecs;

    world::SaveDelta delta;
    delta.world_seed = 1111;
    world::SaveDeltaCodec codec;
    REQUIRE(codec.write_save(file, delta, ecs));

    // Loading into a world whose seed disagrees must fail. The deltas address
    // entities by generated id; against the wrong world they name whatever the
    // other generator happened to put at those ids, which corrupts silently and
    // without bound.
    world::SaveDelta into_wrong_world;
    into_wrong_world.world_seed = 2222; // the caller states which world it is
    CHECK_FALSE(codec.read_save(file, into_wrong_world, ecs));

    // Two controls. The right seed loads...
    world::SaveDelta into_right_world;
    into_right_world.world_seed = 1111;
    CHECK(codec.read_save(file, into_right_world, ecs));
    // ...and zero means "no expectation", which must not be read as a mismatch
    // against a stored seed of 1111.
    world::SaveDelta no_expectation;
    CHECK(codec.read_save(file, no_expectation, ecs));
    CHECK(no_expectation.world_seed == 1111);
}

TEST_CASE("a .dfs with no META is refused rather than read as an empty save") {
    // The container cannot distinguish a file truncated after its header from a
    // legitimately empty one — nothing in the header records a section count,
    // so no reader can close that gap from inside. This is where it IS closed:
    // by a caller that knows META must be there.
    TempDir dir("dfn_save_delta_meta");
    const std::filesystem::path headerless = dir.path / "headerless.dfs";
    dfn::ecs::World ecs;

    {
        serialization::BinaryWriter w;
        w.begin_file(world::SAVE_MAGIC, world::SAVE_FORMAT_VERSION);
        REQUIRE(w.ok());
        REQUIRE(w.save_to_file(headerless)); // valid container, no sections
    }
    world::SaveDelta out;
    CHECK_FALSE(world::SaveDeltaCodec{}.read_save(headerless, out, ecs));

    // Control: the same file plus a META section loads. Otherwise this case
    // would pass on a read_save that always failed.
    const std::filesystem::path with_meta = dir.path / "with_meta.dfs";
    {
        serialization::BinaryWriter w;
        w.begin_file(world::SAVE_MAGIC, world::SAVE_FORMAT_VERSION);
        w.begin_section(world::save_section::META, 1);
        w.write_u64(4242);
        w.end_section();
        REQUIRE(w.save_to_file(with_meta));
    }
    world::SaveDelta ok_out;
    CHECK(world::SaveDeltaCodec{}.read_save(with_meta, ok_out, ecs));
    CHECK(ok_out.world_seed == 4242);

    // And a file that is not a save at all is refused at the magic.
    const std::filesystem::path junk = dir.path / "junk.dfs";
    {
        std::ofstream out_stream(junk, std::ios::binary);
        out_stream << "this is not a save file, it is a text file";
    }
    world::SaveDelta junk_out;
    CHECK_FALSE(world::SaveDeltaCodec{}.read_save(junk, junk_out, ecs));
    CHECK_FALSE(world::SaveDeltaCodec{}.read_save(dir.path / "no_such_file.dfs",
                                                  junk_out, ecs));
}
