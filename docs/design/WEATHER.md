<!--
Created: 10:08:2026 - 02:04:16
Last updated: 10:08:2026 - 02:04:16
-->
<!--
UPD:
- 10:08:2026 - 02:04:16: Weather doctrine skeleton (user ruling в4: all four, CLOUDS FIRST; в10 three cloud kinds at once; в12 ambient sound; в21 Gerstner off the shared wind field). Defines what a weather STATE is (a named parameter tuple in data), how states TRANSITION (adjacency graph, upwind arrival, the wind field persists across transitions), what a ZONE PROFILE contains (state distribution + terrain-aware modifiers: fog pools in swales), and each stand's profile. Acceptance conditions and controls per state; NUMBERS rows named as proposals; ownership split flagged for the lead where open.
-->

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
5. **The schedule is seeded and deterministic** (Rule 13): state sequence and
   dwell times drawn from `WorldGenRng`-style streams keyed (seed, day,
   zone), so a reported frame is reproducible. Dwell band
   `WEATHER_DWELL_S` (600–1800 s proposed).

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
  effects), the local-floor derivative for W5, the seeded schedule.
- **render** — sky, shadow sampling, fog draw; **audio** — W7 mixes.
- **OPEN (lead's call):** where the state machine itself runs (sim vs app) —
  it ticks with game time and must serialize with saves when saves land.
- **OPEN:** whether zones within one big-world map need boundary blending of
  profiles (stands don't; defer until the big world composes regions —
  same door as §2.10 blending, likely the same warp answer).
