/*
Created: 17:08:2026 - 19:05:00
Last updated: 18:08:2026 - 13:08:07
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
- 18:08:2026 - 01:05:34: brush_outline и его радиусы. Обе границы — обод и плоская
  вершина — ДОСТАЮТСЯ ИЗ brush_weight бисекцией, поэтому зажим малого радиуса и
  форма спада наследуются, а не переписываются. Цвет: зелёный вверх, красный
  вниз, синий когда направления НЕТ (сглаживание тянет одни образцы вверх, а
  другие вниз), масть поверхности — для кисти покраски.
- 18:08:2026 - 13:08:07: StrokeRefresh — показ земли ПОКА КНОПКА ЗАЖАТА, с паузой, выведенной
  из измеренной цены перестройки (196 мс/чанк). Реализация в двадцать строк, и
  вся её ценность в том, что она ВЫРАЗИМА В ПРОВЕРКЕ: App держит окно и не
  заводится, а «за штрих земля изменилась больше одного раза» иначе никак не
  спросить — кадр показывает одно состояние.
*/

#include "engine/editor/sources/EditorBrush.h"

#include "engine/core/config/sources/Constants.h"

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

// ===================== WHEN TO SHOW THE GROUND MOVING =======================

bool StrokeRefresh::step(bool dabbed, float dt_s) {
    if (dabbed) {
        pending = true;
    }
    since_s += dt_s;
    if (!pending || since_s < wait_s) {
        return false;
    }
    // ПОКАЗЫВАЕМ. Счётчик обнуляется ЗДЕСЬ, а не в note_cost: цену может
    // сообщить только тот, кто перестраивал, а он вправе этого не делать — и
    // тогда пауза остаётся прежней, что честнее, чем считать бесплатным то,
    // чего не измерили.
    since_s = 0.0f;
    pending = false;
    ++pushes;
    return true;
}

void StrokeRefresh::note_cost(float rebuild_s) {
    spent_s += std::max(rebuild_s, 0.0f);
    // ПАУЗА ВЫВЕДЕНА ИЗ ЦЕНЫ, А НЕ НАЗНАЧЕНА. На машине, где чанк
    // перестраивается за 200 мс, показ будет раз в 0.6 с (упор в потолок); на
    // машине вдвое быстрее — раз в 0.6 же (потолок), а на локальной правке в
    // 20 мс — раз в 0.1 с (упор в пол). Числа сами следуют за железом и за тем,
    // как подешевеет перестройка, если её когда-нибудь сделают локальной.
    wait_s = std::clamp(rebuild_s * static_cast<float>(config::REFRESH_COST_RATIO),
                        static_cast<float>(config::REFRESH_MIN_PERIOD_S),
                        static_cast<float>(config::REFRESH_MAX_PERIOD_S));
}

void StrokeRefresh::end() {
    // СЛЕДУЮЩИЙ ШТРИХ НАЧИНАЕТСЯ С НЕМЕДЛЕННОГО ПОКАЗА, но с ПАМЯТЬЮ О ЦЕНЕ:
    // обнулять wait_s было бы обещанием, что второй штрих дешевле первого.
    since_s = 0.0f;
    pending = false;
    pushes = 0;
    spent_s = 0.0f;
}

// ========================= THE ZONE, MADE VISIBLE ===========================

namespace {

/// A bracket around the last distance at which `pred` still holds. `pred` must
/// be monotone — true up to some boundary and false past it — which brush_weight
/// guarantees by being non-increasing.
struct Bracket {
    float lo = 0.0f; ///< the last distance where the predicate held
    float hi = 0.0f; ///< the first distance where it did not
};

/// THE WHOLE HONESTY ARGUMENT LIVES HERE. The ring's radii are not computed
/// from radius_m and hardness a second time — they are MEASURED off the falloff
/// the brush actually applies. Change brush_weight and the outline follows;
/// there is no second definition to forget (Rule 32).
template <class Pred>
[[nodiscard]] Bracket narrow(Pred pred, float lo, float hi) {
    // Float bisection bottoms out at adjacent representables well before 48
    // halvings; the guard is the loop bound, the break is the real exit.
    for (int i = 0; i < 48; ++i) {
        const float mid = 0.5f * (lo + hi);
        if (!(mid > lo) || !(mid < hi)) {
            break;
        }
        (pred(mid) ? lo : hi) = mid;
    }
    return {lo, hi};
}

/// An upper bound that is certainly outside the brush, found by asking rather
/// than by reading radius_m — otherwise the clamp would be duplicated here.
[[nodiscard]] float outside_bound(const TerrainBrush& brush) {
    float hi = 1.0f;
    for (int guard = 0; guard < 64; ++guard) {
        if (brush_weight(hi, brush.radius_m, brush.hardness) <= 0.0f) {
            return hi;
        }
        hi *= 2.0f;
    }
    return hi;
}

/// THE SWATCH, for the surface brush. Painting has no up and no down, so
/// green/red would answer a question nobody asked; the ring wears the colour of
/// what is about to be painted instead. These are recognition colours for a
/// line on screen, not the terrain's rendered shade — the ground's own look is
/// the shader's business and copying it here would be a second palette to keep
/// in step for no gain.
[[nodiscard]] uint32_t surface_swatch(math::SurfaceClass surface) {
    switch (surface) {
    case math::SurfaceClass::Grass:          return 0xFF4AA85Au;
    case math::SurfaceClass::GrassRockBlend: return 0xFF78928Cu;
    case math::SurfaceClass::Rock:           return 0xFFA09A9Au;
    case math::SurfaceClass::Sand:           return 0xFF88C8D8u;
    case math::SurfaceClass::WaterBed:       return 0xFFC87A3Au;
    }
    return 0xFFFFFFFFu;
}

} // namespace

