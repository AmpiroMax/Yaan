/*
Created: 09:08:2026 - 23:44:12
Last updated: 09:08:2026 - 23:44:12
Module: engine/render
File: engine/render/sources/FloraNeighbours.cpp

Responsibility:
- Neighbour analysis: derives a per-instance FloraShape (crown shyness, lean
  away from crowding, understory suppression, wind phase) from the scatter
  instance array a chunk already has.

Key items:
- analyse_neighbourhood().

Dependencies:
- Uses: ProcFlora.h, FloraSpecies.h, core math ScatterInstance, glm.
- Used by: ScatterBatcher (render), ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md §3.3.
- PURE AND DETERMINISTIC. No globals, no wall-clock, no IO.
- math::ScatterInstance IS FROZEN (Rule 26) and gains no fields: everything here
  is DERIVED from the array core already sends, which is the whole reason this
  analysis is cheap enough to exist.
- Split out of ProcFlora.cpp on 09:08:2026 for Rule 21 (800-line limit) when the
  space-colonization rewrite landed. It was always a separate responsibility.
*/
/*
UPD:
- 09:08:2026 - 23:44:12: Split out of ProcFlora.cpp unchanged (Rule 21).
*/

#include "engine/render/sources/ProcFlora.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

/// splitmix64 — local, deterministic, no shared state.
uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

} // namespace

std::vector<FloraShape> analyse_neighbourhood(std::span<const math::ScatterInstance> all,
                                              size_t count) {
    std::vector<FloraShape> out(std::min(count, all.size()));
    for (size_t i = 0; i < out.size(); ++i) {
        const math::ScatterInstance& a = all[i];
        const FloraSpecies fs = flora_species_of(a.species);
        FloraShape& sh = out[i];
        sh.maturity = a.scale;
        // Wind phase from the instance POSITION, not from the variant: twelve
        // skeletons would give twelve phases, and a stand where every twelfth
        // tree moves in lockstep reads as a repeating pattern the moment the
        // wind gusts. Position-derived, it is unique per tree and still
        // deterministic across runs and chunk borders.
        sh.wind_phase = static_cast<float>(
                            mix64(static_cast<uint64_t>(
                                      static_cast<int64_t>(std::lround(a.position.x * 8.0f)))
                                      * 0x9E3779B1ull
                                  ^ mix64(static_cast<uint64_t>(static_cast<int64_t>(
                                        std::lround(a.position.z * 8.0f)))))
                            >> 40)
            / 16777216.0f;
        if (!is_canopy_tree(fs)) continue;

        const float r_a = species_crown_radius(fs) * a.scale;
        const glm::vec2 pa{a.position.x, a.position.z};
        glm::vec2 pressure{0.0f};
        glm::vec2 worst_dir{0.0f};
        float worst_overlap = 0.0f;
        float tallest_neighbour = 0.0f;

        for (size_t j = 0; j < all.size(); ++j) {
            if (j == i) continue;
            const math::ScatterInstance& b = all[j];
            const FloraSpecies fb = flora_species_of(b.species);
            if (!is_canopy_tree(fb)) continue;
            const glm::vec2 pb{b.position.x, b.position.z};
            const glm::vec2 d = pb - pa;
            const float dist = glm::length(d);
            if (dist < 1e-3f || dist > 40.0f) continue;
            const float r_b = species_crown_radius(fb) * b.scale;
            const float overlap = r_a + r_b - dist;
            if (overlap <= 0.0f) continue;
            const glm::vec2 dir = d / dist;
            pressure += dir / (dist * dist);
            if (overlap > worst_overlap) {
                worst_overlap = overlap;
                worst_dir = dir;
            }
            const float h_b = species_nominal_height(fb) * b.scale;
            tallest_neighbour = std::max(tallest_neighbour, h_b);
        }

        const SpeciesParams& sp = species_params(fs);
        if (worst_overlap > 0.0f) {
            sh.shy_dir = worst_dir;
            sh.shyness = std::min(sp.shyness, worst_overlap / (2.0f * std::max(r_a, 0.1f)));
        }
        const float press = glm::length(pressure);
        if (press > 1e-5f) {
            // Lean AWAY from crowding — this is the visible difference between
            // a forest edge and a forest interior.
            sh.lean_dir = -pressure / press;
            sh.lean = std::min(sp.lean_response * press * 40.0f, 0.12f);
        }
        // Understory: a small tree under a much taller crowded canopy is drawn
        // out and reaching, not a scale model of a mature one.
        const float own_h = species_nominal_height(fs) * a.scale;
        sh.understory = a.scale < 0.75f && tallest_neighbour > own_h * 1.5f;
    }
    return out;
}
} // namespace dfn::render
