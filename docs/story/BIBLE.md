<!--
Created: 09:08:2026 - 14:56:53
Last updated: 09:08:2026 - 15:00:07
-->
<!--
UPD:
- 09:08:2026 - 14:56:53: Created the narrative bible for the chosen main arc
  (Pitch A — The Debt of Harrowmere): world truth, the Harrowmere Feast and
  the Backward Rite, the Reckoning's laws and escalation ladder, the hero's
  legal-not-destined binding (N13 guard), the three factions, three-act
  spine, ally model, naming and text conventions, banned-strings list.
- 09:08:2026 - 15:00:07: Added §5.1 — Harrowward ("the Ward"), House
  Corvane's seat: the user-required castle in the minimal version. Holder
  (Lord Rhys Corvane, act-1 alliance keystone), household roles, the Quiet
  Hand's documentary (not military) grip, four access routes keeping it a
  hub rather than a fortress, act-1 player verbs, and why it earns its build
  cost. Siting and the L0/skyline question are design's binding ruling.
-->

# BIBLE.md — Narrative Bible: The Debt of Harrowmere

Status: **CANON.** The user chose Pitch A (grill в16); elements of pitches B
and C are woven in where marked (`[weave B]`, `[weave C]`) — A is the spine.
Everything here binds all story data files, dialog, and quest text. Changes
to canon go through this file, with a UPD entry.

Companion docs: `PITCHES.md` (the pitch as pitched), `RESEARCH.md` (rules
cited as Nx — binding), `QUEST_FORMAT.md` (the frozen data contract),
`ACT1_VALLEY.md` (act-1 build-out), `CHARACTERS_VAELMERE.md` (the cast),
`docs/design/LANDSCAPE.md` (the built world; place = terrain truth).

Working defaults from the lead (в17/в18), treated as decided: **Vaelmere's
craft = fishing + smallholding farming**; **main quest v1 = 3 acts, 8–12
hand-authored main quests + 6–10 hand-authored secondaries** — grow only
after the first playable end-to-end run.

---

## 1. The premise in one breath

Four generations ago the crown ended a rebellion with a peace-feast and
butchered its guests under truce-banners. It buried them namelessly so they
could never accuse it. The debt was never paid; it compounded. Now the wards
fail, and the dead have opened their case. They do not invade — they
**collect**. And the only man whose payment the old law will accept is a
hired hand in a fishing hamlet who does not yet know his family's real name.

Tone: dark. DNA: Game of Thrones (the crime under the realm, the courts) +
The Lord of the Rings / The Hobbit (the small carrying what the great will
not) + Skyrim (play, epoch-hero framing). Proposed addition, approved for
flavor: the Icelandic sagas — feud-law as narrative engine.

## 2. World truth (canon facts)

**The Kingdom of Ealdmarch.** A cold northern realm of river valleys,
clan-country in the uplands, charter towns downriver. Old rites are kept at
knoll-shrines by hereditary **shrinewardens** — unglamorous keepers of the
naming and burial rites `[weave B]`; the crown's law courts and the shrines
have overlapping, quarrelsome jurisdictions over the dead.

**The Harrow Rising** (~100 years ago). The upland clans rebelled over
levies and land. They were not beaten in the field; they were beaten at the
table.

**The Harrowmere Feast** (~96 years ago; four generations). King **Aldric II,
called the Peacemaker**, invited the defeated clans to a peace-feast under
truce-banners at **Harrowmere Hall**, in the valley then called Harrowmere.
Everyone who came was killed — men, women, hostage children. The official
name of the event is **the Peace of Harrowmere**; the treaty signed after it
fixed the northern border and created half the realm's noble titles. Folk who
know call it **the Feast**, and do not finish the sentence.

**The Backward Rite.** The dead were buried in the barrow under Ravenscar
Crag with the funeral rites deliberately performed backwards. This was not
spite: it was **legal engineering**. The rites that give a dead person a name
before the gods were reversed, stripping the murdered of the standing to
testify. A nameless dead cannot accuse; an unaccused crown cannot be
judged. `[weave C — names as the substance of standing; the barrow's dead
must be re-named to be laid down.]`

