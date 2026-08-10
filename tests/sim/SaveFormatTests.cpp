/*
Created: 10:08:2026 - 21:10:26
Last updated: 10:08:2026 - 21:18:21
Module: tests
File: tests/sim/SaveFormatTests.cpp

Responsibility:
- The save CONTAINER's guarantees, as opposed to the gameplay sections'
  (those are in InteractionTests.cpp). Byte-exact grammar and endianness, the
  committed fixture that proves a file written by an earlier build still loads,
  skip-unknown, fail-soft on corruption, and the two misuse latches the lead
  ruled on.

Key items:
- The Rule 30 control for the round trip: A DIFFERENT PAYLOAD. A reader that
  ignored the bytes and returned a fixed struct passes any single round trip;
  it cannot pass two round trips whose results must differ.

Dependencies:
- Uses: doctest, dfn_core (serialization), dfn_gameplay (inventory section).
- Used by: ctest (sim_save_format). Needs DFN_REPO_ROOT for the fixture path.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THIS SUITE BELONGS TO CORE'S ZONE BY SUBJECT (engine/core/serialization) and
  lives here only because sim implemented the IO under a lead carve. Move it to
  tests/core/ when core takes serialization back; the assertions carry over
  unchanged.
- Do not "fix" the fixture by regenerating it. If the fixture case fails, the
  container grammar changed, and that is the finding — a save file written by
  an older build no longer loads. Regenerating it deletes exactly the evidence
  the case exists to produce.
*/
/*
UPD:
- 10:08:2026 - 21:10:26: Created with the Binary IO implementation.
- 10:08:2026 - 21:18:21: The misuse cases now assert BinaryWriter::ok()
  directly (the lead approved adding it), not only that save_to_file refuses.
*/

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include "engine/core/ecs/sources/World.h"
#include "engine/core/serialization/sources/BinaryReader.h"
#include "engine/core/serialization/sources/BinaryWriter.h"
#include "engine/gameplay/sources/GameplaySave.h"
#include "engine/gameplay/sources/Inventory.h"

namespace {

namespace serialization = dfn::serialization;
namespace gameplay = dfn::gameplay;
using dfn::ecs::EntityId;

constexpr uint32_t TEST_MAGIC = serialization::make_tag('D', 'F', 'S', 'V');
constexpr serialization::SectionTag TAG_MAIN = serialization::make_tag('M', 'A', 'I', 'N');
constexpr serialization::SectionTag TAG_ALIEN = serialization::make_tag('Z', 'Z', 'Z', 'Z');

// The fixture's payload, written once and committed. Every value is a shape
// that has burned somebody in a binary format before: a u32 whose bytes are all
// different (catches a byte-order flip that a palindrome would hide), a
// negative integer (two's complement), a float with a non-trivial mantissa, an
// empty string next to a non-empty one, and both booleans.
constexpr uint32_t FIX_U32 = 0x11223344u;
constexpr int32_t FIX_I32 = -123456;
constexpr float FIX_F32 = -0.15625f; // exact in binary32, so == is legitimate
constexpr uint64_t FIX_U64 = 0x0102030405060708ull;

void write_fixture_payload(serialization::BinaryWriter& w) {
    w.begin_file(TEST_MAGIC, 1);
    w.begin_section(TAG_MAIN, 1);
    w.write_u32(FIX_U32);
    w.write_i32(FIX_I32);
    w.write_f32(FIX_F32);
    w.write_u64(FIX_U64);
    w.write_bool(true);
    w.write_bool(false);
    w.write_string("dfn");
    w.write_string("");
    w.end_section();
}

[[nodiscard]] std::vector<std::byte> read_whole_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in) {
        return {};
    }
    const auto size = static_cast<std::size_t>(in.tellg());
    in.seekg(0);
    std::vector<std::byte> bytes(size);
    if (size > 0) {
        in.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(size));
    }
    return bytes;
}

