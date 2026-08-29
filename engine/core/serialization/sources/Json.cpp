/*
Module: engine/core/serialization
File: engine/core/serialization/sources/Json.cpp

Responsibility:
- Strict recursive-descent JSON parser + canonical writer behind Json.h.
  Single-pass over UTF-8 input with line/column tracking; depth-capped;
  every failure is a JsonError, never an exception or a crash.

Key items:
- Parser (internal), json_parse, json_write.

Dependencies:
- Uses: Json.h, C++ std (charconv).
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Determinism: member order is file order; the writer emits stored order and
  shortest round-trip doubles. No locale-dependent parsing (std::from_chars /
  to_chars only — never strtod/sprintf, which read the C locale).
*/

#include "engine/core/serialization/sources/Json.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>

// Rule 19 dual-toolchain: libc++ gained FLOATING-POINT from_chars/to_chars
// late; Apple clang 15 (the CI secondary) lacks them while Homebrew clang 22
// (primary) has them. The fallback uses the *_l APIs with a null locale,
// which POSIX defines as the C locale — locale-FREE like charconv, unlike
// bare strtod/snprintf which read the ambient locale.
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
#define DFN_JSON_HAS_FP_CHARCONV 1
#else
#define DFN_JSON_HAS_FP_CHARCONV 0
#include <cstdlib>
#include <xlocale.h>
#endif

namespace dfn::serialization {

// --- JsonValue ------------------------------------------------------------------

// Out-of-line special members: JsonMember is complete here (see Json.h).
JsonValue::JsonValue() = default;
JsonValue::JsonValue(const JsonValue&) = default;
JsonValue::JsonValue(JsonValue&&) noexcept = default;
JsonValue& JsonValue::operator=(const JsonValue&) = default;
JsonValue& JsonValue::operator=(JsonValue&&) noexcept = default;
JsonValue::~JsonValue() = default;

namespace {
const JsonValue NULL_NODE{};
} // namespace

JsonValue JsonValue::make_bool(bool v) {
    JsonValue out;
    out.type_ = JsonType::Bool;
    out.bool_ = v;
    return out;
}
JsonValue JsonValue::make_number(double v) {
    JsonValue out;
    out.type_ = JsonType::Number;
    out.number_ = v;
    return out;
}
JsonValue JsonValue::make_string(std::string v) {
    JsonValue out;
    out.type_ = JsonType::String;
    out.string_ = std::move(v);
    return out;
}
JsonValue JsonValue::make_array() {
    JsonValue out;
    out.type_ = JsonType::Array;
    return out;
}
JsonValue JsonValue::make_object() {
    JsonValue out;
    out.type_ = JsonType::Object;
    return out;
}

bool JsonValue::as_bool(bool fallback) const {
    return type_ == JsonType::Bool ? bool_ : fallback;
}
double JsonValue::as_number(double fallback) const {
    return type_ == JsonType::Number ? number_ : fallback;
}
int64_t JsonValue::as_i64(int64_t fallback) const {
    if (type_ != JsonType::Number) return fallback;
    const double rounded = std::nearbyint(number_);
    if (rounded != number_ || std::fabs(number_) > 9.007199254740992e15) {
        return fallback; // not integral, or outside the exact-double range
    }
    return static_cast<int64_t>(rounded);
}
std::string_view JsonValue::as_string(std::string_view fallback) const {
    return type_ == JsonType::String ? std::string_view{string_} : fallback;
}

std::size_t JsonValue::size() const {
    if (type_ == JsonType::Array) return array_.size();
    if (type_ == JsonType::Object) return object_.size();
    return 0;
}
const JsonValue& JsonValue::at(std::size_t index) const {
    if (type_ != JsonType::Array || index >= array_.size()) return NULL_NODE;
    return array_[index];
}
void JsonValue::push_back(JsonValue v) {
    if (type_ == JsonType::Array) array_.push_back(std::move(v));
}
const JsonValue* JsonValue::find(std::string_view key) const {
    if (type_ != JsonType::Object) return nullptr;
    for (const JsonMember& m : object_) {
        if (m.key == key) return &m.value;
    }
    return nullptr;
}
const JsonValue& JsonValue::get(std::string_view key) const {
    const JsonValue* found = find(key);
    return found != nullptr ? *found : NULL_NODE;
}
void JsonValue::add_member(std::string key, JsonValue v) {
    if (type_ == JsonType::Object) {
        object_.push_back(JsonMember{std::move(key), std::move(v)});
    }
}

bool JsonValue::operator==(const JsonValue& other) const {
    if (type_ != other.type_) return false;
    switch (type_) {
    case JsonType::Null: return true;
    case JsonType::Bool: return bool_ == other.bool_;
    case JsonType::Number: return number_ == other.number_;
    case JsonType::String: return string_ == other.string_;
    case JsonType::Array: return array_ == other.array_;
    case JsonType::Object: return object_ == other.object_;
    }
    return false;
}

// --- Parser ---------------------------------------------------------------------

namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    [[nodiscard]] JsonParseResult run() {
        JsonParseResult result;
        skip_whitespace();
        JsonValue root;
        if (!parse_value(root, 0)) {
            result.error = error_;
            return result;
        }
        skip_whitespace();
        if (pos_ != text_.size()) {
            fail("trailing garbage after the root value");
            result.error = error_;
            return result;
        }
        result.ok = true;
        result.root = std::move(root);
        return result;
    }

private:
    std::string_view text_;
    std::size_t pos_ = 0;
    uint32_t line_ = 1;
    uint32_t column_ = 1;
    JsonError error_{};
    bool failed_ = false;

