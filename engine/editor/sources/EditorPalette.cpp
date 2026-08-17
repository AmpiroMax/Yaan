/*
Created: 17:08:2026 - 19:13:38
Last updated: 17:08:2026 - 21:04:27
Module: engine/editor
File: engine/editor/sources/EditorPalette.cpp

Responsibility:
- The object menu's model, declared in EditorPalette.h.

Dependencies:
- Uses: EditorPalette.h, std. No render, no ImGui, no window.
- Used by: EditorPaletteView (the panel), App, tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE FACET VOCABULARY IS GATHERED, NEVER WRITTEN DOWN. If you find yourself
  typing a material or a family name into this file, stop: the shelf already
  knows, and a list here will disagree with it within a week.
- `stair` is the ONE family named in this file, and only to refuse to read its
  triple as a box. The forge writes (going_u, width_u, STEPS) there and pins the
  first number at 1 for both pitches; reading it as a length would report a
  3.5 m flight as 0.25 m. Every other family is handled structurally.
*/
/*
UPD:
- 17:08:2026 - 19:13:38: Создан вместе с EditorPalette.h.
- 17:08:2026 - 19:22:54: Переезд в engine/editor. ARCHITECTURE.md разрешает Dear ImGui
  ТОЛЬКО в engine/editor, а слой editor не имеет права включать engine/app
  (LAYERS в tools/dag_check.py) — значит панель и её модель обязаны жить
  по одну сторону, и эта сторона — editor. Ни строки логики не тронуто.
- 17:08:2026 - 19:37:50: index_of() и selected_index() через него — один поиск на всех.
- 17:08:2026 - 20:58:32: ДВЕ ПОТЕРЯННЫЕ СВОЙСТВА, обе найдены одним рукавом на различимость.
  (1) `hole` у настила выбрасывался — 16 настилов из 24 различаются ТОЛЬКО им,
  то есть меню держало восемь пар неотличимых строк. Теперь метка.
  (2) ширина марша выбрасывалась вместе со всей тройкой — все 76 лестниц
  схлопывались в 38 неотличимых пар. Теперь width_m.
- 17:08:2026 - 21:04:27: first_of_family() — двоичный поиск по началу имени: семейство это ведущий
  токен, значит на отсортированной полке оно непрерывно.
*/

#include "engine/editor/sources/EditorPalette.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace dfn::app {
namespace {

[[nodiscard]] std::string fold(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

[[nodiscard]] std::vector<std::string> split(std::string_view s, char sep) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at <= s.size()) {
        const std::size_t next = s.find(sep, at);
        const std::size_t end = next == std::string_view::npos ? s.size() : next;
        out.emplace_back(s.substr(at, end - at));
        if (next == std::string_view::npos) {
            break;
        }
        at = next + 1;
    }
    return out;
}

[[nodiscard]] bool all_digits(std::string_view s) {
    if (s.empty()) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) != 0;
    });
}

[[nodiscard]] int to_int(std::string_view s) {
    int v = 0;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

/// AxBxC of plain integers. Also the shape of the deck's `holeAxBxCxD`, which
/// is why the caller checks the `hole` prefix first.
[[nodiscard]] bool parse_box(std::string_view tok, int out[3]) {
    const std::vector<std::string> p = split(tok, 'x');
    if (p.size() != 3) {
        return false;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        if (!all_digits(p[i])) {
            return false;
        }
        out[i] = to_int(p[i]);
    }
    return true;
}

/// IS THIS TOKEN A SIZE? This predicate is the whole parser: the material is
/// the word immediately before the first token that answers yes. Nothing else
/// in this file needs to know what a material IS.
[[nodiscard]] bool is_geometry(std::string_view tok) {
    if (tok.empty()) {
        return false;
    }
    int box[3] = {0, 0, 0};
    if (parse_box(tok, box)) {
        return true;
    }
    if (tok.rfind("hole", 0) == 0) {
        return true;
    }
    if (tok[0] == 'd' || tok[0] == 'h' || tok[0] == 'w') {
        return all_digits(tok.substr(1));
    }
    if (tok[0] == 'n') {
        return tok == "nr" || all_digits(tok.substr(1));
    }
    return tok.size() > 1 && tok.back() == 'u' && all_digits(tok.substr(0, tok.size() - 1));
}

} // namespace

// ---------------------------------------------------------------------------
// The name
// ---------------------------------------------------------------------------

