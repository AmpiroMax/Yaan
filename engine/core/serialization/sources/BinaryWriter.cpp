/*
Module: engine/core/serialization
File: engine/core/serialization/sources/BinaryWriter.cpp

Responsibility:
- Implements the Rule 7 container writer declared in BinaryWriter.h: magic +
  version header, tagged length-prefixed sections with back-patched lengths,
  explicit little-endian primitives, atomic save_to_file.

Key items:
- BinaryWriter::begin_file / begin_section / end_section, write_* primitives,
  buffer(), save_to_file().

Dependencies:
- Uses: BinaryWriter.h, std (bit, filesystem, fstream, system_error).
- Used by: dfn_core consumers — gameplay save sections, world file writer,
  SaveDeltaCodec, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7 is absolute: every multi-byte value is emitted byte by byte here.
  There is exactly one place integers become bytes (put_le) and one place raw
  runs are appended (write_bytes); adding a write-a-whole-struct path is a
  violation, and floats must keep going through the integer path.

THE THREE CONTRACT DECISIONS (raised with the lead because the declaration did
not settle them; ruled on and recorded here rather than only in git, since the
next reader hits the edge case in this file and not in a commit log):

1. A WRITE OUTSIDE AN OPEN SECTION IS A LATCHED FAILURE, NEVER AN APPEND.
   The container grammar has nowhere to put such bytes. Appending them would
   produce a file that still LOADS and is subtly wrong — the exact failure mode
   this project keeps paying for, where absence is indistinguishable from a
   neutral result. So misuse poisons the writer: the bytes are dropped, every
   later call is refused, and save_to_file() returns false forever after. The
   already-written buffer() stays readable so a caller can still inspect what it
   built. The latch is the declared ok(). It first rode a sentinel value inside
   open_section_length_offset_, because the header was frozen and had no error
   channel at all; the lead approved widening the contract instead, on the
   grounds that Rule 26 protects a contract from casual change and not from one
   that turned out to be incomplete — and a private member silently carrying a
   second meaning is a defect lying in wait for its next reader.

2. THE READER LATCHES !ok() ON READS OUTSIDE A SECTION, including a read before
   the first next_section(). Enforced in BinaryReader.cpp; noted here because
   the two files define one format between them.

3. container_version IS OPAQUE TO CORE. See BinaryReader::container_version().
*/

#include "engine/core/serialization/sources/BinaryWriter.h"

#include <bit>
#include <fstream>
#include <system_error>

namespace dfn::serialization {

namespace {

/// The one place integers become bytes: `count` bytes of `value`, least
/// significant first. Identical output on a big-endian host (Rule 7).
void put_le(std::vector<std::byte>& out, uint64_t value, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFull));
    }
}

/// Overwrites `count` bytes already in the buffer, same byte order. Used only
/// to back-patch a section's length once its payload size is known.
void patch_le(std::vector<std::byte>& out, std::size_t offset, uint64_t value,
              std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        out[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFull);
    }
}

} // namespace

BinaryWriter::BinaryWriter() = default;

void BinaryWriter::begin_file(uint32_t magic, uint32_t container_version) {
    if (failed_) {
        return;
    }
    if (header_written_) {
        // A second header would sit inside the file as unparseable bytes and
        // make every section after it unreachable. Decision 1: latch.
        failed_ = true;
        return;
    }
    put_le(buffer_, magic, 4);
    put_le(buffer_, container_version, 4);
    header_written_ = true;
}

void BinaryWriter::begin_section(SectionTag tag, uint16_t section_version) {
    if (failed_) {
        return;
    }
    if (!header_written_ || open_section_length_offset_ != 0) {
        // No header yet, or sections would overlap — the header forbids both.
        failed_ = true;
        return;
    }
    put_le(buffer_, tag, 4);
    put_le(buffer_, section_version, 2);
    // The length is a placeholder patched by end_section(). Its offset doubles
    // as the "a section is open" flag, which is why 0 is a safe idle sentinel:
    // the first possible length offset is 8 (file header) + 6 (tag+version).
    open_section_length_offset_ = buffer_.size();
    put_le(buffer_, 0, 8);
}

