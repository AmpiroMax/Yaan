/*
Module: engine/gameplay
File: engine/gameplay/sources/NpcBehaviour.h

Responsibility:
- ПОВЕДЕНИЯ НПС ПОВЕРХ ОЧЕРЕДИ ДЕЙСТВИЙ (docs/design/NPC_NAVIGATION.md §5):
  бродить в радиусе, патруль по точкам, следовать за целью на дистанции,
  повернуться к игроку рядом. Компонент — plain data (правило 8); система —
  ТОЛЬКО enqueue существующих NpcAction и ТОЛЬКО при пустой очереди (правило
  15: второго пути управления нет, набор действий не расширяется).

Key items:
- Wander / Patrol / Follow / LookAtNearby: режимы; NpcBehaviour — компонент с
  режимом, зерном и скрэтчем системы (следующая точка патруля, куда шли за
  целью, куда смотрели).
- run_npc_behaviours(): раз в тик до execute_npc_actions; с сеткой точки
  брожения выбираются на проходимом этаже (nav_locate), без сетки — как есть.
  ГСЧ — splitmix64 от зерна ⊕ EntityId ⊕ тик: прогон записи совпадает
  (правило 13).
- NpcBehaviourReport: что система сделала за тик (прибор).

Dependencies:
- Uses: NpcAction.h (enqueue, действия), NavGrid.h (nav_locate),
  engine/core/ecs, glm, стандартная библиотека.
- Used by: engine/app (стендовый бот, тела НПС), tests/sim
  (sim_npc_behaviours).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Никакой записи в Transform/PlayerState отсюда — только enqueue.
- Правило 12: время — сим-секунды из тиков; часов стены нет.
*/
#pragma once

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/NpcAction.h"

#include <cstdint>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

namespace dfn::ecs {
class World;
}

namespace dfn::gameplay {

struct NavGrid;

/// Бродить: случайная цель в круге, потом пауза.
struct Wander {
    glm::vec3 center{0.0f};
    float radius = 5.0f;
    float pause_s = 1.0f;
    MoveGait gait = MoveGait::Walk;
};

/// Патруль по точкам, по кругу или до конца; пауза на точке.
struct Patrol {
    std::vector<glm::vec3> points;
    bool loop = true;
    float pause_s = 1.0f;
    MoveGait gait = MoveGait::Walk;
};

/// Следовать за целью на дистанции: MoveTo к точке «цель минус distance по
/// линии к нам», переочередь, когда цель ушла дальше distance + slack.
struct Follow {
    ecs::EntityId target{};
    float distance = 2.0f;
    float slack_m = 1.0f; ///< 0 — NAV_FOLLOW_SLACK_M из реестра
    MoveGait gait = MoveGait::Walk;
};

/// Повернуться к игроку, когда он в радиусе (Face при пустой очереди).
struct LookAtNearby {
    float radius = 4.0f;
};

struct NpcBehaviour {
    std::variant<std::monostate, Wander, Patrol, Follow, LookAtNearby> mode;
    uint64_t seed = 0;
    // --- скрэтч системы (не контракт) ---
    uint32_t patrol_next = 0;
    bool patrol_done = false;
    glm::vec3 follow_goal{0.0f};
    bool follow_active = false;
    float look_yaw = 0.0f;   ///< рыск, на который заказали Face
    bool look_active = false;
};

struct NpcBehaviourReport {
    uint32_t enqueued = 0;   ///< действий поставлено в очереди за тик
    uint32_t interrupted = 0; ///< очередей сброшено (Follow: цель ушла)
    uint32_t wander_misses = 0; ///< попыток брожения мимо проходимого этажа
};

/// Один тик всех поведений. `grid` — сетка карты (nullptr — точки как есть).
NpcBehaviourReport run_npc_behaviours(ecs::World& world, const NavGrid* grid, uint64_t sim_tick);

} // namespace dfn::gameplay
