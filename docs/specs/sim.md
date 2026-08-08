<!--
Created: 09:08:2026 - 00:25:29
Last updated: 09:08:2026 - 00:25:29
-->
<!--
UPD:
- 09:08:2026 - 00:25:29: Initial stage-1 spec: contracts for platform
  physics/anim/audio/llm and the gameplay public API; boundary agreements with
  core, render and the lead recorded.
-->

# Spec — sim (`docs/specs/sim.md`)

Owner: agent **sim**. Written per Q35 / `rules/documentation.md` (seven sections).
Stage-1 status: all public headers below exist; zero implementation (per stage-1
contract-freeze discipline, Rule 26).

## Zone of responsibility

Per Rule 25:

- `engine/platform/physics` — `IPhysics` + (later) `sources/{jolt,null}`
- `engine/platform/anim` — `IAnim` + (later) `sources/{ozz,null}`
- `engine/platform/audio` — `IAudio` + (later) `sources/{miniaudio,null}`
- `engine/platform/llm` — `ILlm` + (later) `sources/{llama,null}`
- `engine/physics` — character controller, collision layer semantics (via `IPhysics`)
- `engine/anim` — animation state machines, humanoid rig contract (via `IAnim`)
- `engine/gameplay` — NPC control API (`NpcAction`, the flagship), stats/skills,
  dice combat, dialogue data model, interaction components; later inventory,
  quests, schedules.

In one sentence: everything that makes the simulated world *behave* — bodies
collide, characters move, skeletons pose, sounds play, NPCs act, dice roll,
lines get spoken — behind swappable platform backends (Rules 1, 3, 4).

## Public interface

### Platform contracts (namespace `dfn::platform`, header-only, std+glm only)

**`engine/platform/physics/interfaces/IPhysics.h`**
- `step(dt)` — exactly one fixed step per call, `dt == SIM_DT` (Rule 12).
- Static collision: `create_terrain(TerrainDesc)` (heightmap samples as plain
  floats, row-major `[z][x]`, one body per chunk), `create_static_box(...)`
  (buildings, dungeon prefab pieces), `destroy_body`.
- Kinematic character (capsule): `create_character(CharacterDesc)` (bottom-point
  position, radius/height/max-slope/step-height from NUMBERS constants),
  `move_character(handle, displacement)` — records intent, resolved with
  collide-and-slide during the next `step()`; `character_position` /
  `character_grounded` post-step queries; `teleport_character` for spawns.
- `raycast(origin, unit_dir, max_distance, mask)` → `RayHit` with `user_data`
  (EntityId bits) so hits resolve to entities.
- `CollisionMask` is opaque bits here; bit semantics are defined in
  `engine/physics` (stage 2).

**`engine/platform/anim/interfaces/IAnim.h`**
- Assets in the backend's runtime format (ozz: `.ozz` files built offline):
  `load_skeleton`, `load_clip(skeleton, path)`, `joint_count`, `clip_duration`.
- Per-character `create_instance(skeleton)`.
- `evaluate(instance, span<AnimLayer>, span<glm::mat4> out)` — samples every
  `(clip, time, weight)` layer, blends (backend-normalized weights), writes
  **skinning matrices**: column-major, model-space joint × inverse bind pose,
  ordered by joint index, size ≥ `joint_count`. Plain matrix spans — no
  renderer or bgfx dependency by construction; conventions ACKed by render.

**`engine/platform/audio/interfaces/IAudio.h`**
- `load_sound`, `play(sound, PlayParams)` (bus, volume, pitch, loop, optional
  `Spatial3d` position + min/max distance), voice control, `update(ListenerPose)`.
- Q68 hooks: `play_variation(span<SoundHandle> takes, params)` — backend picks a
  take with rotation + pitch jitter (footsteps per surface material; gameplay
  supplies the takes); `set_bus_reverb(bus, ReverbParams{decay, room_size, wet})`
  — reverb by room volume as a bus property.
- Buses: `create_bus(parent)` tree under implicit master; the fixed set
  (sfx/music/voice/ambient) is created by app — names are content, not contract.
- Adaptive music: `play_music(span<layers>, bus)` starts layers
  sample-synchronized; `set_music_layer(music, i, volume, fade)`; `stop_music`.