void BinaryWriter::end_section() {
    if (failed_) {
        return;
    }
    if (open_section_length_offset_ == 0) {
        failed_ = true; // end_section without begin_section
        return;
    }
    const std::size_t payload_begin = open_section_length_offset_ + 8;
    patch_le(buffer_, open_section_length_offset_,
             static_cast<uint64_t>(buffer_.size() - payload_begin), 8);
    open_section_length_offset_ = 0;
}

// --- Primitives --------------------------------------------------------------
//
// Every one funnels into put_le, so the engine has a single definition of the
// on-disk byte order. Each first asks whether a section is open (decision 1).

void BinaryWriter::write_u8(uint8_t v) {
    if (failed_ || open_section_length_offset_ == 0) {
        failed_ = true;
        return;
    }
    put_le(buffer_, v, 1);
}

void BinaryWriter::write_u16(uint16_t v) {
    if (failed_ || open_section_length_offset_ == 0) {
        failed_ = true;
        return;
    }
    put_le(buffer_, v, 2);
}

void BinaryWriter::write_u32(uint32_t v) {
    if (failed_ || open_section_length_offset_ == 0) {
        failed_ = true;
        return;
    }
    put_le(buffer_, v, 4);
}

void BinaryWriter::write_u64(uint64_t v) {
    if (failed_ || open_section_length_offset_ == 0) {
        failed_ = true;
        return;
    }
    put_le(buffer_, v, 8);
}

// Signed values are written as their two's-complement bit pattern, which is
// exactly what the matching read_i* reinterprets. C++20 fixes two's complement,
// so this is a definition rather than an assumption about the host.
void BinaryWriter::write_i8(int8_t v) { write_u8(static_cast<uint8_t>(v)); }
void BinaryWriter::write_i16(int16_t v) { write_u16(static_cast<uint16_t>(v)); }
void BinaryWriter::write_i32(int32_t v) { write_u32(static_cast<uint32_t>(v)); }
void BinaryWriter::write_i64(int64_t v) { write_u64(static_cast<uint64_t>(v)); }

// Floats reach disk through the integer path, so the stored form is IEEE-754
// little-endian regardless of the host's float ABI.
void BinaryWriter::write_f32(float v) { write_u32(std::bit_cast<uint32_t>(v)); }
void BinaryWriter::write_f64(double v) { write_u64(std::bit_cast<uint64_t>(v)); }

void BinaryWriter::write_bool(bool v) { write_u8(v ? uint8_t{1} : uint8_t{0}); }

void BinaryWriter::write_bytes(std::span<const std::byte> bytes) {
    if (failed_ || open_section_length_offset_ == 0) {
        failed_ = true;
        return;
    }
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
}

void BinaryWriter::write_string(std::string_view utf8) {
    if (failed_ || open_section_length_offset_ == 0) {
        failed_ = true;
        return;
    }
    put_le(buffer_, static_cast<uint64_t>(utf8.size()), 4);
    for (const char c : utf8) {
        buffer_.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
    }
}

std::span<const std::byte> BinaryWriter::buffer() const {
    // Deliberately still valid after a latch: the bytes written before the
    // misuse are real, and a caller inspecting them is how the misuse gets
    // diagnosed. What a poisoned writer must never do is produce a FILE.
    return std::span<const std::byte>(buffer_.data(), buffer_.size());
}

bool BinaryWriter::ok() const { return !failed_; }

bool BinaryWriter::save_to_file(const std::filesystem::path& path) const {
    if (failed_ || open_section_length_offset_ != 0) {
        // Misused, or a section is still open — an unpatched length would
        // record a zero-byte section and silently swallow its payload.
        return false;
    }
    if (!header_written_) {
        return false; // no magic: nothing could ever open this file
    }

    // Atomic in the sense that matters for a save: a reader never observes a
    // half-written file at `path`. The temp file is created in the SAME
    // directory so the rename stays within one filesystem and cannot degrade
    // into a non-atomic copy.
    std::filesystem::path directory = path.parent_path();
    if (directory.empty()) {
        directory = std::filesystem::path(".");
    }
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);

    const std::filesystem::path temp = directory / (path.filename().string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        if (!buffer_.empty()) {
            out.write(reinterpret_cast<const char*>(buffer_.data()),
                      static_cast<std::streamsize>(buffer_.size()));
        }
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(temp, ec);
            return false;
        }
    }

    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}

} // namespace dfn::serialization
