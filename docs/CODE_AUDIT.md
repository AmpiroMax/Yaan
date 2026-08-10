<!--
Created: 10:08:2026 - 20:40:13
Last updated: 10:08:2026 - 20:40:13
-->
<!--
UPD:
- 10:08:2026 - 20:40:13: Created — full read-only audit of size, coupling and quality at commit eda8d6d + working tree, requested by the user. Measurements only; every ratio names its numerator and denominator (Rule 30).
-->

# Code Audit — Daggerfall N

**Scope:** the whole repository at `main`, working tree of 10:08:2026 20:40.
**Method:** `cloc`, `tokei` and `scc` are all absent from this machine (Rule 24 —
agents install nothing), so the counters were written for this audit and are
described below. Excluded from every count: `build*/`, `_deps/`, `third_party/`,
`.git/`, `screenshots/`, `captures/`, `playtest_test_artifacts/`,
`tree_images_examples/`, and binary assets (`.png`, `.wav`).

**Counter definitions** (stated because the numbers are meaningless without them):

- A C++ line is **CODE** if any character outside a comment or a string literal
  is non-whitespace; **COMMENT** if it is non-blank and entirely comment;
  **BLANK** otherwise. Block comments, line comments and string literals are
  tracked with a small state machine, so `"// not a comment"` counts as code and
  a `/* */` spanning ten lines counts as ten comment lines.
- A Markdown line is **PROSE** unless it sits inside an HTML comment block (the
  mandatory header / UPD blocks), which counts as COMMENT.
- CMake / Python / shell: `#`-leading lines are COMMENT.

**Audit control (Rule 27 — a vantage that cannot fail is not evidence).** The
include-graph result below is a *negative* one, so the instrument was checked
first: the repo contains 1716 lines matching `#include`, of which **0** use
angle brackets for an `engine/` path and **1** is relative
(`engine/app/sources/App.cpp:99` → the generated `BuildInfo.h`). A parser that
reads only `#include "engine/..."` therefore sees essentially the whole graph;
it is not reporting "clean" because it is blind.

---

## 1. Size

### 1.1 Headline

| Area | Files | Code | Comment | Blank | Total |
|---|---:|---:|---:|---:|---:|
| `engine/` C++ | 259 | 25 924 | 17 574 | 5 266 | 48 764 |
| `tests/` C++ | 48 | 12 583 | 4 514 | 1 793 | 18 890 |
| `docs/` + `rules/` Markdown | 57 | 17 924 | 1 432 | 2 964 | 22 320 |
| CMake (all layers) | 32 | 1 631 | 925 | 261 | 2 817 |
| `tools/` (Python + shell) | 7 | 581 | 213 | 100 | 894 |
| `games/daggerfall_n/` | 10 | 453 | 10 | 12 | 475 |

**All C++: 307 files, 38 507 code, 22 088 comment, 7 059 blank, 67 654 total.**
Median C++ file: 137 total lines. Mean: 220.

Key ratios, numerator/denominator named:

- **test CODE / engine CODE = 12 583 / 25 924 = 0.49.**
- **docs PROSE / engine CODE = 17 924 / 25 924 = 0.69.**
- **docs TOTAL / (engine + tests) CODE = 22 320 / 38 507 = 0.58.**
- `games/daggerfall_n/` contains **0 lines of C++**. The declared
  `games/daggerfall_n/src/{systems,components}/` does not exist.

### 1.2 Per-module C++ (code / comment / blank / files)

| Module | Code | Comment | Blank | Files | comment ÷ (code+comment) |
|---|---:|---:|---:|---:|---:|
| `engine/render` | 7 224 | 5 077 | 1 319 | 53 | 41.3 % |
| `engine/world` | 6 930 | 4 208 | 1 079 | 42 | 37.8 % |
| `engine/gameplay` | 2 803 | 2 262 | 775 | 38 | 44.7 % |
| `engine/core` | 2 151 | 1 771 | 621 | 36 | 45.2 % |
| `engine/app` | 1 985 | 958 | 266 | 9 | 32.5 % |
| `engine/platform/render` | 1 956 | 1 404 | 432 | 38 | 41.8 % |
| `engine/anim` | 991 | 697 | 203 | 10 | 41.3 % |
| `engine/platform/physics` | 613 | 290 | 135 | 5 | 32.1 % |
| `engine/platform/audio` | 474 | 212 | 96 | 5 | 30.9 % |
| `engine/platform/input` | 326 | 197 | 108 | 7 | 37.7 % |
| `engine/platform/window` | 205 | 181 | 102 | 7 | 46.9 % |
| `engine/platform/llm` | 112 | 118 | 51 | 3 | 51.3 % |
| `engine/platform/anim` | 96 | 100 | 44 | 3 | 51.0 % |
| **`engine/physics`** | **58** | **99** | **35** | **3** | **63.1 %** |
| `engine/editor` | — | — | — | **0 (absent)** | — |

`tests/`: core 4 018 code / 13 files, render 4 644 / 18, sim 2 915 / 12,
character 870 / 3, app 134 / 1.

### 1.3 The comment ratio is the finding — but only half of it is rationale

`engine/` C++ is 40.4 % comment by the naive measure. Split by kind:

| Kind | Lines | Share of all non-blank C++ |
|---|---:|---:|
| Mandatory file header + UPD blocks | 10 947 | 17.9 % |
| Inline rationale comments | 12 094 | 19.8 % |
| Code | 38 146 | 62.3 % |

So **inline rationale ÷ (inline rationale + code) = 12 094 / 50 240 = 24.1 %** —
about one explanatory line per four code lines. That is genuinely heavy and, on
reading, genuinely useful: the comments carry *why*, with measured numbers and
rejected alternatives, not restatements of the code.

The other half is the price of Rule 15's header contract multiplied by Rule 21's
small files, and it is **not** uniformly cheap. Header/UPD lines ÷ code lines:

| Module | header lines | code lines | ratio |
|---|---:|---:|---:|
| `engine/physics` | 92 | 58 | **1.59** |
| `engine/platform/anim` | 98 | 96 | **1.02** |
| `engine/platform/llm` | 109 | 112 | 0.97 |
| `engine/platform/window` | 193 | 205 | 0.94 |
| `engine/gameplay` | 1 709 | 2 803 | 0.61 |
| `engine/core` | 1 155 | 2 151 | 0.54 |
| `engine/render` | 2 373 | 7 230 | 0.33 |

Four modules carry more (or nearly as much) mandatory boilerplate as code.

### 1.4 Largest files, and Rule 21

Rule 21: one responsibility, ~300 lines, **hard limit 800 LOC**.
8 of 307 C++ files exceed 800 *total* lines; 3 exceed 800 *code* lines.

| Total | Code | File |
|---:|---:|---|
| 2 607 | 1 725 | `tests/render/ProcFloraTests.cpp` |
| 1 987 | 1 309 | `tests/core/ForestStandTests.cpp` |
| **1 896** | **1 182** | **`engine/app/sources/App.cpp`** |
| 1 097 | 764 | `tests/core/WorldgenV2Tests.cpp` |
| 1 097 | 683 | `engine/render/sources/ProcFlora.cpp` |
| 1 052 | 643 | `engine/world/sources/WorldgenScatter.cpp` |
| 920 | 612 | `engine/render/sources/RenderSystem.cpp` |
| 824 | 437 | `engine/world/sources/WorldgenMacro.cpp` |

The registry defines `FILE_HARD_LIMIT`, and grep finds **zero** references to it
in any source or tool. The limit is enforced by nothing.

