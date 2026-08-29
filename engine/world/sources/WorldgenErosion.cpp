/*
Module: engine/world
File: engine/world/sources/WorldgenErosion.cpp

Responsibility:
- LF-8 droplet erosion (в17): the transport simulation and its bake.

Key items:
- build_erosion, ErosionGrid::sample / ::at.

Dependencies:
- Uses: WorldgenErosion.h, WorldgenNoise.h (the seeded stream), glm.
- Used by: Worldgen.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 13.1: single-threaded, fixed droplet order, fixed accumulation order.
- Rule 24: derived below from the transport equations, not vendored.
*/

#include "engine/world/sources/WorldgenErosion.h"

#include "engine/world/sources/WorldgenNoise.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {

/// SplitMix64 — the same construction WorldGenRng uses, kept local so the
/// erosion stream cannot be perturbed by an unrelated change to a shared
/// counter (Rule 13.1: a determinism dependency you cannot see is a defect
/// waiting for someone else's commit).
struct Stream {
    uint64_t state;
    float next01() {
        state += 0x9E3779B97F4A7C15ull;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        return static_cast<float>(z >> 40) * (1.0f / 16777216.0f); // 24 bits
    }
};

} // namespace

float ErosionGrid::at(int x, int z) const {
    if (x < 0 || z < 0 || x >= n || z >= n) {
        return 0.0f;
    }
    return delta[static_cast<std::size_t>(z) * static_cast<std::size_t>(n)
                 + static_cast<std::size_t>(x)];
}

float ErosionGrid::sample(glm::vec2 world) const {
    if (n <= 0 || cell <= 0.0f) {
        return 0.0f;
    }
    const float fx = (world.x - origin.x) / cell;
    const float fz = (world.y - origin.y) / cell;
    if (fx < 0.0f || fz < 0.0f || fx >= static_cast<float>(n - 1)
        || fz >= static_cast<float>(n - 1)) {
        return 0.0f;
    }
    const auto x0 = static_cast<int>(fx);
    const auto z0 = static_cast<int>(fz);
    const float tx = fx - static_cast<float>(x0);
    const float tz = fz - static_cast<float>(z0);
    const float a = at(x0, z0) + (at(x0 + 1, z0) - at(x0, z0)) * tx;
    const float b = at(x0, z0 + 1) + (at(x0 + 1, z0 + 1) - at(x0, z0 + 1)) * tx;
    return a + (b - a) * tz;
}

