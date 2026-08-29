/*
Module: engine/world
File: engine/world/sources/WorldFormat.cpp

Responsibility:
- Implements the .dfw container declared in WorldFormat.h: the writer worldgen
  uses offline, and the reader the game uses at load. This is the first half of
  the tooling pivot's baker (в1: nothing is generated in the frame; the world is
  baked into files and the game only places what it reads).

Key items:
- WorldFileWriter::begin / append_chunk / save.
- WorldFileReader::open / info / chunk_directory / load_chunk.

Dependencies:
- Uses: WorldFormat.h, Chunk.h, core serialization (BinaryWriter/BinaryReader).
- Used by: the bake tool, ChunkManager (file-backed streaming), format tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7 is absolute here: every multi-byte value goes through an explicit
  little-endian write or read call. There is no memcpy of a struct anywhere in
  this file and adding one is a format break, not an optimisation — the arrays
  below are written element by element precisely because glm::vec3 has no
  guaranteed layout across compilers.
- BYTE-IDENTICAL OUTPUT for identical input is a tested property, not a hope
  (Rule 13.1). Anything that makes the bytes depend on iteration order, address
  values, or wall clock breaks the determinism test on purpose.
- A truncated or corrupt file must fail SOFT (nullopt / false), never crash and
  never half-load: a chunk that loads with its voxel mesh missing is a hole in
  the ground the player falls through, which is worse than a refusal.
*/

#include "engine/world/sources/WorldFormat.h"

#include "engine/core/config/sources/Constants.h"

