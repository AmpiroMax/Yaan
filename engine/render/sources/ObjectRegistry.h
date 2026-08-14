/*
Created: 14:08:2026 - 23:36:19
Last updated: 15:08:2026 - 01:46:53
Module: engine/render
File: engine/render/sources/ObjectRegistry.h

Responsibility:
- The OBJECT REGISTRY's file format (.dfo) and its read/write API: a baked,
  named render object — mesh streams plus identity — written offline by a
  forge tool and only READ by the game (в1: nothing is generated in the frame).
  This is the third tool of the tooling pivot, after the map browser and the
  world baker.

Key items:
- RegistryObject: name/kind/source + the three mesh streams + content hash.
- write_object() / read_object(): one object per .dfo file.
- object_content_hash(): the FROZEN fnv1a64 identity of the payload.

Dependencies:
- Uses: ProcFlora.h (MeshData streams), core serialization (BinaryWriter/
  BinaryReader, Fnv1a64).
- Used by: tools/forge_trees.cpp (writer), engine/app gallery loading (reader),
  tests/render/ObjectRegistryTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7 in full: explicit little-endian through BinaryWriter/Reader, unknown
  sections skipped, never a struct memcpy.
- THE CONTENT HASH IS THE OBJECT'S IDENTITY and fnv1a64 is frozen forever
  (user-ratified). Two objects with equal hashes are THE SAME OBJECT; a tool
  that re-bakes an unchanged tree must produce an unchanged hash, which is why
  the hash covers the PAYLOAD (streams, in order) and not the name — renaming
  a file must not re-version every reference to its content.
- A corrupt or truncated file fails SOFT (nullopt), never half-loads: an
  object missing its wood stream is not a lighter object, it is a different
  one wearing the same name.
*/
/*
UPD:
- 14:08:2026 - 23:36:19: Created — the .dfo container (user: «инструмент,
  который будет деревья делать и их сохранять, как мы обсуждали и
  договаривались, в реестр объектов»).
- 15:08:2026 - 01:46:53: bark stream + формат v2 (секция BARK; хэш
  версионирован: файл v1 сверяется по правилу v1, без bark).
*/

#pragma once

#include "engine/render/sources/ProcFlora.h" // MeshData

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace dfn::render {

/// One baked object of the registry. The three streams mirror FloraMesh on
/// purpose: `wood` draws with the "prop" program, `cards` with "foliage" plus
/// the leaf atlas, `ground` draws with the wood and never reaches collision —
/// an object that needs different streams is a different KIND, not a fourth
/// vector on this one.
struct RegistryObject {
    std::string name;   ///< human handle, e.g. "oak-forge-v1-a" (not identity)
    std::string kind;   ///< "tree" today; the registry is not tree-shaped
    std::string source; ///< what produced it, e.g. "forge:oak seed=3" (provenance)
    MeshData wood;
    MeshData cards;
    MeshData ground;
    /// TEXTURED wood: trunk and heavy limbs with bark UVs into the leaf
    /// atlas' BarkPlate column. Drawn with the foliage program (albedo from
    /// the texture, real lighting), wind zeroed — a separate stream because
    /// the plain "prop" wood has no UVs and collision reads neither.
    MeshData bark;
    /// fnv1a64 over the payload streams (see object_content_hash). Stored in
    /// the file AND recomputed on read; a mismatch is a refused file, because
    /// a registry whose identities cannot be trusted indexes nothing.
    uint64_t content_hash = 0;
};

/// The payload identity: every stream, vertices then indices, in file order.
/// Name/kind/source are NOT hashed — provenance may be re-worded without
/// changing what the object IS.
[[nodiscard]] uint64_t object_content_hash(const RegistryObject& obj);

/// Writes one object as a .dfo (atomic: temp + rename). Computes and stores
/// the content hash; returns false on IO failure or an empty object (an
/// object with no streams at all is a name pointing at nothing, refused).
[[nodiscard]] bool write_object(const RegistryObject& obj,
                                const std::filesystem::path& path);

/// Reads one .dfo. nullopt on bad magic, truncation, unknown newer version,
/// or a content hash that does not match the streams.
[[nodiscard]] std::optional<RegistryObject> read_object(const std::filesystem::path& path);

} // namespace dfn::render
