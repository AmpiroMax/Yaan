/*
Created: 17:08:2026 - 13:17:36
Last updated: 17:08:2026 - 13:46:59
Module: engine/render
File: engine/render/sources/PartForgeCatalogue.cpp

Responsibility:
- THE SHELF LIST: kit_catalogue() — every part the kit declares, expanded from
  family rows (kind x size x material x wear, connectors by shape x diameter,
  walls by style x opening). Moved VERBATIM out of PartForge.cpp in the
  family split (Rule 21); the geometry lives with its families, the LIST of
  what gets baked lives here.

Key items:
- kit_catalogue().

Dependencies:
- Uses: PartForge.h (PartParams, part_name).
- Used by: tools/forge_kit.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE SEED IS THE NAME (fnv1a over it): a part's wobble is its own and stays
  its own when the catalogue grows. Changing that re-bakes every shelf.
- Counts are asserted by the part-forge tests; a row added here without its
  count updated there is a red suite, on purpose.
*/
/*
UPD:
- 17:08:2026 - 13:17:36: Вынесен из PartForge.cpp дословно (разрез по семьям).
- 17:08:2026 - 13:23:56: ряды крыш: 3 новых покрытия на старых уклонах (36), пологий скат 8x4
  во всех пяти покрытиях (15), вальмы/полувальмы (36), дымники (4).
- 17:08:2026 - 13:46:59: крутые марши 45° (variant 1, -steep): ширины 4/6 x ступени
  8/11/12/13/14 x дерево/камень x 2 износа = 40; пологие 26.5° остаются
  улицам и террасам.
*/

#include "engine/render/sources/PartForge.h"

#include <initializer_list>
#include <vector>

