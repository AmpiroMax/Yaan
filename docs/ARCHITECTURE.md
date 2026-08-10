<!--
Created: 09:08:2026 - 00:06:00
Last updated: 10:08:2026 - 21:17:06
-->
<!--
UPD:
- 09:08:2026 - 00:06:00: Created engine architecture hard contract, adapted from Quicky Engine with amendments approved in the initial-planning grill session (Q23, Q25, Q27, Q30, Q31, Q54, Q61, Q70, Q73).
- 09:08:2026 - 10:42:00: Stage 3 — added the `design` zone (landscape/world design docs) to Rule 25.
- 09:08:2026 - 21:56:40: Added Rules 30-34, all five earned by defects found in the worldgen v2 stage: controls with every test, distribution asserted not bounds, fix the mechanism not the instance, detail sized against viewing distance, never diagnose from an unchecked premise.
- 09:08:2026 - 22:12:38: Rule 30 gained corollaries 30a (a test needs a case that CAN pass it) and 30b (for a diagnosis, the counterfactual is the control).
- 09:08:2026 - 22:43:02: Rule 35 — a number stops belonging to a zone when a second zone must agree with it; the trigger is a thing gaining a dimension.
- 09:08:2026 - 23:55:22: Rule 16 — read the clock per batch, never compute forward. Три зоны сделали эту ошибку за один вечер.
- 10:08:2026 - 00:05:41: Rule 36 — exclusions by cause, not by magnitude; and the pipeline's own metric belongs in the design vocabulary.
- 10:08:2026 - 00:13:43: Rule 36 — добавлена дешёвая стоячая проверка: сравнить максимум с отсечкой, и если они рядом, отвечал фильтр.
- 10:08:2026 - 00:14:34: Rule 30 — когда есть настоящий отвергнутый образец, контролем служит ОН, и порог обязан стоять выше него.
- 10:08:2026 - 00:18:40: Rule 29 — до включения веток каждый коммит перечисляет файлы явно; общий индекс делает голый git commit ловушкой.
- 10:08:2026 - 00:31:03: Rule 27 — точка съёмки, неспособная провалиться, не является доказательством.
- 10:08:2026 - 00:35:07: Rule 30 — на какой ВЕЛИЧИНЕ стоит порог, тоже является измерением; и вилка — это два утверждения.
- 10:08:2026 - 01:45:06: Rule 25 — new character zone (engine/anim + platform/anim, carved from sim): user approved the character agent twice (v51, v16).
- 10:08:2026 - 02:49:47: Rule 27 — цитируемые кадры архивируются в docs/acceptance/ в родном разрешении; рецепт обязателен рядом.
- 10:08:2026 - 11:05:46: Rule 27 — тур замораживает тик и потому слеп ко всему движущемуся; живые пути съёмки признаны доказательством.
- 10:08:2026 - 11:29:06: Rule 30 — каждое приёмочное правило называет свою АГРЕГАЦИЮ и свой ЗНАМЕНАТЕЛЬ, а не только число (три спора за два дня оказались спорами об определении).
- 10:08:2026 - 19:26:40: Правила 37 (линейная карта между двумя именованными константами становится скрытым дефектом, когда между ними ложится третья) и 38 (утверждать исход, а не механизм) — обе выведены двумя зонами независимо, из разных дефектов, за один день.
- 10:08:2026 - 19:53:40: Правило 37 расширено общей формой: опасны САМИ ИМЕНОВАННЫЕ КОНЦЫ линейной карты — не только когда между ними ложится третья константа, но и когда старая МЕНЯЕТ ЗНАЧЕНИЕ. Перенос WALK_SPEED с 3.0 на 1.8 разом пересчитал всю кривую покачивания в 1.667 раза.
- 10:08:2026 - 20:01:34: Правила 39 (теневая копия цепочки становится дефектом, как только у оригинала появляется ветка — три копии одного высотного конвейера разошлись за день, и все наборы остались зелёными, потому что тестировали единственный стенд, где они ещё совпадали) и 40 (Approx().epsilon() не проценты, а на РАЗНОСТИ неверен при любом масштабе). Формулировка 39 — core, чья находка.
- 10:08:2026 - 20:32:07: Правило 41 (прибор, меряющий ОБЪЕКТ, не может принять утверждение о ВИДЕ — одна и та же доля даёт противоположные вердикты). И у правила 16 появилось МЕХАНИЧЕСКОЕ средство: читать date и писать файл одним вызовом оболочки, потому что шесть нарушений после написания правила допустили те, кто мог его процитировать.
- 10:08:2026 - 20:32:52: Правило 42 (бюджет, выраженный в единицах одних часов, ничего не ограничивает в единицах других — бюджет читался как 1 и выдавал 5). Формулировка sim, чья находка; вместе с признаком: величина, повторяющаяся до цифры без разброса, это ограничитель, а не склонность.
- 10:08:2026 - 20:33:41: Правило 43 (ограничение связывает ту величину, на которой написано, а не ту, по которой меряют контракт — карточки легли плоско и потратили запас, который всегда был законным). Формулировка flora; перенумеровано с 39, тот номер уже занят.
- 10:08:2026 - 20:34:56: Правило 36 дополнено случаем ВБЛИЗИ НУЛЯ: «малая поправка» и «ответ» там одного размера. Отброшенный член 2.5 мм при результате 3.0 мм — 83% ответа. Выведено sim и character независимо, из разных дефектов.
- 10:08:2026 - 20:37:27: Правило 16 — почему «замечать» не могло сработать: проверка мерила ВЕЛИЧИНУ ухода, а дефект — ПРОИСХОЖДЕНИЕ отметки. Четырёхсекундный уход это тот же поступок, что тридцатиминутный. Правило 41, наведённое на правило 16 (формулировка character).
- 10:08:2026 - 20:37:53: Правило 16 — соблюдение проверяется по СПОСОБУ, а не по результату: зона нарушила средство в том самом коммите, которым чинила форму починки, и значение вышло верным по везению. Верный результат — это ровно то, что даёт удачливый неверный способ.
- 10:08:2026 - 20:45:59: Правило 44 (константа, подогнанная СКВОЗЬ деталь реализации, перестаёт означать то, что говорит её имя — две среды складывают одни и те же поля по-разному и промахиваются в ПРОТИВОПОЛОЖНЫЕ стороны). Находка core.
- 10:08:2026 - 21:12:54: Правило 45 (порог различимости и порог РАЗДЕЛЕНИЯ — разные объекты; признак виден ДО измерения: если обоснование ни разу не упоминает отвергнутый образец, это пол, и в гнезде приёмки он пропускает всё). Формулировка design.
- 10:08:2026 - 21:17:06: Правило 29 — `git commit --amend` ЗАПРЕЩЁН на общей ветке: HEAD, прочитанный секунду назад, это предпосылка, а не факт. Правка задела чужой коммит, потому что третий агент успел вклиниться между двумя вызовами.
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

