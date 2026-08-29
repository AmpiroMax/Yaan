
# Flora collision — the world you cannot walk through

Owner: agent **sim** (`engine/gameplay`, `engine/physics`).
Code: `engine/gameplay/sources/FloraCollision.{h,cpp}`, the plant path in
`PropCollision.cpp`, the brush factor in `PlayerMovement.cpp`.
Suite: `tests/sim/FloraCollisionTests.cpp` (`ctest -R sim_flora_collision`).

## The complaint

The user, verbatim:

> деревья — не объекты физики, значит их просто малюют, а не строят 3д модели
>
> кусты, поваленные деревья пропускают героя (кусты могли бы замедлять), значит
> они не физические объекты, значит не 3д модель строится, которая
> переиспользуется, а какая-то функция пишется или набор правил, но не 3д объект
> реальный для игры и мира

It is not a complaint about looks. **A world you walk through is not a world.**

## Three answers, because a plant is not one kind of thing

| Class | Answer | Why |
|---|---|---|
| Oak, pine, birch, snag, stunted pine, great oak | **Solid** — drawn triangles in the chunk body | A trunk stops you |
| Fallen log **above** `PLAYER_STEP_HEIGHT` | **Solid** | Go round it or over it |
| Fallen log **under** `PLAYER_STEP_HEIGHT` | **Nothing** | The controller already climbs it for free; triangles would buy nothing |
| Bush, big bush, deadfall | **Drag** — a disc, no body | A shrub you cannot pass is a fence; one you cannot feel is a painting |
| Boulders | untouched | They already had collision from their own drawn mesh |
| Moss, flowers, mushrooms, pebbles | **Nothing** | A mushroom that slowed you down would be a bug report |

The middle row is the user's own proposal («кусты могли бы замедлять») and it is
the right one.

## Why the trunk is MEASURED and not computed

`PropCollision.h` carried this note for four days:

> NOT COVERED YET: tree trunks. `species_trunk_radius()` reports the flared base
> radius against the species' NOMINAL height, while every instance is drawn at a
> per-variant height, so a trunk cylinder built from it would stand up to
> ~0.35 m proud of an oak's actual bark — an invisible wall, which is a worse
> bug than walking through a tree because you cannot see what stopped you.

The objection was right. The conclusion — wait for a per-instance accessor —
was not, because **a second formula is a shadow copy and drifts the day flora
re-authors the first** (Rule 39; this tree has already produced five copies of
one chain and three of one threshold).

So the collider is taken from the mesh flora actually builds for that
`(species, variant, maturity)` and clipped below the crown base. What stops you
is bark you can see, at every maturity, with no second formula to drift. It is
the same principle `PropCollision` already used for buildings and boulders.