PartFacets parse_part_name(std::string_view name) {
    PartFacets f;
    f.name.assign(name);
    const std::vector<std::string> tok = split(name, '-');
    // A name needs at least family, material and one size token. Anything
    // shorter is not a shelf name, and guessing at it would put a row in the
    // menu whose facets are fiction.
    if (tok.size() < 3 || tok[0].empty()) {
        return f;
    }

    std::size_t geom_at = 0;
    for (std::size_t i = 1; i < tok.size(); ++i) {
        if (is_geometry(tok[i])) {
            geom_at = i;
            break;
        }
    }
    // geom_at == 1 means the token right after the family is already a size:
    // there is no material, so the name is outside the grammar. REFUSE rather
    // than borrow the family as a material.
    if (geom_at < 2) {
        return f;
    }

    f.family = tok[0];
    f.material = tok[geom_at - 1];
    for (std::size_t i = 1; i + 1 < geom_at; ++i) {
        f.style += (f.style.empty() ? "" : "-") + tok[i];
    }

    bool saw_wear = false;
    for (std::size_t i = geom_at; i < tok.size(); ++i) {
        const std::string& t = tok[i];
        int box[3] = {0, 0, 0};
        if (t.rfind("hole", 0) == 0) {
            // THE DECK'S DECLARED VOID IS A PROPERTY, NOT NOISE. Dropping it
            // cost 16 of the 24 decks their identity: they differ from each
            // other in NOTHING ELSE, so the menu offered eight pairs of rows
            // it could not tell apart and a property chooser could never
            // reach. Kept as a tag because that is what it is — a word in the
            // name that distinguishes the part — and because a tag needs no
            // new field, no new axis and no new vocabulary.
            f.tags.push_back(t);
            continue;
        }
        if (parse_box(t, box)) {
            f.box_u[0] = box[0];
            f.box_u[1] = box[1];
            f.box_u[2] = box[2];
            continue;
        }
        if (t[0] == 'w' && all_digits(std::string_view(t).substr(1))) {
            f.wear_pct = to_int(std::string_view(t).substr(1)) * 10;
            saw_wear = true;
            continue;
        }
        if (t[0] == 'd' && all_digits(std::string_view(t).substr(1))) {
            f.diameter_m = static_cast<float>(to_int(std::string_view(t).substr(1))) / 100.0f;
            continue;
        }
        if (t[0] == 'h' && all_digits(std::string_view(t).substr(1))) {
            f.height_m = static_cast<float>(to_int(std::string_view(t).substr(1))) * KIT_UNIT_M;
            continue;
        }
        if (t == "nr") {
            f.faces = 0; // round: any angle (HOUSES.md §4)
            continue;
        }
        if (t[0] == 'n' && all_digits(std::string_view(t).substr(1))) {
            f.faces = to_int(std::string_view(t).substr(1));
            continue;
        }
        if (t.size() > 1 && t.back() == 'u' &&
            all_digits(std::string_view(t).substr(0, t.size() - 1))) {
            f.length_m = static_cast<float>(to_int(std::string_view(t).substr(0, t.size() - 1))) *
                         KIT_UNIT_M;
            continue;
        }
        f.tags.push_back(t);
    }

    // Wear is written by every part the forge makes. A name without it is a
    // name from another grammar, and a menu row claiming "износ неизвестен"
    // teaches nothing — refuse and let the shelf test say so.
    if (!saw_wear) {
        return f;
    }

    // THE STAIR EXCEPTION, and it is a reading of the forge's contract rather
    // than an exception to it: for `stair` the triple is (going_u, width_u,
    // STEPS) and the first number is pinned at 1 for both pitches, so it is not
    // a length at all. Its span is left unstated; the measured extent fills it.
    if (f.family == "stair") {
        // (going_u, width_u, STEPS) — SceneStairRules.cpp names the same three.
        // The width is KEPT: it is the passage the player walks up, and dropping
        // it collapsed all 76 flights into 38 indistinguishable pairs, because a
        // 1.0 m and a 1.5 m flight of the same pitch differ in nothing else.
        f.width_m = static_cast<float>(f.box_u[1]) * KIT_UNIT_M;
        f.steps = f.box_u[2];
        f.box_u[0] = 0;
        f.box_u[1] = 0;
        f.box_u[2] = 0;
    }

    f.span_m = std::max({f.diameter_m, f.height_m, f.length_m,
                         static_cast<float>(f.box_u[0]) * KIT_UNIT_M,
                         static_cast<float>(f.box_u[1]) * KIT_UNIT_M,
                         static_cast<float>(f.box_u[2]) * KIT_UNIT_M});
    f.parsed = true;
    return f;
}

