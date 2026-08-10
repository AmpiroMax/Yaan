/*
Created: 10:08:2026 - 21:13:15
Last updated: 10:08:2026 - 21:13:15
Module: engine/world
File: engine/world/sources/LayoutLoad.cpp

Responsibility:
- Implementation of the layout content loader (contract in LayoutLoad.h).

Dependencies:
- Uses: LayoutLoad.h, core Json, <fstream>, <sstream>.
- Used by: linked into dfn_world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every read goes through the checked helpers below. They exist so that a
  missing or mistyped key is a NAMED error rather than a silent fallback: a
  map that half-loads generates a world with a feature quietly absent, which
  reads as a worldgen bug for as long as it takes someone to diff the JSON.
*/
/*
UPD:
- 10:08:2026 - 21:13:15: Created — CODE_AUDIT §3.4 mechanism half.
*/

#include "engine/world/sources/LayoutLoad.h"

#include <fstream>
#include <sstream>

namespace dfn::world {
namespace {

using serialization::JsonValue;

/// Accumulates the first error and its JSON path. Once failed it stays failed,
/// so the caller can write the whole load as straight-line code and check once
/// — the alternative is an if after every field, which is where reads get
/// skipped.
class Loader {
public:
    [[nodiscard]] bool ok() const { return ok_; }
    [[nodiscard]] const std::string& error() const { return error_; }

    void fail(std::string_view path, std::string_view what) {
        if (ok_) {
            ok_ = false;
            error_ = std::string(path) + ": " + std::string(what);
        }
    }

    /// A required member of an object, with its type checked.
    const JsonValue* member(const JsonValue& obj, std::string_view key, serialization::JsonType want,
                            std::string_view path) {
        if (!ok_) return nullptr;
        const JsonValue* v = obj.find(key);
        if (v == nullptr) {
            fail(path, "required key is missing");
            return nullptr;
        }
        if (v->type() != want) {
            fail(path, "wrong type");
            return nullptr;
        }
        return v;
    }

    /// Rejects any key not in `allowed`. A typo must not leave the engine's
    /// neutral value silently in place.
    void only(const JsonValue& obj, std::initializer_list<std::string_view> allowed,
              std::string_view path) {
        if (!ok_) return;
        for (const serialization::JsonMember& m : obj.members()) {
            if (!m.key.empty() && m.key.front() == '_') continue; // _comment etc.
            bool found = false;
            for (const std::string_view a : allowed) {
                if (m.key == a) { found = true; break; }
            }
            if (!found) {
                fail(path, "unknown key '" + m.key + "'");
                return;
            }
        }
    }

    float number(const JsonValue& obj, std::string_view key, std::string_view path) {
        const JsonValue* v = member(obj, key, serialization::JsonType::Number, path);
        return v != nullptr ? static_cast<float>(v->as_number()) : 0.0f;
    }

    int integer(const JsonValue& obj, std::string_view key, std::string_view path) {
        const JsonValue* v = member(obj, key, serialization::JsonType::Number, path);
        return v != nullptr ? static_cast<int>(v->as_i64()) : 0;
    }

    bool boolean(const JsonValue& obj, std::string_view key, std::string_view path) {
        const JsonValue* v = member(obj, key, serialization::JsonType::Bool, path);
        return v != nullptr && v->as_bool();
    }

    /// A fixed-length numeric array (vec2/vec3/vec4 and the point tuples).
    void floats(const JsonValue& obj, std::string_view key, std::string_view path, float* out,
                std::size_t n) {
        const JsonValue* v = member(obj, key, serialization::JsonType::Array, path);
        if (v == nullptr) return;
        if (v->size() != n) {
            fail(path, "expected " + std::to_string(n) + " numbers, got " + std::to_string(v->size()));
            return;
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (!v->at(i).is_number()) { fail(path, "element is not a number"); return; }
            out[i] = static_cast<float>(v->at(i).as_number());
        }
    }

