/*
Created: 09:08:2026 - 00:16:55
Last updated: 10:08:2026 - 21:17:56
Module: engine/core/serialization
File: engine/core/serialization/sources/BinaryWriter.h

Responsibility:
- Section-based binary writer implementing the Rule 7 format discipline: magic +
  version header, tagged length-prefixed sections, explicit little-endian
  primitives. Never memcpy of whole structs.

Key items:
- SectionTag / make_tag: FourCC section identifiers.
- BinaryWriter: header/section/primitive writing into a growable buffer.

Dependencies:
- Uses: std only.
- Used by: world file writer (worldgen tool), SaveDeltaCodec, gameplay save
  sections (via SaveSectionHooks), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7 is absolute: every multi-byte value goes through the explicit
  little-endian write_* calls; adding a "write raw struct" method is a violation.
- STAGE 1 CONTRACT: declarations only; bodies arrive in stage 2.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — section writer per Rule 7 (Q49).
- 10:08:2026 - 21:17:56: ok() added (lead-approved contract change, made
  while implementing the bodies). The class had no way to report that it had
  been misused, so the implementation's failure latch had to ride a sentinel
  value inside open_section_length_offset_ — a private member silently carrying
  a second meaning, which is the shape of half the defects found in this repo.
  Rule 26 protects a contract from casual change, not from one that turned out
  to be incomplete, and "the writer cannot tell you it failed" is incomplete.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace dfn::serialization {

/// FourCC section tag, e.g. make_tag("CHNK"). Stored little-endian like every
/// other u32; tags are format constants, never derived from TypeId (which is
/// not stable across runs).
using SectionTag = uint32_t;

/// Builds a tag from exactly four ASCII characters.
[[nodiscard]] constexpr SectionTag make_tag(char a, char b, char c, char d) {
    return static_cast<uint32_t>(static_cast<unsigned char>(a))
         | (static_cast<uint32_t>(static_cast<unsigned char>(b)) << 8)
         | (static_cast<uint32_t>(static_cast<unsigned char>(c)) << 16)
         | (static_cast<uint32_t>(static_cast<unsigned char>(d)) << 24);
}

/// Writes the on-disk container format (Rule 7, Q49):
///
///   file   := magic:u32  container_version:u32  section*
///   section:= tag:u32  section_version:u16  byte_length:u64  payload[byte_length]
///
/// All integers little-endian, written byte by byte — the format is identical on
/// every platform. Sections may nest logically inside a payload by reusing a
/// writer, but the container itself is a flat section sequence.
///
/// Usage:
///   BinaryWriter w;
///   w.begin_file(WORLD_MAGIC, WORLD_FORMAT_VERSION);
///   w.begin_section(make_tag('I','N','F','O'), 1);
///   w.write_u64(seed);
///   w.end_section();          // patches byte_length
///   w.save_to_file(path);     // atomic: temp file + rename
class BinaryWriter {
public:
    BinaryWriter();

    /// Writes the container header. Must be the first call.
    void begin_file(uint32_t magic, uint32_t container_version);

    /// Opens a section; every write until end_section() lands in its payload.
    /// Sections cannot overlap (no begin inside an open section).
    void begin_section(SectionTag tag, uint16_t section_version);

    /// Closes the current section and back-patches its byte length.
    void end_section();

    // Primitives — explicit little-endian, byte order independent of the host.
    void write_u8(uint8_t v);
    void write_u16(uint16_t v);
    void write_u32(uint32_t v);
    void write_u64(uint64_t v);
    void write_i8(int8_t v);
    void write_i16(int16_t v);
    void write_i32(int32_t v);
    void write_i64(int64_t v);
    void write_f32(float v);   // IEEE-754 bits as u32
    void write_f64(double v);  // IEEE-754 bits as u64
    void write_bool(bool v);   // one byte, 0 or 1

    /// Raw byte run (already-encoded payloads, e.g. heightmap sample arrays
    /// written element-wise elsewhere, or opus blobs).
    void write_bytes(std::span<const std::byte> bytes);

    /// UTF-8 string: u32 byte length + bytes, no terminator.
    void write_string(std::string_view utf8);

    /// False once the writer has been MISUSED in a way the container grammar
    /// cannot express: a write with no section open, a nested or unopened
    /// section, a second file header. Such bytes are dropped rather than
    /// appended, because appending them would produce a file that still LOADS
    /// and is quietly wrong — a save format is the worst possible place to let
    /// absence look like a neutral result. The latch is sticky: once false,
    /// every later call is refused and save_to_file() can never succeed.
    /// buffer() stays readable so a caller can inspect what it built.
    [[nodiscard]] bool ok() const;

    /// The finished buffer. Valid only after all sections are closed.
    [[nodiscard]] std::span<const std::byte> buffer() const;

    /// Writes buffer() atomically (temp file in the target directory + rename).
    /// Returns false on IO failure. All sections must be closed.
    [[nodiscard]] bool save_to_file(const std::filesystem::path& path) const;

private:
    std::vector<std::byte> buffer_;
    std::size_t open_section_length_offset_ = 0; // 0 = no open section
    bool header_written_ = false;
    bool failed_ = false;                        // sticky misuse latch, see ok()
};

} // namespace dfn::serialization