Largest documents: `docs/design/LANDSCAPE.md` 6 871, `docs/specs/flora.md` 2 014,
`docs/specs/render.md` 1 154, `docs/design/WEATHER.md` 1 047, `docs/NUMBERS.md` 810.

---

## 2. Coupling

Built by parsing every `#include "engine/..."` / `"games/..."` line in all 285
C++ translation units and headers: **69 distinct module→module edges**.

### 2.1 Module dependency matrix (include counts, row includes column)

Platform modules are split into `:iface` (the pure contract) and `:impl` (the
backends), because the DAG treats them differently.

| from ↓ / to → | core | world | render | physics | anim | gameplay | app | plat:iface | plat:impl |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `engine/core` | 28 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `engine/world` | 44 | 100 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `engine/render` | 40 | 0 | 70 | 0 | 0 | 0 | 0 | 10 (render) | 0 |
| `engine/physics` | 2 | 0 | 0 | 2 | 0 | 0 | 0 | 2 (physics) | 0 |
| `engine/anim` | 6 | 0 | 0 | 0 | 13 | 0 | 0 | 1 (render) | 0 |
| `engine/gameplay` | 53 | 4 | 1 | 5 | 0 | 59 | 0 | 7 (audio/input/physics) | 0 |
| `engine/app` | 10 | 4 | 10 | 2 | 3 | 14 | 10 | 6 | 4 |
| `tests` | 70 | 38 | 31 | 13 | 9 | 27 | 2 | — | 22 |

Reading the important rows: **`core` includes nothing but itself. `world`
includes nothing but `core` and itself. Every `render`/`physics`/`anim` edge that
leaves its own module goes to `core` or to a platform *interface*, never to a
backend.** `gameplay` reaches down into `{world, physics, anim, render}` and into
platform interfaces, exactly as the DAG permits. `app` is the only module that
touches platform `:impl`, which is what a composition root is for.

### 2.2 DAG conformance — every check, and its result

Declared: `app → editor → gameplay → {world, physics, anim, render} → platform interfaces → core`,
with `core` on std+glm only, `world` on `core` only, and backends the sole home of
third-party headers.

| # | Check | Violations |
|---|---|---:|
| 1 | `engine/core` includes anything outside `engine/core` | **0** |
| 2 | `engine/world` includes anything outside `{core, world}` | **0** |
| 3 | Any engine layer (`core/world/render/physics/anim/gameplay`) includes a platform **backend** (`sources/<lib>/`) | **0** |
| 4 | Back-edge: a lower layer includes a higher one (e.g. `render` → `gameplay`) | **0** |
| 5 | Sibling edge inside `{world, physics, anim, render}` | **0** |
| 6 | A platform **interface** includes anything (must pull in nothing external) | **0** |
| 7 | Third-party header (`bgfx`, `bx`, `bimg`, `Jolt`, `ozz`, `GLFW`, `miniaudio`, `llama`, `imgui`) outside `engine/platform/*/sources/` | **0** |
| 8 | Interface folder outside `engine/platform/` (Rule 0) | **1** (see below) |

**The declared DAG is obeyed.** Third-party isolation is total: all 27
third-party includes live in five backend directories — Jolt ×18
(`platform/physics/sources/jolt/`), GLFW ×4, bgfx/bx/bimg ×4, miniaudio ×1 — and
doctest ×49 confined to `tests/`. glm appears in 13 modules, which Rule 2 permits.

*Control on that count.* A naive grep for the library names finds 137 hits,
5 of them in `engine/app/` — which would be a Rule 1 violation if real. They are
not: all 5 are `#include "engine/platform/<mod>/sources/<backend>/Create*.h"`
(`App.cpp:126,129,131,134,137`), i.e. engine paths whose *directory* is named
after the backend. Those are the factory headers the composition root is
supposed to include. The remaining 105 false positives are the same effect
inside the backend directories themselves. Distinguishing "path contains the
library's name" from "include resolves to the library" is the whole content of
this check, which is why the number is 27 and not 137.

The single Rule 0 exception is `engine/core/ecs/sources/ComponentPool.h:54-62`,
which declares `IComponentPool` with 5 pure-virtual methods. This is type erasure
for the ECS's heterogeneous pool storage — the one construction that genuinely
cannot be expressed otherwise, and it satisfies the interface principle's third
test (dependency inversion) even though Rule 0 forbids its location. **The rule is
wrong here, not the code.** It is also the *only* `virtual` outside
`engine/platform/` in the entire engine.

### 2.3 ⚠ The DAG is obeyed by discipline, not by mechanism

`docs/ARCHITECTURE.md:107` states: *"CMake enforces this: each layer is a target;
forbidden includes fail the build."*

**It does not.** `CMakeLists.txt:88-92` defines `dfn_headers` as an INTERFACE
target whose include directory is the **repository root**, and every `dfn_*`
target links it. `engine/core/CMakeLists.txt:37` is
`target_link_libraries(dfn_core PUBLIC dfn_headers)` — so `dfn_core` can
`#include "engine/render/sources/RenderSystem.h"` and it will compile. Only a
symbol that needs linking would fail, and this codebase is header-heavy enough
that a large class of violations would link fine too. `engine/render` links only
`dfn_headers` (not `dfn_core`) while including 40 `core` headers.

This is Rule 39's own lesson pointed at the architecture: *a comment asserting
two things are the same is not a mechanism that makes them the same.* The clean
result in §2.2 is a **discipline** result. Six agents held the line for two days
by hand. The first agent who does not will not be told.

### 2.4 Fan-in and fan-out

Fan-out = distinct modules included; fan-in = distinct modules that include this one
(include counts in brackets).

| Module | fan-out | fan-in |
|---|---|---|
| `engine/app` | **16 (57)** | 1 (2) |
| `tests` | 14 (214) | 0 |
| `engine/gameplay` | 7 (70) | 2 (41) |
| `engine/render` | 2 (50) | 3 (42) |
| `engine/anim` | 2 (7) | 2 (12) |
| `engine/world` | 1 (44) | 3 (46) |
| `engine/physics` | 2 (4) | 3 (20) |
| **`engine/core`** | **0 (0)** | **9 (227)** |

This is the shape a layered engine is supposed to have: one sink with zero
fan-out and the highest fan-in, one source (`app`) with the highest fan-out and
almost no fan-in.

### 2.5 The de-facto coupling hubs — the riskiest files to change

| Includers | Header |
|---:|---|
| **74** | `engine/core/config/sources/Constants.h` |
| 29 | `engine/core/ecs/sources/World.h` |
| 21 | `engine/core/components/sources/Components.h` |
| **18** | `engine/physics/sources/CollisionLayers.h` |
| 17 | `engine/core/events/sources/EventBus.h` |
| 17 | `engine/core/serialization/sources/ContentHash.h` |
| 17 | `engine/core/ecs/sources/EntityId.h` |
| 16 | `engine/gameplay/sources/PlayerMovement.h` |
| 16 | `engine/platform/render/interfaces/IRenderer.h` |
| 16 | `engine/core/math/sources/SurfaceField.h` |
| 14 | `engine/world/sources/Worldgen.h` |
| 14 | `engine/render/sources/ProcMesh.h` |
| 13 | `engine/world/sources/WorldgenMacro.h` |
| 12 | `engine/world/sources/Chunk.h` |
| 11 | `engine/world/sources/TestbedLayout.h` |

`Constants.h` at 74 includers is the single most-coupled artifact in the project,
and it is *generated from `docs/NUMBERS.md`*. That means **a row edit in a
Markdown file recompiles roughly a quarter of the codebase and can silently
change behaviour in any of 74 translation units.** This is precisely the surface
Rule 37 describes, and it is the largest one in the repo.

