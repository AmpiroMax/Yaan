
# WEATHER.md — Weather & Atmosphere Doctrine

Owner: `design` (Rule 25). Skeleton by the lead's request; it becomes render's
and core's contract after the tech-debt wave. User rulings it carries:
**в4** — all four weather systems, **clouds first**, then fog, wind-as-state,
weather zones; **в10** — all three cloud kinds at once (layers, ground
shadows, cumulus on the horizon); **в12** — ambient sound (wind, leaves,
water, wildlife) with music later; **в21** — sea waves are Gerstner, 3 waves,
driven by the SHARED wind field, damped by depth at the shore; rivers carry
current, not waves. Research grounding: LIVING_WORLD_RESEARCH.md A3
(Tsushima: one wind field, read by everything; weather is a DIRECTION, not a
backdrop).

Everything numeric here is **(предложение — утвердить)** until it has a
NUMBERS row (Rule 35 — every value below has at least two consumers by
construction: one zone generates it, another reads it, design accepts it).

---

## W1. What a weather state IS

**A weather state is a NAMED TUPLE OF AUTHORED PARAMETERS, living in data
(Rule 5), never in code.** A state does not compute anything; it is the
target the systems blend toward. The tuple:

| Field | What it sets | Consumer |
|---|---|---|
| `cloud_layer_cover` | 0–1 coverage of the layered sheet | render (sky) |
| `cloud_cumulus` | horizon cumulus density 0–1 | render (sky) |
| `cloud_shadow_cover` | 0–1 coverage of the ground-shadow map | render (splat darkening) — **one authority with the sky layer, see W4** |
| `fog_base` | uniform fog density | render |
| `fog_pool` | terrain-pooled fog strength (W5 — fog-in-swales) | render, reads terrain |
| `wind_strength` | amplitude MULTIPLIER on the shared wind field | grass, leaves, smoke, particles, Gerstner (в21), sound |
| `wind_gustiness` | temporal variance of that multiplier | same readers |
| `sun_attenuation` | direct light scale | render (lighting) |
| `ambient_shift` | ambient colour/level shift | render (lighting) |
| `sound_mix` | named ambient mix (wind bed, leaf bed, wildlife level) | audio (в12) |

**What a state does NOT contain:** wind *direction* (that is the field's,
W3); time of day (orthogonal); precipitation, storms, seasons-interplay,
music (**FUTURE** — the tuple gains fields when they land, existing states
get explicit values for the new field, never defaults).

**The starting state set (skeleton, names are contract):** `clear`,
`scattered` (cumulus on horizon, broken shadows crossing the ground),
`overcast`, `morning-fog` (high `fog_pool`, low wind), `wind` (high
`wind_strength`, any sky). Five is enough to prove the machinery; the set
grows by the same door as landforms (§2.10 rule 5 of LANDSCAPE.md): tuple +
acceptance + a stand that wants it.

## W2. How states TRANSITION

1. **Only along a declared adjacency graph.** `clear ↔ scattered ↔ overcast`,
   `clear/scattered ↔ morning-fog`, `any ↔ wind`. A world that cuts from
   clear to overcast in one step reads as a lighting bug, not as weather.
2. **A transition is a timed blend of the tuple** over
   `WEATHER_TRANSITION_S` (300–900 s proposed — long enough that the player
   catches weather *changing*, which is the living-world payload, not just
   weather *being different* after a load).
3. **Weather ARRIVES FROM UPWIND.** Cloud cover does not fade in place — the
   coverage field advances across the sky along the wind field's prevailing
   direction, so the horizon cumulus (в10) is the *announcement* of the next
   state: the player can see weather coming the way BR-1 lets them see a
   reveal held back. This is the doctrine's one non-negotiable: **weather has
   an approach.**
4. **The wind FIELD persists across all transitions** (W3) — states modulate
   its amplitude, never replace it. Nothing pops direction.
5. **The schedule is a PURE FUNCTION OF THE DATE, like the moon (lead
   ruling, stage-5 — it dissolved the W8 state-machine question).**
   state-at-time = pure seeded function of (world seed, game time, zone),
   transitions read from the adjacency graph deterministically — the same
   construction as the lunar phase, which is computed from game time with no
   accumulated state so werewolves and lunar magic can know it for any past
   or future day. Consequences: **no state machine runs anywhere and nothing
   serializes** — saves carry game seconds and the weather follows for free;
   any reported frame reproduces from its timestamp alone (this section's
   determinism requirement made structural). Dwell band `WEATHER_DWELL_S`
   (600–1800 s proposed). **The cost, recorded so nobody rediscovers it as a
   bug: weather cannot REACT to events** — a quest cannot summon a storm —
   **without an authored override layer on top.** That layer is a FUTURE
   decision and composes cleanly: an override is a named interval in the
   schedule, still data, still serializable.

## W3. The shared wind field (the spine of the whole system)

**One 2D field for the entire map** (research A3): world-space, slowly
drifting; every reader samples the SAME field — grass, tree leaves, smoke,
particles, cumulus drift, the sea's three Gerstner waves (в21), and the
ambient wind bed's intensity (в12). Parameters `WIND_FIELD_WAVELENGTH`,
`WIND_FIELD_DRIFT_SPEED`, prevailing direction per map — NUMBERS rows,
requested when render starts (values are look-dev, the *sharing* is
doctrine).

**The invariant, and its control (Rule 30):** at any instant, all readers
within a local patch agree on direction and phase to within their stated
lag. **Must-fail control: the current build** — grass sways to one
hard-coded oscillator and nothing else moves at all; two readers on
different oscillators (grass bending east while smoke leans west) is the
rejected case the invariant exists to kill. Can-pass: any two readers
sampling the one field.

The forest stand proves the field (LANDSCAPE.md §8.1.6); the sea stand
proves the waves off it. The field exists BEFORE any weather state does —
`wind_strength` in W1 is a multiplier on something already alive.

## W4. Clouds first (user order, в4/в10) — all three kinds, one authority

- **Layered sheet** — the sky's cloud cover; drifts along the wind field.
- **Ground shadows** — a coverage map darkening the splat. **The sheet and
  the shadow map are ONE field sampled twice** (sky draw and ground
  darkening), never two look-alike fields — Rule 35's state form: two
  drifting copies WILL disagree, and a shadow crossing land while its cloud
  stands still is exactly the defect.
- **Cumulus on the horizon** — the weather announcement (W2.3) and the sky's
  landmark tier; obeys §1.5 value rules against the sky ramp.

Acceptance for the cloud pass (frame conditions, F-doctrine): a shadow
frame where a cloud shadow VISIBLY crosses a known ridge while the sheet
moves the same direction; a horizon frame where cumulus sits ON the horizon
(not pasted mid-sky); a dusk frame proving the sheet takes lighting.
Control: shadows animated with the sheet's motion zeroed — must fail the
crossing frame.

## W5. Fog is terrain-aware — fog pools in swales

`fog_base` is uniform; **`fog_pool` fills from the bottom of local relief**:
density keyed to height above the local floor (valley floor, swale bottom,
the river's reach surface), not above sea level. Morning fog lies in the
LF-2 swales and along the LF-3 river reach and BREAKS on the grives — which
makes the meso tier (BR-5) legible at dawn instead of erased. Proposed rows:
`FOG_POOL_DEPTH` (relief-relative fill height, 2–6 m), `FOG_POOL_FALLOFF`.
**Control: uniform-by-altitude fog at the same density** — it whites out
crests and floors alike and must fail the dawn frame that shows grive tops
standing clear of a filled swale. (Implementation note for core/render: the
"local floor" field is a coarse P3-style derivative of the heightfield —
requirement, not implementation.)

## W6. Zone profiles — what a stand (or region) declares

A **weather profile** accompanies a map's landform composition (LANDSCAPE.md
§2.10 rule 4): (a) the allowed state subset, (b) a weight per state
(CDF-honest, Rule 31 — assert the realized distribution over seeded
schedules, not the weights), (c) prevailing wind direction ± spread,
(d) modifier hooks (fog-pool strength, crest wind bonus, coastal gust
factor). Per-stand skeletons:

- **Forest stand (§8.1):** `clear / scattered / morning-fog / wind`;
  prevailing wind across the grive axis (so crests read windy and swales
  calm — the same relief that hides finds shapes the wind); fog-pool strong
  at dawn. This stand accepts: the wind-field invariant (W3), fog-in-swales
  (W5), cloud shadows over the canopy.
