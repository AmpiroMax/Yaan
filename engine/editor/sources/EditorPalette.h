/*
Created: 17:08:2026 - 19:13:38
Last updated: 17:08:2026 - 19:22:54
Module: engine/editor
File: engine/editor/sources/EditorPalette.h

Responsibility:
- THE OBJECT MENU'S MODEL. Which part is picked, and — because the shelf holds
  2411 of them — how a human finds the one he means: search, facets, favourites,
  recents, quick slots. Every decision here is data in / data out, so it is
  testable without a window (the interface half lives in EditorPaletteView.cpp).

Key items:
- PartFacets + parse_part_name(): the kit spells material, wear, size and the
  connector's facet count INTO the name; this reads them back.
- PaletteModel: the shelf, the query, the result, and what the builder keeps.
- PartMeasure: the MEASURED truth (render::measure_object), injected by the app
  for the rows actually on screen — never read here, never guessed here.

WHY THIS EXISTS (user, 17.08.2026): «нужно справа меню с возможностью выбора
объекта, не только стрелками их перебирать. меню должно открываться на какую-то
кнопку, и когда я его открываю я стою на месте, а мышкой кликаю по меню и
выбираю блок, которым буду строить».

THE NUMBER THAT DECIDES THE DESIGN IS 2411. At that size a list is not a menu,
it is a haystack: paging through it at 20 rows a screen is 121 screens. So the
order of work is search first, facets second, decoration last — and the two
features that actually carry a building session are FAVOURITES and RECENTS,
because a builder uses the same ten parts for hours.

TWO SOURCES FOR ONE SIZE, AND WHY THAT IS NOT TWO TRUTHS. The size FACET is
derived from the NAME, because it must answer over all 2411 rows on every
keystroke and a measurement needs a loaded mesh. The TOOLTIP shows the MEASURED
extent, because that is what will actually occupy space. They are reconciled by
a test over the whole shelf (EditorPaletteTests): the name may never promise
MORE than the mesh delivers. A disagreement there is a finding about the KIT —
it belongs to the houses zone, and it is never fixed by widening the tolerance.

Dependencies:
- Uses: glm, std. Deliberately NOT engine/render: the model must build and be
  tested without a registry, and measurements arrive as plain numbers.
- Used by: engine/app (App wires it to EditorUi), EditorPaletteView, tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO DICTIONARY OF MATERIALS. The facet vocabulary is gathered FROM THE SHELF,
  which is why a part baked tomorrow appears in the filter without a code
  change. A hand-written list of materials would go stale silently, which is
  the same defect build_palette() exists to avoid.
- A NAME THAT DOES NOT FIT THE GRAMMAR IS REFUSED OUT LOUD (parsed == false),
  never laid out by guess. The shelf-wide test asserts zero refusals, so a kit
  change that renames a family goes red in CI rather than mislabelling a row.
- User-facing text is a localization KEY here, never a string (Rule 5).
*/
/*
UPD:
- 17:08:2026 - 19:13:38: Создан — модель меню объектов: разбор имени на фасеты, фильтр, фасетные счётчики, избранное/недавние/слоты по карте, сохранение.
- 17:08:2026 - 19:22:54: Переезд в engine/editor. ARCHITECTURE.md разрешает Dear ImGui
  ТОЛЬКО в engine/editor, а слой editor не имеет права включать engine/app
  (LAYERS в tools/dag_check.py) — значит панель и её модель обязаны жить
  по одну сторону, и эта сторона — editor. Ни строки логики не тронуто.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::app {

/// THE UNIT THE KIT'S NAMES ARE WRITTEN IN. The same 0.25 m as BUILD_GRID_M in
/// BuildTool.h — spelled again rather than included because this header must
/// stay free of the build hand, and the two are pinned equal by a test rather
/// than by a comment (Rule 35: a promise in prose is not a mechanism).
inline constexpr float KIT_UNIT_M = 0.25f;

/// The panel's stable id, so App and the key handler name the same window.
inline constexpr const char* PALETTE_PANEL_ID = "parts";

/// How many quick slots the builder gets. 1..9 on the number row: the count is
/// the keyboard's, not a preference.
inline constexpr int PALETTE_QUICK_SLOTS = 9;

/// How many parts the "recently used" strip remembers per map. Long enough to
/// hold a working session's vocabulary, short enough that the strip stays a
/// strip — a recents list that needs scrolling is a second parts list.
inline constexpr std::size_t PALETTE_RECENTS_LIMIT = 12;

/// WHAT THE NAME SAYS ABOUT A PART. Every field is read back out of the name,
/// so nothing here can disagree with the shelf.
///
/// The grammar, as the forge writes it (engine/render/sources/PartForge.cpp):
///     <family>[-<style>]-<material>-<geometry...>[-<tags>]-w<NN>
/// and the one rule that makes it parseable without a vocabulary: the MATERIAL
/// is the token immediately before the first GEOMETRIC token. Geometric tokens
/// are AxBxC, dNN, nN/nr, hNN, NNu, holeAxBxCxD and wNN.
struct PartFacets {
    std::string name;      ///< the registry name, exactly as on the shelf
    std::string family;    ///< joint, wall, roof, ... (the leading token)
    std::string style;     ///< ashlar, boardv, framex, log, steep — may be empty
    std::string material;  ///< timber, stone, dark, brick, plaster, clay, ...
    /// Post-geometry words that are not sizes: blind / win1 / win2 / door
    /// (the opening), cap (a capital), polu (a half-hip). Kept as a list rather
    /// than as named fields on purpose — a tag the forge invents tomorrow
    /// becomes a filter chip without a code change.
    std::vector<std::string> tags;

    int wear_pct = -1;     ///< 30 / 50 / 80 from wNN; -1 = the name had none

    /// THE CONNECTOR'S FACET COUNT, and it is not decoration: it IS the angle
    /// the next wall may leave at (HOUSES.md §4 — n4 → 90°, n6 → 60°, n8 → 45°,
    /// round → any). A builder picks a post BY this number, so it is shown in
    /// the row, not only offered as a filter.
    /// -1 = the name carries no facet token; 0 = round (`nr`); else 4/6/8.
    int faces = -1;

    float diameter_m = 0.0f;  ///< dNN across flats (d50 → 0.50 m); 0 = none
    float height_m = 0.0f;    ///< hNN in grid units (h13 → 3.25 m); 0 = none
    float length_m = 0.0f;    ///< NNu (12u → 3.0 m); 0 = none
    int box_u[3] = {0, 0, 0}; ///< AxBxC in grid units; all zero = no box token

    /// STEPS IN A FLIGHT. Stairs are the one family whose triple is not a box:
    /// the forge writes (going_u, width_u, STEPS) and pins going_u at 1 for
    /// both pitches (PartForgeCatalogue.cpp — "so it stays 1 and the pair
    /// carries (width, STEPS)"). 0 for everything else.
    int steps = 0;

    /// THE SIZE FACET'S QUANTITY: the largest length the NAME states, in metres.
    /// 0 means the name does not state one (stairs), and the menu then shows the
    /// measured span instead of inventing a number.
    float span_m = 0.0f;

    /// False when the name did not fit the grammar. Such a part is still LISTED
    /// — losing a part from the menu is worse than showing it without facets —
    /// but it is never given guessed facets, and the shelf-wide test counts it.
    bool parsed = false;
};

/// Reads a shelf name into facets. Never throws; sets `parsed` false and leaves
/// the facets empty when the name does not fit the grammar.
[[nodiscard]] PartFacets parse_part_name(std::string_view name);

/// WHAT THE MESH SAYS, handed in by the app from render::measure_object — the
/// same measurement the judge and the build ghost use, never a second scan.
/// The model only stores and shows it.
struct PartMeasure {
    float width_m = 0.0f;   ///< x extent
    float depth_m = 0.0f;   ///< z extent
    float height_m = 0.0f;  ///< y extent (top - bottom)
    int triangles = 0;
    bool known = false;
};

/// Which pile of chips a facet value belongs to.
enum class FacetKind : std::uint8_t {
    Family = 0,   ///< joint, wall, roof, ...
    Material = 1, ///< timber, stone, ...
    Style = 2,    ///< ashlar, boardv, framex, ... (empty style is not offered)
    Wear = 3,     ///< 30 / 50 / 80, as "w30" style keys
    Faces = 4,    ///< n4 / n6 / n8 / round
    Tag = 5,      ///< blind, win1, win2, door, cap, polu
    Count = 6
};

/// One chip: a value that EXISTS on this shelf, how many rows choosing it would
/// leave, and whether it is chosen.
struct FacetValue {
    std::string value;         ///< the raw token; the view localizes it
    std::size_t count = 0;     ///< rows this chip would leave, given the OTHER
                               ///< facets and the search text (a facet count,
                               ///< not a shelf count — a chip reading 0 is a
                               ///< dead end and can be shown as one)
    bool on = false;
};

enum class PaletteSort : std::uint8_t {
    Name = 0,      ///< alphabetical: the shelf's own order, predictable
    Size = 1,      ///< smallest first; parts with no stated size go last
    SizeDesc = 2,
    Recent = 3,    ///< what this map used last, most recent first
};

enum class PaletteView : std::uint8_t { Grid = 0, List = 1 };

/// WHICH CHIPS ONE PART ANSWERS, as indices into the model's facet lists. Built
/// once per shelf so that a keystroke compares integers instead of rebuilding
/// six vectors of strings per row — at 2411 rows the string form cost 2.6 ms
/// per letter, a sixth of a frame for the act of typing.
struct PaletteChipRow {
    std::vector<std::uint16_t> of[static_cast<std::size_t>(FacetKind::Count)];
};

/// THE MENU'S MODEL. Holds the shelf, the query and everything the builder
/// keeps between sessions. No window, no ImGui, no filesystem except the one
/// state file it is explicitly asked to read or write.
class PaletteModel {
public:
    // -- the shelf ------------------------------------------------------------

    /// Replaces the shelf. Parses every name once; the facet vocabulary is
    /// rebuilt from what is actually there.
    void set_parts(std::vector<std::string> names);
    [[nodiscard]] std::size_t part_count() const { return parts_.size(); }
    [[nodiscard]] const PartFacets& part(std::size_t index) const;
    /// How many names did not fit the grammar. Non-zero is a finding, not a
    /// state to live with.
    [[nodiscard]] std::size_t unparsed_count() const { return unparsed_; }

    /// Where measurements come from. Called at most once per part and cached:
    /// the view asks only for the rows it is about to draw, so opening the menu
    /// does not read 2411 meshes. Returning false means "not known yet" and is
    /// not an error.
    using MeasureFn = std::function<bool(const std::string& name, PartMeasure& out)>;
    void set_measure_source(MeasureFn fn) { measure_ = std::move(fn); }
    /// The measurement for a part, fetching it once if the source can give it.
    [[nodiscard]] const PartMeasure& measure(std::size_t index) const;

    // -- the query ------------------------------------------------------------

    /// FILTER AS YOU TYPE. Case-folded substring over the name; a space means
    /// AND, so "wall stone door" narrows the way a human expects rather than
    /// looking for that exact string.
    void set_search(std::string text);
    [[nodiscard]] const std::string& search() const { return search_; }

    void toggle_facet(FacetKind kind, std::string_view value);
    void set_facet(FacetKind kind, std::string_view value, bool on);
    void clear_facets();
    [[nodiscard]] bool any_facet_on() const;
    /// The chips of one kind, in shelf order, with live counts.
    [[nodiscard]] const std::vector<FacetValue>& facet_values(FacetKind kind) const;

    void set_sort(PaletteSort sort);
    [[nodiscard]] PaletteSort sort() const { return sort_; }
    void set_view(PaletteView view) { view_ = view; }
    [[nodiscard]] PaletteView view() const { return view_; }

    /// Show only what this map's builder marked. Both are ordinary filters and
    /// stack with everything else.
    void set_only_favourites(bool on);
    [[nodiscard]] bool only_favourites() const { return only_favourites_; }

    // -- the result -----------------------------------------------------------

    /// Indices into the shelf, filtered and sorted. Recomputed only when the
    /// query changed.
    [[nodiscard]] const std::vector<std::size_t>& results() const;
    [[nodiscard]] std::size_t result_count() const { return results().size(); }
    /// True when the shelf is not empty but the query matched nothing — the
    /// view must say so in words. An empty list with no sentence reads as a
    /// broken menu, and the builder starts clicking to find out.
    [[nodiscard]] bool empty_result() const;

    // -- the cursor (keyboard) ------------------------------------------------

    /// Moves within the filtered result; clamps at both ends rather than
    /// wrapping (wrapping in a 2411-row list loses the reader's place).
    void move_cursor(int delta);
    void set_cursor(std::size_t at);
    [[nodiscard]] std::size_t cursor() const { return cursor_; }
    /// Enter: takes the row under the cursor. False if there is no row.
    bool take_cursor();

    // -- what is picked -------------------------------------------------------

    /// Picks a part by name. Also files it in this map's recents, because
    /// "recent" means "what I built with", and every path to a pick goes
    /// through here.
    void select(std::string_view name);
    [[nodiscard]] const std::string& selected() const { return selected_; }
    /// Index of the selection on the shelf, or part_count() if it is not there.
    [[nodiscard]] std::size_t selected_index() const;

    // -- what the builder keeps (PER MAP) -------------------------------------

    /// THE MAP THIS SESSION IS BUILDING. Favourites and recents are kept per
    /// map on purpose: the ten parts that matter to a town are not the ten that
    /// matter to a flora stand, and one shared list would get in both builders'
    /// way. Changing it swaps the kept lists; it does not touch the shelf.
    void set_map_id(std::string map_id);
    [[nodiscard]] const std::string& map_id() const { return map_id_; }

    void toggle_favourite(std::string_view name);
    [[nodiscard]] bool is_favourite(std::string_view name) const;
    [[nodiscard]] const std::vector<std::string>& favourites() const;

    /// Most-recently-used first, capped. Selecting already files here; this is
    /// for the app to call when a part is actually PLACED, which is a stronger
    /// signal than being looked at.
    void note_used(std::string_view name);
    [[nodiscard]] const std::vector<std::string>& recents() const;
    [[nodiscard]] static std::size_t recents_limit();

    /// Quick slots 1..9. Out-of-range is ignored rather than clamped: a stray
    /// key must not silently overwrite slot 1.
    void set_quick_slot(int slot, std::string_view name);
    [[nodiscard]] const std::string& quick_slot(int slot) const;
    /// Selects whatever slot `slot` holds. False if the slot is empty.
    bool take_quick_slot(int slot);

    // -- persistence ----------------------------------------------------------

    /// Reads the state file. Missing file is not an error: a first run has no
    /// favourites and that is a legal state, not a failure.
    bool load_state(const std::string& path);
    /// Writes every map's kept state, not just this one — the file is the whole
    /// editor's memory, and rewriting it from one map would forget the others.
    bool save_state(const std::string& path) const;

    /// The same two, as strings. This is what the tests drive, so persistence
    /// is provable without touching a disk.
    [[nodiscard]] std::string state_text() const;
    void load_state_text(std::string_view text);

private:
    struct MapState {
        std::string selected;
        std::vector<std::string> favourites;
        std::vector<std::string> recents;
        std::string slots[PALETTE_QUICK_SLOTS];
    };

    void rebuild_facets();
    void invalidate();
    void recompute() const;
    [[nodiscard]] MapState& state();
    [[nodiscard]] const MapState& state() const;

    std::vector<PartFacets> parts_;
    /// Lower-cased names, in parts_ order: the search folds the QUERY once and
    /// the shelf once, never the shelf per keystroke.
    std::vector<std::string> folded_;
    std::vector<PaletteChipRow> chips_;
    std::size_t unparsed_ = 0;
    mutable std::vector<PartMeasure> measures_;
    MeasureFn measure_;

    std::string search_;
    std::vector<std::string> search_terms_; ///< folded, split on spaces
    /// Mutable because the CHIP COUNTS are part of the result, not part of the
    /// query: recompute() fills them while answering a const question.
    mutable std::vector<FacetValue> facets_[static_cast<std::size_t>(FacetKind::Count)];
    PaletteSort sort_ = PaletteSort::Name;
    PaletteView view_ = PaletteView::Grid;
    bool only_favourites_ = false;

    mutable std::vector<std::size_t> results_;
    mutable bool dirty_ = true;
    std::size_t cursor_ = 0;

    std::string map_id_;
    std::string selected_;
    std::vector<std::pair<std::string, MapState>> maps_;
};

} // namespace dfn::app