**The signal.** The killing began at a beacon lit from the **ward-tower on
Ravenscar Crag** by a sworn-sword of House Corvane named **Osric Ferrant**.
The tower's ward-flame — older than the massacre, its purpose half-forgotten
even then — was used as a murder signal, which is the specific desecration
that made this valley the wound.

**The valley's names.** Harrowmere (old) → after the massacre the crown
renamed and resettled it; the hamlet is **Vaelmere** on the river **Vael**,
the lake is **the Mere**. Locals call the crag Ravenscar; the barrow is
called **the Backbarrow** by those who will name it at all; **Harrowmere
Hall** stands swallowed by the SE oak forest; the NW lakeshore cave is
**the Hiding**.

**Ninety-six years of quiet.** The wards held because the shrinewardens kept
tending them, the crown kept the record buried, and nobody said the word
"feast" out loud. All three conditions are now failing at once: the wardens'
line is thin and poor, the realm's succession quarrel has stripped the
garrisons that guarded the old sites, and the barrow's seal has broken.

## 3. The Reckoning — the dark force

**What it is.** Not a god, not a general, not a will. The Reckoning is a
**principle of account** — the debt of the Feast, compounding, now able to
act. It has laws, not plans; it is legible and utterly unnegotiable (N30).
It cannot be bargained with, because a debt is not a person. It can only be
**paid**.

**Its laws** (discoverable in play, stated in dungeon texts and shrine lore):

1. **A debt is owed by the line, not the man.** Guilt passes down the debtor's
   line until settled. This is the law that binds the hero (§4).
2. **Like answers like.** The Reckoning manifests wherever new treachery
   *echoes* the old: an oath broken curdles the oath-breaker's well; a man
   killed in breach of faith rises and walks, patiently, toward whoever
   betrayed him; truce-banners rot on their poles overnight.
3. **Blood is a payment into the account, not against it.** Everyone killed
   in oath-breach or feud is added to the Reckoning's side. This is why
   swords cannot win: an army fighting it *feeds* it (N30, and the
   brief's "otherwise everyone slaughters each other and perishes").
4. **Only the named may testify.** The murdered cannot bring their case until
   their names are restored. Restoring names is the hero's slow weapon and
   the Reckoning's slow clock — both sides need it, for opposite reasons.
5. **The account settles once, entirely, or not at all.** Partial payment
   buys time and nothing else.

**The affliction — the rust.** A blood-colored blight on skin, grain, iron
and thatch. It marks households **in proportion to the lies inside them**:
what a family hides, it wears. It rots nets, seizes hinges, spoils stored
grain, and raises fever in those who lie most. Its social effect is worse
than its medical one: neighbors become inquisitors of each other, and the
valley's economy of small mercies (unrecorded debts, quiet kindnesses,
untold parentage) turns lethal. Common folk do not know it is the Feast's
debt. They know that a marked door means a liar lives there, and that mobs
form fast (N58 — cost shown through ordinary life: cold forges, empty nets,
a silent mill).

**Its soldiers.** The Reckoning does not spawn monsters from nothing; it
**recruits**. Its ranks are the unpaid dead — the Feast's nameless, and
every fresh corpse made in treachery since. In the field they are corpses
with purpose: they walk to their debtors, and they do not stop. Around the
barrow, older and worse things stir that the rite bound down with the dead
`[weave C — the unremembered rise faceless; the deepest of the Feast's dead
have lost so much name they no longer know whom they are owed by, and strike
anything]`. Monsters and caves are written at intent level only until the
combat grill (в19): no mechanics, no numbers, anywhere in story data.

**Escalation ladder** (advances on main-quest stage flags the player caused,
never on wall-clock or level — N3; ambient signals track the same flags —
N7). Urgency is written as dread, never as a deadline the simulation does not
enforce (N1/N2): "the marks are spreading", never "we have three days".

| Rung | Name | Signs | Gate |
|---|---|---|---|
| 0 | The Quiet | valley normal; one rotted banner nobody explains | game start |
| 1 | The Marking | rust on doors, nets, grain; first accusations | act 1 opening |
| 2 | The Collecting | the betrayed dead walk to their betrayers; the first Vaelmere death | act 1 mid |
| 3 | The Summons | the Backbarrow opens its case: the dead assemble, roads out of the valley foul | act 1 close |
| 4 | The Wider Account | the rust reaches the river towns; feud-killings across the realm arm it | act 2 |
| 5 | The Court of the Feast | the dead reconvene the feast to judge the realm; the debt falls due entire | act 3 |