- **River+castle stand (§8.2):** wind CHANNELED along the valley axis
  (modifier, not a second field); dawn fog over the river reach — a flat
  reach under fog is the stand's signature frame; cloud shadows crossing
  the terrace lines. Accepts: fog-over-water, channeled wind.
- **Sea stand (в22):** `wind` weighted high; the three Gerstner waves read
  the field live — a state transition VISIBLY changes the sea (в21), waves
  damp by depth at the shore, no foam yet (в13); cumulus lives on the sea
  horizon; salt haze as `fog_base` floor. Accepts: waves-off-the-field,
  horizon cumulus.

## W7. Sound (в12) — the tuple's audio half

Each state names a `sound_mix`: wind bed (gain follows `wind_strength`
through the SAME field samples — audible gusts match visible ones; the
desync is the rejected case, per research D1's sync finding), leaf bed
(forest masses only), water bed (near `dist_to_water`), wildlife level
(drops in `wind`/`overcast`, dies in fog — silence is also authored).
Music: later, per user. Footsteps-by-material are movement's, not weather's.

## W8. Ownership and open flags (for the lead)

- **design** — this doctrine, state tuples' values, zone profiles,
  acceptance frames.
- **core** — the wind field and cloud-coverage field as deterministic
  world-state (two+ consumers each ⇒ they are world data, not render
  effects), the local-floor derivative for W5, and the schedule function —
  a sibling of those fields, per the W2.5 ruling.
- **render** — sky, shadow sampling, fog draw; **audio** — W7 mixes; the
  app feeds game time exactly as it does for the sky.
- ~~OPEN: where the state machine runs~~ **RULED (lead, stage-5): there is
  no state machine — the schedule is a pure function of the date (W2.5).**
  Nothing to place in sim or app, nothing to serialize.
- **OPEN, parked with its trigger:** whether zones within one big-world map
  need boundary blending of profiles (stands don't). Wakes when two stands
  merge into one world — same door as §2.10 blending, likely the same warp
  answer.

---

# W9. Solar and lunar motion

User ruling this section serves, in his words: the sun is **not visible during
the day at all** (bright yes, no disc); the sun should be **somewhat bigger**;
the moon is **far too small**; **the moon should be bigger than the sun**; the
moon should be visible **day AND night**, dim by day; there should be **TWO
moons, both orbiting the planet**; and the moon should follow a **real
trajectory, not a plain circle** — with the options researched rather than
guessed.

## W9.0 The measuring stick — everything below is derived from this

`CAMERA_FOV_Y` 1.309 rad over `INTERNAL_RES` 640×360:

> angular size of one pixel at screen centre
> = atan(tan(1.309/2) / 180) = atan(0.767327 / 180) = **0.0042629 rad = 0.24424°/px**

**Not** the naive 75°/360 = 0.2083°/px. A perspective projection is linear in
*tangent*, not in angle, so the centre pixel subtends MORE angle than an edge
pixel; the naive division underestimates by 17 %. Every pixel figure in this
section is at screen centre, 640×360.

The consequence that governs the whole section:

| Body | true angular diameter | at 640×360 |
|---|---|---|
| Sun (mean, 1 AU) | 0.5331° (NASA Sun Fact Sheet, 1919″) | **2.18 px** |
| Moon (geocentric mean) | 0.5181° (1865.2″) | **2.12 px** |

**Two pixels.** At true scale neither body can carry an edge, a phase, a
colour, or a size change. Nothing in this section is a stylisation of a
working system; the true-scale sky does not have a working system to stylise.

## W9.1 The eclipse coincidence, and the deliberate departure from it

The Sun's diameter is ~400× the Moon's and it is ~400× farther away, so the two
subtend nearly the same angle: mean ratio **1919″ / 1896″ = 1.012**, the Moon
1.2 % smaller. Their ranges overlap (Moon 1763–2013″ against Sun 1887–1952″),
which is why both total and annular eclipses exist (NASA, *Eclipse geometry*).
This near-equality is one of the more remarked-upon coincidences in the sky.

**We are overriding it, deliberately and by a factor of 2.5.** The user has
ruled that the moon is bigger than the sun. This section states that as a
design decision rather than smuggling it in under a derivation: our sky is
**not** an eclipse-coincidence sky, our moons are enormous, and the resulting
frequent deep total eclipses (W9.7) are a consequence we accept rather than a
side effect we failed to notice. The lore anchor agrees — Nirn has two moons
and canonically gets several solar eclipses a year, known as *Vampire Days*
(UESP, *Lore:Moons*).

## W9.2 What the build actually does — three defects, audited before any number was proposed

Rule 34. Every claim below was read in the source, not inferred from the
complaint.

**D1 — the sun has no disc term, and no size constant exists.**
`fs_sky.sc:234-242` is two glow lobes, `pow(sun_dot,900)*0.85 +
pow(sun_dot,24)*0.10`, added to the sky and written raw (`fs_sky.sc:244`); no
tonemap exists anywhere in the engine. What reads on screen as a disc is the
**RGBA8 clamp**. Solving the saturation angle per channel at noon (zenith
{0.25,0.42,0.66} + `u_sunColor` {1.00,0.96,0.88}) gives red **1.40°**, green
**1.95°**, blue **2.82°** — a ~5.6°-wide white core with coloured fringes, no
edge, and **a size that drifts through the day** as `u_sunColor` and the sky
ramp move. Nothing in the code names it, so there is nothing to tune. The
user's "bright yes, no disc" is exactly right and understated.

**D2 — the game has never once started with a visible moon, and the moon's
size was never the defect.**
Measured, not assumed: `radius = sqrt(1 − MOON_COS_INNER²)` = 0.034923 rad and
`disc = smoothstep(1.06, 0.94, r)` (`fs_sky.sc:99-102`) give a solid disc of
**3.76° = 15.4 px**, soft limb to 4.24° = 17.4 px. That is not a small moon.
But `App.cpp:1329` sets `lunar_phase = frac(days / 28)` and `SkyModel.cpp:111`
sets `angle_moon = angle_sun + phase·TAU`, so **at days = 0 the moon sits at
elongation 0 — dead centre inside the sun's blown-out blob, at new phase,
lit = 0** (light = (0,0,−1) ⇒ dot(n,light) ≤ 0 everywhere but a 0.375 rim).
Every fresh launch, and every screenshot tour, which freezes that clock.
"Far too small / not visible by day" is what a player says when he has never
seen it.
**Neither zone had a bug on its own, and that is the whole lesson.**
`START_TIME_OF_DAY` 0.30 is the lead's, and it was itself a fix — the game used
to open at midnight, in darkness, with no frame hinting that the hour was the
reason. That fix met a lunar phase that starts at zero, and the collision
belonged to nobody: the sky model has no opinion about when a game starts, the
app has no opinion about where the moon is. Same shape as `PLAYER_EYE_FORWARD`,
where the rig had no eye and the camera had no body and the offset between them
was unowned. **This is also how close the project came to tuning a size
constant to repair a placement bug** — which would have left every moon in the
game's future wrong, permanently, for a reason no one could later reconstruct.
Corollary that must not be missed: **there is no day/night gate on the moon.**
`dfn_moon` is called unconditionally at `fs_sky.sc:139`. "Make the moon
visible by day" is not a feature to add; do not add a gate.

**D3 — the daily lag runs backwards.**
`angle_moon = angle_sun + phase·TAU`, and `angle` advances east→up→west, so a
growing phase puts the moon FURTHER ALONG its arc: it rises ~51 min **earlier**
each day. Real moons rise later. Phase and position are coupled — with
opposite signs — so first quarter currently rises six hours before the sun
while being lit like a first quarter, and every phase except new and full is
mirrored.

## W9.3 The structural rule — one angle drives everything

This is the most important requirement in W9, and it is what makes D3
unrepeatable rather than merely repaired.

Per moon there is exactly one state variable: its **elongation east of the
sun**,

> E(t) = E_epoch + TAU · t_days / P_synodic  [+ the eccentricity term, W9.6]

and everything else is derived from it, never stored:

| quantity | derivation |
|---|---|
| hour angle | `HA_moon = HA_sun − E` — the minus is the D3 fix |
| lit fraction | `f = (1 − cos E)/2` |
| lit **direction** | project the actual `u_sunDir` into the moon's (right, up, moonDir) basis |
| rise lag | falls out; W9.5 |

**`u_moonPhase` is deleted as an independent input.** A phase scalar beside a
position angle is two copies of one fact (Rule 35's state form), and it is
precisely how D3 happened: two representations of the same elongation, with
nothing forcing them to agree, and a sign error living in the gap. With the lit
direction projected from the real sun vector there is **no sign left to get
wrong** — a build cannot show the wrong phase for its position, because it has
no way to express one.

The USNO statement this reproduces: new/first quarter/full/last quarter occur
at elongations exactly 0°/90°/180°/270°, and the moon lags the sun by E/15
hours. Our construction gets that identity for free instead of asserting it.

## W9.4 Angular sizes

| Row | rad | deg | px @640×360 | × real |
|---|---|---|---|---|
| `SUN_ANGULAR_DIAMETER` | 0.0384 | 2.20° | 9.0 | 4.13× |
| `SUN_GLARE_ANGULAR_DIAMETER` | 0.1600 | 9.17° | 37.5 | — |
| `MASSER_ANGULAR_DIAMETER` | 0.0977 | 5.60° | 22.9 | 10.8× |
| `SECUNDA_ANGULAR_DIAMETER` | 0.0454 | 2.60° | 10.6 | 5.02× |

Derivations, each with its stated failure point:

- **Masser 22.9 px comes from thin-crescent legibility, not from taste.** The
  lit sliver of a crescent has width ≈ f · D pixels. Requiring a proper
  crescent to be at least 2 px wide sets the smallest legible phase: Masser
  fails below **f = 0.087**, Secunda below **f = 0.188**. Those are stated
  failure points, not bugs — a 5 % crescent *should* be near-invisible, and it
  is in the real sky too.
- **Masser / Secunda = 2.15**, honouring UESP *Lore:Moons*: Masser is "well
  over twice Secunda's size."
- **Masser / Sun = 2.55 and Secunda / Sun = 1.18** — the user's ruling holds
  for the pair, not only for the big one.
- **The glare halo is sized so nothing in the frame gets dimmer.** Today's
  edgeless white smear is ~5.6° across; a 2.20° disc alone would read as *less*
  sun than the user has now, which is the opposite of what he asked for. The
  9.17° falloff covers the old footprint and more, so the change reads as "a
  body appeared inside the brightness", not "the sun shrank".
- **Sun / Secunda confusion is killed three times over**, which is why 9.0 px
  against 10.6 px is acceptable: the sun has a halo and the moons have none;
  the sun sits at the top palette entry; the sun never shows a terminator.

**COST, stated as a second assertion rather than left implicit (Rule 30).**
`INTERNAL_RES` has two presets. At 320×180 every figure above halves: Masser
11.4 px still reads, Secunda 5.3 px loses its phases entirely, the sun 4.5 px
rasterises as a diamond. These sizes are derived against **640×360 because
Rule 27 fixes 640×360 as the acceptance resolution**. The 320×180 preset is a
performance escape hatch on which lunar phase degrades to lit/unlit. That is
accepted, not overlooked.

## W9.5 Orbital periods — a derived non-resonance, and a shipped counterexample

| Row | Value | Daily rise lag |
|---|---|---|
| `MASSER_SYNODIC_DAYS` | 28.0 | 24 h / 27 = **53.33 in-world min/day** |
| `SECUNDA_SYNODIC_DAYS` | 17.305 | 24 h / 16.305 = **88.32 in-world min/day** |

**The lag formula is `DAY_LENGTH / (P − 1)`, not `DAY_LENGTH / P`.** The hour
angle closes at 360(P−1)/P degrees per day, so moonrise-to-moonrise is
P/(P−1) days. Checked against the real sky in the star frame: a 27.396
sidereal-day month gives an interval of 1.03788 sidereal days = 89 428 s =
24 h 50 m 28 s, the measured lunar day, hence the textbook **50.47 min**.
Recorded because the wrong route reaches a right-looking number: 1440/29.53 =
48.8 min is close enough to "about fifty minutes" to survive review, and it is
not the same quantity. Masser's 53.33 is 5.7 % off the real figure — below
anything perceivable, and 28 is kept because it preserves every existing
`LUNAR_MONTH_DAYS` consumer and gives a clean four-week calendar. (P = 29.53
would hit 50.47 exactly, if anyone ever wants it.)

**Why 17.305, and why this is a derivation rather than a preference.**
17.305 = 28 / φ, φ = 1.6180339887. φ is the number *worst* approximated by
rationals — its continued fraction is [1;1,1,1,…], all ones, and Hurwitz's
theorem attains its bound exactly on φ-equivalent numbers. So the pair sits as
far from *every* small-integer resonance as any pair of numbers can. The user's
requirement — "not simple multiples, so they do not visibly resynchronise" —
has a provable answer, and this is it.

- Beat period 1/(1/17.305 − 1/28) = **45.30 game days**.
- Sky separation grows at **7.945°/day = 32.5 px/day** — the two are visibly on
  their own schedules within a single night's observation.

**The control is a shipped AAA build, not a synthetic case (Rule 30).**
Skyrim's `fMasserSpeed` = 0.25 over 7 200 units and `fSecundaSpeed` = 0.30 over
8 640 units: both traverse in 28 800 s, Secunda covering 1.2× the arc, i.e.
24 h against 20 h. **A dead-simple 6:5 ratio that conjoins every five days.**
That is the exact failure this row exists to avoid, and it is in a game the
user loves. Two further must-fail cases from the same build:

- **Shared phase counter.** UESP *Lore talk:Moons*: "there appears to be no
  phase difference coded between Masser and Secunda", which "renders 75 % of
  all Khajiit furstocks impossible." A build where our two moons show the same
  phase is rejected. Ours are independent by construction — two elongations,
  no shared counter to share.
- **No daytime moons.** Skyrim's night-sky object "will begin to alpha out
  during sunrise, reaching a fully culled state after sunrise is complete";
  mods exist solely to undo it. Its moons are flat billboards with eight baked
  phase textures each, rising NE and setting SE — an arc no orbit produces.

Worth keeping from the same research: Skyrim's cycle is 8 phases × 3 days =
**24 days**, so our 28 sits right beside it; and **Daggerfall — our own look
target — used a 32-day cycle with its two moons offset by four days.**
Daggerfall got the offset right where Skyrim did not.

**Epochs.** `MASSER_ELONGATION_EPOCH` = 4.712 rad (270°),
`SECUNDA_ELONGATION_EPOCH` = 4.189 rad (240°). At `START_TIME_OF_DAY` 0.30,
HA_sun = −72°, giving Masser HA +18° at f = 0.500 and Secunda HA +48° at
f = 0.750: **both moons up, 30° apart (123 px), both with a legible
terminator, in the opening frame of a new game.** These rows are not a nicety.
They are the direct repair of D2, and that frame is this section's first
acceptance shot.

## W9.6 What makes the trajectory not a circle

The elements, each with what it buys measured **in pixels** rather than
asserted as realism:

| Row | Value | Real counterpart | Visible effect |
|---|---|---|---|
| `MASSER_INCLINATION` | 0.090 rad (5.16°) | 5.145° to the ecliptic | monthly N/S swing of the arc |
| `SECUNDA_INCLINATION` | 0.209 rad (11.98°) | — | a visibly different arc |
| `MASSER_NODE_EPOCH` | 0.0 rad | — | |
| `SECUNDA_NODE_EPOCH` | 1.571 rad (90°) | — | extremes never coincide |
| `MASSER_ECCENTRICITY` | 0.055 | 0.0549 | 2e = 6.30° = **25.8 px** of wander off a uniform circle; diameter swings 2.25 px p-p |
| `SECUNDA_ECCENTRICITY` | 0.090 | — | 2e = 10.31° = **42.2 px**; diameter swings 2.36 px p-p |
| `MASSER_NODE_PERIOD_DAYS` | 200 (retrograde) | 18.6 y | draconic month 24.56 d |
| `SECUNDA_NODE_PERIOD_DAYS` | 123.6 (= 200/φ) | — | draconic month 15.18 d |

- ecliptic latitude β(t) = i · sin(TAU·t / P_draconic + Ω), with
  P_draconic = 1/(1/P_syn + 1/P_node).
- position along the orbit = mean angle + 2e·sin(M) (the equation of the
  centre), M = TAU·t/P_syn + M₀; distance r = a(1 − e·cos M), so apparent
  diameter D = D_mean / (1 − e·cos M).

**Inclination — the two moons are never on one rail.** Maximum latitude
separation 5.16 + 11.98 = **17.14°**, against a threshold of 2 ×
`MASSER_ANGULAR_DIAMETER` = 10.02°. Margin 1.71×. *Aggregation:* maximum over
one full beat cycle (45.30 days). *Denominator:* the larger moon's angular
diameter. *Control:* equal inclinations and equal nodes → 0° separation, must
fail.

**Eccentricity — and the honest coupling nobody should skip.** At true angular
size (2.1 px) the eccentric diameter swing would be **0.23 px**: invisible,
and modelling it would be waste. It is worth computing only *because* the discs
were enlarged. The two decisions are coupled; do not adopt the enlargement and
then drop the eccentricity as "detail". The real Moon's swing is 12–14 %
(NASA's supermoon figure: 14 % bigger, 30 % brighter at perigee), which our
0.055 reproduces.

**Nodal precession at the real rate is dead, and here is the arithmetic that
killed it.** 18.6 years × 365 days × 48 real-min/day = 325 872 real minutes =
5 431 real hours = **226 real days of continuous play** for one cycle. It
cannot be observed, so it cannot be acceptance-tested, so it must not be
implemented at the real rate. The 200-day figure is a deliberate compression
(160 real hours = one cycle across a long playthrough) and is **the only value
in W9 that is a choice rather than a derivation.** It is flagged rather than
dressed up. Apsidal precession (real 8.85 y) is skipped outright: one
precession term is enough to break periodicity, and a second unobservable one
buys nothing.

**What was NOT adopted, and why.** The real daily lag ranges 25–75 min because
the ecliptic meets the horizon at different angles through the year (the
harvest-moon effect: 25–30 min at mid-northern latitudes, ~11 min at 56.7°N).
That variation is a product of axial tilt and orbital position over a **year**,
and we have no year. It is recorded here as the first thing to revisit when
seasons land.

## W9.7 Eclipses are a consequence, not a feature — and they must be handled

Nobody asked for these; they fall out of W9.4 and W9.6 and will appear whether
or not anyone budgets for them. With sun radius 1.10°, Masser 2.80°, Secunda
1.30°, and latitude arcsine-distributed over ±i:

| Event | Condition | Probability per conjunction | Interval |
|---|---|---|---|
| Masser partial | \|β\| < 3.90° | (2/π)·arcsin(3.90/5.16) = 0.546 | every **51 game days** |
| Masser total | \|β\| < 1.70° | 0.214 | every **131 game days** |
| Secunda partial | \|β\| < 2.40° | 0.128 | every **135 game days** |

Over a 200-real-hour playthrough (~250 game days): roughly five partials and
two totals — **"several times a year", which is exactly the canonical TES
frequency for Vampire Days.** The model reproduces the lore without being told
to, which is the strongest available evidence that the elements are set
sensibly.

**The engineering consequence, flagged rather than buried:** `u_sunColor` and
the directional light must respond to an occulted sun, or the first eclipse
renders as a black disc over a fully-lit landscape. Masser covers the sun
2.5× over, so totality is deep and long. If eclipses need to be rarer, the
lever is **inclination** (higher i ⇒ rarer), never the periods — the periods
are carrying the non-resonance argument and must not be retuned for a second
purpose.

### W9.7-R1 — AN OCCULTED SUN IS DARK (named requirement, with its trigger)

The paragraph above was a note, and a note is what a successor skips. **It is
promoted here to a requirement with an identifier, a trigger, a control and an
acceptance (W9.9 A8), because everything else in this block has those and this
does not become findable by being true.**

> **Trigger.** Whenever the angular separation between the sun's centre and
> either moon's centre falls below the sum of their angular radii —
> `(SUN_ANGULAR_DIAMETER + MASSER_ANGULAR_DIAMETER)/2` = 3.35°, or
> `(SUN + SECUNDA)/2` = 1.85°. This is not an event the sky code opts into: it
> is a geometric fact about positions the sky code already computes every
> frame, and it will occur whether or not anybody has budgeted for it.
>
> **Requirement.** `sun_color` and the directional light scale by
> **(1 − occulted fraction of the solar disc)**, computed as the
> circle–circle overlap area of the two discs the frame is already drawing.
> **No new authored constant is involved** — the geometry comes entirely from
> `SUN_ANGULAR_DIAMETER`, `MASSER_/SECUNDA_ANGULAR_DIAMETER` and the positions
> W9.4–W9.6 already fix. That is why this half is implementable today.
>
> **Failure it prevents, in the user's likely words:** *"the moon turned into a
> black hole and nothing else changed."* A disc that occludes the sun's
> geometry while the landscape stays at noon is not a subtle wrongness; it
> reads as a graphics bug, and a graphics bug seen once costs more trust than
> the feature was ever going to buy.

**CONTROL — today's build, checked in source rather than assumed (Rule 34).**
`SkyModel.cpp:139` reads `env.sun_color = sun_hue * clamp01(smooth01(...,
elevation))`. **The sun's colour is a function of ELEVATION ALONE**; there is no
occultation term anywhere on the path, and `Materials.h:232-233` sets the
look-dev sun from a constant. So today's build is a real shipped rejected
instance and must fail A8 — the strongest kind of control this project accepts.

**HOW SOON — because the answer decides whether this is scheduled or urgent,
and it is nearer than "every ~51 game days" makes it sound.** `MASSER_ELONGATION_EPOCH`
is 270°, so a fresh world starts three-quarters of the way through Masser's
synodic cycle: the **first conjunction is 90/360 × `MASSER_SYNODIC_DAYS`
= 7.0 game days** away, which at `DAY_LENGTH_SECONDS` 2880 s is **5.6 real
hours of continuous play.** The per-conjunction partial probability in the
table above is **0.546**. **So the very first conjunction a new player reaches
is closer to a coin flip than to a rarity, and it arrives inside a first
sitting.** Whether that particular one clears |β| < 3.90° is a two-line
evaluation from `MASSER_NODE_EPOCH`/`MASSER_NODE_PERIOD_DAYS` in the node
convention the sky code owns — **requested of render rather than computed here,
since guessing another zone's convention is exactly Rule 34.** If it lands
inside the tour's window it is a blocker; if it lands at day 180 it is
scheduled. Either way the answer is cheap and nobody has taken it.

### W9.7-R2 — WHAT THE WORLD LOOKS LIKE AT TOTALITY (open, deliberately unnumbered)

R1 fixes the black-disc-over-a-lit-landscape defect completely and needs no new
row. **What the sky and the ambient do at totality is a separate question and I
am not authoring its number today**, for the same reason §5.12's threshold is
still unplaced: nothing has measured an arm. Recorded so the gap is a decision
rather than an oversight, with both ends of the range named because a range is
two assertions:

- **The floor is not black.** A real totality is roughly civil-twilight bright
  — corona plus a lit horizon ring all the way round — and a pitch-black midday
  frame is a *different* bug wearing the fix's clothes.
- **The ceiling is not subtle.** W9.8 establishes 2 · `PALETTE_SHADE_STEP_REF`
  as the smallest change that survives the quantiser at all; totality must read
  as an event and not as a cloud crossing, so its drop is bounded **below** by
  that, not near it.
- **Do not derive the amount by physics on the display ramp.** The sky's
  0.05 → 0.62 sweep is already tone-mapped, and a real eclipse's 10⁴ luminance
  drop divided into it produces a confident, meaningless number. This is the
  Rule 36 trap one step out: the ruler decides the answer.

**⚠ A TRAP TO NAME BEFORE SOMEBODY WALKS INTO IT: `MOON_SOLAR_EXCLUSION` is a
MEASUREMENT exclusion and must never become a DRAW rule.** Checked: its only
consumers today are A4's denominator and its own registry row — there is no
code consumer, so nothing is broken. But the name invites it, and the two
readings collide exactly at the event: the exclusion is 20°, occultation
happens below 3.35°, so **a build that hid the moon within 20° of the sun would
make an eclipse literally undrawable at the moment it occurs**, and the symptom
would be "eclipses don't work" rather than "the exclusion is wrong". One name,
two rules, and the inequality between them is the whole eclipse.

## W9.8 Brightness — the actual fix for "bright yes, no disc"

Measured in the **quantiser's own luma**, weights 0.30/0.59/0.11
(`fs_upscale.sc:47`) — Rule 36's general clause: a rule about what the eye
reads and a rule about what the pipeline preserves are different rules, and a
design must pass both. Ruler: `PALETTE_SHADE_STEP_REF` = 0.0784.

| Row | Value | Derivation |
|---|---|---|
| `SUN_DISC_LUMA` | 1.00 | top of the ramp |
| `SUN_GLARE_LUMA_MAX` | 0.843 | = 1.00 − 2 · step |
| `MASSER_DISC_LUMA` | 0.40 | warm rust, RGB ≈ (0.56, 0.36, 0.30) |
| `SECUNDA_DISC_LUMA` | 0.46 | neutral, RGB ≈ (0.48, 0.46, 0.43) |
| `MOON_LIMB_OUTLINE_LUMA` | 0.18 | 1-px limb ring |
| `MOON_SKY_LUMA_SEPARATION_MIN` | 0.157 | = 2 · step, on **\|moon − sky\|** |
| `MOON_SOLAR_EXCLUSION` | 0.35 rad (20°) | the real limit for seeing the Moon near the Sun |

**Why capping the glare is the fix, and a brighter disc is not.** The sky
around the sun currently saturates the top of the range, so a disc drawn there
has nowhere above it to live — this is D1's mechanism, and it is why "make the
sun brighter" is a no-op. Holding the glare two steps below the ramp's top is
what *creates* the headroom the disc then occupies.

**Why two steps and not one.** One step *is* the quantisation cell, so a
one-step difference can round into the same palette entry depending where in
the cell each value falls. A threshold equal to the resolution of the
instrument has zero margin (corollary 30a). Two steps separate regardless of
phase within the cell. The figure holds with the palette off as well
(`settings.cfg` currently has `palette=0`, so today only the RGBA8 clamp
applies): 0.157 = 40/255 levels of plain 8-bit separation.

**The daytime moon is DARKER than the daytime sky, and the first draft of this
section had it backwards.** Full-Moon disc ≈ **+3.4 mag/arcsec²**; clear noon
zenith sky ≈ **+2.4 to +2.6 mag/arcsec²** (Meinel & Meinel, *Sunsets,
Twilights and Evening Skies*; independent photometry, N. James / BAA). One
magnitude means the daytime moon is only **0.40–0.50× the sky's surface
brightness in linear luminance** — a pale disc darker than the blue around it,
easily naked-eye because 0.4 contrast is ~30× the detection threshold. A moon
drawn brighter than the day sky is the classic "night moon pasted onto a day
sky" look. Hence the rule is an **absolute** separation, `|moon − sky| ≥ 2
steps`, not a directional one; and an earlier draft's cap on render's whole
daytime sky luma is **withdrawn** — the sky may be as bright as it likes.

**Why the limb outline is a requirement and not decoration.** With the moon's
luma fixed and the sky sweeping 0.05 → 0.62 across the day, the sky's luma
necessarily *crosses* the moon's at some hour, and at that hour the separation
rule is unsatisfiable by construction. In the real sky the moon survives that
crossing on hue and on a hard edge; our quantiser weights luma and is nearly
blind to a neutral-versus-blue difference at equal luma. A 1-px ring at 0.18
gives the disc an edge at every sky state — 5.6 steps against a bright sky —
while at night the disc itself carries it at 4.7 steps. **The outline holds
the day, the disc holds the night.**
*Second assertion, offered because it is a legitimate alternative:* put both
moons at ≥ 0.78, above the brightest sky, and no ring is needed. Cost: a night
moon that glares and a daytime moon that reads pasted-on. Recommended against,
but the user should get to see both.

**Two implementation requirements that make the rules enforceable at all:**

- **Composite, do not add.** `sky += dfn_moon(dir)` (`fs_sky.sc:139`) is
  additive, so a daytime moon's luma is sky+moon and clamps to white,
  destroying the terminator — and it makes every threshold above unenforceable
  by construction. Lerp toward the disc colour by coverage. Same for the sun.
- **The unlit limb is transparent**, compositing the sky behind it. By day it
  vanishes (correct — that is why the dark limb of a daytime moon is
  invisible); by night it is black sky (correct). The present `+ 0.05`
  earthshine at `fs_sky.sc:112` is what turns a new moon into a faint grey
  coin instead of nothing.
- **Masser's colour carries a control, and it is the obvious "improvement".**
  A saturated red (1.00, 0.30, 0.20) measures luma **0.499**, because the
  quantiser weights red at 0.30 and green at 0.59 — a saturated red disc
  simply cannot be bright to this pipeline. Masser must therefore be a warm
  rust, not a red. If someone later "makes Masser more red", the daytime
  separation rule must go red. That is the control for the colour row.
- The exclusion in `MOON_SOLAR_EXCLUSION` is chosen **by cause** (proximity to
  the sun makes the moon unobservable in the real sky too), never by magnitude
  (Rule 36).

## W9.9 Acceptance

Every rule names its aggregation and its denominator.

**A1 — the sun has a disc.** In a noon frame, the set of pixels at
`SUN_DISC_LUMA` forms a single connected region whose diameter is
`SUN_ANGULAR_DIAMETER` ± 1 px, and whose luma exceeds every pixel in a 1–3 px
annulus outside it by ≥ 2 · `PALETTE_SHADE_STEP_REF`. *Aggregation:* maximum
over the annulus. *Denominator:* annulus pixels. **Control: today's build** —
its bright region has no diameter independent of `u_sunColor`, and its
per-channel edges sit at 1.40°/1.95°/2.82°, so it cannot produce a single
region of stable diameter. Must fail.

**A2 — the disc's size does not depend on the hour.** The measured disc
diameter at 06:00, 09:00, 12:00, 15:00 and 18:00 in-world varies by ≤ 1 px.
*Aggregation:* max − min across the five frames. *Denominator:* the five
frames. **Control: today's build**, whose blob's saturation radius is a
function of `u_sunColor` and the sky ramp and therefore shrinks toward
sunset. Must fail.

**A3 — the opening frame contains two moons.** A frame taken at
`START_TIME_OF_DAY` on a fresh world contains both discs above the horizon,
separated by ≥ 20°, each with a terminator whose lit sliver is ≥ 2 px, and
each separated from the sky at its own limb by ≥ 2 · `PALETTE_SHADE_STEP_REF`.
*Aggregation:* per-moon, minimum over its 1–3 px limb annulus.
*Denominator:* annulus pixels, per moon. **Control: today's build**, which
places its single moon at elongation 0 inside the solar glare at lit = 0.
Must fail. This is also Rule 27's "a vantage that cannot fail" served: the
frame is the game's own first frame, so it cannot be chosen flatteringly.

**A4 — the moon is up in daylight.** Across a sweep of in-world 08:00–16:00
frames over 28 consecutive game days, at least one moon is above the horizon
and meets A3's separation in ≥ 60 % of frames. *Aggregation:* fraction of
frames. *Denominator:* all daytime frames in the sweep, **excluding** frames
where the only moon above the horizon lies within `MOON_SOLAR_EXCLUSION` of
the sun — an exclusion by cause, not by magnitude. Real-sky anchor: the Moon
is in the daytime sky on roughly 25 of each 29.5 days. **Control:** the same
sweep with a Skyrim-style sunrise alpha-out — 0 %. Must fail.

**A5 — phase and position agree.** Over one full Masser cycle sampled daily,
`f` computed from the rendered terminator matches `(1 − cos E)/2` from the
moon's measured elongation to within 0.05, at every sample. *Aggregation:*
maximum absolute error. *Denominator:* the 28 daily samples. **Control:
today's build**, whose sign error puts first quarter 90° west of the sun
instead of east — error 1.0 at the quarters, 0 at new and full. Must fail,
and note it passes at new and full, which is why a two-frame check would have
missed it.

**A6 — the moon rises later, not earlier.** Successive moonrise times over 7
consecutive game days are monotonically increasing, by 53.33 ± 3 in-world
min/day for Masser. *Aggregation:* per-day difference. *Denominator:* the 7
day-pairs. **Control: today's build**, which is monotonically *decreasing*.
Must fail. This is the assertion D3 would have survived without.

**A7 — the pair does not resynchronise.** Over 45 consecutive game days, the
minimum sky separation between the two moons at a fixed in-world hour is
> 0 on every day but at most one. *Aggregation:* count of days below
threshold. *Denominator:* the 45 days. **Control: Skyrim's 6:5 ratio**
(24 h / 20 h), which conjoins on day 5, 10, 15, … — 9 of 45. A real shipped
rejected instance, and the threshold sits above it.

**A8 — an occulted sun darkens the world** (W9.7-R1's acceptance; the
requirement is stated there with its trigger). Over a sweep of frames spanning
one Masser conjunction at which |β| < 3.90°, sampled at ≤ 2 in-world minutes
around greatest occultation, the **landscape's** mean luma is monotonically
non-increasing as the occulted fraction of the solar disc rises, and at
greatest occultation it is lower than at first contact by more than
2 · `PALETTE_SHADE_STEP_REF`. *Aggregation:* per-frame mean luma over
**terrain pixels only** — the sky is excluded **by cause**, since the sky is
where the black disc is and including it would let the defect darken its own
evidence (Rule 36). *Denominator:* the same frame's mean luma at first contact,
so the rule is a ratio against the un-occulted world and not against an
absolute the tone-mapper owns. **Control: today's build** — `SkyModel.cpp:139`
makes `sun_color` a function of elevation alone, so its landscape luma is
**flat across the whole sweep** and the sequence is non-increasing only in the
degenerate sense. Must fail, and it fails on the *strict* half of the pair,
which is why the rule carries both clauses instead of just monotonicity.
*Deliberately NOT asserted:* how dark totality gets — that is W9.7-R2, open,
and asserting it here would place a number nothing has measured.

Rule 38 applies throughout: these assert **outcomes** — "the disc reads as a
disc", "the phase matches the position", "the moon rises later" — never
mechanisms like "the glare cap is active" or "no clamp occurred". A clamp
occurring somewhere in a bright sky is correct rendering; a disc without an
edge is not.

## W9.10 Rows requested of the lead, and one row that must be retired

Requested (24): `SUN_ANGULAR_DIAMETER`, `SUN_GLARE_ANGULAR_DIAMETER`,
`SUN_DISC_LUMA`, `SUN_GLARE_LUMA_MAX`, `MASSER_ANGULAR_DIAMETER`,
`SECUNDA_ANGULAR_DIAMETER`, `MASSER_SYNODIC_DAYS`, `SECUNDA_SYNODIC_DAYS`,
`MASSER_ELONGATION_EPOCH`, `SECUNDA_ELONGATION_EPOCH`, `MASSER_INCLINATION`,
`SECUNDA_INCLINATION`, `MASSER_NODE_EPOCH`, `SECUNDA_NODE_EPOCH`,
`MASSER_ECCENTRICITY`, `SECUNDA_ECCENTRICITY`, `MASSER_NODE_PERIOD_DAYS`,
`SECUNDA_NODE_PERIOD_DAYS`, `MASSER_DISC_LUMA`, `SECUNDA_DISC_LUMA`,
`MOON_LIMB_OUTLINE_LUMA`, `MOON_SKY_LUMA_SEPARATION_MIN`,
`MOON_SOLAR_EXCLUSION`, and the derived-but-registry-worthy
`SKY_ANGULAR_PIXEL` (0.0042629 rad/px at 640×360 — two zones must agree on it
the moment either sizes anything against the sky).

**`LUNAR_MONTH_DAYS` must be RETIRED, not left standing beside the two new
period rows.** It was unambiguous while there was one moon. With two moons it
is a name that reads as "*the* lunar month", and the werewolf, vampire and
lunar-magic code its own registry note promises will reach for it and get
whichever moon the author happened to picture. This is Rule 37's exact shape:
nothing about the constant changes, no code is touched, no test goes red — the
defect is created by **a row landing in the registry**, nowhere near the code
that breaks. The question Rule 37 tells us to ask when a constant lands beside
existing ones is *what reads across this range*, and the answer here is "every
future system that cares about the moon".

**Parked with its trigger (Rule 35, predictive form):** with no year in the
model, synodic and sidereal periods coincide, so one row serves both. The day
seasons land, the sun acquires a declination, and they become two different
numbers. That is the thing gaining a dimension; the trigger is a year
appearing in the clock, not an argument about the moon.

---

# W10. Cloud volume — the diagnosis before the recommendation

User's words: *«облака классно выглядят, но слишком квадратные, плюс плоские,
надо ощущение их объёма передать, возможно создать реальные трёхмерные
облачка, для этого ещё алгоритмы поискать нужно»*. **He likes them.** Two
specific defects — too square, and flat — and an openness about the method.

## W10.1 The premise, checked in the source before anything was recommended

Rule 34, and the lead's standing instruction: shipping an expensive ray-march
that is still square would be the worst available outcome of this task. So the
squareness was traced to a mechanism first.

**It is the noise basis, and it is axis-aligned three times over.**

`dfn_env.sh:148-163` — the field is **value noise**: a `fract(sin(dot(...)))`
hash read at the four integer lattice corners and bilinearly blended.

1. **Value noise, not gradient noise.** Interpolating *scalars* makes every
   lattice point a local extremum, so the cell structure survives the blend and
   the cell centres read as blobs. Gradient noise stores a *direction* and
   evaluates a dot product, which makes the lattice point a **zero crossing**
   instead. *The Book of Shaders* §11 states the consequence plainly: value
   noise "tends to look **blocky**", and this is exactly why Perlin moved to
   gradient noise in 1985. FastNoise2's own type comparison: value noise has
   "more pronounced grid artifacts than gradient noise… visually
   blockier/smoother appearance."
2. **The interpolant is cubic smoothstep `f*f*(3-2f)`, not quintic.** Its
   second derivative `6 − 12t` is non-zero at both ends, so the second
   derivative is discontinuous at every lattice line — and *GPU Gems* Ch. 5,
   restating Perlin's 2002 *Improving Noise* (ACM TOG 21(3):681–682), notes
   that because shading varies with the derivative of the height function,
   **second-derivative discontinuities are visible**. The fix is one line:
   `6t⁵ − 15t⁴ + 10t³`.
3. **Three octaves, all co-aligned.** `dfn_env.sh:187-199` scales by
   1.00 / 2.03 / 4.07 and offsets by `+vec2(17,31)` and `+vec2(47,89)` —
   **translations only, no rotation between octaves.** A translation does not
   change a lattice's orientation, so all three octaves crease along the *same*
   world X/Z lines and their artefacts **stack** instead of cancelling. This is
   the single largest contributor: three co-aligned grids reinforce each other.
   Credit where it is due — the detuned lacunarity (2.03 / 4.07 rather than
   exactly 2 / 4) is *half* of Quilez's standing advice in his fBm article,
   and it is there deliberately: at exact doublings "the zeros and peaks of the
   different noise waves… don't superpose exactly, which can sometimes create
   unrealistic patterns." **The missing half of that same sentence is the
   rotation.** Whoever wrote this had read the right article and applied one of
   its two recommendations.

**The trap to avoid on the way out, stated now because it is the most common
cause of square clouds in shaders that have already been "fixed":** Worley /
cellular noise is itself grid-based, and at low jitter its cells *are* squares
(FastNoise2: "Grid Jitter — how much cell centres are displaced from their grid
positions; **0 = uniform grid**"). When Worley arrives in Tier 0 item 4, its
jitter goes near 1.0 or the squareness comes straight back wearing a new hat.

**And the nuance that decides whether this is actually fixed:** Perlin's 2002
revision changed the *interpolant* and the *gradient set* — it did **not**
change the axis-aligned cubic lattice. That is precisely why Perlin had to
invent **simplex noise separately (2001)**, which subdivides space into
simplices rather than hypercubes and is described as having "no noticeable
directional artifacts". So "we switched to improved Perlin" is **not** a claim
that axis alignment is gone; FastNoise2 still reports "subtle grid-aligned
artifacts at 45 and 90 degrees" for gradient noise. The per-octave rotation
(Tier 0 item 1) is doing work that no choice of basis does for us.

Coverage is *not* the culprit and should not be touched — `dfn_env.sh:205-219`
already uses a `smoothstep`, not a hard `step`, and the field is remapped
through a logistic CDF to be uniform on [0,1] (Rule 31 satisfied, and creditably
so). **The squareness is upstream of the threshold, in the basis.**

**And the flatness is not the absence of a ray-march. It is the absence of any
lighting at all.**
`fs_sky.sc:165-167` shades the sheet as `mix(cloud_bright, cloud_dark, density)`
and the cumulus as a lerp on normalised height. `dfn_cloud_bright()`
(`fs_sky.sc:120-125`) is a **per-frame scalar constant across the entire sky**.
A search for `beer|extinction|raymarch|henyey|mie|rayleigh` across the whole
engine returns **zero hits**. `u_sunDir` is read for the sheet's *ground*
shadow projection and for the sun glow — **it never lights a cloud.** There is
no normal, no optical depth, no self-shadow, no silver lining, no directional
term of any kind. The clouds are flat because nothing has told them where the
light is.

**A third mechanism, specific to the horizon cumulus and worse than both.**
`fs_sky.sc:181-227` builds the cumulus from a density field of **azimuth
alone**, thresholded against a rising function of elevation, on a ring at a
single fixed distance (`CUMULUS_RING_M` 20 000 m). A one-dimensional function
extruded vertically at one distance **has no depth by construction** — every
cloud in the bank is the same distance away, so there is no parallax, no
overlap, no occlusion, and no size cue. No shading model and no ray-march
repairs that; it needs distance variation before anything else can help.

## W10.2 What our resolution actually justifies (Rule 33)

The lead asked for this honestly, so here is the arithmetic rather than a
preference. The ruler is W9.0's `SKY_ANGULAR_PIXEL` = **0.24424°/px** at
640×360.

**A ray-march buys exactly one thing over a well-shaded 2.5D sheet: structure
that varies ALONG the view ray.** Everything else — coverage shape, edge
erosion, colour, silhouette in plan — a 2.5D field already has. So the question
has a geometric answer, not a taste answer.

**Overhead, the march buys nothing, and no resolution changes that.** The ray
crosses a layer of thickness H at elevation θ over a path length H/sin θ. When
that path is shorter than one feature length L, the density along the ray is
effectively constant and a march returns what a height-gradient lookup returns —
by construction, not by approximation. With `WIND_FIELD_WAVELENGTH` = 600 m as
L:

| sheet thickness | march and sheet agree above |
|---|---|
| 400 m | θ ≥ **42°** |
| 200 m | θ ≥ **20°** |

**On the horizon, where the march would pay, our pixel floor arrives after two
levels of detail.** The cumulus band spans 1100 → 3800 m at 20 km, i.e. 2700 m
subtending 0.135 rad = 7.74° = **31.7 px** tall:

| level | feature size | on screen |
|---|---|---|
| cloud form | 2700 m | 31.7 px |
| erosion 1 (÷4) | 675 m | 7.9 px |
| erosion 2 (÷16) | 170 m | **2.0 px** |
| erosion 3 (÷64) | 42 m | 0.49 px — **sub-pixel** |

**Verdict: 640×360 justifies exactly two levels of internal cloud structure,
and the second one lands on the 2-pixel floor.** A Horizon-class march resolves
four to five; at our resolution it would spend the majority of its work below
the pixel, and an integral whose variations are finer than the pixel footprint
returns the same value as a lookup of the mean. That is arithmetic, not
scepticism.

**The resolution at which a march starts paying for itself:** level 3 needs 4×
our angular resolution to reach 2 px, i.e. **0.061°/px → 2560×1440 internal**,
sixteen times our pixel count. That is explicitly not this project's look, and
`INTERNAL_RES` is a *user graphics setting* whose second preset goes the other
way, to 320×180 — where even level 2 falls to 1 px.

**No published source states a resolution threshold for this.** It was searched
for specifically and does not exist; the arithmetic above is ours, derived from
our own geometry, and is labelled as such rather than dressed in a citation.

**The nearest thing to an authoritative decision rule is Guerrilla's own, and
it initially appears to argue against this section — which is why it gets
answered rather than omitted.** The 2015 notes list what broke their previous
billboard/skydome approach: "none of the solutions made the clouds evolve over
time. There was not a good way to make clouds pass overhead. And there was high
memory usage and overdraw"; plus no inter-cloud shadowing, and pre-baked
lighting that only worked because Killzone had a **static time of day**.
Inverted, that is a four-item threshold — dynamic time of day, weather
evolution, clouds passing overhead, inter-cloud shadowing — and **we need three
of the four.** Read carelessly, that is a case for marching.

Read correctly, it is not, and the distinction matters: **those four are reasons
a BAKED SPRITE SYSTEM failed, not reasons a 2.5D field fails.** We already have
a procedural field evaluated per pixel, drifting on the shared wind field, with
its palette derived per frame from `u_sunColor`, and intersected by ray against
a plane at 2600 m so that clouds genuinely pass overhead with correct parallax
as the player walks under them. We satisfy three of Guerrilla's four criteria
**today**, with the flat sheet we already have. The criterion separates baked
from procedural, not 2.5D from 3D. Only inter-cloud shadowing is out of reach,
and no user ruling asks for it.

**And the cost side, in their numbers rather than ours.** Naive HZD was **20 ms
on PS4**; it shipped at ~2 ms only via a quarter-res buffer updating **1 pixel
in each 4×4 block per frame** with temporal reprojection, then upscaled from
half res. Even *Horizon Forbidden West* on **PS5** renders clouds at
**1920×1080** at 2–3 ms, and *Burning Shores* renders them at **960×540** —
four times our entire internal frame — with the near/far split needed because
temporal upscaling was "unusable for near clouds". Our engine has **one**
offscreen target and no reprojection of any kind (audited). Tier 2 therefore is
not "add a march"; it is "build a temporal reprojection system, then add a
march."

**So the recommendation is: do not ray-march. Two levels of structure and a
real lighting model are reachable from a 2.5D sheet, and they are what the user
asked for — he asked for the *sensation* of volume («ощущение их объёма»), not
for a volume integral.**

**The precedent is a shipped title that made this exact call.** Rare, on *Sea
of Thieves* (SIGGRAPH 2018 Talks): "we chose **not to pursue a ray-marched
approach**… Instead, we developed a system that renders opaque geometry with
simple per-vertex illumination that **approximates sub-surface scattering**",
rendered to an off-screen buffer at quarter res and blurred, blending low- and
high-frequency noise by camera distance "to further give the impression of
depth". That is the target: the *impression* of depth, bought with shading.

## W10.3 The ladder, cheapest first

**Tier 0 — fixes "square". Nearly free, and it must land first.**

1. **Rotate the lattice between octaves.** A fixed 2×2 rotation applied
   cumulatively per octave (the conventional `mat2(0.80, -0.60, 0.60, 0.80)`,
   36.87°) so no two octaves share an axis. Two multiply-adds per octave, and
   it is the largest single win available because it stops three co-aligned
   grids from reinforcing.
2. **Quintic interpolant** `6t⁵ − 15t⁴ + 10t³` in place of `f*f*(3-2f)`
   (Perlin, *Improving Noise*, 2002). One line; removes the C² discontinuity
   at every lattice line.
3. **Domain warp** — evaluate `field(p + k·warp(p))` where `warp` is a second
   noise (Quilez, *Warping*). One to two extra fbm evaluations. **Stated
   honestly: Quilez claims an "organic quality" for this, not the removal of
   grid artefacts** — the anti-alignment inference is ours, and it is why this
   sits at item 3 rather than item 1. Take it for the shape, and let items 1
   and 2 carry the artefact.
4. **Add a Worley/cellular component** (Schneider's Perlin-Worley, the exact
   operator being `remap(perlin, 1.0 − worley, 1.0, 0.0, 1.0)`). Worley
   inverted "makes tightly packed billow shapes", and Schneider's complaint
   about plain fBm is precisely that it "lacks those bulges and billows that
   give a sense of motion". **This is the step that changes the *character* of
   the shape.** Note carefully what it is *not*: Schneider says nothing
   anywhere about grid or lattice artefacts, and the squareness diagnosis in
   W10.1 must not be attributed to him. Worley buys billows; items 1–2 buy
   isotropy. Two different defects, two different fixes, and conflating them is
   how a team ships Perlin-Worley and stays square.
   *(Forward note: by Nubis³ (2023) Guerrilla had replaced Perlin-Worley
   outright with Alligator noise and curl-distorted Alligator, for "more
   appropriate cloud-like lacunarity". Not our problem at this budget, but the
   2015 paper is no longer their own state of the art.)*

**Tier 1 — fixes "flat". Cheap, and it is where the volume sensation actually
comes from.**

5. **A directional term against `u_sunDir`** — today `u_sunDir` does not reach
   cloud shading at all, which is the whole of the flatness. Two candidate
   normals, and the shipped one is the cheaper:
   - **Radial (recommended, and it is what Flight Simulator 2004 shipped —
     Wang, GDC 2004):** the dot product of *(cloud centre → sun)* with *(sample
     → cloud centre)*. Needs a centroid, needs no derivatives, and Microsoft
     chose it explicitly over simulating scattering.
   - Gradient normal by finite differences of the coverage field. **Flagged:
     this is attested only in community posts, not in any primary talk.** It is
     the obvious thing, not the proven thing.
   Pair either with FS2004's other trick: **ambient as a few authored colour
   levels interpolated by height**, which is what gives cumulus their dark
   bottoms without any transport model.
6. **Beer's law on an approximate optical depth**, `T = 1 − e^(−τ·Δs)`. This
   has real, citable prior art with **no marching**: Harris & Lastra's
   real-time clouds (UNC TR 01-005, 2001) put it directly in their pseudocode
   with τ = 8.0, albedo 0.9; and *Spherical Billboards* (Umenhoffer,
   Szirmay-Kalos & Szijártó, ShaderX5 / GRAPHITE 2006) **solve Δs in closed
   form in the fragment shader** rather than integrating it. **Flagged
   honestly: no published article derives cloud optical depth from a 2D
   heightfield and shades a sky layer with it. That specific formulation is
   folklore, and we would be doing it because the geometry supports it, not
   because someone else proved it.**
7. **The powder / Beer's-Powder term** — `e^(−d)·(1 − e^(−2d))`, Schneider's
   own invention for the dark edges facing the light. **This is the single most
   volume-suggesting term in the whole Nubis model and it is a function of
   density, not of a march** — which is precisely why it transfers to us. Note
   it is view-dependent, visible only where the view vector approaches the
   light vector. (Nubis 2017 replaced it with an in-scatter probability
   function; the 2015 form is the cheap one and the right one here.)
8. **Silver lining** — Harris & Lastra get it from a Rayleigh phase function
   ("their characteristic silver lining when viewed looking into the sun");
   Nubis uses dual Henyey-Greenstein, `max(HG(θ,0.6), silver·HG(θ,0.99−spread))`.
   Either is a few instructions.

**Tier 2 — a march, and only for the horizon cumulus, and only after 0 and 1.**
And before any of it: give the cumulus **distance variation**, because a ring
at one distance cannot have depth no matter how it is integrated.

**The ordering is the deliverable, not a nicety.** Tier 2 without Tier 0 is a
ray-marched cloud that is still square — the outcome the lead named as the
worst available. Tier 1 without Tier 0 is a well-lit rectangle. And Horizon's
clouds read as volumes because of Beer, powder and cone-sampled self-shadow —
the *lighting* — not because rays were marched; the march is how those terms
get their inputs at 1080p+, and at 640×360 a density field supplies the same
inputs.

## W10.4 Acceptance

**C1 — the field has no preferred direction.** Take the cloud coverage field
over a 4096×4096 m patch, compute its 2D power spectrum, and integrate power
into 36 angular bins of 10°. The ratio of the strongest bin to the median bin
is ≤ 1.25. *Aggregation:* max/median over bins. *Denominator:* 36 bins over
one patch. **Control: today's field** — three co-aligned lattices put power
spikes on the 0° and 90° bins; it must fail. **Can-pass case (corollary 30a):
the same field with per-octave rotation only**, no other change — the fix must
be sufficient on its own, or the test is measuring the wrong quantity.
Chosen because it is the one quantity on which "square" and "not square"
actually separate: a coverage histogram does not (the logistic remap already
makes both uniform), and an edge-length metric does not (a rotated square has
the same perimeter).

**C2 — clouds take the light.** In a pair of frames at the same timestamp with
the sun in the east and in the west, the same cloud's sunward and anti-sunward
halves differ in quantiser luma by ≥ 2 · `PALETTE_SHADE_STEP_REF`, and the
bright half **swaps sides** between the two frames. *Aggregation:* mean luma
per half, over the cloud's pixels. *Denominator:* the cloud's pixels, split by
the plane through its centroid normal to the sun azimuth. **Control: today's
build**, whose cloud colour is a per-frame scalar identical across the whole
sky — the halves differ by 0 and nothing swaps. Must fail. The swap is the
load-bearing half: a density lerp can produce a luma difference by accident,
but it cannot make it follow the sun.

**C3 — the horizon bank has depth.** In one frame, the cumulus band contains at
least two clouds whose silhouettes overlap, with the nearer one occluding the
farther. *Aggregation:* count of occluding pairs. *Denominator:* the frame.
**Control: today's build** — a ring at a single distance can produce zero
occlusions by construction. Must fail.

Rule 38: these assert outcomes — the field has no preferred direction, the
clouds take the light, the bank has depth — never "a rotation matrix is
applied" or "a march ran". A cloud that happens to look right with a cheap
model is a pass, which is the entire point of W10.2.

## W10.5 Rows requested

None yet, deliberately. Every Tier-0 item is a change of *mechanism* inside
render's own shader with no second consumer, so by Rule 35 it does not belong
in the registry. The rows appear at Tier 1, when the layer acquires an
authored **thickness** and a **scattering coefficient** — at which point the
sheet stops being a plane and gains a dimension, and core's coverage field
must agree with render's about how deep the cloud is. That is Rule 35's
predictive trigger stated in advance: the number to watch for is
`CLOUD_LAYER_THICKNESS`, and it becomes a registry row the moment the ground
shadow's softness is derived from it.

## W10.6 Sources

- Schneider & Vos, **"The Real-Time Volumetric Cloudscapes of Horizon Zero
  Dawn"**, SIGGRAPH 2015 Advances in Real-Time Rendering
  (`advances.realtimerendering.com/s2015/`); book form GPU Pro 7 ch. 11.
  Perlin-Worley, the 128³/32³/128² texture set, three height-gradient presets,
  the RGB weather texture (coverage / precipitation / cloud type),
  Beer's-Powder, 6-sample cone lighting, 64→128 ray steps, 20 ms → ~2 ms.
- Schneider, **"Nubis: Authoring Real-Time Volumetric Cloudscapes with the
  Decima Engine"**, SIGGRAPH 2017 — the remap-based combination, nested-remap
  height gradients, in-scatter probability, dual-HG and dual-Beer, and the
  22 → 1.2 ms optimisation ladder.
- Schneider, **"Nubis, Evolved"**, SIGGRAPH 2022, and **"Nubis Cubed"**,
  SIGGRAPH 2023 — Alligator / curl-distorted Alligator replacing
  Perlin-Worley; SDF-derived voxel dimensional profiles; PS5 resolutions.
- **Quilez**, *fBm* and *Warping* (`iquilezles.org/articles/`) — per-octave
  rotation and detuned lacunarity; domain warping.
- **Perlin**, *Improving Noise*, ACM TOG 21(3), 2002; *GPU Gems* ch. 5 for the
  quintic-interpolant rationale. **Simplex noise (2001)** for isotropy.
- *The Book of Shaders* §11 and the **FastNoise2** noise-type comparison — why
  value noise is blocky, and Worley grid jitter.
- **Harris & Lastra**, *Real-Time Cloud Rendering*, UNC TR 01-005 / CGF 2001 —
  Beer's law without marching, precomputed shading, Rayleigh silver lining.
- **Umenhoffer, Szirmay-Kalos & Szijártó**, *Spherical Billboards*, ShaderX5 /
  GRAPHITE 2006 — closed-form optical depth in the fragment shader.
- **Wang**, *Realistic and Fast Cloud Rendering* (Flight Simulator 2004),
  GDC 2004 — the radial fake normal and height-interpolated ambient levels.
- **Ang, Catling, Ciardi & Kozin**, *The Technical Art of Sea of Thieves*,
  SIGGRAPH 2018 Talks — the shipped decision **not** to ray-march.
- **Patry**, *Ghost of Tsushima* sky, Advances 2021 — the middle road: a real
  march, but into a 768² paraboloid time-sliced over 60 frames.

Deliberately **not** cited, because no authoritative source exists and the
research flagged each: any published pixel-footprint threshold for marching vs.
2D (W10.2's arithmetic is ours); a GPU Gems cloud-impostor chapter (there is
none); and Breath of the Wild / Genshin / Journey cloud techniques (forum
analysis only).