ErosionGrid build_erosion(uint64_t seed, glm::vec2 domain_min, glm::vec2 domain_max,
                          const ErosionParams& params,
                          const std::function<float(glm::vec2)>& base_height, bool enabled) {
    ErosionGrid grid;
    const glm::vec2 lo = domain_min - glm::vec2{params.margin, params.margin};
    const glm::vec2 hi = domain_max + glm::vec2{params.margin, params.margin};
    const float span = std::max(hi.x - lo.x, hi.y - lo.y);
    grid.origin = lo;
    grid.cell = params.cell;
    grid.n = std::max(4, static_cast<int>(span / params.cell) + 1);
    grid.delta.assign(static_cast<std::size_t>(grid.n) * static_cast<std::size_t>(grid.n), 0.0f);
    if (!enabled) {
        // §2.10 LF-8's named control: the same geometry with the pass OFF.
        return grid;
    }

    const int n = grid.n;
    const auto idx = [n](int x, int z) {
        return static_cast<std::size_t>(z) * static_cast<std::size_t>(n)
             + static_cast<std::size_t>(x);
    };

    // Bake the base field onto the grid ONCE. The droplet loop then reads and
    // writes the same array, which is what makes gullies deepen where earlier
    // droplets already cut — erosion is a positive feedback and a pass that
    // reads an immutable base produces smooth hollows and no channels at all.
    std::vector<float> h(grid.delta.size(), 0.0f);
    std::vector<float> h0(grid.delta.size(), 0.0f);
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            const glm::vec2 p{lo.x + static_cast<float>(x) * params.cell,
                              lo.y + static_cast<float>(z) * params.cell};
            const float v = base_height(p);
            h[idx(x, z)] = v;
            h0[idx(x, z)] = v;
        }
    }

    // Bilinear height and its analytic gradient on the working grid. The
    // gradient is the bilinear patch's exact derivative (not a central
    // difference of samples): a droplet steered by a gradient that disagrees
    // with the surface it walks on oscillates across cell boundaries and
    // leaves a checkerboard instead of a channel.
    struct HG { float h; float gx; float gz; };
    const auto height_grad = [&](float fx, float fz) -> HG {
        const int x0 = std::clamp(static_cast<int>(fx), 0, n - 2);
        const int z0 = std::clamp(static_cast<int>(fz), 0, n - 2);
        const float u = std::clamp(fx - static_cast<float>(x0), 0.0f, 1.0f);
        const float v = std::clamp(fz - static_cast<float>(z0), 0.0f, 1.0f);
        const float h00 = h[idx(x0, z0)];
        const float h10 = h[idx(x0 + 1, z0)];
        const float h01 = h[idx(x0, z0 + 1)];
        const float h11 = h[idx(x0 + 1, z0 + 1)];
        HG r;
        r.h = h00 * (1 - u) * (1 - v) + h10 * u * (1 - v) + h01 * (1 - u) * v + h11 * u * v;
        // d/dx and d/dz of that bilinear form, in METERS PER CELL (the droplet
        // steps in cells, so keeping the gradient in cell units keeps the
        // integration dimensionally consistent).
        r.gx = (h10 - h00) * (1 - v) + (h11 - h01) * v;
        r.gz = (h01 - h00) * (1 - u) + (h11 - h10) * u;
        return r;
    };

    // The erosion brush: a normalized cone over brush_radius cells. Cutting at
    // a point instead makes single-cell spikes — the pass has to remove a
    // CHANNEL's worth of material, and the channel is wider than a sample.
    const int br = std::max(1, static_cast<int>(params.brush_radius));
    struct BrushCell { int dx; int dz; float w; };
    std::vector<BrushCell> brush;
    {
        float total = 0.0f;
        for (int dz = -br; dz <= br; ++dz) {
            for (int dx = -br; dx <= br; ++dx) {
                const float d = std::sqrt(static_cast<float>(dx * dx + dz * dz));
                if (d > params.brush_radius) {
                    continue;
                }
                const float w = 1.0f - d / params.brush_radius;
                brush.push_back({dx, dz, w});
                total += w;
            }
        }
        for (BrushCell& b : brush) {
            b.w /= total;
        }
    }

    const auto deposit_at = [&](float fx, float fz, float amount) {
        // Bilinear deposition — the inverse of the bilinear read, so material
        // lands where the droplet actually is. Depositing into the nearest
        // cell biases fans onto the lattice.
        const int x0 = std::clamp(static_cast<int>(fx), 0, n - 2);
        const int z0 = std::clamp(static_cast<int>(fz), 0, n - 2);
        const float u = std::clamp(fx - static_cast<float>(x0), 0.0f, 1.0f);
        const float v = std::clamp(fz - static_cast<float>(z0), 0.0f, 1.0f);
        h[idx(x0, z0)] += amount * (1 - u) * (1 - v);
        h[idx(x0 + 1, z0)] += amount * u * (1 - v);
        h[idx(x0, z0 + 1)] += amount * (1 - u) * v;
        h[idx(x0 + 1, z0 + 1)] += amount * u * v;
    };

    // Droplet COUNT from a density: a stand twice as wide must not get half the
    // gullies per hectare (Rule 32 — the parameter that means something is the
    // density, so that is the one declared).
    const double area_km2 = static_cast<double>(span) * static_cast<double>(span) / 1.0e6;
    const auto droplets =
        static_cast<int64_t>(area_km2 * static_cast<double>(params.droplets_per_km2));

    Stream rng{seed ^ 0xE1051011ull}; // "EROSION" stream, decorrelated from the lattice
    for (int64_t d = 0; d < droplets; ++d) {
        float px = rng.next01() * static_cast<float>(n - 2);
        float pz = rng.next01() * static_cast<float>(n - 2);
        float dx = 0.0f;
        float dz = 0.0f;
        float speed = params.initial_speed;
        float water = params.initial_water;
        float sediment = 0.0f;

        for (int step = 0; step < params.max_steps; ++step) {
            const HG cur = height_grad(px, pz);
            // Direction: inertia carries the old heading, the rest is
            // downhill. Pure gradient descent walks into the first pit and
            // stops; pure inertia ignores the terrain. The mixture is what
            // makes a droplet cut a RUN rather than a dimple.
            dx = dx * params.inertia - cur.gx * (1.0f - params.inertia);
            dz = dz * params.inertia - cur.gz * (1.0f - params.inertia);
            const float len = std::sqrt(dx * dx + dz * dz);
            if (len < 1e-6f) {
                break; // flat and stalled: the droplet has nowhere to go
            }
            dx /= len;
            dz /= len;
            const float nx = px + dx;
            const float nz = pz + dz;
            if (nx < 1.0f || nz < 1.0f || nx > static_cast<float>(n - 2)
                || nz > static_cast<float>(n - 2)) {
                break; // left the padded domain
            }
            const float dh = height_grad(nx, nz).h - cur.h;

            // Sediment capacity: proportional to the drop taken, the speed and
            // the water carried. min_slope is the load-bearing term — without
            // it a droplet reaching flat ground has capacity 0 and dumps its
            // entire load into one cell, which builds cones, not fans.
            const float cap = std::max(-dh, params.min_slope) * speed * water * params.capacity;

            if (sediment > cap || dh > 0.0f) {
                // Uphill or over capacity: DEPOSIT. Going uphill it may only
                // fill the step it just climbed — more than that would let a
                // droplet build terrain above its own source.
                const float drop = (dh > 0.0f) ? std::min(dh, sediment)
                                               : (sediment - cap) * params.deposit_rate;
                deposit_at(px, pz, drop);
                sediment -= drop;
            } else {
                // Under capacity: ERODE, but never more than the drop itself —
                // a droplet that digs deeper than the step it is descending
                // inverts the local slope and the channel eats itself.
                const float take = std::min((cap - sediment) * params.erode_rate, -dh);
                float removed = 0.0f;
                const int cx = static_cast<int>(px);
                const int cz = static_cast<int>(pz);
                for (const BrushCell& b : brush) {
                    const int bx = cx + b.dx;
                    const int bz = cz + b.dz;
                    if (bx < 0 || bz < 0 || bx >= n || bz >= n) {
                        continue;
                    }
                    const float amount = take * b.w;
                    h[idx(bx, bz)] -= amount;
                    removed += amount;
                }
                sediment += removed;
            }

            // Speed from the drop (kinetic, not the true integral — the
            // droplet is a transport heuristic, not a physics body), water
            // evaporates over the run so droplets have finite reach.
            speed = std::sqrt(std::max(0.0f, speed * speed + (-dh) * params.gravity));
            water *= (1.0f - params.evaporation);
            if (water < 0.01f) {
                break;
            }
            px = nx;
            pz = nz;
        }
    }

    for (std::size_t i = 0; i < grid.delta.size(); ++i) {
        grid.delta[i] = std::clamp(h[i] - h0[i], -params.max_cut, params.max_cut);
    }
    return grid;
}

} // namespace dfn::world
