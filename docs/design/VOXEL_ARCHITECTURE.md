<!--
Created: 09:08:2026 - 15:41:46
Last updated: 09:08:2026 - 15:41:46
-->
<!--
UPD:
- 09:08:2026 - 15:41:46: Spike report — voxel terrain architecture evaluation with measured numbers from a throwaway prototype; recommendation, migration order and cost. Authored by core at the lead's direction (docs/design is design's zone; this file is a core deliverable for lead review).
-->

# VOXEL_ARCHITECTURE.md — spike report and recommendation

**Status:** research + prototype. **No shipping code was touched.** The
prototype lives in a scratch directory and is throwaway.

Decision driving this: the user asked for true 3D terrain — «воксели,
переписываем, нам нужно 3д» — caves you walk into with a ceiling overhead,
overhangs, real vertical structure. A heightfield cannot express any of it:
one height per column is the whole limitation.

---

## 0. Verdict on the proposed architecture

**The lead's "macro layer as INPUT, voxels as representation" proposal is
correct. Adopt it.** Two corrections, one of which is not optional:

1. **MANDATORY — store a signed distance field, not binary fill.** "Fill solid
   below the surface" as boolean occupancy stair-steps at any voxel size we
   can afford: 1 m cubes would replace our rolling hills with Minecraft
   terraces, which is a different game. Storing *quantized signed distance to
   the analytic surface* makes 1 m voxels reproduce the current smooth terrain
   (measured below), because the isosurface is reconstructed sub-voxel from
   the distance values. Carving stays trivial — a CSG subtraction on the
   distance field (`d = max(d, -cave_sdf)`), which is how the prototype cut
   its tunnel.
2. **Do not re-sample the macro layer per voxel.** Measured: at 1 m voxels the
   macro sampling costs 166 ms per chunk while the entire voxel build +
   extraction costs 11 ms. Sampling dominates by 15x. Sample the analytic
   field once at the existing 2 m grid (129²) and interpolate into the volume.

Why the proposal is right, stated plainly: every composition rule we have —
C1 landmark visibility, C3 chaining, corridors, fords, pads, the castle's R1-R4,
the scatter exclusion rules — is expressed against the macro field, and all of
it is tested. Native 3D noise would discard that machinery and design's entire
rule set with it, and we would be re-deriving "is the crag visible from the
meadow" from scratch in a representation that makes the question harder. The
carve model keeps all of it and buys real 3D at the cost of one new pass.

The honest caveat is in §5: the derived heightfield stops being a truthful
"ground under the player" answer the moment overhangs exist, and the player
controller is the consumer that cares.

---

## 1. Storage — measured

One 256 m chunk, quantized int8 SDF + int8 material (2 bytes/voxel), terrain
sampled from the real seed-1 world at chunk (2,1) (crag foot, genuine relief).

| Voxel | Grid (x,z) | Slab (y) | Dense, full 64 m | Dense, slab only | Narrow band | Per-column RLE |
|---|---|---|---|---|---|---|
| 1.00 m | 257² | 19 | 8.2 MB | 2.4 MB | 0.5 MB | **0.50 MB** |
| 0.50 m | 513² | 34 | 64.8 MB | 17.1 MB | 4.0 MB | **2.01 MB** |
| 0.25 m | 1025² | 64 | 515.0 MB | 128.2 MB | 31.9 MB | **8.02 MB** |

"Slab" = the volume actually needs only the vertical range the surface passes
through, not the full 64 m world height. That alone is a 3.4x saving.

**Streaming budget, 5×5 resident ring (CHUNK_LOAD_RADIUS 2 = 25 chunks):**

| Voxel | Slab-dense | Narrow band | RLE | Extracted mesh | Triangles |
|---|---|---|---|---|---|
| 1.00 m | 60 MB | 12.5 MB | **12.5 MB** | 98 MB | 3.7 M |
| 0.50 m | 427 MB | 100 MB | **50 MB** | 396 MB | 14.8 M |
| 0.25 m | 3.2 GB | 798 MB | **200 MB** | 1.6 GB | 59.3 M |

Reference: today's heightfield mesh is 33k tris/chunk, **0.83 M tris** for the
ring.