## 4. The hero — bound by law, not by destiny (HARD GUARD)

The player character is a **hired hand in Vaelmere**: a poor young man who
mends nets, shores collapsed banks, carries for the ferry, and is owed money
by two people. He fights with sword and shield and can learn magic like
anyone with the patience for it. Act 1 opens with him doing paid, mundane
work (N12 — ordinariness performed, not narrated).

He is the last living descendant of **Osric Ferrant**, who lit the signal.
The line hid: **Ferrant → Ferris → Fen**. Vaelmere knows the household as
"the Fens", poor and unremarkable. He does not know why the name changed
twice; his surviving kin knows fragments and has reasons not to say.

**The binding, stated exactly (N13 guard — read this before writing any
line of dialog):**

- He is bound by an **account**, a legal-ritual instrument: the Reckoning's
  first law says a debt is settled only by the debtor's line, and he is the
  only one of that line still breathing. The dead do not strike him; they
  **stop and wait**, the way a court waits for the defendant to be seated.
- He is **not chosen, not destined, not prophesied, not special of blood**.
  Any living Ferrant would serve identically. He is not stronger, luckier, or
  magically gifted by ancestry. His power comes from what the player builds
  with the sword, shield and spells in his hands (N21).
- The debt is an **inheritance, like a mortgage on a house** — not a mantle.
  If he refuses, nothing selects a replacement; the account simply goes
  unpaid and the realm dies of it. That is the whole of the pressure.
- **BANNED STRINGS** in all data, dialog, journal and lore text (English and
  Russian): *chosen, chosen one, the Chosen, destined, destiny, fate, fated,
  prophecy, prophesied, foretold, born to, blood of heroes, the blood
  remembers, ancient blood, избранный, предназначение, судьба (as a
  designating force), пророчество, предречено.* Use instead: **the account,
  the debt, the debtor's line, standing (legal), the last of the line,
  what he owes, what is owed to him.** Enemies and allies alike speak of
  him as a *defendant* or a *payer*, never as a savior — the one exception
  being the bard's future-perfect line, once per act (N20).
- **No formal power ever** (N16): the main quest grants no titles, no
  lordship, no faction leadership, no lands. Rewards are trust, access, gear,
  knowledge, and songs.

**His doom (the epoch-hero shape the brief demands).** To settle the account
he must restore the murdered's names — and to do that he must publicly
restore his own family's true name **to the rolls of the guilty**. The
Reckoning's payment is confession, and his confession is the price. The realm
will remember the year the dead rose and the alliance that answered it; the
records that survive will name the butcher's heir who paid. Songs of this
epoch will be sung — his line in them is the one that goes untrue or unsung
(N18: each act takes something small and permanent from him; N20: the world
notices sparingly, and gets him wrong).

**Refusal beat** (N14, act 1): he takes the truth to the one authority that
should own it — Lord Rhys Corvane — who buries it, and nearly buries him.
Delegation tried, delegation closed.

## 5. The human strife (the second front)

Per the brief: the great battle cannot be fought until the ordinary human
war is dealt with, or everyone kills each other over power and dies of the
affliction. All three factions are locally represented in act 1 by envoys,
agents and tenants — never their full apparatus (N31).

**Clan Maddren** — descendants of the butchered clans. They keep a
memorized **roll of their dead** (the only surviving copy of the names the
rite stripped; recited, never written, precisely so the crown could not burn
it). Barred from arms and from most trades; poor; patient for four
generations and no longer patient.
*Right:* everything they say happened, happened; and their roll is the single
most valuable object in the war.
*Wrong:* their young men have begun collecting the debt themselves, in
Corvane children, one for one, by the old law — which is treachery answering
treachery, i.e. payment into the Reckoning's account, not against it.

