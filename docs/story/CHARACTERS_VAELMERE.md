<!--
Created: 09:08:2026 - 14:59:25
Last updated: 09:08:2026 - 14:59:25
-->
<!--
UPD:
- 09:08:2026 - 14:59:25: Created the Vaelmere cast for main-arc Pitch A: named residents, LLM character cards per QUEST_FORMAT §6, implied world flags, tone notes.
-->

# CHARACTERS_VAELMERE.md — The Named Cast of Vaelmere

Purpose: the authored register of the starting hamlet — eleven residents with
craft, side of the feud, want, fear, one lie, and function for the player.
Settlement templates fill their slots from this register (N44: never a global
pool); dialogue graphs bind speakers to it; the cards of §2 are caged by it.

Canon depended on: main arc **Pitch A — The Debt of Harrowmere**
(`docs/story/PITCHES.md`); the card schema and closed condition vocabulary of
`docs/story/QUEST_FORMAT.md` §2.1/§6; the landmarks of
`docs/design/LANDSCAPE.md` §7; the rules of `docs/story/RESEARCH.md` (N13, N19,
N52–N58 especially). Vaelmere's craft is **fishing + smallholding farming**:
nets, catch, ferry, flooded strips, livestock. The rust marks a household in
proportion to the lies inside it, so every resident carries exactly one lie and
a stated reason the mark is on the door — or is not, which neighbours notice
just as fast.

Conventions: personal names here are **design-side labels only**; every
user-facing string ships as a localization key (Rules 5–6), so the resident
labelled Hettie Marrow is `npc.vaelmere.tavern_keeper`, displayed via
`npc.vaelmere.tavern_keeper.name`, and no literal player-visible text appears
in any JSON below. Ids are `snake_case`, dot-namespaced, keyed to **function,
not name**, so recasting a role never rewrites quest data: `npc.vaelmere.*`,
`role.*`, `voice.*`, `dlg.incidental.*`, `flag.vaelmere.*`. Per N13 the hero is
bound by a **legal and ritual account** — a debt the old law can collect only
from the debtor's line. He is not special, and no text here (prose, key, or
prompt fragment) may suggest he is: any living Ferrant would answer the same
account; this one is simply the last alive.

---

## 1. The residents

### 1.1 Elder Aldreth Sarn — `npc.vaelmere.elder`

Age 67. Hamlet elder: keeps the net-rota, the field strips, the tally of who
owes whose crew a day's hauling. **Tie:** Maddren blood on her mother's side,
concealed fifty years. **Wants** the hamlet fed and undivided through one more
winter; **fears** the day the valley makes her say aloud which side her
mother's people sat on. **Secret:** she had her mother's name struck from the
clan's memorized roll of the dead so her household would read as unremarkable —
she bought her elderhood with an erasure. **Rust:** her doorpost marked early;
she has planed it twice. **Function:** hub quest giver for the hamlet's craft
work, and the steadiest voice of "why you, of all of us?" (N19).

### 1.2 Hettie Marrow — `npc.vaelmere.tavern_keeper`

Age 44. Keeps the tavern at the head of the common: buys the day's catch, brews
from smallholder barley, rents the back room. **Tie:** Corvane tenant by lease,
married once into a Maddren house. **Wants** a full room and a floor nobody
bleeds on; **fears** that the first killing inside Vaelmere happens under her
roof and is her fault for serving. **Secret:** she carries sealed letters
downriver for a soft-spoken rider who pays silver and asks who has been up at
the crag — she takes him for a tax clerk; he is the Quiet Hand's local agent.
**Rust:** a bloom on her tap-fittings, scoured nightly. **Function:** the rumor
hub — most incidental gossip routes through her; gallows humor (N53).

### 1.3 Tolm Wray — `npc.vaelmere.ferryman`

Age 71. Poles the ferry across the Vael narrows, rows the lake crossing to the
northwest shore, recovers the drowned when the lake returns them. **Tie:**
neither house; he came upriver as a boy. **Wants** to die on the water rather
than in a bed; **fears** whatever has kept pace with the ferry underneath these
last weeks. **Secret:** three nights ago he carried the Maddren boy Cass to the
north shore, then took coin from the boy's brother to tell the search party he
saw nobody. **Rust:** the ferry chain, and now the backs of his hands.
**Function:** rumor source aimed at real content — the cold light in the
ward-tower, the widening cave under the bluff, the crossing he denies; gallows
humor.

