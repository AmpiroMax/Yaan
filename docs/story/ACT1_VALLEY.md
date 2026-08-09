<!--
Created: 09:08:2026 - 15:00:07
Last updated: 09:08:2026 - 19:32:18
-->
<!--
UPD:
- 09:08:2026 - 15:00:07: Created the act-1 outline for main arc A (The Debt of
  Harrowmere) on the built valley: four main quests with stage skeletons,
  secondaries, fishing/farming template concepts, the Harrowward castle
  beats, escalation and hope-spot placement, act-gate release, world flags,
  and the minimum buildable scope for core/design.
- 09:08:2026 - 15:05:56: Folded in design's castle ruling (LANDSCAPE §6.1):
  location row updated, two new §4 beats keyed to the ruled terrain (the
  castle+barrow one-frame composition; the solar that faces away from the
  grave visible from its own yard), §7 split into design-owned exterior vs
  story-requested interior set, open item 1 closed.
- 09:08:2026 - 19:17:47: Castle rescaled to a fortress per the user's
  decision (LANDSCAPE §6.1 revised): §4 gains the scale note and a third
  beat (the re-read — walls as testimony the valley stopped seeing), §7
  exterior spec updated to the 80x80 m towered curtain on a 120 m terraced
  pad. Interior set unchanged and still act-1 blocking.
- 09:08:2026 - 19:28:46: MQ4's trespass route given a body from design's
  geometry — the unbuilt NNE stretch of ward C, reachable off-corridor past
  the barrow — and the act-1/act-3 rhyme recorded: the player breaks in by
  the same path the dead will later take, under the tower that has watched
  the grave through that gap for four generations.
- 09:08:2026 - 19:32:18: Recorded that the trespass route is now a VALIDATED
  worldgen invariant (continuous, deliberately non-corridor-grade, forced
  within 40 m of the barrow entrance) and that ward C's completion fraction
  has two dependents - the act-1 trespass route and the act-3 muster gap.
-->

# ACT1_VALLEY.md — Act 1 on the Built Valley

Canon: `BIBLE.md` (Pitch A — The Debt of Harrowmere). Rules: `RESEARCH.md`
(Nx, binding). Data contract: `QUEST_FORMAT.md` (frozen §2.1/§4 vocabularies
— everything below is expressible in them; nothing here needs a new atom).
World: `docs/design/LANDSCAPE.md` §7 (the built valley) and §6.1/§6.1.3
(Harrowward: siting, fortress scale, ward phasing, sightlines — all ruled).

Scope share (в18: 3 acts, 8–12 main quests total): **act 1 = 4 main quests**,
**3 secondaries**, **2 template families** (fishing, smallholding — в17).
Nothing here specifies combat mechanics or numbers (в19 pending): monsters
and fights are written at intent level only.

Act 1's job, in order: (1) make the hero ordinary by having the player *do*
ordinary work (N12); (2) show the affliction in the economy the player just
worked in (N58); (3) teach the game's core verb — a debt is settled by
naming and restitution, never by killing; (4) put the crime, the beneficiary
and the cover-up in the same small valley; (5) end by telling him whose blood
he is, and release him outward (N4).

---

## 1. Locations and what each one is for