**Read the clock for every batch; never compute forward from an earlier reading.**
Three separate zones made exactly this mistake in one evening — run `date` once,
then hand-write later stamps by adding minutes to it — and all three produced
entries dated in the future. It is invisible unless a reader notices a date that
has not happened yet, and it does not merely misdate one file: the UPD blocks are
this project's only ordering record across zones, so a stamp written forward
silently reorders history. Provenance arguments depend on it — proving a constant
sweep predated a bug fix meant comparing stamps in two zones' files.

**THE REMEDY IS MECHANICAL, NOT ATTENTIONAL, and it took a second day to learn
that.** Six more violations happened after the rule was written — by agents who
could quote it, including one who committed it minutes after reporting the same
violation in a peer, and including the lead. "Read the clock" is not a practice
anyone fails at deliberately; it fails because the stamp gets written from memory
of a reading taken a few tool calls ago, and a few tool calls is enough time for
the clock to move. So: **read `date` and write the file in the SAME shell
invocation.** If the stamp cannot be produced by the same command that consumes
it, treat that as the defect to fix rather than as care to apply harder. A rule
that only holds while you are paying attention is a rule that fails precisely
when you are busy — which is when the stamps matter.

**And the reason noticing could never have worked, which is Rule 41 pointed at this
rule:** the same batch that produced a 31-minute forward stamp also produced a
FOUR-SECOND one. Both are the same act — a timestamp written from memory instead of
read from the clock — and they differ only in how far the estimate had drifted. A
check enforced by noticing catches the 31-minute case and never the 4-second one,
which means it was measuring **drift** when the defect is **provenance**. "Does the
stamp look wrong?" cannot express "was it read or recalled?", and the harmless-
looking instance is the one that proves the habit is still there. Both of today's
breaches happened while their author was moving fast on a correction to something
else.