    [[nodiscard]] bool at_end() const { return pos_ >= text_.size(); }
    [[nodiscard]] char peek() const { return text_[pos_]; }

    char advance() {
        const char c = text_[pos_++];
        if (c == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return c;
    }

    bool fail(std::string message) {
        if (!failed_) { // first error wins — it is where the problem is
            failed_ = true;
            error_ = JsonError{line_, column_, std::move(message)};
        }
        return false;
    }

    void skip_whitespace() {
        while (!at_end()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                advance();
            } else {
                return; // comments are not JSON: '/' falls through to a value error
            }
        }
    }

    bool expect(char c, const char* what) {
        if (at_end()) return fail(std::string{"unexpected end of input, expected "} + what);
        if (peek() != c) return fail(std::string{"expected "} + what);
        advance();
        return true;
    }

    bool parse_value(JsonValue& out, uint32_t depth) {
        if (depth > JSON_MAX_DEPTH) return fail("nesting depth beyond JSON_MAX_DEPTH");
        if (at_end()) return fail("unexpected end of input, expected a value");
        switch (peek()) {
        case '{': return parse_object(out, depth);
        case '[': return parse_array(out, depth);
        case '"': return parse_string_value(out);
        case 't': return parse_literal("true", JsonValue::make_bool(true), out);
        case 'f': return parse_literal("false", JsonValue::make_bool(false), out);
        case 'n': return parse_literal("null", JsonValue::make_null(), out);
        default: return parse_number(out);
        }
    }

    bool parse_literal(std::string_view word, JsonValue value, JsonValue& out) {
        if (text_.substr(pos_, word.size()) != word) {
            return fail(std::string{"invalid literal (expected '"} + std::string{word} + "')");
        }
        for (std::size_t i = 0; i < word.size(); ++i) advance();
        out = std::move(value);
        return true;
    }

    bool parse_object(JsonValue& out, uint32_t depth) {
        advance(); // '{'
        out = JsonValue::make_object();
        skip_whitespace();
        if (!at_end() && peek() == '}') {
            advance();
            return true;
        }
        while (true) {
            skip_whitespace();
            if (at_end() || peek() != '"') {
                return fail("expected a string object key");
            }
            std::string key;
            if (!parse_string_raw(key)) return false;
            // DUPLICATE KEYS ARE AN ERROR (documented policy): content files
            // are hand-written; "last one wins" silently discards someone's
            // edit and the file looks fine. Linear scan is right-sized —
            // content objects are tens of keys, not thousands.
            if (out.find(key) != nullptr) {
                return fail("duplicate object key '" + key + "'");
            }
            skip_whitespace();
            if (!expect(':', "':' after object key")) return false;
            skip_whitespace();
            JsonValue value;
            if (!parse_value(value, depth + 1)) return false;
            out.add_member(std::move(key), std::move(value));
            skip_whitespace();
            if (at_end()) return fail("unexpected end of input inside an object");
            if (peek() == ',') {
                advance();
                continue;
            }
            if (peek() == '}') {
                advance();
                return true;
            }
            return fail("expected ',' or '}' in an object");
        }
    }

    bool parse_array(JsonValue& out, uint32_t depth) {
        advance(); // '['
        out = JsonValue::make_array();
        skip_whitespace();
        if (!at_end() && peek() == ']') {
            advance();
            return true;
        }
        while (true) {
            skip_whitespace();
            JsonValue element;
            if (!parse_value(element, depth + 1)) return false;
            out.push_back(std::move(element));
            skip_whitespace();
            if (at_end()) return fail("unexpected end of input inside an array");
            if (peek() == ',') {
                advance();
                continue;
            }
            if (peek() == ']') {
                advance();
                return true;
            }
            return fail("expected ',' or ']' in an array");
        }
    }

    bool parse_string_value(JsonValue& out) {
        std::string s;
        if (!parse_string_raw(s)) return false;
        out = JsonValue::make_string(std::move(s));
        return true;
    }