**`engine/platform/llm/interfaces/ILlm.h`**
- Async-only by contract: `submit(CompletionRequest)` → handle, never blocks;
  `status(handle)`; `try_get_result(handle, out)` non-blocking; `cancel`.
  No callback API on purpose — results are polled from the fixed tick, which
  keeps the consumption point deterministic and thread-trivial (Rule 12/13).
- `CompletionRequest` **carries mandatory scripted `fallback_text`** (Q67:
  intent from script, words from LLM) plus max_tokens/temperature/seed/stops.
- `set_inference_allowed(bool)` — the Q64 gate; app flips it false on combat and
  chunk-load states; queued requests wait, in-flight generation may finish.
- `init(LlmInitParams)` auto-selects the model by available VRAM against
  `LLM_VRAM_BUDGET` (primary ≤ `LLM_MAX_PARAMS` quantized, else
  `LLM_FALLBACK_MODEL`); `active_model()` reports. `init` failure ≡ null mode.

### Gameplay contracts (namespace `dfn::gameplay`, `engine/gameplay/sources/`)

**`NpcAction.h` — THE flagship (Q70, Rule 15).**
- `NpcAction = std::variant<MoveTo, Face, Say, GiveItem, Attack, Wait,
  SetSchedule>` — closed set; extension is a group-sync event.
- `NpcActionQueue` component: `vector<QueuedNpcAction{sequence, action}>`,
  front = active, plus executor progress fields. Plain data (Rule 8).
- `enqueue(queue, action) -> sequence` and `clear_queue(queue)` are **the only
  mutation entry points**. Scripts, tests, editor, LLM — same values, same
  channel. No other way to drive an NPC exists (Rule 15).
- `execute_npc_actions(world, physics, events, sim_tick)` — the executor
  system (Rule 9), once per fixed tick.
- Journal-friendly (Rule 13.3): `NpcActionRecord{sim_tick, npc, sequence,
  action}` fully replays an enqueue; `NpcActionCompleted/Failed` events carry
  the same sequence.
- `Say` covers both worlds: scripted `line` id always names the intent;
  optional `llm_text` carries LLM words (Q67).

**`Stats.h`** — `Attribute` (the 8 Daggerfall attributes), `Skill` (~12,
placeholder names until the combat grill; `COUNT` sentinels sized to NUMBERS,
static_asserts bind them in stage 2), `Attributes`/`Skills` components,
`value()` accessors, `record_use()` use-based progression hook (Q42),
`SkillRaised` event.

**`Dice.h`** — `Rng{uint64 state}` seedable/serializable (Rule 13.2),
`make_rng`, `roll_die`, `roll_dice` (XdY), `percent_check`;
`resolve_attack(AttackInput, Rng&) -> AttackOutcome{hit, critical, damage,
hit_roll}` — a pure function; formulas land at the combat grill via NUMBERS.

**`Dialogue.h`** (Q71/Q74/Q79/Q80) — `DialogueLine{id, conditions, segments}`;
`DialogueSegment{text, tone, volume, speed, tag}`; lines never know their
speaker; markup is one vocabulary across languages keyed by line id + segment
index; `segment_content_hash(segment, voice)` — FNV-1a 64 over length-prefixed
(text, tone, volume, speed, tag, voice) in that frozen order; the 16-hex-digit
digest + `.opus` names the audio file; only changed segments re-synthesize;
segment joins are crossfaded (Q79 seam note).

**`NpcComponents.h`** — `VoiceTimbre{VoiceId}` (Q80: the NPC brings the voice),
`ScheduleState{schedule, phase, phase_elapsed}` (Q44).

**`Interaction.h`** (Q11) — `Highlightable{prompt_key (localization key),
max_use_distance}`, `Openable{open, locked, lock_level}`,
`Lootable{loot_table, looted}`.

**`Ids.h`** — `DialogueLineId, VoiceId, ItemId, ScheduleId, LootTableId`:
distinct POD wrappers over the stable 64-bit content hash.

### Null backends (Rule 3 — runnable modes, not stubs)

- **null physics** (Q31): `step` no-op; `move_character` applies the horizontal
  displacement fully, ignores vertical; `grounded` always true; raycasts miss;
  bodies valid-but-inert. Deterministic → gameplay/NPC tests run on it.
- **null anim**: loads succeed, `joint_count` 0, `evaluate` fills identities —
  bind-pose characters, headless tours never crash.
