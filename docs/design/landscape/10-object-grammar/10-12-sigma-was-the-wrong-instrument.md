
### 10.12 σ WAS THE WRONG INSTRUMENT — D2 re-derived on the gradient (stage-5)

A1 came back with **σ = 0.353 against a floor of 0.35, and F7 failed in the same
frame**: the ground still ran unbroken from the player's feet to the tree line.
The probe passed and the picture the probe exists to guarantee did not.

**The lead's diagnosis is correct, and I have verified the arithmetic rather than
accepted it.** For `h = A·sin(2πx/L)`, σ = A/√2 and RMS slope = **2πσ/L** (max
slope is √2× that). So:

| L | RMS slope at σ = 0.353 | grazing threshold |
|---|---|---|
| 20 m | **6.35°** | 4.86° at 20 m — passes |
| 25 m | 5.08° | |
| 40 m | **3.18°** | 2.43° at 40 m — passes, barely |
| **52.2 m** | **2.44°** | **the crossover** |
| 60 m | **2.12°** | 2.43° at 40 m — **fails** |

> **σ bounds AMPLITUDE. Occlusion is a property of SLOPE. They are related only
> through wavelength, and wavelength was never in the contract.** A field can
> hold σ arbitrarily above the floor and remain a shallow swell that hides
> nothing. My derivation of the 2.4° grazing angle from eye geometry is sound;
> I then demanded it of a quantity that does not constrain it.

**Correctly classified as Rule 41, not Rule 48** — σ's zero-dose control behaves
properly (0.000 on flat ground), so the criterion *can* pass and *can* fail. It
is simply aimed one quantity to the left of the target. **A criterion can be
sound, falsifiable, well-controlled, and still be pointed at the neighbour of the
thing you care about**, and that is a distinct failure from the two we already
have rules for.

#### 10.12.1 σ is RETIRED as a gate — not re-floored

**Do not move 0.35.** Raising it would fit a threshold to a proxy that is
structurally incapable of gating the property, which is Rule 45's distinction —
a floor and a separating threshold are different objects, and σ is neither for
this purpose.

- **`GROUND_RELIEF_SIGMA_20M_MIN` — RETIRED as a gate.** σ stays *reported* as a
  diagnostic, with no threshold attached.
- **`GROUND_RELIEF_SIGMA_20M_MAX` = 1.20 SURVIVES UNCHANGED**, and for a
  principled reason rather than by inertia: **the ceiling's job genuinely is
  amplitude.** It bounds churn so the answer to «flat» never becomes unwalkable,
  and a bound on amplitude is exactly what a comfort ceiling wants. The floor's
  job was slope, which is why only the floor falls.

#### 10.12.2 The replacement is not a better proxy — it is the thing itself

The lead proposes the area fraction in the 5–60 m band where local slope exceeds
2.4°. That is a strict improvement over σ and I would accept it. **But §10.11.3
already ruled the sharper answer three messages ago and I did not connect it:
«ground occludes ground» is a RAYCAST FACT, computable in the generator with no
shading at all.** There is no reason to gate on a statistical predictor of a
quantity we can compute directly.

> **`GROUND_OCCLUSION_COUNT` — the D2 gate.** From a standpoint at eye height on
> non-exempt ground, cast a ray along the view direction and count the distinct
> intervals where the heightfield occludes heightfield **within 5–60 m**.
> Evaluate over standpoints × bearings and read the distribution at
> `ACCEPTANCE_PERCENTILE` = 5. **Floor: 3**, unchanged from §10.1.3 — it is a
> picture requirement read off frame 01 and the diagnosis does not touch it.

Four properties, each earning its place:

- **It is the frame test, computed instead of photographed.** Exactly what
  §10.11.3 ruled for every count in §10, now applied to the one count that had
  been left as a proxy.
- **It needs no wavelength constant.** Amplitude and wavelength both enter
  through the geometry that actually matters; one instrument replaces two, and
  neither can be traded off against the other behind its back.
- **The distance-dependence comes free, and my 2.4° was a simplification I
  should have flagged.** The grazing threshold is 4.86° at 20 m and 1.62° at
  60 m; 2.4° is its value at 40 m, the middle of the band. A raycast uses the
  true angle at every distance. **Any area-fraction instrument has to pick one
  angle and is wrong at both ends of the band.**
