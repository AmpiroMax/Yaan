/*
Created: 14:08:2026 - 23:36:19
Last updated: 14:08:2026 - 23:36:19
Module: engine/render
File: engine/render/sources/ObjectRegistry.cpp

Responsibility:
- Implements the .dfo container: write_object / read_object /
  object_content_hash over the Rule 7 section discipline.

Dependencies:
- Uses: ObjectRegistry.h, core serialization.
- Used by: dfn_render target, tools/forge_trees.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every multi-byte value goes through explicit little-endian calls; vertices
  are written FIELD BY FIELD because platform::Vertex's layout is a compiler's
  choice, not a format.
- The hash walks the streams in the same order the file stores them; changing
  either order is a format break that silently re-versions every object.
*/
/*
UPD:
- 14:08:2026 - 23:36:19: Created with ObjectRegistry.h.
*/

#include "engine/render/sources/ObjectRegistry.h"

#include "engine/core/serialization/sources/BinaryReader.h"
#include "engine/core/serialization/sources/BinaryWriter.h"
#include "engine/core/serialization/sources/ContentHash.h"

#include <bit>
#include <cstdio>

namespace dfn::render {
namespace {

/// 'DFNO' — Daggerfall N object file (.dfo).
inline constexpr uint32_t OBJECT_MAGIC = serialization::make_tag('D', 'F', 'N', 'O');
inline constexpr uint32_t OBJECT_FORMAT_VERSION = 1;

namespace section {
inline constexpr serialization::SectionTag INFO = serialization::make_tag('I', 'N', 'F', 'O');
inline constexpr serialization::SectionTag WOOD = serialization::make_tag('W', 'O', 'O', 'D');
inline constexpr serialization::SectionTag CARD = serialization::make_tag('C', 'A', 'R', 'D');
inline constexpr serialization::SectionTag GRND = serialization::make_tag('G', 'R', 'N', 'D');
} // namespace section

inline constexpr uint16_t SECTION_VERSION = 1;

/// Far above any real object (the whole forest chunk is ~160k triangles) and
/// far below "the machine dies allocating" — the same stance as the world
/// format's bound, for the same reason.
inline constexpr uint64_t MAX_ELEMENTS = 16ull * 1024ull * 1024ull;

void write_stream(serialization::BinaryWriter& w, serialization::SectionTag tag,
                  const MeshData& mesh) {
    w.begin_section(tag, SECTION_VERSION);
    w.write_u32(static_cast<uint32_t>(mesh.vertices.size()));
    for (const platform::Vertex& v : mesh.vertices) {
        w.write_f32(v.position.x);
        w.write_f32(v.position.y);
        w.write_f32(v.position.z);
        w.write_f32(v.normal.x);
        w.write_f32(v.normal.y);
        w.write_f32(v.normal.z);
        w.write_f32(v.uv.x);
        w.write_f32(v.uv.y);
        w.write_u32(v.color_rgba);
    }
    w.write_u32(static_cast<uint32_t>(mesh.indices.size()));
    for (const uint32_t i : mesh.indices) {
        w.write_u32(i);
    }
    w.end_section();
}

[[nodiscard]] bool read_stream(serialization::BinaryReader& r, MeshData& mesh) {
    const uint32_t vertex_count = r.read_u32();
    if (static_cast<uint64_t>(vertex_count) > MAX_ELEMENTS) {
        return false;
    }
    mesh.vertices.resize(vertex_count);
    for (platform::Vertex& v : mesh.vertices) {
        v.position.x = r.read_f32();
        v.position.y = r.read_f32();
        v.position.z = r.read_f32();
        v.normal.x = r.read_f32();
        v.normal.y = r.read_f32();
        v.normal.z = r.read_f32();
        v.uv.x = r.read_f32();
        v.uv.y = r.read_f32();
        v.color_rgba = r.read_u32();
    }
    const uint32_t index_count = r.read_u32();
    if (static_cast<uint64_t>(index_count) > MAX_ELEMENTS) {
        return false;
    }
    mesh.indices.resize(index_count);
    for (uint32_t& i : mesh.indices) {
        i = r.read_u32();
    }
    return r.ok();
}

void hash_stream(serialization::Fnv1a64& h, const MeshData& mesh) {
    h.update_u64(mesh.vertices.size());
    for (const platform::Vertex& v : mesh.vertices) {
        // Float BITS, not values: the identity must be exactly the bytes the
        // file stores, or two byte-identical files could hash apart on a
        // platform with different float formatting rules.
        h.update_u64(std::bit_cast<uint32_t>(v.position.x));
        h.update_u64(std::bit_cast<uint32_t>(v.position.y));
        h.update_u64(std::bit_cast<uint32_t>(v.position.z));
        h.update_u64(std::bit_cast<uint32_t>(v.normal.x));
        h.update_u64(std::bit_cast<uint32_t>(v.normal.y));
        h.update_u64(std::bit_cast<uint32_t>(v.normal.z));
        h.update_u64(std::bit_cast<uint32_t>(v.uv.x));
        h.update_u64(std::bit_cast<uint32_t>(v.uv.y));
        h.update_u64(v.color_rgba);
    }
    h.update_u64(mesh.indices.size());
    for (const uint32_t i : mesh.indices) {
        h.update_u64(i);
    }
}

} // namespace

uint64_t object_content_hash(const RegistryObject& obj) {
    serialization::Fnv1a64 h;
    hash_stream(h, obj.wood);
    hash_stream(h, obj.cards);
    hash_stream(h, obj.ground);
    return h.digest();
}

bool write_object(const RegistryObject& obj, const std::filesystem::path& path) {
    if (obj.wood.vertices.empty() && obj.cards.vertices.empty()
        && obj.ground.vertices.empty()) {
        std::fprintf(stderr, "[dfo] \"%s\": refusing to write an object with no "
                             "streams -- a name pointing at nothing\n",
                     obj.name.c_str());
        return false;
    }
    serialization::BinaryWriter w;
    w.begin_file(OBJECT_MAGIC, OBJECT_FORMAT_VERSION);
    w.begin_section(section::INFO, SECTION_VERSION);
    w.write_string(obj.name);
    w.write_string(obj.kind);
    w.write_string(obj.source);
    w.write_u64(object_content_hash(obj));
    w.end_section();
    write_stream(w, section::WOOD, obj.wood);
    write_stream(w, section::CARD, obj.cards);
    write_stream(w, section::GRND, obj.ground);
    if (!w.ok()) {
        return false;
    }
    return w.save_to_file(path);
}

std::optional<RegistryObject> read_object(const std::filesystem::path& path) {
    serialization::BinaryReader r;
    if (!r.open_file(path, OBJECT_MAGIC)) {
        return std::nullopt;
    }
    if (r.container_version() > OBJECT_FORMAT_VERSION) {
        return std::nullopt; // newer than this build understands: refused
    }
    RegistryObject obj;
    uint64_t stored_hash = 0;
    bool streams_ok = true;
    while (const auto s = r.next_section()) {
        if (s->tag == section::INFO) {
            obj.name = r.read_string();
            obj.kind = r.read_string();
            obj.source = r.read_string();
            stored_hash = r.read_u64();
        } else if (s->tag == section::WOOD) {
            streams_ok = read_stream(r, obj.wood) && streams_ok;
        } else if (s->tag == section::CARD) {
            streams_ok = read_stream(r, obj.cards) && streams_ok;
        } else if (s->tag == section::GRND) {
            streams_ok = read_stream(r, obj.ground) && streams_ok;
        }
        // Unknown tags: next_section() steps over them (Rule 7).
    }
    if (!r.ok() || !streams_ok) {
        return std::nullopt;
    }
    // THE HASH IS VERIFIED ON EVERY READ. A registry is an index of
    // identities; an object whose bytes disagree with its stored identity is
    // refused whole, because "mostly the object you asked for" is not a thing
    // a registry can return and stay a registry.
    obj.content_hash = object_content_hash(obj);
    if (obj.content_hash != stored_hash) {
        std::fprintf(stderr, "[dfo] \"%s\": content hash mismatch (stored %llx, "
                             "computed %llx) -- REFUSED\n",
                     path.string().c_str(),
                     static_cast<unsigned long long>(stored_hash),
                     static_cast<unsigned long long>(obj.content_hash));
        return std::nullopt;
    }
    return obj;
}

} // namespace dfn::render
