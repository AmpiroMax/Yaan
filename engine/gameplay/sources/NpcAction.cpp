/*
Module: engine/gameplay
File: engine/gameplay/sources/NpcAction.cpp

Responsibility:
- ИСПОЛНИТЕЛЬ ДЕЙСТВИЙ НПС (контракт NpcAction.h, Q70/Rule 15) поверх той же
  заявки локомоции, что у игрока: НПС — ходок (PlayerState + капсула), и
  исполнитель раз в тик пишет ему ВВОД — рыск к цели, ось «вперёд»,
  передачу, — а дальше сим ведёт его теми же ролями, поворотами и стопами,
  что игрока (player_pre_step/post_step по всем PlayerState). Никакой второй
  дороги управления: только очередь и её исполнитель.
- MoveTo: довернуться, идти, прийти в радиус (NPC_ARRIVE_RADIUS); нет
  прогресса NPC_STUCK_S — PathBlocked. Face: рыск к точке/сущности до
  NPC_FACE_DONE_DEG. Wait: сим-секунды. Say/GiveItem/Attack/SetSchedule —
  не сейчас (закроют диалоги/боёвка/расписания): завершаются сразу, вслух.

Key items:
- spawn_npc(): ходок как игрок + пустая очередь; ввод игрока его не трогает
  (player_accumulate_input пропускает сущности с очередью).
- execute_npc_actions(): один проход по очередям; события на шину post().
- NpcMoveProgress: скрэтч исполнителя (лучшая дистанция, таймер застревания)
- NpcNavState: скрэтч пути (NPC_NAVIGATION.md §4): путевые точки, закрытые
  столбцы со сроком, счётчики перепланов и уступания; план на активации
  MoveTo, переплан по blocked прошлого тика (WalkerLocomotion), по застою и
  по истёкшему уступанию; больше NAV_REPLAN_MAX — PathBlocked.
- Уступание: встречный в NAV_YIELD_M впереди — стоим до NAV_YIELD_S;
  уступает больший EntityId, игроку (без очереди) уступают все.
  — свой компонент, контрактную очередь не расширяет.

Dependencies:
- Uses: NpcAction.h, PlayerMovement.h (PlayerState, spawn как у игрока),
  World, EventBus, Constants (NPC_*, BODY_TURN_RATE, SIM_DT).
- Used by: App (раз в тик до player_pre_step), tests/sim/NpcActionTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Набор действий НЕ расширять здесь (Rule 26 — групповой синк).
- Рыск сим'а: 0 = −Z, по часовой сверху; цель → atan2(dx, −dz).
*/
#include "engine/gameplay/sources/NpcAction.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/NavGrid.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>

namespace dfn::gameplay {

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);

/// Скрэтч исполнителя на активное MoveTo — не часть контрактной очереди.
struct NpcMoveProgress {
    uint64_t sequence = 0;   // к какому действию относится
    float best_m = 0.0f;     // лучшая дистанция до цели
    float stall_s = 0.0f;    // сколько без улучшения на NPC_STUCK_PROGRESS_M
};

/// Скрэтч пути на активное MoveTo (§4) — не часть контрактной очереди.
struct NpcNavState {
    uint64_t sequence = 0;
    NavPath path;
    bool has_path = false;
    uint32_t replans = 0;
    uint32_t yields = 0;
    float yield_s = 0.0f;
    float since_replan_s = 1.0e9f; ///< blocked после переплана не считается NAV_REPLAN_GRACE_S
    struct Block {
        NavCellBlock cell;
        float ttl_s = 0.0f;
    };
    std::vector<Block> blocks;
    std::vector<NavCellBlock> block_cells; ///< срез blocks для поиска
};

float wrap_pi(float a) {
    return std::atan2(std::sin(a), std::cos(a));
}

/// Рыск сим'а к точке: 0 = −Z, положительный — по часовой сверху.
float yaw_to(const glm::vec3& from, const glm::vec3& to) {
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    return std::atan2(dx, -dz);
}

