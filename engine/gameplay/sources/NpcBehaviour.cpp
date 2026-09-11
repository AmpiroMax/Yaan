/*
Module: engine/gameplay
File: engine/gameplay/sources/NpcBehaviour.cpp

Responsibility:
- Система поведений НПС (NpcBehaviour.h): при пустой очереди ставит действия
  по режиму. Wander — случайная точка в круге на проходимом этаже (до
  WANDER_TRIES попыток, иначе тик пропущен и посчитан), затем Wait; Patrol —
  MoveTo к следующей точке и Wait; Follow — MoveTo к точке на distance от
  цели, сброс очереди и новый MoveTo, когда цель ушла дальше slack от той
  точки; LookAtNearby — Face{игрок} при пустой очереди, когда игрок в радиусе
  и рыск к нему ушёл дальше NPC_LOOK_REFACE_DEG от заказанного (иначе Face
  завершался бы каждый тик заново — шторм событий).

Key items:
- splitmix(): один шаг splitmix64 от (зерно ⊕ id ⊕ тик ⊕ попытка) — без
  состояния, прогон записи совпадает (правило 13).
- player_of(): игрок — ходок без очереди действий (первый найденный).
- run_npc_behaviours(): тик.

Dependencies:
- Uses: NpcBehaviour.h, NpcAction.h, NavGrid.h, PlayerMovement.h
  (PlayerState — признак ходока), engine/core/ecs World, реестр.
- Used by: engine/app, tests/sim.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Только enqueue/clear_queue (правило 15). Стоящему НПС без действия
  очередь пуста — поведение решает каждый тик, ставить ли что-то.
*/
#include "engine/gameplay/sources/NpcBehaviour.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/NavGrid.h"
#include "engine/gameplay/sources/PlayerMovement.h"

#include <cmath>
#include <optional>