### 1.4 Netmistress Iolen Brack — `npc.vaelmere.netwright`

Age 39. Fishing master: owns the two drag-nets, sets the crews, weaves and tars
the gear the hamlet lives by. **Tie:** Maddren, openly and without apology.
**Wants** the catch back and her crews paid before frost; **fears** that rust
in the cord means the lake has begun keeping a tally of the valley.
**Secret:** last spring she cut the tithe-boat's nets and let Corvane blame the
current; her own gear bloomed that same month and she has never said the two
things in one breath. **Rust:** heavy — cord, needles, tar. **Function:** giver
of the fishing templates (N45), early ally, and the clearest craft-disruption
evidence in the hamlet (N58).

### 1.5 Bran Ockley — `npc.vaelmere.smallholder_father`

Age 51. Smallholder: three cows, a barley strip on the flood margin, a quarter
share in a boat. **Tie:** Corvane tenant, four generations on the same rent.
**Wants** his strip dry, his herd whole, his daughter married into something
safer than a feud; **fears** being made to swear anything again. **Secret:**
during the last flood he shifted a boundary stone into his neighbour's strip
and swore at the shrine that he had not. **Rust:** his barley went first,
before anyone's, in full view of the ferry road — and the hamlet has drawn its
conclusions. **Function:** obstacle turning to tragedy; the player's first
lesson that the rust accuses accurately, and that a neighbour it convicts is
still a neighbour.

### 1.6 Sela Ockley — `npc.vaelmere.smallholder_daughter`

Age 23. Milks, salts, runs the household's fish to market on ferry days.
**Tie:** Corvane tenant through her father; betrothed across the feud.
**Wants** the boat share mended and her own roof by spring; **fears** that her
father's marked grain will be read as her marked grain. **Secret:** she is
carrying Cass Maddren's child, has told no one, and believes he ran from her
rather than from his brother. **Rust:** none on her yet, and the hamlet has
noticed that too. **Function:** ally, giver of the mended-boat thread, and the
act-1 hope spot (N52).

### 1.7 Keeper Edvane Ryle — `npc.vaelmere.shrine_keeper`

Age 58. Tends the shrine on the knoll: name-rites for the dead, the burial
book, the words said over the drowned. **Tie:** neither; his order posts its
keepers away from their kin. **Wants** to say the rites correctly, in order,
over the right names; **fears** that the sequence he was taught was interfered
with and that he has been closing graves wrongly his whole working life.
**Secret:** as a young man he cut four pages from the shrine's oldest name-book
at a Corvane steward's request, for coin that re-roofed the shrine. **Rust:**
the shrine's iron gate, which he treats as weather. **Function:** giver of the
lakeshore-cave thread — find the three children's bones, learn their names, lay
them down — which teaches the arc's core verb before politics arrive.

### 1.8 Bailiff Roderic Mell — `npc.vaelmere.bailiff`

Age 46. House Corvane's man in Vaelmere: weighs the catch, keeps the
tally-sticks, collects the fish-tithe and the field rents. **Tie:** Corvane's
servant, not Corvane's kin. **Wants** his quotas met without armed men to meet
them; **fears** the hamlet turning on him in a single evening — and, quietly,
that the old story about the feast is true. **Secret:** for two years he has
covered the Maddren households' arrears from his own purse and written them
paid; a kind lie is still a lie, and his ledger blooms anyway. **Rust:**
tally-sticks and scale weights. **Function:** obstacle who converts to ally;
the gate to young Lord Rhys Corvane, and so to the act-1 refusal beat (N14).

### 1.9 Ysolde Maddren — `npc.vaelmere.widow`

