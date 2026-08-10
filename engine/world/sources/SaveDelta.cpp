/*
Created: 10:08:2026 - 21:20:31
Last updated: 10:08:2026 - 21:20:31
Module: engine/world
File: engine/world/sources/SaveDelta.cpp

Responsibility:
- Implements SaveDelta::chunk_delta and the SaveDeltaCodec: the .dfs container's
  world-owned sections (META, entity deltas, dynamic spawns), dispatch of every
  registered module section, and verbatim preservation of sections this build
  does not understand.

Key items:
- SaveDeltaCodec::register_section / write_save / read_save.

Dependencies:
- Uses: SaveDelta.h, core serialization (BinaryWriter/BinaryReader), core ecs.
- Used by: engine/app (save/load), gameplay section registration, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7: a save stores a DELTA against the generated world, never a world copy.
  Nothing here may serialize generated geometry or entity contents — only what
  changed relative to what the generator will produce again from the seed.
- PRESERVING UNKNOWN SECTIONS IS NOT AN OPTIMISATION, IT IS THE FORWARD
  COMPATIBILITY GUARANTEE. If an older build loads a newer save and re-saves it,
  dropping the sections it did not recognise would silently delete the player's
  progress in whatever system owns them. Skip-unknown in the reader keeps the
  load working; copying the bytes back out on the next write is what keeps the
  data alive. Deleting that loop is a data-loss bug, not a cleanup.
*/
/*
UPD:
- 10:08:2026 - 21:20:31: Stage-2 implementation, written by sim under an
  explicit lead carve of core's zone (Rule 25): this file plus its line in
  engine/world/CMakeLists.txt, nothing else in engine/world. It was the last
  declaration-only piece between the engine and a save file the user can
  actually write, after BinaryWriter/BinaryReader landed an hour earlier.
*/

#include "engine/world/sources/SaveDelta.h"

#include <algorithm>
#include <cassert>

namespace dfn::world {

namespace {

// Section body versions. Each is independent of SAVE_FORMAT_VERSION, which
// describes the CONTAINER only (see BinaryReader::container_version): a change
// to the shape of an entity delta bumps the number below and nothing else.
constexpr uint16_t META_VERSION = 1;
constexpr uint16_t ENTITY_DELTAS_VERSION = 1;
constexpr uint16_t DYNAMIC_SPAWNS_VERSION = 1;

void write_coord(serialization::BinaryWriter& w, ChunkCoord coord) {
    w.write_i32(coord.x);
    w.write_i32(coord.z);
}

[[nodiscard]] ChunkCoord read_coord(serialization::BinaryReader& r) {
    ChunkCoord coord;
    coord.x = r.read_i32();
    coord.z = r.read_i32();
    return coord;
}

/// The bucket for `coord`, created if absent. Load builds the delta chunk by
/// chunk out of two independent sections, so both must be able to find or
/// create the same bucket.
[[nodiscard]] ChunkDelta& bucket_for(SaveDelta& delta, ChunkCoord coord) {
    for (ChunkDelta& chunk : delta.chunks) {
        if (chunk.coord == coord) {
            return chunk;
        }
    }
    delta.chunks.push_back(ChunkDelta{coord, {}, {}});
    return delta.chunks.back();
}

} // namespace

const ChunkDelta* SaveDelta::chunk_delta(ChunkCoord coord) const {
    for (const ChunkDelta& chunk : chunks) {
        if (chunk.coord == coord) {
            return &chunk;
        }
    }
    return nullptr; // untouched chunk: the generator's output stands unmodified
}

// --- Codec -------------------------------------------------------------------

/// A section this build did not recognise, kept byte for byte so the next
/// write_save can hand it back unchanged (see the header note).
struct PreservedSection {
    serialization::SectionTag tag = 0;
    uint16_t version = 0;
    std::vector<std::byte> payload;
};

struct SaveDeltaCodec::Impl {
    std::vector<SaveSectionHooks> hooks;
    std::vector<PreservedSection> preserved;