namespace dfn::gameplay {

namespace {

/// Попыток найти точку брожения на проходимом этаже за тик: структурная
/// величина — восемь бросков покрывают круг с запасом, дальше тик пропущен и
/// посчитан (wander_misses), следующий тик бросает заново с другим зерном.
constexpr int WANDER_TRIES = 8;

uint64_t splitmix(uint64_t x) {
    uint64_t z = x + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

float unit(uint64_t x) { return static_cast<float>(splitmix(x) >> 40) * (1.0f / 16777216.0f); }

float yaw_to(const glm::vec3& from, const glm::vec3& to) {
    return std::atan2(to.x - from.x, -(to.z - from.z));
}

float wrap_pi(float a) { return std::atan2(std::sin(a), std::cos(a)); }

std::optional<ecs::EntityId> player_of(ecs::World& world) {
    for (auto [id, ps, tr] : world.view<PlayerState, components::Transform>()) {
        (void)ps;
        (void)tr;
        if (world.get<NpcActionQueue>(id) == nullptr) {
            return id;
        }
    }
    return std::nullopt;
}

} // namespace

NpcBehaviourReport run_npc_behaviours(ecs::World& world, const NavGrid* grid, uint64_t sim_tick) {
    NpcBehaviourReport rep;
    const bool with_grid = grid != nullptr && grid->valid();
    const std::optional<ecs::EntityId> player = player_of(world);
    for (auto [id, beh, queue, transform] : world.view<NpcBehaviour, NpcActionQueue, components::Transform>()) {
        if (auto* w = std::get_if<Wander>(&beh.mode)) {
            if (!queue.pending.empty()) {
                continue;
            }
            bool placed = false;
            for (int t = 0; t < WANDER_TRIES && !placed; ++t) {
                const uint64_t k = beh.seed ^ id.packed() ^ (sim_tick * 0x9E37ull) ^ (static_cast<uint64_t>(t) << 56);
                const float u = unit(k);
                const float v = unit(k ^ 0xA5A5A5A5A5A5A5A5ull);
                const float r = w->radius * std::sqrt(u); // равномерно по кругу
                const float a = 6.2831853f * v;
                glm::vec3 goal = w->center + glm::vec3{r * std::sin(a), 0.0f, r * std::cos(a)};
                if (with_grid) {
                    const auto ref = nav_locate(*grid, goal, static_cast<float>(config::NAV_SNAP_M));
                    if (!ref) {
                        ++rep.wander_misses;
                        continue;
                    }
                    goal = nav_point(*grid, *ref);
                }
                enqueue(queue, MoveTo{goal, 0.0f, w->gait});
                ++rep.enqueued;
                ++rep.moves;
                if (w->pause_s > 0.0f) {
                    enqueue(queue, Wait{w->pause_s});
                    ++rep.enqueued;
                    ++rep.waits;
                }
                placed = true;
            }
        } else if (auto* p = std::get_if<Patrol>(&beh.mode)) {
            if (!queue.pending.empty() || p->points.empty() || beh.patrol_done) {
                continue;
            }
            if (beh.patrol_next >= p->points.size()) {
                if (!p->loop) {
                    beh.patrol_done = true;
                    continue;
                }
                beh.patrol_next = 0;
                // пауза на круг — перед новым кругом (у стендового бота — секунда,
                // как было до переезда патруля сюда: после PathBlocked не
                // долбиться в препятствие каждый тик)
                if (p->pause_at_loop_s > 0.0f) {
                    enqueue(queue, Wait{p->pause_at_loop_s});
                    ++rep.enqueued;
                    ++rep.waits;
                }
            }
            enqueue(queue, MoveTo{p->points[beh.patrol_next], 0.0f, p->gait});
            ++rep.enqueued;
            ++rep.moves;
            if (p->pause_s > 0.0f) {
                enqueue(queue, Wait{p->pause_s});
                ++rep.enqueued;
                ++rep.waits;
            }
            ++beh.patrol_next;
        } else if (auto* f = std::get_if<Follow>(&beh.mode)) {
            const auto* tt = world.alive(f->target) ? world.get<components::Transform>(f->target) : nullptr;
            if (tt == nullptr) {
                continue; // цели нет — стоим; TargetGone — дело исполнителя, не наше
            }
            const float slack = f->slack_m > 0.0f ? f->slack_m : static_cast<float>(config::NAV_FOLLOW_SLACK_M);
            const glm::vec3 d = tt->position - transform.position;
            const float dist = glm::length(glm::vec2{d.x, d.z});
            // точка на distance от цели по линии к нам
            glm::vec3 goal = tt->position;
            if (dist > 1.0e-3f) {
                goal -= d * (f->distance / dist);
            }
            if (queue.pending.empty()) {
                if (dist > f->distance + slack) {
                    enqueue(queue, MoveTo{goal, 0.0f, f->gait});
                    ++rep.enqueued;
                    beh.follow_goal = goal;
                    beh.follow_active = true;
                } else {
                    beh.follow_active = false;
                }
            } else if (beh.follow_active) {
                // цель ушла от точки, к которой идём, дальше slack — заново
                const glm::vec3 moved = goal - beh.follow_goal;
                if (glm::length(glm::vec2{moved.x, moved.z}) > slack) {
                    clear_queue(queue);
                    ++rep.interrupted;
                    enqueue(queue, MoveTo{goal, 0.0f, f->gait});
                    ++rep.enqueued;
                    beh.follow_goal = goal;
                }
            }
        } else if (auto* l = std::get_if<LookAtNearby>(&beh.mode)) {
            if (!queue.pending.empty() || !player) {
                continue;
            }
            const auto* pt = world.get<components::Transform>(*player);
            if (pt == nullptr) {
                continue;
            }
            const glm::vec3 d = pt->position - transform.position;
            if (glm::length(glm::vec2{d.x, d.z}) > l->radius) {
                beh.look_active = false;
                continue;
            }
            const float want = yaw_to(transform.position, pt->position);
            if (beh.look_active
                && std::abs(wrap_pi(want - beh.look_yaw)) < glm::radians(static_cast<float>(config::NPC_LOOK_REFACE_DEG))) {
                continue; // уже смотрим туда — Face заново не ставим
            }
            enqueue(queue, Face{*player, {}});
            ++rep.enqueued;
            beh.look_yaw = want;
            beh.look_active = true;
        }
    }
    return rep;
}

} // namespace dfn::gameplay
