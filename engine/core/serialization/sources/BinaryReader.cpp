/*
Created: 10:08:2026 - 21:05:44
Last updated: 10:08:2026 - 21:05:44
Module: engine/core/serialization
File: engine/core/serialization/sources/BinaryReader.cpp

Responsibility:
- Implements the Rule 7 container reader declared in BinaryReader.h: magic
  validation, section iteration with mandatory skip-unknown, bounds-checked
  little-endian primitive reads that fail soft.

Key items:
- BinaryReader::open / open_file / container_version / next_section /
  section_bytes_remaining, read_* primitives, ok().

Dependencies:
- Uses: BinaryReader.h, BinaryWriter.h (SectionTag), std (bit, fstream).
- Used by: dfn_core consumers — gameplay save sections, world file reader,
  SaveDeltaCodec, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Skip-unknown is mandatory (Rule 7): an unrecognised tag is data to STEP OVER,
  never an error. That is the whole reason a section carries its byte length,
  and it is what lets an old build load a file a newer one wrote.
- Corrupt input must fail soft. Nothing in this file may throw, index out of
  range, or trust a length read from the data before checking it against the
  buffer size — every length in the file is attacker/corruption controlled.
- There is exactly one place bytes become integers (take_le). Do not add a
  read-a-whole-struct path.

TWO OF THE THREE CONTRACT DECISIONS ruled by the lead live here (the third,
writes outside a section, is in BinaryWriter.cpp — the two files define one
format between them):

2. A READ OUTSIDE A SECTION LATCHES !ok(), INCLUDING BEFORE THE FIRST
   next_section(). A caller who forgets the section loop would otherwise read
   the file header as payload and get plausible-looking garbage, which is
   diagnosed three sessions later as "the save is corrupt". After open(), the
   cursor and the section end are deliberately equal, so any read before the
   first next_section() under-runs and latches exactly like a truncation.

3. container_version IS OPAQUE TO CORE — documented at the accessor below.
*/
/*
UPD:
- 10:08:2026 - 21:05:44: Stage-2 implementation, written by sim under an
  explicit lead carve of core's zone (Rule 25), together with BinaryWriter.cpp.
*/

#include "engine/core/serialization/sources/BinaryReader.h"

#include <bit>
#include <cstring>
#include <fstream>

namespace dfn::serialization {

namespace {

constexpr std::size_t FILE_HEADER_BYTES = 8;     // magic u32 + container version u32
constexpr std::size_t SECTION_HEADER_BYTES = 14; // tag u32 + version u16 + length u64

/// The one place bytes become integers: `count` bytes at `offset`, least
/// significant first. The caller has already bounds-checked the range.
[[nodiscard]] uint64_t take_le(std::span<const std::byte> data, std::size_t offset,
                               std::size_t count) {
    uint64_t value = 0;
    for (std::size_t i = 0; i < count; ++i) {
        value |= static_cast<uint64_t>(std::to_integer<uint8_t>(data[offset + i]))
              << (8 * i);
    }
    return value;
}

} // namespace

BinaryReader::BinaryReader() = default;

bool BinaryReader::open(std::span<const std::byte> data, uint32_t expected_magic) {
    // Note: owned_ is intentionally NOT cleared — open_file() fills it and then
    // calls this with a span INTO it.
    data_ = data;
    cursor_ = 0;
    section_end_ = 0;
    container_version_ = 0;
    ok_ = false;

    if (data_.size() < FILE_HEADER_BYTES) {
        return false; // truncated below the header
    }
    if (static_cast<uint32_t>(take_le(data_, 0, 4)) != expected_magic) {
        return false; // not our format, or not this file's format
    }
    container_version_ = static_cast<uint32_t>(take_le(data_, 4, 4));
    cursor_ = FILE_HEADER_BYTES;
    // Decision 2: cursor == section_end means "no section open", so a read
    // taken before the first next_section() under-runs and latches.
    section_end_ = FILE_HEADER_BYTES;
    ok_ = true;
    return true;
}

bool BinaryReader::open_file(const std::filesystem::path& path,
                             uint32_t expected_magic) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        ok_ = false;
        return false;
    }
    const std::streamoff size = in.tellg();
    if (size < 0) {
        ok_ = false;
        return false;
    }
    in.seekg(0, std::ios::beg);
    owned_.assign(static_cast<std::size_t>(size), std::byte{0});
    if (size > 0) {
        in.read(reinterpret_cast<char*>(owned_.data()),
                static_cast<std::streamsize>(size));
        if (in.gcount() != static_cast<std::streamsize>(size)) {
            ok_ = false;
            return false;
        }
    }
    return open(std::span<const std::byte>(owned_.data(), owned_.size()),
                expected_magic);
}

uint32_t BinaryReader::container_version() const {
    // DECISION 3 (lead ruling): this number is OPAQUE TO CORE. The container
    // deliberately does not know what its sections mean, so it cannot decide
    // whether a given version is loadable — that judgement belongs to whoever
    // owns the sections, and each section carries its own u16 version for
    // exactly that purpose (see the stored_version parameter on every section
    // reader). A container that range-checked this field would have to be
    // edited every time any section anywhere changed, which is a shadow copy of
    // every section's version list living in core (Rule 39's shape, built into
    // a file format).
    //
    // What it IS for: identifying the LAYOUT OF THE CONTAINER ITSELF — the
    // header and section framing implemented in these two files. It changes
    // only if that framing changes, which should be approximately never.
    // What it is NOT for: gating content migration. Migration lives in the
    // section readers, keyed on the section version.
    return container_version_;
}

