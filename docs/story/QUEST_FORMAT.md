<!--
Created: 09:08:2026 - 14:03:29
Last updated: 09:08:2026 - 14:03:29
-->
<!--
UPD:
- 09:08:2026 - 14:03:29: Initial quest data format PROPOSAL: quest state-machine
  schema (states/transitions/conditions/effects), world-flag registry, journal
  and localization key conventions, dialogue graph nodes over sim's
  DialogueLine, settlement-craft template quest concept, LLM character card
  schema. Sent to sim for contract negotiation.
-->

# QUEST_FORMAT.md — Quest & Dialogue Data Format (PROPOSAL)

Status: **PROPOSAL — not a contract until sim ACKs** (Rule 26 discipline).
Owner: `story`. Runtime owner: `sim` (`engine/gameplay`). Grounded in the
feature-requests grill (в8, в11, в12), DECISIONS §5 (Q65–Q67, Q70–Q71) and
sim's shipped stage-1 headers (`Dialogue.h`, `NpcAction.h`, `Ids.h`, `ILlm.h`).

Design premises, fixed by prior decisions:

- Quests are **data-driven state machines**; progress lives in the SaveDelta;
  there is a **global world-flag registry** and a **journal component** (в11).
- Content in data files, never C++; every user-facing string is a
  **localization key** from the first line (Rules 5–6, Q59).
- Quests drive NPCs **only** through `NpcAction` (Rule 15, Q70).
- Quest dialog nodes are **fixed/authored**; the LLM voices only incidental
  NPCs within authored character cards, with mandatory scripted fallback
  (в12, Q66–Q67); the game is fully playable on null LLM (Q62, Rule 3).
- No external dialogue middleware (Ink/Yarn): Rule 24 (pinned deps only) and
  sim's existing `Dialogue.h` make a small JSON graph + runner the right
  build; we author a *graph*, not a language.

File format: **JSON** (core's serialization module lists JSON/TOML for
content; JSON nests graphs better). Proposed layout (path pending the lead's
`data/` vs `assets/` ruling — see specs/story.md):

```
games/daggerfall_n/data/quests/          # one file per quest: <quest_id>.quest.json
games/daggerfall_n/data/quests/templates/# craft-keyed quest templates
games/daggerfall_n/data/dialogs/         # dialogue graphs: <topic_id>.dlg.json
games/daggerfall_n/data/npcs/cards/      # LLM character cards: <npc_id>.card.json
games/daggerfall_n/data/world_flags.json # THE flag registry (single file)
```

---

## 1. Identifiers and localization keys

- All content ids are strings in data, hashed to the stable 64-bit content
  hash (core's FNV-1a, same mechanism as sim's `Ids.h` POD id wrappers) at
  load. Proposed new id types: `QuestId`, `FlagId`, `TopicId`, `NpcCardId`.
- Id convention: `snake_case`, dot-namespaced:
  `quest.main.act1_embers`, `flag.vaelmere.mill_dispute_resolved`,
  `dlg.vaelmere.elder_intro`.
- Localization keys mirror the owning id:
  `quest.main.act1_embers.name`, `quest.main.act1_embers.journal.010`,
  `dlg.vaelmere.elder_intro.n3.text`. A literal user-facing string anywhere
  in quest/dialog JSON is a validation error — only keys.

## 2. Quest state machine (`*.quest.json`)

```json
{
  "id": "quest.side.millers_debt",
  "category": "side",                  // main | side | template_instance
  "name_key": "quest.side.millers_debt.name",
  "vars": { "barrels_found": 0 },      // quest-local ints/bools, saved in delta
  "start": "s_offered",
  "states": {
    "s_offered": {
      "journal_key": "quest.side.millers_debt.journal.010",
      "on_enter": [ { "set_flag": "flag.vaelmere.miller_met", "value": true } ],
      "transitions": [
        { "to": "s_search", "when": [ { "dialog_exit": "dlg.vaelmere.miller.accept" } ] },
        { "to": "s_refused", "when": [ { "dialog_exit": "dlg.vaelmere.miller.refuse" } ] }
      ]
    },
    "s_search": {
      "journal_key": "quest.side.millers_debt.journal.020",
      "transitions": [
        { "to": "s_return",
          "when": [ { "var": "barrels_found", "op": ">=", "value": 3 } ] }
      ]
    },
    "s_return": { "...": "..." },
    "s_done":   { "terminal": "completed",
                  "journal_key": "quest.side.millers_debt.journal.100",
                  "on_enter": [ { "give_item": "item.septim_pouch", "count": 1 } ] },
    "s_refused": { "terminal": "failed" }
  }
}
```

Semantics (the contract sim implements):

- Exactly one active state per quest instance; `terminal` states end it
  (`completed | failed`); terminal is forever (journal keeps the record).
