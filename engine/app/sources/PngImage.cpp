/*
Module: engine/app
File: engine/app/sources/PngImage.cpp

Responsibility:
- The DEFLATE reader (RFC 1951), the zlib wrapper (RFC 1950) and the PNG
  container + unfilter (RFC 2083) behind PngImage.h.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
*/

#include "engine/app/sources/PngImage.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>

namespace dfn::app {

namespace {

// ---------------------------------------------------------------------------
// BIT READER. LSB-first, which is what DEFLATE's Huffman codes and its literal
// fields both want -- with the one exception the format itself makes: a code is
// packed starting from the MOST significant bit of its value, which is why
// decode() below shifts the accumulated code left rather than right.
// ---------------------------------------------------------------------------
class BitReader {
public:
    explicit BitReader(std::span<const uint8_t> data) : data_(data) {}

    [[nodiscard]] bool ok() const { return ok_; }

    uint32_t bit() {
        if (count_ == 0) {
            if (pos_ >= data_.size()) {
                ok_ = false; // truncated: every caller checks ok() before use
                return 0;
            }
            buf_ = data_[pos_++];
            count_ = 8;
        }
        const uint32_t b = buf_ & 1u;
        buf_ >>= 1;
        --count_;
        return b;
    }

    uint32_t bits(int n) {
        uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            v |= bit() << i;
        }
        return v;
    }

    /// Drops the rest of the current byte (stored blocks start byte-aligned).
    void align() { count_ = 0; }