    /// One point of a polyline, read positionally out of an array element.
    void point(const JsonValue& arr, std::size_t i, std::string_view path, float* out,
               std::size_t n) {
        if (!ok_) return;
        const JsonValue& p = arr.at(i);
        if (!p.is_array() || p.size() != n) {
            fail(path, "point " + std::to_string(i) + " must be " + std::to_string(n) + " numbers");
            return;
        }
        for (std::size_t k = 0; k < n; ++k) {
            if (!p.at(k).is_number()) { fail(path, "point element is not a number"); return; }
            out[k] = static_cast<float>(p.at(k).as_number());
        }
    }

private:
    bool ok_ = true;
    std::string error_{};
};

glm::vec2 vec2_of(Loader& ld, const JsonValue& obj, std::string_view key, std::string_view path) {
    float v[2]{};
    ld.floats(obj, key, path, v, 2);
    return {v[0], v[1]};
}

void load_bump(Loader& ld, const JsonValue& obj, std::string_view path, BumpStamp& out) {
    ld.only(obj, {"center", "radius", "height"}, path);
    out.center = vec2_of(ld, obj, "center", path);
    out.radius = ld.number(obj, "radius", path);
    out.height = ld.number(obj, "height", path);
}

SiteKind site_kind_of(Loader& ld, std::string_view s, std::string_view path) {
    if (s == "Hamlet") return SiteKind::Hamlet;
    if (s == "Shrine") return SiteKind::Shrine;
    if (s == "DungeonEntrance") return SiteKind::DungeonEntrance;
    if (s == "TowerRuin") return SiteKind::TowerRuin;
    ld.fail(path, "unknown site kind '" + std::string(s) + "'");
    return SiteKind::Shrine;
}

/// A carved corridor. `survey_space` says whether the engine's ascent and
/// footprint scaling apply — the crag tunnel was surveyed against a 52 m
/// Ravenscar and has to grow with the summit; the barrow passage is at valley
/// level and is not a function of the summit at all.
void load_corridor(Loader& ld, const JsonValue& obj, std::string_view path, glm::vec2 pivot,
                   CarveCorridor& out) {
// See the note at the waypoint transform below: the compiled defaults this
// asset replaces were folded WITHOUT a fused multiply-add, and at -O2 this
// translation unit would fuse. One ULP moves a generated byte (Rule 13.1).
#pragma clang fp contract(off)
    ld.only(obj, {"survey_space", "points", "half_width", "height", "daylight_portals"}, path);
    const bool survey = ld.boolean(obj, "survey_space", path);
    const JsonValue* pts = ld.member(obj, "points", serialization::JsonType::Array, path);
    if (pts == nullptr) return;
    const std::size_t n = pts->size();
    if (n < 2 || n > std::size(out.points)) {
        ld.fail(path, "point count must be 2.." + std::to_string(std::size(out.points)));
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        float p[3]{};
        ld.point(*pts, i, path, p, 3);
        if (!ld.ok()) return;
        // CONTRACTION OFF, and this is measured rather than stylistic.
        // The compiled table these waypoints replace is a constexpr aggregate
        // initializer, which clang evaluates WITHOUT fusing the multiply-add:
        // its wp6.z is 233.279999. The same expression compiled into this
        // library at -O2 fuses, giving 233.280014 — one ULP out, which is
        // enough to move a generated byte and Rule 13.1 does not let a
        // migration do that quietly.
        //
        // I got this backwards once and it is worth recording why: I measured
        // `TestbedLayout::spanz(225.6f)` in a printf and got the FUSED value,
        // concluded the compiled default was fused, and "fixed" the loader
        // with std::fma — which moved it further away. The premise was wrong,
        // not the arithmetic: a constant expression evaluated at a call site
        // is not the same evaluation as the one inside the aggregate
        // initializer that actually produces the default (Rule 34).
        //
        // The hazard itself is older than this file and survives it: that the
        // shipped tunnel geometry depends on whether the compiler fuses is a
        // determinism risk that predates the asset. This pragma pins the
        // migration to today's bytes; it does not make the geometry robust.
        out.points[i] = survey ? glm::vec3{pivot.x + (p[0] - pivot.x) * TestbedLayout::FOOTPRINT_SCALE,
                                           TestbedLayout::lift(p[1]),
                                           pivot.y + (p[2] - pivot.y) * TestbedLayout::FOOTPRINT_SCALE}
                               : glm::vec3{p[0], p[1], p[2]};
    }
    out.point_count = static_cast<int>(n);
    out.half_width = ld.number(obj, "half_width", path);
    out.height = ld.number(obj, "height", path);
    out.daylight_portals = ld.boolean(obj, "daylight_portals", path);
}

} // namespace

LayoutLoadResult load_layout(const JsonValue& root, TestbedLayout& out) {
    Loader ld;
    if (!root.is_object()) return {false, "root: document must be an object"};
    ld.only(root, {"stand", "erosion", "crag", "knoll", "bluff", "lake", "river", "troughs",
                   "sites", "corridors", "watchpoint", "castle", "carves", "forests"},
            "root");

    // --- stand + landform switches -------------------------------------
    if (const JsonValue* s = ld.member(root, "stand", serialization::JsonType::String, "stand")) {
        const std::string_view name = s->as_string();
        if (name == "Testbed") out.stand = StandId::Testbed;
        else if (name == "Forest") out.stand = StandId::Forest;
        else ld.fail("stand", "unknown stand '" + std::string(name) + "'");
    }
    out.erosion = ld.boolean(root, "erosion", "erosion");

    // --- crag ----------------------------------------------------------
    if (const JsonValue* c = ld.member(root, "crag", serialization::JsonType::Object, "crag")) {
        ld.only(*c, {"center", "rockline_frac", "treeline_frac", "arete_count", "ridge_cell",
                     "ridge_amp_frac", "ridge_amp_meters"}, "crag");
        out.crag.center = vec2_of(ld, *c, "center", "crag.center");
        // radius and peak_height are NOT in the file: they are NUMBERS rows
        // (L0_BASE_RADIUS, L0_RELIEF), i.e. registry, not map. The lines below
        // are FRACTIONS of the summit for exactly that reason — see the header.
        out.crag.rockline = out.crag.peak_height * ld.number(*c, "rockline_frac", "crag.rockline_frac");
        out.crag.treeline = out.crag.peak_height * ld.number(*c, "treeline_frac", "crag.treeline_frac");
        out.crag.arete_count = ld.integer(*c, "arete_count", "crag.arete_count");
        out.crag.ridge_cell = ld.number(*c, "ridge_cell", "crag.ridge_cell");
        out.crag.ridge_amp_frac = ld.number(*c, "ridge_amp_frac", "crag.ridge_amp_frac");
        out.crag.ridge_amp_meters = ld.number(*c, "ridge_amp_meters", "crag.ridge_amp_meters");
    }

    if (const JsonValue* k = ld.member(root, "knoll", serialization::JsonType::Object, "knoll")) {
        load_bump(ld, *k, "knoll", out.knoll);
    }
    if (const JsonValue* b = ld.member(root, "bluff", serialization::JsonType::Object, "bluff")) {
        load_bump(ld, *b, "bluff", out.bluff);
    }

    // --- lake / river ---------------------------------------------------
    if (const JsonValue* l = ld.member(root, "lake", serialization::JsonType::Object, "lake")) {
        ld.only(*l, {"center", "half_extent", "rim_rise", "rim_band_frac", "rim_fade_frac",
                     "outlet_dir", "outlet_bias"}, "lake");
        out.lake.center = vec2_of(ld, *l, "center", "lake.center");
        out.lake.half_extent = vec2_of(ld, *l, "half_extent", "lake.half_extent");
        out.lake.rim_rise = ld.number(*l, "rim_rise", "lake.rim_rise");
        out.lake.rim_band_frac = ld.number(*l, "rim_band_frac", "lake.rim_band_frac");
        out.lake.rim_fade_frac = ld.number(*l, "rim_fade_frac", "lake.rim_fade_frac");
        out.lake.outlet_dir = vec2_of(ld, *l, "outlet_dir", "lake.outlet_dir");
        out.lake.outlet_bias = ld.number(*l, "outlet_bias", "lake.outlet_bias");
    }
    if (const JsonValue* r = ld.member(root, "river", serialization::JsonType::Object, "river")) {
        ld.only(*r, {"source", "source_search_radius"}, "river");
        out.river.source = vec2_of(ld, *r, "source", "river.source");
        out.river.source_search_radius = ld.number(*r, "source_search_radius", "river.source_search_radius");
    }

    // --- drainage troughs ------------------------------------------------
    if (const JsonValue* t = ld.member(root, "troughs", serialization::JsonType::Array, "troughs")) {
        if (t->size() != std::size(out.troughs)) {
            ld.fail("troughs", "expected " + std::to_string(std::size(out.troughs)) + " troughs");
        } else {
            for (std::size_t i = 0; i < t->size() && ld.ok(); ++i) {
                const std::string path = "troughs[" + std::to_string(i) + "]";
                const JsonValue& o = t->at(i);
                if (!o.is_object()) { ld.fail(path, "must be an object"); break; }
                ld.only(o, {"points", "half_width", "floor_source", "floor_mouth", "wall_height",
                            "shoulder_frac"}, path);
                ValleyTrough& v = out.troughs[i];
                const JsonValue* pts = ld.member(o, "points", serialization::JsonType::Array, path);
                if (pts == nullptr) break;
                if (pts->size() < 2 || pts->size() > std::size(v.points)) {
                    ld.fail(path, "point count out of range");
                    break;
                }
                for (std::size_t j = 0; j < pts->size(); ++j) {
                    float p[2]{};
                    ld.point(*pts, j, path, p, 2);
                    v.points[j] = {p[0], p[1]};
                }
                v.point_count = static_cast<int>(pts->size());
                v.half_width = ld.number(o, "half_width", path);
                v.floor_source = ld.number(o, "floor_source", path);
                v.floor_mouth = ld.number(o, "floor_mouth", path);
                v.wall_height = ld.number(o, "wall_height", path);
                v.shoulder_frac = ld.number(o, "shoulder_frac", path);
            }
        }
    }

    // --- sites (ORDER IS LOAD-BEARING: WorldEntityIds follow it) ---------
    if (const JsonValue* s = ld.member(root, "sites", serialization::JsonType::Array, "sites")) {
        if (s->size() != std::size(out.sites)) {
            ld.fail("sites", "expected " + std::to_string(std::size(out.sites)) + " sites");
        } else {
            for (std::size_t i = 0; i < s->size() && ld.ok(); ++i) {
                const std::string path = "sites[" + std::to_string(i) + "]";
                const JsonValue& o = s->at(i);
                if (!o.is_object()) { ld.fail(path, "must be an object"); break; }
                ld.only(o, {"name", "position", "kind"}, path);
                out.sites[i].position = vec2_of(ld, o, "position", path);
                if (const JsonValue* k = ld.member(o, "kind", serialization::JsonType::String, path)) {
                    out.sites[i].kind = site_kind_of(ld, k->as_string(), path);
                }
            }
        }
    }

    // --- POI-chain corridors --------------------------------------------
    if (const JsonValue* c = ld.member(root, "corridors", serialization::JsonType::Array, "corridors")) {
        if (c->size() != std::size(out.corridors)) {
            ld.fail("corridors", "expected " + std::to_string(std::size(out.corridors)) + " corridors");
        } else {
            for (std::size_t i = 0; i < c->size() && ld.ok(); ++i) {
                const std::string path = "corridors[" + std::to_string(i) + "]";
                const JsonValue& o = c->at(i);
                if (!o.is_object()) { ld.fail(path, "must be an object"); break; }
                ld.only(o, {"name", "points"}, path);
                CorridorLayout& cl = out.corridors[i];
                const JsonValue* pts = ld.member(o, "points", serialization::JsonType::Array, path);
                if (pts == nullptr) break;
                if (pts->size() < 2 || pts->size() > std::size(cl.points)) {
                    ld.fail(path, "point count out of range");
                    break;
                }
                for (std::size_t j = 0; j < pts->size(); ++j) {
                    float p[2]{};
                    ld.point(*pts, j, path, p, 2);
                    cl.points[j] = {p[0], p[1]};
                }
                cl.point_count = static_cast<int>(pts->size());
            }
        }
    }

    out.watchpoint = vec2_of(ld, root, "watchpoint", "watchpoint");

    // --- castle ----------------------------------------------------------
    if (const JsonValue* c = ld.member(root, "castle", serialization::JsonType::Object, "castle")) {
        ld.only(*c, {"center", "approach_corridor"}, "castle");
        out.castle.center = vec2_of(ld, *c, "center", "castle.center");
        out.castle.approach_corridor = ld.integer(*c, "approach_corridor", "castle.approach_corridor");
    }

    // --- carves ----------------------------------------------------------
    if (const JsonValue* c = ld.member(root, "carves", serialization::JsonType::Object, "carves")) {
        ld.only(*c, {"crag_tunnel", "barrow_passage", "barrow_chamber", "barrow_site_index",
                     "lakeshore_site_index"}, "carves");
        // The survey pivot is the crag itself, read from the layout rather than
        // repeated as a literal: the footprint scales ABOUT the summit, so the
        // peak's coordinates would otherwise be map content living in two files.
        const glm::vec2 pivot = out.crag.center;
        if (const JsonValue* t = ld.member(*c, "crag_tunnel", serialization::JsonType::Object,
                                           "carves.crag_tunnel")) {
            load_corridor(ld, *t, "carves.crag_tunnel", pivot, out.carves.crag_tunnel);
        }
        if (const JsonValue* p = ld.member(*c, "barrow_passage", serialization::JsonType::Object,
                                           "carves.barrow_passage")) {
            load_corridor(ld, *p, "carves.barrow_passage", pivot, out.carves.barrow_passage);
        }
        if (const JsonValue* h = ld.member(*c, "barrow_chamber", serialization::JsonType::Object,
                                           "carves.barrow_chamber")) {
            ld.only(*h, {"center", "half_extent"}, "carves.barrow_chamber");
            float v[3]{};
            ld.floats(*h, "center", "carves.barrow_chamber.center", v, 3);
            out.carves.barrow_chamber.center = {v[0], v[1], v[2]};
            ld.floats(*h, "half_extent", "carves.barrow_chamber.half_extent", v, 3);
            out.carves.barrow_chamber.half_extent = {v[0], v[1], v[2]};
        }
        // "No lakeshore adit" is a VALUE, assigned rather than left to
        // whatever the caller handed us. The loader must fully determine the
        // layout: a field it merely declines to touch is indistinguishable
        // from a field it forgot, and the coverage control in the tests exists
        // precisely to make that distinction visible.
        out.carves.lakeshore_adit = CarveCorridor{};
        out.carves.barrow_site_index = ld.integer(*c, "barrow_site_index", "carves.barrow_site_index");
        out.carves.lakeshore_site_index =
            ld.integer(*c, "lakeshore_site_index", "carves.lakeshore_site_index");
    }

    // --- forest masses ----------------------------------------------------
    if (const JsonValue* f = ld.member(root, "forests", serialization::JsonType::Object, "forests")) {
        ld.only(*f, {"oak_rects", "pine_annulus_r0", "pine_annulus_r1", "pine_strip_count",
                     "pine_strip_duty", "pine_strip", "forced_clearing_center",
                     "forced_clearing_radius"}, "forests");
        ForestRegions& fr = out.forests;
        if (const JsonValue* r = ld.member(*f, "oak_rects", serialization::JsonType::Array,
                                           "forests.oak_rects")) {
            if (r->size() != std::size(fr.oak_rects)) {
                ld.fail("forests.oak_rects",
                        "expected " + std::to_string(std::size(fr.oak_rects)) + " rects");
            } else {
                for (std::size_t i = 0; i < r->size(); ++i) {
                    float v[4]{};
                    ld.point(*r, i, "forests.oak_rects", v, 4);
                    fr.oak_rects[i] = {v[0], v[1], v[2], v[3]};
                }
            }
        }
        fr.pine_annulus_r0 = ld.number(*f, "pine_annulus_r0", "forests.pine_annulus_r0");
        fr.pine_annulus_r1 = ld.number(*f, "pine_annulus_r1", "forests.pine_annulus_r1");
        fr.pine_strip_count = ld.number(*f, "pine_strip_count", "forests.pine_strip_count");
        fr.pine_strip_duty = ld.number(*f, "pine_strip_duty", "forests.pine_strip_duty");
        float v[4]{};
        ld.floats(*f, "pine_strip", "forests.pine_strip", v, 4);
        fr.pine_strip = {v[0], v[1], v[2], v[3]};
        fr.forced_clearing_center = vec2_of(ld, *f, "forced_clearing_center",
                                            "forests.forced_clearing_center");
        fr.forced_clearing_radius = ld.number(*f, "forced_clearing_radius",
                                              "forests.forced_clearing_radius");
    }

    return {ld.ok(), ld.error()};
}

LayoutLoadResult load_layout_file(const std::filesystem::path& path, TestbedLayout& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {false, path.string() + ": cannot open"};
    std::ostringstream text;
    text << in.rdbuf();
    const serialization::JsonParseResult parsed = serialization::json_parse(text.str());
    if (!parsed.ok) {
        return {false, path.string() + ":" + std::to_string(parsed.error.line) + ":"
                           + std::to_string(parsed.error.column) + ": " + parsed.error.message};
    }
    LayoutLoadResult r = load_layout(parsed.root, out);
    if (!r.ok) r.error = path.string() + ": " + r.error;
    return r;
}

} // namespace dfn::world