/// Reads the fixture payload back and checks every field. Shared by the
/// freshly-written case and the committed-fixture case so the two cannot drift.
void check_fixture_payload(serialization::BinaryReader& r) {
    const auto section = r.next_section();
    REQUIRE(section.has_value());
    CHECK(section->tag == TAG_MAIN);
    CHECK(section->version == 1);
    CHECK(r.read_u32() == FIX_U32);
    CHECK(r.read_i32() == FIX_I32);
    CHECK(r.read_f32() == FIX_F32);
    CHECK(r.read_u64() == FIX_U64);
    CHECK(r.read_bool() == true);
    CHECK(r.read_bool() == false);
    CHECK(r.read_string() == "dfn");
    CHECK(r.read_string() == std::string());
    CHECK(r.section_bytes_remaining() == 0);
    CHECK(r.ok());
}

[[nodiscard]] uint8_t byte_at(std::span<const std::byte> data, std::size_t i) {
    return std::to_integer<uint8_t>(data[i]);
}

} // namespace

// --- The grammar itself ------------------------------------------------------

TEST_CASE("the container's bytes are the documented grammar, least significant first") {
    serialization::BinaryWriter w;
    w.begin_file(TEST_MAGIC, 7);
    w.begin_section(TAG_MAIN, 3);
    w.write_u32(FIX_U32);
    w.end_section();
    const auto bytes = w.buffer();

    // file := magic:u32 version:u32 | section := tag:u32 ver:u16 len:u64 payload
    REQUIRE(bytes.size() == 8 + 14 + 4);

    // make_tag('D','F','S','V') puts 'D' in the LOW byte, so a little-endian
    // u32 write must put 'D' FIRST. This is the assertion that fails on a
    // big-endian host, which is the entire point of Rule 7's "written
    // explicitly": the file must not depend on the machine that wrote it.
    CHECK(byte_at(bytes, 0) == uint8_t{'D'});
    CHECK(byte_at(bytes, 1) == uint8_t{'F'});
    CHECK(byte_at(bytes, 2) == uint8_t{'S'});
    CHECK(byte_at(bytes, 3) == uint8_t{'V'});
    CHECK(byte_at(bytes, 4) == 7);
    CHECK(byte_at(bytes, 5) == 0);

    CHECK(byte_at(bytes, 8) == uint8_t{'M'});
    CHECK(byte_at(bytes, 12) == 3); // section version u16, low byte
    CHECK(byte_at(bytes, 13) == 0);
    CHECK(byte_at(bytes, 14) == 4); // payload length u64 == 4, low byte first
    CHECK(byte_at(bytes, 21) == 0); // ...and the high byte is genuinely zero

    // 0x11223344 has four distinct bytes: a byte-order flip cannot hide here.
    CHECK(byte_at(bytes, 22) == 0x44);
    CHECK(byte_at(bytes, 23) == 0x33);
    CHECK(byte_at(bytes, 24) == 0x22);
    CHECK(byte_at(bytes, 25) == 0x11);
}

TEST_CASE("a file written by an earlier build still loads byte for byte") {
    const std::filesystem::path fixture =
        std::filesystem::path(DFN_REPO_ROOT) / "tests" / "sim" / "fixtures" /
        "container_v1.bin";
    REQUIRE(std::filesystem::exists(fixture));

    // Arm 1: the committed bytes parse, field for field. This is the arm that
    // survives the day someone reorders a write.
    serialization::BinaryReader from_disk;
    REQUIRE(from_disk.open_file(fixture, TEST_MAGIC));
    CHECK(from_disk.container_version() == 1);
    check_fixture_payload(from_disk);

    // Arm 2: today's writer reproduces those bytes exactly. Arm 1 alone would
    // still pass if the writer changed and the reader changed with it — the two
    // are written by the same hand and would agree with each other on a new,
    // incompatible format. Only the committed bytes are outside that loop.
    serialization::BinaryWriter w;
    write_fixture_payload(w);
    const auto fresh = w.buffer();
    const auto stored = read_whole_file(fixture);
    REQUIRE(stored.size() == fresh.size());
    bool identical = true;
    for (std::size_t i = 0; i < stored.size(); ++i) {
        if (stored[i] != fresh[i]) {
            identical = false;
            MESSAGE("first differing byte at offset " << i);
            break;
        }
    }
    CHECK(identical);

    // Control: the same bytes must be REFUSED under a different magic. Without
    // this, "open_file returns true" could be a function that always returns
    // true and both arms above would still pass.
    serialization::BinaryReader wrong_magic;
    CHECK_FALSE(wrong_magic.open_file(fixture, serialization::make_tag('N', 'O', 'P', 'E')));
}