| Place (LANDSCAPE §7) | Story role in act 1 |
|---|---|
| **Vaelmere** (hamlet, lake shore) | home, work, the rust's human cost, the cast |
| **The Mere** (lake) + river Vael, fords | the craft economy: nets, catch, ferry, flooded fields |
| **Harrowward — the Ward** (castle, sited at the crag's foot ~55 m from the barrow, gate facing the valley/ford — LANDSCAPE §6.1) | state power, petitions, testimony, the muniment room, the Refusal beat |
| **Shrine knoll** | the naming rite; the shrinewarden; where the dead are laid down |
| **The Hiding** (NW lakeshore cave) | the three children who fled the Feast and starved: the first repayable debt |
| **Harrowmere Hall** (SE forest ruin) | the feast-hall itself, tables still set: what actually happened |
| **The Backbarrow** (crag south face) | the mass grave, the Backward Rite, the case opening |
| **Ravenscar ward-tower** (crag top) | the beacon-log: the signal, and the name that lit it — act-1 climax |

Occlude-and-reveal is already in the terrain (LANDSCAPE §1.4): the barrow is
hidden from the hamlet, the forest ruin sits in its clearing, the tower
crowns the skyline from everywhere. Act 1 walks the player *up* that
hierarchy: water level → forest → barrow → crag.

## 2. Main quests

Stage numbering in tens with gaps (N32); one stage = one journal-visible
change (N33/N34); one active objective at a time except hub N-of-M (N42);
every stage gets a failure/interference transition (N39). Journal entries are
written to read correctly after a 20-hour absence (N6).

### MQ1 — "The Shoring" (opening; ordinariness performed)

The hero is paid a few coppers to shore a collapsed slope on Ravenscar's
south face after the spring rains — a real job, with tools, for a named
employer, alongside a named neighbor. It is deliberately unheroic (N12): dig,
prop, haul, get paid. The slope gives anyway, and opens the **Backbarrow's
antechamber**.

Inside: cold, old grave-goods with every inscription **scraped away**, and
the dead — who rise, and then **stop**, and stand looking at him while his
neighbor is struck down running. They do not touch him. They step aside.
He walks out of a barrow no one walks out of, which is the fact the whole
valley will hold against him for the rest of act 1 (N19: the cast carries
the doubt).

- Stages: 10 hired → 20 slope collapses / enter → 30 the dead stand → 40
  escape → 50 report in Vaelmere → 60 nobody believes the part that matters.
- Sets: `flag.act1.barrow_opened`, `flag.hero.dead_will_not_strike`,
  `flag.vaelmere.first_death`.
- Escalation rung 1 (the Marking) begins on stage 50: rust appears on doors
  and nets over the following days.
- Interference: if the player never reports, the elder sends someone to
  fetch him (quest continues; the rumor gets worse without his version).

### MQ2 — "What the Rust Reads" (the valley sickens; the core verb taught)

Rust marks doors in proportion to the lies inside. Vaelmere starts eating
itself: a mob wants the marked houses answered for, the Corvane bailiff wants
the tithe anyway, and one household is marked that everyone knows to be
honest — the **Fens**, the hero's own kin. (His family's hidden name is the
lie; he doesn't know that yet, and neither does the player.)

Hub-and-spoke (N37): 3 spokes in any order — trace the marked houses; take
the trouble to the **Ward** on petition day (§4); follow the drowned-sounding
rumor to **the Hiding**. The cave holds the small skeletons of **three
children** who fled the Feast and starved hiding, and scratched marks on the
wall that are almost names. Bringing what remains to the **shrine knoll** and
having the shrinewarden perform the **naming rite** lays them down — and the
rust recedes for a street. **This is the lesson:** the dead are not enemies
to be killed; they are claimants to be named and paid.

- Stages: 10 rust spreads → 20/30/40 spokes → 50 the Hiding → 60 the names
  recovered → 70 the rite at the shrine → 80 the street cleans.
- Requires the shrinewarden (naming rite) and the Ward (petition day) to
  exist. Gate: 2 of 3 spokes + the rite.
- Sets: `flag.act1.first_naming_done`, `flag.vaelmere.mob_defused` (or
  `flag.vaelmere.mob_blood` if the player let it happen — foldback per N35,
  paid back in act 2 dialog per N36).
- Hope spot (N52): if the mob is defused, Vaelmere holds its lantern night
  on the water anyway — placed immediately before MQ3's darkness.

### MQ3 — "The Hall Under the Oaks" (what actually happened)

A Maddren-blood resident (or the roll-keeper's grandson, if the player earned
that trust in a secondary) leads him to **Harrowmere Hall** in the SE oak
forest: the feast-hall, roof gone, oaks through the floor, **tables still
set**. Here the player reconstructs the Feast from the room itself — the
seating, the barred doors, the truce-banners rotted on their poles — and
finds the **Backward Rite** written into the hall's stones: the murdered were
unnamed on purpose, so they could never accuse.

Intent-level danger: what the rite bound down is thickest here, and the
deepest of the Feast's dead have lost so much name that they no longer know
whom they are owed by, and strike anything `[weave C]`. Combat detail waits
on в19.

- Stages: 10 the offer of a guide → 20 the hall → 30 reconstruct the Feast →
  40 the rite discovered → 50 out alive with proof of *what*, not *who*.
- Sets: `flag.act1.feast_truth_known`, `flag.maddren.trust_1`.
- Interference: hostile to Maddrens in MQ2 → no guide; the player finds the
  hall alone, later, and loses the roll-keeper's help for the act.

### MQ4 — "The Black Signal" (act-1 climax; the crag; the name)

Two documents exist, in two places, and neither alone is enough. The
**ward-tower** on Ravenscar holds the **beacon-log** — proof that the tower's
ward-flame was lit as a *signal*, at the hour of the Feast. The **Ward's
muniment room** holds the crown's grant to House Corvane, which names the
sworn-sword who lit it: **Osric Ferrant**.

The climb to the tower is act 1's summit beat (N11 — the act's climax at the
landmark that has dominated every skyline since minute one). Getting the
grant is intrigue at the castle (§4). Putting the two together gives the
hero the sentence that ends act 1: *the man who lit the signal was my
family, and my family changed its name twice to survive it.* The Fen kin
confirms the fragments she has held back all act.