Note the fourth entry: `engine/physics/sources/CollisionLayers.h` has 18
includers and lives in a module containing 58 lines of code.

### 2.6 Cross-zone coupling (Rule 25)

Zone→zone include counts. `engine/core/components` is attributed to `lead` per
Rule 25, which is why `core → lead` is non-zero.

| from ↓ / to → | core | render | sim | character | lead |
|---|---:|---:|---:|---:|---:|
| **core** | 170 | 0 | 0 | 0 | 1 |
| **render** | 38 | 114 | 0 | 0 | 3 |
| **sim** | 52 | 2 | 84 | 0 | 8 |
| **character** | 5 | 1 | 0 | 15 | 1 |
| **lead** | 14 | 17 | 23 | 3 | 11 |
| **tests** | 101 | 38 | 56 | 10 | 9 |

Every cross-zone edge points *down* the DAG. There is no zone pair that reaches
into each other. The only edges worth naming:

- `sim → lead` ×8 and `render → lead` ×3 and `character → lead` ×1 are all the
  same file: `engine/core/components/sources/Components.h`. Three zones depend on
  one lead-owned header, which is the intended shared-component design, but it
  means **any change to `Components.h` requires a four-way sync** under Rule 26.
- `sim → render` ×2: `engine/gameplay` → `engine/render/sources/ProcMesh.h` and
  `IInput.h`. Both are DAG-legal (`gameplay` may include `render`).
- `character → render` ×1: `engine/anim/sources/BodyMesh.h:45` →
  `IRenderer.h`. Legal (an engine layer may include a platform interface).

**Rule 25 contains an ownership collision.** `engine/anim` is assigned to `sim`
(row 3) *and* to `character` (row 5) in the same table
(`docs/ARCHITECTURE.md:322,324`). Two agents own the same directory by contract.

---

## 3. Universality and quality

### 3.1 ⚠ The persistence pillar is 100 % declaration and 0 % implementation

This is the most alarming finding in the audit and it leads.

- `engine/core/serialization/sources/BinaryWriter.h` declares 15 methods.
  `BinaryReader.h` declares its counterpart. **`grep -rn 'BinaryWriter::\|BinaryReader::' engine tests` returns 0 results.**
  There is no `.cpp`. Not one method is defined anywhere in the repository.
- **6 files include `BinaryWriter.h` and 5 include `BinaryReader.h`.** None of
  them can link.
- `engine/world/sources/WorldFormat.h` (119 lines, the `.dfw` world format) and
  `engine/world/sources/SaveDelta.h` (153 lines, the `.dfs` save format) have no
  `.cpp` and appear in **0** test files.
- The save/load round-trip tests exist and are **compiled out**:
  `tests/sim/InteractionTests.cpp:466-471` sets `DFN_SIM_HAVE_BINARY_IO 0` with
  the note *"BLOCKED, NOT SKIPPED … these two cases cannot LINK today"*.

Rule 7 ("version migration functions exist from day one"), Rule 13's save-delta
determinism, and Q56's "saves store a delta, never a world copy" are all
architecture with no implementation behind it. **The game cannot save.** The
honesty is exemplary — the blocked test is documented rather than deleted, which
is exactly right — but 516 lines of header contract with zero executable code is
a pillar, not a detail.

### 3.2 ⚠ The RPG layer is a facade: 488 lines of headers with zero includers

| Header | Lines | Files that include it |
|---|---:|---:|
| `engine/gameplay/sources/NpcAction.h` | 201 | **0** |
| `engine/gameplay/sources/Stats.h` | 116 | **0** |
| `engine/gameplay/sources/Dialogue.h` | 101 | **0** |
| `engine/gameplay/sources/NpcComponents.h` | 70 | **0** |

`Stats.h:107` declares `void record_use(Skills&, Skill, uint32_t);` — never
defined, never called. `Condition.h` (102 lines) is included by exactly one file:
`Dialogue.h`, which is itself included by nothing.

Rule 15 states *"There is no way to control an NPC except by enqueueing a typed
`NpcAction`. Direct mutation of NPC state outside the action executor is a
violation."* There is no action executor, no NPC system, and no NPC. The
invariant is currently unfalsifiable — which by Rule 30 makes it a description,
not an invariant.

For a project described as "a first-person open-world RPG (Skyrim-like play)":
stats, skills, dialogue, quests and NPCs together are **488 dead lines**, while
terrain, flora and rendering are 14 154 live ones. The engine is presently a
**landscape renderer with a walking camera**, and the audit should say so plainly.

### 3.3 The composition root, quantified

`engine/app/sources/App.cpp` — 1 896 total lines, 1 182 code. **2.4× the Rule 21
hard limit**, and the largest non-test file in the engine.

- `App::run()` spans lines **1305–1870 = 565 lines in one function**.
- `App::enter_world()` spans lines **451–904 = 453 lines in one function**.
- `App.h` declares **44 member variables**. By inspection, **15 of them
  (34 %)** are capture/playtest/debug instrumentation: `capture_pending_`,
  `capture_dir_`, `capture_after_s_`, `capture_after_elapsed_`,
  `capture_then_close_`, `captures_written_`, `close_after_flush_`,
  `menu_shot_frames_`, `restore_`, `restore_target_`, `restore_attempts_`,
  `body_probe_`, `mirror_puppet_`, `pt_dir_`, `pt_shots_`.
- One member is documented dead: `App.h` — `int restore_attempts_ = 0; //
  vestigial after the teleport fix; kept at 0`.

**Rule 10 violation, with file and line:** `engine/app/sources/App.cpp:169`

```cpp
static std::unordered_map<uint64_t, ChunkPhysics> g_chunk_physics;
```

File-scope mutable game data, outside the ECS `World`, keyed by packed chunk
coordinate. Rule 10 says *"No global mutable state. No singletons holding game
data."* This is also the exact failure Rule 35's state clause names — *"the
composition root must never reconstruct bookkeeping a subsystem already holds"* —
because `world::ChunkManager` already tracks chunk residency. Two copies of one
fact, and they drift silently.

Two lesser instances: `engine/render/sources/ScatterBatcher.cpp:94`
(`static std::set<size_t> reported;` — diagnostic dedup, non-deterministic across
runs, not thread-safe) and `engine/app/sources/Localization.cpp:33,40` (two
function-local `static std::unordered_map`s holding the string table — a
singleton holding game data).

`tests/app/` contains **one** file, 134 code lines, against 1 985 code lines of
`engine/app`. **test code ÷ impl code = 134 / 1 985 = 0.07**, the lowest in the
project by a factor of six.

### 3.4 Reusability: is `engine/` game-agnostic?

Mostly yes in *structure*, and no in *content*.

**The structure is right.** The world generator is a set of generic passes
parameterised by a `TestbedLayout` field on `WorldGenParams`
(`engine/world/sources/Worldgen.h:79`), with a `StandId` selector
(`TestbedLayout.h:72`) already carrying a second stand (`Forest`). Nothing about
the algorithms is Daggerfall-specific.

**The content is in the wrong repository half.** `games/daggerfall_n/` contains
**0 lines of C++** — only 453 lines of JSON assets, 25 `.wav` files and a 51-line
localization table. Everything game-specific therefore lives in `engine/`:

- **`engine/world/sources/TestbedLayout.h` — 441 lines of authored map data**:
  the hamlet "Vaelmere" at (360, 500), Ravenscar Crag, the Backbarrow, the
  Harrowward castle, 6 named sites, 5 corridor polylines, 8 tunnel waypoints,
  and 155 float literals. This is a specific game world compiled into the
  reusable engine library. Rule 5 says content lives in data files; the header's
  own defence ("coordinates are layout data, not tunable constants — NUMBERS.md
  is not their home") answers the wrong objection: the issue is not NUMBERS.md,
  it is `engine/` vs `games/`.
- `engine/render/sources/Tour.cpp:553-558` names acceptance steps
  `"crag_from_vaelmere"` against this world's landmarks.
- **27 files in `engine/`** mention this world's proper nouns (Ravenscar,
  Vaelmere, Backbarrow, Harrowward, Corvane, Maddren) — 33 occurrences. Most are
  in comments and are harmless; `TestbedLayout.h` and `Tour.cpp` are not.
- `crag` has been promoted to API vocabulary: **118 occurrences in identifiers**
  (`CragStamp`, `crag_treeline`, …) across `engine/world` and `engine/render`.
  A second game would inherit a type named after this game's mountain.
- `engine/gameplay/sources/Stats.h:55` hardcodes the 8 Daggerfall attributes and
  12 skills as C++ enums — content in C++ per Rule 5, and Rule 6 says adding
  content must not require recompiling.
- `engine/app/sources/App.cpp:312,347,361` hardcode `"Daggerfall N"` as the
  window title and two `games/daggerfall_n/assets/...` paths.

**Verdict:** `engine/` is *architecturally* reusable and *literally* not. The fix
is small and mechanical — move `TestbedLayout`'s defaults into a data file under
`games/daggerfall_n/assets/` and load them — and it gets smaller every day it is
done sooner.

### 3.5 Rule 14 / Rule 5: numbers and content in C++

- `docs/NUMBERS.md` holds **456 table rows**, generating **435 `inline constexpr`
  constants** into `dfn_generated/Constants.h`.
- **135 of those 435 (31 %) are referenced nowhere** in `engine/`, `tests/`,
  `games/` or `tools/`. `docs/NUMBERS.md` itself marks 37 rows as "НЕ ПОСТРОЕНЫ",
  so the registry is honest about roughly a quarter of its own drift; the real
  figure is **3.6× larger than the registry admits**. Unused blocks include all
  8 `LR_*` regional-mountain rows, all 9 `MASSER_*` lunar rows, the
  `BENCH_VEG_*` and `BORDER_*` families, and `FILE_HARD_LIMIT`.
- Going the other way: **2 016 non-benign float literals** in `engine/`
  (comments and strings stripped; `0.0/1.0/0.5/2.0/3.0/4.0/10.0/100.0/0.25/1.5`
  excluded as structural). Concentrated in `engine/render` (1 292) and
  `engine/world` (434). Worst files: `ProcMesh.cpp` 289, `FloraSpecies.cpp` 233,
  `TestbedLayout.h` 155, `ProcTexture.cpp` 131, `ProcFlora.cpp` 128.

Not all 2 016 are Rule 14 violations — much of `FloraSpecies.cpp` is a species
shape table, which is *content* rather than a *constant*, so it is Rule 5's
problem instead of Rule 14's. Either way, the ratio to state is
**2 016 literals in code ÷ 435 constants in the registry = 4.6**, and 31 % of the
registry is unreferenced. The registry is not currently winning.

### 3.6 Interface discipline

The project's own test: an interface exists only for (1) third-party isolation,
(2) multiple interchangeable runtime implementations, (3) dependency inversion.

| Interface | Methods | Backends | Test 1 | Test 2 | Verdict |
|---|---:|---|---|---|---|
| `IWindow` | 10 | glfw, null | ✅ | ✅ | earns it |
| `IInput` | 12 | glfw, null | ✅ | ✅ | earns it |
| `IRenderer` | 27 | bgfx, null | ✅ | ✅ | earns it |
| `IAudio` | 25 | miniaudio, null | ✅ | ✅ | earns it |
| `IPhysics` | 31 | jolt, null | ✅ | ✅ | earns it |
| **`IAnim`** | **17** | **null only** | ❌ | ❌ | **fails all three** |
| **`ILlm`** | **15** | **null only** | ❌ | ❌ | **fails all three** |

`IAnim` (115 lines) and `ILlm` (146 lines) each have **exactly one includer in
the repository — their own null factory** (`CreateNullAnim.h:31`,
`CreateNullLlm.h:31`). Neither is instantiated: `App.h:169-173` owns
`unique_ptr`s to `IWindow`, `IInput`, `IRenderer`, `IPhysics`, `IAudio` and
nothing else. `engine/anim` mentions `IAnim` only in two `docs/README.md` lines
as a future plan; it does its own posing without it.

These are 261 lines and 32 pure-virtual methods designed against ozz-animation
and llama.cpp, neither of which is integrated. They are not wrong to exist —
Rule 3 mandates a null backend and the contracts are thoughtful — but they are
**speculative contracts frozen (Rule 26) against libraries whose real APIs have
not been met yet**, which is historically how abstraction layers acquire the
wrong shape.

**Where an interface is missing:** nowhere obvious. The tempting case is a
"terrain height source" abstraction, and Rule 39's three-drifted-copies incident
is the argument for it — but the correct fix there is one shared *function*, not
an interface, and the project's own interface principle says so. The audit finds
no place that should have an interface and lacks one.

### 3.7 Rule 9 — systems as free functions

**Clean, and checked rather than assumed.** Zero classes outside the composition
root store a platform interface as a member (regex over all engine headers for
`I{Renderer,Physics,Audio,Input,Window,Anim,Llm}` members: **0 hits**). The
`unique_ptr`s to interfaces exist only in `App.h:169-173`.

`render::RenderSystem` is a class, but it takes `platform::IRenderer&` as a
parameter on all 10 of its public methods and stores none — the class holds GPU
handle bookkeeping only, which is state a renderer legitimately owns. This is
the rule applied correctly rather than nominally.

### 3.8 Test coverage, honestly assessed

**48 suites are registered** (`tests/{core,render,sim,character,app}.cmake`:
13 + 18 + 13 + 3 + 1), containing **441 `TEST_CASE`s** and **2 425 assertions**.
None of those three numbers is a coverage measure and none is reported as one.

**Reach.** Of 137 headers in `engine/`, **90 (66 %) are directly included by at
least one test file.** Per zone:

| Zone | headers reached / total |
|---|---|
| `engine/anim`, `engine/physics` | 5/5, 2/2 (100 %) |
| `engine/render` | 22/27 (81 %) |
| `engine/gameplay` | 17/23 (74 %) |
| `engine/world` | 16/23 (70 %) |
| `engine/core` | 15/25 (60 %) |
| `engine/app` | 2/4 (50 %) |
| `engine/platform/render` | 3/8 (38 %) |
| **`engine/platform/window`, `engine/platform/input`** | **1/5 (20 %)** |

**Subsystems with no test at all** (verified by symbol grep across `tests/`, not
just by include):

| Subsystem | test files mentioning it |
|---|---:|
| `world/SaveDelta` (save format) | **0** |
| `world/WorldFormat` (`.dfw` world file) | **0** |
| `gameplay/Dialogue`, `gameplay/Condition` | **0** |
| `gameplay/Stats` (`Attributes`, `Skill`) | **0** |
| `render/VoxelMesher`, `render/WindModel`, `render/DebugDraw` | **0** |
| `app/Menu` | **0** |
| `core/types/TypeId` | **0** |
| `core/serialization/Binary{Reader,Writer}` | 1, **compiled out** |

Thin by ratio (test code ÷ impl code, both C++ code lines):