- **It is read at p05, so a single lucky bearing cannot buy a pass** — and a
  single unlucky one cannot fail it.

**Scoping ruling: the raycast is TERRAIN-ONLY. Boulders and outcrops are
excluded.** Not an arbitrary boundary — §10.2 already named the failure it
prevents: «objects without octaves give a flat table with props on it — a
diorama». If B1's scatter could satisfy D2, we could ship a billiard table with
rocks on it and pass. Objects are counted separately and already are, by
`MIDGROUND_OBJECT_COUNT_MIN`.

**I am not assigning a value beyond the floor of 3, and the lead is right that
this is the point.** When core's distribution arrives, the number falls out
without negotiation: floor 3, read at p05. **The percentile is the margin** —
that is what `ACCEPTANCE_PERCENTILE` was introduced for, so no additional
1.6× is taken here.

#### 10.12.3 THE ACTIONABLE FINDING — the approved meso band straddles the failure line

This is the part core can act on today, and it is derived rather than guessed.

> **At the achieved σ = 0.353, the field clears the 40 m grazing angle only if
> its dominant wavelength is below ≈ 52 m.** `GROUND_MESO_WAVELENGTH` is
> approved at **25–60 m**. The top third of our own approved band cannot
> produce occlusion at the amplitude we are producing.

So A1's failure is probably **not** a missing octave — it is the meso octave
sitting at the wrong end of a range that was never checked against the grazing
geometry, because the grazing geometry did not exist when the range was written.
Two levers, and the arithmetic says which is cheaper:

- **Shorten L** toward 25–40 m: at L = 25 the RMS slope is 5.08°, double the
  requirement, at unchanged amplitude and unchanged walkability.
- **Raise σ** at fixed L = 60: needs σ ≥ 0.40 for RMS slope alone to reach 2.4°,
  and that only reaches the *threshold*, with no margin.

**Shortening the wavelength is strictly better** — it buys slope without buying
amplitude, so it costs nothing against `GROUND_RELIEF_SIGMA_20M_MAX`, against
corridors, or against `PLAYER_STEP_HEIGHT`. Recommended to core as the first
knob. Whether `GROUND_MESO_WAVELENGTH_MAX` should come down from 60 is a NUMBERS
question I raise but do not settle here: **the occlusion count is the gate, and
if core reaches 3 at p05 with the range as written, the range stays.** I will not
constrain the mechanism when I have just been corrected for constraining the
wrong quantity (Rule 38).

#### 10.12.4 §2.7's fifth octave — REASSIGNED, not deleted

The lead is right that a line the document holds and never applies will be
applied on the next reading — that is a Rule 39 shadow copy with a delay fuse.
§2.7's «optional fifth octave at 2–4 m / 0.1–0.2 m for surface tooth» is
**removed from §2.7 in this edit**, and it is removed as a *reassignment* rather
than as a deletion, because the work it described still has to happen:

**§10.2 ruled that band out of the heightmap's reach** (`LOD_VOXEL_SIZE_L0` = 1.0
m samples a 2–4 m period 2–4 times, which aliases) **and assigned it to objects.**
Surface tooth at 0.1–0.2 m is B1's small end, B6's tufts, and the gravel of
frame 01 — not an octave. §2.7 now says so and points at §10.2, so the next
reader finds the reassignment instead of the orphaned promise.

#### 10.12.5 LF-8 — REBUILT ON CONNECTIVITY, and the rebuild makes it truer

Core is right that a depth-threshold gully detector is Rule 47 in relief costume:
it locates its subject by the very local depth it then measures, so on bumpy
ground the flank of a brow scores like a washout. **I am not abandoning LF-8 and
I am not moving its threshold.**

> **A gully is not a deep place. It is a place that DRAINS.** What separates a
> washout from a hollow is topology: a gully's floor descends monotonically and
> **connects to the drainage network**; a hollow is closed, or drains nowhere.
>
> **RULING: LF-8 is rebuilt to locate its subject by CONNECTIVITY TO THE
> DRAINAGE, and only then to measure depth and profile.** Reuse §3.1's existing
> descent/pond-and-spill field — no new machinery, and no new constant.