// ---------------------------------------------------------------------------
// The shelf
// ---------------------------------------------------------------------------

namespace {

/// The chip values one part carries for one facet kind. Zero values means the
/// part answers no chip of that kind — it then matches only when that kind has
/// nothing selected, which is what "this wall has no style" should mean.
void values_of(const PartFacets& p, FacetKind kind, std::vector<std::string>& out) {
    out.clear();
    switch (kind) {
    case FacetKind::Family:
        if (!p.family.empty()) {
            out.push_back(p.family);
        }
        break;
    case FacetKind::Material:
        if (!p.material.empty()) {
            out.push_back(p.material);
        }
        break;
    case FacetKind::Style:
        if (!p.style.empty()) {
            out.push_back(p.style);
        }
        break;
    case FacetKind::Wear:
        if (p.wear_pct >= 0) {
            out.push_back(std::to_string(p.wear_pct));
        }
        break;
    case FacetKind::Faces:
        if (p.faces == 0) {
            out.emplace_back("r");
        } else if (p.faces > 0) {
            out.push_back(std::to_string(p.faces));
        }
        break;
    case FacetKind::Tag:
        out = p.tags;
        break;
    case FacetKind::Count:
        break;
    }
}

} // namespace

void PaletteModel::set_parts(std::vector<std::string> names) {
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    parts_.clear();
    parts_.reserve(names.size());
    unparsed_ = 0;
    for (std::string& n : names) {
        parts_.push_back(parse_part_name(n));
        if (!parts_.back().parsed) {
            ++unparsed_;
        }
    }
    measures_.assign(parts_.size(), PartMeasure{});
    rebuild_facets();
    invalidate();
}

const PartFacets& PaletteModel::part(std::size_t index) const {
    static const PartFacets none;
    return index < parts_.size() ? parts_[index] : none;
}

const PartMeasure& PaletteModel::measure(std::size_t index) const {
    static const PartMeasure none;
    if (index >= parts_.size()) {
        return none;
    }
    PartMeasure& m = measures_[index];
    // Retried while unknown on purpose: "not measured yet" is a stage the app
    // passes through as it loads the shelf, not a permanent answer.
    if (!m.known && measure_) {
        PartMeasure got;
        if (measure_(parts_[index].name, got)) {
            got.known = true;
            m = got;
        }
    }
    return m;
}

