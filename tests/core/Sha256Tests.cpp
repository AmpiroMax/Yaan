/*
Module: tests/core
File: tests/core/Sha256Tests.cpp

Responsibility:
- SHA-256 against the FIPS 180-4 vectors ("abc", the empty string, the
  two-block 448-bit message) and against `shasum -a 256` on a run that
  crosses the padding boundary; the control (Rule 30) is a one-bit change
  that must move the digest.

Dependencies:
- Uses: engine/core/serialization Sha256, doctest.
- Used by: ctest (test_sha256).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/core/serialization/sources/Sha256.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using dfn::serialization::sha256_file;
using dfn::serialization::sha256_hex;

namespace {

std::vector<uint8_t> bytes_of(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

} // namespace

TEST_CASE("sha256: FIPS 180-4 vectors") {
    CHECK(sha256_hex(bytes_of("abc"))
          == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256_hex(bytes_of(""))
          == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256_hex(bytes_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))
          == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("sha256: padding boundaries (55, 56, 64 bytes) and a million 'a'") {
    // 55 bytes: the last block holds the message, 0x80 and the length.
    CHECK(sha256_hex(bytes_of(std::string(55, 'a')))
          == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    // 56 bytes: the length no longer fits, a second block is appended.
    CHECK(sha256_hex(bytes_of(std::string(56, 'a')))
          == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    // 64 bytes: exactly one full block, padding is a whole second block.
    CHECK(sha256_hex(bytes_of(std::string(64, 'a')))
          == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    CHECK(sha256_hex(bytes_of(std::string(1000000, 'a')))
          == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("sha256: control -- one flipped bit moves the digest") {
    std::vector<uint8_t> a = bytes_of("The quick brown fox jumps over the lazy dog");
    std::vector<uint8_t> b = a;
    b[10] ^= 0x01;
    CHECK(sha256_hex(a) == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    CHECK(sha256_hex(a) != sha256_hex(b));
}

TEST_CASE("sha256: a file hashes as its bytes; a missing file is nullopt") {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() / "dfn_sha256_test.bin";
    {
        std::FILE* f = std::fopen(p.string().c_str(), "wb");
        REQUIRE(f != nullptr);
        std::fputs("abc", f);
        std::fclose(f);
    }
    const auto h = sha256_file(p);
    REQUIRE(h.has_value());
    CHECK(*h == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::filesystem::remove(p);
    CHECK_FALSE(sha256_file(p).has_value());
}