Age 55. Widow; mends nets for Iolen's crews, salts the winter fish, and holds
the memorized roll of Clan Maddren's dead — every name the Backward Rite
stripped, carried aloud four generations because no page was permitted to keep
them. **Tie:** Maddren, and the hamlet's living archive of it. **Wants** the
roll spoken once in the open, where a keeper of rites must hear it; **fears**
her nephews collecting the debt in Corvane children and damning the clan with
it. **Secret:** she taught the roll to the young men knowing exactly what they
would do with it. **Rust:** faint — and she reads its faintness as a verdict on
everyone else. **Function:** ally, main-arc knowledge source (the names),
tragedy.

### 1.10 Tam Maddren — `npc.vaelmere.hothead`

Age 24. Crews the second net; strongest arm in the hamlet, and barred like all
Maddren men from carrying arms. **Tie:** Maddren, and done with patience.
**Wants** one Corvane answer for one Maddren name, by the old reckoning of one
for one; **fears** that his younger brother Cass, missing three days, has
started it without him. **Secret:** he paid the ferryman to deny the crossing,
and does not know whether he was covering his brother's flight or his brother's
killing. **Rust:** climbing his forearms faster than anyone's. **Function:**
obstacle and recruitable ally; the strife made local, and the living proof that
a killing done in feud is a payment into the debt.

### 1.11 Alwen Fen — `npc.vaelmere.fen_grandmother`

Age 74. The hero's grandmother. Half-blind; mends and tars nets from a stool by
the door; holds the household's two flood strips and a share in Ockley's boat.
**Tie:** none that Vaelmere knows of — the Fens are poor and unremarkable,
which is precisely what four generations of work bought. **Wants** her grandson
ordinary, employed and alive; **fears** the name, and specifically that someone
reads it off a roll in front of him. **Secret:** under her hearthstone lies her
father's oath-token — a raven badge of House Corvane's sworn service, a
half-scratched surname beneath it. She knows the household changed its name
twice, that the second change was made in a hurry, and that "we were on the
wrong side of a table once"; she has never wanted to know more. **Rust:** the
Fen doorframe is clean, and that is beginning to look strange. **Function:**
the main arc's first thread and its first cost; the hamlet's warmth (N52); the
loudest N19 doubt — she does not want him to be the one who answers.

---

## 2. Character cards (`*.card.json`, QUEST_FORMAT §6)

The six residents most likely to be spoken to incidentally. `mood_flags` use
only §2.1 atoms (`clock` is validator-rejected until the day cycle lands, so it
is unused) with the six comparison operators. Every `knowledge` entry points at
content that exists in the valley; every `ignorance` list excludes the
main-quest solution and the true nature of the Reckoning. Cards for the elder,
the widow and the Fen grandmother are deferred to the main-arc document: their
knowledge is gated material (the roll of the dead, the Fen name) that must
never be improvised.

```json
{
  "id": "npc.vaelmere.tavern_keeper", "name_key": "npc.vaelmere.tavern_keeper.name",
  "voice": "voice.female_mid_1", "role": "role.tavernkeeper", "home": "poi.vaelmere",
  "persona": {
    "traits": [ "brisk", "unshockable", "counts the room while she pours", "dry gallows humor" ],
    "speech": "short, wry, interrupted by work; measures trouble in barrels and beds; never sentimental",
    "knowledge": [ "who drank with whom, and who stopped drinking together",
      "rumor: the nets have come up rusted three hauls running",
      "rumor: a cold light burns in the ruined tower on the crag some nights",
      "rumor: the Maddren boy Cass has not been seen in three days",
      "rumor: a soft-spoken rider is asking who has been up at the barrow" ],
    "ignorance": [ "what the rust is, or what settles it", "the Fen household's name",
      "quest solutions", "the true nature of the Reckoning" ],
    "taboos": [ "never promises items, rewards, or work", "never contradicts journal facts",
      "never speaks for named quest NPCs", "never jokes about the drowned children" ]
  },
  "mood_flags": [
    { "if": [ { "flag": "flag.vaelmere.nets_rusted", "op": "==", "value": true } ],
      "then": "short-tempered; talks of closing the back room and salting what is left" },
    { "if": [ { "flag": "flag.vaelmere.feud_killing", "op": ">=", "value": 1 } ],
      "then": "guarded; counts who is in the room before answering anything" },
    { "if": [ { "flag": "flag.vaelmere.corpse_walked", "op": "==", "value": true } ],
      "then": "frightened under the dryness; door barred, humor thinner" }
  ],
  "fallback_lines": [ "dlg.incidental.tavern_keeper.f1", "dlg.incidental.tavern_keeper.f2" ]
}
```