| Zone | test code | impl code | ratio |
|---|---:|---:|---:|
| character | 870 | 1 087 | 0.80 |
| sim | 2 915 | 4 060 | 0.72 |
| render | 4 644 | 9 711 | 0.48 |
| core | 4 018 | 9 081 | 0.44 |
| **lead (app)** | **134** | **1 985** | **0.07** |

The `app` figure is the one to act on: the largest single file in the engine, the
one holding a global mutable map and 44 members, has one 134-line test.

*Assertion-quality findings — see §3.9.*

### 3.9 Assertion quality and controls

Structurally the suite is sound: no file has zero assertions, density runs
2.1–10.6 assertions per case, and the "disabled tests" sweep comes back
**clean** — `WARN(` **0 occurrences**, `#if 0` **0**, commented-out `TEST_CASE`
**0**, `doctest::skip` **1 and it is `skip(false)`**, `should_fail(true)` **1**
and correctly used as a documented XFAIL (`tests/core/VoxelTests.cpp:355` with
its flip condition at `:345-350`). All 48 files are registered with CTest.

The problem is not skipped tests. It is assertions that pass by construction.

#### The `Approx().epsilon()` exposure (Rule 40)

Confirmed against the vendored header (`doctest.h:3983-3987`): the tolerance is
`eps × (scale + max(|lhs|, |expected|))` with `scale = 1.0`. Note the `max()` —
this is slightly **worse** than Rule 40 states, because a drifting measured
value partly buys its own tolerance.

| Measure | Count |
|---|---:|
| `Approx(` occurrences | 384 |
| …at the **default** epsilon (1.19e-5) — tight, not a concern | **285** |
| `.epsilon(` occurrences | 100 (7 in comments → **93 live**) |
| Live `.epsilon()` on a **difference / residual / gap / clearance / distance** | **≥34 of 93 (37 %)** |

Those 34 are Rule 40 violations by definition, whatever their band. Worked
examples, band computed:

| Site | Assertion | Admitted band | Why it matters |
|---|---|---|---|
| `tests/character/ClipTests.cpp:490` | `chest_ahead_of_eye == Approx(0.026f).epsilon(0.02)` | **±0.0205 m on a 0.026 m gap** — width 158 % of the value | a clearance |
| `tests/sim/JoltPhysicsTests.cpp:128,146` | `position.y == Approx(GROUND_Y).epsilon(0.02)`, `GROUND_Y=10` | **±0.22 m** | *"stands on it without sinking"* passes a character 22 cm into a flat floor |
| `tests/sim/JoltPhysicsTests.cpp:188` | `hit.distance == Approx(20.0f).epsilon(0.01)` | **±0.21 m** | a distance that is geometrically exactly 20.000 m |
| `tests/sim/MovementSolidTests.cpp:150` | `rise == Approx(target).epsilon(0.10)`, `JUMP_HEIGHT=1.0` | **(0.800, 1.222) m** | comment says *"10 % is the band"*; it is −20 %/+22 %, a 42 cm window |
| `tests/render/ProcFloraTests.cpp:254` | `lo >= Approx(floor_m).epsilon(0.05)`, `CANOPY_CLEARANCE_MIN=2.2` | **±0.16 m** → foliage at 2.04 m | the walkability floor leaks 16 cm |
| `tests/core/WorldgenV2Tests.cpp:581` | `height_at == Approx(sp.height).epsilon(0.001)` | at h ≈ 100 m, **±0.101 m** | drawn-vs-analytic terrain agreement — exactly the §3.10 class of bug |
| `tests/render/SkyModelTests.cpp:65` | `full.y == Approx(0.0f).epsilon(0.08)` | ±0.08 on a unit vector = **±4.59°** | *"on the horizon"* |

`tests/character/ClipTests.cpp:173-179` and `:262-275` already contain the
correct remediation pattern — explicit absolute bounds with the arithmetic
spelled out, including a note that a prior `.epsilon(0.25)` on an ankle height
admitted ±0.268 m. **The team has already solved this once and not swept the
rest of the suite.** `:490` in that same file is a leftover.

#### ⚠ Assertions that cannot fail — three verified in this audit

**1. Mathematically unfalsifiable.** `tests/sim/TunnelWalkTests.cpp:242`

```cpp
CHECK(down.position.y == doctest::Approx(deep.y).epsilon(0.35));
```

`deep = tunnel.points[3]` → `lift(30.0f)` = `21 + 9 × (115/52)` = **40.904 m**.
Admitted band = `0.35 × (1 + 40.904)` = **14.67 m**. But the raycast three lines
above starts at `deep.y + 1.0` and travels **6.0 m** downward, so
`down.position.y ∈ [35.90, 41.90]` — a maximum possible deviation of **5.0 m**.
With `REQUIRE(down.hit)` on `:241` already guaranteeing a hit, **no value the
raycast can physically return will fail line 242.** A tunnel floor 5 m out of
place passes.

**2. The clamp is inside the function under test.**
`tests/core/TimeTests.cpp:67,68,78,79`

```cpp
CHECK(ts.alpha() >= 0.0);  CHECK(ts.alpha() < 1.0);
// engine/core/time/sources/FixedTimestep.cpp:55-58
double FixedTimestep::alpha() const {
    const double a = accumulator_ / step_dt_;
    return a < 0.0 ? 0.0 : (a >= 1.0 ? 0.0 : a);
}
```

Unfalsifiable for any accumulator value, including a corrupted one. This matters
because the case at `:71-81` — *"catch-up clamp drops the excess after a stall
(spiral guard)"* — uses `alpha()`'s range as its **evidence that the excess was
dropped**, and that evidence is manufactured by the clamp rather than by the
drop. Also `TimeTests.cpp:97-99`: three `>= 0.0` tautologies in a case titled
*"clock ticks are non-negative and monotonic-ish"* that never compares `t2` to
`t1`. **The one property in the title is the one not tested.**

**3. The band is wider than the thing it guards.**
`tests/render/RenderSystemTests.cpp:246`. The comment at `:239-242` states the
purpose: *"the sideways part ROTATED with the body — a light left at the carrier
origin (or at the eye) casts no visible shadow at all, which is the bug this
whole feature exists to avoid."* Line `:243-245` pins the displacement
**magnitude** at 0.35 m with the default epsilon (correct and tight). Line `:246`
is the **only** assertion of **direction**:

```cpp
CHECK(env.point_lights[0].position.z == doctest::Approx(49.65f).epsilon(0.01));
```

Band = `0.01 × (1 + 49.65)` = **±0.5065 m**, wider than the entire 0.35 m
displacement. `z = 50.0` — **zero lateral offset, the exact bug named in the
comment** — satisfies it: `|50.0 − 49.65| = 0.35 < 0.5065`. So does a torch
displaced 0.35 m forward instead of right.

#### Zero-margin identities (Rule 30a)

`tests/render/ProcFloraTests.cpp:299-301` asserts
`species_crown_base(s) / species_nominal_height(s) >= BIRCH_CROWN_BASE_FRACTION_MIN`.
But `ProcFlora.cpp:905-907` defines `species_crown_base(s) =
species_nominal_height(s) * sp.crown_base_frac`, and `FloraSpecies.cpp:279` sets
`birch.crown_base_frac = f(config::BIRCH_CROWN_BASE_FRACTION_MIN)`. The
assertion reduces to **`X >= X`**. It cannot fail for any change to the mesh
builder; the enclosing case is titled *"flora: sizes stay inside the design
bands"* and these three lines never call `build_flora_mesh`. `:305` is the same
shape for pine and passes only through the `==` branch — `FloraSpecies.cpp:175`
admits it in prose: *"this species sits ON it."*