void PaletteModel::rebuild_facets() {
    std::vector<std::string> vals;
    for (std::size_t k = 0; k < static_cast<std::size_t>(FacetKind::Count); ++k) {
        // Remembering what was on across a shelf swap: the builder changed map,
        // not mind, and re-ticking six chips is the kind of chore that makes a
        // filter go unused.
        std::vector<std::string> was_on;
        for (const FacetValue& v : facets_[k]) {
            if (v.on) {
                was_on.push_back(v.value);
            }
        }
        facets_[k].clear();
        for (const PartFacets& p : parts_) {
            values_of(p, static_cast<FacetKind>(k), vals);
            for (const std::string& v : vals) {
                const auto it = std::find_if(facets_[k].begin(), facets_[k].end(),
                                             [&v](const FacetValue& f) { return f.value == v; });
                if (it == facets_[k].end()) {
                    facets_[k].push_back({v, 0, false});
                }
            }
        }
        std::sort(facets_[k].begin(), facets_[k].end(),
                  [](const FacetValue& a, const FacetValue& b) { return a.value < b.value; });
        for (FacetValue& f : facets_[k]) {
            f.on = std::find(was_on.begin(), was_on.end(), f.value) != was_on.end();
        }
    }

    // The two caches the query runs on. They are derived, so they are rebuilt
    // here and nowhere else: a second place that fills them is a second place
    // that can forget to (Rule 39).
    folded_.clear();
    folded_.reserve(parts_.size());
    chips_.assign(parts_.size(), PaletteChipRow{});
    for (std::size_t i = 0; i < parts_.size(); ++i) {
        folded_.push_back(fold(parts_[i].name));
        for (std::size_t k = 0; k < static_cast<std::size_t>(FacetKind::Count); ++k) {
            values_of(parts_[i], static_cast<FacetKind>(k), vals);
            for (const std::string& v : vals) {
                const auto it = std::lower_bound(
                    facets_[k].begin(), facets_[k].end(), v,
                    [](const FacetValue& f, const std::string& s) { return f.value < s; });
                if (it != facets_[k].end() && it->value == v) {
                    chips_[i].of[k].push_back(
                        static_cast<std::uint16_t>(it - facets_[k].begin()));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// The query
// ---------------------------------------------------------------------------

void PaletteModel::set_search(std::string text) {
    search_ = std::move(text);
    search_terms_.clear();
    for (std::string& t : split(fold(search_), ' ')) {
        if (!t.empty()) {
            search_terms_.push_back(std::move(t));
        }
    }
    invalidate();
}

void PaletteModel::set_facet(FacetKind kind, std::string_view value, bool on) {
    for (FacetValue& f : facets_[static_cast<std::size_t>(kind)]) {
        if (f.value == value) {
            f.on = on;
            invalidate();
            return;
        }
    }
}

void PaletteModel::toggle_facet(FacetKind kind, std::string_view value) {
    for (FacetValue& f : facets_[static_cast<std::size_t>(kind)]) {
        if (f.value == value) {
            f.on = !f.on;
            invalidate();
            return;
        }
    }
}

void PaletteModel::clear_facets() {
    for (auto& kind : facets_) {
        for (FacetValue& f : kind) {
            f.on = false;
        }
    }
    invalidate();
}

bool PaletteModel::any_facet_on() const {
    for (const auto& kind : facets_) {
        for (const FacetValue& f : kind) {
            if (f.on) {
                return true;
            }
        }
    }
    return false;
}

const std::vector<FacetValue>& PaletteModel::facet_values(FacetKind kind) const {
    recompute();
    return facets_[static_cast<std::size_t>(kind)];
}

void PaletteModel::set_sort(PaletteSort sort) {
    sort_ = sort;
    invalidate();
}

void PaletteModel::set_only_favourites(bool on) {
    only_favourites_ = on;
    invalidate();
}

void PaletteModel::invalidate() { dirty_ = true; }

namespace {

/// Does this part survive the text and every facet kind EXCEPT `skip`? Passing
/// a kind to skip is what makes the chip counts mean "if I clicked this", which
/// is the only count worth showing: a chip labelled with a shelf total tells
/// the builder nothing about the list he is looking at.
///
/// `any_on` is precomputed by the caller once per query rather than once per
/// row — six scans of the chip lists per part is the same 2411x waste the
/// folded name was.
[[nodiscard]] bool passes(const std::string& folded, const PaletteChipRow& chips,
                          const std::vector<std::string>& terms,
                          const std::vector<FacetValue>* facets, const bool* any_on,
                          FacetKind skip) {
    for (const std::string& t : terms) {
        if (folded.find(t) == std::string::npos) {
            return false;
        }
    }
    for (std::size_t k = 0; k < static_cast<std::size_t>(FacetKind::Count); ++k) {
        if (!any_on[k] || static_cast<FacetKind>(k) == skip) {
            continue;
        }
        bool hit = false;
        for (const std::uint16_t at : chips.of[k]) {
            hit = hit || facets[k][at].on;
        }
        if (!hit) {
            return false;
        }
    }
    return true;
}

} // namespace

void PaletteModel::recompute() const {
    if (!dirty_) {
        return;
    }
    dirty_ = false;

    const MapState& st = state();
    bool any_on[static_cast<std::size_t>(FacetKind::Count)] = {};
    for (std::size_t k = 0; k < static_cast<std::size_t>(FacetKind::Count); ++k) {
        for (const FacetValue& f : facets_[k]) {
            any_on[k] = any_on[k] || f.on;
        }
    }

    // The favourites filter is a set lookup rather than a linear search per row:
    // the list is short, but the query runs it 2411 times.
    std::vector<const std::string*> fav_sorted;
    if (only_favourites_) {
        fav_sorted.reserve(st.favourites.size());
        for (const std::string& f : st.favourites) {
            fav_sorted.push_back(&f);
        }
        std::sort(fav_sorted.begin(), fav_sorted.end(),
                  [](const std::string* a, const std::string* b) { return *a < *b; });
    }
    const auto is_fav = [&fav_sorted](const std::string& name) {
        const auto it = std::lower_bound(
            fav_sorted.begin(), fav_sorted.end(), name,
            [](const std::string* a, const std::string& b) { return *a < b; });
        return it != fav_sorted.end() && **it == name;
    };

    results_.clear();
    results_.reserve(parts_.size());
    for (std::size_t i = 0; i < parts_.size(); ++i) {
        if (only_favourites_ && !is_fav(parts_[i].name)) {
            continue;
        }
        if (passes(folded_[i], chips_[i], search_terms_, facets_, any_on, FacetKind::Count)) {
            results_.push_back(i);
        }
    }

    const auto rank = [&st](const std::string& name) {
        const auto it = std::find(st.recents.begin(), st.recents.end(), name);
        return it == st.recents.end() ? st.recents.size()
                                      : static_cast<std::size_t>(it - st.recents.begin());
    };
    switch (sort_) {
    case PaletteSort::Name:
        break; // parts_ is already sorted by name
    case PaletteSort::Size:
    case PaletteSort::SizeDesc: {
        const bool asc = sort_ == PaletteSort::Size;
        std::stable_sort(results_.begin(), results_.end(), [this, asc](std::size_t a, std::size_t b) {
            // A part whose name states no size goes LAST in both directions:
            // "unknown" is not "zero", and sorting it to the top would put the
            // stairs above every screw in the ascending order.
            const float sa = parts_[a].span_m > 0.0f ? parts_[a].span_m : 1e9f;
            const float sb = parts_[b].span_m > 0.0f ? parts_[b].span_m : 1e9f;
            if (sa == sb) {
                return parts_[a].name < parts_[b].name;
            }
            if (sa >= 1e9f || sb >= 1e9f) {
                return sa < sb;
            }
            return asc ? sa < sb : sb < sa;
        });
        break;
    }
    case PaletteSort::Recent:
        std::stable_sort(results_.begin(), results_.end(),
                         [this, &rank](std::size_t a, std::size_t b) {
                             return rank(parts_[a].name) < rank(parts_[b].name);
                         });
        break;
    }

    // THE CHIP COUNTS, one pass per kind. Each pass leaves its OWN kind's
    // selection out of the filter, which is what makes a count read "this is
    // what clicking me would leave" rather than "this is what is already shown".
    for (std::size_t k = 0; k < static_cast<std::size_t>(FacetKind::Count); ++k) {
        const FacetKind kind = static_cast<FacetKind>(k);
        for (FacetValue& f : facets_[k]) {
            f.count = 0;
        }
        for (std::size_t i = 0; i < parts_.size(); ++i) {
            if (only_favourites_ && !is_fav(parts_[i].name)) {
                continue;
            }
            if (!passes(folded_[i], chips_[i], search_terms_, facets_, any_on, kind)) {
                continue;
            }
            for (const std::uint16_t at : chips_[i].of[k]) {
                ++facets_[k][at].count;
            }
        }
    }
}

const std::vector<std::size_t>& PaletteModel::results() const {
    recompute();
    return results_;
}

bool PaletteModel::empty_result() const { return !parts_.empty() && results().empty(); }

// ---------------------------------------------------------------------------
// The cursor and the pick
// ---------------------------------------------------------------------------

void PaletteModel::set_cursor(std::size_t at) {
    const std::size_t n = result_count();
    cursor_ = n == 0 ? 0 : std::min(at, n - 1);
}

void PaletteModel::move_cursor(int delta) {
    const std::size_t n = result_count();
    if (n == 0) {
        cursor_ = 0;
        return;
    }
    long long at = static_cast<long long>(cursor_) + delta;
    at = std::max<long long>(0, std::min<long long>(at, static_cast<long long>(n) - 1));
    cursor_ = static_cast<std::size_t>(at);
}

bool PaletteModel::take_cursor() {
    const std::vector<std::size_t>& r = results();
    if (cursor_ >= r.size()) {
        return false;
    }
    select(parts_[r[cursor_]].name);
    return true;
}

void PaletteModel::select(std::string_view name) {
    selected_.assign(name);
    MapState& st = state();
    st.selected = selected_;
    if (!selected_.empty()) {
        note_used(selected_);
    }
}

std::size_t PaletteModel::index_of(std::string_view name) const {
    const auto it = std::lower_bound(
        parts_.begin(), parts_.end(), name,
        [](const PartFacets& p, std::string_view n) { return p.name < n; });
    return (it != parts_.end() && it->name == name)
               ? static_cast<std::size_t>(it - parts_.begin())
               : parts_.size();
}

std::size_t PaletteModel::selected_index() const { return index_of(selected_); }

std::size_t PaletteModel::first_of_family(std::string_view family) const {
    const std::string prefix = std::string(family) + "-";
    const auto it = std::lower_bound(
        parts_.begin(), parts_.end(), prefix,
        [](const PartFacets& p, const std::string& n) { return p.name < n; });
    return (it != parts_.end() && it->family == family)
               ? static_cast<std::size_t>(it - parts_.begin())
               : parts_.size();
}

} // namespace dfn::app