// --- Rule 30: the round trip's control is a DIFFERENT payload ----------------

TEST_CASE("the round trip returns the payload it was GIVEN, not a payload") {
    // Two payloads, distinct in every field. A reader that ignored the bytes
    // and returned a fixed struct would pass whichever of these it was built
    // from and fail the other, which is exactly the discrimination a single
    // round-trip case cannot make.
    struct Payload {
        uint32_t a;
        int32_t b;
        float c;
        std::string d;
    };
    const Payload first{1u, -7, 0.5f, "alpha"};
    const Payload second{4294967295u, 2147483647, -12.25f, "omega-and-then-some"};
    REQUIRE(first.a != second.a);
    REQUIRE(first.d != second.d);

    auto round_trip = [](const Payload& in) {
        serialization::BinaryWriter w;
        w.begin_file(TEST_MAGIC, 1);
        w.begin_section(TAG_MAIN, 1);
        w.write_u32(in.a);
        w.write_i32(in.b);
        w.write_f32(in.c);
        w.write_string(in.d);
        w.end_section();

        serialization::BinaryReader r;
        REQUIRE(r.open(w.buffer(), TEST_MAGIC));
        REQUIRE(r.next_section().has_value());
        Payload out{};
        out.a = r.read_u32();
        out.b = r.read_i32();
        out.c = r.read_f32();
        out.d = r.read_string();
        REQUIRE(r.ok());
        return out;
    };

    const Payload out_first = round_trip(first);
    const Payload out_second = round_trip(second);

    CHECK(out_first.a == first.a);
    CHECK(out_first.b == first.b);
    CHECK(out_first.c == first.c);
    CHECK(out_first.d == first.d);
    CHECK(out_second.a == second.a);
    CHECK(out_second.b == second.b);
    CHECK(out_second.c == second.c);
    CHECK(out_second.d == second.d);
    // The discriminating assertion: the two results are not the same object.
    CHECK(out_first.a != out_second.a);
    CHECK(out_first.d != out_second.d);
}

