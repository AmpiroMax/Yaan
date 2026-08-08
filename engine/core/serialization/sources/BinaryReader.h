/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/serialization
File: engine/core/serialization/sources/BinaryReader.h

Responsibility:
- Section-based binary reader for the Rule 7 container format: validates magic +
  version, iterates tagged length-prefixed sections, skips unknown sections,
  reads explicit little-endian primitives with bounds checking.

Key items:
- SectionInfo: tag + version + size of the current section.
- BinaryReader: header validation, section iteration, primitive reads.

Dependencies:
- Uses: std, SectionTag (BinaryWriter.h).
- Used by: world file reader, SaveDeltaCodec, gameplay save sections, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Skip-unknown is mandatory (Rule 7): an unrecognized tag is data to step over,
  never an error. Corrupt data must fail soft (ok() == false), never crash.
- STAGE 1 CONTRACT: declarations only; bodies arrive in stage 2.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — section reader per Rule 7 (Q49) with
  skip-unknown and bounds-checked reads.
*/

#pragma once

#include "engine/core/serialization/sources/BinaryWriter.h" // SectionTag

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dfn::serialization {

/// Metadata of the section the reader is currently positioned at.
struct SectionInfo {
    SectionTag tag = 0;
    uint16_t version = 0;
    uint64_t byte_length = 0;
};

/// Reads the container format written by BinaryWriter (see its header for the
/// layout). All reads are bounds-checked: on any violation (truncated file, read
/// past section end, bad magic) the reader latches ok() == false and every
/// subsequent read returns zero/empty — callers check ok() once at the end of a
/// section rather than after every call.
///
/// Usage:
///   BinaryReader r;
///   if (!r.open_file(path, WORLD_MAGIC) ) ...;
///   while (auto s = r.next_section()) {
///       switch (s->tag) {
///       case INFO_TAG: seed = r.read_u64(); break;
///       default: break;                    // unknown: next_section() skips it
///       }
///   }
///   if (!r.ok()) ...;
class BinaryReader {
public:
    BinaryReader();

    /// Borrows `data` (caller keeps it alive) and validates magic. Returns false
    /// on bad magic/truncation. The container version is NOT range-checked here;
    /// callers decide migration (Rule 7: migration functions from day one).
    [[nodiscard]] bool open(std::span<const std::byte> data, uint32_t expected_magic);

    /// Reads the whole file into an internal buffer, then open(). False on IO
    /// failure or bad magic.
    [[nodiscard]] bool open_file(const std::filesystem::path& path, uint32_t expected_magic);

    /// Container version from the header (valid after a successful open).
    [[nodiscard]] uint32_t container_version() const;

    /// Advances to the next section: skips any unread remainder of the current
    /// section (this is how unknown sections are skipped), then returns the next
    /// section's info, or nullopt at end of file.
    [[nodiscard]] std::optional<SectionInfo> next_section();

    /// Bytes of the current section's payload not yet consumed.
    [[nodiscard]] uint64_t section_bytes_remaining() const;

    // Primitives — explicit little-endian, bounds-checked against the current
    // section. On under-run: latch !ok(), return 0/empty.
    [[nodiscard]] uint8_t read_u8();
    [[nodiscard]] uint16_t read_u16();
    [[nodiscard]] uint32_t read_u32();
    [[nodiscard]] uint64_t read_u64();
    [[nodiscard]] int8_t read_i8();
    [[nodiscard]] int16_t read_i16();
    [[nodiscard]] int32_t read_i32();
    [[nodiscard]] int64_t read_i64();
    [[nodiscard]] float read_f32();
    [[nodiscard]] double read_f64();
    [[nodiscard]] bool read_bool();

    /// Reads exactly `out.size()` bytes into `out` (or latches !ok()).
    void read_bytes(std::span<std::byte> out);

    /// Reads a write_string() string: u32 byte length + UTF-8 bytes.
    [[nodiscard]] std::string read_string();

    /// False after any bounds/magic/format violation. Reset only by open().
    [[nodiscard]] bool ok() const;

private:
    std::vector<std::byte> owned_;      // filled by open_file
    std::span<const std::byte> data_;   // whole container
    std::size_t cursor_ = 0;
    std::size_t section_end_ = 0;       // absolute end of current section payload
    uint32_t container_version_ = 0;
    bool ok_ = false;
};

} // namespace dfn::serialization