**House Corvane** — descendants of the swords that did the killing, now the
valley's minor gentry; landlords of half of Vaelmere's fields and boats.
Young **Lord Rhys Corvane** was raised on the lie that the Feast was a
battle.
*Right:* the living committed none of it; on learning the truth, Rhys
genuinely tries to make amends — remitting rents, funding the shrine,
protecting Maddren tenants from mobs.
*Wrong:* amends without confession. He will pay any price except the public
truth, because the public truth unmakes his house, his title, and his
tenants' livelihoods with it. His silence is exactly the lie the rust reads.

### 5.1 The Ward — House Corvane's seat (the castle)

User requirement (relayed by the lead): the world has a **seat of state
power, present in the minimal version**. Under this arc the native answer is
House Corvane's castle **in the valley**; the crown's capital is referenced
from act 1 and reached later, never built now. **Geography — siting and the
landmark-hierarchy question (castle vs Ravenscar Crag as the L0 skyline
owner) is design's ruling and binding on this document; the fiction below is
written to survive either outcome.**

**What it is.** **Harrowward**, called simply **the Ward** — a small stone
hall-castle: curtain wall, gatehouse, hall, solar, chapel, muniment room,
kennels, a granary and a tithe-yard. Not a royal fortress; a gentry seat.
It was raised **by the first Corvane lord with the crown's grant, in the
years right after the Feast**, and its charter says it exists "to ward the
barrow and keep the peace of Harrowmere". Both halves are true and neither
is honest: it sits on the massacre's land to keep watch on the evidence, and
its endowment is the murdered clans' fields. The Ward is the crime's
beneficiary, built in the shape of a promise to guard it.

**Who holds it.** **Lord Rhys Corvane**, young — early twenties, three years
into the seat after his father's death. Raised on the family lie that the
Feast was a battle, and on the family duty to be *better than the rumors*.
He is genuinely decent and genuinely trapped: remits rents in bad seasons,
funds the shrine's lamps, shields Maddren tenants from mobs — and will pay
any price except the public truth, because the public truth voids his
charter, his title and his tenants' tenancies together. He is the human face
of the castle and **the act-1 keystone of the chain of debts** (§7): the
first alliance link the player can close truly, transactionally or brutally,
and the one that teaches the lattice's grammar. His arc across three acts is
the slow, costly journey from "amends without confession" to standing up at
the Court of the Feast and saying what his house did — or refusing, and being
collected.

**Who serves in it** (cast detail lives in `CHARACTERS_VAELMERE.md`; roles
fixed here): a **steward/castellan** who runs rents, tithes and the
petition-day list, and who is the player's real gatekeeper; a **gate-serjeant**
with a handful of retainers (the valley has no garrison — the succession
quarrel took it, which is exactly why the wards failed); a **chaplain** of
the young reforming faith, uneasy with the knoll-shrine's old rites
`[weave B]`; the **dowager** — Rhys's grandmother, who **knows the truth
entire** and has spent her life ensuring he did not; and a resident
**crown clerk**, styled an archivist, who is the **Quiet Hand's** presence
without a single soldier: he reads the muniment room's charters "for the
treasury", writes letters south, and quietly decides which testimony ever
leaves the valley.

**How the Quiet Hand holds it without making it a fortress.** The Hand's
grip is *documentary and social*, not military: one clerk, the lord's own
fear, and the threat of a border treaty. The castle is therefore a **hub the
player walks into**, not a wall to be stormed. Access rules (design/sim
implementable, no new NpcAction needed):

- **Petition day** (a recurring open-hall day) — the front door: anyone may
  enter the hall to bring grievances, dues, and disputes. This is the
  player's normal way in, and act 1's political theatre happens here in
  public, in front of tenants.
- **Business access** — delivering the fishing tithe, carting, mending,
  carrying for the ferry: the craft economy (в17) is the player's second key,
  and gets him into the yard, granary and kitchens on ordinary days.
- **Invitation** — after he becomes interesting to Rhys (or to the clerk).
- **Trespass** — the muniment room and solar are closed; entering them off
  the record is intrigue, not assault: keys, distraction, the dowager's
  indulgence, or the chaplain's conscience. Being caught means **eviction,
  disgrace and a closed door for a stage**, never a dungeon-and-execution
  dead end (N39: the failure branch is reactivity, not a wall).