TEST_CASE("the inventory section restores the inventory it saved, and a different one differently") {
    // Same control one layer up, on the real gameplay section, because that is
    // the code a save actually runs through. Two worlds, two stack lists.
    auto save_then_load = [](const std::vector<gameplay::ItemStack>& stacks) {
        dfn::ecs::World world;
        const EntityId owner = world.spawn();
        world.add(owner, gameplay::Inventory{stacks});

        serialization::BinaryWriter w;
        w.begin_file(TEST_MAGIC, 1);
        w.begin_section(gameplay::SECTION_INVENTORY, gameplay::INVENTORY_SECTION_VERSION);
        gameplay::write_inventory_section(w, world);
        w.end_section();

        // Wipe the live state so a "restore" that did nothing cannot pass.
        world.get<gameplay::Inventory>(owner)->stacks.clear();

        serialization::BinaryReader r;
        REQUIRE(r.open(w.buffer(), TEST_MAGIC));
        const auto section = r.next_section();
        REQUIRE(section.has_value());
        REQUIRE(gameplay::read_inventory_section(r, world, section->version));
        return world.get<gameplay::Inventory>(owner)->stacks;
    };

    const std::vector<gameplay::ItemStack> hoard{
        {gameplay::ItemId{0xAAAA'BBBB'CCCC'DDDDull}, 17},
        {gameplay::ItemId{0x0000'0000'0000'0001ull}, 1}};
    const std::vector<gameplay::ItemStack> pittance{
        {gameplay::ItemId{0x1234'5678'9ABC'DEF0ull}, 3}};

    const auto restored_hoard = save_then_load(hoard);
    const auto restored_pittance = save_then_load(pittance);

    REQUIRE(restored_hoard.size() == 2);
    CHECK(restored_hoard[0].item.value == hoard[0].item.value);
    CHECK(restored_hoard[0].count == 17);
    CHECK(restored_hoard[1].item.value == hoard[1].item.value);
    REQUIRE(restored_pittance.size() == 1);
    CHECK(restored_pittance[0].item.value == pittance[0].item.value);
    CHECK(restored_pittance[0].count == 3);
    // The control: a fixed-struct reader cannot satisfy both.
    CHECK(restored_hoard.size() != restored_pittance.size());
}

// --- Skip-unknown (Rule 7's whole reason for length-prefixing) ---------------

TEST_CASE("an unknown section is stepped over, not an error") {
    serialization::BinaryWriter w;
    w.begin_file(TEST_MAGIC, 1);
    w.begin_section(TAG_ALIEN, 99);       // a section this build never heard of
    w.write_u64(0xDEAD'BEEF'DEAD'BEEFull); // ...with a payload it cannot parse
    w.write_string("from a newer build");
    w.end_section();
    w.begin_section(TAG_MAIN, 1);
    w.write_u32(FIX_U32);
    w.end_section();

    serialization::BinaryReader r;
    REQUIRE(r.open(w.buffer(), TEST_MAGIC));

    uint32_t recovered = 0;
    int sections_seen = 0;
    while (const auto section = r.next_section()) {
        ++sections_seen;
        if (section->tag == TAG_MAIN) {
            recovered = r.read_u32();
        }
        // TAG_ALIEN deliberately falls through with NOTHING read from it.
    }
    CHECK(sections_seen == 2);
    CHECK(recovered == FIX_U32);
    CHECK(r.ok()); // an unknown tag is data, never a failure

    // Control: the unknown section must actually have contained something to
    // skip. If its payload were empty the case would pass on a reader with no
    // skip logic at all, and would be measuring nothing.
    CHECK(w.buffer().size() > 8 + 2 * 14 + 4);
}

// --- Fail soft ---------------------------------------------------------------

TEST_CASE("truncation is detected, at every offset, and never crashes") {
    serialization::BinaryWriter w;
    write_fixture_payload(w);
    const auto complete = w.buffer();
    const std::vector<std::byte> bytes(complete.begin(), complete.end());

    // Control first: the intact buffer must succeed. Half of this case is
    // worthless without it — "always !ok()" would pass every truncation.
    {
        serialization::BinaryReader r;
        REQUIRE(r.open(std::span<const std::byte>(bytes.data(), bytes.size()), TEST_MAGIC));
        check_fixture_payload(r);
    }

    // Every proper prefix of a valid file must either refuse to open or end
    // !ok(). What it must never do is report a clean load of partial data.
    //
    // ONE EXCEPTION, AND IT IS A PROPERTY OF THE FORMAT RATHER THAN OF THIS
    // READER: a file cut at exactly 8 bytes is a complete header followed by
    // zero sections, which is byte-identical to a legitimately empty container.
    // Nothing in the header records how many sections should follow, so no
    // reader can tell the two apart. This is the "absence looks like success"
    // shape at the format level, and the honest response is to name it rather
    // than to let a loop quietly skip it: the protection has to live one layer
    // up, in a caller that checks it actually SAW the sections it needed. It is
    // reported to the lead as the one gap the container cannot close by itself.
    constexpr std::size_t HEADER_ONLY = 8;
    int opened = 0;
    int clean_lies = 0;
    for (std::size_t cut = 0; cut < bytes.size(); ++cut) {
        if (cut == HEADER_ONLY) {
            continue;
        }
        serialization::BinaryReader r;
        if (!r.open(std::span<const std::byte>(bytes.data(), cut), TEST_MAGIC)) {
            continue; // refused at the header — the correct answer for cut < 8
        }
        ++opened;
        while (const auto section = r.next_section()) {
            const uint64_t remaining = r.section_bytes_remaining();
            for (uint64_t i = 0; i < remaining + 8; ++i) {
                (void)r.read_u8(); // deliberately over-read past the end
            }
        }
        if (r.ok()) {
            ++clean_lies;
            MESSAGE("truncation at " << cut << " reported a clean load");
        }
    }
    CHECK(opened > 0); // the loop must have exercised the section path at all
    CHECK(clean_lies == 0);

    // And the exception is asserted rather than assumed, so the day the header
    // gains a section count this line fails and points at the comment above.
    serialization::BinaryReader header_only;
    REQUIRE(header_only.open(std::span<const std::byte>(bytes.data(), HEADER_ONLY),
                             TEST_MAGIC));
    CHECK_FALSE(header_only.next_section().has_value());
    CHECK(header_only.ok()); // indistinguishable from an empty container, today
}

TEST_CASE("a section that lies about its length cannot walk off the buffer") {
    serialization::BinaryWriter w;
    w.begin_file(TEST_MAGIC, 1);
    w.begin_section(TAG_MAIN, 1);
    w.write_u32(FIX_U32);
    w.end_section();
    const auto good = w.buffer();
    std::vector<std::byte> corrupt(good.begin(), good.end());

    // Overwrite the section's u64 length with something enormous. This is the
    // one field in the format that an attacker or a bad disk controls and that
    // the reader would otherwise use as an index.
    for (std::size_t i = 0; i < 8; ++i) {
        corrupt[14 + i] = static_cast<std::byte>(0xFF);
    }
    serialization::BinaryReader r;
    REQUIRE(r.open(std::span<const std::byte>(corrupt.data(), corrupt.size()), TEST_MAGIC));
    CHECK_FALSE(r.next_section().has_value());
    CHECK_FALSE(r.ok());
}

// --- The two misuse latches (lead rulings 1 and 2) ---------------------------

TEST_CASE("a read before the first next_section() latches instead of returning header bytes") {
    serialization::BinaryWriter w;
    write_fixture_payload(w);

    serialization::BinaryReader early;
    REQUIRE(early.open(w.buffer(), TEST_MAGIC));
    const uint32_t stolen = early.read_u32(); // caller forgot the section loop
    CHECK(stolen == 0);
    CHECK_FALSE(early.ok());

    // Control: the identical read one call later, INSIDE the section, succeeds.
    // Without this arm the case is satisfied by a reader that never works.
    serialization::BinaryReader proper;
    REQUIRE(proper.open(w.buffer(), TEST_MAGIC));
    REQUIRE(proper.next_section().has_value());
    CHECK(proper.read_u32() == FIX_U32);
    CHECK(proper.ok());
}

TEST_CASE("a write outside a section can never become a file") {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "dfn_save_format_test";
    std::filesystem::remove_all(dir);

    serialization::BinaryWriter misused;
    misused.begin_file(TEST_MAGIC, 1);
    CHECK(misused.ok()); // still well-formed up to here
    misused.write_u32(FIX_U32); // no begin_section: nowhere in the grammar
    CHECK_FALSE(misused.ok()); // and the writer SAYS SO — it does not just drop
    misused.begin_section(TAG_MAIN, 1);
    misused.write_u32(FIX_U32);
    misused.end_section();
    CHECK_FALSE(misused.ok()); // sticky: a later well-formed call cannot clear it
    CHECK_FALSE(misused.save_to_file(dir / "misused.bin"));
    CHECK_FALSE(std::filesystem::exists(dir / "misused.bin"));

    // A still-open section is the same defect wearing a different hat: its
    // length was never patched, so the payload would read as a zero-byte
    // section and vanish silently.
    serialization::BinaryWriter unclosed;
    unclosed.begin_file(TEST_MAGIC, 1);
    unclosed.begin_section(TAG_MAIN, 1);
    unclosed.write_u32(FIX_U32);
    CHECK_FALSE(unclosed.save_to_file(dir / "unclosed.bin"));

    // Control: the well-formed writer DOES produce a file, and it reads back.
    // "save_to_file always returns false" passes both arms above.
    serialization::BinaryWriter good;
    write_fixture_payload(good);
    CHECK(good.ok());
    const std::filesystem::path path = dir / "good.bin";
    REQUIRE(good.save_to_file(path));
    REQUIRE(std::filesystem::exists(path));
    // And the atomic write left no debris behind.
    CHECK_FALSE(std::filesystem::exists(dir / "good.bin.tmp"));

    serialization::BinaryReader r;
    REQUIRE(r.open_file(path, TEST_MAGIC));
    check_fixture_payload(r);

    std::filesystem::remove_all(dir);
}
