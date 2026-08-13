/*
Created: 13:08:2026 - 16:05:00
Last updated: 13:08:2026 - 16:05:00
Module: engine/gameplay
File: engine/gameplay/sources/FloraCollision.cpp

Responsibility:
- Measures the collider of one plant out of the mesh flora draws for it, and
  memoizes the result per (species, variant, bucketed maturity).

Key items:
- kind_of(): which physical answer a species gets.
- cut_height(): where the bole stops and the crown begins.
- measure(): the one place a collider is derived, by clipping drawn triangles.

Dependencies:
- Uses: FloraCollision.h, render ProcFlora (build_flora_mesh,
  analyse_neighbourhood, flora_species_of/flora_owns, species_crown_base),
  core math ScatterSpecies, generated constants (PLAYER_STEP_HEIGHT).
- Used by: PropCollision, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- No flora table, formula or threshold may be restated here. Maturity comes
  from flora's own analyse_neighbourhood; every dimension comes from vertices.
*/
/*
UPD:
- 13:08:2026 - 16:05:00: Created.
*/

#include "engine/gameplay/sources/FloraCollision.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/ProcFlora.h"

namespace dfn::gameplay {

namespace {

constexpr float STEP_HEIGHT = static_cast<float>(config::PLAYER_STEP_HEIGHT);

// WHICH SPECIES ARE WHAT, and the switch is exhaustive with no `default` on
// purpose: core's ScatterSpecies grew from 5 to 18 once already, and the zone
// that answered with a `default` drew nothing for every new ordinal, silently,
// with its suite green (flora's own note on flora_owns()). Here the same
// mistake would make a new class walk-through, which is the defect this file
// exists to remove — so a new ordinal must fail to COMPILE.
[[nodiscard]] FloraSolidKind kind_of(math::ScatterSpecies s) {
    switch (s) {
    // Standing wood: the bole stops you.
    case math::ScatterSpecies::OakTree:
    case math::ScatterSpecies::PineTree:
    case math::ScatterSpecies::BirchTree:
    case math::ScatterSpecies::Snag:
    case math::ScatterSpecies::SnagPale:
    case math::ScatterSpecies::StuntedPine:
    case math::ScatterSpecies::GreatOak:
        return FloraSolidKind::Solid;
    // Downed wood: solid IF it stands above the step (decided in measure()).
    case math::ScatterSpecies::FallenLog:
        return FloraSolidKind::Solid;
    // Brush: no body, a drag disc. A shrub you cannot walk through is a fence
    // and a shrub you cannot feel is a painting; the ground between them is
    // "it costs you speed", which is the user's own proposal.
    case math::ScatterSpecies::Bush:
    case math::ScatterSpecies::BigBush:
    case math::ScatterSpecies::Deadfall:
        return FloraSolidKind::Drag;
    // Boulders already have collision from their drawn triangles, through
    // PropCollision's scatter path. Answering Solid here would give every
    // stone in the world a SECOND body.
    case math::ScatterSpecies::Stone:
        return FloraSolidKind::None;
    // Ground cover. Nothing here reaches a shin, and a mushroom that slowed
    // you down would be a bug report.
    case math::ScatterSpecies::MossPatch:
    case math::ScatterSpecies::FlowerCarpet:
    case math::ScatterSpecies::FlowerAccent:
    case math::ScatterSpecies::FlowerJewel:
    case math::ScatterSpecies::FlowerUmbel:
    case math::ScatterSpecies::Mushroom:
    case math::ScatterSpecies::PebbleCluster:
        return FloraSolidKind::None;
    }
    return FloraSolidKind::None;
}

// Where the solid part of a STANDING plant stops. Two rules meet here and the
// lower one wins: the reach physics needs (TRUNK_COLLISION_HEIGHT) and the
// height at which this instance's own crown starts. Below the crown base there
// is nothing but bole, so the cut is what makes "keep the drawn triangles"
// safe — and `species_crown_base` is flora's accessor, not a fraction restated
// here.
[[nodiscard]] float cut_height(render::FloraSpecies fs, float maturity) {
    const float crown_base = render::species_crown_base(fs) * maturity;
    // A crown base of zero (a shrub-shaped species) would cut everything away;
    // a plant that low is not on the Solid path anyway, but the clamp keeps the
    // function total rather than trusting the caller.
    if (crown_base <= 0.05f) {
        return TRUNK_COLLISION_HEIGHT;
    }
    return std::min(TRUNK_COLLISION_HEIGHT, crown_base);
}

// Keeps every triangle with a vertex below `cut` and reports the extent of what
// was kept. Vertices are in the plant's local space, stem base at the origin.
void clip_below(const render::MeshData& src, float cut, render::MeshData& dst, float& top,
                float& max_radius) {
    std::vector<uint32_t> remap(src.vertices.size(), 0xFFFFFFFFu);
    for (size_t i = 0; i + 2 < src.indices.size(); i += 3) {
        const uint32_t idx[3] = {src.indices[i], src.indices[i + 1], src.indices[i + 2]};
        if (idx[0] >= src.vertices.size() || idx[1] >= src.vertices.size() ||
            idx[2] >= src.vertices.size()) {
            continue;
        }
        const float lowest = std::min({src.vertices[idx[0]].position.y,
                                       src.vertices[idx[1]].position.y,
                                       src.vertices[idx[2]].position.y});
        if (lowest > cut) {
            continue;
        }
        for (const uint32_t v : idx) {
            if (remap[v] == 0xFFFFFFFFu) {
                remap[v] = static_cast<uint32_t>(dst.vertices.size());
                dst.vertices.push_back(src.vertices[v]);
                const glm::vec3& p = src.vertices[v].position;
                top = std::max(top, p.y);
                max_radius = std::max(max_radius, std::sqrt(p.x * p.x + p.z * p.z));
            }
            dst.indices.push_back(remap[v]);
        }
    }
}

// The horizontal reach and the height of everything drawn — the two numbers a
// drag disc is. Measured over the WOOD stream: the cards are leaves, and a leaf
// card is a two-sided quad whose corners swing wide of the mass it represents.
void measure_extent(const render::MeshData& src, float& radius, float& top) {
    for (const platform::Vertex& v : src.vertices) {
        radius = std::max(radius, std::sqrt(v.position.x * v.position.x +
                                            v.position.z * v.position.z));
        top = std::max(top, v.position.y);
    }
}

[[nodiscard]] uint64_t memo_key(math::ScatterSpecies s, uint32_t variant, float maturity) {
    const auto bucket = static_cast<uint32_t>(
        std::lround(std::floor(maturity / FLORA_COLLISION_MATURITY_STEP)));
    return (static_cast<uint64_t>(s) << 40) | (static_cast<uint64_t>(variant) << 20) |
           bucket;
}

[[nodiscard]] float bucketed(float maturity) {
    return std::floor(maturity / FLORA_COLLISION_MATURITY_STEP) *
           FLORA_COLLISION_MATURITY_STEP;
}

} // namespace

FloraSolidKind flora_solid_kind(math::ScatterSpecies species) { return kind_of(species); }

float flora_collision_maturity(math::ScatterSpecies species, glm::vec2 world_xz,
                               float instance_scale) {
    // FLORA'S OWN DRAW, ASKED FOR RATHER THAN REPRODUCED. The tier draw lives
    // inside analyse_neighbourhood (canopy trees take math::flora_maturity_for,
    // everything else keeps ScatterInstance::scale, and the great oak is exempt
    // because it already IS the giant tier). Restating those three rules here
    // would be a fourth copy of a routing that has already been wrong once.
    //
    // A ONE-ELEMENT SPAN IS SOUND, and that is the property worth stating:
    // maturity is position-keyed, so it does not depend on which neighbours are
    // in the array. Only lean/shyness/understory do, and none of the three
    // moves the bole (measured: zero axis shift below 4 m). Calling the real
    // function on the whole chunk instead would be O(n^2) over ~16 000 scatter
    // instances = 178 ms per chunk, which is a stutter, not a budget.
    math::ScatterInstance inst;
    inst.position = glm::vec3{world_xz.x, 0.0f, world_xz.y};
    inst.scale = instance_scale;
    inst.species = species;
    const std::vector<render::FloraShape> shape =
        render::analyse_neighbourhood(std::span<const math::ScatterInstance>{&inst, 1}, 1);
    return shape.empty() ? instance_scale : shape.front().maturity;
}

const FloraSolid& flora_solid(FloraCollisionCache& cache, math::ScatterSpecies species,
                              uint32_t variant, float maturity) {
    const uint64_t key = memo_key(species, variant, maturity);
    if (const auto it = cache.solids.find(key); it != cache.solids.end()) {
        ++cache.hits;
        return it->second;
    }
    ++cache.misses;

    FloraSolid solid;
    solid.kind = flora_solid_kind(species);
    // A class flora does not mesh has nothing to measure. Asking flora rather
    // than assuming keeps this in step with the routing predicate it owns.
    if (solid.kind != FloraSolidKind::None && render::flora_owns(species)) {
        const render::FloraSpecies fs = render::flora_species_of(species);
        const float mat = std::max(bucketed(maturity), FLORA_COLLISION_MATURITY_STEP);
        render::FloraShape shape;
        shape.maturity = mat;
        const render::FloraMesh drawn =
            render::build_flora_mesh(fs, variant, shape, render::FloraLod::Full);

        if (solid.kind == FloraSolidKind::Solid) {
            // A DOWNED LOG IS SOLID ALL THE WAY UP: it has no crown to cut off,
            // and cutting it at 4 m would do nothing but cost a comparison.
            const bool downed = species == math::ScatterSpecies::FallenLog;
            const float cut = downed ? 1.0e9f : cut_height(fs, mat);
            clip_below(drawn.wood, cut, solid.mesh, solid.top, solid.max_radius);
            // THE STEP IS THE WATERSHED WE ALREADY HAVE. `PLAYER_STEP_HEIGHT`
            // is what the character controller climbs for free, so a log whose
            // top is under it is ALREADY not an obstacle — giving it a body
            // would add triangles that change nothing except the budget. Above
            // it, the log is a thing you go round or over, which is what design
            // asked a fallen tree to be.
            if (downed && solid.top <= STEP_HEIGHT) {
                solid.kind = FloraSolidKind::None;
                solid.mesh = render::MeshData{};
            }
            if (solid.mesh.indices.size() < 3) {
                solid.kind = FloraSolidKind::None;
            }
        } else {
            measure_extent(drawn.wood, solid.drag_radius, solid.drag_top);
            // Brush that does not reach the knee is litter. Litter is drawn,
            // heard and stepped on; it does not slow a walker down, and a drag
            // disc under the step height would be a mystery deceleration.
            if (solid.drag_top <= STEP_HEIGHT || solid.drag_radius <= 0.0f) {
                solid.kind = FloraSolidKind::None;
            }
        }
    } else {
        solid.kind = FloraSolidKind::None;
    }

    return cache.solids.emplace(key, std::move(solid)).first->second;
}

} // namespace dfn::gameplay
