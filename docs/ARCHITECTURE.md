<!--
Created: 09:08:2026 - 00:06:00
Last updated: 09:08:2026 - 21:56:40
-->
<!--
UPD:
- 09:08:2026 - 00:06:00: Created engine architecture hard contract, adapted from Quicky Engine with amendments approved in the initial-planning grill session (Q23, Q25, Q27, Q30, Q31, Q54, Q61, Q70, Q73).
- 09:08:2026 - 10:42:00: Stage 3 — added the `design` zone (landscape/world design docs) to Rule 25.
- 09:08:2026 - 21:56:40: Added Rules 30-34, all five earned by defects found in the worldgen v2 stage: controls with every test, distribution asserted not bounds, fix the mechanism not the instance, detail sized against viewing distance, never diagnose from an unchecked premise.
-->

# Architecture & Code Rules (Humans + AI Agents) — HARD CONTRACT

This document is a **hard contract** for all contributors (human and AI).

- If a change would violate a rule: **stop** and propose an alternative.
- If a rule is ambiguous: **stop** and ask for clarification before coding.

Companion documents (read all three before writing code):

- [DECISIONS.md](DECISIONS.md) — consolidated design decisions with rationale links.
- [NUMBERS.md](NUMBERS.md) — the single source of truth for every numeric constant.
- `rules/structure.md`, `rules/documentation.md` — target layout and docs workflow.

---

## Project overview

**Daggerfall N** is a first-person open-world RPG (Skyrim-like play, Daggerfall-like look) built on a custom C++ engine. Rendering via bgfx, physics via Jolt, skeletal animation via ozz-animation, audio via miniaudio, optional local LLM via llama.cpp. Custom ECS (evolved from Quicky Engine). 100% agent-written code.

### Repository layout

```
engine/                          # Reusable engine library
├── core/                        # Pure foundation — no external deps (glm exempt, see Rule 2)
│   ├── ecs/                     # World, EntityId, ComponentPool, View + batch ops
│   ├── math/                    # glm wrappers, AABB, frustum, rays
│   ├── events/                  # EventBus
│   ├── time/                    # Clock, fixed timestep
│   ├── types/                   # Compile-time TypeId, handles
│   ├── serialization/           # Section-based binary IO, hashing, JSON/TOML for content
│   ├── config/                  # EngineConfig, generated constants (from NUMBERS.md)
│   └── components/              # Shared plain-data components (lead-owned)
├── platform/                    # HAL — the ONLY place that includes third-party libs
│   ├── window/                  # IWindow  + sources/{glfw,null}
│   ├── input/                   # IInput   + sources/{glfw,null}
│   ├── render/                  # IRenderer + sources/{bgfx,null}   (interface: lead-authored)
│   ├── audio/                   # IAudio   + sources/{miniaudio,null}
│   ├── physics/                 # IPhysics + sources/{jolt,null}
│   ├── anim/                    # IAnim    + sources/{ozz,null}
│   └── llm/                     # ILlm     + sources/{llama,null}
├── render/                      # Materials, camera, meshes, post-process (low-res + palette)
├── physics/                     # Character controller, collision layers (via IPhysics)
├── anim/                        # Animation state machines, humanoid rig contract (via IAnim)
├── world/                       # Chunks, streaming, world file format, worldgen library
├── gameplay/                    # Stats, dice, combat, inventory, dialogue, quests, NPC (NpcAction)
├── editor/                      # In-game editor mode (Dear ImGui — allowed here only)
└── app/                         # Bootstrap: wires backends, runs the loop (lead-owned)

games/daggerfall_n/              # The game: content data, game-specific systems/components
    ├── assets/                  # Textures, audio, JSON/TOML content, localization, voice
    ├── docs/
    └── src/{systems,components}/

tools/                           # header_check.py, worldgen CLI, voice_gen, constants generator
rules/                           # Permanent development rules
docs/                            # ARCHITECTURE.md, DECISIONS.md, NUMBERS.md, specs/, devlog/
tests/                           # doctest suites + visual tour harness
third_party/                     # (avoid; prefer FetchContent — see Rule 24)
```