- **null audio**: silent success everywhere, `is_playing` false. Playable with
  no audio device.
- **null llm** (Q62 — THE default mode): `submit` succeeds, status `Done`
  immediately, result = `fallback_text`, `from_fallback = true`, instant and
  deterministic (Rule 13.2). The game is **fully playable** on null LLM: every
  `Say`/dialogue intent always has scripted words by construction, because
  `fallback_text` is a mandatory request field — there is no code path that
  requires generated text.

## Internal design

Stage 2+ (nothing implemented yet; recorded so a newcomer can continue):

- **`engine/physics`**: `CharacterController` wraps a `CharacterHandle`; per
  fixed tick: snapshot `Transform/CameraPose` curr→prev, integrate desired
  velocity (input intents or NpcAction movement; gravity; `WALK_SPEED`/
  `RUN_SPEED`), call `move_character`, after `step()` read back position and
  write `Transform` + `CameraPose` (eye = capsule bottom + `PLAYER_EYE_HEIGHT`).
  `CollisionLayers.h` defines the `CollisionMask` bits (static, character,
  interactable, ...). Terrain bodies: one per loaded chunk from core's
  `HeightFieldView` (uint16 → float conversion here), created/destroyed on
  ChunkLoaded/ChunkUnloaded events.
- **`engine/anim`**: `AnimStateMachine` (locomotion states first: idle/walk/run)
  maps state + blend weights to `AnimLayer` spans; humanoid rig contract (joint
  naming per Quaternius rig, Q15) documented in `engine/anim/docs/`; palettes
  evaluated at fixed tick into a component-owned buffer for render (stage 3).
- **`engine/gameplay` executor**: one dispatch per active action type; MoveTo
  steers via the controller (straight-line stage 2, pathfinding later), Say
  hands segments to the dialogue system (audio via `IAudio`, per-segment files
  by content hash, crossfade at joins), Attack snapshots stats → `resolve_attack`,
  Wait counts `active_elapsed`. Completion/failure pops and publishes events.
  Schedule system: reads `ScheduleState` + content schedule, enqueues actions —
  never mutates NPCs directly (Rule 15 applies to internal systems too).
- **LLM flow** (stage 4): script picks intent (a `DialogueLineId` + prompt
  context), submits with the line's text as fallback, polls on later ticks;
  arrived text becomes `Say{line, llm_text}`; Piper synthesizes at runtime for
  LLM words, offline Opus files serve scripted ones.
- **Threading**: all sim-side code is single-threaded at the fixed tick; the
  only worker thread in my zone lives inside the llama backend, invisible
  behind the async `ILlm` contract.

### FetchContent plan (Rule 24 — pinned tags, agents install nothing)

Declared in my zones' `CMakeLists.txt` (per-layer ownership, Q34), fetched only
by backend targets — interface targets stay header-only (Rule 1):

| Library | Pin | Used by | Notes |
|---|---|---|---|
| JoltPhysics | tag `v5.2.0` | `platform/physics/sources/jolt` | static lib; RTTI/exceptions settings matched to ours |
| ozz-animation | tag `0.16.0` | `platform/anim/sources/ozz` | runtime libs only (`ozz_base`, `ozz_animation`); offline tools built host-side for the asset pipeline |
| miniaudio | tag `0.11.22` | `platform/audio/sources/miniaudio` | single header; implementation TU inside the backend only |
| llama.cpp | tag `b5013` | `platform/llm/sources/llama` | Metal on macOS, Vulkan/CUDA on Windows; ggml backends trimmed to what we ship |

Pins are verified at the first stage-2 configure on a clean clone; if a pinned
tag proves unusable (build breakage on our toolchains), the replacement pin is
approved by the lead and recorded here — never floated to a branch head.

## Dependencies

Boundary agreements (Rule 26), all reached during stage 1 and recorded verbatim
with the owning agent:

**With core (agreed):**
- ECS surface `dfn::ecs`: `EntityId` (generational, by value in my components,
  Rule 8) at `engine/core/ecs/sources/EntityId.h`; `World` spawn/destroy +
  batch ops + `GroupId` chunk grouping; `view<T...>`; resources. Components
  holding `std::vector`/`std::string` are supported (pool moves preserve them).
  I need no exclusion filters or sort guarantees for stage 2 — frozen without.