float turn_toward(float yaw, float want, float rate) {
    const float d = wrap_pi(want - yaw);
    const float most = rate * DT;
    return yaw + std::clamp(d, -most, most);
}

void finish(NpcActionQueue& queue, events::EventBus& events, ecs::EntityId npc,
            NpcActionFailure reason) {
    if (queue.pending.empty()) {
        return;
    }
    const uint64_t seq = queue.pending.front().sequence;
    queue.pending.erase(queue.pending.begin());
    queue.active_elapsed = 0.0f;
    if (reason == NpcActionFailure::None) {
        events.post(NpcActionCompleted{npc, seq});
    } else {
        events.post(NpcActionFailed{npc, seq, reason});
    }
}

} // namespace

ecs::EntityId spawn_npc(ecs::World& world, platform::IPhysics& physics,
                        const glm::vec3& spawn_pos) {
    const ecs::EntityId id = spawn_player(world, physics, spawn_pos);
    world.add(id, NpcActionQueue{});
    return id;
}

uint64_t enqueue(NpcActionQueue& queue, NpcAction action) {
    const uint64_t seq = queue.next_sequence++;
    queue.pending.push_back(QueuedNpcAction{seq, std::move(action)});
    return seq;
}

void clear_queue(NpcActionQueue& queue) {
    if (!queue.pending.empty()) {
        queue.interrupted_sequence = queue.pending.front().sequence;
    }
    queue.pending.clear();
    queue.active_elapsed = 0.0f;
}

namespace {

glm::vec3 forward_of(float yaw) { return glm::vec3{std::sin(yaw), 0.0f, -std::cos(yaw)}; }

void expire_blocks(NpcNavState& ns) {
    for (auto it = ns.blocks.begin(); it != ns.blocks.end();) {
        it->ttl_s -= DT;
        it = it->ttl_s <= 0.0f ? ns.blocks.erase(it) : it + 1;
    }
    ns.block_cells.clear();
    for (const NpcNavState::Block& b : ns.blocks) {
        ns.block_cells.push_back(b.cell);
    }
}

bool cell_closed(const NpcNavState& ns, const NavCellBlock& c) {
    for (const NpcNavState::Block& b : ns.blocks) {
        if (b.cell.ix == c.ix && b.cell.iz == c.iz) {
            return true;
        }
    }
    return false;
}

/// Закрыть столбец под точкой и кольцо ring ячеек вокруг: чужая капсула —
/// кольцо в радиус капсулы (эрозия временных блоков не считается), запертость
/// — кольцо 1, чтобы обход отклонил ввод дальше LOCO_BLOCKED_RELEASE_DEG.
void close_cells(NpcNavState& ns, const NavGrid& g, const glm::vec3& p, int ring) {
    NavCellBlock c;
    if (!nav_cell_of(g, p, c)) {
        return;
    }
    for (int dz = -ring; dz <= ring; ++dz) {
        for (int dx = -ring; dx <= ring; ++dx) {
            const int64_t ix = static_cast<int64_t>(c.ix) + dx;
            const int64_t iz = static_cast<int64_t>(c.iz) + dz;
            if (ix < 0 || iz < 0 || ix >= g.nx || iz >= g.nz) {
                continue;
            }
            const NavCellBlock cc{static_cast<uint32_t>(ix), static_cast<uint32_t>(iz)};
            bool found = false;
            for (NpcNavState::Block& b : ns.blocks) {
                if (b.cell.ix == cc.ix && b.cell.iz == cc.iz) {
                    b.ttl_s = static_cast<float>(config::NAV_BLOCK_TTL_S);
                    found = true;
                    break;
                }
            }
            if (!found) {
                ns.blocks.push_back({cc, static_cast<float>(config::NAV_BLOCK_TTL_S)});
                ns.block_cells.push_back(cc);
            }
        }
    }
}

bool plan(NpcNavState& ns, NavContext& nav, const glm::vec3& from, const glm::vec3& to) {
    ns.has_path = nav_find_path(*nav.grid, nav.search, from, to, ns.path, true, ns.block_cells);
    ns.path.next = ns.has_path && ns.path.points.size() > 1 ? 1 : 0;
    ns.since_replan_s = 0.0f;
    return ns.has_path;
}

/// Встречный, которому надо уступить: в NAV_YIELD_M впереди по ходу, в конусе
/// ±45°; уступает БОЛЬШИЙ EntityId (у пары — ровно один), игроку (без
/// очереди) — все. Тот, чей столбец уже закрыт (обходим), не считается —
/// иначе стоящий игрок держал бы НПС вечно.
std::optional<glm::vec3> yield_to(ecs::World& world, ecs::EntityId me, const glm::vec3& pos, float yaw,
                                  const NpcNavState& ns, const NavGrid& g) {
    const glm::vec3 fwd = forward_of(yaw);
    const float reach = static_cast<float>(config::NAV_YIELD_M);
    std::optional<glm::vec3> found;
    for (auto [other, ops, otr] : world.view<PlayerState, components::Transform>()) {
        (void)ops;
        if (other == me) {
            continue;
        }
        const glm::vec3 d = otr.position - pos;
        const float dist = glm::length(glm::vec2{d.x, d.z});
        if (dist > reach || dist < 1.0e-4f) {
            continue;
        }
        if ((d.x * fwd.x + d.z * fwd.z) / dist < 0.7071f) {
            continue;
        }
        const bool other_is_player = world.get<NpcActionQueue>(other) == nullptr;
        const bool i_yield = other_is_player || me.index > other.index
                             || (me.index == other.index && me.generation > other.generation);
        if (!i_yield) {
            continue;
        }
        NavCellBlock oc;
        if (nav_cell_of(g, otr.position, oc) && cell_closed(ns, oc)) {
            continue; // уже обходим
        }
        found = otr.position;
    }
    return found;
}

} // namespace