`tests/sim/StepFeelTests.cpp:242-247` compares three compile-time constants
(`WALK_SPEED < JOG_SPEED < RUN_SPEED`) and is labelled at `:240-241` as the
Rule 30 control for gait mapping. It tests the mapping's **inputs**, not its
outcome — Rule 38 exactly. The real control measures three distinct speeds
*through the movement code*.

#### ⚠ Metrics that fail open — green on a totally broken subsystem

- **A defect counter with no proof it can count.**
  `tests/sim/TunnelWalkTests.cpp:405,427` assert `tunnelled == 0` from
  `count_tunnelling_ticks` (`:356-386`), which contains
  `if (distance < 1e-4f) continue; // blocked outright: the correct outcome`.
  **A character that never moves at all** — spawned in solid rock, or with a
  broken `move_character` — takes that `continue` every tick and scores a
  perfect zero. "No tunnelling happened" and "the detector is dead" are
  indistinguishable. Nothing anywhere shows the function can return non-zero.
- `tests/core/WorldgenV2Tests.cpp:521-525` — the whole case is
  `CHECK(max_corridor_avg_slope(ctx) <= CORRIDOR_SLOPE_MAX)`.
  `WorldgenValidation.cpp:468-492` initialises `worst = 0.0f` and updates only
  inside `if (length > 0.0f)`. **A world with zero corridors returns 0.0 and
  passes** — the most complete possible failure of the corridor system scores
  perfectly.
- `tests/core/ForestStandTests.cpp:1121-1129` — `worst_clearance` starts at
  `1e9f`, minimised only inside a doubly-nested loop; empty routes leave it at
  `1e9` and `CHECK(worst_clearance > 0.0f)` passes.
- `tests/sim/TunnelWalkTests.cpp:345-348` — the case titled *"the player walks
  THROUGH the crag, not over it"* **never asserts the player got through.**
  `waypoint` is printed by `MESSAGE` at `:341` and read by no `CHECK`. A walk
  that stalls at 4/8 passes all four assertions — and `TestbedLayout.h:49`
  records that this exact failure has already happened once
  (*"sim_tunnel_walk stalled at 7/8 ungrounded"*).

The project understands the pattern and applies it inconsistently:
`landmark_visibility_fraction` (`WorldgenValidation.cpp:161`) returns
`open == 0 ? 0.0f : visible/open` and so **fails safe**; `LodSeamTests.cpp:206-209`
carries an explicit sample-count tripwire with the comment *"if this ever reads
zero, the table is measuring nothing"*. Those guards are exactly what the four
sites above lack.

#### Controls: better than expected

A mechanical scan (control comment, `CHECK_FALSE`/`REQUIRE_FALSE`, or a
negative-outcome assertion) finds evidence in **45 of 48 files (94 %)**, and
hand-verification of a sample confirms the mechanical count is not inflated:

- **Exemplary:** `tests/sim/PlaytestTests.cpp` is the best test file in the
  repository — 7 cases of which **6 are pure controls**, plus an explicit
  Rule 30a can-pass arm at `:101`, and a UPD header documenting the repair of a
  Rule 38 violation in its own control. `tests/render/TerrainLodTests.cpp:418-431`
  builds an all-culling frustum and asserts `draw() == 0` specifically to
  separate "culled" from "missing". `tests/core/CoarseLodTests.cpp:101-145`
  carries two named counterfactual arms. `tests/character/ClipTests.cpp:247-248,
  492-499` and `tests/sim/InteractionTests.cpp` (19 `CHECK_FALSE`, whole cases
  that are rejections) are strong.
- **Two files with no control at all:** `tests/render/SkyModelTests.cpp`
  (6 cases, ~30 assertions, every one a positive claim; no degenerate phase or
  time is ever fed in) and `tests/sim/CollisionCostTests.cpp` (**1 case, 112
  lines, whose only `CHECK` is `body.valid()` at `:108`** — it times four
  decimation levels and `MESSAGE`s the numbers. It is a benchmark registered as
  a test; nothing but a physics crash can turn it red).
- **Thresholds below every real failure:** `TunnelWalkTests.cpp:222` allows
  2.4× the measured cost, self-described as *"a CATASTROPHE GUARD, not a
  budget"* — honest, but a description. `:346` requires
  `ticks_under_rock > 600` in a 5400-tick loop, i.e. **11 %**, under a comment
  claiming *"most of the walk happens inside the massif"*.

**Verdict on coverage:** the pass count is not the story and the control
discipline is genuinely above average for a two-day codebase — but the suite has
a concentrated soft spot in `tests/sim/TunnelWalkTests.cpp`, and a systemic
`.epsilon()` exposure on 34 difference-typed quantities that the team has
already learned about once and not finished sweeping.

*Caveat on line numbers:* the working tree was live during this audit (other
agents editing). All cited lines were re-confirmed in a final pass except
`ForestStandTests.cpp` and `BodyTests.cpp`, which shifted ~12 and ~40 lines
mid-audit; the `ForestStandTests.cpp:1121-1129` citation is from the final read.

### 3.10 Duplication and shadow copies

**Rule 39 is not a solved problem — it is an open one.** A clone detector
(comments and whitespace stripped, ≥6 consecutive identical normalised lines)
over 236 engine files and 25 476 normalised lines finds **39 clone groups /
320 duplicated lines = 1.3 %**, which is a healthy figure. But the serious
findings are all *shorter* than 6 lines, which is the whole point: shadow copies
are short, and the metric that would catch them does not exist.

Eight comments in `engine/` assert that two things are the same. **Two of those
pairs have already diverged**; two are held together by a real mechanism; four
are latent.

#### ⚠ A. Two live splat pipelines that disagree (verified in this audit)

Both run every frame — `App.cpp:532` (voxel path) and `App.cpp:535` (heightfield
fallback for chunks with no voxel mesh) — and feed **the same shader**.
`engine/render/sources/VoxelMesher.h:27` claims the mapping "mirrors
TerrainMesher's `SurfaceClass` mapping **so the two sources cannot drift apart
visually**". There is no shared function; the comment is the only mechanism.

Four of five surface classes agree. The fifth does not:

```cpp
// path A — engine/render/sources/TerrainMesher.cpp:157-159
case math::SurfaceClass::GrassRockBlend:
    rock_w = BLEND_CLASS_ROCK_W;        // 0.5f  (TerrainMesher.cpp:61)

// path B — engine/world/sources/VoxelVolume.cpp:123-126
case math::SurfaceClass::GrassRockBlend:
case math::SurfaceClass::Grass:
default:
    return math::VoxelMaterial::Grass;  // → VoxelMesher.cpp:77, all weights 0.0f
```

**`GrassRockBlend` draws with rock weight 0.5 on one path and 0.0 on the other.**
`math::VoxelMaterial` (`VoxelField.h:52-58`) has only 5 members and no blend
value, so the class cannot survive the hop — the divergence is structural, not a
typo. The `default:` at `VoxelVolume.cpp:125` additionally defeats `-Wswitch`, so
a sixth `SurfaceClass` would silently become grass with no warning.

#### ⚠ B. Terrain render mesh and terrain collision mesh split every quad on opposite diagonals

`engine/platform/physics/sources/jolt/JoltPhysics.cpp:28` claims the grid
triangulation is "**identical to** the render mesher… collision and visuals
**cannot disagree**". Verified in this audit — they are not identical:

```cpp
// render  — engine/render/sources/TerrainMesher.cpp:201
{i00, i11, i10,  i00, i01, i11}        // shared diagonal i00–i11
// physics — .../jolt/JoltPhysics.cpp:330-331
(i00, i01, i10);  (i10, i01, i11)      // shared diagonal i01–i10
```

Winding is consistent (+Y up on both); only the diagonal differs, so the two
surfaces meet at quad corners and separate in the interior by
`|h00 + h11 − h01 − h10| / 2`. Measured against real worldgen output (16 chunks
of the 4×4 testbed, 129×129 samples, 2 m step, 262 144 cells):

| mean divergence | > 1 cm | > 5 cm | > 25 cm | > 1 m | worst |
|---:|---:|---:|---:|---:|---:|
| 0.0155 m | 9.69 % | 2.55 % | 1.18 % | 0.23 % | **18.56 m** |

**The tell is textbook Rule 39.** The only test of this path,
`tests/sim/JoltPhysicsTests.cpp:92`, builds its chunk with
`height_scale = 0.0f` (`:77`, *"flat: every sample decodes to the offset"*). On
flat ground the twist term is identically zero and the two triangulations agree
exactly — *"the place it does not apply is usually the case everyone is
testing."*

**Mitigating, and it matters:** the app has moved terrain collision to
`create_terrain_mesh_body` (`App.cpp:548`), which ferries render's own index
buffer — a real mechanism, correctly done. The divergent heightfield path
survives in `IPhysics.h:172`, `engine/physics/sources/TerrainCollision.cpp:74`
and two tests. It is **dormant, not shipping** — but it is armed.

#### C. Hand-rolled hashing: 18 implementations, 10 distinct algorithms

**SplitMix64 appears 7 times with byte-identical constants**
(`0x9E3779B97F4A7C15` / `0xBF58476D1CE4E5B9` / `0x94D049BB133111EB`):
`WorldgenNoise.h:41` (which declares itself canonical), `Worldgen.cpp:123`
(re-inlines it four lines after calling the canonical one at `:116`),
`WorldgenErosion.cpp:43` and `Dice.cpp:44` (both **deliberate** freezes under
Rule 13.1 / save-format stability — correct), `FloraField.h:97`,
`FloraBuild.cpp:55`, `FloraNeighbours.cpp:45`.

`FloraNeighbours.cpp:45` is a zero-risk deletion today: same namespace
`dfn::render`, and `FloraBuild.h:101` already exports an identical `mix64`. Both
carry the identical copy-pasted doc comment.

Also: **5 independent value-noise builders** (3 different fade curves, 2
different lattice→float conversions for the same job); the cloud hash duplicated
verbatim CPU↔GPU (`CloudModel.cpp:76-91` ↔ `shaders/dfn_env.sh:169-219`,
including octave weights and the mean/SD constants); and a **Bayer matrix
written 4×** where `dfn_env.sh:341-355` already has it and `fs_terrain.sc` /
`fs_path.sc` include that header and redefine it anyway.

One genuinely clean case: `ContentHash.h:44` (FNV-1a) is single-source and
respected, with `Localization.h:22-23` explicitly forbidding a second copy.

Separately, `RenderSystem.cpp:165-170` uses `x*K1 ^ (y*K2)` with no avalanche
step as an `unordered_map` key — low bits never mix, so bucket choice is decided
almost entirely by `y`. A correctness/performance smell independent of duplication.

#### D. Tests holding their own copy of the thing under test

Three cases where the test cannot catch drift because it transcribes the value
rather than reading it:

- `tests/character/ClipTests.cpp:74` — `p.foot_length * 0.25f; // BodyMesh
  FOOT_HEEL_RATIO`. Third copy of `0.25` after `BodyMesh.cpp:46` and
  `Clips.cpp:55`; drift in either engine copy leaves the suite green.
- `tests/render/PaletteTests.cpp:94-102` — `quantise_in`, a re-implementation of
  `BgfxPalette.cpp:185-197`. The palette is validated through the test's own
  quantiser, so changing the real one does not go red.
- `tests/sim/JoltPhysicsTests.cpp:92` — the flat chunk above.

#### E. Open-coded computations

| Lines | Sites |
|---:|---|
| 22 | `core/math/sources/FloraField.h:146-168` · `world/sources/WorldgenForest.cpp:123-144` — the same 1024-bin rank-equalisation CDF, differing only in the sampled noise fn and probe stride (i.e. exactly what a parameter is for). `WorldgenForest.cpp:102` calls it "the FloraField technique, applied to THIS construction"; the file does not include `FloraField.h`. |
| 11 | `world/sources/WorldgenHydrology.cpp:474-484` · `world/sources/WorldgenWater.cpp:99-109` — nearest-station 3×3 bin ring. Divergence axis already present: one divides by a compile-time `BIN_SIZE`, the other by a runtime `hydro.bin_size`; they agree only because `:455` assigns one to the other. |
| 9 | `app/sources/App.cpp:944-952` · `App.cpp:990-998` — mirror-puppet aim yaw, in one file, already asymmetric (one gated on `!p.aimed`). |
| 8 | `world/sources/Chunk.cpp:76-83` · `world/sources/VoxelMesh.cpp:64-71` — `math::VoxelMeshView` built field-for-field twice. |

Three clone groups (backend/null override declarations in
`BgfxRenderer.h`/`NullRenderer.h`, `JoltPhysics.cpp`/`NullPhysics.cpp`,
`GlfwWindow.h`/`NullWindow.h`) are structural mirrors of an interface, enforced by
`override`, and are **not** findings.

#### The two healthy cases, which show the project already knows the fix

- **LOD ladder** (`CoarseTerrain.h:61-66`): two zones must agree, and
  `tests/core/LodSeamTests.cpp:136-143` links both and pins them equal. A
  mechanism, not a comment.
- **Key enum** (`GlfwInput.cpp:44`): an exhaustive `switch` with no `default:`
  (`:129-130`), so `-Wswitch` catches a new `Key` at compile time. Note the
  contrast with `VoxelVolume.cpp:125`, where the `default:` throws that away.

And the codebase has already killed a table of exactly this kind and recorded
why: `ScatterBatcher.cpp:39-44, 81-88` deleted `species_radius` because "a table
that must agree with a mesh will disagree again" (stone tabled 0.5 vs ~0.78
built; birch 3.1 vs ~4.94), replacing it with a measured bounding circle and
keeping the old values as a Rule 30 failing control. **That is the correct fix
pattern for finding A**, already demonstrated in this repo.

### 3.11 Hygiene — measured, and it is good

| Measure | Value |
|---|---|
| `TODO` / `FIXME` / `HACK` / `XXX` in `engine/` + `tests/` | **0** across 67 654 lines |
| `#pragma … diagnostic` / `-Wno-` warning suppressions | **0** |
| Raw `new` in code | **3**, all in `platform/physics/sources/jolt/JoltPhysics.cpp:185,351,386` where Jolt's own refcounting requires it |
| Raw `delete` | **0** |
| `reinterpret_cast` / `const_cast` | 2 / 2 |
| `python3 tools/header_check.py --all` | **passes on every tracked file** |
| User-facing string literals in C++ | **1** (`Menu.cpp:180`, the `">"` cursor glyph) against 44 `loc()` call sites |
| Git history | 308 commits, all agent-authored |

Zero TODOs across 67 kloc is not the usual sign of a clean codebase — usually it
means TODOs are not being written down. Here it appears to be real: the project
writes its open problems as prose in UPD blocks and in `docs/BOARD.md` instead,
and `tests/sim/InteractionTests.cpp:460` ("BLOCKED, NOT SKIPPED") is the pattern.

### 3.12 Documented-vs-actual mismatches