### Module layout convention

- Every module is a folder containing `docs/` and `sources/`.
- **Platform modules only** additionally contain `interfaces/` (pure virtual contract);
  their `sources/` hold backend implementations in subfolders (`bgfx/`, `null/`, ...).
- Include paths are absolute from the repo root:
  `#include "engine/core/ecs/sources/World.h"`.

### Dependency DAG (arrows = "may include")

```
app → editor → gameplay → { world, physics, anim, render } → platform interfaces → core
platform sources (backends) → third-party libraries (the only place)
```

- `core` depends on nothing (std + glm only).
- `world` depends on `core` only (pure data + generation; no physics, no rendering).
- `gameplay` never touches bgfx/Jolt/ozz/GLFW directly — only engine layers above.
- `editor` may use Dear ImGui directly (documented exception; nothing else may).
- CMake enforces this: each layer is a target; forbidden includes fail the build.

---

## Interface principle (when to create an interface)

An interface (abstract contract + multiple implementations) is created **only** for:

1. **Third-party isolation** (bgfx, Jolt, ozz, miniaudio, llama.cpp, GLFW). Mandatory.
2. **Multiple interchangeable runtime implementations.**
3. **Dependency inversion** — engine calls code whose concrete type it must not know.

Do **NOT** create interfaces for pure algorithms/data structures (ECS, math, worldgen,
serialization) or stable in-engine logic consumed by the game. The public header **is**
the contract there.

**Rule 0 — interfaces live only in `engine/platform/`.** The only modules with an
`interfaces/` folder are `platform/{window,input,render,audio,physics,anim,llm}`.

---

## Layering & isolation rules

### Rule 1 — Layered architecture
The DAG above MUST NOT be violated. `core`, `world`, `gameplay` have zero external
dependencies. Platform interfaces are header-only and pull in nothing external.
Platform `sources/` are the ONLY place with third-party `#include`s.

### Rule 2 — glm exemption
**glm is the project-wide math vocabulary and is treated as part of the standard
library.** It may appear anywhere, including `core` and platform interfaces. No other
third-party header gets this status.

### Rule 3 — Null backend mandatory
Every platform interface ships a `null/` backend, and null backends are **runnable
modes, not stubs**: headless tests run with null render; the game is fully playable
with null LLM (Q62); physics can be nulled for debugging (Q31). A feature that
crashes under a null backend is a bug.

### Rule 4 — Backend swappability
Every platform interface must be implementable by a different library without
changing engine or game code.

---

## Content & data rules

### Rule 5 — Content lives in data files, NEVER in C++
Items, stats, spells, dialogue, loot tables, quest definitions — text files (JSON/TOML)
under `games/daggerfall_n/assets/`. World geometry/heightmaps — the binary world format.
**All user-facing strings go through localization files from the first line of code
(Q59); a literal user-facing string in C++ is a violation.**

### Rule 6 — No recompilation for content
Adding content MUST NOT require recompiling.

### Rule 7 — Binary format discipline (Q49)
The world/save format is section-based: magic + version header, then tagged
length-prefixed sections; unknown sections are skipped. **Never `memcpy` whole structs
to disk. Byte order is little-endian, written explicitly.** Saves store a delta
against the generated world, never a world copy (Q56). Version migration functions
exist from day one.

---

## ECS rules

See the `core` spec (docs/specs/core.md) for the full design; base semantics inherited
from Quicky's `ECS_DESIGN.md` (sparse sets, generational `EntityId`, deferred destroy).

### Rule 8 — Components are plain data
Plain structs, public fields. No virtual methods, no inheritance, no backend headers,
no pointers to other components (use `EntityId`).

### Rule 9 — Systems are (near-)stateless functions
Systems operate via `World::view<T...>()`, receive platform interfaces as parameters,
never store them, never `new`/`delete` entities directly.

### Rule 10 — World is the single source of truth
No global mutable state. No singletons holding game data. Shared read-only data via
`World::add_resource<T>()`.

### Rule 11 — Batch ECS operations on streaming paths (Q Rule 27)
Chunk load/unload creates and destroys entities **in batches**. Per-entity
`spawn`/`destroy` calls inside streaming paths are forbidden.

