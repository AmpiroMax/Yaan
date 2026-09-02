/*
Module: engine/core/serialization
File: engine/core/serialization/sources/Sha256.h

Responsibility:
- SHA-256 (FIPS 180-4) over a byte run, and its lowercase hex spelling. It is
  the IDENTITY OF A TEXTURE FILE named from a .dfo (section TEX, character
  skin wave): the importer stores the digest of the PNG it saw, the loader
  recomputes it over the PNG it finds, and a mismatch is a refused texture.

Key items:
- sha256(): one-shot digest of a byte span, 32 bytes.
- sha256_hex(): the same as 64 lowercase hex characters (what SHA256SUMS
  files and the TEX section carry).
- sha256_file(): digest of a file's bytes; nullopt when the file cannot be
  read.

Dependencies:
- Uses: std only.
- Used by: tools/import_gltf (writes the digest), engine/app SkinnedCharacter
  (verifies it), tests/core/Sha256Tests.cpp.

Notes:
- WHY NOT fnv1a64 (ContentHash.h). The 64-bit FNV names things this project
  made itself; a texture arrives from OUTSIDE the tree (MPFB, an artist's
  export) next to a SHA256SUMS file its author wrote with a standard tool,
  and the digest in the .dfo must be comparable with that file by eye and by
  `shasum -a 256`. A second, private hash would make "is this the PNG the
  bake saw" a question only our own binary can answer.
- Not a security boundary: it verifies that a file is the one that was
  baked against, nothing more, so a straightforward reference implementation
  is the whole of it and speed is irrelevant (a 4 MB skin hashes in ~10 ms).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ALGORITHM FROZEN: the digest lives in baked files and in SHA256SUMS written
  by other tools. There is nothing to tune here.
*/

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace dfn::serialization {

using Sha256Digest = std::array<uint8_t, 32>;

/// SHA-256 of `bytes`.
[[nodiscard]] Sha256Digest sha256(std::span<const uint8_t> bytes);

/// 64 lowercase hex characters of a digest.
[[nodiscard]] std::string sha256_hex(const Sha256Digest& digest);

/// sha256_hex(sha256(bytes)).
[[nodiscard]] std::string sha256_hex(std::span<const uint8_t> bytes);

/// Digest of a file's bytes as hex; nullopt when the file cannot be opened
/// or read in full.
[[nodiscard]] std::optional<std::string> sha256_file(const std::filesystem::path& path);

} // namespace dfn::serialization
