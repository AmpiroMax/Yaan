/*
Created: 10:08:2026 - 01:58:00
Last updated: 10:08:2026 - 01:58:00
Module: engine/core/serialization
File: engine/core/serialization/sources/Json.h

Responsibility:
- Strict JSON reader for content files (Rule 5/6: items, interactables,
  placement, localization live in data). Parses UTF-8 text into a DOM
  (JsonValue: null/bool/number/string/array/object) with precise line/column
  errors. No exceptions cross the API boundary; parsing is deterministic
  (object member order = file order, always).

Key items:
- JsonValue (the DOM node), JsonMember, JsonType.
- json_parse() -> JsonParseResult {ok, root, error {line, column, message}}.
- json_write() — canonical serializer (round-trip guarantee, test anchor).

Dependencies:
- Uses: C++ std only.
- Used by: content loading (app/gameplay consumers arrive with the lead's
  wiring), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- STRICTNESS POLICY (documented, tested): duplicate object keys are an ERROR
  (content is hand-written; last-wins silently eats typos), trailing garbage
  after the root value is an ERROR, truncation is an ERROR, comments and
  NaN/Infinity are not JSON and are ERRORS. Nesting depth is capped
  (JSON_MAX_DEPTH) so corrupt input fails soft instead of overflowing the
  stack (same doctrine as BinaryReader).
- Error strings are developer-facing only (Rule 5 exemption).
*/
/*
UPD:
- 10:08:2026 - 01:58:00: Created — tech-debt wave task 3 (grill в23 item г).
*/

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::serialization {

/// Nesting depth cap: a parser recursing per level must bound corrupt input.
inline constexpr uint32_t JSON_MAX_DEPTH = 128;

enum class JsonType : uint8_t { Null, Bool, Number, String, Array, Object };

struct JsonMember; // forward (object member)

/// One DOM node. Plain data, movable; accessors are total (wrong-type reads
/// return the fallback rather than trapping) so consumer code stays linear.
class JsonValue {
public:
    // Special members are out-of-line: JsonMember (below) contains JsonValue,
    // so vector<JsonMember> here holds an incomplete type — legal since C++17
    // ONLY while no member function is instantiated, which implicit inline
    // special members would do right here.
    JsonValue();
    JsonValue(const JsonValue&);
    JsonValue(JsonValue&&) noexcept;
    JsonValue& operator=(const JsonValue&);
    JsonValue& operator=(JsonValue&&) noexcept;
    ~JsonValue();

    static JsonValue make_null() { return JsonValue{}; }
    static JsonValue make_bool(bool v);
    static JsonValue make_number(double v);
    static JsonValue make_string(std::string v);
    static JsonValue make_array();
    static JsonValue make_object();

    [[nodiscard]] JsonType type() const { return type_; }
    [[nodiscard]] bool is_null() const { return type_ == JsonType::Null; }
    [[nodiscard]] bool is_bool() const { return type_ == JsonType::Bool; }
    [[nodiscard]] bool is_number() const { return type_ == JsonType::Number; }
    [[nodiscard]] bool is_string() const { return type_ == JsonType::String; }
    [[nodiscard]] bool is_array() const { return type_ == JsonType::Array; }
    [[nodiscard]] bool is_object() const { return type_ == JsonType::Object; }

    [[nodiscard]] bool as_bool(bool fallback = false) const;
    [[nodiscard]] double as_number(double fallback = 0.0) const;
    /// Number as integer when it is exactly integral; fallback otherwise
    /// (content ids/counts — 7.5 is not a count and reports the fallback).
    [[nodiscard]] int64_t as_i64(int64_t fallback = 0) const;
    [[nodiscard]] std::string_view as_string(std::string_view fallback = {}) const;

    // Array access.
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] const JsonValue& at(std::size_t index) const; ///< null node if out of range
    [[nodiscard]] const std::vector<JsonValue>& items() const { return array_; }
    void push_back(JsonValue v); ///< array only (no-op otherwise)

    // Object access. Member order is FILE ORDER, deterministically.
    [[nodiscard]] const JsonValue* find(std::string_view key) const; ///< nullptr if absent
    [[nodiscard]] const JsonValue& get(std::string_view key) const;  ///< null node if absent
    [[nodiscard]] const std::vector<JsonMember>& members() const { return object_; }
    /// Appends a member (object only; duplicate keys are the PARSER's error —
    /// this builder trusts its caller and is used by tests).
    void add_member(std::string key, JsonValue v);

    [[nodiscard]] bool operator==(const JsonValue& other) const;

private:
    JsonType type_ = JsonType::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::vector<JsonMember> object_;
};

/// One object member. A vector of these (not a map) keeps file order and
/// makes iteration deterministic — Rule 13 applies to content reads too.
struct JsonMember {
    std::string key;
    JsonValue value;

    [[nodiscard]] bool operator==(const JsonMember& other) const {
        return key == other.key && value == other.value;
    }
};

/// Parse error location: 1-based line/column of the offending character.
struct JsonError {
    uint32_t line = 0;
    uint32_t column = 0;
    std::string message{}; ///< developer-facing
};

struct JsonParseResult {
    bool ok = false;
    JsonValue root{};   ///< valid only when ok
    JsonError error{};  ///< valid only when !ok
};

/// Parses strict JSON (RFC 8259). Errors instead of tolerating: duplicate
/// object keys, trailing garbage, truncation, comments, control characters in
/// strings, NaN/Infinity, depth beyond JSON_MAX_DEPTH. Never throws.
[[nodiscard]] JsonParseResult json_parse(std::string_view text);

/// Canonical serializer: minimal whitespace, members in stored order, shortest
/// round-trip number form. json_parse(json_write(v)) reproduces v exactly —
/// the round-trip test pins it.
[[nodiscard]] std::string json_write(const JsonValue& value);

} // namespace dfn::serialization
