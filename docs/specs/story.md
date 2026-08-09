<!--
Created: 09:08:2026 - 14:01:56
Last updated: 09:08:2026 - 14:19:25
-->
<!--
UPD:
- 09:08:2026 - 14:01:56: Initial story-zone spec: mission, zone, contracts with
  sim/design/lead, team structure, deliverable plan (research, three pitches,
  quest data format proposal), verification, non-goals.
- 09:08:2026 - 14:19:25: Stage deliverables done: RESEARCH.md (N1–N58),
  PITCHES.md (three pitches + Russian summaries, awaiting в16 choice),
  QUEST_FORMAT.md contract-frozen with sim (Condition.h/Dialogue.h/Ids.h
  landed). Plan step 2 marked DONE; content gate holds.
-->

# Spec — story (`docs/specs/story.md`)

Owner: agent **story** (lead narrative designer). Written per Q35 /
`rules/documentation.md` (seven sections). Founding brief:
`docs/grill_sessions/feature-requests-1.md` (в7–в12) — read it before touching
anything in this zone; the user's answers there are creative law.

Written so a successor agent can continue from this file alone: agent sessions
die on reboots; this spec plus the docs it lists are the durable knowledge.

## Zone of responsibility

Per Rule 25 (AGENTS.md team table):

- `docs/story/` — narrative bible, main-arc pitches, characters, quest and
  dialog authoring docs, research. Created this stage.
- `games/daggerfall_n/data/quests/` — quest DATA files only (JSON/TOML per
  Rules 5–6). **Path note:** AGENTS.md says `data/quests`; the ARCHITECTURE.md
  repo layout puts JSON/TOML content under `games/daggerfall_n/assets/`.
  Flagged to the lead; until ruled, no data files exist (nothing is blocked —
  content authoring is gated on the user's pitch choice anyway).
- Quest RUNTIME code belongs to **sim** (`engine/gameplay`). Story authors
  data and contracts; sim executes. Format changes are negotiated with sim by
  message, never by editing `engine/` (foreign zone).

In one sentence: everything the game *says and means* — the world's story, the
main quest, side quests, settlement quest templates, character cards, dialog
trees — authored as docs and data files that sim's runtime consumes.

Creative law (в7–в12, DECISIONS-bound):

- **Doom-driven hero** in the spirit of Skyrim: a person forced to become the
  hero of one particular epoch — unimportant before, unimportant after, but
  here and now songs and legends will be sung of him. Sword + shield, casts
  magic. Monsters, caves, treasures, mysteries, royal intrigues on his path.
- **Two-front conflict:** a mystical force that will cover the world in
  darkness AND ordinary human strife the hero must resolve, gathering allies
  for the great battle — otherwise everyone slaughters each other over
  "power" and perishes from the affliction.
- Source DNA: The Lord of the Rings, The Hobbit, Skyrim, Game of Thrones.
  Tone: **dark**.
- **Hybrid quest structure (в8):** main quest + a pack of secondaries are
  hand-authored; small per-settlement quests are template/hybrid, keyed to
  each settlement's craft.
- **Dialogs (в12):** quest dialog nodes are fixed/authored; the LLM voices
  only incidental townsfolk and road encounters, inside character cards this
  zone authors (Q65–Q67: script sets intent, LLM gives words, mandatory
  scripted fallback).
- **Quest state (в11):** data-driven state machines + world-flag registry +
  journal; sim implements, story defines the content format needs.

## Public interface

What other zones and the user consume from this zone:

| Artifact | Consumer | Status |
|---|---|---|
| `docs/story/PITCHES.md` — three distinct main-arc pitches + Russian summaries | user (picks one) | this stage |
| `docs/story/RESEARCH.md` — actionable narrative-design rules (cited as N1, N2, ...) | story team, lead | this stage |
| `docs/story/QUEST_FORMAT.md` — quest state-machine schema, settlement-craft template concept, character card schema | sim (contract negotiation), lead | this stage, PROPOSAL until sim ACKs |
| `docs/story/BIBLE.md` — the narrative bible (world truth, factions, timeline, naming) | all zones | after pitch choice |
| Quest/dialog/character-card data files | sim runtime | after pitch choice + format ACK |
| Localization keys for every user-facing string (Rules 5–6, Q59) | lead (localization files), render (UI) | with first data files |

Contract style: like sim's platform interfaces, everything here is
consumer-facing and frozen per stage once ACKed (Rule 26). The quest format is
a *proposal* until sim agrees; the pitches are *options* until the user picks.

## Internal design

Team (user-approved structure, feature-requests grill в9–в10): story lead +
2–3 task subagents spawned per need:

1. **research** — web research distilled into actionable rules (RESEARCH.md).
2. **plot drafting** — raw material generation (dark-force concepts, faction
   triads, hooks); the lead synthesizes, never ships raw output.
3. **documentation / data formats** — schema drafting against sim's real
   headers, once volume demands it.

Working method:

- Pitches are written *into the built world*: LANDSCAPE.md §7 is the act-1
  stage (Vaelmere hamlet, Ravenscar Crag + tower ruin, barrow / forest ruin /
  lakeshore cave dungeons, shrine knoll). A pitch that ignores the terrain is
  invalid.
- The quest format is designed *against sim's shipped contracts* (see
  Dependencies), not in a vacuum: quest conditions must be expressible in the
  world-flag registry; dialog lines must fit `DialogueLine{id, conditions,
  segments}`; every NPC behavior a quest needs must be an existing `NpcAction`
  or a group-sync request for a new one (Rule 15).