**So compliance is audited on the METHOD, never on the output.** The zone that
disclosed six violations then fixed them, was corrected on the FORM of the fix, and
**used the forbidden method in the commit that fixed the form** — reading `date` in
one tool call and writing the stamp as a literal in the next. The value was right,
by luck of timing rather than by construction, and nobody could have caught it by
looking at the file, because a correct output is exactly what a lucky wrong method
produces. Asking "is this stamp right?" cannot find it. Asking "was this stamp read
by the command that wrote it?" finds it every time.

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
| `character` | `engine/anim`, `engine/platform/anim` (rig, animations, first-person body; carved from sim) |
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

**A vantage that cannot fail is not evidence.** The frame must contain the subject
ACROSS THE RANGE THE PROPERTY UNDER TEST VARIES OVER — a banding check needs the
material at several luminances, not a flat strip at one; a silhouette check needs
several bearings, not the flattering one; a legibility check needs the landmark at
its own acceptance distance. A clean result off a frame that could not have shown
the defect is measuring the absence of a test. This is Rule 30 moved from the test
suite into the camera, and it was earned twice in one evening by the same agent
catching it in themselves.

**The screenshot tour is not the only sanctioned evidence, and for anything that
MOVES it is the wrong instrument.** The tour freezes the tick to make its frames
deterministic, so it can photograph still life and nothing else — every animation,
every drift, every gait in this project was unreachable by it until two live-world
capture paths appeared (a gated simulation probe and the playtest bot's incident
shots). A frame of a moving thing comes from a live tick, with the achieved state
(phase, clip time, timestamp) recorded beside it so the recipe is checkable. A still
frame cannot show a motion artifact at all — do not claim one from it.

**Evidence frames are archived, not only recipes.** A reproduction recipe (binary
provenance, seed, probe env, eye, time) is the CONTROL — it reproduces bit-identically
under Rule 13 and is mandatory wherever a frame is cited. But `screenshots/` is
gitignored and binaries die, so any frame a ruling or an acceptance CITES is also
copied into `docs/acceptance/` (tracked), at native internal resolution (640x360 — the
4x tour upscale adds pixels, not information), filename carrying the commit and the
subject. Recipes prove the present; pixels preserve the past — every provenance-dead
measurement hunt this project has run would have been cheaper with the original
pixels. Curated only: a frame nobody cites does not enter.

### Rule 28 — Three strikes (Q61)
Three failed attempts at the same problem → **stop and message the lead** with what
was tried. A fourth silent attempt is a violation.

### Rule 29 — Branch per agent (Q61)
Each agent works on its own branch once implementation starts; merges to `main` only
on a green build. Whoever breaks the merge fixes it immediately.

**Until that is actually enforced, every commit names its files explicitly:
`git commit -- <paths>`, never a bare `git commit`.** Agents share one working tree
and therefore one index, and `git add` is global state: a bare commit sweeps up
whatever any other agent has staged in the seconds since. It has already happened —
a docs commit silently absorbed twelve files of another zone's work, including the
fix for the only crash a user hit that day, so the most valuable change of the night
is recorded under a message about a documentation rule and a bisect will point at the
wrong commit. Nothing was lost and history was correctly NOT rewritten, because
rewriting shared `main` under four agents is worse than a misleading message.

**`git commit --amend` IS BANNED on the shared branch, and the reason is Rule 34
in its cheapest possible form: HEAD read a second ago is a PREMISE, not a fact.**
An agent committed with a message file whose name a peer had already used in the
shared scratchpad, so their commit went in wearing another zone's message; they
reached for `--amend` to fix it, and a third agent had committed **in the gap
between the two calls** — so the amend rewrote the wrong commit. Nothing was
lost, and the repair was the right tool (`git commit-tree` plus a
compare-and-swap `git update-ref`, which touches neither the index nor anyone's
working tree). But with five agents committing concurrently that window is
always open, and a wrong message is cheaper than a rewritten peer.

The same lesson one layer down: **the shared scratchpad is shared.** A commit
message at `msg1.txt` is not a filename, it is an unlocked resource. Name files
uniquely per agent.
The failure is silent, it will recur, and the next collision may not compile.

