
# AI Agent Instructions — Daggerfall N

Before starting ANY work in this repository, read and follow these documents, in order:

## Mandatory reading

1. **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — Hard contract. Layout, layering,
   isolation, ECS/simulation rules, file headers, naming, process rules. Rules are
   numbered; cite them by number when discussing changes.
2. **[docs/DECISIONS.md](docs/DECISIONS.md)** — What we're building and why. Every
   technology and design choice with links back to the grill session.
3. **[docs/NUMBERS.md](docs/NUMBERS.md)** — The single source of truth for numeric
   constants. Never hardcode a constant that belongs here.
4. **[rules/structure.md](rules/structure.md)**, **[rules/documentation.md](rules/documentation.md)** —
   target folder layout and documentation workflow.

## Team

| Agent | Zone (see Rule 25) | Spec |
|---|---|---|
| `core`   | `engine/core`, `engine/world` | `docs/specs/core.md` |
| `render` | `engine/platform/{window,input,render}`, `engine/render` | `docs/specs/render.md` |
| `sim`    | `engine/platform/{physics,anim,audio,llm}`, `engine/physics`, `engine/anim`, `engine/gameplay` | `docs/specs/sim.md` |
| `design` | `docs/design` — landscape/world design bible, asset briefs, placement rules | `docs/design/LANDSCAPE.md` |
| `story`  | `docs/story` — narrative bible, pitches, characters, quest/dialog authoring; `games/daggerfall_n/assets/quests` (content only — quest RUNTIME code belongs to sim) | `docs/specs/story.md` |
| lead     | `engine/app`, `engine/editor`, shared components, CMake root, `docs/`, `tools/` | — |

## Key constraints (summary — the contract has the full list)

- Follow docs/ARCHITECTURE.md strictly. If a change would violate a rule — stop and
  propose an alternative (to the lead).
- Never edit a foreign zone; message its owner instead (Rule 25).
- Public interfaces are frozen per stage; changes go through a group sync (Rule 26).
- Install nothing; dependencies only via pinned FetchContent (Rule 24).
- Three failed attempts at one problem → stop and message the lead (Rule 28).
- Game content in data files, user-facing strings in localization files — never in
  C++ (Rules 5-6).
- Every file starts with the header/UPD block; `tools/header_check.py --all` must pass.
- All code, comments, docs, commits — in English.
