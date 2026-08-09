<!--
Created: 09:08:2026 - 14:03:03
Last updated: 09:08:2026 - 14:03:03
-->
<!--
UPD:
- 09:08:2026 - 14:03:03: Created narrative-design research pass: actionable rules for main-quest pacing, doom-hero arcs, faction/ally design, quest branching, hybrid quest generation, dark tone.
-->

# Narrative Design Research — Actionable Rules

Rules are imperative and checkable. Cite by ID (N1, N2, ...). Grounded in project constraints: hand-authored main + secondaries, template settlement quests keyed to local craft, data-driven quest state machines with world-flag registry and journal, act 1 in a small valley testbed, sword+shield+magic hero, LLM voices incidental townsfolk only.

## 1. Main-Quest Pacing vs Open Exploration

- **N1.** Never write countdown language ("before it's too late", "we have days") into a main-quest step unless a real in-game timer or state change enforces it.
  Why: stated urgency the simulation ignores is the most cited open-world dissonance (Skyrim's Alduin). Source: ResetEra open-world pacing threads; ludonarrative-dissonance essay (Medium).
- **N2.** Phrase main-quest urgency as dread, not deadline: "the signs are worsening" (scales with player-triggered flags), never "the attack comes at dawn".
  Why: dread survives 40 hours of wandering; deadlines rot on the vine. Source: Witcher 3 analyses — urgent narrative "not laden with time bombs".
- **N3.** Escalate the threat only on main-quest stage transitions the player caused, never on wall-clock or play-hours; ambient decoration (omens, refugees, prices) may track the same flags.
  Why: player-caused escalation makes "the world held still" a feature, not a bug. Source: ResetEra structure threads; UESP quest docs.
- **N4.** After every act-gate quest, have the quest-giver explicitly release the player, naming a reason the next step waits on them ("learn the valley — you'll need friends here").
  Why: Morrowind's Caius Cosades ("familiarize yourself with the locals") is the canonical fix for guilt-free exploration. Source: UESP Morrowind main quest; Fandom Caius Cosades.
- **N5.** Gate acts by explicit stage flags, never by player level or elapsed time; each act boundary is a single bottleneck quest whose closing dialog restates the stakes.
  Why: players returning after 20 hours need the story re-armed at the gate. Source: TV Tropes Branch-and-Bottleneck; CreationKit stage docs.
- **N6.** Write every main-quest journal entry to read correctly after a long absence: restate who wants what and why, never a bare "return to X".
  Why: the journal is the wanderer's re-entry point; only the latest entry is prominent. Source: CreationKit log-entry docs.
- **N7.** Per act escalation, change at least two cheap ambient signals in already-visited spaces (a guard line, a boarded door, an omen at the crag).
  Why: if nothing visible changes, escalation is only words — the core Skyrim criticism. Source: ResetEra quest-design overhaul thread.
- **N8.** Do not let any side or settlement quest outrank the main quest's current stakes; side content may be personal, never apocalyptic.
  Why: competing apocalypses (Skyrim's civil war vs Alduin) split urgency and cheapen both — anti-pattern. Source: ResetEra story-driven-RPG thread; theanimeelitist Skyrim essay.
- **N9.** Run at most one urgent authored thread at a time; if a secondary claims urgency, put a real short timer on it and keep it local.
  Why: opt-in, enforced micro-urgency spices exploration without contradicting N1. Source: Morrowind quest-timing docs (UESP); ludonarrative-dissonance essay.
- **N10.** Main-quest hooks must pull, not push: rumors, letters, and sightlines invite the next stage; never auto-start a main stage from a trigger volume hit mid-exploration.
  Why: a pushed stage interrupts the player's own story and breeds resentment of the critical path. Source: ResetEra content-pacing thread; Skyrim "forces you into it" mod rationale (Nexus: At Your Own Pace / Not So Fast).
- **N11.** In the act-1 valley, place the first main-quest beat within sight of the hamlet and the act-1 climax at the crag tower; the critical path must be walkable in minutes.
  Why: pacing should die from player choice, never from travel — density beats area. Source: ResetEra content-pacing thread; project constraint.

## 2. Doom-Driven / Reluctant Hero Arc

- **N12.** Establish the hero's ordinariness mechanically before the inciting event: act 1 opens with mundane craft-scale tasks (settlement templates double as this), not combat glory.
  Why: ordinariness the player performed is credible; ordinariness narrated is not. Source: Bilbo/Frodo reluctant-hero analyses (dummies.com; fantasy-faction).
- **N13.** The hero is chosen by position, not prophecy — the one standing there when it happened. Ban "chosen one", "destined", and bloodline from all dialog and journal text.
  Why: circumstance keeps the brief's "unimportant before, unimportant after" intact. Source: Wikipedia/Grokipedia reluctant hero; LotR Frodo analyses.
- **N14.** Write one explicit Refusal beat: an early stage where the hero tries to hand the burden to an authority — and the authority fails or refuses.
  Why: the rise is credible only if delegation was tried and closed off (Frodo offers the Ring to Gandalf and Galadriel). Source: thewritepractice.com Call/Refusal; Campbell monomyth summaries.
- **N15.** Have NPCs of rank underestimate the hero on first meeting in every act, softening one act later, via conditional dialog on reputation/act flags.
  Why: the world updating its opinion is the rise; instant respect deletes the arc. Source: fantasy-faction "A Different Kind of Hero".
- **N16.** Never grant the hero formal power: no titles, lordships, or faction leadership from the main quest — rewards are trust, songs, and access.
  Why: "unimportant after" is a hard brief constraint; Frodo returns to the Shire diminished, not crowned. Source: reactormag Frodo essay; project brief.
- **N17.** Remove or sideline the mentor at the act 2/3 boundary (death, departure, or discrediting) via a flagged stage.
  Why: the rise completes only when no one more qualified remains. Source: gamedeveloper.com Hero's Journey in games; kindlepreneur 12 stages.
- **N18.** Each act, the main quest permanently takes something small and personal from the hero or their circle (an item, a trust, a comfort), recorded as world flags.
  Why: doom reads as doom only if the ledger visibly runs negative (Frodo's wound never heals). Source: dummies.com Frodo as unwitting hero.
- **N19.** Give the doubt to the supporting cast: named NPCs voice "why you?" and "you'll die out there", so reluctance exists in the world even with a silent player-hero.
  Why: a first-person RPG hero cannot monologue doubt; the cast must carry it. Source: reluctant-hero archetype analyses; pcwrede driven-vs-reluctant heroes.
- **N20.** Foreshadow "songs will be sung" diegetically and sparingly: at most one bard/rumor line per act, authored, future-perfect flavored ("they'll tell of this").
  Why: it must feel like the world noticing, not the game flattering the player. Source: project brief; reluctant-hero design discussions.
- **N21.** Keep the hero's competence inside the sword+shield+magic kit the player has; never cutscene-grant powers the mechanics don't.
  Why: credibility of the rise = the player did it with the tools on their back. Source: project constraint; ludonarrative-dissonance essay.

## 3. Factions United Against a Mystical Threat

- **N22.** Before writing any faction quest, write the faction's one-line "why they're right" and "why they're wrong"; both must surface in authored dialog.
  Why: GoT's throne war works because no side is merely evil — and all are wrong about what matters. Source: Den of Geek / wethrones White Walker analyses.
- **N23.** Establish the mystical threat to the player (seen in a dungeon, at the tower) acts before the factions accept it; the unite-them arc is the player closing that knowledge gap.
  Why: if everyone already believes, there is no act 2. Source: GoT White Walkers vs politics analyses (Hypebeast; Collider).
- **N24.** Price each faction's alliance in its own currency: it joins only after a quest resolving or suspending its human grievance, never after being shown proof alone.
  Why: evidence persuades nobody whose interest points elsewhere — GoT's core observation. Source: wethrones essay; winteriscoming.net.
- **N25.** Structure ally-gathering as ME2 loyalty missions: one recruitment quest (they'll listen) plus one optional loyalty quest (they'll hold), each setting a distinct world flag.
  Why: two-tier commitment gives the finale cheap, high-value branching inputs. Source: TheGamer / Wikipedia ME2 Suicide Mission analyses.
- **N26.** Pay every alliance flag off visibly in the finale: each flag buys a concrete beat (a gate held, a named rescue) and each missing flag costs one — resolved by flag checks, not a stat sum.
  Why: presence/absence beats arithmetic (ME2 Suicide Mission, Kaer Morhen). Source: RPGSite ME2 choices; Witcher Fandom Battle of Kaer Morhen / Full Crew.
- **N27.** Cap recruitable allies at what the finale can individually showcase (5-7); every ally gets at least one authored line and one visible action in the final battle.
  Why: an ally without a payoff beat is a filler quest — banned by brief. Source: Kaer Morhen Full Crew design; project constraint.
- **N28.** Never fully dissolve the human conflict before the finale: factions cooperate under visible strain (barbed lines between rivals in shared scenes) and at most suspend — not settle — their war.
  Why: instant harmony retroactively cheapens act 1-2 strife; GoT's rushed resolution is the anti-pattern. Source: winteriscoming.net "deserved a better story".
- **N29.** If the player can side with one faction in the human conflict, that choice recolors finale dialog and epilogue but never blocks the unite-against-threat spine (foldback, not fork).
  Why: keeps faction choice meaningful at foldback cost, not double-campaign cost. Source: TV Tropes Branch-and-Bottleneck (Witcher 2 as the expensive exception).
- **N30.** Keep the threat unsympathetic but legible: rules, signs, and an origin discoverable in dungeon texts — never a negotiation scene — and make the finale spend that dug-up knowledge.
  Why: knowable-but-alien unifies factions without stealing their moral spotlight; GoT's late "knife trick" trivialization is the anti-pattern. Source: Den of Geek; Collider; winteriscoming.net.
- **N31.** Represent each faction in the act-1 valley by a small envoy, agent, or outpost — never its full apparatus — and stage faction scenes at valley landmarks.
  Why: keeps grand strife affordable in the testbed while seeding later acts. Source: project constraint (valley testbed); GoT model of distant war felt through local agents.

## 4. Quest Branching for Data-Driven State Machines

- **N32.** Author every quest as numbered stages in tens (10, 20 ... 200) with gaps for insertion; one stage = one journal-visible change of situation.
  Why: the proven Bethesda model, and it maps 1:1 onto our state machines. Source: CreationKit quest tutorials; UESP.
- **N33.** Each stage sets exactly: journal entry, active objective(s), and zero or more world flags — nothing else may touch world state.
  Why: a single write-path keeps the flag registry auditable. Source: CreationKit Quest Objectives / Scripting docs.
- **N34.** Journal entries append per stage and never rewrite history; write each assuming earlier entries were forgotten (see N6).
  Why: matches Bethesda's log model and our journal spec. Source: CreationKit log-entry docs.
- **N35.** Branch with foldback: diverge on choice, reconverge within 1-2 stages at a bottleneck, and record the choice as a world flag instead of parallel stage chains. Reserve full splits for at most 2-3 pivotal main-quest moments.
  Why: branching cost stays linear while choices stay recorded. Source: heterogenoustasks Standard Patterns; TV Tropes Story Branching.
- **N36.** Prefer delayed consequences: a choice flag set in act 1 should preferentially fire in act 2-3 dialog conditions or quest variants.
  Why: delay converts one cheap flag into perceived deep reactivity and defeats save-scumming (Witcher 3's signature move). Source: pominis / erma branching analyses of Witcher 3.
- **N37.** Structure each act as hub-and-spoke: one hub quest exposes 2-4 spokes completable in any order; the hub's gate stage requires N-of-M spoke flags.
  Why: order-independence is what lets exploration and story coexist (see N3-N4). Source: ME2 structure; branch-and-bottleneck literature.
- **N38.** Every world flag gets a registry entry at authoring time: name, setter (quest.stage), and all known readers; a flag with no reader is deleted or given one before ship.
  Why: unread flags are the data-driven equivalent of filler. Source: CreationKit designer-notes practice; project world-flag registry.
- **N39.** For every stage, define the failure/interference transition (target dies, item lost, faction hostile) — minimum: quest fails, flag set, one NPC comments.
  Why: open worlds guarantee broken quests; an unhandled break is a bug, a handled one is reactivity. Source: UESP quest docs; Daggerfall Workshop quest-debugging forums.
- **N40.** Condition dialog on (quest, stage, flags) and advance stages only via explicit dialog actions; never duplicate quest logic inside dialog trees.
  Why: keeps the fixed/authored dialog layer a pure view over quest state — required by our LLM-only-for-incidentals rule. Source: CreationKit dialogue-conditioning model; project constraint.
- **N41.** Ship a machine-checkable path per quest: enumerate stage transitions from start to every ending and walk the state machine in CI to detect dead ends and orphan stages.
  Why: data-driven quests are graphs; graphs are testable — Daggerfall's untested generated quests were famously breakable. Source: Daggerfall quest-debugging history; UESP quest-hacking guide.
- **N42.** Show one active objective per quest at a time; multiple simultaneous objectives are allowed only in hub N-of-M quests.
  Why: objective sprawl is how journals become to-do lists and stakes evaporate. Source: CreationKit objectives docs; open-world quest-log criticism (ResetEra).

## 5. Hybrid Procedural + Hand-Authored Quests

- **N43.** Build settlement templates on the Daggerfall model: a template = text resources (offer/refuse/accept/fail/complete) + a data block of typed slots (persons, places, items, clocks, tasks/flags).
  Why: separating text from logic lets writers and the quest runtime iterate independently. Source: UESP Daggerfall quest-hacking guide; dfworkshop Template docs.
- **N44.** Fill template slots only from the settlement's authored register (its craft, named NPCs, landmarks, current flags) — never from global random pools.
  Why: Radiant felt generic precisely because slot-fills ignored local context; craft-keyed settlements are the fix. Source: terminally-incoherent Radiant critique; UESP Skyrim:Radiant; project plan.
- **N45.** Every template names its stakes in craft terms — the miller's quest is about grain, flood, or millstone; write one template per settlement-craft pair before any generic fetch/kill template.
  Why: craft-specific verbs are what make a generated quest read as authored. Source: Radiant criticism — "no actual stories or characters to it".
- **N46.** Keep generated quests structurally simple: 2-4 stages, one twist slot maximum; reserve multi-stage drama for hand-authored content.
  Why: procedural systems cannot sustain complex arcs; pretending otherwise is how Radiant broke. Source: labjogos Radiant thesis; fredzed "Is random generation worth it".
- **N47.** Cap each template at 1-2 issues per settlement, then retire or reskin via a different twist slot; track issuance in the flag registry.
  Why: Radiant's staleness came from unlimited reissue against a small target pool. Source: UESP Skyrim:Radiant; community criticism.
- **N48.** Let template quests read act flags: offer text varies by act, and late-act variants reference the threat (one conditional text swap per template minimum).
  Why: cheap act-awareness kills the "parallel universe side content" feel. Source: Radiant-improvement research (chained, coherent generation).
- **N49.** Route all template text through authored string tables with slot substitution; the LLM voices incidental townsfolk within character cards only and never generates quest-critical text, objectives, or journal entries.
  Why: quest text must be testable and canon-safe. Source: project constraint; Radiant stock-dialogue criticism.
- **N50.** Give every template an authored hook line explaining why this giver, now ("the ford flooded, and the ore wagons with it"); never deliver via note-with-map-marker.
  Why: Radiant's noteboard delivery is the canonical grounding anti-pattern. Source: terminally-incoherent Radiant critique.
- **N51.** Dry-run every template against every settlement register at build time; a slot that cannot fill fails the build — no silent fallback to generic filler.
  Why: fallback fills are how "the blacksmith wants 10 bear pelts" ships; fail loud instead. Source: Daggerfall quest-compiling docs (dfworkshop); Radiant criticism.

## 6. Dark Tone Management

- **N52.** Schedule at least one hope spot per act — a small authored scene of warmth (a festival, a rescue that sticks, a craft completed) — placed immediately before the act's darkest beat.
  Why: contrast makes grimness land; relentless misery numbs. Source: golemproductions grimdark essay; sanddancer dark-fantasy guide.
- **N53.** Ration gallows humor to characters it belongs to (soldiers, gravediggers, healers) via their character cards; ban jokes in main-quest climax nodes and in the threat's own scenes.
  Why: dry in-world humor vents pressure; authorial jokes break dread. Source: dabblewriter grimdark guide.
- **N54.** Never nullify a player victory off-screen: a completed quest's good outcome may be complicated later (see N36) but not retroactively stomped.
  Why: agency is the antidote to grimdark fatigue. Source: golemproductions grimdark essay.
- **N55.** Keep settlement template quests predominantly solvable with a good outcome; concentrate tragedy in hand-authored content where it can be earned.
  Why: randomly distributed bleakness reads as authorial cruelty, not a dark world. Source: TTRPG tone-calibration guides (gooeycube; ttrpglexicon).
- **N56.** After each act's dark climax, open the next act with a quiet recovery scene in the hamlet before new stakes arrive.
  Why: grimness is paced as tension-release cycles, not a flat descent. Source: sanddancer pacing guidance.
- **N57.** Anchor hope in the small and human — the craft economy, named NPCs, songs — never in promised cosmic salvation.
  Why: the big picture stays dark and uncertain; the small picture rewards care and keeps play worthwhile. Source: grimdark hope analyses; project brief.
- **N58.** Express the threat's cost through craft disruption (the forge cold, the nets empty, the mill silent), not gore quotas.
  Why: dark = consequences for ordinary life the player has touched; it also reuses the settlement-craft registers we already author. Source: sanddancer immersive-dark-settings guide; project plan.

## Sources

- https://www.resetera.com/threads/how-to-solve-the-open-world-content-pacing-problem.18175/
- https://www.resetera.com/threads/an-open-world-structure-is-not-good-for-a-story-driven-rpg-do-you-agree-with-this.1111053/
- https://www.resetera.com/threads/open-world-quest-design-needs-a-serious-overhaul.56175/
- https://medium.com/@gatherer286/ludonarrative-dissonance-or-why-i-almost-never-do-all-of-the-side-content-in-games-anymore-b3e9778db8fc
- https://www.nexusmods.com/skyrimspecialedition/mods/2475
- https://en.uesp.net/wiki/Morrowind:Report_to_Caius_Cosades
- https://en.uesp.net/wiki/Morrowind:Quest_Timing
- https://elderscrolls.fandom.com/wiki/Caius_Cosades_(Morrowind)
- https://en.wikipedia.org/wiki/Reluctant_hero
- https://www.dummies.com/article/academics-the-arts/language-language-arts/literature/examining-frodo-as-the-unwitting-hero-of-tolkiens-middle-earth-200406/
- https://fantasy-faction.com/2016/a-different-kind-of-hero
- https://reactormag.com/the-bane-of-banality-frodo-baggins/
- https://pcwrede.com/pcw-wp/driven-vs-reluctant-heroes/
- https://thewritepractice.com/call-to-adventure/
- https://www.gamedeveloper.com/design/how-games-take-the-player-through-the-hero-s-journey-part-1-departure
- https://www.denofgeek.com/books/game-of-thrones-sympathy-for-the-white-walkers/
- https://wethrones.medium.com/a-darkness-that-will-swallow-the-dawn-what-was-the-purpose-of-the-white-walkers-in-game-of-thrones-d1d9f58b2c0
- https://winteriscoming.net/the-white-walkers-deserved-better-story-game-of-thrones
- https://collider.com/game-of-thrones-white-walkers-explained/
- https://hypebeast.com/2017/7/game-of-thrones-white-walkers-theory
- https://www.thegamer.com/mass-effect-2-suicide-mission-characters-choices-archetypes/
- https://en.wikipedia.org/wiki/Suicide_Mission_(Mass_Effect_2)
- https://www.rpgsite.net/feature/11118-mass-effect-2-suicide-mission-choices-how-to-make-sure-everyone-lives-to-get-the-best-ending
- https://witcher.fandom.com/wiki/The_Battle_of_Kaer_Morhen
- https://witcher.fandom.com/wiki/Full_Crew
- https://ck.uesp.net/wiki/Bethesda_Tutorial_Planning_the_Quest
- https://ck.uesp.net/wiki/Bethesda_Tutorial_Quest_Objectives
- https://ck.uesp.net/wiki/Bethesda_Tutorial_Basic_Quest_Scripting
- https://tvtropes.org/pmwiki/pmwiki.php/Main/BranchAndBottleneckPlotStructure
- https://tvtropes.org/pmwiki/pmwiki.php/Main/StoryBranching
- https://heterogenoustasks.wordpress.com/2015/01/26/standard-patterns-in-choice-based-games/
- https://www.pominis.com/en/blog/branching-narratives-explained-designing-choices-that-matter
- https://en.uesp.net/wiki/Daggerfall_Mod:Quest_hacking_guide
- https://dfu-modding.fandom.com/wiki/Quest_Creation
- https://www.dfworkshop.net/questing-part-2-compiling/
- https://forums.dfworkshop.net/viewtopic.php?p=30309
- https://en.uesp.net/wiki/Skyrim:Radiant
- https://www.terminally-incoherent.com/blog/2011/12/16/skyrim-radiant-quest-system/
- https://labjogos.ist.utl.pt/en/thesis/procedural-quest-generation-improving-skyrim-s-radiant-story
- https://fredzed.substack.com/p/is-random-generation-worth-it
- https://golemproductions.substack.com/p/do-ttrpgs-have-a-grimdark-problem
- https://www.dabblewriter.com/articles/grimdark-fantasy
- https://sanddancer.pub/blogs/ink-shadows-blog/how-to-build-immersive-dark-fantasy-settings-that-haunt
- https://shop.gooeycube.com/blogs/news/tabletop-rpg-adventures-calibrating-tone-in-tabletop-rpgs
