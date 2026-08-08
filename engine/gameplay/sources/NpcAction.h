/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 00:18:26
Module: engine/gameplay
File: engine/gameplay/sources/NpcAction.h

Responsibility:
- THE NPC control API (Q70, Rule 15): the typed NpcAction variant set, the
  per-NPC action queue component, and the executor contract. There is NO other
  way to drive an NPC — scripts, tests, the editor and (later) the LLM all
  enqueue the same values through enqueue().

Key items:
- MoveTo / Face / Say / GiveItem / Attack / Wait / SetSchedule: action payloads.
- NpcAction: std::variant over the payloads — a plain, copyable, serializable value.
- NpcActionQueue: per-NPC component (plain data, Rule 8); front = active action.
- enqueue() / clear_queue(): the ONLY mutation entry points (Rule 15).
- execute_npc_actions(): the executor system (Rule 9), runs once per fixed tick.
- NpcActionRecord: journal entry (Rule 13.3 — actions are recordable/replayable).
- NpcActionCompleted / NpcActionFailed: events published on the EventBus.

Dependencies:
- Uses: engine/core/ecs (EntityId by value; World/EventBus forward-declared),
  engine/platform/physics (forward-declared, executor parameter), glm, stdlib.
- Used by: game systems (schedules, quests, combat AI), tests, the editor,
  later the LLM planner — all through the same enqueue().

Notes:
- Rule 15 enforcement by construction: executor internals live in
  engine/gameplay sources; no other public function mutates NPC state. Direct
  writes to an NPC's Transform/stats outside the executor are a contract
  violation reviewable by grep.
- Journal-friendly (Rule 13.3): every payload is a value type (ids, EntityId,
  vectors, one std::string). enqueue() assigns a per-NPC monotonic sequence
  number; (npc, sequence, sim_tick, action) is the complete replay record, and
  Completed/Failed events reference the same sequence.
- Durations are sim seconds accumulated from fixed ticks (Rule 12); the
  executor never reads wall-clock time. Speeds come from NUMBERS constants
  (WALK_SPEED / RUN_SPEED) selected via MoveGait, never literals (Rule 14).
- Queue semantics: front of pending is the active action; completion pops it;
  failure pops it and publishes NpcActionFailed (the enqueuer decides what
  next — the executor never invents actions). clear_queue() interrupts the
  active action. Queues are chunk-resident state and are DROPPED on chunk
  unload (agreed with core, stage-1 sync): schedules re-derive intent on
  respawn; the queue itself is never save-delta state.
- Say: for scripted lines llm_text is empty and `line` names the DialogueLine
  (voiced offline, Q69); when the LLM provides the words (Q67), llm_text holds
  them and `line` still names the scripted intent whose fallback/conditions
  applied — so a journal replay works with either backend.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NEVER add a second control path for NPCs (Rule 15). New capabilities = new
  variant alternatives, added only via group sync (Rule 26).
- Keep every payload a plain serializable value; no pointers, no handles to
  platform objects.
*/
/*
UPD:
- 09:08:2026 - 00:18:26: Initial stage-1 contract (Q70 action set, queue
                         component, executor + journal + events).
*/

#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>
#include <variant>
#include <vector>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/Ids.h"

namespace dfn::ecs {
class World;
}
namespace dfn::events {
class EventBus;
}
namespace dfn::platform {
class IPhysics;
}

namespace dfn::gameplay {

// --- Action payloads (Q70) ---------------------------------------------------

enum class MoveGait : uint8_t {
    Walk, // WALK_SPEED (NUMBERS.md)
    Run,  // RUN_SPEED  (NUMBERS.md)
};

// Walk/run to a world position. Completes when within acceptance_radius;
// fails with PathBlocked when no progress is possible.
struct MoveTo {
    glm::vec3 target{0.0f};         // meters, world space
    float acceptance_radius = 0.0f; // 0 = NPC_ARRIVE_RADIUS default (NUMBERS.md)
    MoveGait gait = MoveGait::Walk;
};

// Turn to face an entity (if target is valid) or a world point.
struct Face {
    ecs::EntityId target{};  // invalid => face `point`
    glm::vec3 point{0.0f};   // meters, world space
};

// Speak a dialogue line (Q71: the line itself does not know its speaker; the
// NPC's VoiceTimbre component selects the audio, Q80).
struct Say {
    DialogueLineId line{}; // scripted intent — always set
    std::string llm_text;  // empty = scripted words; non-empty = LLM words (Q67)
};

// Transfer items to another entity's inventory.
struct GiveItem {
    ecs::EntityId recipient{};
    ItemId item{};
    uint32_t count = 1;
};

// Engage a target with the equipped means; resolution goes through the dice
// combat API (Dice.h) inside the executor.
struct Attack {
    ecs::EntityId target{};
};

// Idle for a duration of simulation time.
struct Wait {
    float seconds = 0.0f; // sim seconds (fixed ticks * SIM_DT)
};

// Switch the NPC's daily schedule; takes effect when the active action ends.
struct SetSchedule {
    ScheduleId schedule{};
};

// The closed action set (Q70). Extending it is a group-sync event (Rule 26).
using NpcAction = std::variant<MoveTo, Face, Say, GiveItem, Attack, Wait, SetSchedule>;

// --- Queue component (Rule 8: plain data) ------------------------------------

struct QueuedNpcAction {
    uint64_t sequence = 0; // per-NPC monotonic, assigned by enqueue()
    NpcAction action;
};

// Per-NPC component. Front of `pending` is the active action. Executor-owned
// progress fields live here so the component alone snapshots execution state.
struct NpcActionQueue {
    std::vector<QueuedNpcAction> pending;
    uint64_t next_sequence = 1;  // sequence for the next enqueue
    float active_elapsed = 0.0f; // sim seconds spent on the active action
};

// --- The ONLY mutation entry points (Rule 15) --------------------------------

// Appends the action, assigns and returns its sequence number.
uint64_t enqueue(NpcActionQueue& queue, NpcAction action);

// Drops all pending actions, interrupting the active one (it fails with
// Interrupted). Used on schedule changes, combat alarms, chunk unload.
void clear_queue(NpcActionQueue& queue);

// --- Executor (Rule 9: near-stateless system) --------------------------------

enum class NpcActionFailure : uint8_t {
    None,
    PathBlocked,   // MoveTo could not make progress
    TargetGone,    // referenced entity no longer alive
    InvalidAction, // malformed payload (e.g. Say without a line)
    Interrupted,   // clear_queue() while active
};

// Published on the EventBus when an action finishes.
struct NpcActionCompleted {
    ecs::EntityId npc{};
    uint64_t sequence = 0;
};
struct NpcActionFailed {
    ecs::EntityId npc{};
    uint64_t sequence = 0;
    NpcActionFailure reason = NpcActionFailure::None;
};

// Journal entry (Rule 13.3): everything needed to replay one enqueue.
struct NpcActionRecord {
    uint64_t sim_tick = 0;
    ecs::EntityId npc{};
    uint64_t sequence = 0;
    NpcAction action;
};

// Runs once per fixed tick over view<NpcActionQueue, ...>: advances every NPC's
// active action (movement via IPhysics, speech via the dialogue system, combat
// via the dice API), pops finished ones, publishes Completed/Failed events.
// Receives interfaces as parameters and stores nothing (Rule 9).
void execute_npc_actions(ecs::World& world, platform::IPhysics& physics,
                         events::EventBus& events, uint64_t sim_tick);

} // namespace dfn::gameplay