**Reading:** voxel *storage* is a non-issue at 1 m (12.5 MB of RLE for the
whole ring — less than one chunk's mesh). The real cost is the **extracted
mesh**: 98 MB and 3.7 M triangles, 4.4x today's geometry. 0.25 m is off the
table for streamed terrain by an order of magnitude on both counts. 0.5 m is
affordable in storage but not in geometry without aggressive LOD.

---

## 2. Surface extraction — measured

Prototype implements **surface nets** (one vertex per sign-changing cell,
quads across shared edges). Times are single-threaded on this machine, one
chunk, cold.

| Voxel | Macro sampling | Volume build | Extraction | Vertices | Triangles |
|---|---|---|---|---|---|
| 1.00 m | 166.1 ms | 1.3 ms | **9.7 ms** | 73.9 k | 146.7 k |
| 0.50 m | 661.8 ms | 9.2 ms | **62.4 ms** | 296.8 k | 591.4 k |
| 0.25 m | 2633.2 ms | 63.3 ms | **457.0 ms** | 1.19 M | 2.37 M |

**Per-chunk generation number the lead asked for.** Today: ~30 ms. Naively
voxelized at 1 m: **177 ms** — a 6x regression, and 94 % of it is macro
sampling at 257² instead of 129², not voxel work. With the fix in §0.2
(sample once at 129², interpolate): **~30 ms macro + 11 ms voxel ≈ 41 ms**.
That is the number to plan against, and it is acceptable.

**Algorithm choice — recommend surface nets now, dual contouring later:**

- **Marching cubes:** produces sliver triangles and cannot represent sharp
  edges. Slivers are actively bad for physics (thin triangles make degenerate
  contact manifolds). Reject.
- **Surface nets (recommended):** one vertex per cell gives uniform, well-
  shaped quads and clean walkable floors — exactly what a character controller
  wants. Simple, fast (9.7 ms), easy to keep deterministic. Rounds sharp
  edges, which is *correct* for terrain and *wrong* for the castle terrace,
  quarry cuts and cave mouths.
- **Dual contouring:** surface nets plus QEF vertex placement using hermite
  normals — keeps sharp creases. This is the upgrade path when the terrace and
  quarry edges look too soft; it is a change to vertex placement only, so the
  rest of the pipeline is unaffected. Costs more time and needs care against
  non-manifold output. Defer.

For our low-resolution pixel look, surface nets at 1 m is visually sufficient:
terrain detail lands between today's 2 m heightfield and finer.

---

## 3. Determinism — measured, no hazards found

Prototype builds the same chunk twice and compares:

- **Volume bytes identical: YES** (byte-for-byte over 1.25 M voxels).
- **Extracted mesh hash: identical** (`0a04f10912b2e7bf` both runs, FNV-1a
  over all vertex bits and indices).

Why it holds, and what to protect:

- Quantizing the SDF to int8 at generation makes the stored volume an **exact
  integer state**. Extraction is then a pure function of integers plus a fixed
  arithmetic sequence — the same determinism story as today's uint16 heights,
  not a weaker one.
- **Hazards to forbid in the implementation:** (a) multithreaded extraction
  that appends vertices in completion order — vertex indices must come from a
  fixed traversal, with threads writing to pre-assigned ranges; (b) any
  `unordered_map` iteration feeding output order; (c) fast-math/FMA
  differences if we ever add QEF — same rule as today, no fast-math;
  (d) parallel reduction of normals (float addition is not associative).
- Rule 13.1 stays testable exactly as now: hash the volume and the mesh.

---

## 4. Physics — proposal (sim owns the backend)

Today: heightfield → mesh → `IPhysics::create_terrain`, one static body per
chunk. Jolt's `HeightFieldShape` is fundamentally incapable of overhangs, so
it must go.

Proposal, for sim to accept or amend:

- **One static body per chunk, unchanged** — the streaming lifetime,
  create/destroy on ChunkLoaded/ChunkUnloaded, stays exactly as it is.
- **`MeshShape` built from the extracted triangles.** Jolt's `MeshShape` is
  the standard static-geometry shape and handles arbitrary topology including
  ceilings and overhangs.
- **Collision mesh should probably be its own coarser extraction.** 147 k
  tris/chunk is a lot of `MeshShape` build time and memory for collision that
  the player experiences at ~0.35 m step resolution. Extracting collision at
  2 m voxels (≈ 37 k tris) is likely indistinguishable in play and 4x cheaper.
  Needs measurement with the real backend — sim's call.
- **Interface impact:** `create_terrain(HeightFieldView)` is replaced or
  joined by a mesh-based entry point. That is a frozen-interface change and
  needs a group sync (Rule 26).

---

## 5. Contract fallout — precise

**The frozen `HeightFieldView` SURVIVES, as a derived view.** This is the
key structural result and it is what makes the migration affordable.

| Consumer | Needs | Verdict |
|---|---|---|
| Scatter placement (P5) | "ground height at (x,z)" | **Unchanged** — derived heightfield |
| Site pads (P4), corridors, fords | ground height / slope at (x,z) | **Unchanged** — derived heightfield |
| C1/R2/R4 validation raycasts | terrain + canopy + castle occlusion | **Unchanged** initially; becomes 3D-aware only when caves must occlude |
| Tour camera ground placement | ground height at (x,z) | **Unchanged** |
| render `TerrainMesher` | heightfield → triangles | **BREAKS** — replaced by voxel extraction output |
| physics `create_terrain` | heightfield → collision | **BREAKS** — see §4 |
| Player controller ground query | "what is under me" | **BREAKS under overhangs** — see below |

- **Additive, no break:** a new `VoxelChunkView` / extracted-mesh handoff,
  delivered on the same ChunkLoaded/ChunkUnloaded events with the same
  lifetime rules. Same pattern as the `SurfaceFieldView` handoff that already
  worked cleanly with render.
- **The honest limitation:** a heightfield answers "the topmost surface at
  (x,z)". Once a cave exists, the truthful answer is "the surface under the
  player's current Y", which is a 3D query. So: the derived heightfield stays
  valid for everything that works on open ground (scatter, pads, validation,
  tour) and **must not be used for controller ground-finding once caves ship**.
  The controller needs a voxel raycast/sphere query. This is unavoidable in
  any 3D representation and should be planned, not discovered.
- Worldgen's own passes (P1-P5) keep operating on the 2D field and stay as
  they are. Only the new carve pass and the queries above are 3D-aware.

---

## 6. Migration order — main builds and runs at every commit

**Stage 1 — representation swap, zero visible change.** Volume + extraction in
`engine/world`, built from the existing heightfield with **no carving**.
Render consumes extracted meshes instead of `TerrainMesher` output; physics
moves to `MeshShape`. The world looks and plays as it does today; the tour is
the proof. All existing suites keep passing because the derived heightfield
still exists. *This is the risky stage and it ships without new content.*

**Stage 2 — the carve pass (P7), after P4/P5.** Caves, the tunnel-and-
switchback route up the crag, the barrow interior, quarry cuts. New design
rules needed from design (cave dimensions, ceiling clearance, lighting
assumptions) and new invariants from me (passages walkable end to end,
ceiling ≥ player height, no cave mouth inside a building pad, caves do not
undercut the castle terrace).

**Stage 3 — 3D-aware queries.** Controller ground query against the volume,
scatter on cave floors, validation raycasts that respect ceilings.

**Stage 4 — LOD and budget.** Coarser extraction for distant chunks (the
3.7 M triangle number demands it), collision mesh decimation, streaming
pacing. Then building interiors as carved volumes.

---

## 7. Honest cost

**Core-side estimate, in agent-sessions:**

| Stage | Core sessions | Other zones |
|---|---|---|
| 1 — representation swap | 3-4 | render 2, sim 1 |
| 2 — carve pass + invariants | 3-4 | design 1-2 |
| 3 — 3D queries | 2-3 | sim 1-2 |
| 4 — LOD, budget, interiors | 3+ | render 2+ |
| **Total** | **11-15** | **6-9** |

**If we must ship 3D in ONE stage**, cut to roughly 5-6 core sessions:

- **Keep:** 1 m voxels, SDF storage, surface nets, physics `MeshShape`, the
  derived heightfield, one carve pass with a small hand-authored set (the crag
  tunnel + the barrow interior — the two the user actually asked to walk into),
  determinism tests, a walkability invariant for the carved passages.
- **Cut:** 0.5/0.25 m resolution; dual contouring (accept soft edges);
  LOD (rely on view distance and accept the triangle bill on the near ring);
  cave scatter and cave-aware C1; building interiors; editor support;
  collision mesh decimation (use the render mesh and eat the cost).
- **Accept as debt:** the controller's ground query gets a special case for
  "inside a carved volume" rather than a general 3D query, and distant chunks
  keep the old heightfield mesh (hybrid rendering) until LOD lands.

---

## 8. What the prototype proved about carving

Driving a 3.5 m radius tunnel into the crag foot as a CSG subtraction:

- Mesh went from 73 898 to 74 824 vertices — **the tunnel costs ~1 % more
  geometry**, because only the carved shell adds surface.
- **30 columns** ended up with two or more solid spans — i.e. air below solid
  below air: a genuine ceiling over walkable floor.
- A heightfield can represent **none** of them. That is the whole argument for
  the change, and it is now measured rather than asserted.

---

## 9. Sources

Prototype: `voxel_spike.cpp` (scratch, throwaway), built against the shipping
`dfn_world` macro layer at seed 1, chunk (2,1). Techniques: surface nets
(Gibson, "Constrained Elastic Surface Nets"), dual contouring (Ju et al. 2002)
as the deferred upgrade, narrow-band SDF storage as used by OpenVDB-class
systems. Engine-internal grounding: NUMBERS.md (CHUNK_SIZE, HEIGHTMAP_*,
WORLDGEN_MAX_HEIGHT, CHUNK_LOAD_RADIUS), docs/specs/core.md (frozen
HeightFieldView contract, streaming lifetimes), docs/design/LANDSCAPE.md
(the pass pipeline this proposal preserves).
