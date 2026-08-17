/*
Created: 17:08:2026 - 19:05:00
Last updated: 17:08:2026 - 20:09:15
Module: engine/editor
File: engine/editor/sources/EditorBrush.cpp

Responsibility:
- The terrain brush and the planting hand declared in EditorBrush.h.

Dependencies:
- Uses: EditorBrush.h, engine/world.
- Used by: the brush panel, engine/app (EditorPlant), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NOTHING IN THIS FILE DECIDES WHETHER A PLACEMENT IS ALLOWED. That question
  belongs to world::check_scene and its answer is worded in engine/app's
  EditorPlant.cpp. If you catch yourself writing `if (on_a_path) return false`
  here, the rule belongs in the judge, where the tools and the agents see it.
- THE BRUSH WORKS IN DELTAS AGAINST THE GENERATOR'S GROUND, never in absolute
  heights. Storing "the ground here is 31.4 m" would freeze the edit against
  the world as it was on the day it was painted, and every later change to the
  generator would leave the composer's hills floating over new terrain.
*/
/*
UPD:
- 17:08:2026 - 19:05:00: Создан вместе с EditorBrush.h.
- 17:08:2026 - 20:06:53: Переезд в engine/editor вместе с заголовком; судейская половина
  (plant_dab, edit_placement и разбор вердикта по разности) уехала в
  engine/app/sources/EditorPlant.cpp — ей нужен BuildTool, а он выше по слоям.
- 17:08:2026 - 20:09:15: stroke_step — решение на нажатии, а не покадрово (см. заголовок).
*/

#include "engine/editor/sources/EditorBrush.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>

namespace dfn::app {
namespace {

/// The deterministic stream a planting dab draws from. Same mixer worldgen
/// uses (splitmix64), for the same reason: a dab must be reproducible, and a
/// test that says "these eleven candidates" is worth more than one that says
/// "about ten, somewhere".
class DabRng {
public:
    explicit DabRng(uint64_t seed) : state_(seed ^ 0x5EED10C0FFEEB00Dull) {}

