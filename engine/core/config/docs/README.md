<!--
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
-->
<!--
UPD:
- 09:08:2026 - 00:16:55: Stage 1 — public contract documented (headers only; generator arrives in stage 2).
-->

# engine/core/config

## Responsibility

The consuming side of the generated constants (Q54, Rule 14): one include point
through which all code reaches the values generated from `docs/NUMBERS.md`.

## Key types

- `sources/Constants.h` — re-exports `dfn_generated/Constants.h` (emitted by the
  lead-owned `tools/gen_constants` into the build tree). Generation contract:
  namespace `dfn::config`, `inline constexpr`, names exactly as the NUMBERS.md
  tables (`SIM_TICK_RATE` uint32, `SIM_DT` double, `WALK_SPEED`/`RUN_SPEED`
  float, `CHUNK_SIZE` float, `HEIGHTMAP_RESOLUTION` uint32, `HEIGHTMAP_STEP`
  float, ...).

## Usage example

```cpp
#include "engine/core/config/sources/Constants.h"
dfn::time::FixedTimestep ts(dfn::config::SIM_DT, dfn::config::SIM_MAX_CATCHUP_STEPS);
```

## Dependencies

Uses the generated header only. Used by every module needing a numeric constant.
Rule 14: a hardcoded simulation/gameplay constant in C++ is a violation — extend
NUMBERS.md instead.