- **Transitions are evaluated on the fixed tick** against conditions;
  first matching transition wins (author order = priority). Deterministic
  under null LLM by construction (Rule 13.2) — conditions read only
  world/quest state, never wall-clock or LLM output.
- **Effects run on transition entry (`on_enter`)**, exactly once — never in a
  re-presentable dialog node (double-apply pitfall).
- `journal_key` on a state appends that entry to the journal component when
  the state is entered (append-only history, Bethesda-stage style numbering
  in the keys: 010/020/100 leaves gaps for insertions).
- Save contract (в11): per quest — active state id hash, `vars`, journal
  entries added; plus the global flag registry. Written through sim's
  SaveDelta section hooks; format internals are sim's.

### 2.1 Condition vocabulary (needs sim ACK — the "quest grill" its spec left open)

A `when` array is a conjunction (AND); an `any` wrapper gives OR. Each atom is
one of a **closed set** (extension = group sync, like `NpcAction`):

| Atom | Fields | Backed by |
|---|---|---|
| `flag` | flag id, `op` (`== != < <= > >=`), value | world-flag registry |
| `var` | quest-local var, op, value | quest instance |
| `quest_state` | quest id, state id (or `completed`/`failed`) | quest system |
| `dialog_exit` | dialog exit-point id (§4) | dialogue runner event |
| `has_item` | item id, op, count | inventory (sim, stage 2.5+) |
| `entered_location` | trigger volume / POI id | world trigger (needs sim+core: POI trigger volumes) |
| `npc_dead` | npc id | gameplay events |
| `clock` | day-phase id (`night`, ...) | day cycle (в2; FUTURE until it lands) |

This deliberately extends sim's minimal `DialogueLine` key/op/value
`conditions` with the same shape — one evaluator can serve both.

### 2.2 Effect vocabulary (closed set, same discipline)

`set_flag`, `set_var` (set/add), `give_item`/`take_item`, `start_quest`,
`journal_note` (extra entry without a state change), `npc_action` (enqueue a
typed `NpcAction` on a named NPC — the ONLY NPC hook, Rule 15),
`set_topic` (enable/disable a dialogue topic for an NPC, §4).
Explicitly absent: teleports, stat edits, spawns — if a quest needs one, that
is a group-sync request, not a new effect snuck into data.

## 3. World-flag registry (`world_flags.json`)

Single declarative file: every flag used by any quest MUST be declared here.

```json
{ "flags": [
  { "id": "flag.vaelmere.mill_dispute_resolved", "type": "bool", "default": false,
    "comment": "Set by quest.side.millers_debt s_done; read by elder dialogs." }
] }
```