| `docs/ARCHITECTURE.md` says | Actually |
|---|---|
| L107: "CMake enforces this: forbidden includes fail the build" | It does not (§2.3) |
| L73: `editor/` — in-game editor, Dear ImGui allowed here | **`engine/editor/` does not exist**; ImGui is never fetched or included |
| L69: `physics/` — "character controller, collision layers" | The character controller is `engine/gameplay/sources/PlayerMovement.{h,cpp}` (16 includers). `engine/physics` is 3 files / 58 code lines |
| L45: "skeletal animation via ozz-animation, optional local LLM via llama.cpp" | Neither is integrated; `IAnim`/`ILlm` have null backends only |
| L79: `games/daggerfall_n/src/{systems,components}/` | Does not exist; `games/` has 0 C++ |
| L322/L324 (Rule 25) | `engine/anim` is assigned to two owners at once |

---

## 4. Summary

### Lead finding — two live Rule 39 shadow copies, one of them on screen

Ahead of everything else in this summary, because it is a **defect, not a
weakness**, and it is verified twice (§3.10 A and B):

- `GrassRockBlend` terrain draws with **rock weight 0.5 on the heightfield path
  and 0.0 on the voxel path**, both live on the same chunk-load branch
  (`App.cpp:532` vs `:535`), feeding the same shader. The comment at
  `VoxelMesher.h:27` promising the two "cannot drift apart visually" is the only
  thing that was ever holding them together.
- The terrain **render** mesh and the terrain **collision** heightfield mesh
  split every grid quad on **opposite diagonals**
  (`TerrainMesher.cpp:201` vs `JoltPhysics.cpp:330-331`), diverging by up to
  **18.56 m** on real worldgen output — under a comment claiming they are
  "identical" and "cannot disagree". This path is currently dormant (the app
  uses the voxel mesh for collision), and its only test uses a **flat** chunk,
  where the divergence is identically zero.

### The three weakest things

1. **The persistence pillar does not exist** (§3.1). `BinaryWriter`/`BinaryReader`
   have zero method definitions in the repository; `WorldFormat.h` and
   `SaveDelta.h` have no `.cpp` and no test; the round-trip tests are compiled
   out. The game cannot save, and Rules 7 and 13 are unbacked.
2. **The RPG layer is 488 dead lines** (§3.2). `NpcAction.h`, `Stats.h`,
   `Dialogue.h`, `NpcComponents.h` have zero includers between them;
   `record_use()` is declared and never defined. Rule 15's central NPC invariant
   is currently unfalsifiable.
3. **The composition root is carrying the game** (§3.3). `App.cpp` at 1 896 lines
   is 2.4× the hard limit, `run()` is 565 lines, `App.h` has 44 members of which
   15 are test instrumentation, `App.cpp:169` holds a global mutable
   `g_chunk_physics` duplicating `ChunkManager`'s bookkeeping (Rules 10 and 35),
   and the whole of `engine/app` is covered by one 134-line test (ratio 0.07).

### The three strongest things

1. **The layering is genuinely clean and it is now measured** (§2.2). Across
   1 716 include lines: `core` includes nothing, `world` includes only `core`, no
   engine layer touches a backend, there are no back-edges and no sibling edges,
   and all 27 third-party includes sit in five backend directories. Six agents
   held a hard contract for two days without a single breach.
2. **Rule 9 is obeyed in substance, not nominally** (§3.7). Zero classes outside
   `App` store a platform interface. `RenderSystem` takes `IRenderer&` on every
   call. This is the discipline that makes the null backends and headless tests
   actually work.
3. **The comments are load-bearing and the hygiene is exceptional** (§1.3, §3.11).
   24 % inline rationale carrying measured numbers and rejected alternatives;
   zero TODO/FIXME/HACK; zero warning suppressions; three raw `new`s, all forced
   by Jolt; a header contract that passes on every file. The `core` ECS
   (`World.h`) is the best-written code in the project — documented
   preconditions, batch APIs matching Rule 11, generational ids, deleted copy.

### What I would fix first, in order

0. **The two divergent shadow copies** (§3.10 A, B) — these are defects, not
   debt. `VoxelVolume.cpp:123` (drop the `default:` while you are there, so
   `-Wswitch` starts working) and `JoltPhysics.cpp:324-333` (flip the diagonal to
   match `TerrainMesher.cpp:201`, and give `JoltPhysicsTests` a non-flat chunk —
   its flat one is the reason nobody saw this). Then delete
   `FloraNeighbours.cpp:45` (free — `FloraBuild.h:101` already exports the
   identical `mix64`) and lift one `mix64` into `engine/core/math/`, which
   collapses 5 of the 7 SplitMix64 copies.
1. **Make CMake actually enforce the DAG** (§2.3). Today's clean graph is the
   perfect moment: the enforcement lands green, so it can never be argued down
   later as "too disruptive". Give each layer its own `target_include_directories`
   instead of the repo-root `dfn_headers`, or add a CI check that runs the
   include parser from this audit. *Why first: it is the cheapest, and it is the
   only item that protects every other item.*
2. **Delete or implement the dead pillars** (§3.1, §3.2). 1 004 lines of
   header (`Binary*.h`, `WorldFormat.h`, `SaveDelta.h`, `NpcAction.h`, `Stats.h`,
   `Dialogue.h`, `NpcComponents.h`) currently describe a system that does not
   exist, and they are frozen public contracts under Rule 26 — so they cost
   review attention on every sync while backing nothing. Either implement
   `BinaryWriter`/`BinaryReader` (which unblocks the two written-and-ready tests
   at `InteractionTests.cpp:472-540` immediately) or move the headers to
   `docs/specs/` where a design is allowed to be a design.
3. **Move `TestbedLayout`'s defaults out of `engine/`** (§3.4). 441 lines of one
   game's map compiled into the reusable library. The injection point already
   exists (`WorldGenParams::layout`), so this is a load-from-data change, not a
   redesign — and it is the single edit that converts "architecturally reusable"
   into "reusable".
4. **Split `App.cpp`** (§3.3), starting by lifting `g_chunk_physics` into
   `ChunkManager` (Rule 35) and moving the 15 capture/playtest members into a
   separate `AppInstrumentation` unit. That alone removes about a third of the
   file's state and most of its untested surface.
5. **Repair the five assertions that cannot fail, then sweep the 34 epsilons**
   (§3.9). In order of how badly they mislead:
   `TunnelWalkTests.cpp:242` (unfalsifiable — 14.67 m band on a 5 m possible
   range); `TunnelWalkTests.cpp:345-348` (add `CHECK(waypoint ==
   tunnel.point_count)` — the case name promises it and the failure has already
   happened once); `TunnelWalkTests.cpp:356-386` (give
   `count_tunnelling_ticks` a Rule 30a arm proving it can return non-zero, and
   assert the capsule moved); `RenderSystemTests.cpp:246` (band 0.5065 m guards
   a 0.35 m displacement and admits the exact bug the comment names);
   `ProcFloraTests.cpp:299-305` (an `X >= X` identity — derive `frac` from the
   built mesh). Then add sample-count guards at `WorldgenV2Tests.cpp:524` and
   `ForestStandTests.cpp:1129`, as `LodSeamTests.cpp:206` already does. The
   epsilon sweep has a worked pattern in this repo already at
   `ClipTests.cpp:173-179`.
6. **Reconcile `docs/ARCHITECTURE.md` with the code** (§3.12). Six statements in
   the hard contract are now false. A contract with false clauses trains readers
   to skim it, which is Rule 38's failure mode applied to documentation.