    /// Raw bytes for a stored block. Only legal right after align().
    [[nodiscard]] const uint8_t* take(size_t n) {
        if (pos_ + n > data_.size()) {
            ok_ = false;
            return nullptr;
        }
        const uint8_t* p = data_.data() + pos_;
        pos_ += n;
        return p;
    }

private:
    std::span<const uint8_t> data_;
    size_t pos_ = 0;
    uint32_t buf_ = 0;
    int count_ = 0;
    bool ok_ = true;
};

// ---------------------------------------------------------------------------
// CANONICAL HUFFMAN, in the counts+symbols shape rather than as a lookup table.
// It decodes one bit at a time, which is slower than a table and is the right
// trade here: the menu inflates half a megabyte ONCE at startup, and a table
// build is the part of a decoder that is easy to get subtly wrong.
// ---------------------------------------------------------------------------
struct Huffman {
    std::array<uint16_t, 16> counts{}; // how many codes of each length 0..15
    std::vector<uint16_t> symbols;     // symbols ordered by (length, value)
};

bool build_huffman(Huffman& h, const uint8_t* lengths, size_t n) {
    h.counts.fill(0);
    for (size_t i = 0; i < n; ++i) {
        ++h.counts[lengths[i]];
    }
    h.counts[0] = 0; // length 0 means "this symbol is not in the alphabet"

    // Offsets of each length's run inside the symbol array.
    std::array<uint16_t, 16> offs{};
    uint16_t total = 0;
    for (int len = 1; len < 16; ++len) {
        offs[len] = total;
        total = static_cast<uint16_t>(total + h.counts[len]);
    }
    h.symbols.assign(total, 0);
    for (size_t i = 0; i < n; ++i) {
        if (lengths[i] != 0) {
            h.symbols[offs[lengths[i]]++] = static_cast<uint16_t>(i);
        }
    }
    return total > 0;
}

int decode_symbol(BitReader& br, const Huffman& h) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len < 16; ++len) {
        code |= static_cast<int>(br.bit());
        const int count = h.counts[len];
        if (code - first < count) {
            return h.symbols[static_cast<size_t>(index + (code - first))];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1; // a code longer than 15 bits cannot exist in DEFLATE
}

// The two static alphabets of a fixed-Huffman block, spelled out by the RFC.
void build_fixed(Huffman& lit, Huffman& dist) {
    std::array<uint8_t, 288> ll{};
    for (size_t i = 0; i < 144; ++i) ll[i] = 8;
    for (size_t i = 144; i < 256; ++i) ll[i] = 9;
    for (size_t i = 256; i < 280; ++i) ll[i] = 7;
    for (size_t i = 280; i < 288; ++i) ll[i] = 8;
    build_huffman(lit, ll.data(), ll.size());
    std::array<uint8_t, 30> dl{};
    dl.fill(5);
    build_huffman(dist, dl.data(), dl.size());
}

constexpr uint16_t LEN_BASE[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,
                                   15, 17, 19, 23, 27, 31, 35, 43, 51,  59,
                                   67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr uint8_t LEN_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                   2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint16_t DIST_BASE[30] = {1,    2,    3,    4,    5,    7,     9,
                                    13,   17,   25,   33,   49,   65,    97,
                                    129,  193,  257,  385,  513,  769,   1025,
                                    1537, 2049, 3073, 4097, 6145, 8193,  12289,
                                    16385, 24577};
constexpr uint8_t DIST_EXTRA[30] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                    4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                    9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool inflate_block_body(BitReader& br, const Huffman& lit, const Huffman& dist,
                        std::vector<uint8_t>& out) {
    for (;;) {
        const int sym = decode_symbol(br, lit);
        if (sym < 0 || !br.ok()) {
            return false;
        }
        if (sym < 256) {
            out.push_back(static_cast<uint8_t>(sym));
            continue;
        }
        if (sym == 256) {
            return true; // end of block
        }
        const int li = sym - 257;
        if (li >= 29) {
            return false;
        }
        const size_t length =
            LEN_BASE[li] + static_cast<size_t>(br.bits(LEN_EXTRA[li]));
        const int di = decode_symbol(br, dist);
        if (di < 0 || di >= 30) {
            return false;
        }
        const size_t distance =
            DIST_BASE[di] + static_cast<size_t>(br.bits(DIST_EXTRA[di]));
        if (distance > out.size()) {
            return false; // a back-reference before the start of the stream
        }
        // COPIED BYTE BY BYTE ON PURPOSE: overlapping copies (distance <
        // length) are legal and are how DEFLATE encodes a run, so memcpy would
        // be wrong here rather than merely faster.
        size_t src = out.size() - distance;
        for (size_t i = 0; i < length; ++i) {
            out.push_back(out[src + i]);
        }
    }
}

/// RFC 1951. Returns false on any malformed stream -- there is no partial
/// success worth having: a half-inflated image is a picture of a bug.
bool inflate(std::span<const uint8_t> in, std::vector<uint8_t>& out) {
    BitReader br(in);
    for (;;) {
        const uint32_t final_block = br.bit();
        const uint32_t type = br.bits(2);
        if (!br.ok()) {
            return false;
        }
        if (type == 0) {
            br.align();
            const uint8_t* hdr = br.take(4);
            if (hdr == nullptr) {
                return false;
            }
            const size_t len = static_cast<size_t>(hdr[0]) | (static_cast<size_t>(hdr[1]) << 8);
            const size_t nlen = static_cast<size_t>(hdr[2]) | (static_cast<size_t>(hdr[3]) << 8);
            if ((len ^ 0xFFFFu) != nlen) {
                return false; // the format's own checksum on the length
            }
            const uint8_t* body = br.take(len);
            if (body == nullptr) {
                return false;
            }
            out.insert(out.end(), body, body + len);
        } else if (type == 1) {
            Huffman lit;
            Huffman dist;
            build_fixed(lit, dist);
            if (!inflate_block_body(br, lit, dist, out)) {
                return false;
            }
        } else if (type == 2) {
            const size_t hlit = br.bits(5) + 257;
            const size_t hdist = br.bits(5) + 1;
            const size_t hclen = br.bits(4) + 4;
            if (!br.ok() || hlit > 288 || hdist > 32) {
                return false;
            }
            // The code-length alphabet is itself Huffman-coded, and its lengths
            // arrive in this permuted order (RFC 1951, 3.2.7).
            static constexpr uint8_t ORDER[19] = {16, 17, 18, 0, 8,  7, 9,  6,
                                                  10, 5,  11, 4, 12, 3, 13, 2,
                                                  14, 1,  15};
            std::array<uint8_t, 19> clen{};
            for (size_t i = 0; i < hclen; ++i) {
                clen[ORDER[i]] = static_cast<uint8_t>(br.bits(3));
            }
            Huffman code_huff;
            if (!build_huffman(code_huff, clen.data(), clen.size()) || !br.ok()) {
                return false;
            }
            std::vector<uint8_t> lengths(hlit + hdist, 0);
            size_t i = 0;
            while (i < lengths.size()) {
                const int sym = decode_symbol(br, code_huff);
                if (sym < 0 || !br.ok()) {
                    return false;
                }
                if (sym < 16) {
                    lengths[i++] = static_cast<uint8_t>(sym);
                } else if (sym == 16) {
                    if (i == 0) {
                        return false; // "repeat previous" with no previous
                    }
                    const uint8_t prev = lengths[i - 1];
                    size_t n = 3 + br.bits(2);
                    while (n-- > 0 && i < lengths.size()) {
                        lengths[i++] = prev;
                    }
                } else if (sym == 17) {
                    size_t n = 3 + br.bits(3);
                    while (n-- > 0 && i < lengths.size()) {
                        lengths[i++] = 0;
                    }
                } else {
                    size_t n = 11 + br.bits(7);
                    while (n-- > 0 && i < lengths.size()) {
                        lengths[i++] = 0;
                    }
                }
            }
            Huffman lit;
            Huffman dist;
            build_huffman(lit, lengths.data(), hlit);
            build_huffman(dist, lengths.data() + hlit, hdist);
            if (!inflate_block_body(br, lit, dist, out)) {
                return false;
            }
        } else {
            return false; // type 3 is reserved
        }
        if (final_block != 0) {
            return br.ok();
        }
    }
}

/// RFC 1950: two header bytes, then the DEFLATE stream. The Adler-32 trailer is
/// not verified -- the PNG's own per-chunk CRC already covers the bytes, and a
/// second checksum would only tell us the same thing twice.
bool zlib_inflate(std::span<const uint8_t> in, std::vector<uint8_t>& out) {
    if (in.size() < 2) {
        return false;
    }
    const uint8_t cmf = in[0];
    const uint8_t flg = in[1];
    if ((cmf & 0x0Fu) != 8) {
        return false; // compression method must be deflate
    }
    if ((static_cast<uint32_t>(cmf) * 256u + flg) % 31u != 0u) {
        return false; // the header's own check
    }
    if ((flg & 0x20u) != 0u) {
        return false; // a preset dictionary: never produced by any PNG writer
    }
    return inflate(in.subspan(2), out);
}

uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
         | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = p > a ? p - a : a - p;
    const int pb = p > b ? p - b : b - p;
    const int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) {
        return a;
    }
    return pb <= pc ? b : c;
}

} // namespace