    [[nodiscard]] bool registered(serialization::SectionTag tag) const {
        return std::any_of(hooks.begin(), hooks.end(),
                           [tag](const SaveSectionHooks& h) { return h.tag == tag; });
    }
};

SaveDeltaCodec::SaveDeltaCodec() : impl_(std::make_unique<Impl>()) {}
SaveDeltaCodec::~SaveDeltaCodec() = default;

void SaveDeltaCodec::register_section(SaveSectionHooks hooks) {
    assert(!impl_->registered(hooks.tag) &&
           "duplicate save section tag: two modules would overwrite each other");
    assert(hooks.write && hooks.read && "a section without both hooks cannot round-trip");
    if (impl_->registered(hooks.tag)) {
        return; // release builds: first registration wins, second is ignored
    }
    impl_->hooks.push_back(std::move(hooks));
}

bool SaveDeltaCodec::write_save(const std::filesystem::path& path, const SaveDelta& delta,
                                const ecs::World& ecs) {
    serialization::BinaryWriter writer;
    writer.begin_file(SAVE_MAGIC, SAVE_FORMAT_VERSION);

    writer.begin_section(save_section::META, META_VERSION);
    writer.write_u64(delta.world_seed);
    writer.end_section();

    // Entity deltas and dynamic spawns are two sections over the same chunk
    // buckets, so each carries its own chunk coordinates rather than relying on
    // the other's ordering. A save is read by a build that may know one section
    // and not the other.
    writer.begin_section(save_section::ENTITY_DELTAS, ENTITY_DELTAS_VERSION);
    writer.write_u32(static_cast<uint32_t>(delta.chunks.size()));
    for (const ChunkDelta& chunk : delta.chunks) {
        write_coord(writer, chunk.coord);
        writer.write_u32(static_cast<uint32_t>(chunk.entity_deltas.size()));
        for (const EntityDelta& entity : chunk.entity_deltas) {
            writer.write_u64(entity.world_id);
            writer.write_u8(static_cast<uint8_t>(entity.kind));
            writer.write_f32(entity.position_xz.x);
            writer.write_f32(entity.position_xz.y);
            writer.write_f32(entity.yaw);
        }
    }
    writer.end_section();

    writer.begin_section(save_section::DYNAMIC_SPAWNS, DYNAMIC_SPAWNS_VERSION);
    writer.write_u32(static_cast<uint32_t>(delta.chunks.size()));
    for (const ChunkDelta& chunk : delta.chunks) {
        write_coord(writer, chunk.coord);
        writer.write_u32(static_cast<uint32_t>(chunk.dynamic_spawns.size()));
        for (const DynamicSpawn& spawn : chunk.dynamic_spawns) {
            writer.write_u64(spawn.dynamic_id);
            writer.write_u64(spawn.archetype);
            write_coord(writer, spawn.chunk);
            writer.write_f32(spawn.position_xz.x);
            writer.write_f32(spawn.position_xz.y);
            writer.write_f32(spawn.yaw);
        }
    }
    writer.end_section();

    for (const SaveSectionHooks& hook : impl_->hooks) {
        writer.begin_section(hook.tag, hook.version);
        hook.write(writer, ecs);
        writer.end_section();
    }

    // Hand back everything the last load did not understand. A tag that has
    // since become known is skipped: the live module is now the authority on
    // it, and emitting both copies would produce a file with two sections of
    // the same tag whose read order decides the outcome.
    for (const PreservedSection& section : impl_->preserved) {
        if (impl_->registered(section.tag)) {
            continue;
        }
        writer.begin_section(section.tag, section.version);
        writer.write_bytes(section.payload);
        writer.end_section();
    }

    if (!writer.ok()) {
        return false; // a hook misused the writer; do not put that on disk
    }
    return writer.save_to_file(path);
}

bool SaveDeltaCodec::read_save(const std::filesystem::path& path, SaveDelta& out_delta,
                               ecs::World& ecs) {
    // SEED GUARD, and the contract the declaration left open: a NON-ZERO
    // out_delta.world_seed on entry is the caller's statement of which world it
    // is about to load into, and a save written against a different seed is
    // refused. Zero means "no expectation" and the stored seed is simply
    // reported. Loading a save into the wrong world does not fail loudly on its
    // own — it produces deltas addressed to entities the generator never made,
    // which is a silent, unbounded corruption of somebody's game.
    const uint64_t expected_seed = out_delta.world_seed;

    serialization::BinaryReader reader;
    if (!reader.open_file(path, SAVE_MAGIC)) {
        return false; // missing, unreadable, or not a .dfs at all
    }

    SaveDelta loaded;
    std::vector<PreservedSection> preserved;
    bool saw_meta = false;

    while (const auto section = reader.next_section()) {
        switch (section->tag) {
        case save_section::META: {
            if (section->version > META_VERSION) {
                return false;
            }
            loaded.world_seed = reader.read_u64();
            saw_meta = true;
            break;
        }
        case save_section::ENTITY_DELTAS: {
            if (section->version > ENTITY_DELTAS_VERSION) {
                return false;
            }
            const uint32_t chunk_count = reader.read_u32();
            for (uint32_t c = 0; c < chunk_count && reader.ok(); ++c) {
                ChunkDelta& bucket = bucket_for(loaded, read_coord(reader));
                const uint32_t count = reader.read_u32();
                for (uint32_t i = 0; i < count && reader.ok(); ++i) {
                    EntityDelta entity;
                    entity.world_id = reader.read_u64();
                    entity.kind = static_cast<EntityDelta::Kind>(reader.read_u8());
                    entity.position_xz.x = reader.read_f32();
                    entity.position_xz.y = reader.read_f32();
                    entity.yaw = reader.read_f32();
                    bucket.entity_deltas.push_back(entity);
                }
            }
            break;
        }
        case save_section::DYNAMIC_SPAWNS: {
            if (section->version > DYNAMIC_SPAWNS_VERSION) {
                return false;
            }
            const uint32_t chunk_count = reader.read_u32();
            for (uint32_t c = 0; c < chunk_count && reader.ok(); ++c) {
                ChunkDelta& bucket = bucket_for(loaded, read_coord(reader));
                const uint32_t count = reader.read_u32();
                for (uint32_t i = 0; i < count && reader.ok(); ++i) {
                    DynamicSpawn spawn;
                    spawn.dynamic_id = reader.read_u64();
                    spawn.archetype = reader.read_u64();
                    spawn.chunk = read_coord(reader);
                    spawn.position_xz.x = reader.read_f32();
                    spawn.position_xz.y = reader.read_f32();
                    spawn.yaw = reader.read_f32();
                    bucket.dynamic_spawns.push_back(spawn);
                }
            }
            break;
        }
        default: {
            const auto hook = std::find_if(
                impl_->hooks.begin(), impl_->hooks.end(),
                [&](const SaveSectionHooks& h) { return h.tag == section->tag; });
            if (hook != impl_->hooks.end()) {
                if (!hook->read(reader, ecs, section->version)) {
                    return false; // the module declared the data unrecoverable
                }
            } else {
                // Unknown to this build: copy it out verbatim so the next
                // write_save can hand it back. This is the whole forward
                // compatibility story — see the header note.
                PreservedSection kept;
                kept.tag = section->tag;
                kept.version = section->version;
                kept.payload.resize(
                    static_cast<std::size_t>(reader.section_bytes_remaining()));
                reader.read_bytes(kept.payload);
                preserved.push_back(std::move(kept));
            }
            break;
        }
        }
        if (!reader.ok()) {
            return false; // truncation or a section that over-read its length
        }
    }

    if (!reader.ok() || !saw_meta) {
        // A .dfs with no META is not an empty save, it is a truncated one — and
        // this is the one place the container's inability to tell those apart
        // (nothing in its header records a section count) is closed, by a
        // caller that knows which section MUST be there.
        return false;
    }
    if (expected_seed != 0 && loaded.world_seed != expected_seed) {
        return false; // a save for a different world
    }

    out_delta = std::move(loaded);
    impl_->preserved = std::move(preserved);
    return true;
}

} // namespace dfn::world
