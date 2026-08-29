
## 2.10 The landform dictionary (user-ratified, в18, stage-5)

The unit of landscape authorship is a **NAMED LANDFORM: a generation recipe
(stated as REQUIREMENTS on core's generator, never as an implementation) +
its acceptance conditions (with controls, Rule 30) + the maps that use it.**
A map-stand declares its composition of landforms (§8); the big world later
blends the same entries. Vintage Story's landform system is the precedent
(research B5); ours differs in that an entry without acceptance conditions
cannot enter the dictionary at all.

**Dictionary-level rules:**

1. **A recipe is a requirement set, not a function name.** Core may build one
   entry from three passes or three entries from one field; acceptance does
   not care.
2. **Landform boundaries are domain-warped, never straight field edges**
   (research rank 2). A straight border between two landforms in a frame is
   a failed frame.
3. **Every entry names its controls.** A landform whose acceptance nothing
   can fail is a description (Rule 30).
4. **Composition is declared, not emergent:** a stand's brief lists its
   landforms; a form appearing on a map that does not declare it is a bug,
   not a bonus.
5. **New entries enter by the same door:** recipe + acceptance + a map that
   wants it. A landform likely to acquire a name gets a story consult before
   it lands (§2.9.5 precedent).

**Seed entries** (constants are proposals → NUMBERS; existing rulings are
cited, not restated):

**LF-1 — rolling plain.** *Recipe:* base field + §2.7 micro-relief (0.3–0.6 m
waves, в9), NO meso hills; one plain per map may be preserved deliberately
calm — the authored exception (в9). *Acceptance:* slope histogram inside the
§2.7 band; the eye-height horizon frame shows waves, not a billiard table.
*Control:* the pre-§2.7 flat plane — the real rejected instance («земля
плоская») fails. *Used by:* forest stand (glades), river stand (terrace
tops), big world.

**LF-2 — ridge-and-swale hills.** *Recipe:* anisotropic meso field per
§2.1's ridgelet rule — 2–5 m relief (в9), elongated, direction-coherent
grives with swales between; amplitude distribution CDF-checked (Rule 31).
*Acceptance:* BR-5 works here — from a swale floor, the neighboring swale is
hidden over ≥ half of bearings at eye height; the crest reveals it. **This
acceptance is bare-terrain and stays bare-terrain — it is LF-2's own recipe
test, for contexts where LF-2 stands without a forest floor (river-valley
shoulders, big-world hills). On the forest stand specifically, BR-5's gate
is the composed terrain+trunks+floor instrument ruled at §1.7 BR-5,
stage-5 — landform-only and forest-stand are two CONTEXTS for one rule, not
two rules.** *Control:* an isotropic bump field of the same amplitude —
round bumps are the shape the user already rejected (Запрос 1). *Used by:*
forest stand, river-valley shoulders, big world.

**LF-3 — river valley with terraces.** *Recipe:* valley cross-profile
carrying the 25–35 m navigable river (в15): 2–3 terrace steps per side
(terrace operator applied in the valley mask, research rank 5); tributary
streams 3–5 m wide for scale contrast; LF-8 erosion feeds gullies and fans
into it; the river obeys §3.1 whole (monotone + flat reaches); **current on
the surface, not waves** (в21). **On a navigable river the §3.1.6 ford rule
is SUPERSEDED by bridges** — a ford caps depth at 0.4 m and kills
navigability; the POI graph is kept whole by bridges, and fords remain the
rule for the 3–5 m tributaries. *Acceptance:* terraces read as horizontal
lines in the cross-valley frame; width within 25–35 m at every station;
navigable = continuous channel ≥ `NAVIGABLE_DRAFT` (1.2 m proposed) edge to
edge. *Control:* the current testbed river — the real rejected instance
(в6: «не как лужица что сейчас») fails the width acceptance. *Used by:*
river+castle stand; later the big world.

**LF-4 — scree apron (talus).** Already ruled in §5.12 — entered here as the
dictionary's first citizen. *Recipe:* per §5.12: a HEIGHT rule at the massif
foot, not a clearing; scree, stone, scrub, stunted pine; never spires
(§2.9.1). *Acceptance:* per §5.12 — the forest does not eat the massif base
in the valley frame. *Control:* forest drawn to the rockline — the frame
that forced §5.12. *Used by:* massif surrounds (testbed), coastal cliff
bases (sea stand).

**LF-5 — rocky crest / outcrop.** *Recipe:* ridge-noise crest lines on grive
tops where the meso field peaks (research rank 3 — new landforms only; the
built massif is not re-opened), plus outcrop clusters at convex slope breaks
(rank 7). **Outcrops are associative:** each is explained by its slope
break; a free-floating boulder on flat ground is a placement error.
*Acceptance:* a crest line reads at 640×360 from its declared distance
(Rule 33 sizes it, F2 dates it); outcrop value contrast against grass passes
§1.5. *Control:* the same outcrop set scattered uniformly on flat ground —
the "boulder sprinkle" every generator ships by default. *Used by:* forest
stand ridges, river-valley shoulders, sea-stand headlands.

**LF-6 — coastal cliffs.** *Recipe:* cliff face with the §4.1 pale stratum
visible as horizontal banding; undercuts/overhangs at the waterline — the
voxel advantage, spent sparingly (1–2 per stand, rank 8); **pale spires
sited per §2.9** — backdrop rock or canopy, never skyline: a spire group at
a cliff foot reading against the face is the canonical siting; **Gerstner
waves — 3 waves off the SHARED wind field (в21) — damped by depth toward
the shore.** *Acceptance:* strata visible in the cliff frame; spires break
no horizon in any tour frame; wave amplitude → 0 at the waterline.
*Control:* §2.9's own rejected siting (spires against sky) — a real
rejected instance. *Used by:* sea stand (third map, в22).

**LF-7 — forest floor.** *Recipe:* §5.10 finally BUILT — BigBush, both
FallenLog classes, colonnade spacing 12–18 m, ≥ 2.2 m under-canopy
clearance; understory clumped by the BR-4 field; path margins rich per
BR-3. *Acceptance:* §5.10's own rules + BR-3/BR-4 measured inside a forest
mass; the "walked wood" frame from a path. *Control:* the current floor —
trees over uniform grass, nothing else; the real rejected instance.
*Used by:* forest stand first, every forest mass after.

**LF-8 — erosion overlay (droplet).** Not a place but a PASS, and it lives
in the dictionary because maps declare it like a form (в17, ratified).
*Recipe:* seeded droplet erosion on the coarse grid BEFORE the SDF
(research B6, rank 4): gullies (промоины), hollows (лощины), outwash fans
(веера выноса); deterministic — fixed droplet count from seeded RNG; water
placements stay derived-only (§7.1a), so the trace is allowed to move.
*Acceptance:* fans appear where gullies exit onto lower ground —
associative, each fan explained by its gully. *Control:* the same map with
the pass OFF must fail the gully acceptance; if no frame can tell the
difference, the pass is decoration and does not land. *Used by:* every
stand; the big world.

