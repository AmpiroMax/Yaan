/*
Created: 10:08:2026 - 02:59:28
Last updated: 10:08:2026 - 10:29:50
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
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/WorldgenMacro.h"

#include <algorithm>
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