**What the player can DO there in act 1:** petition (rents, rust-marked
doors, a missing Maddren boy); pay or dodge the fishing tithe; give
testimony — and watch what happens to it; witness the lord's justice in a
feud case he is trying to settle honestly and cannot; be recruited by the
clerk as a pair of useful hands; read what he should not in the muniment
room (the crown grant that names the sworn-sword **Osric Ferrant** — the
hero's blood, in a charter, in the castle's own strongbox); take the truth to
Rhys and be buried for it (the **Refusal beat**, N14); and — if he plays it
truly — leave with the first ally in the lattice.

**Why the castle earns its build cost (story asset, not scenery):** it puts
the beneficiary of the crime a walk away from the crime; it gives act 1 a
*second* kind of space (interiors, ranks, public audience) against the
hamlet's mud and water; it hosts the intrigue the brief asks for at valley
scale; it is the natural act-3 muster point and, if the finale needs a place
for the living to stand while the dead convene, the Ward is that place.

**The Crown's Quiet Hand** — royal agents whose standing instruction, king
after king, is that the Feast stays buried: the treaty that followed it holds
the northern border and legitimizes half the realm's titles, and the realm is
mid-succession-quarrel `[weave C — a distant, unresolved succession keeps
garrisons stripped and archives burning; it is the pressure behind the Hand,
not a second apocalypse (N8)]`.
*Right:* they are not lying about the consequences — the truth spoken now
cracks a border treaty during a dynastic crisis, and people will die of it.
*Wrong:* they have graduated from suppressing documents to suppressing
witnesses, and the suppression is itself fresh treachery — every silenced
witness arms the thing they are trying to contain.

**How the strife feeds the darkness (mechanically, not metaphorically):**
every revenge killing re-performs the original treachery and wakes more of
the barrow (law 2 + law 3); every suppressed testimony and every silenced
witness keeps the murdered nameless (law 4), which is the Reckoning's own
food supply as much as the hero's obstacle. Politics is the war, not a
distraction from it.

## 6. Structure — three acts

Scope v1 (в18): 3 acts, 8–12 hand-authored main quests, 6–10 hand-authored
secondaries. Act gates are explicit stage flags, never level or elapsed time,
and each gate ends with the quest-giver **explicitly releasing the player**,
naming why the next step waits on him (N4/N5).

- **Act 1 — The Valley (built testbed).** The seal fails; the hero learns the
  Feast happened, that the valley is the crime scene, and finally whose blood
  he is. Closes at the Ravenscar tower archive. Detail: `ACT1_VALLEY.md`.
  Release line at the gate: the account cannot be settled here — the roll of
  names is elsewhere, the courts are elsewhere, and he needs both.
- **Act 2 — The River and the Court.** Out of the valley: river towns under
  the rust, the Maddren roll and its keepers, a lords' assembly at a
  river-castle, and the Quiet Hand moving from documents to knives. The hero
  gathers the chain of debts (§7) and learns what the crown will do to keep
  the Feast buried. Ends with the account's terms known and the alliance
  half-built, at a cost.
- **Act 3 — The Court of the Feast.** The debt falls due. The dead reconvene
  the feast in the Backbarrow and Harrowmere Hall to judge the realm. The
  finale is a **trial that is also a battle**: the assembled allies hold the
  valley and the hall while the names are read and the confession made; every
  ally flag buys an authored beat, every missing one costs one (N26/N27).
  The hero pays: the true name goes onto the rolls of the guilty, and the
  ward is relit — and thereafter must be tended, by someone, forever
  `[weave B — the small dark post nobody sings about]`.

The valley is act 1 and act 3's stage: the game returns to the built world
for the finale, which is also why act 1 must plant every location the finale
spends.

## 7. Ally-gathering — the Chain of Debts

Nobody joins for the greater good (N24). Each keystone ally names a price
held by someone else, so the alliance is a **lattice of debts** the player
walks: the shrinewardens need their naming-rites roll back from a crown
vault; the vault's clerk needs a Corvane favor; Corvane needs the Maddren
feud stopped; the Maddrens need their dead named — which needs the
shrinewardens. Every link closes one of three ways, recorded as world flags,
not parallel quest chains (N35):