namespace dfn::render {

std::vector<PartParams> kit_catalogue() {
    std::vector<PartParams> out;
    // One row per FAMILY, expanded as length x SECTION x material x wear. The
    // section is a PAIR and not two independent lists on purpose: real timber
    // comes in a handful of sizes (square post, laid-flat beam, batten), and a
    // free cross product would fill the kit with 200 near-identical sticks
    // nobody would ever choose between while the useful pieces stayed rare.
    struct Sec {
        int w;
        int h;
    };
    const auto add = [&out](PartKind kind, std::initializer_list<int> lengths,
                            std::initializer_list<Sec> sections,
                            std::initializer_list<PartMaterial> mats,
                            std::initializer_list<float> wears) {
        for (int L : lengths) {
            for (Sec s : sections) {
                for (PartMaterial m : mats) {
                    for (float w : wears) {
                        PartParams p;
                        p.kind = kind;
                        p.material = m;
                        p.length_u = L;
                        p.width_u = s.w;
                        p.height_u = s.h;
                        p.wear = w;
                        p.name = part_name(p);
                        // The seed is the NAME, so a part's wobble is its own
                        // and stays its own when the catalogue grows.
                        uint64_t h = 1469598103934665603ull;
                        for (unsigned char c : p.name) {
                            h = (h ^ c) * 1099511628211ull;
                        }
                        p.seed = h;
                        out.push_back(std::move(p));
                    }
                }
            }
        }
    };
    using PM = PartMaterial;
    const std::initializer_list<PM> woods = {PM::Timber, PM::TimberDark};
    const std::initializer_list<float> wear2 = {0.3f, 0.8f};

    // STICKS — the user's word, and the kit's spine: nearly everything else is
    // these at an angle. Sections in grid units: 1x1 batten, 2x1 laid flat,
    // 2x2 beam, 3x3 main post (25/50/75 cm).
    add(PartKind::Beam, {2, 4, 6, 8, 12, 16}, {{1, 1}, {2, 1}, {2, 2}, {3, 3}}, woods, wear2);
    add(PartKind::Post, {4, 6, 8, 10, 12, 16}, {{1, 1}, {2, 2}, {3, 3}}, woods, wear2);
    add(PartKind::Plank, {4, 8, 12, 16}, {{1, 1}, {2, 1}},
        {PM::Timber, PM::TimberDark, PM::Stone}, wear2);

    // ENCLOSURE. For a wall the section pair means (thickness, HEIGHT); for a
    // gable and a roof it means (thickness/depth, RISE) — the part's second
    // dimension is whatever that part is actually measured by.
    add(PartKind::WallPanel, {8, 12, 16}, {{1, 8}, {1, 10}, {1, 12}},
        {PM::Timber, PM::TimberDark, PM::Plaster, PM::Stone}, wear2);
    add(PartKind::Gable, {8, 12, 16}, {{1, 4}, {1, 6}, {1, 8}}, {PM::Timber, PM::Plaster},
        wear2);
    add(PartKind::RoofSlope, {8, 12, 16}, {{8, 6}, {8, 8}, {12, 8}, {12, 12}},
        {PM::Thatch, PM::Shingle}, wear2);
    // ВАРИАНТЫ КРЫШ (пользователь, 17.08): три новых покрытия на тех же
    // уклонах, плюс пологий скат {8,4} (26.6°) во всех пяти покрытиях — из
    // него собирается односкатная кровля сарая и навеса.
    add(PartKind::RoofSlope, {8, 12, 16}, {{8, 6}, {8, 8}, {12, 8}, {12, 12}},
        {PM::Timber, PM::Tile, PM::Turf}, {0.5f});
    add(PartKind::RoofSlope, {8, 12, 16}, {{8, 4}},
        {PM::Thatch, PM::Shingle, PM::Timber, PM::Tile, PM::Turf}, {0.5f});
    // Вальмовый скат (и полувальма вариантом) — length = eaves depth, section
    // = (run, rise), как у прямого ската.
    {
        for (int variant : {0, 1}) {
            for (PartMaterial mtl : {PM::Thatch, PM::Shingle, PM::Tile}) {
                for (int depth : {8, 12, 16}) {
                    for (const auto& pitch : {std::pair{8, 6}, std::pair{8, 8}}) {
                        PartParams p;
                        p.kind = PartKind::RoofHip;
                        p.material = mtl;
                        p.length_u = depth;
                        p.width_u = pitch.first;
                        p.height_u = pitch.second;
                        p.variant = variant;
                        p.wear = 0.5f;
                        p.name = part_name(p);
                        uint64_t hash = 1469598103934665603ull;
                        for (unsigned char c : p.name) {
                            hash = (hash ^ c) * 1099511628211ull;
                        }
                        p.seed = hash;
                        out.push_back(std::move(p));
                    }
                }
            }
        }
    }
    // Дымник на конёк, два размера и два дерева.
    add(PartKind::SmokeVent, {2, 3}, {{1, 1}}, {PM::Timber, PM::TimberDark},
        {0.5f});

    // GETTING IN, GETTING UP. A stair's length is unused (its run follows from
    // the step count), so it stays 1 and the pair carries (width, STEPS).
    // Two steps is in the list because a footing course is half a metre tall
    // and a three-step flight overshoots it — the kit has to be able to reach
    // the heights the kit's own parts make.
    add(PartKind::Stair, {1},
        {{4, 2}, {4, 3}, {4, 5}, {4, 7}, {6, 2}, {6, 5}, {6, 7}, {6, 9}, {8, 7}},
        {PM::Timber, PM::Stone}, wear2);
    // КРУТЫЕ МАРШИ (пользователь: «более крутые, чтобы на второй этаж за
    // длину этих доводили»): variant 1 — 45°, going 1u. Step counts follow
    // the FLOORS the kit's walls make: 11 legacy массовка, 12/13/14 the
    // dwelling wall row, 8 the half-storey loft (2.0 m). The gentle 26.5°
    // family above STAYS — streets and terraces need it; what a house needs
    // is a flight whose plan run equals its rise. Rise is 1u in both
    // families and immovable: PLAYER_STEP_HEIGHT 0.35 passes 0.25 and
    // refuses 0.50 (HOUSES.md §6) — a 2u-rise "steep" stair would be
    // unclimbable scenery, so it is not on this shelf.
    {
        for (int width : {4, 6}) {
            for (int steps : {8, 11, 12, 13, 14}) {
                for (PartMaterial mtl : {PM::Timber, PM::Stone}) {
                    for (float w : wear2) {
                        PartParams p;
                        p.kind = PartKind::Stair;
                        p.material = mtl;
                        p.variant = 1;
                        p.length_u = 1;
                        p.width_u = width;
                        p.height_u = steps;
                        p.wear = w;
                        p.name = part_name(p);
                        uint64_t hash = 1469598103934665603ull;
                        for (unsigned char c : p.name) {
                            hash = (hash ^ c) * 1099511628211ull;
                        }
                        p.seed = hash;
                        out.push_back(std::move(p));
                    }
                }
            }
        }
    }
    add(PartKind::DoorFrame, {4, 6}, {{1, 8}, {1, 10}, {2, 10}}, {PM::Timber, PM::Stone},
        wear2);
    add(PartKind::DoorLeaf, {3, 5}, {{1, 7}, {1, 9}}, woods, wear2);
    add(PartKind::WindowFrame, {3, 4, 6}, {{1, 3}, {1, 4}, {1, 6}}, woods, {0.5f});

    // GROUND AND YARD.
    add(PartKind::Footing, {4, 8, 12}, {{2, 2}, {2, 4}, {4, 4}}, {PM::Stone}, wear2);
    add(PartKind::Fence, {6, 8, 12}, {{1, 4}, {1, 6}}, woods, wear2);

    // СОЕДИНИТЕЛИ (HOUSES.md §3-4). Expanded by rule, like everything else:
    // shape (4/6/8/round facets) x across-flats diameter (35/50/75/100 cm,
    // derived d >= T + 0.1 from panel thickness T = 0.25) x material x height.
    // Every combination is forged even where a 0.35 octagon's facet (14.5 cm)
    // is too narrow for a 25 cm wall — the JUDGE owns that pairing rule and
    // needs the real rejected part to fail (Rule 30); thinner panels (fence
    // rails, shelf boards) still seat on it legally.
    const auto add_joint = [&out](std::initializer_list<int> shapes,
                                  std::initializer_list<int> diameters_cm,
                                  std::initializer_list<PartMaterial> mats,
                                  std::initializer_list<int> heights_u,
                                  std::initializer_list<int> variants,
                                  std::initializer_list<float> wears) {
        for (int n : shapes) {
            for (int d : diameters_cm) {
                for (PartMaterial m : mats) {
                    for (int h : heights_u) {
                        for (int v : variants) {
                            for (float w : wears) {
                                PartParams p;
                                p.kind = PartKind::JointPost;
                                p.material = m;
                                p.facets = n;
                                p.diameter_cm = d;
                                p.length_u = h;
                                p.variant = v;
                                p.wear = w;
                                p.name = part_name(p);
                                uint64_t hash = 1469598103934665603ull;
                                for (unsigned char c : p.name) {
                                    hash = (hash ^ c) * 1099511628211ull;
                                }
                                p.seed = hash;
                                out.push_back(std::move(p));
                            }
                        }
                    }
                }
            }
        }
    };
    // Timber joints: every shape and diameter, wall height (11u = 2.75 m) and
    // tall (16u = 4 m), no capitals — a capital on a log is not carpentry.
    add_joint({4, 6, 8, 0}, {35, 50, 75, 100}, woods, {11, 16}, {0}, wear2);
    // Masonry joints: stone and brick, with and without capitals. The 1.0 m
    // row is the castle colonnade's (HOUSES.md §3.1).
    add_joint({4, 6, 8, 0}, {35, 50, 75, 100}, {PM::Stone, PM::Brick}, {11, 16},
              {0, 1}, wear2);

    // ЛЕЖНИ: floor-to-wall bedding logs. 4-facet (flat bed, flat seat) and
    // round; the two smaller diameters — a 0.75 sleeper would eat the room's
    // headroom from below.
    {
        const std::initializer_list<int> lens = {8, 12, 16};
        for (int n : {4, 0}) {
            for (int d : {35, 50}) {
                for (PartMaterial m : {PM::Timber, PM::TimberDark, PM::Stone}) {
                    for (int L : lens) {
                        for (float w : wear2) {
                            PartParams p;
                            p.kind = PartKind::Sleeper;
                            p.material = m;
                            p.facets = n;
                            p.diameter_cm = d;
                            p.length_u = L;
                            p.wear = w;
                            p.name = part_name(p);
                            uint64_t hash = 1469598103934665603ull;
                            for (unsigned char c : p.name) {
                                hash = (hash ^ c) * 1099511628211ull;
                            }
                            p.seed = hash;
                            out.push_back(std::move(p));
                        }
                    }
                }
            }
        }
    }

    // ПЕРЕВЯЗКА ТОРЦОВ СРУБА: corner nodes at the two wall heights the log
    // panels come in (11u массовка, plus a low 6u for porches and sheds).
    add(PartKind::LogCorner, {6, 11}, {{1, 1}}, woods, wear2);

    // ВАРИАНТЫ СТЕН (пользователь, 17.08: «жду кучу разных вариантов стен»).
    // Style x material x length x opening, wall height 11u (2.75), one wear:
    // what varies here is CONSTRUCTION, and a wear twin of every construction
    // would double the shelf without adding a single choice.
    {
        struct StyleRow {
            int variant;
            std::initializer_list<PartMaterial> mats;
            std::initializer_list<int> openings;
        };
        const std::initializer_list<StyleRow> styles = {
            {1, {PM::Timber, PM::TimberDark}, {0, 1, 3}},           // сруб
            {2, {PM::Plaster, PM::Clay}, {0, 1, 2, 3}},             // фахверк, раскос
            {3, {PM::Plaster, PM::Clay}, {0, 1, 2, 3}},             // фахверк, крест
            {4, {PM::Plaster, PM::Clay}, {0, 1, 2, 3}},             // фахверк, К
            {5, {PM::Timber, PM::TimberDark}, {0, 1, 2, 3}},        // обшивка верт.
            {6, {PM::Timber, PM::TimberDark}, {0, 1, 2, 3}},        // обшивка гориз.
            {7, {PM::Stone}, {0, 1, 3}},                            // тёсаный камень
            {8, {PM::Stone}, {0, 3}},                               // бут
            {9, {PM::Brick}, {0, 1, 2, 3}},                         // кирпич
            {10, {PM::Stone}, {0, 1, 3}},                           // низ-камень
        };
        for (const StyleRow& row : styles) {
            for (PartMaterial mat : row.mats) {
                for (int len : {8, 12, 16}) {
                    for (int open : row.openings) {
                        PartParams p;
                        p.kind = PartKind::WallPanel;
                        p.material = mat;
                        p.variant = row.variant;
                        p.opening = open;
                        p.length_u = len;
                        p.width_u = 1;
                        p.height_u = 11;
                        p.wear = 0.5f;
                        p.name = part_name(p);
                        uint64_t hash = 1469598103934665603ull;
                        for (unsigned char c : p.name) {
                            hash = (hash ^ c) * 1099511628211ull;
                        }
                        p.seed = hash;
                        out.push_back(std::move(p));
                    }
                }
            }
        }
    }
    return out;
}

} // namespace dfn::render