NpcNavReport npc_nav_report(const ecs::World& world, ecs::EntityId npc) {
    NpcNavReport r;
    const auto* ns = world.get<NpcNavState>(npc);
    if (ns == nullptr) {
        return r;
    }
    r.has_path = ns->has_path;
    r.waypoints = static_cast<uint32_t>(ns->path.points.size());
    r.next = ns->path.next;
    r.path_m = ns->path.length_m();
    r.replans = ns->replans;
    r.yields = ns->yields;
    r.blocks = static_cast<uint32_t>(ns->blocks.size());
    return r;
}

void execute_npc_actions(ecs::World& world, platform::IPhysics& physics,
                         events::EventBus& events, uint64_t sim_tick, NavContext* nav) {
    (void)physics;
    (void)sim_tick;
    const bool with_nav = nav != nullptr && nav->grid != nullptr && nav->grid->valid();
    const float turn_rate = static_cast<float>(config::NPC_TURN_RATE);
    for (auto [id, queue, state, transform] :
         world.view<NpcActionQueue, PlayerState, components::Transform>()) {
        // Вводу НПС каждый тик — с чистого листа: без действия он стоит.
        state.move_axes = glm::vec2{0.0f};
        state.run = false;
        state.jog = false;
        if (queue.interrupted_sequence != 0) {
            // clear_queue() снял активное действие — сообщить, как велит контракт
            events.post(NpcActionFailed{id, queue.interrupted_sequence,
                                        NpcActionFailure::Interrupted});
            queue.interrupted_sequence = 0;
        }
        // КОРПУС НПС ВЕДЁТ ТЕЛО (§16.4): исполнитель заказывает, куда встать
        // лицом (want_yaw), тело поворачивается клипом, на ходу доворачивается
        // к прицелу симом; без действия заказа нет. (Прежний шов копирует
        // прицел в корпус — NpcBodies, контрольная рука.)
        state.want_yaw_valid = false;
        if (queue.pending.empty()) {
            continue;
        }
        QueuedNpcAction& active = queue.pending.front();
        queue.active_elapsed += DT;
        if (auto* move = std::get_if<MoveTo>(&active.action)) {
            const float radius = move->acceptance_radius > 0.0f
                                     ? move->acceptance_radius
                                     : static_cast<float>(config::NPC_ARRIVE_RADIUS);
            const glm::vec2 d{move->target.x - transform.position.x,
                              move->target.z - transform.position.z};
            const float dist = glm::length(d);
            if (dist <= radius) {
                finish(queue, events, id, NpcActionFailure::None);
                continue;
            }
            // --- ПУТЬ ПО СЕТКЕ (§4) ---------------------------------------------
            glm::vec3 steer = move->target;
            NpcNavState* ns = nullptr;
            if (with_nav) {
                ns = world.get<NpcNavState>(id);
                if (ns == nullptr) {
                    world.add(id, NpcNavState{});
                    ns = world.get<NpcNavState>(id);
                }
                if (ns->sequence != active.sequence) {
                    *ns = NpcNavState{};
                    ns->sequence = active.sequence;
                    if (!plan(*ns, *nav, transform.position, move->target)) {
                        finish(queue, events, id, NpcActionFailure::PathBlocked);
                        continue;
                    }
                    ns->since_replan_s = 1.0e9f; // стартовый план — не переплан, тишины нет
                }
                expire_blocks(*ns);
                ns->since_replan_s += DT;
                // капсула заперта прошлым тиком (§16.9) — столбец впереди закрыт,
                // переплан; после переплана NAV_REPLAN_GRACE_S сигнал не считается:
                // защёлка машины держится, пока новый ввод не ушёл дальше
                // LOCO_BLOCKED_RELEASE_DEG, и каждый тик перепланировал бы заново
                const auto* walker = world.get<WalkerLocomotion>(id);
                const bool blocked = walker != nullptr && walker->request.valid && walker->request.blocked
                                     && ns->since_replan_s >= static_cast<float>(config::NAV_REPLAN_GRACE_S);
                // встречный впереди — уступаем; ждали дольше NAV_YIELD_S — обходим
                const auto other = yield_to(world, id, transform.position, state.yaw, *ns, *nav->grid);
                bool replan = false;
                glm::vec3 close_at{0.0f};
                int ring = 1;
                if (blocked) {
                    close_at = transform.position
                               + forward_of(state.yaw) * (static_cast<float>(config::PLAYER_CAPSULE_RADIUS) + nav->grid->cell);
                    replan = true;
                } else if (other) {
                    ring = static_cast<int>(std::ceil(2.0f * static_cast<float>(config::PLAYER_CAPSULE_RADIUS) / nav->grid->cell));
                    ++ns->yields;
                    ns->yield_s += DT;
                    if (ns->yield_s >= static_cast<float>(config::NAV_YIELD_S)) {
                        close_at = *other;
                        replan = true;
                        ns->yield_s = 0.0f;
                    }
                } else {
                    ns->yield_s = 0.0f;
                }
                if (replan) {
                    if (++ns->replans > static_cast<uint32_t>(config::NAV_REPLAN_MAX)) {
                        finish(queue, events, id, NpcActionFailure::PathBlocked);
                        continue;
                    }
                    close_cells(*ns, *nav->grid, close_at, ring);
                    if (!plan(*ns, *nav, transform.position, move->target)) {
                        finish(queue, events, id, NpcActionFailure::PathBlocked);
                        continue;
                    }
                }
                if (other && !replan) {
                    // стоим, лицом к своей точке — ввода нет
                    state.want_yaw = yaw_to(transform.position, ns->has_path ? ns->path.points[ns->path.next] : move->target);
                    state.want_yaw_valid = true;
                    continue;
                }
                if (ns->has_path && !ns->path.points.empty()) {
                    const uint32_t last = static_cast<uint32_t>(ns->path.points.size() - 1);
                    while (ns->path.next < last) {
                        const glm::vec3& w = ns->path.points[ns->path.next];
                        const float dw = glm::length(glm::vec2{w.x - transform.position.x, w.z - transform.position.z});
                        if (dw > static_cast<float>(config::NAV_WAYPOINT_M)) {
                            break;
                        }
                        ++ns->path.next;
                    }
                    steer = ns->path.points[ns->path.next];
                    if (ns->path.next == last) {
                        steer = move->target;
                    }
                }
            }
            const float want_yaw = yaw_to(transform.position, steer);
            state.yaw = turn_toward(state.yaw, want_yaw, turn_rate);
            state.want_yaw = want_yaw;
            state.want_yaw_valid = true;
            // Идём, когда цель впереди: иначе сначала доворот на месте (клипы
            // поворота у тела стреляют по той же разнице «взгляд − корпус»).
            const float off = std::abs(wrap_pi(want_yaw - state.yaw));
            if (off < glm::radians(static_cast<float>(config::NPC_MOVE_CONE_DEG))) {
                state.move_axes = glm::vec2{0.0f, 1.0f};
                state.run = move->gait == MoveGait::Run;
            }
            NpcMoveProgress* prog = world.get<NpcMoveProgress>(id);
            if (prog == nullptr) {
                world.add(id, NpcMoveProgress{active.sequence, dist, 0.0f});
                prog = world.get<NpcMoveProgress>(id);
            } else if (prog->sequence != active.sequence) {
                *prog = NpcMoveProgress{active.sequence, dist, 0.0f};
            }
            if (prog != nullptr) {
                if (dist < prog->best_m - static_cast<float>(config::NPC_STUCK_PROGRESS_M)) {
                    prog->best_m = dist;
                    prog->stall_s = 0.0f;
                } else if (state.move_axes.y > 0.0f) {
                    prog->stall_s += DT;
                    if (prog->stall_s >= static_cast<float>(config::NPC_STUCK_S)) {
                        // с сеткой застой — вторая линия переплана (§4 (б)):
                        // столбец впереди закрыт, план заново; сверх
                        // NAV_REPLAN_MAX — тупик
                        if (ns != nullptr && ns->replans < static_cast<uint32_t>(config::NAV_REPLAN_MAX)) {
                            ++ns->replans;
                            close_cells(*ns, *nav->grid,
                                        transform.position
                                            + forward_of(state.yaw)
                                                  * (static_cast<float>(config::PLAYER_CAPSULE_RADIUS) + nav->grid->cell),
                                        1);
                            prog->stall_s = 0.0f;
                            if (!plan(*ns, *nav, transform.position, move->target)) {
                                finish(queue, events, id, NpcActionFailure::PathBlocked);
                                continue;
                            }
                        } else {
                            finish(queue, events, id, NpcActionFailure::PathBlocked);
                            continue;
                        }
                    }
                }
            }
        } else if (auto* face = std::get_if<Face>(&active.action)) {
            glm::vec3 point = face->point;
            if (!face->target.is_null()) {
                const auto* tt = world.alive(face->target)
                                     ? world.get<components::Transform>(face->target)
                                     : nullptr;
                if (tt == nullptr) {
                    finish(queue, events, id, NpcActionFailure::TargetGone);
                    continue;
                }
                point = tt->position;
            }
            const float want_yaw = yaw_to(transform.position, point);
            state.yaw = turn_toward(state.yaw, want_yaw, turn_rate);
            state.want_yaw = want_yaw;
            state.want_yaw_valid = true;
            if (std::abs(wrap_pi(want_yaw - state.yaw))
                < glm::radians(static_cast<float>(config::NPC_FACE_DONE_DEG))) {
                finish(queue, events, id, NpcActionFailure::None);
            }
        } else if (auto* wait = std::get_if<Wait>(&active.action)) {
            if (queue.active_elapsed >= wait->seconds) {
                finish(queue, events, id, NpcActionFailure::None);
            }
        } else {
            // Say / GiveItem / Attack / SetSchedule — не сейчас (диалоги, боёвка,
            // расписания); контракт не даёт исполнителю выдумывать действия,
            // так что действие просто завершается, и об этом сказано вслух.
            static bool said = false;
            if (!said) {
                std::fprintf(stderr, "[npc] Say/GiveItem/Attack/SetSchedule ещё не исполняются "
                                     "— завершаются сразу (сказано один раз)\n");
                said = true;
            }
            finish(queue, events, id, NpcActionFailure::None);
        }
    }
}

} // namespace dfn::gameplay