- NPCs are **chunk-resident**: spawned/destroyed in batches by ChunkManager
  (Rule 11). Consequences: queued `NpcAction`s are dropped on unload (schedules
  re-derive intent); save-delta NPC state = position/orientation, ScheduleState,
  stats + use counters, inventory, interaction flags — never the queue.
- Save-delta hooks: `dfn::world::SaveDeltaCodec::register_section(SaveSectionHooks{tag,
  version, write, read})` at `engine/world/sources/SaveDelta.h`; gameplay
  registers its sections at startup; container format, skip-unknown and
  container-level migration are core's (Rule 7).
- Heightmap access: `dfn::math::HeightFieldView` at
  `engine/core/math/sources/HeightField.h` — raw `uint16` + per-chunk
  scale/offset, `resolution` 129, `step` 2.0 m, row-major `[z*res+x]`,
  `origin` is x,z (vec2); **I convert** to float meters
  (`height_offset + raw * height_scale`) and build the `TerrainDesc` vec3
  origin as `(origin.x, height_offset, origin.y)`. Obtained per chunk from
  `world::ChunkManager`; lifetime until after ChunkUnloaded dispatch.
- Stable content hash: `dfn::serialization` FNV-1a 64 (`fnv1a64`, streaming
  `Fnv1a64` with `update_length_prefixed`) at
  `engine/core/serialization/sources/ContentHash.h`; offset basis
  14695981039346656037, prime 1099511628211, seedless, frozen forever;
  `segment_content_hash` is built on it with length-prefixed fields.
- EventBus (`dfn::events`, post + pump) for my events; `BinaryReader/Writer`
  for my save sections.

**With render (agreed):**
- Interpolation contract (Rule 12): I write, at fixed tick only, the shared
  pairs `Transform`/`PreviousTransform` and `CameraPose`/`PreviousCameraPose`
  (snapshot curr→prev, then integrate). Render reads (prev, curr, alpha),
  lerps position, shortest-arc yaw, clamped pitch, builds view/proj. Sim never
  sees alpha; render never writes controller state.
- Conventions mirrored from render: meters/radians (Rule 14), right-handed
  Y-up, +X east, +Z south, yaw 0 = −Z, positive yaw = clockwise from above,
  positive pitch = look up.
- Skinning (stage 3, informational): palettes as plain `span<glm::mat4>`,
  column-major, model-space joint × inverse bind, joint-index order, size =
  `joint_count`, evaluated at fixed tick; null anim = identities. The
  `IRenderer` skinned-mesh submit arrives via contract sync, not here.
- Flagged stage-3 topic (render cares): sub-tick mouse latency via a
  render-side view-only yaw/pitch offset, zeroed each tick. Not in stage 2.

**With the lead (agreed):**
- Shared components live in lead-owned `engine/core/components/sources/Components.h`:
  `Transform`, `PreviousTransform`, `CameraPose`, `PreviousCameraPose` — ACKed
  as authored; eye point derived in my controller, no extra fields.
- `HoverTarget{EntityId}` World resource: gameplay-written (IPhysics raycast),
  render-read; type authored by the lead (pending only core's EntityId header
  landing — not blocking).
- NUMBERS additions accepted: `PLAYER_CAPSULE_RADIUS` 0.35 m,
  `PLAYER_CAPSULE_HEIGHT` 1.8 m, `PLAYER_EYE_HEIGHT` 1.7 m,
  `PLAYER_STEP_HEIGHT` 0.35 m, `PLAYER_MAX_SLOPE` 0.87 rad — provisional,
  movement grill will tune.

**Open questions for the group sync:**
1. NUMBERS names referenced by my headers but not yet listed:
   `NPC_ARRIVE_RADIUS` (MoveTo default) and `INTERACT_DISTANCE` (Highlightable
   default) — lead to add with values.
2. `Skill` enum final list (~12) and combat formulas — combat grill; contract
   shapes frozen regardless.