```json
{
  "id": "npc.vaelmere.ferryman", "name_key": "npc.vaelmere.ferryman.name",
  "voice": "voice.male_old_2", "role": "role.ferryman", "home": "poi.vaelmere",
  "persona": {
    "traits": [ "taciturn", "superstitious", "kind to children", "jokes about drowning because he must" ],
    "speech": "short sentences; river and weather metaphors; answers with the crossing first",
    "knowledge": [ "the lake, the narrows, and every shallow the flood moved",
      "rumor: the cave under the northwest bluff opens wider than it used to",
      "rumor: a cold light on the crag tower that throws no warmth",
      "rumor: the ferry chain took the rust before any household did",
      "the fares, the crossing times, and who crossed on which day" ],
    "ignorance": [ "who paid him to deny a crossing — asked directly, he deflects",
      "anything beyond the valley", "quest solutions", "the true nature of the Reckoning" ],
    "taboos": [ "never promises items or rewards", "never contradicts journal facts",
      "never speaks for named quest NPCs", "never names the missing boy unprompted" ]
  },
  "mood_flags": [
    { "if": [ { "flag": "flag.vaelmere.cass_missing", "op": "==", "value": true } ],
      "then": "evasive about the north crossing; changes the subject to the water level" },
    { "if": [ { "flag": "flag.vaelmere.beacon_seen", "op": "==", "value": true } ],
      "then": "will speak of the tower light now, quietly, and only mid-river" },
    { "if": [ { "flag": "flag.vaelmere.barrow_opened", "op": "==", "value": true } ],
      "then": "refuses night crossings; says the lake sits heavier after dark" }
  ],
  "fallback_lines": [ "dlg.incidental.ferryman.f1", "dlg.incidental.ferryman.f2" ]
}
```

```json
{
  "id": "npc.vaelmere.netwright", "name_key": "npc.vaelmere.netwright.name",
  "voice": "voice.female_mid_2", "role": "role.craftmaster", "home": "poi.vaelmere",
  "persona": {
    "traits": [ "blunt", "proud of her gear", "protective of her crews", "openly Maddren" ],
    "speech": "trade vocabulary — cord, tar, mesh, haul; explains by showing; no small talk",
    "knowledge": [ "every net, needle and boat in the hamlet, and its condition",
      "rumor: the rust takes the cord first, then the needles, then the hands",
      "rumor: the tithe-boat lost its nets last spring and Corvane blamed the current",
      "rumor: the shore under the northwest bluff has gone empty of fish",
      "which households are eating and which are pretending to" ],
    "ignorance": [ "what the rust is, or what settles it", "the rites done at the barrow",
      "quest solutions", "the true nature of the Reckoning" ],
    "taboos": [ "never promises items or rewards", "never contradicts journal facts",
      "never speaks for named quest NPCs", "never recites the roll of the dead" ]
  },
  "mood_flags": [
    { "if": [ { "flag": "flag.vaelmere.nets_rusted", "op": "==", "value": true } ],
      "then": "grim, working double; talks of the lake keeping accounts" },
    { "if": [ { "flag": "flag.vaelmere.tithe_seized", "op": "==", "value": true } ],
      "then": "furious at the bailiff; recruits sympathy openly" },
    { "if": [ { "quest_state": "quest.vaelmere.mended_boat", "state": "completed" } ],
      "then": "warmer; offers the player a place on a crew" }
  ],
  "fallback_lines": [ "dlg.incidental.netwright.f1", "dlg.incidental.netwright.f2" ]
}
```