float brush_rim_m(const TerrainBrush& brush) {
    const auto inside = [&brush](float d) {
        return brush_weight(d, brush.radius_m, brush.hardness) > 0.0f;
    };
    // THE FIRST DISTANCE THAT IS OUT, not the last one that is in: a ring drawn
    // on `lo` would sit a float inside the brush and claim the rim is untouched
    // when the rim is exactly where the weight reaches zero.
    return narrow(inside, 0.0f, outside_bound(brush)).hi;
}

float brush_core_m(const TerrainBrush& brush) {
    const auto full = [&brush](float d) {
        return brush_weight(d, brush.radius_m, brush.hardness) >= 1.0f;
    };
    if (!full(0.0f)) {
        return 0.0f; // cannot happen for the real falloff; not assumed anyway
    }
    // THE LAST DISTANCE STILL AT FULL STRENGTH. A soft brush (hardness 0) has
    // none, and the bisection returns 0 on its own — no special case, because a
    // special case here would be a third statement about where the fade starts.
    return narrow(full, 0.0f, brush_rim_m(brush)).lo;
}

BrushDirection brush_direction(const TerrainBrush& brush, float ground_m) {
    switch (brush.mode) {
    case BrushMode::Raise:
        return BrushDirection::Up;
    case BrushMode::Lower:
        return BrushDirection::Down;
    case BrushMode::Smooth:
        // ONE DAB, BOTH WAYS. Smoothing pulls a bump down and fills the dip
        // beside it in the same stroke, so neither colour is true of it, and
        // picking one would teach the builder a direction the tool does not have.
        return BrushDirection::Mixed;
    case BrushMode::Flatten:
        if (brush.flatten_height_m > ground_m) {
            return BrushDirection::Up;
        }
        if (brush.flatten_height_m < ground_m) {
            return BrushDirection::Down;
        }
        return BrushDirection::Mixed; // level with the ground: nothing moves
    case BrushMode::Paint:
        return BrushDirection::None;
    }
    return BrushDirection::None;
}

uint32_t brush_outline_color(const TerrainBrush& brush, BrushDirection direction) {
    if (brush.mode == BrushMode::Paint) {
        return surface_swatch(brush.paint);
    }
    switch (direction) {
    case BrushDirection::Up:
        return BRUSH_UP_GREEN;
    case BrushDirection::Down:
        return BRUSH_DOWN_RED;
    case BrushDirection::Mixed:
    case BrushDirection::None:
        break;
    }
    return BRUSH_MIXED_BLUE;
}

int brush_outline_segments(float rim_m) {
    const int wanted = static_cast<int>(std::lround(6.2831853 * static_cast<double>(rim_m)));
    return std::clamp(wanted, 24, 128);
}

BrushOutline brush_outline(const TerrainBrush& brush, glm::vec2 centre,
                           const BrushGround& ground) {
    BrushOutline out;
    out.rim_m = brush_rim_m(brush);
    out.core_m = brush_core_m(brush);
    const auto ground_at = [&ground](glm::vec2 p) {
        return ground.base_at != nullptr ? ground.base_at(ground.ctx, p) : 0.0f;
    };
    out.direction = brush_direction(brush, ground_at(centre));
    out.color_rgba = brush_outline_color(brush, out.direction);

    const int segments = brush_outline_segments(out.rim_m);
    const auto ring = [&](float radius, std::vector<glm::vec3>& into) {
        if (radius <= 0.0f) {
            return;
        }
        into.reserve(static_cast<std::size_t>(segments) + 1);
        for (int i = 0; i <= segments; ++i) {
            // `i % segments` on the last step, so the closing point is the
            // FIRST one bit for bit rather than a second point that rounds to
            // almost the same place and leaves a hairline gap.
            const float a = 6.2831853f * static_cast<float>(i % segments)
                          / static_cast<float>(segments);
            const glm::vec2 p{centre.x + std::cos(a) * radius,
                              centre.y + std::sin(a) * radius};
            into.push_back({p.x, ground_at(p) + BRUSH_OUTLINE_LIFT_M, p.y});
        }
    };
    ring(out.rim_m, out.rim);
    // THE INNER RING ONLY WHEN THERE IS SOMETHING INSIDE IT. A flat top
    // narrower than the ground's own lattice holds no sample but the one under
    // the crosshair, so a ring around it separates nothing — it is a dot at the
    // aim point that the builder would read as a boundary.
    //
    // AND WITHOUT THIS FLOOR EVERY SOFT BRUSH WOULD WEAR A SPECK: at hardness 0
    // the algebraic flat top is zero, but the smoothstep still returns EXACTLY
    // 1.0f for the first few hundred microns (1 - 3t² rounds to 1 while
    // t < 1e-4), and brush_core_m reports that truthfully because it measures
    // the function rather than the formula. Under a millimetre is true and not
    // worth a line.
    if (out.core_m >= world::RELIEF_STEP_M) {
        ring(out.core_m, out.core);
    }
    return out;
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