- Types: `bool` | `int`. Defaults are the generated-world state; the SaveDelta
  stores only changed flags (matches core's delta philosophy, Q56).
- Undeclared flag referenced anywhere → validation error. The registry is the
  cross-quest coupling surface, so it stays reviewable in one file.

## 4. Dialogue graphs (`*.dlg.json`) — authored nodes over sim's `DialogueLine`

Sim's `Dialogue.h` gives lines (id + conditions + voiced segments, Q71) but no
tree. The graph layer references lines by id; the speaker binds at runtime
(lines never know their speaker — Q71 requirement preserved).

```json
{
  "id": "dlg.vaelmere.miller",
  "start": "n_greet",
  "nodes": {
    "n_greet": { "line": "dlg.vaelmere.miller.greet",
                 "choices": [
                   { "text_key": "dlg.vaelmere.miller.c_help", "to": "n_job",
                     "if": [ { "flag": "flag.vaelmere.miller_met", "op": "==", "value": false } ] },
                   { "text_key": "dlg.common.farewell", "to": "x_bye" }
                 ] },
    "n_job":  { "line": "dlg.vaelmere.miller.job", "next": "x_accept_or..." },
    "x_bye":    { "exit": true },
    "x_accept": { "exit": true }
  }
}
```

- Node kinds: **line** (a `DialogueLineId`, auto-advance via `next`),
  **choice list** (player options: `text_key`, optional `if`, optional
  effects applied on the transition), **exit** points (named — quests listen
  for them via the `dialog_exit` condition atom; the dialogue runner emits an
  event, quests never reach into dialogue internals).
- Validation: every node terminates or branches (no stalls); every choice
  target exists; every `line` id exists in the line pool; conditions use §2.1
  atoms only.
- Topics: an NPC's dialogue is a set of topic graphs gated by `set_topic`
  effects + conditions — this is how quest progress opens/closes speech
  without editing NPC data at runtime.

## 5. Settlement-craft template quests (в8)

Concept (schema detail lands after the pitch + Vaelmere's craft, в17):

- Each settlement declares a **craft** (fishing, logging, smithing, ...) in
  its settlement data (design/core zone; we consume the id).
- A **template** is a normal quest state machine with **typed slots** instead
  of concrete ids: `@giver` (role: e.g. `role.craftmaster`), `@target_poi`
  (POI class: e.g. `dungeon_entrance`, picked from the POI graph — never raw
  coordinates, per LANDSCAPE.md's derived-only rule), `@item`
  (craft-keyed loot/goods table), `@antagonist` (encounter class), plus
  slot constraints (distance band in POI-graph hops, "not used by an active
  instance", cooldowns).
- Templates are **keyed to crafts**: `templates/fishing/*.quest.json` etc.,
  plus a small craft-neutral set. The craft binds the *fiction* (a fishing
  village asks about nets, drowned kin, the lake cave), so instances read
  local, not Radiant-generic.
- **Instantiation resolves slots to concrete ids and emits a standard quest
  instance** — the runner has ONE runtime path; generated and hand-authored
  quests are indistinguishable to sim, saves, and the journal. Instantiator
  code is sim's (or a shared worldgen-style offline tool — to negotiate);
  the template schema, slot vocabulary and per-craft fiction are story's.
- Journal/dialog text in templates uses localization keys with slot
  interpolation (`{giver_name}`, `{target_poi_name}`) — names are themselves
  localized strings, никогда raw English in data.

## 6. LLM character cards (`*.card.json`) — в12, Q65–Q67

The card is the authored cage for incidental-NPC chatter. Script picks the
intent; the card shapes the words; fallback text ships always (null LLM =
full game, Q62).

```json
{
  "id": "npc.vaelmere.old_ferryman",
  "name_key": "npc.vaelmere.old_ferryman.name",
  "voice": "voice.male_old_2",
  "role": "role.ferryman",
  "home": "poi.vaelmere",
  "persona": {
    "traits": [ "taciturn", "superstitious", "kind to children" ],
    "speech": "short sentences; river metaphors; never curses",
    "knowledge": [ "the lake and its moods", "vaelmere gossip",
                   "rumor: lights in the crag tower at night" ],
    "ignorance": [ "anything beyond the valley", "quest solutions",
                   "the true nature of the dark force" ],
    "taboos": [ "never promises items or rewards",
                "never contradicts journal facts",
                "never speaks for named quest NPCs" ]
  },
  "mood_flags": [ { "if": [ { "flag": "flag.act1.darkness_seen", "op": "==", "value": true } ],
                    "then": "frightened, speaks of leaving" } ],
  "fallback_lines": [ "dlg.incidental.ferryman.f1",
                      "dlg.incidental.ferryman.f2" ]
}
```

- `persona` fields are **prompt fragments** (English; the prompt language vs
  output language is sim/lead's localization call — flagged open).
- `knowledge`/`ignorance`/`taboos` are the containment contract: cards may
  add flavor and rumor, never quest state, items, or canon contradictions.
  Rumors are story-authored strings pointing at real content (the tower
  lights) — the LLM embellishes, never invents targets.
- `mood_flags` reuse the §2.1 condition atoms — world state tints the chatter.
- `fallback_lines` are mandatory (≥ 1): they map to real `DialogueLine`s with
  voiced segments and become `CompletionRequest.fallback_text` (Q67).
- Named quest NPCs get cards too (for their *idle* chatter only) — their
  quest dialog is always the fixed graph of §4.

## 7. Validation (story-side tooling need)

A schema/reference checker must exist before the first real quest lands
(negotiate owner: `tools/` is the lead's zone):

1. JSON schema conformance for all four file kinds.
2. Every referenced flag declared; every id referenced exists; every
   localization key present in both languages; zero literal user-facing text.
3. Graph checks: unreachable states/nodes, stall nodes, terminal
   reachability, effect-on-revisitable-node lint.
4. Determinism lint: no condition atom outside the closed set.

## 8. Open questions for sim (the ask)

1. ACK/counter the **condition atoms** (§2.1) and **effect set** (§2.2) —
   especially `dialog_exit` eventing and `entered_location` (needs a trigger
   volume or POI-radius mechanism: sim+core).
2. ACK the **one-runtime-path** rule for template instances, and who owns the
   instantiator (sim runtime vs offline tool).
3. ACK the **dialogue graph layer** (§4) as the structure above
   `Dialogue.h` lines — replaces the "minimal until the quest grill"
   conditions placeholder with §2.1 atoms.
4. Card → `CompletionRequest` assembly (§6): field-to-prompt mapping and
   prompt language are sim's to specify; story guarantees the containment
   fields and fallbacks.
5. Journal component shape (в11): story needs append-only entries of
   (quest id, journal key, sim_tick) — anything more?

Nothing here requires new `NpcAction` variants for act 1 as drafted; if a
chosen pitch does, that goes through the group sync (Rule 15/26).
