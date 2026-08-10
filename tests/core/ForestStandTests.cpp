/*
Created: 10:08:2026 - 02:59:28
Last updated: 10:08:2026 - 11:37:17
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
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/WorldgenMacro.h"
#include "engine/world/sources/WorldgenVantages.h"

#include <algorithm>
#include <cctype>
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
    // Scanned over the stand (16 bearings, find 0.5 m, eye 1.7 m):
    //   ring 40+80 m: p50 0.03, p90 0.19, max 0.53, >= 0.50 on 0% of ground
    //   ring 80 m only: p50 0.06, p90 0.31, max 0.69, >= 0.50 on 3%
    //   ring 40 m only: p50 0.00, p90 0.06 — at 40 m against a 100 m grive
    //                   wavelength a ring often crosses NO crest at all
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
    // most-occluded candidate in their cell, so they beat the ground's own
    // median (0.03 measured over the stand). That is the placement rule doing
    // its job on a landform that cannot yet carry it.
    CHECK(med > 0.06f);
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
    float worst_clearance = 1e9f;
    for (const world::PathRoute& r : c.paths.routes) {
        for (std::size_t k = 1; k + 1 < r.points.size(); ++k) {
            worst_clearance =
                std::min(worst_clearance, r.heights[k] - world::terrain_height(c, r.points[k]));
        }
    }
    INFO("worst tread-above-ground clearance ", worst_clearance, " m");
    CHECK(worst_clearance > 0.0f);
    CHECK(worst_clearance
          == doctest::Approx(static_cast<float>(config::PATH_GROOVE_DEPTH)).epsilon(0.25));
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