| Method | How | Finale behavior |
|---|---|---|
| **Truly** | resolve the underlying grievance (confession, restitution, naming) | the ally holds; authored beat pays off at full value |
| **Transactionally** | pay, trade, trick, leverage | the ally arrives with terms; the beat pays off diminished, with a barbed line |
| **Brutally** | steal, coerce, kill, blackmail | the ally arrives — and breaks when the Reckoning offers a better price (it always does, being made of treachery) |

Two-tier commitment per ally (N25): a **recruitment** flag ("they will
listen") and an optional **settlement** flag ("they will hold"). Cap the
roster at what the finale can individually showcase — **5–7 keystones**, each
with at least one authored line and one visible action in the final battle
(N27); an ally without a payoff beat is filler and is cut.

Skipped or failed links do not merely subtract: the Reckoning takes the
unresolved strife and runs with it. Unreconciled Maddren avengers arrive at
the Court of the Feast **as claimants against the player's own side**; a
brutalized ally's grievance becomes another entry in the account.

**The thesis, in mechanics:** the dark loses only where human debts were
settled truly, because every falsely closed link is a door it already owns.

## 8. Themes and tone rules

- **Confession is the weapon.** Every major beat asks someone to say out loud
  what their house has hidden. The game's courage is verbal as often as
  martial.
- **The small pays for the great.** The crown's crime is settled by a hired
  hand and a hamlet; the mighty mostly obstruct (LotR's shape, GoT's cast).
- **Hope is small and human** (N57): the catch comes in, a marriage across
  the feud holds, the shrine's lamps are lit, a child is named. One authored
  hope spot per act, placed immediately before the act's darkest beat (N52);
  after each dark climax, a quiet recovery scene in the hamlet (N56).
- **Gallows humor belongs to those who earn it** (N53) — the ferryman, the
  gravedigger, old soldiers. No jokes in climax nodes, none in the
  Reckoning's own scenes.
- **Never nullify a player victory off-screen** (N54): outcomes may be
  complicated later, never retroactively stomped.
- **Settlement/template quests stay mostly solvable with a good outcome**
  (N55); tragedy is concentrated in hand-authored content where it is earned.

## 9. Naming and text conventions

- **Sound:** Anglo-Norse, hard consonants, place-names from landscape and
  work (Vaelmere, Ravenscar, Harrowmere Hall, the Hiding, Coldharbour).
  Clans: single hard surnames (Maddren, Corvane, Ferrant, Fen). Avoid
  apostrophes and invented diacritics entirely.
- **Registers:** hamlet folk speak plainly, short sentences, work-metaphors
  (nets, weather, weight, rot). Gentry speak in obligations and titles.
  Quiet Hand agents speak in the passive voice and never in the first person
  plural. The dead, when they speak at all, speak in **legal formulae** —
  claim, standing, witness, term, forfeit — never in threats.
- **Never write the Reckoning as a character**: no "it wants", no "it
  smiles". Write it as a case proceeding.
- **All user-facing text lives in localization files as keys** (Rules 5–6,
  Q59): quest/dialog/card data reference `quest.*`, `dlg.*`, `npc.*` keys —
  a literal player-visible string in data or C++ is a violation.
- Content paths (lead's ruling): `games/daggerfall_n/assets/quests/`,
  `.../dialogs/`, `.../npcs/cards/`, `.../world_flags.json`.
- **LLM prompt language** (lead's ruling): English prompts with an explicit
  output-language instruction; character-card persona fields are written in
  English (QUEST_FORMAT §6).

## 10. Open questions (for the user / the lead)

1. **Player character voice:** silent protagonist with authored choice text
   (recommended — doubt carried by the cast per N19), or spoken lines?
2. **Magic in the world's eyes:** is spellcraft common and licensed, or
   suspect? Affects how the hamlet reacts to a hired hand who casts.
3. **Act 2's geography** needs design's zone (a river town, a castle) —
   story will file a request once act 1 data is walking in engine.
4. **Combat grill (в19)** gates every monster and battle detail here; this
   bible deliberately stops at intent level.
5. Region-scale succession detail `[weave C]` is currently pressure, not
   plot; if the user wants it foregrounded, it becomes act 2's spine and
   needs its own pass.