### Rule 30 — Every test ships with a control
A test is published together with the case it exists to REJECT, and that case must
FAIL it. An invariant that nothing fails is not an invariant, it is a description.
Three separate shape invariants were shipped in one evening that a smooth analytic
cone passed; the third was caught only because it was run against a known-bad object
before being trusted. Controls are cheap — a cone, a sphere, a plane, a flat field.
"My test passes" and "my test discriminates" are different claims and only the second
one is worth reporting.

**When a real rejected instance exists, IT is the control, and the threshold must
sit above it. And WHICH QUANTITY a threshold belongs on is itself a measurement:
if no value on a quantity separates the accepted cases from the rejected ones, the
QUANTITY is wrong, not the threshold.** That is the mechanical form of this
project's central failure — nine invariants measured the mountain and none measured
the view, and the way to have caught it in an afternoon was to ask of each one
whether ANY threshold on it would separate the mountain the user rejected from one
he would accept. For most of them the answer was no, and that is computable rather
than a matter of judgement.

**Every acceptance rule names its AGGREGATION and its DENOMINATOR, not just its
number.** Three arguments in two days turned out to be definitions wearing a
measurement's clothes: a dispersion threshold whose denominator was never stated
(ideal Poisson or the same placement unclumped — they disagreed only for the class
under dispute), a control taken once and applied across classes whose densities
differ (right exactly where everyone was looking, wrong everywhere else), and a rule
reading "a ring of samples at 40-80 m" that passes read as one ring and fails read
per-distance. In all three the number was never the disagreement. A threshold
without its aggregation and its denominator is not yet a rule.

**A range is two assertions, not one.** Both ends get derived and both get measured.
Six defects tonight came from an unexamined end: a crown-width band whose maximum
was arithmetically too small to satisfy the aspect ceiling it shared a species with,
and which passed only because the generator never happened to land in the corner the
band permitted. A synthetic worst case is the easy reject: a limb-spread test shipped
with a synthetic palm at 0.06 against a floor of 0.15, passed it correctly, and still
passed the tree the user had rejected in words, which measured 0.17-0.19. A threshold
placed below every real failure is a description, not a test.

Two corollaries, both earned the same evening:

**30a — a test also needs a case that CAN pass it.** An invariant requiring three
detected ridges on a body that has exactly three has zero margin by construction: a
perfect specimen fails when one corner reads a degree shy, and what is being measured
is the detector rather than the object. When a generator's input and a test's threshold
are the same number, that is a coincidence, not a check.

**30b — for a diagnosis, the counterfactual is the control.** "These tests went red
right after my change, therefore my change broke them" is Rule 34 with the premise
hidden inside a correlation, which is how it slips past people who would never assert
the premise out loud. Run the other arm before reverting. A good change was nearly
withdrawn this way to avoid breaking two peers whose tests were already red for their
own reasons.

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

### Rule 35 — A number stops belonging to a zone when a second zone must agree with it
Such a number moves to `docs/NUMBERS.md` and both zones read it there. "Zone A tells
zone B a value and B hardcodes it" is the same defect as "B keeps a literal A has to
match" — the direction of the conversation does not change that there are two copies.

The predictive form, which is what makes this findable before it bites: **the second
consumer appears when a thing GAINS A DIMENSION.** How far a boulder sinks into the
ground mattered to nobody until boulders acquired collision, at which point the drawn
surface and the solid surface had to be the same number. A torch's length mattered to
nobody until the torch acquired a mesh, at which point the wood and the flame had to
be the same number. So the trigger to watch for is not "two zones are arguing", it is
"something that was only drawn is now also simulated, or the reverse".

The same reasoning applies to STATE, not only to constants: the composition root must
never reconstruct bookkeeping a subsystem already holds. Two copies drift whether they
are numbers or sets.

### Rule 36 — A measurement's exclusions are chosen by cause, never by magnitude
Filtering a sample set by size makes the filter the result. A skirt-depth measure
excluded tunnel geometry as "anything more than 1 m below the surface" and reported
a maximum of 0.9937 m — **the cutoff was the answer**, and it looked like a
measurement right up to the moment a constant was about to be set from it.
Reclassifying the same samples by their CAUSE (was this vertex carved?) gave
1.1111 m, which is 0.11 m more than the constant would have allowed.