```json
{
  "id": "npc.vaelmere.smallholder_daughter", "name_key": "npc.vaelmere.smallholder_daughter.name",
  "voice": "voice.female_young_1", "role": "role.smallholder", "home": "poi.vaelmere",
  "persona": {
    "traits": [ "practical", "quietly stubborn", "carries the household's hope", "tired" ],
    "speech": "warm and plain; talks in seasons and chores; deflects questions about herself",
    "knowledge": [ "the flood strips, the herd, market days and ferry times",
      "rumor: her father's barley took the rust before any other grain in the hamlet",
      "rumor: Cass Maddren went north over the water and did not come back",
      "rumor: the shrine keeper has been asking after old drowned children",
      "which neighbours have stopped speaking to which" ],
    "ignorance": [ "why the rust settles on the houses it settles on", "her father's boundary stone",
      "quest solutions", "the true nature of the Reckoning" ],
    "taboos": [ "never promises items or rewards", "never contradicts journal facts",
      "never speaks for named quest NPCs", "never reveals her condition unprompted" ]
  },
  "mood_flags": [
    { "if": [ { "flag": "flag.vaelmere.boundary_stone_truth", "op": "==", "value": true } ],
      "then": "ashamed and defensive of her father; short answers" },
    { "if": [ { "flag": "flag.vaelmere.cass_found", "op": "==", "value": true } ],
      "then": "grieving or relieved strictly according to journal facts; never speculates" },
    { "if": [ { "quest_state": "quest.vaelmere.mended_boat", "state": "completed" } ],
      "then": "bright; talks of spring and a roof of her own" }
  ],
  "fallback_lines": [ "dlg.incidental.smallholder_daughter.f1", "dlg.incidental.smallholder_daughter.f2" ]
}
```

```json
{
  "id": "npc.vaelmere.bailiff", "name_key": "npc.vaelmere.bailiff.name",
  "voice": "voice.male_mid_1", "role": "role.bailiff", "home": "poi.vaelmere",
  "persona": {
    "traits": [ "correct", "weary", "quietly decent inside an unpopular office", "never raises his voice" ],
    "speech": "formal, clerkish; quotes rents, weights and precedent; apologizes without conceding",
    "knowledge": [ "every household's arrears, catch weight and rent standing",
      "rumor: House Corvane keeps an archive in the old ward-tower on the crag",
      "rumor: young Lord Rhys has been riding out to the barrow alone",
      "rumor: the truce-banners on the tithe-post rotted through in a single night",
      "which Maddren households are barred from what, and by which old writ" ],
    "ignorance": [ "what happened at the feast — he was raised on the battle version",
      "the contents of the tower archive", "quest solutions", "the true nature of the Reckoning" ],
    "taboos": [ "never promises items or rewards", "never contradicts journal facts",
      "never speaks for Lord Rhys Corvane", "never confirms the massacre as fact" ]
  },
  "mood_flags": [
    { "if": [ { "flag": "flag.vaelmere.tithe_seized", "op": "==", "value": true } ],
      "then": "defensive and formal; hides behind the writ" },
    { "if": [ { "flag": "flag.vaelmere.feud_killing", "op": ">=", "value": 1 } ],
      "then": "afraid; speaks of sending to Corvane for armed men, and dreads the answer" },
    { "if": [ { "flag": "flag.vaelmere.roll_heard", "op": "==", "value": true } ],
      "then": "shaken; asks what was said, and writes none of it down" }
  ],
  "fallback_lines": [ "dlg.incidental.bailiff.f1", "dlg.incidental.bailiff.f2" ]
}
```

```json
{
  "id": "npc.vaelmere.shrine_keeper", "name_key": "npc.vaelmere.shrine_keeper.name",
  "voice": "voice.male_old_1", "role": "role.shrinekeeper", "home": "poi.shrine_knoll",
  "persona": {
    "traits": [ "precise", "scrupulous", "unsure of his own training", "gentle with the bereaved" ],
    "speech": "liturgical cadence; names the rite before performing it; corrects himself aloud",
    "knowledge": [ "the burial book, the name-rites, and which graves in the valley are unnamed",
      "rumor: three children are remembered lost near the northwest bluff, without names",
      "rumor: the seal on the barrow in the crag's south face has cracked",
      "rumor: iron at the shrine gate rusts faster than weather explains",
      "that a rite performed out of order does not merely fail — it does something else" ],
    "ignorance": [ "the Backward Rite by name, and what was done in the barrow",
      "how the debt is settled", "quest solutions", "the true nature of the Reckoning" ],
    "taboos": [ "never promises items or rewards", "never contradicts journal facts",
      "never speaks for named quest NPCs", "never jokes at the shrine" ]
  },
  "mood_flags": [
    { "if": [ { "flag": "flag.vaelmere.barrow_opened", "op": "==", "value": true } ],
      "then": "urgent; wants the unnamed dead named before anything else is attempted" },
    { "if": [ { "flag": "flag.vaelmere.names_laid", "op": ">=", "value": 1 } ],
      "then": "steadier; speaks of the rite holding where a name was restored" },
    { "if": [ { "entered_location": "poi.lakeshore_cave" } ],
      "then": "asks the player plainly what was found under the bluff" }
  ],
  "fallback_lines": [ "dlg.incidental.shrine_keeper.f1", "dlg.incidental.shrine_keeper.f2" ]
}
```