std::optional<SectionInfo> BinaryReader::next_section() {
    if (!ok_) {
        return std::nullopt;
    }
    // Skip whatever the caller did not read. THIS is skip-unknown (Rule 7): an
    // unrecognised tag needs no special handling, because the caller simply
    // reads nothing and the next call steps over the whole payload.
    if (section_end_ > cursor_) {
        cursor_ = section_end_;
    }
    if (cursor_ == data_.size()) {
        return std::nullopt; // clean end of file
    }
    if (data_.size() - cursor_ < SECTION_HEADER_BYTES) {
        ok_ = false; // trailing bytes that cannot be a section header
        return std::nullopt;
    }

    SectionInfo info;
    info.tag = static_cast<SectionTag>(take_le(data_, cursor_, 4));
    info.version = static_cast<uint16_t>(take_le(data_, cursor_ + 4, 2));
    info.byte_length = take_le(data_, cursor_ + 6, 8);
    const std::size_t payload_begin = cursor_ + SECTION_HEADER_BYTES;

    // The length comes from the data, so it is checked against the buffer
    // before it is trusted — and checked in a form that cannot itself overflow.
    if (info.byte_length > static_cast<uint64_t>(data_.size() - payload_begin)) {
        ok_ = false; // truncated file, or a corrupted length
        return std::nullopt;
    }

    cursor_ = payload_begin;
    section_end_ = payload_begin + static_cast<std::size_t>(info.byte_length);
    return info;
}

uint64_t BinaryReader::section_bytes_remaining() const {
    if (!ok_ || section_end_ <= cursor_) {
        return 0;
    }
    return static_cast<uint64_t>(section_end_ - cursor_);
}

// --- Primitives --------------------------------------------------------------
//
// Every read is bounds-checked against the CURRENT SECTION, not against the
// file: a section that claims fewer bytes than its reader expects must fail,
// even when the bytes of the next section happen to sit right behind it.

namespace {

/// Shared gate for the primitive reads. Latches !ok and yields 0 when the
/// current section cannot supply `count` more bytes — which covers truncation,
/// an over-reading section reader, and a read taken outside any section. The
/// return value is informational; callers act on ok() instead, which is the
/// contract the header states (check once per section, not once per field).
bool take(bool& ok, std::size_t& cursor, std::size_t section_end,
          std::size_t count, uint64_t& out, std::span<const std::byte> data) {
    if (!ok || cursor + count > section_end || section_end > data.size()) {
        ok = false;
        out = 0;
        return false;
    }
    out = take_le(data, cursor, count);
    cursor += count;
    return true;
}

} // namespace

uint8_t BinaryReader::read_u8() {
    uint64_t v = 0;
    take(ok_, cursor_, section_end_, 1, v, data_);
    return static_cast<uint8_t>(v);
}

uint16_t BinaryReader::read_u16() {
    uint64_t v = 0;
    take(ok_, cursor_, section_end_, 2, v, data_);
    return static_cast<uint16_t>(v);
}

uint32_t BinaryReader::read_u32() {
    uint64_t v = 0;
    take(ok_, cursor_, section_end_, 4, v, data_);
    return static_cast<uint32_t>(v);
}

uint64_t BinaryReader::read_u64() {
    uint64_t v = 0;
    take(ok_, cursor_, section_end_, 8, v, data_);
    return v;
}

int8_t BinaryReader::read_i8() { return static_cast<int8_t>(read_u8()); }
int16_t BinaryReader::read_i16() { return static_cast<int16_t>(read_u16()); }
int32_t BinaryReader::read_i32() { return static_cast<int32_t>(read_u32()); }
int64_t BinaryReader::read_i64() { return static_cast<int64_t>(read_u64()); }

float BinaryReader::read_f32() { return std::bit_cast<float>(read_u32()); }
double BinaryReader::read_f64() { return std::bit_cast<double>(read_u64()); }

bool BinaryReader::read_bool() {
    // Any non-zero byte reads as true. A stricter rule (only 0 and 1 are legal)
    // would reject files a future writer is still allowed to produce, and the
    // writer already normalises to 0/1.
    return read_u8() != 0;
}

void BinaryReader::read_bytes(std::span<std::byte> out) {
    // Order matters: the cursor/end comparison guards the subtraction below it
    // from wrapping (both are unsigned).
    if (!ok_ || cursor_ > section_end_ || out.size() > section_end_ - cursor_) {
        ok_ = false;
        std::memset(out.data(), 0, out.size());
        return;
    }
    std::memcpy(out.data(), data_.data() + cursor_, out.size());
    cursor_ += out.size();
}

std::string BinaryReader::read_string() {
    const uint32_t length = read_u32();
    if (!ok_) {
        return {};
    }
    if (static_cast<uint64_t>(length) > section_bytes_remaining()) {
        ok_ = false; // a length the section cannot possibly contain
        return {};
    }
    std::string out(static_cast<std::size_t>(length), '\0');
    for (uint32_t i = 0; i < length; ++i) {
        out[i] = static_cast<char>(std::to_integer<unsigned char>(data_[cursor_ + i]));
    }
    cursor_ += length;
    return out;
}

bool BinaryReader::ok() const { return ok_; }

} // namespace dfn::serialization