**The standing check, which costs nothing: after any filtered measurement, compare
the reported extreme against the cutoff. If they are within a few percent, the
filter is the result.** 0.9937 against a 1 m cutoff should have been read that way
on sight.

Sibling of Rule 31: there the field's shape decided the answer, here the filter's
threshold does. Both produce a number that survives review because nothing about it
looks wrong.

**THE SIBLING CASE: near zero, "a small correction" and "the answer" are the same
size.** The parent check compares a reported extreme against a filter's cutoff.
When a result sits near ZERO, the test is not *"is this term small"* but **"is it
small relative to the RESIDUAL"** — and only the second one discriminates.

The eye-vs-chest cancellation was computed as **−0.0030 m** by a linear form that
dropped the chest corner's own cos(θ) foreshortening. The dropped term was
**0.0025 m** — 83% of the reported answer. The exact figure is −0.0055 m. Nothing
about *"I will ignore a 2.5 mm second-order term"* looks wrong when written down;
it is wrong only because the result it feeds is 3 mm.

Same disease as the parent: there the filter's threshold became the result, here a
discarded small term did. **The shared trigger is a reading close to zero** —
precisely when a quantity you decided was negligible stops being negligible,
because negligible is a RATIO and the denominator just collapsed.

**Mechanical form: after any result near zero, recompute the largest term you
dropped and divide it by the result. If that ratio is not small, you have not
measured the thing.**

And the general form, which cost a full evening in another guise: **the pipeline's
own metric is part of the design vocabulary.** Colour separation was reasoned about
in Euclidean RGB while the quantiser weights the channels 0.30/0.59/0.11, so a
difference that lives in blue is nearly invisible to it. A rule about what the EYE
reads and a rule about what the PIPELINE preserves are different rules, and a
design must pass both.

### Rule 37 — A linear map between two named constants becomes a latent defect the moment a third named constant lands between them
The animation blend interpolated between `WALK_SPEED` 1.8 and `RUN_SPEED` 6.0. That
was correct while those were the only two gears. The user then ruled three speeds,
`JOG_SPEED` 3.0 landed between them, and jog silently began rendering as a walk clip
leaning (3.0−1.8)/(6.0−1.8) = **0.286 toward run** — a gait nobody chose, and not a
gait at all, just what a linear map does when asked about a point it was never
calibrated for.

**Nothing about the interpolation changed.** No code was touched, no test went red,
no review would have caught it, because the defect was created by a ROW LANDING IN A
REGISTRY. That is what makes it worth a rule: the change that introduces it does not
happen anywhere near the code that breaks.

Sibling of Rule 35 in shape. There a NUMBER gained a second consumer; here a RANGE
gained an interior point. The trigger to watch for is the same kind of thing — not a
bug report, but a registry edit — and the question to ask when a constant lands
between two existing ones is: **what interpolates across this range?**

**The general form, which is wider than the first case and was found the same day
by a different zone: the danger is a linear map's ENDPOINTS being named constants at
all — whether a new one lands between them, or an old one MOVES.**
`bob_amplitude_target(v) = HEADBOB_AMPLITUDE_AT_WALK × v / WALK_SPEED` was authored
when `WALK_SPEED` was 3.0. The three-speed ruling moved `WALK_SPEED` to 1.8 five
hours later, and because the map is *anchored* on that name, **every point of the
curve below the cap was rescaled by 3.0/1.8 = 1.667× at once** — walking and jogging
silently received 67% more head bob than anyone chose, and the cap engagement point
slid from 5.14 m/s down to 3.09 m/s. Nobody touched the bob code, no test went red.

The interior-point case corrupts one region; the moved-anchor case corrupts the
**whole curve** and is therefore easier to miss, because nothing looks locally
wrong — everything is consistently off. So the standing question has two halves:
when a constant lands between two others, ask what interpolates across the range;
when a constant's VALUE changes, ask **what is anchored on its name**.

Note what this rule does NOT license: in the bob case the running gait was clamped
by `HEADBOB_AMPLITUDE_MAX` both before and after, so the rescale did not explain the
judder it was found while investigating. The zone reported it anyway, as true rather
than as convenient. A rule that only surfaces the defects that happen to be the one
you are hunting is a rule nobody runs.

### Rule 38 — Assert the outcome, not the mechanism
Two zones reached this independently, from unrelated defects, on the same day, which
is the only reason it is a rule rather than a preference:

- *"the residual slip is imperceptible"*, **not** *"the clamp is inactive"* — at
  `WALK_SPEED` the clamp still binds by 0.4%, so the mechanism-shaped assertion goes
  red on correct code the day it is written.
- *"this gait renders as this gait"*, **not** *"no interpolation ran"* — a blend
  mid-transition is CORRECT animation, so the mechanism-shaped assertion forbids the
  right implementation.

The shared failure mode is the point, and it is worse than being too loose: **a test
that goes red on correct code does not get argued with, it gets weakened.** A
mechanism-shaped assertion therefore does not merely fail to catch things — it trains
everyone reading the suite that tests are obstacles. That is a different and more
expensive failure than a test that simply passes when it should not.

**Corollary, and it is the half that is easy to skip:** when you loosen an assertion
to stop it forbidding correct code, RE-VERIFY THAT THE CONTROL STILL FAILS IT
(Rule 30). Speed-derived gait selection leans 0.286 after 0.1 s and after an hour —
being a pure function of speed, it has nothing to settle — so it still fails a
steady-state assertion exactly where a legitimate transition blend would have
finished settling. That check is what separates a refinement from a quiet gutting.

### Rule 39 — A shadow copy of a chain is a latent defect the moment the original gains a branch
Rule 35's state clause says two copies drift. This is the trigger that makes the
drift findable: the copies are IDENTICAL when written, and they are usually
accompanied by a comment saying so — *"exactly the chunk builder's chain"*, *"this is
the same function evaluated here"*. **That comment is true on the day it is written
and it is the only thing holding the two together**, which means the defect is
created by a change that happens **nowhere near the copy**: a new branch, a new
stand, a new mode landing in the original.

Nothing goes red, because the copies still agree everywhere the new branch does not
apply — **and the place it does not apply is usually the case everyone is testing.**

Three copies of one height pipeline drifted this way in a single day. The drawn
ground stood up to 1.50 m from the ground everything was placed on; the LOD coarse
nodes built a different terrain from the chunks they are contractually required to
meet bit for bit (16158 of 16641 samples disagreeing, −1.5015 .. +1.2634 m); and
every suite stayed green throughout, because the testbed was the one stand where all
three still agreed.

**The question to ask when adding a branch is not "did I break a caller" but "WHO
ELSE CLAIMS TO BE THIS FUNCTION?" — and a comment asserting two things are the same
is not a mechanism that makes them the same. Calling one function is.**

Sibling of 37 in shape (a change breaking code far from itself, nothing red in
between); sibling of 35 in substance (two copies of one fact).

A note on the control this earned, because it generalises past height fields: the
right control for a shadow-copy fix is **the pre-fix copy written out verbatim**, and
its value is that it is EQUAL to the correct answer on the case everyone tests while
wrong on the case nobody did. That asymmetry is the finding rather than a weakness —
no amount of testing the well-trodden stand can catch a copy that diverges only where
a newer branch applies.

### Rule 40 — `Approx().epsilon()` is not a percentage, and it is simply wrong on a difference
doctest's tolerance is `eps * (scale + max(|lhs|,|rhs|))` with `scale` defaulting to
**1.0**. So `.epsilon(e)` admits `e * (1 + |x|)`, which is inflated over the relative
band the author meant by a factor of **(1 + |x|)/|x|** — near 1 for large quantities
and catastrophic for small ones, which is most physical quantities in this project:
fractions, metres of clearance, joint offsets. `.epsilon(0.25)` around 0.15 admits
**−0.14 .. +0.44 m**, so a clearance assertion passed a path buried 14 cm under its
own ground, and an ankle assertion claiming to prove a foot is "planted rather than
dangling" carried **±0.27 m** of slack — a quarter of a metre of vertical freedom on
the exact property it names.

**And the sharper form, which is not about magnitude at all: `.epsilon()` is wrong
whenever the quantity is a DIFFERENCE, at any scale.** A height error, a residual, a
gap, a slip — these are all quantities whose correct value is ZERO, and scaling the
tolerance by the operand is scaling it by the wrong number entirely. A 0.02 epsilon
on "does this log sit on the ground" became 0.42 m of slack because the ground
happened to be 20 m up, which admits a log floating knee-high.

**Use explicit bounds.** They are longer to write and they say what they mean, and on
a difference they are the only thing that can.