**Refusal beat (N14).** He takes it to Lord Rhys Corvane — the one authority
who should own this. Rhys, decent and trapped, **buries it**: the document is
"a copy of a copy", the clerk takes custody of it, and the hero is warned off
his own blood in his own valley. Delegation tried; delegation closed. The
crown clerk (Quiet Hand) now knows exactly who the hero is, which is act 2's
opening danger.

- Stages: 10 the log's existence → 20 the climb → 30 the beacon-log → 40 the
  muniment room (three routes: petition-day access, the dowager's indulgence,
  trespass) → 50 the two documents joined → 60 kin confirms → 70 taken to
  Rhys → 80 buried → 90 the account named aloud (a stage that only sets the
  frame: he is the debtor's line, and the dead are waiting for him).
- Sets: `flag.act1.true_name_known`, `flag.corvane.refused_confession`,
  `flag.quiethand.hero_marked`, `flag.act1.complete`.
- Escalation to rung 3 (the Summons): the Backbarrow's dead assemble; roads
  out of the valley foul.
- **Act gate + explicit release (N4/N5):** the shrinewarden (or the Fen kin)
  states plainly why the next step is elsewhere and waits on him — *the roll
  of the murdered's names is not in this valley, and the account cannot be
  read without it; learn the valley first, and take what friends you have.*
  No countdown language anywhere (N1/N2): the pressure is "the marks spread",
  never "you have N days".

## 3. Secondaries (hand-authored, act 1)

1. **"One for One"** — a Maddren boy has gone missing days after a Corvane
   child did. Resolvable truly (find both alive, expose who staged it),
   transactionally (a payment that buys silence) or brutally. Feeds the
   chain-of-debts grammar early and sets `flag.maddren.feud_check`.
2. **"The Rotted Banner"** — an old truce-banner in the tavern rots overnight
   and nobody will touch it; tracing who hung it opens the valley's local
   memory of the Feast and gives the first true rumor of the Backward Rite.
3. **"The Tithe and the Flood"** — the Ward's fishing tithe falls due the
   week the river takes the low fields; the player can side with the tenants,
   the bailiff, or find the third answer. Establishes Rhys as decent and
   trapped before MQ4 makes the player hate him.

## 4. The Ward (castle) in act 1 — beats and access

Fiction per `BIBLE.md` §5.1; geography ruled by design (LANDSCAPE §6.1) and
folded in: the Ward sits at the crag's foot, reads as a pale-grey notch
against the crag's body (never against sky — the tower keeps the skyline),
its gate faces the valley and the ford, and **its yard and gate have clear
line of sight to the Backbarrow entrance ~55 m away**.

**Scale (revised 09:08:2026):** the Ward is a fortress, not a hall — it
**reads from Vaelmere** as a grey band along the crag's foot, resolving on
approach into gatehouse, towers and roofs. The act-1 reveal is therefore
"you did not know how big it was", not "you did not know it was there", and
the hamlet lives in sight of Corvane power every day. Per BIBLE §5.1 the
walls are testimony — the house fortified against what it buried — and they
are badly under-manned since the succession war took the garrison.

Three beats now key off that terrain truth:

- **The re-read** (MQ3→MQ4): once the player knows what happened at
  Harrowmere, the castle stops being scenery. Towers overlooking a grave, a
  charter that says *ward the barrow* — the evidence stood in the open for
  four generations and the valley stopped seeing it. Author one late-act
  line from a resident who has looked at those walls daily and never once
  asked what they were built against.

- **The frame** (MQ1 aftermath, free): walking up to the castle to report or
  petition, the player has the barrow mouth and the castle in one view. No
  dialog needed — the composition says who profits from that grave.
- **The turned back** (MQ4, Refusal): the solar looks out over the valley and
  the ford, not over the barrow. Rhys buries the truth in a room deliberately
  facing away from it, while the thing itself is visible from his own yard
  fifty paces off. Write the scene to use that.

Act-1 beats that **live in the castle**:

- **Petition day** (MQ2 spoke; secondaries 1 and 3): the public hall — the
  player brings a grievance and watches the lord's justice work honestly and
  fail structurally, in front of tenants. Introduces steward, gate-serjeant,
  chaplain, dowager, and the crown clerk in one scene, with ranks legible.
- **The tithe-yard** (secondary 3; craft templates): the fishing tithe is the
  player's ordinary-business key into the yard and granary — the craft
  economy (в17) doubles as castle access.
- **The muniment room** (MQ4 stage 40): the crown grant naming Osric Ferrant.
  Three routes — petition-day access earned, the dowager's indulgence
  (she knows the truth entire and has her own reasons), or **trespass over
  the unfinished wall**. Caught = eviction and a closed door for a stage,
  plus a disgrace flag paid back in act 2 (N39, N36) — never a dead end.
- **The gap as the trespass route (act-1/act-3 rhyme — use it).** Design's
  geometry gives the third route a body: the unbuilt NNE stretch of ward C
  is reachable off-corridor up the steep spur, past the barrow. This is a
  **validated worldgen invariant** (LANDSCAPE §6.1.3), not a hope: the route
  is checked every run, deliberately kept non-corridor-grade (30–45°, no
  width guarantee — the difficulty is the point), and forced to pass within
  40 m of the barrow entrance, so the rhyme is geometric rather than lucky.
  Ward C's completion fraction now has **two dependents** (this route and the
  act-3 muster gap) — nobody tunes it for one without checking the other. So the
  player who breaks into Harrowward climbs in **by the same path the dead
  will take in act 3**, through the hole the Corvanes could not bring
  themselves to close, under the tower that has watched the grave through
  it for four generations. Author the act-3 muster to make the player
  recognize the ground: he was here first, alone, doing something shameful
  for a good reason.
