<!--
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 22:31:38
-->
<!--
UPD:
- 09:08:2026 - 00:18:26: Stage-1 state: public contract headers only, no
  implementation yet.
- 09:08:2026 - 01:02:15: Stage 2 — implemented player movement
  (PlayerMovement.h/.cpp + World wrappers) and the dice RNG (Dice.cpp);
  NpcAction/stats/dialogue remain headers-only by stage scope.
- 09:08:2026 - 14:12:02: Quest-grill sync — new Condition.h (closed
  ConditionAtom vocabulary shared by dialogue + quests, QUEST_FORMAT.md §2.1);
  Dialogue.h conditions swapped to it; Ids.h gained QuestId/FlagId/TopicId/
  NpcCardId. Segment hashing untouched.
- 09:08:2026 - 22:31:38: Interaction stage part 2 — visible hands (ViewModel),
  prop collision (buildings + boulders from the drawn triangles), inventory
  screen state, player action latches, and jump/crouch/swim.
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
- `sources/Ids.h` — typed stable content ids (incl. QuestId/FlagId/TopicId/
  NpcCardId from the quest-grill sync).
- `sources/Condition.h` — closed ConditionAtom/ConditionGroup vocabulary, one
  evaluator for dialogue lines, graph choices and quest transitions
  (QUEST_FORMAT.md §2.1; semantics record in the header notes).
- `sources/ViewModel.h` (implemented) — the VISIBLE first-person hand and the
  item in it. The hand is an ordinary world-space entity parked at the eye
  anchor each tick, so render's existing ECS pass draws it: no viewmodel pass,
  no IRenderer change. Reuses the torch hand anchor (TORCH_HAND_OFFSET_RIGHT /
  _BELOW_EYE / HAND_OFFSET_FORWARD), so flame and wood cannot drift apart.
- `sources/PropCollision.h` (implemented) — buildings and boulders become
  SOLID, with their bodies built from the same triangles render draws
  (`build_site_mesh` / `build_scatter_mesh`). One merged static body per
  resident chunk, created and destroyed with the chunk. A doorway modelled in
  render becomes walkable here with no change. Tree trunks deliberately absent
  until render exposes a per-instance trunk radius (see the header).
- `sources/InventoryScreen.h` (implemented) — the Skyrim-style screen state the
  user specified: a list of item NAME KEYS plus a rotatable preview angle pair.
  Selection is tracked by item id across refreshes, not by row.
- `sources/PlayerActions.h` (implemented) — one per-tick call that consumes the
  player's latched action keys (E interact, F light, I inventory).
- `sources/PlayerMovement.h` (implemented, stage 2) — `PlayerState` component;
  ref-based core `accumulate_input`/`player_pre_step`/`player_post_step`
  (unit-testable without a World) plus World-facing `spawn_player` +
  per-tick wrappers for engine/app. Tick order: accumulate (per render frame)
  -> pre_step -> app calls `IPhysics::step(SIM_DT)` -> post_step; pre_step
  snapshots curr->prev on Transform/CameraPose pairs (Rule 12 contract).
  Implemented: `Dice.cpp` (splitmix64; `resolve_attack` waits for the combat
  grill).
  JUMP / CROUCH / SWIM (v1, user-approved) live here too. Jump takeoff speed is
  DERIVED from `JUMP_HEIGHT`; crouch resizes the CAPSULE (a camera-only crouch
  is a lie in a voxel world with real ceilings) and stand-up is refused while
  something is overhead; swimming uses two thresholds
  (`SWIM_ENTER_DEPTH` / `SWIM_EXIT_DEPTH`) because one threshold flickers on a
  shelving shore. Water depth arrives as a PARAMETER — the authoritative source
  is `world::ChunkManager::water_surface_at`, never the drawn water primitives,
  which over-cover (core's ruling).

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
