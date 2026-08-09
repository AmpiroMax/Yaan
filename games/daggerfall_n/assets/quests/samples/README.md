<!--
Created: 09:08:2026 - 19:00:00
Last updated: 09:08:2026 - 19:00:00
-->
<!--
UPD:
- 09:08:2026 - 19:00:00: Created parser test fixtures for core's JSON reader:
  one deeply nested quest, one dialogue graph, a world_flags fragment, a
  number-typing fixture, and three one-defect-each invalid files.
-->

# Parser fixtures — story → core

Owner: `story`. Consumer: `core` (JSON reader in `engine/core/serialization`),
requested jointly by story and sim on 09:08:2026.

These are **fixtures, not shipping content**. They carry the shapes story
actually emits, so core's reader is tested against real structure instead of
invented examples. The format they express is contract-frozen in
`docs/story/QUEST_FORMAT.md` (§2.1 condition atoms, §2.2 effects, §4 dialogue
graphs); the fiction they reference is canon from `docs/story/BIBLE.md` and
`docs/story/ACT1_VALLEY.md`.

Core is welcome to copy these into `tests/` fixtures; if they move, ping
story so this README stops claiming to own them.

## Valid fixtures

| File | Exercises |
|---|---|
| `quest.vaelmere.cave_children.quest.json` | The deep case: quest → states → transitions → `when{all_of, any_of}` → condition atoms (5 levels), arrays of objects at three levels, `on_enter` effect arrays, a nested `npc_action` and `set_topic` effect object, quest-local `vars`, a failure state (`s_marks_lost`) that loops back. Every atom and operator used is from the frozen closed set. |
| `../../dialogs/samples/dlg.vaelmere.shrine_keeper.dlg.json` | Dialogue graph: `nodes` map, choice arrays with per-choice `if` condition arrays, auto-advance `next` nodes, named `exit` nodes (the ones quests listen for via `dialog_exit`). |
| `world_flags.fragment.json` | The registry shape — a long flat array of small objects. The real file grows to hundreds/low thousands of entries. |
| `numbers.json` | Number typing (requirement 4): integers that must stay integers, one integer beyond double's exact range (`9007199254740993`), `uint32` max, a numeric-looking **string** that must not be coerced, plus `null` and one double. The `expected` object states the assertions in prose. |

## Invalid fixtures — one defect each

Deliberately one defect per file, so error messages can be pinned precisely
rather than "the parser rejected something".

| File | Defect | What story needs from the error |
|---|---|---|
| `invalid/missing_comma.invalid.json` | comma missing after `name_key` | line/column at the offending token |
| `invalid/bad_nesting.invalid.json` | a `]` closing an object that needed `}` first, deep inside `transitions[0].when` | line/column **and node path** — this is the case where a bare line number is nearly useless, since the file is structurally plausible for 14 lines |
| `invalid/duplicate_key.invalid.json` | `"type"` declared twice in one object | an **error naming the key and both positions** |

**Note on the duplicate-key fixture — this is the important one.** It is
*valid* JSON by the letter of the spec: a stock parser (checked with
Python's `json`) accepts it silently and keeps the last value. That is
precisely the failure mode story asked core to detect: in a
`world_flags.json` of a thousand entries, a duplicated flag id would be
eaten without a word, and the quest that reads the eaten entry would fail
somewhere else entirely, months later. So this fixture must **fail** in our
reader even though it passes a conformant one — a deliberate, documented
divergence from the spec, not an oversight.

## Conventions the fixtures assume

- Ids are dot-namespaced snake_case (`quest.*`, `dlg.*`, `flag.*`, `item.*`,
  `npc.*`, `topic.*`), hashed to the frozen FNV-1a content hash at load.
- **No user-facing text anywhere** (Rules 5–6): every player-visible string
  is a localization key. `comment` and `_purpose` fields are author notes,
  never displayed.
- The JSON subset story emits: objects, arrays, strings, ints, bools, null.
  No comments, no trailing commas; floats are not required by story data.
