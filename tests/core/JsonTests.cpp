/*
Created: 10:08:2026 - 01:58:00
Last updated: 10:08:2026 - 01:58:00
Module: tests
File: tests/core/JsonTests.cpp

Responsibility:
- Suite for the strict JSON reader (Rule 30: every acceptance case ships with
  the malformed inputs the strictness policy exists to REJECT — truncation,
  trailing garbage, duplicate keys, bad escapes, comments, depth bombs — and
  each must fail with a located error). Round-trip pins the writer.

Key items:
- Acceptance parse, strictness controls, unicode escapes, round-trip,
  determinism, error locations.

Dependencies:
- Uses: doctest, dfn_core (serialization/Json).
- Used by: ctest (test_json).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The reject list IS the documented policy; relaxing one needs a spec sync.
*/
/*
UPD:
- 10:08:2026 - 01:58:00: Created with the JSON reader (tech-debt task 3).
*/

#include "engine/core/serialization/sources/Json.h"

#include <doctest/doctest.h>
#include <string>

using dfn::serialization::json_parse;
using dfn::serialization::json_write;
using dfn::serialization::JsonParseResult;
using dfn::serialization::JsonType;
using dfn::serialization::JsonValue;

TEST_CASE("json: a content-shaped document parses with order and types intact") {
    const char* text = R"({
        "id": "torch_iron",
        "display_name_key": "item.torch_iron.name",
        "weight_kg": 1.5,
        "stack_max": 4,
        "flammable": true,
        "enchantment": null,
        "tags": ["light_source", "held"],
        "light": {"radius_m": 9.0, "flicker": true}
    })";
    const JsonParseResult result = json_parse(text);
    REQUIRE(result.ok);
    const JsonValue& root = result.root;
    REQUIRE(root.is_object());
    CHECK(root.size() == 8);

    // Member order is FILE order — deterministic iteration is part of the API.
    CHECK(root.members()[0].key == "id");
    CHECK(root.members()[7].key == "light");

    CHECK(root.get("id").as_string() == "torch_iron");
    CHECK(root.get("weight_kg").as_number() == doctest::Approx(1.5));
    CHECK(root.get("stack_max").as_i64() == 4);
    CHECK(root.get("flammable").as_bool());
    CHECK(root.get("enchantment").is_null());
    REQUIRE(root.get("tags").is_array());
    CHECK(root.get("tags").at(0).as_string() == "light_source");
    CHECK(root.get("tags").at(1).as_string() == "held");
    CHECK(root.get("light").get("radius_m").as_number() == doctest::Approx(9.0));

    // Total accessors: absent/mistyped reads report the fallback, no traps.
    CHECK(root.get("absent").is_null());
    CHECK(root.get("absent").as_i64(-1) == -1);
    CHECK(root.get("weight_kg").as_i64(-1) == -1); // 1.5 is not a count
    CHECK(root.get("tags").at(99).is_null());
    CHECK(root.find("id") != nullptr);
    CHECK(root.find("missing") == nullptr);
}

TEST_CASE("json: scalars, empty containers, and number forms") {
    CHECK(json_parse("null").root.is_null());
    CHECK(json_parse("true").root.as_bool());
    CHECK(json_parse("[]").root.is_array());
    CHECK(json_parse("{}").root.is_object());
    CHECK(json_parse("0").root.as_number() == 0.0);
    CHECK(json_parse("-0.5").root.as_number() == doctest::Approx(-0.5));
    CHECK(json_parse("1e3").root.as_number() == doctest::Approx(1000.0));
    CHECK(json_parse("2.5E-2").root.as_number() == doctest::Approx(0.025));
    CHECK(json_parse("9007199254740992").root.as_i64() == 9007199254740992LL);
    CHECK(json_parse("  [1, 2]  ").ok); // surrounding whitespace is fine
}

TEST_CASE("json: unicode escapes decode to UTF-8 (incl. surrogate pairs)") {
    const JsonParseResult cyr = json_parse(R"("Ж")");
    REQUIRE(cyr.ok);
    CHECK(cyr.root.as_string() == "Ж");

    const JsonParseResult emoji = json_parse(R"("😀")");
    REQUIRE(emoji.ok);
    CHECK(emoji.root.as_string() == "\xF0\x9F\x98\x80");

    // Raw UTF-8 passes through untouched (localization files are UTF-8).
    const JsonParseResult raw = json_parse("\"Вэйлмир\"");
    REQUIRE(raw.ok);
    CHECK(raw.root.as_string() == "Вэйлмир");

    const JsonParseResult escapes = json_parse(R"("a\"b\\c\/d\n\t")");
    REQUIRE(escapes.ok);
    CHECK(escapes.root.as_string() == "a\"b\\c/d\n\t");
}

