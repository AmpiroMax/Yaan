/*
Created: 10:08:2026 - 02:59:28
Last updated: 10:08:2026 - 20:40:52
Module: tests
File: tests/core/ForestStandTests.cpp

Responsibility:
- Forest stand suite (LANDSCAPE §8.1 / §2.10): the stand selector's testbed
  byte-identity guard (pinned heightmap hash), forest-stand determinism, and
  the LF-1/LF-2 landform acceptances WITH their named controls (Rule 30):
  grive elongation vs the isotropic round-bump control, amplitude
  distribution over the declared band (Rule 31), swale-floor continuity, the
  swale-hides-swale occlusion mechanism vs the flat-glade control.

Dependencies:
- Uses: doctest, dfn_world (Worldgen, WorldgenForest, WorldgenMacro),
  dfn_core (ContentHash).
- Used by: ctest (test_forest_stand).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The pinned testbed hash guards STAND-BRANCH NEUTRALITY, not eternity: it
  changes ONLY with a deliberate, design-acked testbed terrain change, and
  whoever changes it re-pins it in the same commit with the reason in the
  UPD block. Scatter/entities are deliberately NOT pinned (content passes
  evolve); the heightmap is the stable core.
*/
/*
UPD:
- 10:08:2026 - 02:59:28: Created — stand selector + LF-1/LF-2 acceptances.
- 10:08:2026 - 10:29:50: tensor_ratio window 72 m -> 320 m — THE INSTRUMENT WAS
  MEASURING ITSELF: inside one wavelength every smooth field reads elongated,
  so the isotropic control passed the acceptance it exists to fail (4.8
  against a 2.5 floor). At ~3 wavelengths aniso 3.10 / control 1.61.
  Continuity floor raised 0.5 -> 0.65 on the post-fix measurements.
- 10:08:2026 - 10:39:07: LF-8 acceptances — gullies vs the pass-OFF control, and the
  fan-association SYMMETRY test after the density instrument was measured
  passing a shuffled field at 1.000.
- 10:08:2026 - 10:50:58: §8.1 path network acceptances: BR-2 (real endpoints, measured
  overhead, the ornament control), BR-1 (hidden run, re-measured on the
  SHIPPED terrain, with the open-glade must-fail control), the three-band wear
  field on flora's datum, path flatness (curvature + cross tilt, after the
  max-min instrument was caught measuring slope), and the class rule.
- 10:08:2026 - 11:08:01: BR-6 cadence per regime with the tail clause, and BR-5 RECORDED
  AS AN OPEN DEFECT with its measurement — the landform cannot carry it while
  its swale floors percolate.
- 10:08:2026 - 11:11:16: PathClass ordinals pinned — flora's maintenance column maps to
  them positionally across a DAG seam no static_assert can reach.
- 10:08:2026 - 11:37:17: The vantage suite, and it is the reason three defects
  are not in the tree: the LF-2 standpoints were the argmin of a field that is
  EXACTLY ZERO over 55 % of the stand, so "the minimum" was a plateau of tied
  ties and the winner was whichever corner the scan reached first; the BR-1
  pair stood at 402 m against a 185 m control, which is two pictures rather
  than a control; and the first run of all of it was read off a STALE BINARY
  because a `head -20` on the build log hid the compile errors under the
  warnings (Rule 34 — the premise was unchecked). Adds: the cross-section
  agreement between PathNetwork::sample and core/math, the render handoff's
  exactness, GoalKind's ordinal pin, and the mechanical pairing rule that a
  control's label is its claim's label plus "_control" — a Rule 27 obligation
  only a human can check is one that stops being checked.
- 10:08:2026 - 11:51:23: §5.10 acceptances. The density test measured
  SNAG_DENSITY_OPEN over the WHOLE STAND and read 0.029/ha against a declared
  0.25-0.5 — a nine-fold miss that was entirely the denominator, since the oak
  mass covers this stand and "open ground" is the clearings alone. Fixed by
  exporting the placement's own domain predicates and dividing by the area the
  placement multiplied by. Plus: logs measured against the CONTOUR with the
  fold-uniform 0.785 rad of a drawn yaw as the reachable control, nothing dead
  on a tread, and the canopy envelope against its own old value.
- 10:08:2026 - 11:59:55: §5.11 acceptances, and one of them was rewritten
  because the first version could not fail: BR-3's headline ratio came out at
  ~27000 against an off-path ground carrying NO ground cover at all, since
  flora's ForestFloor rows have per_100m = 0 (the column is per 100 LINEAR
  metres and a forest floor is not a linear feature). Comparing against zero is
  not a measurement, so the claim asserted is design's actual ruling — the
  margin ORDERS by maintenance, hint-path > dirt > cobble > 0 — and the gap is
  reported rather than papered over.
- 10:08:2026 - 12:11:07: BR-3 measured against a real denominator at last —
  same-set, design's ruled reading (65655b2): margin 0.1077/m2 vs wood
  0.000730/m2, ratio 148. Logged, with the ORDERING clause as the gate, which
  is design's ruling and flora's recommendation. Plus the one-dimension-per-row
  invariant, whose §5.12 arm records the apron's three consumerless rows as a
  NAMED GAP so "the apron is done" cannot be inferred from a green run.
- 10:08:2026 - 12:15:37: TWO RULED-BUT-UNFIXED DEFECTS NAMED IN PLACE, both of
  them cases of a green assertion about the wrong thing. (1) The moss density
  above. (2) The tread-clearance case proves the ribbon clears the HEIGHT FIELD,
  and the field is not what occludes it: the drawn ground is the VOXEL surface
  at VOXEL_SIZE 1.0 m, on which a PATH_GROOVE_DEPTH of 0.15 m cannot exist at
  all (render's finding, measured by lift sweep). Left green on purpose — what
  it proves is still worth proving — but it does not prove the road is visible,
  and anything placed by HEIGHT and drawn against the VOXEL surface inherits
  the same gap.
- 10:08:2026 - 19:45:47: The tread-clearance case now proves the RIGHT object:
  a new case measures the tread against the surface actually drawn (chunk
  heightmap -> voxel volume -> surface nets), with the pre-pass field -- the
  ground this stand shipped this morning -- as a control that FAILS it (worst
  -0.287 m vs +0.100 m, median -0.006 m vs +0.146 m). Writing that control
  exposed a second defect in both cases: Approx(x).epsilon(0.25) is not a +-25%
  band, doctest's tolerance is eps*(scale + max|lhs|,|rhs|) with scale 1.0, so
  the band around 0.15 was -0.14..+0.44 m and a BURIED tread passed it. Both
  replaced with explicit bounds.
- 10:08:2026 - 19:59:10: The dead-wood "stands on the shipped ground" check was
  .epsilon(0.02) against a ~20 m height, i.e. a 0.42 m tolerance — it claimed
  the wood was on the ground while admitting a log floating knee-high. The
  quantity is a height ERROR and its threshold now sits in metres: worst
  deviation < 0.01 m, with the pre-path field as a control that must differ by
  more than 0.05 m so the assertion cannot be measuring nothing.
- 10:08:2026 - 20:13:53: BR-5 canary restated as a DIFFERENCE with a MEASURED
  denominator (design's amendment, 0c24946). The old form compared the siting
  median against a 0.06 literal that was twice a ground median design has now
  withdrawn as arithmetically impossible — a threshold derived from a number
  nobody can reproduce. The unchosen-ground median is now measured in the test
  beside the claim. The withdrawn figures are struck through rather than
  deleted: a number quoted into a design ruling should stay findable by whoever
  reads the ruling.
- 10:08:2026 - 20:20:20: probe caveat recorded (sim's catch): the three-arg
  build_voxel_volume omits the derived adit corridors and is exact only on a
  stand with no carves.
- 10:08:2026 - 20:40:52: THE max() FLOOR OVERSHOOTS THE AUTHORED EDGE DENSITY,
  measured after flora asked whether the max was deliberate. It is (design
  ruled max-not-product so cobble keeps a moss residual), but `base` is
  normalised by the EDGE RAMP'S integral, so wherever clump > edge*rich the
  realised count exceeds per_100m*rich. Cobble's FlowerCarpet weight is exactly
  0.0 and it realises ~20 per 100 m of its own route against dirt's 27 and
  faint-trail's 45 — a residual it is not. Recorded, not enforced: closing the
  gap between the ruling's intent and its effect is design's.
  Incidentally settles flora's bare-verge question in the direction they
  suspected: Dirt places 2.7x MORE than authored, so an empty 40 m of verge is
  drifts, not under-placement.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/core/math/sources/FloraEdgeRules.h"
#include "engine/core/math/sources/FloraField.h"
#include "engine/world/sources/WorldgenScatter.h"
#include "engine/world/sources/WorldgenVantages.h"
#include "engine/world/sources/VoxelMesh.h"
#include "engine/world/sources/VoxelVolume.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <functional>
#include <cmath>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <vector>

using namespace dfn;
using world::ChunkCoord;
using world::WorldGenParams;

namespace {

WorldGenParams forest_params() {
    WorldGenParams p{1, {0, 0}, {3, 3}};
    p.layout = world::forest_stand_layout();
    return p;
}

const world::WorldGenContext& forest() {
    static const world::WorldGenContext ctx = world::build_world_context(forest_params());
    return ctx;
}

/// Median of a scratch vector (sorts its copy).
float median(std::vector<float> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

/// Structure-tensor eigenvalue ratio of a scalar field over a 9x9 window —
/// the same instrument the §2.1 anisotropy invariant uses: elongated relief
/// reads high, round bumps read near 2.
///
/// THE WINDOW MUST SPAN SEVERAL WAVELENGTHS OR THE INSTRUMENT MEASURES
/// ITSELF. The first cut sampled 7x7 at 12 m = a 72 m window against a 100 m
/// grive wavelength: inside less than one wavelength EVERY smooth field is a
/// single flank, so its gradients are all parallel and the ratio is high by
/// construction. Measured across window spans (aniso / isotropic control):
/// 72 m -> 6.6 / 4.8, 150 m -> 3.7 / 2.2, 320 m -> 3.2 / 1.8. The control
/// only separates once the window holds ~3 wavelengths, which is where it is
/// pinned (9x9 at 40 m = 320 m). Gradient step stays 6 m — small against the
/// wavelength, so it estimates a gradient rather than a chord.
template <typename FieldFn>
float tensor_ratio(FieldFn&& field, glm::vec2 center) {
    float jxx = 0.0f, jzz = 0.0f, jxz = 0.0f;
    for (int iz = -4; iz <= 4; ++iz) {
        for (int ix = -4; ix <= 4; ++ix) {
            const glm::vec2 p{center.x + ix * 40.0f, center.y + iz * 40.0f};
            const float gx = field({p.x + 6.0f, p.y}) - field({p.x - 6.0f, p.y});
            const float gz = field({p.x, p.y + 6.0f}) - field({p.x, p.y - 6.0f});
            jxx += gx * gx;
            jzz += gz * gz;
            jxz += gx * gz;
        }
    }
    const float tr = jxx + jzz;
    const float disc = std::sqrt(std::max(0.0f, tr * tr - 4.0f * (jxx * jzz - jxz * jxz)));
    return ((tr + disc) * 0.5f) / std::max((tr - disc) * 0.5f, 1e-6f);
}

} // namespace

TEST_CASE("stand selector: the default testbed heightmap is byte-identical (pinned)") {
    // The stand branch must be INERT when layout.stand == Testbed. Pinned
    // from the pre-selector build (probe 10.08.2026: chunk (1,1) heightmap
    // FNV-1a 64). If this trips without a deliberate design-acked terrain
    // change, the selector has leaked into the testbed path.
    const world::WorldGenContext ctx = world::build_world_context(WorldGenParams{1, {0, 0}, {3, 3}});
    const world::Chunk c = world::generate_chunk(ctx, ChunkCoord{1, 1});
    serialization::Fnv1a64 h;
    h.update({reinterpret_cast<const std::byte*>(c.heightmap.samples.data()),
              c.heightmap.samples.size() * sizeof(uint16_t)});
    CHECK(h.digest() == 0xf187b04c05574f77ull);
}

TEST_CASE("forest stand: deterministic (Rule 13.1) and waterless by declaration") {
    const world::WorldGenContext a = world::build_world_context(forest_params());
    const world::WorldGenContext b = world::build_world_context(forest_params());
    const world::Chunk ca = world::generate_chunk(a, ChunkCoord{2, 1});
    const world::Chunk cb = world::generate_chunk(b, ChunkCoord{2, 1});
    REQUIRE(ca.heightmap.samples == cb.heightmap.samples);
    REQUIRE(ca.scatter.size() == cb.scatter.size());
    for (std::size_t i = 0; i < ca.scatter.size(); ++i) {
        CHECK(ca.scatter[i].position == cb.scatter[i].position);
        CHECK(ca.scatter[i].species == cb.scatter[i].species);
    }
    // Composition rule (§2.10 rule 4): no water landform is declared, so no
    // sample may carry a water surface and no entity may exist (P4 is empty).
    CHECK(a.hydrology.ok);
    CHECK(a.hydrology.stations.empty());
    for (float ws : ca.surface.water_surface) {
        CHECK(ws == math::NO_WATER);
    }
    CHECK(ca.entities.empty());
}

TEST_CASE("LF-2 grives are elongated; the isotropic round-bump control fails (Rule 30)") {
    // Acceptance: direction-coherent elongated grives — structure-tensor
    // median well above the isotropic reading. Control: the same field with
    // the anisotropic stretch OFF is the round-bump shape the user already
    // rejected (Запрос 1) and must sit below the floor.
    std::vector<float> aniso, iso;
    for (float wz = 80.0f; wz < 1000.0f; wz += 90.0f) {
        for (float wx = 80.0f; wx < 1000.0f; wx += 90.0f) {
            const glm::vec2 c{wx, wz};
            if (glm::length(c - glm::vec2{512.0f, 640.0f}) < 160.0f) {
                continue; // the glade taper is not the grive field
            }
            aniso.push_back(tensor_ratio(
                [](glm::vec2 p) { return world::forest_grive_component(1, p, false); }, c));
            iso.push_back(tensor_ratio(
                [](glm::vec2 p) { return world::forest_grive_component(1, p, true); }, c));
        }
    }
    REQUIRE(aniso.size() >= 20);
    const float m_aniso = median(aniso);
    const float m_iso = median(iso);
    INFO("aniso median ", m_aniso, " iso median ", m_iso);
    // Measured at the pinned 320 m window: aniso 3.10, control 1.61. The
    // threshold sits between them with headroom on both sides; note the
    // control is only BELOW it because the window spans ~3 wavelengths (see
    // tensor_ratio) — at the first cut's 72 m window the control read 4.8.
    CHECK(m_aniso >= 2.5f);
    CHECK(m_iso < 2.5f); // the control MUST fail the acceptance
}

TEST_CASE("LF-2 amplitude distribution covers the declared 2-5 m band (Rule 31)") {
    std::vector<float> amp;
    for (float z = 2.0f; z < 1024.0f; z += 8.0f) {
        for (float x = 2.0f; x < 1024.0f; x += 8.0f) {
            amp.push_back(world::forest_grive_amplitude(1, {x, z}));
        }
    }
    std::sort(amp.begin(), amp.end());
    const auto pct = [&](float q) {
        return amp[static_cast<std::size_t>(q * (amp.size() - 1))];
    };
    INFO("min ", pct(0.0f), " p10 ", pct(0.1f), " p90 ", pct(0.9f), " max ", pct(1.0f));
    // Both ends are assertions (Rule 30: a range is two claims) and the
    // quartiles assert the SHAPE — a bell that never leaves the middle of the
    // band passes a bounds check and fails these.
    CHECK(pct(0.0f) >= world::LF2_HILL_RELIEF_MIN - 0.01f);
    CHECK(pct(0.0f) <= world::LF2_HILL_RELIEF_MIN + 0.15f);
    CHECK(pct(1.0f) >= world::LF2_HILL_RELIEF_MAX - 0.15f);
    CHECK(pct(1.0f) <= world::LF2_HILL_RELIEF_MAX + 0.01f);
    CHECK(pct(0.1f) <= 2.7f);
    CHECK(pct(0.9f) >= 4.3f);
}

TEST_CASE("LF-2 realized relief sits in band; the glade is authored calm (в9)") {
    // 128 m windows of the grive component: median max-min relief must sit in
    // the ruled band. The glade must be CALM (meso suppressed) while still
    // carrying §2.7 micro — flat, not sterile.
    std::vector<float> relief;
    for (float wz = 0.0f; wz < 1024.0f; wz += 128.0f) {
        for (float wx = 0.0f; wx < 1024.0f; wx += 128.0f) {
            if (glm::length(glm::vec2{wx + 64.0f, wz + 64.0f} - glm::vec2{512.0f, 640.0f})
                < 200.0f) {
                continue;
            }
            float lo = 1e9f, hi = -1e9f;
            for (float z = wz; z < wz + 128.0f; z += 4.0f) {
                for (float x = wx; x < wx + 128.0f; x += 4.0f) {
                    const float v = world::forest_grive_component(1, {x, z}, false);
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
            relief.push_back(hi - lo);
        }
    }
    REQUIRE(relief.size() >= 20);
    const float med = median(relief);
    INFO("window relief median ", med);
    CHECK(med >= world::LF2_HILL_RELIEF_MIN);
    CHECK(med <= world::LF2_HILL_RELIEF_MAX + 0.2f);

    // Glade: the full FIELD (with taper) inside the authored plain.
    const auto& lay = forest().params.layout;
    const glm::vec2 gc = lay.forests.forced_clearing_center;
    float glo = 1e9f, ghi = -1e9f;
    for (float z = -60.0f; z <= 60.0f; z += 4.0f) {
        for (float x = -60.0f; x <= 60.0f; x += 4.0f) {
            const float h = world::terrain_height(forest(), gc + glm::vec2{x, z});
            glo = std::min(glo, h);
            ghi = std::max(ghi, h);
        }
    }
    // Base rolls (~10 m over 512 m -> ~1.4 m across 120 m) + micro (<=1.2 m
    // peak-to-peak) still exist; the 2-5 m grive tier must NOT.
    INFO("glade relief ", ghi - glo);
    CHECK(ghi - glo < 3.0f);
    // Micro retained: the glade is not a billiard table (LF-1's own
    // acceptance — the control is the pre-§2.7 flat plane).
    CHECK(ghi - glo > 0.4f);
}

TEST_CASE("swale floors are continuous and swales hide each other (LF-2 acceptance)") {
    // Floor mask on a 4 m grid: grive component below 0.05 m. Continuity:
    // the largest 4-connected floor component holds most floor cells — this
    // is what the fog pass (WEATHER W5) will pool into.
    constexpr int N = 256;
    std::vector<uint8_t> floor_mask(N * N, 0);
    int floor_count = 0;
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            const glm::vec2 p{x * 4.0f, z * 4.0f};
            if (world::forest_grive_component(1, p, false) < 0.05f) {
                floor_mask[static_cast<std::size_t>(z) * N + x] = 1;
                ++floor_count;
            }
        }
    }
    REQUIRE(floor_count > N * N / 20); // swales exist at all
    // Largest component by BFS.
    std::vector<int> comp(N * N, -1);
    int best = 0;
    std::vector<int> stack;
    int comp_id = 0;
    for (int i = 0; i < N * N; ++i) {
        if (!floor_mask[i] || comp[i] >= 0) continue;
        int size = 0;
        stack.push_back(i);
        comp[i] = comp_id;
        while (!stack.empty()) {
            const int c = stack.back();
            stack.pop_back();
            ++size;
            const int cx = c % N, cz = c / N;
            const int nb[4] = {cx > 0 ? c - 1 : -1, cx < N - 1 ? c + 1 : -1,
                               cz > 0 ? c - N : -1, cz < N - 1 ? c + N : -1};
            for (const int n : nb) {
                if (n >= 0 && floor_mask[n] && comp[n] < 0) {
                    comp[n] = comp_id;
                    stack.push_back(n);
                }
            }
        }
        best = std::max(best, size);
        ++comp_id;
    }
    INFO("floor cells ", floor_count, " largest component ", best);
    // Measured after the percolation fix (largest component / floor cells):
    // 1024 m domain seeds 1/2/3 -> 0.74 / 0.71 / 0.88; 2048 m -> 0.84 / 0.98 /
    // 0.94. It GROWS with the domain, which is the percolating signature; the
    // pre-fix 0.35 lip measured 0.36 / 0.46 / 0.55 at 1024 m and FELL to
    // 0.23 / 0.40 / 0.42 at 2048 m — disconnected potholes, growing apart.
    // 0.65 sits below every post-fix reading and above every pre-fix one, so
    // the rejected instance is the control (Rule 30).
    CHECK(static_cast<float>(best) >= 0.65f * static_cast<float>(floor_count));

    // Occlusion mechanism (BR-5's arithmetic, LF-2's acceptance): from a
    // swale-floor point, an eye-height ray to a floor point 80 m away is cut
    // by the crest between them for most such pairs. Control: the same
    // geometry on the authored GLADE is open ground and must NOT occlude —
    // a raycaster that reports occlusion there is measuring itself.
    const auto occluded = [&](glm::vec2 a, glm::vec2 b) {
        const float eye_a = world::terrain_height(forest(), a)
                          + static_cast<float>(config::PLAYER_EYE_HEIGHT);
        const float eye_b = world::terrain_height(forest(), b) + 0.5f; // a find-sized target
        for (float t = 0.1f; t < 0.95f; t += 0.05f) {
            const glm::vec2 p = a + (b - a) * t;
            const float ray_y = eye_a + (eye_b - eye_a) * t;
            if (world::terrain_height(forest(), p) > ray_y) {
                return true;
            }
        }
        return false;
    };
    int pairs = 0, hidden = 0;
    for (int z = 8; z < N - 8 && pairs < 400; z += 5) {
        for (int x = 8; x < N - 8 && pairs < 400; x += 5) {
            if (!floor_mask[static_cast<std::size_t>(z) * N + x]) continue;
            const glm::vec2 a{x * 4.0f, z * 4.0f};
            if (glm::length(a - glm::vec2{512.0f, 640.0f}) < 200.0f) continue;
            // find a floor point ~80 m away (any bearing)
            for (int dz = -20; dz <= 20; dz += 5) {
                const int bx = x + (dz == 0 ? 20 : 0), bz = z + dz;
                if (bx >= N || bz < 0 || bz >= N) continue;
                if (!floor_mask[static_cast<std::size_t>(bz) * N + bx]) continue;
                const glm::vec2 b{bx * 4.0f, bz * 4.0f};
                const float d = glm::length(b - a);
                if (d < 60.0f || d > 110.0f) continue;
                // only pairs with a crest between them are "neighboring swales"
                float crest = 0.0f;
                for (float t = 0.2f; t < 0.85f; t += 0.1f) {
                    crest = std::max(
                        crest, world::forest_grive_component(1, a + (b - a) * t, false));
                }
                if (crest < 1.5f) continue;
                ++pairs;
                if (occluded(a, b)) ++hidden;
                break;
            }
        }
    }
    REQUIRE(pairs >= 30);
    INFO("cross-crest pairs ", pairs, " hidden ", hidden);
    CHECK(static_cast<float>(hidden) >= 0.5f * static_cast<float>(pairs));

    // The flat-glade control: rays across the authored plain stay open.
    int open_pairs = 0, open_hidden = 0;
    for (float ang = 0.0f; ang < 6.2f; ang += 0.5f) {
        const glm::vec2 a = glm::vec2{512.0f, 640.0f}
                          + glm::vec2{std::cos(ang), std::sin(ang)} * 30.0f;
        const glm::vec2 b = glm::vec2{512.0f, 640.0f}
                          - glm::vec2{std::cos(ang), std::sin(ang)} * 30.0f;
        ++open_pairs;
        if (occluded(a, b)) ++open_hidden;
    }
    INFO("glade pairs ", open_pairs, " hidden ", open_hidden);
    CHECK(open_hidden <= open_pairs / 4);
}

TEST_CASE("LF-8 erosion cuts gullies the pass-OFF control lacks (§2.10, в17)") {
    // The dictionary's own clause: "the same map with the pass OFF must fail
    // the gully acceptance; if no frame can tell the difference, the pass is
    // decoration and does not land." The control is the SAME entry point with
    // layout.erosion = false, so what is compared is the pass, not two
    // generators.
    WorldGenParams po = forest_params();
    po.layout.erosion = false;
    const world::WorldGenContext off = world::build_world_context(po);
    const world::WorldGenContext& on = forest();

    CHECK(off.erosion.n == on.erosion.n); // same geometry, so the same instrument reads both
    for (const float v : off.erosion.delta) {
        REQUIRE(v == 0.0f);
    }

    // A gully is a NOTCH IN THE CROSS-SLOPE PROFILE: standing on a flank and
    // looking along the contour, the ground dips and comes back. Sampling
    // along the slope instead would count the slope itself.
    const auto notch_stations = [](const world::WorldGenContext& c) {
        int hits = 0;
        for (float wz = 100.0f; wz < 950.0f; wz += 17.0f) {
            for (float wx = 100.0f; wx < 950.0f; wx += 17.0f) {
                const glm::vec2 w{wx, wz};
                const float hx = world::terrain_height(c, {w.x + 4.0f, w.y})
                               - world::terrain_height(c, {w.x - 4.0f, w.y});
                const float hz = world::terrain_height(c, {w.x, w.y + 4.0f})
                               - world::terrain_height(c, {w.x, w.y - 4.0f});
                const float m = std::sqrt(hx * hx + hz * hz);
                if (m / 8.0f < 0.06f) {
                    continue; // flanks only: a swale floor has no cross-slope
                }
                const glm::vec2 across{-hz / m, hx / m};
                float prof[31];
                for (int i = 0; i < 31; ++i) {
                    prof[i] = world::terrain_height(
                        c, w + across * (static_cast<float>(i - 15) * 2.0f));
                }
                for (int i = 3; i < 28; ++i) {
                    const float l = std::max({prof[i - 1], prof[i - 2], prof[i - 3]});
                    const float r = std::max({prof[i + 1], prof[i + 2], prof[i + 3]});
                    if (prof[i] < l - 0.25f && prof[i] < r - 0.25f) {
                        ++hits;
                        break;
                    }
                }
            }
        }
        return hits;
    };
    const int hits_on = notch_stations(on);
    const int hits_off = notch_stations(po.layout.erosion ? on : off);
    INFO("gullied stations: pass ON ", hits_on, " vs control OFF ", hits_off);
    // Measured seed 1: ON 70-94 across the swept rates, OFF 5. The control is
    // not zero because the §2.7 micro octave makes a few 0.25 m dips of its
    // own — which is exactly why the threshold is a RATIO against the measured
    // control and not an absolute count.
    CHECK(hits_off <= 12);
    CHECK(hits_on >= 5 * std::max(1, hits_off));

    // The pass must DECORATE the landform, not replace it: the grives are
    // 2-5 m and the cut may not eat them.
    std::vector<float> d = on.erosion.delta;
    std::sort(d.begin(), d.end());
    const float p01 = d[d.size() / 100];
    const float p99 = d[d.size() * 99 / 100];
    INFO("delta p1 ", p01, " p99 ", p99, " min ", d.front(), " max ", d.back());
    CHECK(p01 > -1.0f);
    CHECK(p99 < 1.0f);
    CHECK(d.front() >= -1.51f);
}

TEST_CASE("LF-8 fans are ASSOCIATIVE — each sits below a gully (§2.10 acceptance)") {
    // "Fans appear where gullies exit onto lower ground — associative, each
    // fan explained by its gully."
    //
    // THE OBVIOUS INSTRUMENT DOES NOT WORK AND ITS CONTROL SAYS SO. "Is there
    // a gully cell within R of this fan cell" measures DENSITY, not
    // association: on a spatially shuffled delta field of the identical
    // distribution it returns 1.000, i.e. it passes a field with no structure
    // whatsoever. Measured, so recorded (Rule 30a: an instrument nothing can
    // fail is not an instrument).
    //
    // What does work is a SYMMETRY test. A fan is downslope of its gully, so
    // search a wedge UPHILL and the mirrored wedge DOWNHILL and compare. The
    // shuffled control has no preferred direction and reads ~1.00 by
    // construction, which makes the test self-controlling.
    const world::WorldGenContext& ctx = forest();
    const world::ErosionGrid& g = ctx.erosion;
    const world::TestbedLayout& lay = ctx.params.layout;
    const auto base = [&](glm::vec2 p) { return world::macro_height(1, lay, p); };

    std::vector<float> shuffled = g.delta;
    { // deterministic shuffle: the control must be reproducible too
        uint64_t s = 0x5EEDFACEull;
        for (std::size_t i = shuffled.size(); i > 1; --i) {
            s = s * 6364136223846793005ull + 1442695040888963407ull;
            std::swap(shuffled[i - 1], shuffled[(s >> 33) % i]);
        }
    }

    const auto association = [&](const std::vector<float>& d, float dir) {
        const int n = g.n;
        const auto at = [&](int x, int z) {
            return (x < 0 || z < 0 || x >= n || z >= n)
                     ? 0.0f
                     : d[static_cast<std::size_t>(z) * static_cast<std::size_t>(n)
                         + static_cast<std::size_t>(x)];
        };
        int fans = 0, explained = 0;
        for (int z = 12; z < n - 12; ++z) {
            for (int x = 12; x < n - 12; ++x) {
                if (at(x, z) < 0.25f) {
                    continue;
                }
                const glm::vec2 w{g.origin.x + static_cast<float>(x) * g.cell,
                                  g.origin.y + static_cast<float>(z) * g.cell};
                // Uphill is taken on the PRE-erosion field: the deposit whose
                // cause we are asking about must not be allowed to answer.
                const float hx = base({w.x + 6.0f, w.y}) - base({w.x - 6.0f, w.y});
                const float hz = base({w.x, w.y + 6.0f}) - base({w.x, w.y - 6.0f});
                const float m = std::sqrt(hx * hx + hz * hz);
                if (m < 0.05f) {
                    continue; // no defined uphill: not a specimen either way
                }
                ++fans;
                const float ux = dir * hx / m;
                const float uz = dir * hz / m;
                bool found = false;
                for (int s = 2; s <= 10 && !found; ++s) { // 8..40 m
                    for (int a = -1; a <= 1 && !found; ++a) {
                        const auto qx = x + static_cast<int>(std::lround(
                                                ux * s - static_cast<float>(a) * uz * s * 0.5f));
                        const auto qz = z + static_cast<int>(std::lround(
                                                uz * s + static_cast<float>(a) * ux * s * 0.5f));
                        if (at(qx, qz) <= -0.30f) {
                            found = true;
                        }
                    }
                }
                if (found) {
                    ++explained;
                }
            }
        }
        REQUIRE(fans > 500); // a distribution, not an anecdote (Rule 31)
        return static_cast<float>(explained) / static_cast<float>(fans);
    };

    const float up = association(g.delta, +1.0f);
    const float down = association(g.delta, -1.0f);
    const float c_up = association(shuffled, +1.0f);
    const float c_down = association(shuffled, -1.0f);
    INFO("real up ", up, " down ", down, " | shuffled up ", c_up, " down ", c_down);
    // Measured seed 1: real 0.549 / 0.375 (ratio 1.46), shuffled 0.825 / 0.819
    // (ratio 1.01). The shuffled field scores HIGHER in absolute terms and
    // still fails, which is the point of using the ratio.
    CHECK(up / down >= 1.30f);
    CHECK(c_up / c_down < 1.10f); // the control MUST fail the acceptance
}

namespace {

/// Eye-height visibility over the SHIPPED terrain, marched at BR-1's own 4 m
/// station spacing so the test and the generator use one instrument.
bool eye_visible(const world::WorldGenContext& c, glm::vec2 from, glm::vec2 to) {
    const float d = glm::length(to - from);
    if (d < 1e-3f) {
        return true;
    }
    const float eye = world::terrain_height(c, from) + static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const float tgt = world::terrain_height(c, to) + 2.0f;
    const int steps = std::max(2, static_cast<int>(d / 4.0f));
    for (int i = 1; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (world::terrain_height(c, from + (to - from) * t) > eye + (tgt - eye) * t) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("BR-2: every route ends at a real goal, near-shortest; the ornament fails") {
    const world::WorldGenContext& c = forest();
    const world::PathNetwork& net = c.paths;
    REQUIRE(net.goals.size() >= 4); // §8.1 item 2: 4-6 real goals
    REQUIRE(net.goals.size() <= 6);
    REQUIRE(net.routes.size() >= 4);

    float total = 0.0f;
    for (const world::PathRoute& r : net.routes) {
        // Clause (i): both endpoints are REGISTERED goals. Not a formality —
        // this is the clause with teeth (§1.7 names clause (ii)'s Rule 30a
        // trap explicitly: the generator IS a cost search, so (ii) can never
        // fail its own raw output).
        CHECK(r.goal_a >= 0);
        CHECK(r.goal_b >= 0);
        CHECK(r.goal_a < static_cast<int>(net.goals.size()));
        CHECK(r.goal_b < static_cast<int>(net.goals.size()));
        CHECK(glm::length(r.points.front() - net.goals[static_cast<std::size_t>(r.goal_a)].position)
              < 1.0f);
        CHECK(glm::length(r.points.back() - net.goals[static_cast<std::size_t>(r.goal_b)].position)
              < 1.0f);
        total += r.length_m;
    }
    // §8.1 item 3: >= 2 km of network, so BR-6's gap statistics are a
    // distribution and not an anecdote.
    INFO("total network ", total, " m");
    CHECK(total >= 2000.0f);

    const float ratio = net.max_detour_ratio();
    INFO("MEASURED detour overhead (max over routes) ", ratio);
    CHECK(ratio <= static_cast<float>(config::DETOUR_MAX));

    // THE ORNAMENT CONTROL (§1.7's must-fail case): a hand-drawn "scenic" S
    // between the same two goals, ignoring the cost field. It must blow the
    // ceiling — otherwise the ceiling admits painted paths.
    const world::PathRoute& r0 = net.routes.front();
    const glm::vec2 a = r0.points.front();
    const glm::vec2 b = r0.points.back();
    const glm::vec2 dir = glm::normalize(b - a);
    const glm::vec2 nrm{-dir.y, dir.x};
    std::vector<glm::vec2> ornament;
    for (int i = 0; i <= 64; ++i) {
        const float t = static_cast<float>(i) / 64.0f;
        ornament.push_back(a + (b - a) * t
                           + nrm * (std::sin(t * 6.2831853f * 2.0f) * 0.30f
                                    * glm::length(b - a)));
    }
    float ornament_len = 0.0f;
    for (std::size_t i = 1; i < ornament.size(); ++i) {
        ornament_len += glm::length(ornament[i] - ornament[i - 1]);
    }
    const float ornament_ratio = ornament_len / r0.optimal_length_m;
    INFO("ornament ratio ", ornament_ratio);
    CHECK(ornament_ratio > static_cast<float>(config::DETOUR_MAX)); // the control MUST fail
}

TEST_CASE("BR-1: the trace hides its destination; the open-glade control does not") {
    const world::WorldGenContext& c = forest();
    for (const world::PathRoute& r : c.paths.routes) {
        INFO("route ", r.goal_a, "->", r.goal_b, " longest hidden run ", r.longest_hidden_run_m);
        CHECK(r.longest_hidden_run_m >= static_cast<float>(config::HIDE_REVEAL_MIN_RUN_M));
    }
    // Re-measure independently on the SHIPPED terrain, not on the generator's
    // routing grid: a rule verified only against the surface the generator
    // used is a rule verified against the generator.
    const world::PathRoute& r = c.paths.routes.front();
    const glm::vec2 dest = c.paths.goals[static_cast<std::size_t>(r.goal_b)].position;
    float run = 0.0f, best = 0.0f;
    for (std::size_t i = 0; i + 1 < r.points.size(); ++i) {
        if (glm::length(r.points[i] - dest) < 4.0f) {
            continue; // Rule 36: at one station out, "the destination" is underfoot
        }
        if (!eye_visible(c, r.points[i], dest)) {
            run += glm::length(r.points[i + 1] - r.points[i]);
            best = std::max(best, run);
        } else {
            run = 0.0f;
        }
    }
    INFO("independently measured hidden run ", best);
    CHECK(best >= static_cast<float>(config::HIDE_REVEAL_MIN_RUN_M));

    // §1.7's NAMED MUST-FAIL CONTROL: a straight line across the preserved
    // plain (в9's glade) between two mutually visible points — zero occluded
    // stations by construction. A raycaster that reports concealment HERE is
    // measuring itself, not the composition.
    const glm::vec2 gc = c.params.layout.forests.forced_clearing_center;
    const glm::vec2 pa = gc + glm::vec2{-55.0f, 0.0f};
    const glm::vec2 pb = gc + glm::vec2{55.0f, 0.0f};
    float ctrl_run = 0.0f, ctrl_best = 0.0f;
    for (float t = 0.0f; t < 1.0f; t += 4.0f / 110.0f) {
        const glm::vec2 p = pa + (pb - pa) * t;
        if (glm::length(p - pb) < 4.0f) {
            continue;
        }
        if (!eye_visible(c, p, pb)) {
            ctrl_run += 4.0f;
            ctrl_best = std::max(ctrl_best, ctrl_run);
        } else {
            ctrl_run = 0.0f;
        }
    }
    INFO("glade control hidden run ", ctrl_best);
    CHECK(ctrl_best < static_cast<float>(config::HIDE_REVEAL_MIN_RUN_M));
}

TEST_CASE("the wear field: worn centre, pressed margins, rich edge OUTSIDE the tread") {
    const world::WorldGenContext& c = forest();
    const world::PathRoute& r = c.paths.routes.front();
    const std::size_t i = r.points.size() / 2;
    const glm::vec2 dir = glm::normalize(r.points[i + 1] - r.points[i - 1]);
    const glm::vec2 nrm{-dir.y, dir.x};

    const world::PathSample centre = c.paths.sample(r.points[i]);
    REQUIRE(centre.worn_half_width > 0.0f);
    CHECK(centre.wear > 0.95f);      // bare, trodden
    CHECK(centre.edge == 0.0f);      // BR-3 (i): ~0 decoration on the centre
    // FLORA'S DATUM: negative on the trodden surface, by construction.
    CHECK(centre.dist_from_worn_edge < 0.0f);

    float last_wear = 2.0f;
    float peak_edge = 0.0f;
    float peak_at = 0.0f;
    for (float t = 0.0f; t <= 6.0f; t += 0.25f) {
        const world::PathSample s = c.paths.sample(r.points[i] + nrm * t);
        CHECK(s.wear <= last_wear + 1e-4f); // monotone decreasing outward
        last_wear = s.wear;
        // The two claims are OPPOSITE claims about the same ground and may
        // never both be true: no decoration lives on the trodden surface.
        CHECK((s.wear <= 0.0f || s.edge <= 0.0f));
        if (s.edge > peak_edge) {
            peak_edge = s.edge;
            peak_at = s.dist_from_worn_edge;
        }
    }
    INFO("edge peak ", peak_edge, " at datum ", peak_at);
    CHECK(peak_edge > 0.9f);
    CHECK(peak_at > 0.0f);   // outside the worn edge, per the datum
    CHECK(peak_at < 1.0f);   // and hugging it, not drifting into the wood
    // Beyond the band the field is off (BR-3 (iii): monotone to nothing).
    CHECK(c.paths.sample(r.points[i] + nrm * 8.0f).edge == 0.0f);
}

TEST_CASE("a path is FLATTER than its surroundings (§8.1 item 1)") {
    const world::WorldGenContext& c = forest();
    std::vector<float> curv_on, curv_off, tilt_on, tilt_off;
    for (const world::PathRoute& r : c.paths.routes) {
        for (std::size_t i = 3; i + 3 < r.points.size(); ++i) {
            const glm::vec2 d = glm::normalize(r.points[i + 1] - r.points[i - 1]);
            const glm::vec2 nrm{-d.y, d.x};
            // SECOND DIFFERENCE, not max-min: a max-min window on a constant
            // slope measures the SLOPE. The first instrument did exactly that
            // and reported the path only 15% flatter than open ground while
            // the cross-section was in fact dead level.
            const auto curv = [&](glm::vec2 q) {
                return std::fabs(world::terrain_height(c, q - d * 3.0f)
                                 - 2.0f * world::terrain_height(c, q)
                                 + world::terrain_height(c, q + d * 3.0f));
            };
            const auto tilt = [&](glm::vec2 q) {
                return std::fabs(world::terrain_height(c, q + nrm)
                                 - world::terrain_height(c, q - nrm));
            };
            curv_on.push_back(curv(r.points[i]));
            tilt_on.push_back(tilt(r.points[i]));
            // The control is the ground 14 m away: same terrain, same
            // instrument, no path.
            curv_off.push_back(curv(r.points[i] + nrm * 14.0f));
            tilt_off.push_back(tilt(r.points[i] + nrm * 14.0f));
        }
    }
    REQUIRE(curv_on.size() > 100);
    const float c_on = median(curv_on);
    const float c_off = median(curv_off);
    const float t_on = median(tilt_on);
    const float t_off = median(tilt_off);
    INFO("curvature ON ", c_on, " OFF ", c_off, " | cross tilt ON ", t_on, " OFF ", t_off);
    CHECK(c_off >= 3.0f * c_on);  // measured 5.7x
    CHECK(t_on <= 0.02f);         // level across the tread: measured 0.000
    CHECK(t_off > 0.015f);        // the control is not level
}

TEST_CASE("path classes follow their RULE — and the steps class has no ground here yet") {
    const world::WorldGenContext& c = forest();
    int seen[4] = {0, 0, 0, 0};
    float max_grade = 0.0f;
    for (const world::PathRoute& r : c.paths.routes) {
        for (std::size_t i = 0; i < r.classes.size(); ++i) {
            ++seen[static_cast<int>(r.classes[i])];
            if (i > 0) {
                const float dh = std::fabs(world::terrain_height(c, r.points[i])
                                           - world::terrain_height(c, r.points[i - 1]));
                max_grade = std::max(max_grade,
                                     dh / std::max(0.1f, glm::length(r.points[i] - r.points[i - 1])));
            }
        }
    }
    INFO("cobble ", seen[0], " dirt ", seen[1], " faint ", seen[2], " steps ", seen[3],
         " | max route grade ", max_grade);
    // Three classes are BUILT and each appears where its rule says.
    CHECK(seen[0] > 0); // cobble: the approach to the largest goal
    CHECK(seen[1] > 0); // dirt: between goals
    CHECK(seen[2] > 0); // faint: thinning toward the small goals

    // THE FOURTH CLASS IS HONEST ABOUT ITSELF. StoneSteps is implemented and
    // its rule is live, but this stand's DECLARED landforms (LF-1 + LF-2:
    // 2-5 m relief over ~100 m) never produce a route grade above 0.22 — the
    // measured maximum is 0.12 even where the router deliberately stops
    // contouring on the summit approach. Steps arrive with LF-5 (rocky crest /
    // outcrop), which §8.1 declares and which is not built yet. Lowering the
    // threshold until the class appeared would put stone stairs on a lawn and
    // certify nothing; the test therefore asserts the RULE holds, not that the
    // class is populated.
    CHECK(max_grade < 0.22f);
    CHECK(seen[3] == 0);
}

TEST_CASE("BR-6: the find cadence holds in both regimes, tail clause included") {
    const world::WorldGenContext& c = forest();
    REQUIRE(!c.finds.empty()); // THE CONTROL IS THE REAL REJECTED INSTANCE: the
                               // world before this pass had no find layer at
                               // all, so every gap was infinite. Any threshold
                               // above zero encounters stands above it.
    const float R = static_cast<float>(config::FIND_ENCOUNTER_RADIUS);

    // A scripted walk per regime — §1.7 measures walks, not maps.
    const auto walk = [&](const std::vector<glm::vec2>& route, world::FindRegime regime) {
        std::vector<float> gaps;
        std::vector<char> used(c.finds.size(), 0);
        float since = 0.0f;
        for (std::size_t i = 1; i < route.size(); ++i) {
            since += glm::length(route[i] - route[i - 1]);
            for (std::size_t k = 0; k < c.finds.size(); ++k) {
                if (used[k] != 0 || c.finds[k].regime != regime) {
                    continue;
                }
                if (glm::length(c.finds[k].position - route[i]) <= R) {
                    used[k] = 1;
                    gaps.push_back(since);
                    since = 0.0f;
                }
            }
        }
        std::sort(gaps.begin(), gaps.end());
        return gaps;
    };

    // ONE SCRIPTED WALK PER ROUTE, then the gap lists are merged. Concatenating
    // the routes into a single polyline was the first cut and it TELEPORTED
    // between route ends: the jump entered the statistics as a 953 m gap and
    // failed the tail clause with an artefact of the instrument. A walk that
    // teleports is not a walk.
    std::vector<float> road_gaps;
    for (const world::PathRoute& r : c.paths.routes) {
        const std::vector<float> g = walk(r.points, world::FindRegime::NearRoad);
        road_gaps.insert(road_gaps.end(), g.begin(), g.end());
    }
    std::sort(road_gaps.begin(), road_gaps.end());
    const float road_spacing = world::find_spacing_m(world::FindRegime::NearRoad);
    REQUIRE(road_gaps.size() >= 10); // §8.1 item 3: >= 10 gaps per regime
    const float road_med = road_gaps[road_gaps.size() / 2];
    INFO("road: ", road_gaps.size(), " gaps, median ", road_med, " m, max ", road_gaps.back(),
         " m, regime spacing ", road_spacing);
    CHECK(road_med >= road_spacing * 0.6f);
    CHECK(road_med <= road_spacing * 1.4f);
    // THE TAIL CLAUSE IS THE POINT (Rule 31): a mean can hide a desert.
    CHECK(road_gaps.back() <= road_spacing * static_cast<float>(config::FIND_GAP_MAX_MULT));

    // Cross-country: a long zigzag that does not follow the network.
    std::vector<glm::vec2> wild;
    for (int leg = 0; leg < 8; ++leg) {
        glm::vec2 a{60.0f + static_cast<float>(leg) * 110.0f, 60.0f};
        glm::vec2 b{60.0f + static_cast<float>(leg) * 110.0f, 960.0f};
        if (leg % 2 != 0) {
            std::swap(a, b);
        }
        for (float t = 0.0f; t <= 1.0f; t += 4.0f / 900.0f) {
            wild.push_back(a + (b - a) * t);
        }
    }
    const std::vector<float> wild_gaps = walk(wild, world::FindRegime::Wilderness);
    const float wild_spacing = world::find_spacing_m(world::FindRegime::Wilderness);
    REQUIRE(wild_gaps.size() >= 10);
    const float wild_med = wild_gaps[wild_gaps.size() / 2];
    INFO("wild: ", wild_gaps.size(), " gaps, median ", wild_med, " m, regime spacing ",
         wild_spacing);
    CHECK(wild_med >= wild_spacing * 0.6f);
    CHECK(wild_med <= wild_spacing * 1.4f);
    // The two regimes must be DISTINGUISHABLE — в20's whole point is that one
    // is denser than the other, and FIND_NEAR_ROAD_MULT / FIND_WILD_MULT were
    // chosen as the smallest pair that survives Poisson spread.
    CHECK(wild_med > road_med * 1.5f);
}

TEST_CASE("BR-5 on this stand: the siting works, the LANDFORM does not (open defect)") {
    // BR-5 asks that a find be occluded from >= FIND_OCCLUSION_FRAC of a
    // 40-80 m eye-height ring. ON THIS STAND'S BARE GROUND IT IS NOT, and this
    // test records the measurement instead of pretending otherwise.
    //
    // THE PER-RING FIGURES BELOW ARE WITHDRAWN (design, 10.08.2026, §1.7
    // amended in 0c24946). They were recorded as a per-distance pair and are
    // arithmetically impossible as one: two equal groups cannot have medians
    // 0.03 and 0.06 while the POOLED median of the same draw is 0.1042, which
    // is what the generator actually recorded and what the new instrument
    // reproduces to four decimals. They were almost certainly a pooled reading
    // mislabelled per-distance — the exact ambiguity Rule 30's aggregation
    // clause uses this rule as its worked example of.
    //
    // The measured per-distance control is now 0.0000 / 0.0417 / 0.2083 at
    // 40 / 60 / 80 m (tests/core/FindOcclusionTests.cpp). Kept here struck
    // through rather than deleted, because a number that was quoted into a
    // design ruling should stay findable by whoever reads that ruling:
    //   [WITHDRAWN] ring 40+80 m: p50 0.03 / ring 80 m: p50 0.06
    //
    // THE CAUSE IS A CONFLICT BETWEEN TWO RATIFIED REQUIREMENTS ON ONE
    // LANDFORM, not a placement bug. LF-2's swale floors must be CONTINUOUS
    // (fog pools there, WEATHER W5), and continuity is percolation: 55% of the
    // ground sits at floor level in ONE connected network. A connected level
    // floor is precisely a network of long open sightlines. The same fix that
    // made fog possible is what opened the sightlines.
    //
    // Escalated to the lead as a NUMBERS row. Candidate resolutions, none of
    // them core's to pick: shorten LF2_HILL_WAVELENGTH so a 40 m ring always
    // crosses a crest; measure the ring at 60-80 m; or — most likely correct —
    // accept that BR-5 measured on BARE TERRAIN is the wrong instrument for a
    // FOREST stand, since §8.1's own frame (e) names logs and bushes as the
    // sightline breakers and LF-7 is not built yet.
    const world::WorldGenContext& c = forest();
    std::vector<float> wild_occ;
    for (const world::Find& f : c.finds) {
        if (f.regime == world::FindRegime::Wilderness) {
            wild_occ.push_back(f.occluded_fraction);
        }
    }
    REQUIRE(wild_occ.size() >= 20);
    const float med = median(wild_occ);
    INFO("wilderness find occlusion median ", med);

    // What IS assertable today: the SITING WORKS — finds are placed on the
    // most-occluded candidate in their cell, so they beat the ground they
    // stand on. THE DENOMINATOR IS MEASURED HERE rather than carried as a
    // literal: the 0.06 that used to sit in this line was twice a ground
    // median that has since been withdrawn, which made it a threshold derived
    // from a number nobody could reproduce.
    std::vector<float> ground_occ;
    for (const world::Find& f : c.finds) {
        // The same ray, aimed at ground the placement did NOT choose: one
        // find-spacing away, perpendicular to nothing in particular. This is
        // the "ground's own occlusion" the siting has to beat.
        ground_occ.push_back(world::occluded_fraction_at(
            [&](glm::vec2 q) { return world::terrain_height(c, q); }, {},
            f.position + glm::vec2{37.0f, 23.0f}, 80.0f, 24));
    }
    const float ground_med = median(ground_occ);
    INFO("siting median ", med, " against unchosen-ground median ", ground_med);
    CHECK(med > ground_med);
    // And the defect is pinned, so it cannot be quietly "fixed" by a threshold
    // drifting down: if this ever passes, BR-5 has become satisfiable and the
    // test must be rewritten into the real gate.
    CHECK(med < static_cast<float>(config::FIND_OCCLUSION_FRAC));
}

TEST_CASE("PathClass ordinals are a cross-zone contract and are pinned") {
    // Flora's PathClassRichness (FloraEdgeRules.h) maps to these ordinals
    // POSITIONALLY, and world and render are siblings in the DAG: neither
    // declaration can see the other, so no static_assert can catch a reorder.
    // A permutation here does not fail to compile — it gardens a cobbled
    // gutter and leaves a hint-path swept, silently. This test is the only
    // thing standing between those two facts.
    CHECK(static_cast<uint8_t>(world::PathClass::Cobble) == 0);
    CHECK(static_cast<uint8_t>(world::PathClass::Dirt) == 1);
    CHECK(static_cast<uint8_t>(world::PathClass::FaintTrail) == 2);
    CHECK(static_cast<uint8_t>(world::PathClass::StoneSteps) == 3);
    // Widths are read across the seam too (render sizes its splat from them).
    CHECK(world::path_half_width(world::PathClass::Cobble)
          > world::path_half_width(world::PathClass::Dirt));
    CHECK(world::path_half_width(world::PathClass::Dirt)
          > world::path_half_width(world::PathClass::StoneSteps));
    CHECK(world::path_half_width(world::PathClass::StoneSteps)
          > world::path_half_width(world::PathClass::FaintTrail));
}

TEST_CASE("GoalKind ordinals are a cross-zone contract and are pinned") {
    // Same disease as PathClass, one struct along: math::PathGoalMark carries
    // the kind as a bare uint8_t because render cannot name world's enum, and
    // render switches on it to pick a marker. A renumber here does not fail to
    // compile anywhere — it just puts a shrine's marker on a woodcutter's hut.
    CHECK(static_cast<uint8_t>(world::GoalKind::ClearingShrine) == 0);
    CHECK(static_cast<uint8_t>(world::GoalKind::Spring) == 1);
    CHECK(static_cast<uint8_t>(world::GoalKind::WoodcuttersHut) == 2);
    CHECK(static_cast<uint8_t>(world::GoalKind::SpireGroup) == 3);
    CHECK(static_cast<uint8_t>(world::GoalKind::CrestCairn) == 4);
}

TEST_CASE("the path cross-section has ONE definition and PathSample calls it") {
    // Render draws the tread per pixel from math::path_wear_profile /
    // path_edge_profile and flora plants the verge against the same two. If
    // PathNetwork::sample ever grows its own copy of either ramp, the verge
    // walks off the edge it was measured against and nothing fails — so the
    // agreement is asserted rather than assumed.
    const world::WorldGenContext& c = forest();
    const world::PathNetwork& net = c.paths;
    REQUIRE(!net.routes.empty());
    const world::PathRoute& r = net.routes.front();
    const auto mid = r.points.size() / 2;
    const glm::vec2 t = glm::normalize(r.points[mid + 1] - r.points[mid]);
    const glm::vec2 n{-t.y, t.x};
    int checked = 0;
    for (float off = 0.0f; off <= 4.0f; off += 0.1f) {
        const world::PathSample s = net.sample(r.points[mid] + n * off);
        CHECK(s.wear
              == doctest::Approx(math::path_wear_profile(s.dist_to_center / s.worn_half_width))
                     .epsilon(1e-5));
        CHECK(s.edge
              == doctest::Approx(math::path_edge_profile(s.dist_from_worn_edge,
                                                         net.rich_edge_band_m))
                     .epsilon(1e-5));
        ++checked;
    }
    CHECK(checked > 30);
    // The two ramps are OPPOSITE claims about the same ground, so nowhere may
    // both be positive: that would be ground both trodden bare and richly
    // vegetated. The check has a case that can fail it — the offsets above
    // sweep straight through the worn edge where a sloppy ramp would overlap.
    for (float off = 0.0f; off <= 4.0f; off += 0.05f) {
        const world::PathSample s = net.sample(r.points[mid] + n * off);
        CHECK_FALSE((s.wear > 0.0f && s.edge > 0.0f));
    }
    // And the peak sits where design put it: OUTSIDE the worn edge by a boot's
    // width, not at the edge itself.
    CHECK(math::path_edge_profile(0.0f, 2.5f) == doctest::Approx(0.0f));
    CHECK(math::path_edge_profile(math::PATH_EDGE_PEAK_M, 2.5f) == doctest::Approx(1.0f));
    CHECK(math::path_edge_profile(2.5f, 2.5f) == doctest::Approx(0.0f));
}

TEST_CASE("the render handoff carries the whole network and nothing invented") {
    const world::WorldGenContext& c = forest();
    std::vector<math::PathStation> stations;
    std::vector<uint32_t> offsets;
    std::vector<math::PathGoalMark> goals;
    world::path_render_stations(c.paths, stations, offsets, goals);

    REQUIRE(offsets.size() == c.paths.routes.size() + 1);
    CHECK(offsets.front() == 0);
    CHECK(offsets.back() == stations.size());
    CHECK(goals.size() == c.paths.goals.size());
    for (std::size_t ri = 0; ri < c.paths.routes.size(); ++ri) {
        const world::PathRoute& r = c.paths.routes[ri];
        REQUIRE(offsets[ri + 1] - offsets[ri] == r.points.size());
        for (std::size_t k = 0; k < r.points.size(); ++k) {
            const math::PathStation& st = stations[offsets[ri] + k];
            CHECK(st.position.x == doctest::Approx(r.points[k].x));
            CHECK(st.tread_height == doctest::Approx(r.heights[k]));
            CHECK(st.path_class == static_cast<uint8_t>(r.classes[k]));
            CHECK(st.worn_half_width == doctest::Approx(world::path_half_width(r.classes[k])));
        }
    }
    // RENDER'S LOAD-BEARING ASSUMPTION, checked here rather than in render's
    // head: a ribbon drawn AT tread_height must clear the flattened ground, or
    // it z-fights along its whole length. Measured, not asserted from the
    // formula — the formula is what could be wrong.
    //
    // This is the clearance against the PLACED field. The clearance against the
    // ground the player actually SEES is a separate and, until 10.08.2026, a
    // failing claim — see "the drawn ground is the placed ground" below, which
    // is the assertion that proves the road is visible.
    float worst_clearance = 1e9f;
    for (const world::PathRoute& r : c.paths.routes) {
        for (std::size_t k = 1; k + 1 < r.points.size(); ++k) {
            worst_clearance =
                std::min(worst_clearance, r.heights[k] - world::terrain_height(c, r.points[k]));
        }
    }
    INFO("worst tread-above-ground clearance ", worst_clearance, " m");
    CHECK(worst_clearance > 0.0f);
    // Explicit bounds. This was Approx(GROOVE).epsilon(0.25), which sounds like
    // +-25% and is not: doctest's tolerance is eps*(scale + max|lhs|,|rhs|)
    // with scale 1.0, so the band was -0.14..+0.44 m and a buried tread passed
    // it. Same idiom, same file, same change (Rule 32).
    CHECK(worst_clearance > static_cast<float>(config::PATH_GROOVE_DEPTH) * 0.75f);
    CHECK(worst_clearance < static_cast<float>(config::PATH_GROOVE_DEPTH) * 1.25f);
}

TEST_CASE("the drawn ground IS the placed ground, and the pre-pass field is the control") {
    // THE OBJECT UNDER TEST IS THE SURFACE THE PLAYER SEES (Rule 38: assert the
    // outcome). Everything this engine places — the tread, moss, pebbles, every
    // scatter root — is placed by terrain_height(); everything drawn and
    // collided comes out of the chunk heightmap, through the voxel volume, out
    // of surface nets. Those were two different grounds on this stand until
    // 10.08.2026, because generate_chunk carried its own copy of the pass stack
    // and the copy was never told about LF-8 erosion or the path flatten.
    //
    // Measured then, on chunk (1,2), 116 stations: worst tread clearance
    // against the DRAWN surface -0.663 m (the road buried two thirds of a metre
    // under its own ground), median drawn-minus-placed on open ground
    // -0.56..+0.81 m at the 1st/99th percentile and +-1.5 m at the extremes,
    // which is the erosion overlay's own clamp appearing verbatim.
    //
    // NOT a voxel-lattice defect: VOXEL_SIZE 1.0 m against PATH_GROOVE_DEPTH
    // 0.15 m was the plausible mechanism and it was wrong. Surface nets
    // reconstructs the height field to +-0.03 m on open ground (arm measured
    // with a vertex-snap refinement in and out — identical to 4 decimal places,
    // so the refinement was dropped rather than shipped as a fix for nothing).
    const world::WorldGenContext& c = forest();
    const auto CHUNK = static_cast<float>(config::CHUNK_SIZE);
    const ChunkCoord cc{1, 2}; // the chunk carrying the most path stations (163)

    // The drawn surface, as a query: the highest upward-facing extracted vertex
    // within half a metre of a point. Half a metre because a 1 m lattice puts
    // 1-4 vertices in that disc, and what buries a ribbon is the HIGHEST of
    // them, not their average.
    const auto drawn_surface = [&](const world::VoxelMeshData& m, glm::vec2 q) {
        float top = -1e9f;
        for (std::size_t i = 0; i < m.positions.size(); ++i) {
            if (m.normals[i].y <= 0.0f) continue;
            const glm::vec3& v = m.positions[i];
            const float dx = v.x - q.x;
            const float dz = v.z - q.y;
            if (dx * dx + dz * dz > 0.36f) continue;
            top = std::max(top, v.y);
        }
        return top;
    };
    // Worst and median tread clearance over the stations inside `cc`, against
    // whatever ground `mesh` describes. AGGREGATION AND DENOMINATOR, named:
    // the denominator is the stations of this chunk, interior ones only (a
    // route endpoint is a goal marker, not a tread), and the two aggregations
    // are the minimum and the median over them.
    const auto tread_clearance = [&](const world::VoxelMeshData& mesh) {
        std::vector<float> cl;
        for (const world::PathRoute& r : c.paths.routes) {
            for (std::size_t k = 1; k + 1 < r.points.size(); ++k) {
                const glm::vec2 q = r.points[k];
                if (static_cast<int>(std::floor(q.x / CHUNK)) != cc.x) continue;
                if (static_cast<int>(std::floor(q.y / CHUNK)) != cc.z) continue;
                const float top = drawn_surface(mesh, q);
                if (top < -1e8f) continue;
                cl.push_back(r.heights[k] - top);
            }
        }
        REQUIRE(cl.size() > 50);
        return std::pair<float, float>{*std::min_element(cl.begin(), cl.end()), median(cl)};
    };

    const world::Chunk chunk = world::generate_chunk(c, cc);
    const auto shipped_height = [&](glm::vec2 w) { return world::terrain_height(c, w); };
    // THREE ARGS IS CORRECT HERE AND WRONG ON THE TESTBED (sim's catch).
    // generate_chunk passes a FOURTH — the derived entrance adit corridors —
    // and omitting it extracts a surface with solid rock where the tunnel is.
    // This stand has none (forest_stand_layout zeroes the crag, the barrow and
    // both carves), so the two calls agree exactly. Anyone copying this pattern
    // to the testbed must pass the derived carves or they are measuring a world
    // the game does not ship.
    const world::VoxelMeshData drawn = world::extract_surface_nets(
        world::build_voxel_volume(chunk, shipped_height, c.params.layout));
    // EXPLICIT BOUNDS, NOT Approx().epsilon(). doctest's epsilon is not a
    // relative band: its tolerance is eps * (scale + max|lhs|,|rhs|) with scale
    // defaulting to 1.0, so ".epsilon(0.25)" against 0.15 admits anything from
    // -0.14 to +0.44 m — it would have accepted a tread buried 14 cm under the
    // ground, which is most of the defect this case exists to reject. Found by
    // the control below passing when it had to fail.
    const auto GROOVE = static_cast<float>(config::PATH_GROOVE_DEPTH);
    const auto [worst, mid] = tread_clearance(drawn);
    INFO("drawn-surface tread clearance: worst ", worst, " m, median ", mid, " m");
    CHECK(worst > 0.0f);
    CHECK(mid > GROOVE * 0.75f);
    CHECK(mid < GROOVE * 1.25f);

    // THE CONTROL IS THE REAL REJECTED INSTANCE (Rule 30): the ground this
    // stand actually shipped this morning — macro + erosion, with the path
    // flatten missing, which is precisely what generate_chunk used to write.
    // It must FAIL the assertion above, and it must fail it on the tread
    // rather than by being a different terrain: away from the paths this field
    // and the shipped one are the same field.
    world::Chunk uncarved = chunk;
    const auto prepath_height = [&](glm::vec2 w) {
        return world::macro_height(c.params.seed, c.params.layout, w) + c.erosion.sample(w);
    };
    const auto RES = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
    const auto STEP = static_cast<float>(config::HEIGHTMAP_STEP);
    for (uint32_t z = 0; z < RES; ++z) {
        for (uint32_t x = 0; x < RES; ++x) {
            const glm::vec2 w{static_cast<float>(cc.x) * CHUNK + static_cast<float>(x) * STEP,
                              static_cast<float>(cc.z) * CHUNK + static_cast<float>(z) * STEP};
            uncarved.heightmap.samples[static_cast<std::size_t>(z) * RES + x] =
                world::quantize_height(prepath_height(w));
        }
    }
    const world::VoxelMeshData control = world::extract_surface_nets(
        world::build_voxel_volume(uncarved, prepath_height, c.params.layout));
    const auto [c_worst, c_mid] = tread_clearance(control);
    INFO("control (no path flatten) clearance: worst ", c_worst, " m, median ", c_mid, " m");
    CHECK(c_worst < 0.0f);
    CHECK_FALSE(c_mid > GROOVE * 0.75f);
}

TEST_CASE("the stand publishes vantages, and every claim ships with its control") {
    const world::WorldGenContext& c = forest();
    const auto vs = world::forest_vantages(c.params.seed, c.params.layout, c.paths, c.finds);
    REQUIRE(!vs.empty());

    const auto find_label = [&](const std::string& l) -> const math::StandVantage* {
        for (const math::StandVantage& v : vs) {
            if (v.label == l) return &v;
        }
        return nullptr;
    };
    const auto has_prefix = [&](const std::string& p) {
        for (const math::StandVantage& v : vs) {
            if (v.label.rfind(p, 0) == 0) return true;
        }
        return false;
    };

    // THE PAIRING RULE, MECHANICALLY (Rule 27). The convention is that a
    // control's label is its claim's label plus "_control" — chosen so this
    // loop can exist. A prettier scheme ("br1_hidden" against
    // "br1_visible_control") pairs only in a reader's head, and an obligation
    // only a reader can check is one that quietly stops being checked.
    int controls = 0;
    for (const math::StandVantage& v : vs) {
        const std::string suffix = "_control";
        if (v.label.size() <= suffix.size()
            || v.label.compare(v.label.size() - suffix.size(), suffix.size(), suffix) != 0) {
            continue;
        }
        ++controls;
        const std::string claim = v.label.substr(0, v.label.size() - suffix.size());
        INFO("orphan control ", v.label, " -> expected claim ", claim);
        CHECK(find_label(claim) != nullptr);
    }
    CHECK(controls >= 2); // and the loop above is vacuous without this
    CHECK(has_prefix("br1_hidden"));
    CHECK(find_label("lf2_swale_floor") != nullptr);
    CHECK(find_label("lf2_swale_floor_control") != nullptr);
    // The money frame: the tread with its margins in shot. At least the dirt
    // class, which is what the network is mostly built of.
    CHECK(find_label("path_along_dirt") != nullptr);
    CHECK(has_prefix("goal_"));

    // Labels are filenames. A duplicate silently overwrites an archived frame,
    // and the frame that survives is whichever ran last.
    for (std::size_t i = 0; i < vs.size(); ++i) {
        for (std::size_t j = i + 1; j < vs.size(); ++j) {
            INFO("duplicate label ", vs[i].label);
            CHECK(vs[i].label != vs[j].label);
        }
        for (const char ch : vs[i].label) {
            CHECK((std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_'));
        }
        // Inside the stand, and aimed at something that is not itself.
        CHECK(vs[i].position.x > 0.0f);
        CHECK(vs[i].position.x < static_cast<float>(config::TESTBED_SIZE));
        CHECK(vs[i].position.y > 0.0f);
        CHECK(vs[i].position.y < static_cast<float>(config::TESTBED_SIZE));
        CHECK(glm::length(vs[i].subject - vs[i].position) > 1.0f);
        CHECK(vs[i].eye_offset == doctest::Approx(config::PLAYER_EYE_HEIGHT));
        // The yaw must agree with the subject, or a consumer that re-aims and
        // a consumer that does not would shoot two different frames.
        const glm::vec2 d = vs[i].subject - vs[i].position;
        CHECK(vs[i].yaw == doctest::Approx(std::atan2(d.x, -d.y)).epsilon(1e-4));
    }

    // Rule 13.1: the query is a pure function, so a second call is the same list.
    const auto again = world::forest_vantages(c.params.seed, c.params.layout, c.paths, c.finds);
    REQUIRE(again.size() == vs.size());
    for (std::size_t i = 0; i < vs.size(); ++i) {
        CHECK(again[i].label == vs[i].label);
        CHECK(again[i].position.x == doctest::Approx(vs[i].position.x));
        CHECK(again[i].position.y == doctest::Approx(vs[i].position.y));
        CHECK(again[i].yaw == doctest::Approx(vs[i].yaw));
    }
}

TEST_CASE("BR-1's vantage pair: the destination is absent from one frame and in the other") {
    const world::WorldGenContext& c = forest();
    const auto vs = world::forest_vantages(c.params.seed, c.params.layout, c.paths, c.finds);
    const math::StandVantage* hidden = nullptr;
    const math::StandVantage* control = nullptr;
    for (const math::StandVantage& v : vs) {
        if (v.label.rfind("br1_hidden", 0) != 0) continue;
        if (v.label.size() > 8 && v.label.compare(v.label.size() - 8, 8, "_control") == 0) {
            control = &v;
        } else {
            hidden = &v;
        }
    }
    REQUIRE(hidden != nullptr);
    REQUIRE(control != nullptr);
    // Same goal — otherwise the pair is two pictures, not a control.
    CHECK(glm::length(hidden->subject - control->subject) < 1.0f);

    // The claim, re-measured on the SHIPPED field rather than trusted from the
    // router's own 4 m grid.
    INFO("hidden standpoint ", hidden->position.x, ",", hidden->position.y);
    CHECK_FALSE(eye_visible(c, hidden->position, hidden->subject));
    INFO("control standpoint ", control->position.x, ",", control->position.y);
    CHECK(eye_visible(c, control->position, control->subject));

    // MATCHED RANGE is what makes it a control and not a coincidence: an
    // unmatched pair differs in two things at once and a reader could credit
    // the distance for the disappearance. Within a couple of stations.
    const float dh = glm::length(hidden->subject - hidden->position);
    const float dc = glm::length(control->subject - control->position);
    INFO("hidden range ", dh, " control range ", dc);
    // Stated as a FRACTION of the range, which is what "the goal looks about
    // the same size in both" actually means; the absolute cap is the
    // generator's own selection rule restated so a loosened rule fails here.
    CHECK(std::fabs(dh - dc) < 0.25f * dh);
    CHECK(std::fabs(dh - dc) < 25.0f);
    // And both frames are shot where a goal WOULD read — 3 m of shrine over
    // 360 lines at CAMERA_FOV_Y is ~8 lines at 120 m and ~2 at 400 m. A frame
    // whose subject is invisible either way cannot fail, so it is not evidence.
    CHECK(dh > 40.0f);
    CHECK(dh < 220.0f);
}

TEST_CASE("the LF-2 vantages stand on the landform, and the glade control does not") {
    const world::WorldGenContext& c = forest();
    const auto vs = world::forest_vantages(c.params.seed, c.params.layout, c.paths, c.finds);
    const math::StandVantage* floor_v = nullptr;
    const math::StandVantage* crest_v = nullptr;
    const math::StandVantage* glade_v = nullptr;
    for (const math::StandVantage& v : vs) {
        if (v.label == "lf2_swale_floor") floor_v = &v;
        if (v.label == "lf2_crest") crest_v = &v;
        if (v.label == "lf2_swale_floor_control") glade_v = &v;
    }
    REQUIRE(floor_v != nullptr);
    REQUIRE(crest_v != nullptr);
    REQUIRE(glade_v != nullptr);

    const float g_floor = world::forest_grive_component(c.params.seed, floor_v->position);
    const float g_crest = world::forest_grive_component(c.params.seed, crest_v->position);
    INFO("grive at floor ", g_floor, " at crest ", g_crest);
    CHECK(g_crest - g_floor >= world::LF2_HILL_RELIEF_MIN);

    /// Relief a standing camera would actually see: max minus min of the
    /// SHIPPED field over a disc the size of the frame's foreground.
    const auto local_relief = [&](glm::vec2 p) {
        float lo = 1e9f, hi = -1e9f;
        for (float dz = -40.0f; dz <= 40.0f; dz += 5.0f) {
            for (float dx = -40.0f; dx <= 40.0f; dx += 5.0f) {
                const float h = world::terrain_height(c, p + glm::vec2{dx, dz});
                lo = std::min(lo, h);
                hi = std::max(hi, h);
            }
        }
        return hi - lo;
    };
    const float r_crest = local_relief(crest_v->position);
    const float r_glade = local_relief(glade_v->position);
    INFO("relief around crest ", r_crest, " around glade control ", r_glade);
    // THE CONTROL MUST FAIL THE CLAIM. в9's calm plain is real shipped ground
    // with §2.7 micro-relief still on it ("flat, not sterile"), so this is not
    // a comparison against zero — it is a comparison against ground that has
    // texture but no landform.
    CHECK(r_crest >= world::LF2_HILL_RELIEF_MIN);
    CHECK(r_glade < world::LF2_HILL_RELIEF_MIN);
    CHECK(r_crest > r_glade * 2.0f);
    // Same bearing and same pitch as the swale frame: the two frames differ in
    // the ground underfoot and in nothing the camera did.
    CHECK(glade_v->pitch == doctest::Approx(floor_v->pitch));
    CHECK(glade_v->yaw == doctest::Approx(floor_v->yaw));
}

TEST_CASE("the path_along vantages stand ON the tread with both margins in frame") {
    const world::WorldGenContext& c = forest();
    const auto vs = world::forest_vantages(c.params.seed, c.params.layout, c.paths, c.finds);
    int checked = 0;
    for (const math::StandVantage& v : vs) {
        if (v.label.rfind("path_along_", 0) != 0) {
            continue;
        }
        ++checked;
        const world::PathSample s = c.paths.sample(v.position);
        INFO(v.label, " wear ", s.wear, " dist_to_center ", s.dist_to_center);
        CHECK(s.wear > 0.9f);            // on the bare worn centre, not beside it
        CHECK(s.edge == doctest::Approx(0.0f));
        // Aimed ALONG the tread: the subject is on the path too, and far
        // enough away that the frame has depth rather than a metre of dirt.
        const world::PathSample sub = c.paths.sample(v.subject);
        CHECK(sub.wear > 0.5f);
        CHECK(glm::length(v.subject - v.position) > 20.0f);
        // Pitched down far enough to put the margins in shot. A level frame
        // would put BR-3's rich edge in the bottom few rows of 360 and the
        // acceptance would be a picture of trees.
        CHECK(v.pitch < -0.08f);
        // The rich edge is REACHABLE from the standpoint, i.e. there is verge
        // beside this tread and not a cliff or another path.
        const glm::vec2 t = glm::normalize(v.subject - v.position);
        const glm::vec2 n{-t.y, t.x};
        const float peak_off = s.worn_half_width + math::PATH_EDGE_PEAK_M;
        CHECK(c.paths.sample(v.position + n * peak_off).edge > 0.7f);
        CHECK(c.paths.sample(v.position - n * peak_off).edge > 0.7f);
    }
    INFO("path_along vantages ", checked);
    CHECK(checked >= 2); // the SET is the evidence for the per-class scoping
}


namespace {

/// Every scatter instance of the forest stand over a block of chunks, with the
/// area it was counted over — so a density is measured, never inferred from
/// the lattice constant that produced it (that would test arithmetic).
struct FloorCensus {
    std::vector<math::ScatterInstance> all;
    float hectares = 0.0f;
    [[nodiscard]] int count(math::ScatterSpecies s) const {
        int n = 0;
        for (const auto& i : all) {
            if (i.species == s) ++n;
        }
        return n;
    }
    [[nodiscard]] float per_ha(math::ScatterSpecies s) const {
        return static_cast<float>(count(s)) / hectares;
    }
    /// Density per hectare OF THE GROUND THE ROW APPLIES TO. Measuring an
    /// open-ground density over the whole stand read 0.029/ha against a
    /// declared 0.25-0.5 and looked like a nine-fold placement bug; it was the
    /// denominator, because on the §8.1 stand the oak mass covers everything
    /// and "open ground" is the clearings alone. The eligible area is sampled
    /// through the SAME predicate the placement used.
    [[nodiscard]] float per_eligible_ha(math::ScatterSpecies s,
                                        const std::function<bool(glm::vec2)>& eligible) const {
        constexpr float STEP = 4.0f;
        const float side = static_cast<float>(config::TESTBED_SIZE);
        int hits = 0, total = 0;
        for (float z = STEP * 0.5f; z < side; z += STEP) {
            for (float x = STEP * 0.5f; x < side; x += STEP) {
                ++total;
                if (eligible({x, z})) ++hits;
            }
        }
        const float frac = static_cast<float>(hits) / static_cast<float>(total);
        return static_cast<float>(count(s)) / std::max(hectares * frac, 1e-3f);
    }
};

const FloorCensus& floor_census() {
    static const FloorCensus c = [] {
        FloorCensus out;
        const world::WorldGenContext& ctx = forest();
        for (int cz = 0; cz < 4; ++cz) {
            for (int cx = 0; cx < 4; ++cx) {
                const world::Chunk ch = world::generate_chunk(ctx, ChunkCoord{cx, cz});
                out.all.insert(out.all.end(), ch.scatter.begin(), ch.scatter.end());
            }
        }
        const float side = static_cast<float>(config::TESTBED_SIZE);
        out.hectares = side * side / 10000.0f;
        return out;
    }();
    return c;
}

} // namespace

TEST_CASE("§5.10: the forest floor is no longer bare ground, at the declared densities") {
    const FloorCensus& c = floor_census();
    // Every one of these rows carried «НЕ ПОСТРОЕНО ... в мире нет ничего»
    // in NUMBERS.md until this pass existed. The point of the test is that the
    // rows now have a CONSUMER whose output matches them, so it measures the
    // realized per-hectare density against the declared band rather than
    // asserting the lattice constant back to itself.
    //
    // The realized figure lands BELOW the band because placement is gated:
    // water margins, corridors, pads, entrance rings, clearings and the path
    // treads all reject candidates. The band is therefore the CEILING and the
    // assertion is two-sided but asymmetric — the floor is a fraction of the
    // band, and the fraction is stated so a gate that starts rejecting
    // everything is caught rather than absorbed.
    const world::WorldGenContext& ctx = forest();
    const uint64_t seed = ctx.params.seed;
    const auto& lay = ctx.params.layout;
    const auto interior = [&](glm::vec2 p) { return world::in_forest_interior(seed, lay, p); };
    const auto open = [&](glm::vec2 p) { return world::in_open_ground(seed, lay, p); };
    const auto mass = [&](glm::vec2 p) { return world::in_forest_mass(lay, p); };

    struct Row {
        const char* name;
        math::ScatterSpecies sp;
        double band_min;
        double band_max;
        const std::function<bool(glm::vec2)>* domain;
    };
    const std::function<bool(glm::vec2)> f_interior = interior;
    const std::function<bool(glm::vec2)> f_open = open;
    const std::function<bool(glm::vec2)> f_mass = mass;
    const Row rows[] = {
        {"Snag (forest)", math::ScatterSpecies::Snag, config::SNAG_DENSITY_FOREST_MIN,
         static_cast<double>(config::SNAG_DENSITY_FOREST_MAX), &f_interior},
        {"SnagPale (open)", math::ScatterSpecies::SnagPale, config::SNAG_DENSITY_OPEN_MIN,
         config::SNAG_DENSITY_OPEN_MAX, &f_open},
        {"BigBush", math::ScatterSpecies::BigBush,
         static_cast<double>(config::BIGBUSH_DENSITY_MIN),
         static_cast<double>(config::BIGBUSH_DENSITY_MAX), &f_mass},
        {"FallenLog", math::ScatterSpecies::FallenLog,
         static_cast<double>(config::LOG_DENSITY_BIG_MIN),
         static_cast<double>(config::LOG_DENSITY_BIG_MAX), &f_interior},
        {"Deadfall", math::ScatterSpecies::Deadfall,
         static_cast<double>(config::LOG_DENSITY_SMALL_MIN),
         static_cast<double>(config::LOG_DENSITY_SMALL_MAX), &f_interior},
    };
    for (const Row& r : rows) {
        const float d = c.per_eligible_ha(r.sp, *r.domain);
        INFO(r.name, " realized ", d, "/ha against band ", r.band_min, "..", r.band_max);
        CHECK(d > 0.0f);                                  // it is IN THE WORLD
        CHECK(d <= static_cast<float>(r.band_max) * 1.05f); // never above the band
        CHECK(d >= static_cast<float>(r.band_min) * 0.35f); // and not gated to a sprinkle
    }
    // The pale snag is the RARE one — the split exists because a lone bone-white
    // trunk in the open is a landmark and a grey one in the wood is weather.
    // If the two densities ever cross, the split has stopped meaning anything.
    const float dense = c.per_eligible_ha(math::ScatterSpecies::Snag, interior);
    const float rare = c.per_eligible_ha(math::ScatterSpecies::SnagPale, open);
    INFO("snag forest ", dense, "/ha of wood, pale ", rare, "/ha of open");
    CHECK(dense > rare * 2.0f);
}

TEST_CASE("§5.10: logs lie ACROSS the slope, and the flat-ground control cannot say so") {
    const FloorCensus& c = floor_census();
    const world::WorldGenContext& ctx = forest();
    /// The angle between a log's axis and the local CONTOUR, in radians,
    /// folded onto [0, pi/2] because a log has no head or tail.
    const auto off_contour = [&](const math::ScatterInstance& i) {
        const glm::vec2 p{i.position.x, i.position.z};
        constexpr float D = 3.0f;
        const float gx = world::terrain_height(ctx, {p.x + D, p.y})
                       - world::terrain_height(ctx, {p.x - D, p.y});
        const float gz = world::terrain_height(ctx, {p.x, p.y + D})
                       - world::terrain_height(ctx, {p.x, p.y - D});
        const glm::vec2 axis{std::sin(i.yaw), std::cos(i.yaw)};
        const glm::vec2 grad{gx, gz};
        // On the contour, the axis is PERPENDICULAR to the gradient.
        const float dot = std::fabs(glm::dot(glm::normalize(grad), axis));
        return std::asin(std::clamp(dot, 0.0f, 1.0f));
    };

    // Only logs on ground steep enough for "across the slope" to mean anything:
    // the exclusion is BY CAUSE, not by magnitude (Rule 36) — on ground with no
    // gradient there is no contour to lie along, and folding those samples in
    // would dilute the measurement with cases that carry no information.
    std::vector<float> steep, flat;
    for (const auto& i : c.all) {
        if (i.species != math::ScatterSpecies::FallenLog
            && i.species != math::ScatterSpecies::Deadfall) {
            continue;
        }
        const glm::vec2 p{i.position.x, i.position.z};
        (world::terrain_slope(ctx, p) > 0.05f ? steep : flat).push_back(off_contour(i));
    }
    REQUIRE(steep.size() > 200);
    const float mean_steep = median(steep);
    INFO("median angle off contour, sloping ground: ", mean_steep, " rad (", steep.size(),
         " logs); flat ground: ", flat.size(), " logs");
    // Across the slope = near 0 off the contour. A drawn yaw would sit at the
    // mean of a fold-uniform distribution, pi/4 = 0.785 — THE CONTROL, and it
    // is a real one because the jitter is +-0.22 rad by construction, so a
    // broken derivation lands on it rather than near it.
    CHECK(mean_steep < 0.25f);
    // And the control is reachable: a uniform draw would fail this by 3x.
    CHECK(mean_steep < 0.7853982f * 0.5f);
}

TEST_CASE("§5.10: nothing dead lies on the tread or in a corridor") {
    const FloorCensus& c = floor_census();
    const world::WorldGenContext& ctx = forest();
    int checked = 0;
    float worst_sink = 0.0f;
    float worst_prepath = 0.0f;
    for (const auto& i : c.all) {
        switch (i.species) {
        case math::ScatterSpecies::Snag:
        case math::ScatterSpecies::SnagPale:
        case math::ScatterSpecies::BigBush:
        case math::ScatterSpecies::FallenLog:
        case math::ScatterSpecies::Deadfall:
            break;
        default:
            continue;
        }
        ++checked;
        const glm::vec2 p{i.position.x, i.position.z};
        const world::PathSample s = ctx.paths.sample(p);
        INFO("dead wood at ", p.x, ",", p.y, " dist_from_worn_edge ", s.dist_from_worn_edge);
        CHECK(s.dist_from_worn_edge > 0.9f);
        // And it stands on the SHIPPED ground, not on the pre-erosion field —
        // the defect the scatter context inherited when the stand was wired.
        //
        // ABSOLUTE BOUND, and it used to be .epsilon(0.02). Against a ~20 m
        // terrain height doctest's tolerance is eps*(1 + |h|) = 0.42 m, so this
        // line claimed "stands on the ground" while admitting a piece of dead
        // wood floating knee-high. The quantity is a height ERROR; its
        // threshold belongs in metres, not in parts of an unrelated altitude.
        worst_sink = std::max(worst_sink, std::fabs(i.position.y - world::terrain_height(ctx, p)));
        // The control is the field this defect actually put things on.
        worst_prepath = std::max(
            worst_prepath,
            std::fabs(i.position.y
                      - (world::macro_height(ctx.params.seed, ctx.params.layout, p)
                         + ctx.erosion.sample(p))));
    }
    CHECK(checked > 500);
    INFO("worst deviation from shipped ground ", worst_sink, " m; from the pre-path field ",
         worst_prepath, " m");
    CHECK(worst_sink < 0.01f);
    // AND THE CONTROL CAN FAIL IT (Rule 30): the pre-path field is a real
    // surface this scatter was once placed against, and it is a DIFFERENT
    // surface — if it were not, this assertion would be measuring nothing.
    CHECK(worst_prepath > 0.05f);
}

TEST_CASE("the canopy occlusion envelope is the GIANT tier, not the nominal height") {
    // A giant is a DaleOak with maturity > 1 (design §5.10 — one system, not
    // two), so the ceiling a sightline must clear is the species max TIMES
    // TREE_MATURITY_GIANT_MULT_MAX. Modelling the nominal height is the
    // "half the world" defect one factor further out than the one the OAK/PINE
    // height constants were introduced to fix.
    const world::WorldGenContext& c = forest();
    const auto& lay = c.params.layout;
    float best = 0.0f;
    for (float z = 100.0f; z < 900.0f; z += 37.0f) {
        for (float x = 100.0f; x < 900.0f; x += 37.0f) {
            const glm::vec2 p{x, z};
            best = std::max(best, world::canopy_height_at(c.params.seed, lay, p,
                                                          world::terrain_height(c, p)));
        }
    }
    const auto nominal = static_cast<float>(config::OAK_HEIGHT_MAX);
    const auto giant = static_cast<float>(config::TREE_MATURITY_GIANT_MULT_MAX);
    INFO("envelope ", best, " nominal ", nominal, " x giant ", giant);
    CHECK(best == doctest::Approx(nominal * giant));
    // THE CONTROL IS THE OLD VALUE, and the threshold sits above it: if the
    // envelope ever collapses back to the nominal height this fails, which is
    // the whole point of writing it as a comparison rather than as an equality
    // to a number somebody could re-tune to match.
    CHECK(best > nominal * 1.05f);
    // Real trees reach it: the draw is not a ceiling nothing touches.
    float tallest = 0.0f;
    for (const auto& i : floor_census().all) {
        if (i.species == math::ScatterSpecies::OakTree) {
            tallest = std::max(tallest,
                               math::flora_maturity_for({i.position.x, i.position.z}));
        }
    }
    INFO("tallest drawn maturity multiplier ", tallest);
    CHECK(tallest > giant * 0.95f);
}

TEST_CASE("§5.11: the rich edge exists, at flora's declared per-100m counts") {
    const FloorCensus& c = floor_census();
    const world::WorldGenContext& ctx = forest();

    // Route length per CLASS, because per_100m is per 100 linear metres of
    // feature and the maintenance column scales by class — pooling the classes
    // would divide a hint-path's count by a cobbled street's length.
    float len_by_class[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (const world::PathRoute& r : ctx.paths.routes) {
        for (std::size_t i = 0; i + 1 < r.points.size(); ++i) {
            len_by_class[static_cast<int>(r.classes[i])] += glm::length(r.points[i + 1] - r.points[i]);
        }
    }
    const float total_len = len_by_class[0] + len_by_class[1] + len_by_class[2] + len_by_class[3];
    REQUIRE(total_len > 1000.0f);

    // Every instance within the margin band of a path, by species.
    int edge_count = 0;
    int by_species[18] = {};
    for (const auto& i : c.all) {
        const glm::vec2 p{i.position.x, i.position.z};
        const world::PathSample s = ctx.paths.sample(p);
        if (s.dist_from_worn_edge < 0.0f || s.dist_from_worn_edge > 4.0f) {
            continue;
        }
        const auto sp = static_cast<int>(i.species);
        if (sp < static_cast<int>(math::ScatterSpecies::MossPatch)) {
            continue;
        }
        ++edge_count;
        ++by_species[sp];
    }
    // FLORA'S OWN FALSIFICATION, quoted so it can fail: their PathMargin rows
    // sum to 58 instances per 100 m per SIDE. "If your first run comes out near
    // 23 instead of 58, the ramp integral is missing" — 23 is 58 x the ramp's
    // own mean (~0.4), i.e. exactly the number a peak-density wiring produces.
    // So 23 is not a soft miss, it is the SIGNATURE of the units bug, and the
    // window below is placed to exclude it rather than to bracket 58 loosely.
    const float per_100m_per_side =
        static_cast<float>(edge_count) / (total_len / 100.0f) * 0.5f;
    INFO("realized ", per_100m_per_side, " instances per 100 m per side (", edge_count,
         " over ", total_len, " m); flora's table sums to 58, the peak-density bug reads 23");
    CHECK(edge_count > 0);
    CHECK(per_100m_per_side > 30.0f); // above the units-bug signature
    CHECK(per_100m_per_side < 120.0f);

    // ALL SEVEN EDGE SPECIES ARE IN THE WORLD except the jewel, which is a
    // PLACEMENT BUDGET AND NOT A PROBABILITY (design's ruling): common_scatter
    // is false and per_100m is 0, so it may only arrive at a find or a
    // wilderness pearl. Its ABSENCE here is the rule working.
    CHECK(by_species[static_cast<int>(math::ScatterSpecies::MossPatch)] > 0);
    CHECK(by_species[static_cast<int>(math::ScatterSpecies::FlowerCarpet)] > 0);
    CHECK(by_species[static_cast<int>(math::ScatterSpecies::FlowerAccent)] > 0);
    CHECK(by_species[static_cast<int>(math::ScatterSpecies::Mushroom)] > 0);
    CHECK(by_species[static_cast<int>(math::ScatterSpecies::PebbleCluster)] > 0);
    CHECK(by_species[static_cast<int>(math::ScatterSpecies::FlowerJewel)] == 0);
    // The carpet outnumbers the accent, as the rows say (18 vs 8 per 100 m).
    CHECK(by_species[static_cast<int>(math::ScatterSpecies::FlowerCarpet)]
          > by_species[static_cast<int>(math::ScatterSpecies::FlowerAccent)]);
}

TEST_CASE("BR-3: the margin ORDERS by maintenance, hint-path > dirt > cobble > bare") {
    const FloorCensus& c = floor_census();
    const world::WorldGenContext& ctx = forest();

    // WHAT THIS TEST DOES *NOT* CLAIM, stated first because the obvious version
    // of it is worthless: BR-3's headline ratio is "the margin is richer than
    // THE GROUND", and that ratio is currently ~27000 because the off-path
    // ground carries no ground cover at all. The §5.11 ForestFloor rows exist
    // in flora's table with per_100m = 0 — the column is instances per 100
    // LINEAR metres and a forest floor is not a linear feature, so those rows
    // carry no density anyone has authored. Until design gives them an areal
    // one, "richer than the ground" compares against zero and CANNOT FAIL, so
    // it is not asserted here. Reported to flora and design instead.
    //
    // WHAT IS MEASURABLE, and is design's actual maintenance ruling: the margin
    // must ORDER by class. A rich verge is what grows where nobody sweeps, so
    // the hint-path (BR-3's specimen class) carries the full band, dirt is
    // moderate, and the cobbled street is suppressed — AND A COBBLED STREET
    // FAILING THE RATIO IS A PASS. The ordering is falsifiable in both
    // directions and every term of it is nonzero.
    int in_band[4] = {};
    float band_area[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (const auto& i : c.all) {
        if (static_cast<int>(i.species) < static_cast<int>(math::ScatterSpecies::MossPatch)) {
            continue;
        }
        const glm::vec2 p{i.position.x, i.position.z};
        const world::PathSample s = ctx.paths.sample(p);
        if (s.dist_from_worn_edge >= 0.0f && s.dist_from_worn_edge <= 4.0f) {
            ++in_band[static_cast<int>(s.path_class)];
        }
    }
    for (const world::PathRoute& r : ctx.paths.routes) {
        for (std::size_t i = 0; i + 1 < r.points.size(); ++i) {
            band_area[static_cast<int>(r.classes[i])] +=
                glm::length(r.points[i + 1] - r.points[i]) * 4.0f * 2.0f; // 4 m band, both sides
        }
    }
    const auto density = [&](world::PathClass k) {
        const int idx = static_cast<int>(k);
        return band_area[idx] > 1.0f ? static_cast<float>(in_band[idx]) / band_area[idx] : -1.0f;
    };
    const float faint = density(world::PathClass::FaintTrail);
    const float dirt = density(world::PathClass::Dirt);
    const float cobble = density(world::PathClass::Cobble);
    INFO("margin density /m2 — hint-path ", faint, ", dirt ", dirt, ", cobble ", cobble);
    REQUIRE(faint > 0.0f);
    REQUIRE(dirt > 0.0f);
    REQUIRE(cobble > 0.0f);

    // The ordering design ruled.
    CHECK(faint > dirt);
    CHECK(dirt > cobble);
    // And it is an ordering with real separation, not three numbers that
    // happen to sort: the maintenance weights differ by ~2x between the
    // neighbouring classes, so a wiring that ignored the column entirely would
    // land all three within noise of each other and fail this.
    CHECK(faint > cobble * 1.6f);

    // A KEPT VERGE IS NOT BARE GROUND. The swept class keeps whatever
    // base x clump gives it — §1.1 does not stop at the town gate, and a margin
    // suppressed to nothing would re-make «земля плоская и мёртвая» inside the
    // settlement, which is the complaint this whole stage exists to answer.
    // This is the clause that fails if the composition is written as a PRODUCT
    // instead of as a FLOOR: cobble's flower and pebble weights are exactly 0,
    // so a product zeroes the street's margin outright.
    CHECK(cobble > faint * 0.15f);
}

TEST_CASE("the two invariants that came off clump_field_edged() (flora's, now core's)") {
    // Flora deleted clump_field_edged() at core's request and named the two
    // properties it carried as UNOWNED until core's suite took them. They are
    // composition-level now: they live in scatter_path_edges' field term.
    const world::WorldGenContext& ctx = forest();
    const auto seed32 = static_cast<uint32_t>(ctx.params.seed);
    const auto compose = [&](math::ClumpClass cc, glm::vec2 p, float edge, float rich) {
        return std::max(math::clump_field(cc, p, seed32), edge * rich);
    };

    // (i) THE FLOOR NEVER SUBTRACTS: the composed value is >= the bare field
    // everywhere. Trivially violated by writing `field * edge` where BR-3 wants
    // a floor — and `field * edge` is the more natural thing to type.
    int checked = 0;
    for (float z = 100.0f; z < 700.0f; z += 13.0f) {
        for (float x = 100.0f; x < 700.0f; x += 13.0f) {
            const glm::vec2 p{x, z};
            const float bare = math::clump_field(math::ClumpClass::Flowers, p, seed32);
            for (const float e : {0.0f, 0.25f, 0.6f, 1.0f}) {
                for (const float r : {0.0f, 0.55f, 1.0f}) {
                    CHECK(compose(math::ClumpClass::Flowers, p, e, r) >= bare - 1e-6f);
                    ++checked;
                }
            }
        }
    }
    CHECK(checked > 5000);

    // (ii) A KEPT VERGE IS NOT BARE GROUND, and flora named the DISCRIMINATING
    // CASE because everywhere else the two candidate models agree: ground where
    // THE FIELD IS ZERO. On ordinary ground "richness scales the peak" and
    // "richness scales the whole density" give the same answer, so a test that
    // samples ordinary ground passes under both and proves neither.
    int zero_field_cases = 0;
    for (float z = 100.0f; z < 900.0f && zero_field_cases < 200; z += 3.0f) {
        for (float x = 100.0f; x < 900.0f && zero_field_cases < 200; x += 3.0f) {
            const glm::vec2 p{x, z};
            const float bare = math::clump_field(math::ClumpClass::Flowers, p, seed32);
            if (bare > 1e-6f) {
                continue; // not the discriminating case
            }
            ++zero_field_cases;
            // Specimen class (richness 1) still floors to the full peak here...
            CHECK(compose(math::ClumpClass::Flowers, p, 1.0f, 1.0f) == doctest::Approx(1.0f));
            // ...and the swept class reads EXACTLY the field, which is 0 here —
            // this is the case that separates the two models, and it is the one
            // the test would have missed by sampling anywhere convenient.
            CHECK(compose(math::ClumpClass::Flowers, p, 1.0f, 0.0f) == doctest::Approx(bare));
        }
    }
    INFO("zero-field discriminating cases exercised: ", zero_field_cases);
    CHECK(zero_field_cases >= 100); // Rule 30a: the case must be REACHABLE
}

TEST_CASE("§5.11: a rule row states its density in exactly one dimension") {
    // per_100m is a count per 100 LINEAR metres; per_m2 is areal. A row with
    // BOTH is a row two passes each believe they own, and the symptom is a
    // doubled density nobody can attribute to either. Never both is the hard
    // invariant; the wired habitats must additionally have ONE.
    int wired = 0, apron = 0;
    for (std::size_t k = 0; k < math::FLORA_EDGE_RULE_COUNT; ++k) {
        const math::FloraEdgeRule& r = math::FLORA_EDGE_RULES[k];
        INFO("rule ", k, " per_100m ", r.per_100m, " per_m2 ", r.per_m2);
        CHECK_FALSE((r.per_100m > 0.0f && r.per_m2 > 0.0f));
        if (r.habitat == math::EdgeHabitat::TalusApron) {
            // §5.12 IS NOT BUILT. These rows carry no density in either
            // dimension and that is a NAMED GAP, not an exemption — recorded
            // here so "the apron is done" cannot be inferred from a green run.
            ++apron;
            continue;
        }
        if (!r.common_scatter) {
            continue; // the jewel: a placement BUDGET, not a density
        }
        ++wired;
        CHECK((r.per_100m > 0.0f) != (r.per_m2 > 0.0f));
    }
    CHECK(wired >= 8);
    CHECK(apron == 3); // §5.12: three rows, no consumer, no density
}

TEST_CASE("§5.11: the forest floor carries cover, and BR-3 finally has a denominator") {
    const FloorCensus& c = floor_census();
    const world::WorldGenContext& ctx = forest();
    const auto is_cover = [](math::ScatterSpecies s) {
        return static_cast<int>(s) >= static_cast<int>(math::ScatterSpecies::MossPatch)
            && static_cast<int>(s) <= static_cast<int>(math::ScatterSpecies::PebbleCluster);
    };

    int off_path = 0, in_band = 0;
    int moss_off = 0, mush_off = 0;
    for (const auto& i : c.all) {
        if (!is_cover(i.species)) continue;
        const glm::vec2 p{i.position.x, i.position.z};
        const float e = ctx.paths.sample(p).dist_from_worn_edge;
        if (e >= 0.0f && e <= 4.0f) {
            ++in_band;
            continue;
        }
        ++off_path;
        if (i.species == math::ScatterSpecies::MossPatch) ++moss_off;
        if (i.species == math::ScatterSpecies::Mushroom) ++mush_off;
    }
    // The denominator EXISTS now — that is the whole point of this pass.
    CHECK(off_path > 0);
    CHECK(moss_off > 0);
    CHECK(mush_off > 0);

    // Eligible area for the areal rows, through the placement's own predicate.
    const uint64_t seed = ctx.params.seed;
    const auto& lay = ctx.params.layout;
    int hits = 0, total = 0;
    for (float z = 2.0f; z < static_cast<float>(config::TESTBED_SIZE); z += 4.0f) {
        for (float x = 2.0f; x < static_cast<float>(config::TESTBED_SIZE); x += 4.0f) {
            ++total;
            if (world::in_forest_interior(seed, lay, {x, z})) ++hits;
        }
    }
    const float side = static_cast<float>(config::TESTBED_SIZE);
    const float eligible_ha =
        side * side / 10000.0f * static_cast<float>(hits) / static_cast<float>(total);
    const float moss_ha = static_cast<float>(moss_off) / eligible_ha;
    const float mush_ha = static_cast<float>(mush_off) / eligible_ha;
    // FLORA AUTHORED 40/ha AND 20/ha AS *BASE* DENSITIES — before the clump
    // field, which is design's composition order and which flora states
    // outright for the mushroom row ("rings with most of the wood bare, which
    // is the intent"). So the REALISED figures are below the authored ones by
    // the field's own mean, and the same is true of the ShadeOfTrunk anchor
    // gate. The bound checked is therefore the AUTHORED CEILING plus a floor
    // that a collapsed gate would breach; the realised numbers are reported to
    // flora rather than asserted to a value nobody has ruled.
    INFO("realised moss ", moss_ha, "/ha (authored base 40), mushroom ", mush_ha,
         "/ha (authored base 20), over ", eligible_ha, " ha of forest interior");
    CHECK(moss_ha <= 40.0f * 1.05f);
    CHECK(mush_ha <= 20.0f * 1.05f);
    // MEASURED 10.08.2026: moss 6.09/ha, mushroom 1.61/ha — the authored figure
    // times the clump field's own mean (0.152 and 0.081).
    //
    // MUSHROOM IS CORRECT; MOSS IS A RULED, UNFIXED DEFECT AND THESE BOUNDS
    // ENCODE THAT. Flora ruled the asymmetry: 20/ha is a BASE (they wrote
    // "before clumping", because the field IS the intended look), while 40/ha
    // is a REALISED count ("44 stems x ~2/3 carrying a basal patch" counts
    // patches on the ground). So moss ships 6.6x low and the row wants
    // per_m2 ~= 0.0263, which is a NUMBERS change with a frame to re-shoot.
    //
    // The ceiling below is deliberately the AUTHORED figure even though moss's
    // is not a ceiling at all: when the row is corrected the realised value
    // will approach 40 and this assertion will still hold, so nothing here has
    // to be relaxed to land the fix. What must NOT happen is the gap being
    // closed by widening the clump field — flora's warning, and the reason the
    // floors are stated as absolutes rather than as a fraction of the row.
    CHECK(moss_ha > 3.0f);
    CHECK(mush_ha > 0.8f);
    // Moss outnumbers mushrooms, as the authored rows say (40 vs 20) — and by
    // more than that, since the mushroom field is the tightest in the set.
    CHECK(moss_ha > mush_ha);

    // DESIGN'S RULED READING (65655b2): same-set denominator — the numerator
    // counts edge species, so the denominator counts edge species too. Logged,
    // not gated: design demoted the ratio to a floor and made the ORDERING the
    // formal gate, on flora's finding that a world clearing the bar by 2-10x
    // is describing itself rather than testing itself.
    const float off_density = static_cast<float>(off_path) / (side * side);
    float band_area = 0.0f;
    for (const world::PathRoute& r : ctx.paths.routes) {
        for (std::size_t i = 0; i + 1 < r.points.size(); ++i) {
            band_area += glm::length(r.points[i + 1] - r.points[i]) * 4.0f * 2.0f;
        }
    }
    const float band_density = static_cast<float>(in_band) / band_area;
    INFO("BR-3 same-set: margin ", band_density, "/m2 vs wood ", off_density,
         "/m2, ratio ", band_density / std::max(off_density, 1e-9f));
    CHECK(band_density > off_density * static_cast<float>(config::RICH_EDGE_RATIO));
}

TEST_CASE("§5.11: the max() floor overshoots the authored edge density, and cobble proves it") {
    // FLORA ASKED WHETHER `max(clump, edge * rich)` (WorldgenScatter.cpp) was
    // deliberate. IT IS — design ruled max-not-product in §1.7 BR-3 (12:07:08)
    // so a swept cobble gutter keeps a MOSS RESIDUAL, and a plain product zeros
    // it (validated by mutation before the ruling: the product correctly reds
    // the suite). This case is not a challenge to that ruling.
    //
    // WHAT IT RECORDS is the consequence nobody had stated. `base` is
    // normalised by the EDGE RAMP'S OWN INTEGRAL so the realised count lands on
    // per_100m whatever the ramp's shape — an identity that holds only while
    // the field IS the ramp. Wherever clump > edge*rich the realised density
    // exceeds the authored per_100m * rich, by an amount set by the clump
    // field's distribution rather than by anything authored.
    //
    // MEASURED, seed 1, per 100 m of that class's OWN route:
    //   class          flowers realised / authored
    //   Cobble   (0)      20.44 / 0.00      <- weight is ZERO
    //   Dirt     (1)      26.72 / 9.90      2.70x
    //   FaintTrail(2)     44.87 / 18.00     2.49x
    //
    // "Residual" is what the ruling intends; ~20 per 100 m against an authored
    // zero is not a residual, it is most of a dirt verge. That gap between the
    // ruling's intent and the implementation's effect is design's to close, so
    // this case MEASURES and does not enforce.
    const world::WorldGenContext& c = forest();
    const auto CHUNKM = static_cast<float>(config::CHUNK_SIZE);

    std::map<int, double> route_len;
    for (const world::PathRoute& r : c.paths.routes) {
        for (std::size_t k = 0; k + 1 < r.points.size(); ++k) {
            route_len[static_cast<int>(r.classes[k])] +=
                glm::length(r.points[k + 1] - r.points[k]);
        }
    }
    // THE DENOMINATOR IS THE CLASS'S OWN METRES. per_100m is linear, and
    // dividing by the whole network reads low by exactly the fraction that is
    // another class — how §5.10's snag density once read nine-fold under.
    std::map<int, int> flowers;
    for (int cz = 0; cz < 4; ++cz) {
        for (int cx = 0; cx < 4; ++cx) {
            const auto s = world::build_scatter(
                c.params.seed, c.params.layout, c.hydrology, c.sites, c.erosion, c.paths,
                {static_cast<float>(cx) * CHUNKM, static_cast<float>(cz) * CHUNKM},
                {static_cast<float>(cx + 1) * CHUNKM, static_cast<float>(cz + 1) * CHUNKM});
            for (const math::ScatterInstance& i : s) {
                if (i.species != math::ScatterSpecies::FlowerCarpet) continue;
                const world::PathSample ps = c.paths.sample({i.position.x, i.position.z});
                if (ps.dist_from_worn_edge > c.paths.rich_edge_band_m + 1.0f) continue;
                ++flowers[static_cast<int>(ps.path_class)];
            }
        }
    }
    const auto per100 = [&](int cls) {
        return route_len[cls] < 50.0 ? 0.0
                                     : flowers[cls] / (route_len[cls] / 100.0);
    };
    const double cobble = per100(static_cast<int>(world::PathClass::Cobble));
    const double dirt = per100(static_cast<int>(world::PathClass::Dirt));
    const double faint = per100(static_cast<int>(world::PathClass::FaintTrail));
    INFO("flowers per 100 m of own route — cobble ", cobble, ", dirt ", dirt, ", faint ", faint);

    // DESIGN'S FORMAL ACCEPTANCE IS THE ORDERING, and it still holds.
    CHECK(faint > dirt);
    CHECK(dirt > cobble);
    // AND THE NAMED GAP: cobble's authored FlowerCarpet weight is exactly 0.0,
    // so anything it carries is the max() floor rather than an authored
    // density. Asserted as PRESENT, because the day it becomes a true residual
    // this case must be revisited rather than quietly keep passing.
    CHECK(cobble > 5.0); // it is ~20; a residual it is not
    // The separation the authored weights ask for (0 : 0.55 : 1.0) is
    // compressed by the floor into roughly 0.45 : 0.60 : 1.0. Recorded as a
    // band so a drift in either direction is visible.
    CHECK(cobble / faint > 0.30);
    CHECK(cobble / faint < 0.60);
}
