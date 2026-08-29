/*
Module: engine/world
File: engine/world/sources/WorldFormat.h

Responsibility:
- The world file format (.dfw): magic, container version, section tags, and the
  reader/writer API over the Rule 7 section discipline. Worldgen writes it
  offline; the game only reads (Q13).

Key items:
- WORLD_MAGIC / WORLD_FORMAT_VERSION / section tags.
- WorldInfo: header section payload (seed, extent).
- WorldFileReader / WorldFileWriter: typed IO over BinaryReader/BinaryWriter.

Dependencies:
- Uses: engine/core/serialization, Chunk.h.
- Used by: Worldgen (writer), ChunkManager (reader), tools/worldgen CLI, editor
  (via lead), format tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7 applies in full: little-endian via BinaryWriter/Reader only, unknown
  sections skipped, migration functions from version 1 on.
- Bumping WORLD_FORMAT_VERSION requires a migration path in the same changeset.
*/

#pragma once

#include "engine/core/serialization/sources/BinaryReader.h"
#include "engine/core/serialization/sources/BinaryWriter.h"
#include "engine/world/sources/Chunk.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace dfn::world {

/// 'DFNW' — Daggerfall N world file (.dfw).
inline constexpr uint32_t WORLD_MAGIC = serialization::make_tag('D', 'F', 'N', 'W');
inline constexpr uint32_t WORLD_FORMAT_VERSION = 1;

/// Section tags of the world container. One INFO section, then one CHNK section
/// PER CHUNK (so the reader can index sections once and load chunk payloads
/// selectively during streaming), each followed by its optional ENTS section.
namespace section {
inline constexpr serialization::SectionTag INFO = serialization::make_tag('I', 'N', 'F', 'O');
inline constexpr serialization::SectionTag CHUNK = serialization::make_tag('C', 'H', 'N', 'K');
inline constexpr serialization::SectionTag ENTITIES = serialization::make_tag('E', 'N', 'T', 'S');
} // namespace section

/// Payload of the INFO section — identity and extent of the generated world.
struct WorldInfo {
    uint64_t seed = 0;            ///< Worldgen seed (Rule 13.1 determinism anchor).
    uint32_t worldgen_version = 0;///< Version of the generator that produced this.
    ChunkCoord min_chunk;         ///< Inclusive chunk-grid bounds of the region.
    ChunkCoord max_chunk;
};

/// Read access to a .dfw file. Designed for streaming: open() parses the header
/// and builds a chunk directory (tag scan, payloads untouched); load_chunk()
/// then decodes single chunks on demand.
class WorldFileReader {
public:
    WorldFileReader();
    ~WorldFileReader();

    /// Opens and indexes the file. False on IO error, bad magic, or a container
    /// version newer than this build understands (older versions are migrated
    /// transparently per Rule 7). Keeps the file data alive until destruction.
    [[nodiscard]] bool open(const std::filesystem::path& path);

    [[nodiscard]] const WorldInfo& info() const;

    /// All chunk coordinates present in the file.
    [[nodiscard]] std::vector<ChunkCoord> chunk_directory() const;

    /// Decodes one chunk (heightmap + generated entity records). nullopt if the
    /// coord is absent or its payload is corrupt.
    [[nodiscard]] std::optional<Chunk> load_chunk(ChunkCoord coord) const;

private:
    struct Impl; // owned bytes + BinaryReader + coord -> section offset index
    std::unique_ptr<Impl> impl_;
};

/// Write access — used by worldgen (offline tool) and the editor's world
/// re-export. The game itself never writes a .dfw (Q13; runtime changes go to
/// the save delta instead, Q56).
class WorldFileWriter {
public:
    WorldFileWriter();
    ~WorldFileWriter();

    /// Begins a file with the INFO section. Chunks must then be appended in
    /// deterministic order (row-major by coord, z outer) — byte-identical output
    /// for identical input is part of the worldgen determinism test (Rule 13.1).
    void begin(const WorldInfo& info);

    /// Appends one chunk as a CHNK section (+ ENTS section when non-empty).
    void append_chunk(const Chunk& chunk);

    /// Finishes and writes the file atomically. False on IO failure.
    [[nodiscard]] bool save(const std::filesystem::path& path);

private:
    struct Impl; // BinaryWriter + state guard
    std::unique_ptr<Impl> impl_;
};

} // namespace dfn::world