const uint8_t* Image::at(int x, int y) const {
    static const uint8_t transparent[4] = {0, 0, 0, 0};
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return transparent;
    }
    return rgba.data() + (static_cast<size_t>(y) * static_cast<size_t>(width)
                          + static_cast<size_t>(x)) * 4u;
}

Image decode_png(std::span<const uint8_t> bytes) {
    static constexpr uint8_t SIG[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    Image img;
    if (bytes.size() < 8 || std::memcmp(bytes.data(), SIG, 8) != 0) {
        std::fprintf(stderr, "[png] это не PNG (подпись не та или файл пуст)\n");
        return img;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t depth = 0;
    uint8_t color_type = 0;
    uint8_t interlace = 0;
    bool have_header = false;
    std::vector<uint8_t> idat;
    std::vector<uint8_t> palette;   // RGB triples
    std::vector<uint8_t> pal_alpha; // tRNS for colour type 3

    size_t off = 8;
    while (off + 8 <= bytes.size()) {
        const uint32_t len = be32(bytes.data() + off);
        const char* type = reinterpret_cast<const char*>(bytes.data() + off + 4);
        if (off + 12 + len > bytes.size()) {
            std::fprintf(stderr, "[png] обрыв файла внутри чанка\n");
            return Image{};
        }
        const uint8_t* body = bytes.data() + off + 8;
        if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            width = be32(body);
            height = be32(body + 4);
            depth = body[8];
            color_type = body[9];
            interlace = body[12];
            have_header = true;
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            palette.assign(body, body + len);
        } else if (std::memcmp(type, "tRNS", 4) == 0) {
            pal_alpha.assign(body, body + len);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), body, body + len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        off += 12 + len;
    }

    if (!have_header || width == 0 || height == 0) {
        std::fprintf(stderr, "[png] нет заголовка IHDR или нулевой размер\n");
        return Image{};
    }
    // REFUSED OUT LOUD, NOT APPROXIMATED. See the header: a decoder that
    // guesses produces art that looks badly exported, and the bug then gets
    // filed against the artist.
    if (depth != 8 || interlace != 0
        || (color_type != 0 && color_type != 2 && color_type != 3
            && color_type != 4 && color_type != 6)) {
        std::fprintf(stderr,
                     "[png] не читаю: глубина %u, тип цвета %u, чересстрочность %u "
                     "(умею только глубину 8, типы 0/2/3/4/6, без чересстрочности)\n",
                     static_cast<unsigned>(depth), static_cast<unsigned>(color_type),
                     static_cast<unsigned>(interlace));
        return Image{};
    }
    if (color_type == 3 && palette.empty()) {
        std::fprintf(stderr, "[png] палитровый файл без чанка PLTE\n");
        return Image{};
    }

    const size_t channels = (color_type == 0) ? 1
                          : (color_type == 2) ? 3
                          : (color_type == 3) ? 1
                          : (color_type == 4) ? 2
                                              : 4;
    const size_t stride = static_cast<size_t>(width) * channels;

    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * height);
    if (!zlib_inflate(idat, raw) || raw.size() < (stride + 1) * height) {
        std::fprintf(stderr, "[png] поток IDAT не разжался (%zu байт из %zu)\n",
                     raw.size(), (stride + 1) * height);
        return Image{};
    }

    // UNFILTER IN PLACE, LINE BY LINE. Each line's filter reads the ALREADY
    // unfiltered line above it, which is why the previous line is kept rather
    // than re-derived.
    std::vector<uint8_t> flat(stride * height, 0);
    const size_t bpp = channels; // depth 8: one byte per channel
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t filter = raw[(stride + 1) * y];
        const uint8_t* src = raw.data() + (stride + 1) * y + 1;
        uint8_t* dst = flat.data() + stride * y;
        const uint8_t* up = (y > 0) ? flat.data() + stride * (y - 1) : nullptr;
        for (size_t x = 0; x < stride; ++x) {
            const int a = (x >= bpp) ? dst[x - bpp] : 0;
            const int b = (up != nullptr) ? up[x] : 0;
            const int c = (up != nullptr && x >= bpp) ? up[x - bpp] : 0;
            int v = src[x];
            switch (filter) {
            case 0: break;
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) / 2; break;
            case 4: v += paeth(a, b, c); break;
            default:
                std::fprintf(stderr, "[png] неизвестный фильтр строки %u\n",
                             static_cast<unsigned>(filter));
                return Image{};
            }
            dst[x] = static_cast<uint8_t>(v & 0xFF);
        }
    }

    img.width = static_cast<int>(width);
    img.height = static_cast<int>(height);
    img.rgba.assign(static_cast<size_t>(width) * height * 4u, 0);
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
        const uint8_t* s = flat.data() + i * channels;
        uint8_t* d = img.rgba.data() + i * 4u;
        switch (color_type) {
        case 0: d[0] = d[1] = d[2] = s[0]; d[3] = 255; break;
        case 2: d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; break;
        case 3: {
            const size_t k = static_cast<size_t>(s[0]) * 3u;
            d[0] = (k + 2 < palette.size()) ? palette[k] : 0;
            d[1] = (k + 2 < palette.size()) ? palette[k + 1] : 0;
            d[2] = (k + 2 < palette.size()) ? palette[k + 2] : 0;
            d[3] = (s[0] < pal_alpha.size()) ? pal_alpha[s[0]] : 255;
            break;
        }
        case 4: d[0] = d[1] = d[2] = s[0]; d[3] = s[1]; break;
        default: d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; break;
        }
    }
    return img;
}

Image load_png(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "[png] нет файла %s\n", path.c_str());
        return Image{};
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    Image img = decode_png(bytes);
    if (img.empty()) {
        std::fprintf(stderr, "[png] %s не прочитан\n", path.c_str());
    }
    return img;
}

const Image& cached_png(const std::string& path) {
    static std::map<std::string, Image> cache;
    const auto it = cache.find(path);
    if (it != cache.end()) {
        return it->second;
    }
    return cache.emplace(path, load_png(path)).first->second;
}

} // namespace dfn::app