#include <cstring>
#include <fstream>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace dfn::world {
namespace {

/// Section payload versions. Separate from the container version so one
/// section can grow without invalidating the file (Rule 7).
inline constexpr uint16_t INFO_SECTION_VERSION = 1;
inline constexpr uint16_t CHUNK_SECTION_VERSION = 1;
inline constexpr uint16_t ENTITIES_SECTION_VERSION = 1;

/// A sanity bound on any length read from a file, so a corrupt count cannot
/// make the reader try to allocate the address space before it fails. It is
/// deliberately far above any real chunk (the heightmap is HEIGHTMAP_RESOLUTION
/// squared samples, a dense voxel surface is tens of thousands of vertices) and
/// far below "the machine dies trying". It is an absurdity bound and NOT a
/// lattice check — the heightmap gets a real one at its read site.
inline constexpr uint64_t MAX_ARRAY_ELEMENTS = 64ull * 1024ull * 1024ull;

void write_vec3(serialization::BinaryWriter& w, const glm::vec3& v) {
    w.write_f32(v.x);
    w.write_f32(v.y);
    w.write_f32(v.z);
}

[[nodiscard]] glm::vec3 read_vec3(serialization::BinaryReader& r) {
    const float x = r.read_f32();
    const float y = r.read_f32();
    const float z = r.read_f32();
    return {x, y, z};
}

/// Reads an element count and REFUSES an implausible one by raising the
/// caller's own failure flag. It returns 0 in that case, and 0 is also a
/// perfectly valid count — which is exactly why the refusal cannot be signalled
/// by the return value: an empty array and a rejected file would look the same,
/// and the file would load as terrain with a hole in it.
[[nodiscard]] uint64_t read_count(serialization::BinaryReader& r, bool& refused) {
    const uint32_t n = r.read_u32();
    if (static_cast<uint64_t>(n) > MAX_ARRAY_ELEMENTS) {
        refused = true;
        return 0;
    }
    return n;
}

/// Reads a whole file into memory. The reader can do this itself, but the
/// chunk directory needs the bytes to OUTLIVE the scan (load_chunk re-walks
/// them), so the file is owned here and both readers borrow it.
[[nodiscard]] bool read_whole_file(const std::filesystem::path& path,
                                   std::vector<std::byte>& out) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    if (size > 0
        && !in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size))) {
        out.clear();
        return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------

struct WorldFileWriter::Impl {
    serialization::BinaryWriter writer;
    bool begun = false;
};

WorldFileWriter::WorldFileWriter() : impl_(std::make_unique<Impl>()) {}
WorldFileWriter::~WorldFileWriter() = default;

void WorldFileWriter::begin(const WorldInfo& info) {
    impl_->writer.begin_file(WORLD_MAGIC, WORLD_FORMAT_VERSION);
    impl_->writer.begin_section(section::INFO, INFO_SECTION_VERSION);
    impl_->writer.write_u64(info.seed);
    impl_->writer.write_u32(info.worldgen_version);
    impl_->writer.write_i32(info.min_chunk.x);
    impl_->writer.write_i32(info.min_chunk.z);
    impl_->writer.write_i32(info.max_chunk.x);
    impl_->writer.write_i32(info.max_chunk.z);
    impl_->writer.end_section();
    impl_->begun = true;
}

void WorldFileWriter::append_chunk(const Chunk& chunk) {
    if (!impl_->begun) {
        return; // the writer's own misuse latch will refuse the save
    }
    serialization::BinaryWriter& w = impl_->writer;
    w.begin_section(section::CHUNK, CHUNK_SECTION_VERSION);
    w.write_i32(chunk.coord.x);
    w.write_i32(chunk.coord.z);

    // Heightmap. The samples go out element by element: they are uint16 in
    // memory too, but writing the vector's bytes would make the file
    // endian-dependent, and the format's whole promise is that it is not.
    w.write_u32(static_cast<uint32_t>(chunk.heightmap.samples.size()));
    w.write_f32(chunk.heightmap.height_scale);
    w.write_f32(chunk.heightmap.height_offset);
    for (const uint16_t s : chunk.heightmap.samples) {
        w.write_u16(s);
    }

    // P3 surface data. Three parallel arrays with their own counts rather than
    // one count for all three: they are filled by different worldgen passes and
    // a chunk that has water inputs but no splat class is a real intermediate
    // state, not a corruption.
    w.write_u32(static_cast<uint32_t>(chunk.surface.dist_to_water.size()));
    for (const float v : chunk.surface.dist_to_water) {
        w.write_f32(v);
    }
    w.write_u32(static_cast<uint32_t>(chunk.surface.water_surface.size()));
    for (const float v : chunk.surface.water_surface) {
        w.write_f32(v);
    }
    w.write_u32(static_cast<uint32_t>(chunk.surface.surface_class.size()));
    for (const uint8_t v : chunk.surface.surface_class) {
        w.write_u8(v);
    }

    // The extracted voxel surface — THE reason this file exists. Terrain is
    // drawn from it and collided against it; regenerating it at load is the
    // per-chunk cost the baker removes.
    w.write_u32(static_cast<uint32_t>(chunk.voxels.positions.size()));
    for (const glm::vec3& p : chunk.voxels.positions) {
        write_vec3(w, p);
    }
    w.write_u32(static_cast<uint32_t>(chunk.voxels.normals.size()));
    for (const glm::vec3& n : chunk.voxels.normals) {
        write_vec3(w, n);
    }
    w.write_u32(static_cast<uint32_t>(chunk.voxels.materials.size()));
    for (const uint8_t m : chunk.voxels.materials) {
        w.write_u8(m);
    }
    w.write_u32(static_cast<uint32_t>(chunk.voxels.indices.size()));
    for (const uint32_t i : chunk.voxels.indices) {
        w.write_u32(i);
    }

    // Scatter (P5 vegetation and stones): data-only instances, no ECS entity.
    w.write_u32(static_cast<uint32_t>(chunk.scatter.size()));
    for (const math::ScatterInstance& s : chunk.scatter) {
        write_vec3(w, s.position);
        w.write_f32(s.yaw);
        w.write_f32(s.scale);
        w.write_u32(static_cast<uint32_t>(s.species));
    }
    w.end_section();

    // ENTS follows its CHNK and is omitted entirely when empty — the reader
    // pairs them by adjacency, so an absent section reads as "no entities"
    // without costing eighteen bytes per empty chunk.
    if (!chunk.entities.empty()) {
        w.begin_section(section::ENTITIES, ENTITIES_SECTION_VERSION);
        w.write_u32(static_cast<uint32_t>(chunk.entities.size()));
        for (const GeneratedEntityRecord& e : chunk.entities) {
            w.write_u64(e.world_id);
            w.write_u64(e.archetype);
            w.write_f32(e.position_xz.x);
            w.write_f32(e.position_xz.y);
            w.write_f32(e.yaw);
            w.write_f32(e.ground_y);
        }
        w.end_section();
    }
}

bool WorldFileWriter::save(const std::filesystem::path& path) {
    if (!impl_->begun || !impl_->writer.ok()) {
        return false;
    }
    return impl_->writer.save_to_file(path);
}

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------

struct WorldFileReader::Impl {
    std::vector<std::byte> bytes;
    WorldInfo info;
    /// coord -> ORDINAL of its CHNK section in the file. An ordinal rather than
    /// a byte offset because BinaryReader owns its cursor and exposes no seek:
    /// load_chunk() re-walks the section list, which only steps over payloads
    /// (pointer arithmetic, no decoding) and costs nothing measurable against
    /// the decode that follows. When the reader grows a seek, this becomes an
    /// offset and the walk disappears.
    std::unordered_map<uint64_t, std::size_t> chunk_ordinal;
    std::vector<ChunkCoord> order; ///< directory in file order (deterministic)
};

WorldFileReader::WorldFileReader() : impl_(std::make_unique<Impl>()) {}
WorldFileReader::~WorldFileReader() = default;

bool WorldFileReader::open(const std::filesystem::path& path) {
    impl_->chunk_ordinal.clear();
    impl_->order.clear();
    impl_->info = WorldInfo{};

    if (!read_whole_file(path, impl_->bytes)) {
        return false;
    }
    serialization::BinaryReader r;
    if (!r.open(std::span<const std::byte>{impl_->bytes}, WORLD_MAGIC)) {
        impl_->bytes.clear();
        return false;
    }
    // A container from a NEWER build is refused rather than guessed at; older
    // versions would be migrated here (Rule 7 — the hook exists from day one
    // even while there is exactly one version to migrate between).
    if (r.container_version() > WORLD_FORMAT_VERSION) {
        return false;
    }

    bool saw_info = false;
    std::size_t ordinal = 0;
    while (const auto s = r.next_section()) {
        if (s->tag == section::INFO) {
            impl_->info.seed = r.read_u64();
            impl_->info.worldgen_version = r.read_u32();
            impl_->info.min_chunk.x = r.read_i32();
            impl_->info.min_chunk.z = r.read_i32();
            impl_->info.max_chunk.x = r.read_i32();
            impl_->info.max_chunk.z = r.read_i32();
            saw_info = true;
        } else if (s->tag == section::CHUNK) {
            const ChunkCoord coord{r.read_i32(), r.read_i32()};
            impl_->chunk_ordinal.emplace(chunk_group(coord), ordinal);
            impl_->order.push_back(coord);
        }
        // Any other tag is data this build does not know: next_section() steps
        // over it. That is the forward-compatibility guarantee, not laziness.
        ++ordinal;
    }
    if (!saw_info || !r.ok()) {
        impl_->chunk_ordinal.clear();
        impl_->order.clear();
        impl_->bytes.clear();
        return false;
    }
    return true;
}

const WorldInfo& WorldFileReader::info() const { return impl_->info; }

std::vector<ChunkCoord> WorldFileReader::chunk_directory() const { return impl_->order; }

std::optional<Chunk> WorldFileReader::load_chunk(ChunkCoord coord) const {
    const auto it = impl_->chunk_ordinal.find(chunk_group(coord));
    if (it == impl_->chunk_ordinal.end()) {
        return std::nullopt;
    }
    // The path is re-read rather than kept mapped because the reader owns its
    // bytes; open() proved the file parses, so this cannot be the first place a
    // corruption is met — but it is still checked, because "it parsed a moment
    // ago" is not a property of a file on a disk somebody else can write.
    serialization::BinaryReader r;
    if (!r.open(std::span<const std::byte>{impl_->bytes}, WORLD_MAGIC)) {
        return std::nullopt;
    }
    std::size_t ordinal = 0;
    std::optional<serialization::SectionInfo> s;
    while ((s = r.next_section())) {
        if (ordinal == it->second) {
            break;
        }
        ++ordinal;
    }
    if (!s || s->tag != section::CHUNK) {
        return std::nullopt;
    }

    Chunk chunk;
    chunk.coord = {r.read_i32(), r.read_i32()};

    bool refused = false; // raised by any implausible count, checked once at the end

    const uint64_t sample_count = read_count(r, refused);
    // THE HEIGHTMAP IS READ ON A LATTICE THIS BUILD BELIEVES IN, AND THE FILE
    // DOES NOT CARRY ITS OWN. `sample_count` was checked only against the
    // absurdity bound, while every reader downstream — Heightmap::height_at,
    // Heightmap::view — strides by config::HEIGHTMAP_RESOLUTION. A world baked
    // on a different lattice therefore LOADED, reported ok(), and then indexed
    // past the end of the vector: a silent out-of-bounds read handed to render
    // and to physics as if it were ground.
    //
    // Found while moving HEIGHTMAP_STEP 2.0 -> 1.0 m (18.08.2026), which is
    // exactly the edit that makes stale files exist. The .relief sidecar has
    // refused a lattice mismatch by name since the day it was written; this,
    // the far bigger file, did not. Refusing is all that is owed here — the
    // caller regenerates a chunk it cannot read (ChunkManager does so already),
    // so a rejected bake costs generation time and nothing else.
    const uint64_t expected_samples = static_cast<uint64_t>(config::HEIGHTMAP_RESOLUTION)
                                    * static_cast<uint64_t>(config::HEIGHTMAP_RESOLUTION);
    if (sample_count != expected_samples) {
        return std::nullopt;
    }
    chunk.heightmap.height_scale = r.read_f32();
    chunk.heightmap.height_offset = r.read_f32();
    chunk.heightmap.samples.resize(static_cast<std::size_t>(sample_count));
    for (uint16_t& v : chunk.heightmap.samples) {
        v = r.read_u16();
    }

    const uint64_t dist_count = read_count(r, refused);
    chunk.surface.dist_to_water.resize(static_cast<std::size_t>(dist_count));
    for (float& v : chunk.surface.dist_to_water) {
        v = r.read_f32();
    }
    const uint64_t water_count = read_count(r, refused);
    chunk.surface.water_surface.resize(static_cast<std::size_t>(water_count));
    for (float& v : chunk.surface.water_surface) {
        v = r.read_f32();
    }
    const uint64_t class_count = read_count(r, refused);
    chunk.surface.surface_class.resize(static_cast<std::size_t>(class_count));
    for (uint8_t& v : chunk.surface.surface_class) {
        v = r.read_u8();
    }

    const uint64_t position_count = read_count(r, refused);
    chunk.voxels.positions.resize(static_cast<std::size_t>(position_count));
    for (glm::vec3& p : chunk.voxels.positions) {
        p = read_vec3(r);
    }
    const uint64_t normal_count = read_count(r, refused);
    chunk.voxels.normals.resize(static_cast<std::size_t>(normal_count));
    for (glm::vec3& n : chunk.voxels.normals) {
        n = read_vec3(r);
    }
    const uint64_t material_count = read_count(r, refused);
    chunk.voxels.materials.resize(static_cast<std::size_t>(material_count));
    for (uint8_t& m : chunk.voxels.materials) {
        m = r.read_u8();
    }
    const uint64_t index_count = read_count(r, refused);
    chunk.voxels.indices.resize(static_cast<std::size_t>(index_count));
    for (uint32_t& i : chunk.voxels.indices) {
        i = r.read_u32();
    }

    const uint64_t scatter_count = read_count(r, refused);
    chunk.scatter.resize(static_cast<std::size_t>(scatter_count));
    for (math::ScatterInstance& inst : chunk.scatter) {
        inst.position = read_vec3(r);
        inst.yaw = r.read_f32();
        inst.scale = r.read_f32();
        inst.species = static_cast<math::ScatterSpecies>(r.read_u32());
    }

    // The chunk's entities live in the NEXT section when there are any. Peeking
    // costs one step of the walk we are already standing in.
    if (const auto next = r.next_section(); next && next->tag == section::ENTITIES) {
        const uint64_t entity_count = read_count(r, refused);
        chunk.entities.resize(static_cast<std::size_t>(entity_count));
        for (GeneratedEntityRecord& e : chunk.entities) {
            e.world_id = r.read_u64();
            e.archetype = r.read_u64();
            e.position_xz.x = r.read_f32();
            e.position_xz.y = r.read_f32();
            e.yaw = r.read_f32();
            e.ground_y = r.read_f32();
        }
    }

    // ONE check for the whole chunk, at the end. A half-decoded chunk is worse
    // than a refused one: it is terrain with holes, and the player falls
    // through a hole without ever being told a file was corrupt.
    if (!r.ok() || refused) {
        return std::nullopt;
    }
    return chunk;
}

} // namespace dfn::world