---

## Simulation rules

### Rule 12 — Fixed timestep
Simulation (physics, gameplay, AI) runs at the fixed rate in NUMBERS.md; rendering is
uncapped and interpolates. Gameplay code never reads wall-clock time.

### Rule 13 — Determinism, three levels (Q73)
1. **Worldgen is strictly deterministic**: same seed → same world, covered by a test
   from the first commit. Non-negotiable.
2. **Simulation is deterministic with the null LLM backend.** Tests and bug
   reproduction run with null LLM by default.
3. With a live LLM backend the simulation is knowingly non-deterministic; LLM
   decisions will later be journaled for replay.

### Rule 14 — Units
Meters, seconds, radians — everywhere. Degrees only at the UI boundary.
Constants come from the header generated out of NUMBERS.md; a hardcoded
gameplay/simulation constant in C++ is a violation.

### Rule 15 — NPC control API (Q70)
**There is no way to control an NPC except by enqueueing a typed `NpcAction`.**
Scripts, tests, the editor, and (later) the LLM all submit the same action values.
Direct mutation of NPC state outside the action executor is a violation.

---

## File header rules

Every source file (`.h`, `.cpp`, `.md`, `CMakeLists.txt`, `.cmake`, `.py`, `.sh`)
MUST start with the header comment. Formats (identical to Quicky):

**C++ (.h / .cpp):**
```
/*
Created: dd:mm:yyyy - hh:mm:ss
Last updated: dd:mm:yyyy - hh:mm:ss
Module: <module_path>
File: <relative_path_from_repo_root>

Responsibility:
- What this file does (1-3 lines).

Key items:
- Important types, functions, constants.

Dependencies:
- Uses: ... / Used by: ...

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- <file-specific constraints>
*/
/*
UPD:
- dd:mm:yyyy - hh:mm:ss: Description of change.
*/
```

**Markdown:** HTML comment block with `Created`/`Last updated` + `UPD` block.
**CMake / Python / Shell:** `#`-comment block, same fields.

### Rule 16 — Timestamps are real
System time, no placeholders. `Last updated` equals the newest UPD entry.

### Rule 17 — UPD is append-only
Never delete UPD entries. Every meaningful change adds one.

### Rule 18 — Per-module docs reflect current code
Each module keeps `docs/README.md` describing its **current** state. Read it before
modifying the module; update it in the same changeset. See `rules/documentation.md`.

---

## Code style

### Rule 19 — C++23, dual-toolchain
Primary toolchain: Homebrew clang (>= 22), `-std=c++23`. CI also builds with Apple
clang 15 at `-std=c++20`: C++23-only features require a feature-test guard with a
C++20 fallback — use them only when the win is clear. No compiler-specific
extensions. `#pragma once`. `std::unique_ptr` over raw `new`/`delete`.

### Rule 20 — Naming

| Item | Convention | Example |
|---|---|---|
| Types / Classes / Structs | PascalCase | `ComponentPool`, `EntityId` |
| Functions / Methods | snake_case | `get_component()`, `spawn()` |
| Variables / Fields | snake_case | `dense_data`, `generation` |
| Constants | UPPER_SNAKE_CASE | `SIM_TICK_RATE` |
| Namespaces | snake_case, root `dfn` | `dfn::ecs`, `dfn::world` |
| Files | PascalCase.h/.cpp | `World.cpp`, `IRenderer.h` |
| Directories | snake_case | `engine/core/ecs/sources/` |

### Rule 21 — Single responsibility per file
One clear responsibility. Aim for ~300 lines; **hard limit 800 LOC** (Q23 — three
parallel agents make big files the main merge-conflict source).

### Rule 22 — No logic in entrypoints
`main.cpp` only parses config, constructs backends, creates App, calls `run()`.

### Rule 23 — English everywhere in the repo
Code comments, docs, commit messages — English. User-facing game strings live in
localization files (any language).

---

## Process rules (agents)