    [[nodiscard]] uint64_t next_u64() {
        state_ += 0x9E3779B97F4A7C15ull;
        uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    /// [0, 1)
    [[nodiscard]] float next_unit() {
        return static_cast<float>(next_u64() >> 40) / 16777216.0f;
    }

    [[nodiscard]] float next_range(float lo, float hi) {
        return lo + (hi - lo) * next_unit();
    }

private:
    uint64_t state_;
};

/// The ground including the composer's own edits: base + layer. Used wherever
/// the question is "how high is it HERE, now" rather than "what did the
/// generator make" — smoothing reads it, and so does anything being sat down.
[[nodiscard]] float finished_at(const world::ReliefLayer& layer, const BrushGround& g,
                                glm::vec2 p) {
    const float base = g.base_at != nullptr ? g.base_at(g.ctx, p) : 0.0f;
    return layer.empty() ? base : base + layer.height_delta_at(p);
}

/// The lattice window a dab of this radius covers, as inclusive index bounds.
struct SampleWindow {
    int32_t x0 = 0;
    int32_t x1 = 0;
    int32_t z0 = 0;
    int32_t z1 = 0;
};

[[nodiscard]] SampleWindow window_of(glm::vec2 center, float radius_m) {
    SampleWindow w;
    w.x0 = world::relief_index_floor(center.x - radius_m);
    w.x1 = world::relief_index_floor(center.x + radius_m) + 1;
    w.z0 = world::relief_index_floor(center.y - radius_m);
    w.z1 = world::relief_index_floor(center.y + radius_m) + 1;
    return w;
}

} // namespace

// =============================== THE GROUND =================================

float brush_weight(float dist_m, float radius_m, float hardness) {
    const float r = std::max(radius_m, BRUSH_MIN_RADIUS_M);
    if (dist_m >= r) {
        return 0.0f; // outside the rim, and EXACTLY zero: a brush that leaked a
                     // millionth past its own edge would slowly tilt the world
    }
    if (dist_m <= 0.0f) {
        return 1.0f;
    }
    const float h = std::clamp(hardness, 0.0f, BRUSH_HARDNESS_MAX);
    const float flat = r * h; // full strength out to here
    if (dist_m <= flat) {
        return 1.0f;
    }
    // SMOOTHSTEP AND NOT A LINE. The stroke has to meet untouched ground with a
    // matching slope: a linear fade is C1-discontinuous at the rim, and lit
    // terrain draws that discontinuity as a visible ring around every dab —
    // which reads as a defect in the terrain rather than in the falloff.
    const float t = (dist_m - flat) / (r - flat);
    const float s = t * t * (3.0f - 2.0f * t);
    return 1.0f - s;
}

BrushDabReport apply_brush(world::ReliefLayer& layer, const TerrainBrush& brush,
                           glm::vec2 center, float dt_s, const BrushGround& ground) {
    BrushDabReport report;
    if (brush.mode == BrushMode::Flatten) {
        // NOT A GAP — flatten's output is a pad, not samples. Reporting zero
        // here is the honest answer to "what did you write into the layer".
        return report;
    }
    const float radius = std::max(brush.radius_m, BRUSH_MIN_RADIUS_M);
    if (dt_s <= 0.0f && brush.mode != BrushMode::Paint) {
        return report; // no time passed, so nothing moved
    }
    const SampleWindow win = window_of(center, radius);

    for (int32_t iz = win.z0; iz <= win.z1; ++iz) {
        for (int32_t ix = win.x0; ix <= win.x1; ++ix) {
            const glm::vec2 at{world::relief_world_of(ix), world::relief_world_of(iz)};
            const glm::vec2 d = at - center;
            const float dist = std::sqrt(d.x * d.x + d.y * d.y);
            const float w = brush_weight(dist, radius, brush.hardness);
            if (w <= 0.0f) {
                continue;
            }

            if (brush.mode == BrushMode::Paint) {
                // PAINT IS A DECISION, NOT AN ACCUMULATION. A class does not
                // half-apply: what the falloff decides here is WHETHER this
                // sample is inside the stroke, not how much of the colour it
                // got. Half a rock is not a thing the ground can be.
                layer.set_surface(ix, iz, brush.paint);
                ++report.samples_touched;
                report.any = true;
                if (report.samples_touched == 1) {
                    report.min_xz = report.max_xz = at;
                } else {
                    report.min_xz = glm::min(report.min_xz, at);
                    report.max_xz = glm::max(report.max_xz, at);
                }
                continue;
            }

            const float before = layer.delta_at(ix, iz);
            float after = before;
            switch (brush.mode) {
            case BrushMode::Raise:
                after = before + brush.strength_m_s * dt_s * w;
                break;
            case BrushMode::Lower:
                after = before - brush.strength_m_s * dt_s * w;
                break;
            case BrushMode::Smooth: {
                // TOWARDS THE AVERAGE OF THE FOUR NEIGHBOURS, and the average is
                // of the FINISHED ground rather than of the deltas. Averaging
                // deltas alone would smooth the composer's edit while leaving
                // the generator's own ridge under it untouched — the ground
                // would refuse to flatten and he would conclude the tool does
                // nothing.
                const float step = world::RELIEF_STEP_M;
                const float here = finished_at(layer, ground, at);
                const float avg = 0.25f
                                * (finished_at(layer, ground, {at.x - step, at.y})
                                   + finished_at(layer, ground, {at.x + step, at.y})
                                   + finished_at(layer, ground, {at.x, at.y - step})
                                   + finished_at(layer, ground, {at.x, at.y + step}));
                // Rate, not a jump: `strength_m_s * dt` is a fraction of the way
                // there per second, so holding the brush converges instead of
                // ringing between over-corrections.
                const float k = std::clamp(brush.strength_m_s * dt_s * w, 0.0f, 1.0f);
                after = before + (avg - here) * k;
                break;
            }
            case BrushMode::Flatten:
            case BrushMode::Paint:
                break; // handled above; listed so a new mode cannot be forgotten
            }

            const float moved = after - before;
            if (moved == 0.0f) {
                continue;
            }
            layer.set_delta(ix, iz, after);
            ++report.samples_touched;
            report.max_abs_delta_m = std::max(report.max_abs_delta_m, std::fabs(moved));
            report.sum_abs_delta_m += std::fabs(moved);
            if (!report.any) {
                report.any = true;
                report.min_xz = report.max_xz = at;
            } else {
                report.min_xz = glm::min(report.min_xz, at);
                report.max_xz = glm::max(report.max_xz, at);
            }
        }
    }

    if (report.any) {
        // ONE LATTICE STEP OF PADDING, for the same reason ReliefLayer::bounds_xz
        // pads: a delta at a sample reaches half a cell either way through the
        // bilinear filter, so a chunk that only overlaps the padding still has
        // ground that moved and still has to be rebuilt.
        const glm::vec2 pad{world::RELIEF_STEP_M, world::RELIEF_STEP_M};
        report.min_xz -= pad;
        report.max_xz += pad;
    }
    return report;
}

world::ScenePad flatten_pad(const TerrainBrush& brush, glm::vec2 center) {
    world::ScenePad pad;
    pad.center = center;
    pad.radius = std::max(brush.radius_m, BRUSH_MIN_RADIUS_M);
    pad.height = brush.flatten_height_m;
    // THE BLEND IS DERIVED FROM THE BRUSH'S OWN FALLOFF, not chosen. The soft
    // part of this brush runs from hardness*R to R, so that is exactly how far
    // the pad has to fade for the flattened patch to look like the one the
    // builder saw under his cursor while he was aiming it.
    const float h = std::clamp(brush.hardness, 0.0f, BRUSH_HARDNESS_MAX);
    pad.blend = std::max(pad.radius * (1.0f - h), world::RELIEF_STEP_M);
    return pad;
}

bool stroke_step(BrushStroke& stroke, bool pointer_down, bool ui_wants_mouse) {
    stroke.just_ended = false;

    if (!pointer_down) {
        // THE BUTTON IS UP: whatever was happening is over. A stroke that was
        // blocked ends silently — it never bit anything, so there is nothing to
        // rebuild and nothing to report.
        if (stroke.active) {
            stroke.active = false;
            stroke.just_ended = true;
        }
        stroke.blocked = false;
        return false;
    }

    if (!stroke.active && !stroke.blocked) {
        // THE DECISION IS MADE ONCE, AT THE PRESS, and this is the whole design.
        // Testing the flag every frame instead would mean a drag that started on
        // the size slider begins digging the moment the pointer crosses off the
        // panel — the builder is still holding the same button, still adjusting
        // the same slider, and a hole appears. So a stroke that began over the
        // interface is BLOCKED for its entire life.
        if (ui_wants_mouse) {
            stroke.blocked = true;
            return false;
        }
        stroke.active = true;
    }

    // ...and by the same argument in reverse, a stroke that began on the GROUND
    // keeps painting even if the pointer wanders over a panel mid-stroke.
    // Interrupting it there would tear a sculpted ridge in half at the panel's
    // edge, which is a defect the builder would blame on the brush.
    return stroke.active;
}

// ============================ THE VEGETATION ================================

std::vector<PlantCandidate> plant_candidates(const PlantBrush& brush, glm::vec2 center,
                                             uint64_t seed, const BrushGround& ground) {
    std::vector<PlantCandidate> out;
    if (brush.species.empty() || brush.count <= 0) {
        return out;
    }
    const float radius = std::max(brush.radius_m, 0.0f);
    DabRng rng(seed);
    // Rejection against the ones already placed in THIS dab. Bounded tries per
    // instance rather than "until it fits": a dense brush on a small radius has
    // no room for its own count, and a loop that insisted would hang the editor
    // rather than plant nine trees where ten were asked for.
    constexpr int TRIES_PER_INSTANCE = 12;
    for (int i = 0; i < brush.count; ++i) {
        for (int t = 0; t < TRIES_PER_INSTANCE; ++t) {
            // Uniform over the DISC, not over the square: sqrt on the radius is
            // what keeps a dab from being visibly denser at its own centre.
            const float ang = rng.next_range(0.0f, 6.2831853f);
            const float rr = radius * std::sqrt(rng.next_unit());
            const glm::vec2 p{center.x + std::cos(ang) * rr, center.y + std::sin(ang) * rr};
            bool too_close = false;
            for (const PlantCandidate& other : out) {
                const float dx = other.position.x - p.x;
                const float dz = other.position.z - p.y;
                if (dx * dx + dz * dz < brush.min_spacing_m * brush.min_spacing_m) {
                    too_close = true;
                    break;
                }
            }
            if (too_close && t + 1 < TRIES_PER_INSTANCE) {
                continue;
            }
            if (too_close) {
                break; // no room; this instance simply does not happen
            }
            PlantCandidate c;
            const uint64_t pick = rng.next_u64() % brush.species.size();
            c.object = brush.species[static_cast<std::size_t>(pick)];
            const float y = ground.base_at != nullptr ? ground.base_at(ground.ctx, p) : 0.0f;
            c.position = {p.x, y, p.y};
            c.yaw = brush.random_yaw ? rng.next_range(0.0f, 6.2831853f) : 0.0f;
            c.scale = rng.next_range(brush.scale_min, brush.scale_max);
            out.push_back(c);
            break;
        }
    }
    return out;
}

std::size_t pick_placement(const world::SceneDoc& doc, glm::vec2 aim_xz,
                           bool (*radius_of)(void* ctx, const std::string& name,
                                             float& radius_m),
                           void* ctx) {
    const std::size_t none = doc.placements.size();
    if (radius_of == nullptr) {
        return none;
    }
    std::size_t best = none;
    float best_dist = 0.0f;
    for (std::size_t i = 0; i < doc.placements.size(); ++i) {
        const world::Placement& p = doc.placements[i];
        float radius = 0.0f;
        if (!radius_of(ctx, p.object, radius)) {
            // An object the registry does not carry is SKIPPED, not guessed at.
            // Inventing a reach here would let the crosshair delete a thing
            // whose size nobody knows, which is the one deletion nobody can
            // undo by clicking again in the same place.
            continue;
        }
        const float dx = p.position.x - aim_xz.x;
        const float dz = p.position.z - aim_xz.y;
        const float dist = std::sqrt(dx * dx + dz * dz);
        // A FLOOR UNDER THE REACH: a fern's radius is centimetres, and a
        // crosshair that has to land inside it is a crosshair that never picks
        // one up. The floor is small enough that it cannot make a fern outrank
        // a tree the aim is actually inside of.
        if (dist <= std::max(radius, 0.35f) && (best == none || dist < best_dist)) {
            best = i;
            best_dist = dist;
        }
    }
    return best;
}

} // namespace dfn::app
