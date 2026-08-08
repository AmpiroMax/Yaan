/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/serialization
File: engine/core/serialization/sources/ContentHash.h

Responsibility:
- Stable, platform-independent, seedless 64-bit content hashing (FNV-1a 64).
  Names voice-segment audio files on disk (Q79/Q80) and keys asset lookups.

Key items:
- fnv1a64(): one-shot hash of a byte/string run.
- Fnv1a64: streaming accumulator for multi-field hashes.

Dependencies:
- Uses: std only.
- Used by: tools/voice_gen pipeline, gameplay dialogue (segment_content_hash),
  engine/render asset name hashes (agreed with render, Rule 26).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ALGORITHM FROZEN FOREVER: these hashes name files on disk; changing the
  algorithm, basis, or prime orphans every shipped voice file. Never "improve"
  this.
- Multi-field hashes MUST use update_length_prefixed per field (documented fixed
  field order) so field boundaries can't alias ("ab"+"c" vs "a"+"bc").
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — FNV-1a 64 one-shot + streaming
  accumulator; contract agreed with sim for the voice pipeline (Q79).
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dfn::serialization {

/// FNV-1a 64-bit parameters (frozen).
inline constexpr uint64_t FNV1A64_OFFSET_BASIS = 14695981039346656037ull;
inline constexpr uint64_t FNV1A64_PRIME = 1099511628211ull;

/// One-shot FNV-1a 64 over a byte run. Deterministic across platforms, builds,
/// and runs (processes bytes in order; no host-endianness involved).
[[nodiscard]] uint64_t fnv1a64(std::span<const std::byte> bytes);

/// One-shot FNV-1a 64 over the UTF-8 bytes of `text`.
[[nodiscard]] uint64_t fnv1a64(std::string_view text);

/// Streaming FNV-1a 64 accumulator for hashing several fields into one digest.
///
/// Contract for multi-field content identity (e.g. a voice segment: text +
/// markup + paralinguistic tag + voice id, Q79): feed every field through
/// update_length_prefixed IN A DOCUMENTED FIXED ORDER. The length prefix keeps
/// field boundaries unambiguous. Numeric fields go through update_u64 (which
/// feeds the value's 8 little-endian bytes).
class Fnv1a64 {
public:
    Fnv1a64() = default;

    void update(std::span<const std::byte> bytes);
    void update(std::string_view text);

    /// Feeds `v` as 8 little-endian bytes (host-independent).
    void update_u64(uint64_t v);

    /// Feeds the field's byte length (as update_u64) and then its bytes.
    /// The required entry point for every variable-length field of a composite
    /// content identity.
    void update_length_prefixed(std::string_view text);
    void update_length_prefixed(std::span<const std::byte> bytes);

    /// Current digest. May be read mid-stream; further updates keep folding.
    [[nodiscard]] uint64_t digest() const { return state_; }

private:
    uint64_t state_ = FNV1A64_OFFSET_BASIS;
};

} // namespace dfn::serialization