Why this is a genuine improvement rather than a way to get the light green:

- **It is Rule 47-proof by construction.** The subject is located by topology,
  which is not the property under test; depth is measured afterward, on a set
  chosen without reference to it.
- **It makes the feature physically true.** Gullies are erosional and they carry
  water. A "gully" unconnected to any drainage was always a modelling error that
  the old detector could not see.
- **It agrees with the world model we already committed to.** B2 places outcrops
  where erosion *strips*; LF-8 finds where erosion *cuts*. Same process, same
  field, two features — which is the kind of consistency that makes a generated
  world read as one place.

**LF-8 stays RED until rebuilt.** A red light with a known cause and a specified
fix is worth more than a green one bought by a threshold.

#### 10.12.6 The authored clearing (в9) — EXEMPT, with a derived bound, ruled explicitly rather than by silence

The lead is right to refuse silence here. The clearing's own contract requires
3.0 m of calm, so meso relief and rock are suppressed and it cannot meet an
occlusion floor. §10.1.2's exemption list does not name it, and it should.

**It is the same category as the existing exemptions**, which are not an
administrative list but a principle I had left implicit: corridors, building
pads, the castle terrace and the shore band are all **flatness that is authored
and does work**. So is the clearing. But an exemption that is not bounded eats
the rule, so:

> **An authored flat place is exempt from `GROUND_OCCLUSION_COUNT` when all
> three hold: (a) the flatness is AUTHORED, not emergent; (b) it is BOUNDED —
> radius ≤ 50 m; (c) it is SURROUNDED by non-exempt ground.**

**(b) is derived, not chosen.** The criterion is about what is in the *frame*,
and the band that matters is 5–60 m. At radius ≤ 50 m, a standpoint anywhere in
the clearing still has non-exempt ground **inside its own 5–60 m band**, so **the
frame taken in the clearing passes on the ground beyond its edge.** The clearing
is calm underfoot and the world is still not flat — which is what an authored
rest beat is supposed to feel like, and the conflict dissolves rather than being
waived.

**And the bound has teeth, which is how I know it is a rule and not a
courtesy:** `PLAIN_EXTENT` is 400–600 m, so **the plain is NOT exempt** and
remains fully bound by §10.1. That was always the intent — §10.1.2 binds
«including inside `PLAIN_EXTENT`» — and it is now the consequence of a stated
principle instead of a special case.

*Core should check в9's actual extent against 50 m; if it is larger, the
exemption does not apply to it as authored and the clearing needs either a
smaller calm core or a bumpier rim. That is a design question I will take, not a
number to bend.*

#### 10.12.7 The mid-ground result — and the number to report is 8, not 17

**0 → 17 placed and readable, 8 unoccluded, against a floor of 5, counted in the
generator by projection with no pixel segmented.** That is §10.4.1 moving from a
claim about a frame nobody had taken to a measured pass, and it is the first
number in this document to be produced under §10.11.3's rule.

**One precision so the figure does not drift on its next retelling: the floor of
5 applies to the UNOCCLUDED count.** §10.4.1's wording is «distinct object
silhouettes … sitting between the near ground and the horizon» — a silhouette
hidden behind another object is not a distinct silhouette in the frame. **So the
result is 8 against 5, and 17 is the placement figure, not the score.** Reporting
17 would inflate the margin by 3.4× at the next reading, and this is exactly the
moment such a thing gets fixed cheaply.

##### Numbers (Rule 35, via lead)

| constant | action | note |
|---|---|---|
| `GROUND_OCCLUSION_COUNT_MIN` | **new, = 3** | at p05 over standpoints × bearings, 5–60 m band, **heightfield only** |
| `GROUND_RELIEF_SIGMA_20M_MIN` | **RETIRE as a gate** | σ still reported as a diagnostic, no threshold |
| `GROUND_RELIEF_SIGMA_20M_MAX` | **unchanged, 1.20** | the ceiling's job is amplitude, and it still does it |
| `GROUND_MESO_WAVELENGTH_MAX` | **flagged, not changed** | 60 m cannot produce occlusion at the achieved σ; the gate decides, not me |
| `AUTHORED_FLAT_RADIUS_MAX` | **new, = 50 m** | derived from the 5–60 m band, §10.12.6 |

---