### Rule 41 — An instrument that measures the OBJECT cannot accept a claim about the VIEW
The apron around the massif was accepted on "fraction of the mountain's low
silhouette hidden by canopy". At 300 m the apron moved that fraction by **1.1
points** while demonstrably fixing the defect it was built for, and a third of the
silhouette stayed hidden — which is also, correctly, what a forested valley looks
like. Both readings are true at once, and when two correct readings disagree about
the verdict, **the ruler is what is wrong, not either answer.**

The case the fraction cannot see: 69% visible spread evenly reads as a mountain
behind a wood; 69% visible with the BOTTOM curtained reads as a painted backdrop.
**Identical fraction, opposite verdicts.** The quantity is a property of the
object; the acceptance is a property of the picture. Replaced with the ground
junction — a contiguous run where the lowest visible massif pixel is
massif-meeting-ground rather than canopy edge.

The general trigger, and it is cheap to apply: **when an acceptance number moves
by almost nothing while everyone agrees the thing got better, do not widen the
threshold — ask whether the quantity can express the difference at all.** A
threshold argument is the expected shape of this failure and it is always the
wrong argument, because no threshold on a quantity that cannot see the difference
will ever separate the cases (Rule 30's mechanical form).

Sibling of Rule 30's "which quantity a threshold sits on is itself a measurement",
and of Rule 36 — there the filter decided the answer, here the choice of quantity
does. All three are the same disease: **the measurement looked rigorous and was
about the wrong thing.**

### Rule 42 — A budget denominated in one clock's units enforces nothing in another clock's
`CHUNK_LOAD_BUDGET` was 1 chunk per `ChunkManager::update`, and its own NUMBERS row
priced the thing it protects against in FRAMES: *"~83 ms per chunk, at 2 per frame
already 166 ms — reads as a freeze."* But `update()` was called once per SIM STEP,
inside the fixed-timestep catch-up loop, so one frame could contain up to
`SIM_MAX_CATCHUP_STEPS` of them. **The budget read as 1 and delivered 5.**

**The protection inverts exactly where it is needed:** the slower the frame, the
more catch-up steps it runs, the more streaming work it is permitted to do. A
budget meant to prevent hitches was scaled by the size of the hitch.

Neither number is wrong. `1` is right per tick; `5` is right as a spiral-of-death
guard. The defect is that they live in different clocks and the limit that matters
is in the second one. It survived review by two zones who each reasoned correctly
from a wrong model of the call site, and it was found only by grepping where the
call actually is.

**The check, and it is mechanical: for every budget, name the unit of the thing it
protects. If that unit is not the unit the budget is counted in, there is a
conversion factor — and the conversion factor is variable.**

Sibling of Rule 35 (one number, two zones) and Rule 37 (a range gains an interior
point). Here a RATE is correct in one denominator and wrong in another, and nothing
about either figure looks wrong in isolation.

**The detection heuristic that found it is worth as much as the rule: a measured
quantity that repeats to the digit with zero variance is a CLAMP, not a tendency.**
Every stall episode across nine runs sat at exactly 5.00 ticks per frame against a
normal of 0.503. That was sitting in data already collected for a different
question — nobody had to instrument anything, only to notice that a real
measurement does not repeat exactly.

### Rule 43 — A containment bounds the quantity it is written on, never the quantity the contract is measured on
Card clusters were contained by a 3D corner reach — `centre + hypot(half_width,
half_height) <= envelope` — while the species width band is measured HORIZONTALLY.
Those are the same number only for a plane standing upright, which every card
happened to be. The first build that laid cards flat spent the whole reach
horizontally, occupied an allowance that had always been legal, and the birch
measured 7.36 m against a band asserted at 7 — **with no code changing on the path
that broke.**

The tell is that **nothing was violated.** Containment held exactly (1.44 + 2.23 =
3.67 against an envelope of 3.67), the contract still failed, and the gap between
the two quantities had been sitting there unmeasured the whole time. A bound in one
quantity is an upper bound in another only by an inequality nobody wrote down, and
when the geometry changes orientation, distribution or shape, it cashes that
inequality in.

So: **when you write a containment, write down WHICH quantity the acceptance is
measured on, and enforce on that one.** If the two differ, the slack between them
is a budget the next change will spend — silently, because the change that spends
it is nowhere near the code that bounds it.

Sibling of Rules 35 and 37 in shape: there a NUMBER gained a second consumer and a
RANGE gained an interior point; here a BOUND gained a second quantity. The trigger
is the same kind of event — not a bug report, but a change of representation.

### Rule 44 — A constant fitted through an implementation detail no longer means what its name says
Two habitats compose the same two fields differently, and nothing in the code or the
docs said so:

```
PathMargin   field = max(clump, edge * rich)   <- a FLOOR
ForestFloor  field = clump                     <- a pure PRODUCT
```

PathMargin realises **2.5–2.7× OVER** its authored density (a cobble gutter whose
authored flower weight is exactly 0.0 carries ~20 per 100 m); ForestFloor realises
**0.15–0.31×**, i.e. 3.3–6.6× UNDER. **They err in opposite directions, which is
precisely why neither was obvious** — an overshoot and an undershoot in one system
read as two unrelated tuning problems rather than as one unexamined composition.

A pure product delivers `authored × E[clump]`, and the measured mean of that field
is **0.086–0.163 by class** — nowhere near 1. So the shortfall is set by **the mean
of a noise field, which is not an authored quantity** and moves whenever the field
is retuned for how it looks.

**The trap is the fix that looks obvious: raise the row to compensate.** It works,
and it changes what the row MEANS — from "density" to "density before an
implementation detail". Every such row is then calibrated against an unauthored
property of a working system, and the day the field is retuned for appearance, every
row fitted through it moves silently and nobody connects the two changes.

So: **before tuning a constant to make an output right, measure whether the path
between them has a gain you did not author.** If it does, the constant is the wrong
place to fix it — and if you fix it there anyway, RENAME the row to say what it now
means, because the next reader will take its name literally.

Rule 31's shape one level up: there the field's distribution decided an answer, here
an unauthored property of a field is being absorbed into a named constant. Sibling
of Rule 41 — both are cases where the number is right and the thing it denotes is
not what anyone thinks.

### Rule 45 — A legibility floor and a separating threshold are different objects
A **floor** answers *"below what value can the eye not read this at all?"* and is
derived from the DISPLAY — pixel size, quantiser step, contrast. A **separating
threshold** answers *"what value puts the rejected picture on one side and the
accepted picture on the other?"* and can only be derived from **two measured
arms**.

A 20 px legibility floor was derived correctly from the palette quantiser and put
in an acceptance's slot. The real rejected instance then measured **106 px** — it
cleared the threshold by 5.3×. A floor in an acceptance's slot **passes
everything**, and it does so while looking rigorous, because its derivation is
sound; it is simply the answer to a different question.

**The tell is available BEFORE any measurement, which is what makes this cheap:
a threshold whose derivation never mentions the REJECTED INSTANCE is a floor.**
Read the justification. If it talks only about perception, resolution or physical
limits and never about the thing that must fail, it cannot separate anything.

Corollary, and it is the harder half: **movement is not discrimination.** A
quantity that moves when the fix lands is not thereby the right quantity. The
alternative proposed here moved by 8.8% and was still refused, because the defect
is vertical and the quantity is horizontal — a massif at full width with its
bottom third curtained scores perfectly and IS the rejected picture. Rule 41's
identical-number-opposite-verdicts failure, one axis over.

**And a procedure needs a STOPPING CONDITION or the threshold argument returns.**
The one adopted here: measure the rejected arm against the ideal arm FIRST, and
if they do not separate by more than the instrument's own re-shoot noise, **refuse
the quantity and write nothing.** Finding the quantity wrong a third time is a
legitimate outcome and beats a fitted number.

Sibling of Rule 30 (a threshold must sit above the real rejected instance; which
quantity it sits on is itself a measurement) and Rule 41 (an instrument that
measures the object cannot accept a claim about the view). Rule 41 has now fired
twice on the same acceptance one day apart — the second time on the quantity Rule
41 itself installed, which is the strongest available argument that this class of
error is not carelessness.

---

## Build & quality gates

- Configure + build from clean clone: one command, zero warnings
  (`-Wall -Wextra -Wpedantic`).
- `python3 tools/header_check.py --all` passes (also runs in the pre-commit hook).
- Worldgen determinism test passes (Rule 13.1).
- Game targets compile without any backend headers on their include path
  (enforced by CMake target isolation).