    static void append_utf8(std::string& s, uint32_t cp) {
        if (cp < 0x80) {
            s.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            s.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool parse_hex4(uint32_t& out) {
        out = 0;
        for (int i = 0; i < 4; ++i) {
            if (at_end()) return fail("unexpected end of input in \\u escape");
            const char c = advance();
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<uint32_t>(c - 'A' + 10);
            else return fail("invalid hex digit in \\u escape");
        }
        return true;
    }

    bool parse_string_raw(std::string& out) {
        advance(); // '"'
        out.clear();
        while (true) {
            if (at_end()) return fail("unterminated string");
            const char c = advance();
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("raw control character in string (use \\u escape)");
            }
            if (c != '\\') {
                out.push_back(c); // UTF-8 bytes pass through
                continue;
            }
            if (at_end()) return fail("unterminated escape sequence");
            const char e = advance();
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                uint32_t cp = 0;
                if (!parse_hex4(cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDBFF) { // high surrogate: need the pair
                    if (pos_ + 1 >= text_.size() || text_[pos_] != '\\'
                        || text_[pos_ + 1] != 'u') {
                        return fail("high surrogate without a following \\u low surrogate");
                    }
                    advance();
                    advance();
                    uint32_t low = 0;
                    if (!parse_hex4(low)) return false;
                    if (low < 0xDC00 || low > 0xDFFF) {
                        return fail("invalid low surrogate");
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return fail("unpaired low surrogate");
                }
                append_utf8(out, cp);
                break;
            }
            default: return fail("invalid escape character");
            }
        }
    }

    bool parse_number(JsonValue& out) {
        const std::size_t start = pos_;
        if (!at_end() && peek() == '-') advance();
        // Strict grammar: int part (no leading zeros), optional frac, optional
        // exp. from_chars accepts a superset (hex, inf), so validate first.
        if (at_end()) return fail("unexpected end of input in number");
        if (peek() == '0') {
            advance();
        } else if (peek() >= '1' && peek() <= '9') {
            while (!at_end() && peek() >= '0' && peek() <= '9') advance();
        } else {
            return fail("invalid value");
        }
        if (!at_end() && peek() == '.') {
            advance();
            if (at_end() || peek() < '0' || peek() > '9') {
                return fail("digits required after decimal point");
            }
            while (!at_end() && peek() >= '0' && peek() <= '9') advance();
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            advance();
            if (!at_end() && (peek() == '+' || peek() == '-')) advance();
            if (at_end() || peek() < '0' || peek() > '9') {
                return fail("digits required in exponent");
            }
            while (!at_end() && peek() >= '0' && peek() <= '9') advance();
        }
        double value = 0.0;
        const char* first = text_.data() + start;
        const char* last = text_.data() + pos_;
#if DFN_JSON_HAS_FP_CHARCONV
        const auto [ptr, ec] = std::from_chars(first, last, value);
        const bool converted = ec == std::errc{} && ptr == last;
#else
        // NUL-terminate the validated slice for strtod_l (C locale).
        std::array<char, 64> slice{};
        const std::size_t len = static_cast<std::size_t>(last - first);
        bool converted = false;
        if (len < slice.size()) {
            std::copy(first, last, slice.begin());
            char* end = nullptr;
            value = ::strtod_l(slice.data(), &end, nullptr);
            converted = end == slice.data() + len;
        } else {
            // Longer than any content number needs; grammar already validated,
            // so this is a range refusal, not a parse ambiguity.
            converted = false;
        }
#endif
        if (!converted || !std::isfinite(value)) {
            return fail("number out of range");
        }
        out = JsonValue::make_number(value);
        return true;
    }
};

void write_value(const JsonValue& v, std::string& out) {
    switch (v.type()) {
    case JsonType::Null: out += "null"; break;
    case JsonType::Bool: out += v.as_bool() ? "true" : "false"; break;
    case JsonType::Number: {
        std::array<char, 40> buf{};
#if DFN_JSON_HAS_FP_CHARCONV
        // Shortest round-trip form (to_chars default) — locale-free.
        const auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(),
                                             v.as_number());
        out.append(buf.data(), ec == std::errc{} ? ptr : buf.data());
#else
        // %.17g always round-trips a double; C locale via the _l API.
        const int n = ::snprintf_l(buf.data(), buf.size(), nullptr, "%.17g",
                                   v.as_number());
        if (n > 0) out.append(buf.data(), static_cast<std::size_t>(n));
#endif
        break;
    }
    case JsonType::String: {
        out.push_back('"');
        for (const char c : v.as_string()) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::array<char, 8> esc{};
                    std::snprintf(esc.data(), esc.size(), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += esc.data();
                } else {
                    out.push_back(c); // UTF-8 passes through
                }
            }
        }
        out.push_back('"');
        break;
    }
    case JsonType::Array: {
        out.push_back('[');
        for (std::size_t i = 0; i < v.items().size(); ++i) {
            if (i > 0) out.push_back(',');
            write_value(v.items()[i], out);
        }
        out.push_back(']');
        break;
    }
    case JsonType::Object: {
        out.push_back('{');
        bool first = true;
        for (const JsonMember& m : v.members()) {
            if (!first) out.push_back(',');
            first = false;
            write_value(JsonValue::make_string(m.key), out);
            out.push_back(':');
            write_value(m.value, out);
        }
        out.push_back('}');
        break;
    }
    }
}

} // namespace

JsonParseResult json_parse(std::string_view text) {
    return Parser{text}.run();
}

std::string json_write(const JsonValue& value) {
    std::string out;
    write_value(value, out);
    return out;
}

} // namespace dfn::serialization