3. llama.cpp pin `b5013` to be validated on both toolchains at stage-2
   configure (heaviest dependency; fallback pin decision is the lead's).
4. Dialogue `conditions` schema is deliberately minimal (key/op/value) until
   the quest grill.

## Step-by-step plan

1. **Stage 1 (this deliverable)** — spec + public headers above; boundary
   agreements recorded; no `.cpp`.
2. **Stage 2 (skeleton walk, Q37/Q51)** — in order:
   a. `platform/physics/sources/null` then `sources/jolt` (FetchContent pin);
   b. `engine/physics`: collision layers, terrain-from-HeightFieldView,
      `CharacterController` writing Transform/CameraPose pairs;
   c. doctest: controller on a flat chunk (jolt vs null parity where defined);
      support the four-screenshot tour criterion with render/lead.
3. **Stage 2.5 (gameplay core loop)** — `enqueue`/`clear_queue`/executor for
   MoveTo/Face/Wait on null physics; Stats + `record_use`; Dice + splitmix64;
   seed-replay tests (Rule 13.2); save sections registered via SaveDeltaCodec.
4. **Stage 3 (bodies and voices)** — `platform/anim` null + ozz backends;
   `engine/anim` locomotion state machine; skinning palette handoff sync with
   render; `platform/audio` null + miniaudio; footsteps-by-material and
   room-reverb wiring (Q68); Say with offline Opus segments, content-hash
   naming, crossfade joins; Attack via `resolve_attack` (post combat grill).
5. **Stage 4 (LLM)** — `platform/llm` null first (already the shipping mode),
   then llama backend: worker thread, VRAM model selection, Q64 gating wired to
   app states; Say with `llm_text`; decision journaling hooks (Rule 13.3).
6. Each stage ends with docs/README updates (Rule 18) and, where the picture
   changes, a tour run (Rule 27).

## How it is verified

- **Stage 1**: `python3 tools/header_check.py --all` passes; headers follow
  IRenderer's adapter style; lead cross-checks contracts at the group sync.
- **Determinism tests** (Rule 13.2): fixed seed + scripted NpcAction sequence
  on null physics/LLM ⇒ byte-identical world state hash after N ticks; dice
  distribution + replay tests with fixed seeds.
- **Rule 15 enforcement**: a test drives an NPC through every action type using
  only `enqueue()`; grep-level review that no system outside the executor
  writes NPC Transform/stats; journal replay test: recorded `NpcActionRecord`
  stream reproduces the same end state.
- **Null-mode runs** (Rule 3): the doctest suite runs everything with all-null
  backends headless; a feature that crashes under null is a bug by definition.
- **Physics**: controller unit tests (slide along wall, step-up ≤
  `PLAYER_STEP_HEIGHT`, slope reject > `PLAYER_MAX_SLOPE`, grounded
  transitions) on generated terrain fixtures; jolt backend additionally passes
  the stage-2 visual tour (Rule 27) — capsule stands on the flat chunk.
- **Dialogue/audio**: hash stability golden test (known segment + voice ⇒ known
  digest — guards the frozen algorithm); voiced-file lookup falls back to
  text-only display when the file is missing (Q75 resync path).
- **LLM**: null backend returns fallback on first poll (test); llama backend
  soak test asserts zero sim-tick blocking (submit/poll timing) and gate
  compliance (no new inference while disallowed).
- Anything visual (skinned characters, highlights) goes through the screenshot
  tour with per-frame checklists (Rule 27) — never "should work" prose.

## What this zone does NOT do

- **No rendering**: no bgfx, no IRenderer calls, no draw submission; anim
  outputs plain matrix spans, render consumes them (their zone).
- **No window/input**: GLFW is render's; I consume input as intents/components.
- **No world generation or chunk streaming**: core owns `engine/world`; I react
  to ChunkLoaded/Unloaded events and read HeightFieldView.
- **No ECS internals, no shared components**: core owns the ECS; the lead owns
  `engine/core/components` — I propose, never author, cross-zone types.
- **No save container format**: I write sections through core's SaveDeltaCodec;
  magic/versioning/skip-unknown are core's (Rule 7).
- **No content in C++** (Rules 5/6): items, schedules, dialogue lines, loot
  tables, prompts are data files; user-facing strings only as localization keys.
- **No hardcoded constants** (Rule 14): every gameplay number comes from the
  NUMBERS-generated header.
- **No second NPC control path** (Rule 15): not even for tests, the editor, or
  "just this once" — new capabilities are new `NpcAction` alternatives via sync.
- **No synchronous LLM inference, ever**; no TTS in the engine — offline
  synthesis lives in `tools/voice_gen/` (lead zone), runtime Piper arrives only
  behind a platform contract if/when scheduled.
- **No physics sandbox** (Q11): interactions are explicit, marked objects.
