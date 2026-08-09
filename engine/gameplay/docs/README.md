<!--
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 01:02:15
-->
<!--
UPD:
- 09:08:2026 - 00:18:26: Stage-1 state: public contract headers only, no
  implementation yet.
- 09:08:2026 - 01:02:15: Stage 2 — implemented player movement
  (PlayerMovement.h/.cpp + World wrappers) and the dice RNG (Dice.cpp);
  NpcAction/stats/dialogue remain headers-only by stage scope.
-->

# engine/gameplay

## Responsibility

Game-agnostic RPG mechanics: the NPC control API (`NpcAction`, Q70/Rule 15),
attributes/skills with use-based progression (Q42), dice-roll combat (Q10),
the dialogue data model (Q71/Q74/Q79/Q80), and interaction components (Q11).
Zero external dependencies (Rule 1); platform access only through interfaces
passed into systems.

## Key types

- `sources/NpcAction.h` — `NpcAction` variant (MoveTo/Face/Say/GiveItem/Attack/
  Wait/SetSchedule), `NpcActionQueue` component, `enqueue()` (the ONLY way to
  drive an NPC), `execute_npc_actions()`, journal record + events.
- `sources/Stats.h` — `Attribute` (8), `Skill` (~12 placeholder), `Attributes`/
  `Skills` components, `record_use()` progression hook, `SkillRaised` event.
- `sources/Dice.h` — seedable `Rng`, `roll_die`/`roll_dice`/`percent_check`,
  `resolve_attack()`.
- `sources/Dialogue.h` — `DialogueLine`/`DialogueSegment`/`DialogueCondition`,
  `segment_content_hash()` (names audio files, frozen).
- `sources/NpcComponents.h` — `VoiceTimbre` (Q80), `ScheduleState` (Q44).
- `sources/Interaction.h` — `Highlightable`, `Openable`, `Lootable` (Q11).
- `sources/Ids.h` — typed stable content ids.
- `sources/PlayerMovement.h` (implemented, stage 2) — `PlayerState` component;
  ref-based core `accumulate_input`/`player_pre_step`/`player_post_step`
  (unit-testable without a World) plus World-facing `spawn_player` +
  per-tick wrappers for engine/app. Tick order: accumulate (per render frame)
  -> pre_step -> app calls `IPhysics::step(SIM_DT)` -> post_step; pre_step
  snapshots curr->prev on Transform/CameraPose pairs (Rule 12 contract).
  Implemented: `Dice.cpp` (splitmix64; `resolve_attack` waits for the combat
  grill).

## Usage example

```cpp
using namespace dfn::gameplay;
auto& queue = *world.get<NpcActionQueue>(npc);
enqueue(queue, MoveTo{.target = market_stall, .gait = MoveGait::Walk});
enqueue(queue, Say{.line = greeting_line_id});
// each fixed tick, in the sim system order:
execute_npc_actions(world, physics, events, sim_tick);
```

## Dependencies

- Uses: `engine/core` (ecs, events, serialization hash), platform interfaces as
  system parameters only. glm exempt (Rule 2).
- Used by: `games/daggerfall_n` systems, `engine/editor`, tests, later the LLM
  planner — all NPC control through the same `enqueue()` (Rule 15).