**The run measured the difference.** A pine at (204.60, 113.48) in the testbed
world draws at maturity 0.417 — a sapling. Its measured collider is 30
triangles with a 0.327 m widest reach, and the player is stopped 0.533 m from
its axis (0.18 m bole + 0.35 m capsule). A collider built from
`species_trunk_radius()` (0.845 m, the pine's flare at its NOMINAL height)
would have stopped the player at 1.20 m — **0.66 m of invisible wall around a
stick.**

Cost is `O(species × variants × maturity buckets)`, never `O(instances)` —
flora's own cost rule. Maturity is bucketed DOWNWARD, so a collider is at worst
a couple of centimetres thinner than the tree drawn over it: you may brush the
bark, never a wall short of it.

## Why brush is not a body, and not a second slowdown

Forty shrubs a hectare would be forty bodies for the sensation of "slightly
harder to walk here". A bush is a **drag disc** in `BrushField`, built by the
same chunk reconcile as the trunk bodies, read through **the same speed
multiplier wading already used** (Rule 35: one place answers "why am I slow").

Two media meet on that multiplier and **the slowest wins — they do not
multiply**: 0.6 × 0.65 = 0.39 would make a willow thicket at the water's edge,
the most atmospheric place in the world, the one place the player cannot cross.

The density ramps linearly from the outermost leaf to the heart of the shrub, so
a walker feels themselves push IN rather than hit a speed wall at the rim.

## Budget (measure before you place — Rule 42)

25 resident chunks, 163.8 ha, 19 406 scatter instances, testbed seed 1:

| Quantity | Value | Against |
|---|---|---|
| Physics bodies added | **0** | Jolt world sized for 16 384; a capsule per trunk would be ~7 200 |
| Triangles added | 56 975 (2 279/chunk) | the chunk body already held ~20 800 → +9.9 % |
| Solid plant instances | 1 894 | an oak bole is 20 triangles; a great oak 241 |
| Drag discs | 2 784 (17.0/ha) | cost no bodies at all |
| Plant pass, cold | 19.0 ms for 25 chunks (0.76 ms/chunk) | 2.5 % of the 774.8 ms the pass already cost |
| Plant pass, warm | 1.3 ms | — |
| `update_prop_collision`, steady | 0.0008 ms/tick | SIM_DT 16.67 ms |
| `brush_density_at` | 0.0003 ms/query | once per tick |
| Collider memo | 2.21 MB / 878 entries | — |

Every figure is in the unit that limits it. The per-trunk-capsule alternative is
what makes the "zero bodies" line load-bearing: at 44 trees/ha × 25 chunks it is
~7 200 resident bodies against a 16 384 cap, with no room for logs, doors or
NPCs, and one increase of `CHUNK_LOAD_RADIUS` would blow it. The suite asserts
the body count so nobody re-adds them without seeing the price.

## Acceptance recipe (Rule 27)

**Arm 1 — the trunk (library-level walk, real world, real Jolt, real movement
code).** Spawn 6 m west of an oak, walk +X for 8 s.

- CONTROL, plant bodies NOT built: closest approach to the stem axis **0.01 m**,
  crossed the tree, 14.38 m travelled at 1.802 m/s.
- SHIPPED: stopped **1.12 m** from the axis, 4.88 m travelled, never crossed.

**Arm 2 — the brush.** The same path, the same ground, the same 8 s; the control
differs in the drag and in nothing else (an "open ground 40 m away" control was
tried first and rejected — it varies in the terrain too, and duly came out
FASTER in the bushes than on the control ground, Rule 48.3).

- inside the bush (density > 0.5): drag off **1.802 m/s**, drag on **1.325 m/s**
  = 73.5 %.
- passable: crossed the bush in both arms, 12.80 m against 13.56 m.

**Arm 3 — the shipping binary.** `dfn_app` with a route through brush:

```
DFN_NULL_RENDER=1 DFN_NULL_AUDIO=1 DFN_PLAYTEST=patrol \
DFN_PLAYTEST_SECONDS=200 DFN_PLAYTEST_SEED=7 \
DFN_PLAYTEST_ROUTE='120.84,150.30;128,128;204.60,113.48;221.46,109.07;128,128' \
DFN_PLAYTEST_DIR=<dir> DFN_SPEED_PROBE=<file>.csv ./build_sim/engine/app/dfn_app
```

12 000 ticks, 353.4 m walked. Commanded horizontal speed: **1.800 m/s** on clear
ground, **1.580** in brush, **1.350** in brush deeper than 0.5 (actual 1.345).
`DFN_SPEED_PROBE` carries `brush_density`, `medium_factor`, `pos_x`, `pos_z`
alongside the speeds, because "the bush slowed me down" is a claim about the
speed AND about what the walker was standing in, and a row with only the speed
cannot tell a bush from a hill or a released key.

## Open — not this zone's to fix

**The great oak's stair is not a stair.** Its whole bole is solid to the crown
base now (241 triangles), treads included, so the surfaces exist. The stair does
not. Measured off the drawn mesh: 28 treads, and of the 27 consecutive pairs,
**zero** can be taken by a walker —

- rise 0.42 m (`GREAT_OAK_STEP_RISE`) against `PLAYER_STEP_HEIGHT` 0.35: every
  step is above what the controller climbs for free;
- horizontal gap 2.40 m, because the treads spiral by the GOLDEN ANGLE (137.5°)
  around a ~2.2 m bole. Two treads 0.42 m apart vertically are on opposite sides
  of the trunk.

It is 28 isolated pegs on a helix, not a staircase with a tuning problem, and no
collision work on this side can change it. It needs a rise at or under
`PLAYER_STEP_HEIGHT` and an azimuth step small enough that consecutive treads
touch. `NUMBERS.md` already says the platform radius is sim's call; the rise
should be too.