- **The Refusal beat** (MQ4 stage 70–80): in the solar, privately. Rhys
  buries it; the clerk takes custody of the document. This is the scene the
  whole act has been walking toward, and it needs an interior with a door
  that closes.
- **Recruitment** (chain of debts, first link): after the burial, Rhys can
  still be closed **truly** (he begins the long turn toward confession),
  **transactionally** (he buys the hero's silence with protection and
  supplies), or **brutally** (blackmail with the grant's copy) — one flag,
  three colors, paid off in acts 2–3 (N35/N36, BIBLE §7).

**Access invariant** (why this is a hub, not a wall): the castle must be
enterable by an unarmoured nobody on foot, on ordinary business, in daylight,
without violence. The Quiet Hand's grip here is one clerk and a border
treaty — not a garrison.

## 5. Settlement-craft templates (в17: fishing + smallholding)

Concept only — schema is `QUEST_FORMAT.md` §5; instances come after act 1's
hand-authored data walks in engine. Both families are craft-native (N44/N45),
2–4 stages, one twist slot (N46), issuance capped and flagged (N47), with an
act-flag text variant so they acknowledge the darkness (N48), and an authored
hook line explaining why this giver, now (N50).

- **Fishing:** nets rotted by the rust and the wright who cannot prove they
  were sound; a boat lost at the far shore; the catch failing where the
  Mere's water has gone wrong near the Hiding; ferrying someone who should
  not be ferried.
- **Smallholding:** the low fields flooded at the ford; a marked grain store
  the neighbors want burned; a beast sickened; a boundary dispute between two
  households whose grandfathers stood on opposite sides at the Feast.

Both families stay predominantly solvable with a good outcome (N55); the
tragedy lives in the hand-authored quests.

## 6. World flags introduced in act 1

To be merged into `assets/world_flags.json` with the cast's flags
(`CHARACTERS_VAELMERE.md`) when data authoring starts. Every flag lists its
setter and readers at authoring time (N38).

| Flag | Type | Set by | Read by |
|---|---|---|---|
| `flag.act1.barrow_opened` | bool | MQ1.20 | ambient, MQ2, escalation |
| `flag.hero.dead_will_not_strike` | bool | MQ1.30 | dialog everywhere, act 3 |
| `flag.vaelmere.first_death` | bool | MQ1.30 | cast dialog, mob logic |
| `flag.act1.first_naming_done` | bool | MQ2.70 | shrine, MQ3, act 3 rite |
| `flag.vaelmere.mob_defused` / `..._blood` | bool | MQ2.80 | act 2 dialog, hope spot |
| `flag.act1.feast_truth_known` | bool | MQ3.40 | all faction dialog |
| `flag.maddren.trust_1` | bool | MQ3.10 | secondary 1, act 2 roll access |
| `flag.act1.true_name_known` | bool | MQ4.60 | everything after |
| `flag.corvane.refused_confession` | bool | MQ4.80 | Rhys's act-2/3 arc |
| `flag.quiethand.hero_marked` | bool | MQ4.80 | act 2 opening danger |
| `flag.ward.disgraced` | bool | trespass caught | castle access, act 2 |
| `flag.act1.complete` | bool | MQ4.90 | act gate |

## 7. Minimum buildable scope for act 1 (for core / design / render)

What act 1 **cannot** ship without, in priority order:

1. **Vaelmere** as a walkable hamlet with interiors enough for the cast to be
   spoken to (tavern, a dwelling or two, the tithe-yard edge).
2. **The Backbarrow** — an enterable dungeon space with an antechamber
   (MQ1) and deeper rooms (act 3 reuses it).
3. **The shrine knoll** — an interactable rite site (MQ2's payoff; the game's
   thesis lives here).
4. **The Hiding** (lakeshore cave) — a small cave with a readable
   wall-scratch set piece.
5. **Harrowmere Hall** (forest ruin) — one interior-scale ruin with a hall
   layout: the tables are the storytelling.
6. **Ravenscar ward-tower** — climbable to a top room holding the
   beacon-log (act-1 climax; already the L0's crown).
7. **Harrowward castle** — REQUIRED for MQ2's petition spoke, secondary 3,
   and MQ4's muniment room + Refusal beat. Exterior mass, pad and access are
   design's (LANDSCAPE §6.1, revised: 80×80 m towered curtain, twin-tower
   gatehouse, hall + keep-solar inside the enclosure, 120 m terraced pad,
   graded walkable ramp to the gatehouse). Minimum
   **interior** set, which is story's ask of whoever builds interiors:
   **hall (public), yard/tithe-yard, muniment room, solar**. Gatehouse and
   chapel are desirable, not blocking. Without the castle, MQ4's Refusal beat
   has nowhere to happen and act 1 loses both its state-power face and its
   intrigue; the beats that require it are listed in §4.

Everything else in act 1 (fords, forest, meadows, POI chain) already exists
in the built testbed.

## 8. Open items

1. ~~Design's ruling on castle siting and the L0/skyline hierarchy~~ —
   **RESOLVED** (LANDSCAPE §6.1, folded in): crag keeps L0 and the skyline,
   the Ward reads against the crag's body as a pale-grey L1 mass, gate
   valley-facing, yard-to-barrow sightline guaranteed. Nothing in the fiction
   had to bend.
2. Player-voice question (BIBLE §10.1) affects how MQ4's Refusal beat is
   staged.
3. в19 combat grill gates every fight in MQ1/MQ3 and the barrow's deeper
   rooms.
4. Act 2 needs geography outside the valley (river town, a lords' assembly
   seat) — story will file the request once act-1 data is walking in engine.