TEST_CASE("json: CONTROLS — every documented rejection really rejects") {
    // Rule 30: the strictness policy is these cases failing. Each entry names
    // the rule it pins; a parser change that admits one turns a documented
    // error into silent acceptance and MUST show up here.
    const struct {
        const char* name;
        const char* text;
    } rejected[] = {
        {"truncated object", R"({"a": 1)"},
        {"truncated array", R"([1, 2)"},
        {"truncated string", R"("abc)"},
        {"truncated escape", R"("abc\)"},
        {"truncated number", "-"},
        {"trailing garbage", R"({"a": 1} extra)"},
        {"trailing second value", "1 2"},
        {"duplicate key", R"({"a": 1, "a": 2})"},
        {"comment", "// hello\n1"},
        {"leading zero", "01"},
        {"bare dot", "1."},
        {"missing exponent digits", "1e"},
        {"NaN literal", "NaN"},
        {"Infinity literal", "Infinity"},
        {"single quotes", "'a'"},
        {"unquoted key", "{a: 1}"},
        {"trailing comma object", R"({"a": 1,})"},
        {"trailing comma array", "[1,]"},
        {"raw control char", "\"a\nb\""},
        {"bad escape", R"("\q")"},
        {"bad hex", R"("\u12GZ")"},
        {"unpaired high surrogate", R"("\uD83D")"},
        {"unpaired low surrogate", R"("\uDE00")"},
        {"lone value comma", "[,]"},
        {"empty input", ""},
        {"whitespace only", "   "},
        {"colon for comma", R"({"a": 1: "b": 2})"},
    };
    for (const auto& c : rejected) {
        CAPTURE(c.name);
        const JsonParseResult r = json_parse(c.text);
        CHECK_FALSE(r.ok);
        CHECK_FALSE(r.error.message.empty());
        CHECK(r.error.line >= 1); // every error is located
    }

    // Depth bomb: JSON_MAX_DEPTH+ levels must fail soft, not overflow.
    std::string deep;
    for (int i = 0; i < 4000; ++i) deep += '[';
    const JsonParseResult bomb = json_parse(deep);
    CHECK_FALSE(bomb.ok);
    CHECK(bomb.error.message.find("depth") != std::string::npos);
    // ...while legal nesting inside the cap parses (30a: the guard has a
    // passing neighbor, so it measures depth, not the detector).
    std::string legal;
    for (int i = 0; i < 100; ++i) legal += '[';
    for (int i = 0; i < 100; ++i) legal += ']';
    CHECK(json_parse(legal).ok);
}

TEST_CASE("json: errors carry the offending line and column") {
    // Line/column point AT the problem — the whole point of a strict reader
    // for hand-written content.
    const JsonParseResult r = json_parse("{\n  \"a\": 1,\n  \"a\": 2\n}");
    REQUIRE_FALSE(r.ok);
    CHECK(r.error.line == 3);
    CHECK(r.error.message.find("duplicate") != std::string::npos);
    CHECK(r.error.message.find("'a'") != std::string::npos);

    const JsonParseResult t = json_parse("[1, 2\n");
    REQUIRE_FALSE(t.ok);
    CHECK(t.error.line == 2); // ran off the end after the newline
}

TEST_CASE("json: round-trip — write(parse(x)) and parse(write(dom)) are exact") {
    // Writer pin: canonical output re-parses to the SAME DOM.
    const char* text = R"({
        "name": "Вэйлмир",
        "population": 11,
        "ford_depth": 0.4,
        "wealth": -2.25e-3,
        "gates": [true, false, null],
        "keep": {"floors": 2, "banner": "raven \"black\""}
    })";
    const JsonParseResult first = json_parse(text);
    REQUIRE(first.ok);
    const std::string written = json_write(first.root);
    const JsonParseResult second = json_parse(written);
    REQUIRE(second.ok);
    CHECK(first.root == second.root);
    // Canonical output is a fixpoint: writing again changes nothing.
    CHECK(json_write(second.root) == written);

    // Hand-built DOM round-trips too (the builder API is part of the surface).
    JsonValue obj = JsonValue::make_object();
    obj.add_member("id", JsonValue::make_string("bread"));
    obj.add_member("value", JsonValue::make_number(3.0));
    JsonValue tags = JsonValue::make_array();
    tags.push_back(JsonValue::make_string("food"));
    tags.push_back(JsonValue::make_number(0.125));
    obj.add_member("tags", std::move(tags));
    obj.add_member("note", JsonValue::make_string("tab\there\n\x01"));
    const JsonParseResult back = json_parse(json_write(obj));
    REQUIRE(back.ok);
    CHECK(back.root == obj);
}

TEST_CASE("json: parsing is deterministic (two parses, one DOM)") {
    const char* text = R"({"b": [1, {"c": 2}], "a": 3})";
    const JsonParseResult r1 = json_parse(text);
    const JsonParseResult r2 = json_parse(text);
    REQUIRE(r1.ok);
    REQUIRE(r2.ok);
    CHECK(r1.root == r2.root);
    CHECK(json_write(r1.root) == json_write(r2.root));
    // Order stored is file order, not alphabetical: "b" precedes "a".
    CHECK(r1.root.members()[0].key == "b");
    CHECK(r1.root.members()[1].key == "a");
}