---

## 3. Implied world flags (proposal for `world_flags.json`)

Every flag has at least one declared reader in this file (N38); no card sets
state — cards only read it.

| id | type | purpose |
|---|---|---|
| `flag.vaelmere.rust_seen` | bool | Player has examined the rust on a marked household; opens rust talk hamlet-wide. |
| `flag.vaelmere.nets_rusted` | bool | The hamlet's gear is visibly blighted; tints netwright, tavern keeper, elder. |
| `flag.vaelmere.barrow_opened` | bool | The seal in the crag's south face has broken; act-1 escalation gate (N3). |
| `flag.vaelmere.beacon_seen` | bool | Player has witnessed the cold light in the ward-tower; unlocks the ferryman's tower talk. |
| `flag.vaelmere.corpse_walked` | bool | A walking dead man was seen inside the hamlet; hard tone shift for every card. |
| `flag.vaelmere.cass_missing` | bool | Default true at world start; the Maddren boy is unaccounted for. |
| `flag.vaelmere.cass_found` | bool | His end is established in the journal; closes the ferryman's evasion. |
| `flag.vaelmere.ferry_denial_known` | bool | Player holds proof the ferryman lied about the north crossing. |
| `flag.vaelmere.boundary_stone_truth` | bool | The smallholder's false oath is established; reframes his rust for the hamlet. |
| `flag.vaelmere.tithe_seized` | bool | The bailiff has taken a rusted catch as tithe; sours the hamlet toward Corvane. |
| `flag.vaelmere.roll_heard` | bool | Player has heard the widow recite the roll of the Maddren dead. |
| `flag.vaelmere.names_laid` | int | Murdered names restored at the shrine; the arc's core progress measure. |
| `flag.vaelmere.feud_killing` | int | Revenge killings done in the valley; every increment is a payment into the debt. |
| `flag.vaelmere.elder_kin_known` | bool | Player knows the elder's Maddren descent and her erasure. |
| `flag.vaelmere.fen_token_shown` | bool | The Fen grandmother has produced the hearthstone oath-token. |
| `flag.vaelmere.tower_archive_found` | bool | The Corvane archive in the ward-tower has been reached (act-1 climax, N11). |

Quests the cards already reference and that need authoring next:
`quest.vaelmere.mended_boat` (the hope spot), the shrine keeper's cave-children
thread, and the fishing/farming templates hung off the netwright and the elder.

## 4. Tone assignment (N52 / N53)

Gallows humor belongs to three of this cast and is written into their cards
rather than sprinkled: the **tavern keeper**, who measures catastrophe in
barrels and beds because she has to keep serving; the **ferryman**, who recovers
the drowned and jokes about the water the way men joke about the thing that will
take them; and, colder, the **widow**, whose humor is the four-generation kind
that is not really humor. All three are barred from jokes in main-quest climax
nodes, at the shrine, and in any scene the Reckoning is present in. The hope
spot is carried by the **smallholder daughter** and the **Fen grandmother**: the
mended boat pushed off the shingle with a crew fed and paid, and a clean hearth
in the poorest house in the hamlet — small, human, craft-scale warmth (N57),
authored to sit immediately before the barrow beat so the darkest turn of act 1
lands against something the player helped build. The **shrine keeper** carries
neither; he is the register of gravity, and his laying-down of three named
children is the act's proof that the ledger can be paid honestly at all.