- Nothing user-facing in C++, ever (Rules 5–6): all text as localization keys
  from the first data file.

## Dependencies

**On sim (negotiated by message):**
- `engine/gameplay/sources/Dialogue.h` — `DialogueLine{id, conditions,
  segments}`, segments carry (text, tone, volume, speed, tag) per Q71;
  lines never know their speaker. Sim's spec marks the `conditions` schema
  "deliberately minimal (key/op/value) until the quest grill" — QUEST_FORMAT.md
  is exactly that negotiation.
- `NpcAction` (Rule 15, Q70) — the ONLY channel quests may drive NPCs
  through; `Say{line, llm_text}` covers scripted + LLM speech.
- `ILlm` `CompletionRequest.fallback_text` mandatory (Q67) — every character
  card must provide scripted fallback lines; the game is fully playable on
  null LLM (Rule 3, Q62).
- Future: quest runner, journal component, world-flag registry, SaveDelta
  quest sections (в11) — sim implements against the ACKed format.

**On design:** `docs/design/LANDSCAPE.md` — place names, POI graph, the
derived-only rule for water-adjacent placements (quests must not table
coordinates that hydrology can move; reference POIs/stamps, not raw coords).

**On the lead:** pitch selection relayed from the user; localization file
conventions; the `data/quests` vs `assets/` path ruling; any NUMBERS constants
narrative needs (e.g. journal limits) — proposed, never invented in data.

**On the user:** the pitch choice (grill в16, round 2 pending); Vaelmere's
craft (в17); main quest v1 scope (в18).

## Step-by-step plan

1. **DONE** — zone created; mandatory reading (AGENTS.md, ARCHITECTURE.md,
   DECISIONS.md, feature-requests-1.md, LANDSCAPE.md, sim spec).
2. **DONE (this stage):** RESEARCH.md (58 rules N1–N58, research subagent);
   PITCHES.md (three distinct pitches — The Debt of Harrowmere / The Kindly
   Dark / The Unwriting — with Russian summaries, awaiting the user's в16
   choice); QUEST_FORMAT.md negotiated with sim and **CONTRACT-FROZEN** (sim
   landed Condition.h/Dialogue.h/Ids.h per its §9 record; lead batches the
   sync record). **HARD GATE holds: no quest content beyond pitch level
   until the user picks a pitch.**
3. After pitch choice: BIBLE.md (world truth for the chosen pitch), act-1
   quest outline on the valley testbed, named characters of Vaelmere
   (keyed to its craft, в17).
4. After sim ACKs the format: first hand-authored quest as data + its
   localization keys + character cards for Vaelmere incidentals; walk it in
   engine with sim.
5. Settlement-craft template quests: schema first (QUEST_FORMAT.md §
   templates), instances after the testbed hamlet has a craft.
6. Each stage: spec + zone docs updated in the same changeset (Rule 18),
   UPD entries appended (Rule 17).

## How it is verified

- `python3 tools/header_check.py --all` passes for every file in the zone.
- Docs in English; user-facing pitch summaries additionally in Russian
  (project rule; devlog convention).
- QUEST_FORMAT.md counts as verified only when sim ACKs it (message,
  recorded in the devlog at the next sync) — until then it is marked PROPOSAL.
- Quest data files (future): validated against the schema by a checker tool
  (to be negotiated — likely `tools/`, lead zone); zero user-facing literals
  outside localization files (greppable); every referenced world flag,
  location, NpcAction variant exists.
- Narrative rules are checkable: pitches must cite which RESEARCH.md rules
  they satisfy; act-1 content must map to real LANDSCAPE.md POIs.

## What this zone does NOT do

- **No engine or game C++** — not even the quest runner: sim's zone. No edits
  under `engine/` or `games/daggerfall_n/src/`, ever.
- **No terrain/placement design** — design's zone; story requests places via
  message (e.g. "act 2 needs a second settlement"), design places them.
- **No NPC control paths besides `NpcAction`** (Rule 15) — quest data may
  only reference typed actions sim exposes.
- **No user-facing strings outside localization files** (Rules 5–6, Q59).
- **No numeric constants invented in data** — NUMBERS.md via the lead
  (Rule 14 discipline applies to content too).
- **No quest content before the user picks a pitch** — pitches are options,
  not canon; writing canon early wastes the user's decision.
- **No LLM free-writing of quest dialog** (в12): authored nodes only; LLM
  is scoped to incidental chatter within authored character cards.
- **No promises the engine cannot keep:** no quest markers (the landscape is
  the quest marker, LANDSCAPE.md §1.4), no scripted camera, no cutscenes —
  unless a group sync adds them.
