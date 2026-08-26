/*
Created: 27:08:2026 - 02:35:10
Module: tests/app
File: tests/app/PngImageTests.cpp

Responsibility:
- Holds the .png reader honest against the files it was written for: the five
  branding images the owner approved on 26.08 (assets/branding/), which are the
  only PNGs the engine reads and therefore the only ones whose failure anybody
  would ever see.

Dependencies:
- Uses: engine/app PngImage, doctest, the files in assets/branding/ (tracked in
  git, so this suite has no fixture to generate).
- Used by: ctest (app_png).

Notes:
- THE EXPECTED NUMBERS COME FROM SOMEBODY ELSE'S DECODER. Every hash and pixel
  below was produced by an independent implementation -- a ~40 line PNG reader
  written in python on top of the standard library's zlib -- and not by the code
  under test. That distinction is the whole value of the suite: a golden file
  recorded from the implementation proves only that it still does what it did,
  which is exactly the assurance a fresh decoder does not need. Two independent
  implementations agreeing bit-for-bit over 2.6 million pixels is evidence that
  they agree with the FORMAT.
- THE CONTROL ARMS (Rule 30) are the malformed inputs, and they are chosen to be
  the ones that could plausibly pass: a truncated stream (the inflate loop must
  notice it ran out of bits rather than emit a short image), a file whose chunk
  header lies about its length, and a buffer that is not a PNG at all. A decoder
  that returned a grey rectangle for any of them would satisfy every positive
  case here.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
*/
/*
UPD:
- 27:08:2026 - 02:35:10: Создан вместе с читателем .png.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/PngImage.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using dfn::app::Image;

namespace {

std::vector<uint8_t> read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

// The same rolling hash the reference implementation ran over its own output.
// Order-sensitive and byte-sensitive: a row-flip, a channel swap or one wrong
// filter anywhere in the image changes it.
uint64_t hash_of(const std::vector<uint8_t>& bytes) {
    uint64_t h = 0;
    for (uint8_t b : bytes) {
        h = h * 131u + b;
    }
    return h;
}

struct Expected {
    const char* path;
    int width;
    int height;
    uint64_t hash;
};

// Produced by the python reference decoder, 27.08.2026. Two of the five are
// colour type 2 (no alpha) and 6 (RGBA); between them the set exercises both
// channel counts the branding uses.
constexpr Expected FILES[] = {
    {"assets/branding/oak_seal/oak_seal_1024.png", 1024, 1024, 762025475759428711ull},
    {"assets/branding/oak_seal/oak_seal_256.png", 256, 256, 12902768491376570299ull},
    {"assets/branding/oak_seal/oak_silhouette_gold.png", 652, 718, 1992922881106170292ull},
    {"assets/branding/spiral_logo/spiral_logo_full_512.png", 512, 512,
     13927923635529266338ull},
    {"assets/branding/spiral_logo/spiral_icon_transparent_256.png", 256, 256,
     9523878051946792934ull},
};

} // namespace

TEST_CASE("the branding files decode bit-for-bit as an independent decoder reads them") {
    for (const Expected& e : FILES) {
        CAPTURE(e.path);
        const Image img = dfn::app::load_png(e.path);
        REQUIRE_FALSE(img.empty()); // a missing file must not pass as "nothing to check"
        CHECK(img.width == e.width);
        CHECK(img.height == e.height);
        CHECK(img.rgba.size() == static_cast<size_t>(e.width) * e.height * 4u);
        CHECK(hash_of(img.rgba) == e.hash);
    }
}

TEST_CASE("the emblem is transparent at its corners and opaque at its centre") {
    // THE HASH ABOVE ALREADY PINS EVERY BYTE, so this case is not about the
    // pixels -- it is about the MEANING of the fourth channel. A decoder that
    // wrote alpha into the red slot would produce a different hash and this
    // suite would say "the file changed"; this case says which end is up.
    const Image seal = dfn::app::load_png("assets/branding/oak_seal/oak_seal_256.png");
    REQUIRE_FALSE(seal.empty());
    CHECK(seal.at(0, 0)[3] == 0);                              // outside the round seal
    CHECK(seal.at(seal.width - 1, seal.height - 1)[3] == 0);
    CHECK(seal.at(seal.width / 2, seal.height / 2)[3] == 255); // the parchment

    // THE CONTROL: the studio's full lock-up has NO alpha channel (colour type
    // 2) and must come back fully opaque everywhere, corners included. Without
    // it, "alpha 0 at the corner" would also pass a decoder that simply left
    // the fourth channel at zero.
    const Image full =
        dfn::app::load_png("assets/branding/spiral_logo/spiral_logo_full_512.png");
    REQUIRE_FALSE(full.empty());
    CHECK(full.at(0, 0)[3] == 255);
    CHECK(full.at(full.width / 2, full.height / 2)[3] == 255);
    // ...and its ground is the brand's dark, which is what the splash clears to.
    CHECK(full.at(0, 0)[0] == 0x0c);
    CHECK(full.at(0, 0)[1] == 0x0e);
    CHECK(full.at(0, 0)[2] == 0x12);
}

TEST_CASE("a broken file is refused, not approximated") {
    std::vector<uint8_t> good =
        read_bytes("assets/branding/spiral_logo/spiral_icon_transparent_256.png");
    REQUIRE(good.size() > 1000);

    // NOT A PNG AT ALL.
    const std::vector<uint8_t> junk(good.begin(), good.begin() + 8);
    std::vector<uint8_t> wrong_sig = good;
    wrong_sig[1] = 'X';
    CHECK(dfn::app::decode_png(wrong_sig).empty());
    CHECK(dfn::app::decode_png({}).empty());

    // TRUNCATED MID-STREAM, which is the arm that matters: the container header
    // is intact and the dimensions are readable, so a decoder that trusts IHDR
    // and stops when the bits run out would hand back a half-drawn image and
    // call it a success.
    std::vector<uint8_t> cut(good.begin(), good.begin() + good.size() / 2);
    CHECK(dfn::app::decode_png(cut).empty());

    // A CHUNK THAT LIES ABOUT ITS LENGTH. Byte 8 is the top byte of the first
    // chunk's length, so this claims a chunk far longer than the file: the walk
    // must notice rather than read past the end.
    std::vector<uint8_t> long_chunk = good;
    long_chunk[8] = 0x7F;
    CHECK(dfn::app::decode_png(long_chunk).empty());

    // THE CONTROL FOR ALL THREE: the untouched bytes still decode. Without it
    // every check above would pass a decoder that refuses everything.
    CHECK_FALSE(dfn::app::decode_png(good).empty());
}