### Rule 24 — Agents install nothing
No `brew install`, no `pip install` outside a checked-in venv setup, no system
changes. All C++ dependencies via CMake `FetchContent` with pinned tags. A clean
clone must configure and build with one documented command.

### Rule 25 — Directory ownership
| Owner | Zone |
|---|---|
| `core`   | `engine/core`, `engine/world` |
| `render` | `engine/platform/{window,input,render}`, `engine/render` |
| `sim`    | `engine/platform/{physics,anim,audio,llm}`, `engine/physics`, `engine/anim`, `engine/gameplay` |
| `design` | `docs/design` (landscape/world design bible, asset briefs, placement rules) |
| lead     | `engine/app`, `engine/editor`, `engine/core/components`, root CMake, `docs/`, `rules/`, `tools/` |

An agent never edits a foreign zone. Needing a change there → message the owner.
Each layer has its own `CMakeLists.txt` owned by its zone owner (Q34).

### Rule 26 — Contract freeze (Q38)
Public interfaces (everything another zone includes) are frozen for the duration of a
stage. Changing one requires a group sync, recorded in `docs/devlog/`.

### Rule 27 — Visual verification mandatory (Q24, Q26-Q24)
Any change that can affect the rendered image must be verified by running the
screenshot tour and checking each frame against the checklist. "It should work" in
prose is not an accepted verification.

### Rule 28 — Three strikes (Q61)
Three failed attempts at the same problem → **stop and message the lead** with what
was tried. A fourth silent attempt is a violation.

### Rule 29 — Branch per agent (Q61)
Each agent works on its own branch once implementation starts; merges to `main` only
on a green build. Whoever breaks the merge fixes it immediately.

### Rule 30 — Every test ships with a control
A test is published together with the case it exists to REJECT, and that case must
FAIL it. An invariant that nothing fails is not an invariant, it is a description.
Three separate shape invariants were shipped in one evening that a smooth analytic
cone passed; the third was caught only because it was run against a known-bad object
before being trusted. Controls are cheap — a cone, a sphere, a plane, a flat field.
"My test passes" and "my test discriminates" are different claims and only the second
one is worth reporting.

### Rule 31 — Assert the distribution, not the bounds
A random or noise field is verified UNIFORM (or explicitly-shaped) over its whole
declared range before anything is tuned against it. Bounds checks pass on a field that
never leaves the top third of its range. This is not hypothetical: every seeded spread
in the massif model was silently returning only the upper 60 % of its range, in lumps,
which meant one half of a documented design rule had never been generated at all and
every constant fitted against that field was fitted against a lie. When such a field is
fixed, the fixer lists which constants were tuned while it was broken, because they all
have to be re-derived.

### Rule 32 — Fix the mechanism, not the instance
When a defect is traced to a shared helper, every consumer of that helper is inspected
in the same change. Repairing the one call site that surfaced the symptom and leaving
the helper feeding the others is not a fix; it converts a visible bug into an invisible
one. Corollary, learned the same evening: a diagnosis written down but applied only
locally is a diagnosis not yet acted on.

### Rule 33 — Detail is sized against the viewing distance
Structure sized as a fraction of the object it belongs to shrinks out of legibility as
that object recedes. A crest sized against the mountain is invisible from the valley;
the same reasoning invalidated a summit feature whose outline was identical to the bare
profile. Features exist to be read from the distance the acceptance frame is taken
from, and that distance is the input to their size.

### Rule 34 — Never diagnose from an unchecked premise
A claim about another zone's code — that a fix landed, that a unit is absolute, that a
field is seeded — is checked in the source or asked of its owner before any conclusion
is built on it. Sound reasoning from a false premise is indistinguishable from sound
reasoning until it wastes a build. This rule exists because it was broken four times in
one evening, twice by agents who had just invoked it against someone else.

---

## Build & quality gates

- Configure + build from clean clone: one command, zero warnings
  (`-Wall -Wextra -Wpedantic`).
- `python3 tools/header_check.py --all` passes (also runs in the pre-commit hook).
- Worldgen determinism test passes (Rule 13.1).
- Game targets compile without any backend headers on their include path
  (enforced by CMake target isolation).
