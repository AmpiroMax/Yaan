/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
Module: engine/core/serialization
File: engine/core/serialization/sources/ContentHash.cpp

Responsibility:
- FNV-1a 64 implementation (one-shot + streaming). Frozen algorithm (Q79).

Key items:
- fnv1a64 overloads, Fnv1a64 accumulator methods.

Dependencies:
- Uses: ContentHash.h.
- Used by: dfn_core; worldgen determinism test hashes heights through this.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ALGORITHM FROZEN FOREVER (see header). Do not modify constants or byte order.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — implementation (built ahead of the rest of
  the serialization module because the Rule 13.1 determinism test hashes
  through it; BinaryWriter/Reader stay deferred).
*/

#include "engine/core/serialization/sources/ContentHash.h"

namespace dfn::serialization {

namespace {
uint64_t fold(uint64_t state, const std::byte* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        state ^= static_cast<uint64_t>(std::to_integer<uint8_t>(data[i]));
        state *= FNV1A64_PRIME;
    }
    return state;
}
} // namespace

uint64_t fnv1a64(std::span<const std::byte> bytes) {
    return fold(FNV1A64_OFFSET_BASIS, bytes.data(), bytes.size());
}

uint64_t fnv1a64(std::string_view text) {
    return fold(FNV1A64_OFFSET_BASIS,
                reinterpret_cast<const std::byte*>(text.data()), text.size());
}

void Fnv1a64::update(std::span<const std::byte> bytes) {
    state_ = fold(state_, bytes.data(), bytes.size());
}

void Fnv1a64::update(std::string_view text) {
    state_ = fold(state_, reinterpret_cast<const std::byte*>(text.data()), text.size());
}

void Fnv1a64::update_u64(uint64_t v) {
    std::byte le[8];
    for (int i = 0; i < 8; ++i) {
        le[i] = static_cast<std::byte>((v >> (8 * i)) & 0xFFu);
    }
    state_ = fold(state_, le, 8);
}

void Fnv1a64::update_length_prefixed(std::string_view text) {
    update_u64(static_cast<uint64_t>(text.size()));
    update(text);
}

void Fnv1a64::update_length_prefixed(std::span<const std::byte> bytes) {
    update_u64(static_cast<uint64_t>(bytes.size()));
    update(bytes);
}

} // namespace dfn::serialization
