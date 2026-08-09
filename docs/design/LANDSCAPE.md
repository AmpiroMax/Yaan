<!--
Created: 09:08:2026 - 10:45:06
Last updated: 09:08:2026 - 21:36:00
-->
<!--
UPD:
- 09:08:2026 - 10:45:06: Created the landscape design bible (stage 3): composition principles, detail layers with worldgen pass order, water rules, terrain palette, flora and structure catalogs, testbed v2 plan, sources. All new numeric values are proposals pending NUMBERS.md approval.
- 09:08:2026 - 12:44:58: Amendments from render's stage-3b look-dev probes: C1/C4 visibility validation is now canopy-aware with a clearance factor and L0 sight wedges (pine wall buried the crag from town); riverbed/mud splat band capped (WaterBed 2.74% vs ~1% water was over-wide); dist_to_water field range requirement; §7.1 fords are now derived from the generated trace (fixed coords removed), seed-1 generated hydrology actuals recorded, hamlet flood-margin re-score flagged.
- 09:08:2026 - 13:15:01: Core implemented all three rulings — doc synced to built state: crag pines are radial ridge strips (closed annulus can never pass canopy-C1), peak-raising marked a dead-end knob, §3.1 fords strengthened (bed truly raised; whole corridor mask ford-shallow), §7.1a open items resolved (lake "sprawl" was pond overflow, true basin at target), new rule: water-adjacent placements are derived-only, never tabled.
- 09:08:2026 - 13:17:06: §2.1 landform anisotropy rule from user feedback (feature_requests.md Запрос 1): mid-scale hills must be elongated direction-coherent ridgelets, not round bumps; HILL_ANISOTROPY 2.0-3.0 proposed.
- 09:08:2026 - 13:18:17: §2.1 anisotropy sharpened per core's implementation intent (agreed at sync): mid octave only, drifting per-valley axis field (no global corduroy), recorded cautions — river trace will shift (safe under the §7.1a derived-only rule) and work is gated on HILL_ANISOTROPY landing in NUMBERS.md + lead scheduling.
- 09:08:2026 - 13:19:34: §2.1 technique decided (core + design): anisotropic input-stretch chosen over domain-warp — elongation along the axis, cross-axis rhythm pinned at 128 m (what corridors/C1 grid feel); domain-warp rejected (wiggles crests, dilutes the shared-axis read); ping-first threshold at ~100 m cross-axis compression.
- 09:08:2026 - 13:22:17: HILL_ANISOTROPY approved at 2.5 in NUMBERS.md (stage-3 close, sync №3) — §2.1 proposal marker removed, gates recorded as cleared, P1 retune scheduled with §2.1 as the contract.
- 09:08:2026 - 15:00:23: New §6.1 — castle (House Corvane's seat, story pitch A) ruled as L1-max staged inside the L0's angular footprint: crag keeps the skyline, castle reads against its body, flank occlusion allowed / crown occlusion forbidden, scored in C1 both as occluder and as attractor, binding fix order on C1 failure. Siting (spur pad, ford command, barrow proximity, composite POI), minimal-version mass table, mid-range readability ruling, testbed pad at (760,330); castle row added to §7.1.
- 09:08:2026 - 15:05:00: §6.1 folded in story's constraints for "Harrowward": gentry hall-castle mass program (horizontal hall + single solar vertical replaces the tall keep — also buys C1 clearance headroom), value-not-height doctrine tying the Ward to the crag's rock value, binding access invariant (graded ramp on the approach side within §2.4 corridor limits — a scarp-only pad is a failed placement), and a checkable Ward→Backbarrow sightline. CASTLE_KEEP_HEIGHT retired in favour of CASTLE_HALL_HEIGHT/CASTLE_SOLAR_HEIGHT/CASTLE_GATE_HEIGHT.
- 09:08:2026 - 15:08:24: §6.1.2 — gate orientation settled as valley-facing (story canon, BIBLE §5.1), and the two new castle invariants (approach ramp, yard/gate->barrow sightline) explicitly joined to the C1-guarded set: re-validated by the same canopy-aware raycast on every worldgen run, not once at authoring time (raised by story: a later pine retune could occlude either).
- 09:08:2026 - 15:22:13: C2 scope corrected (my error): POI_VISIBLE_COUNT is region-only per NUMBERS.md/Q46 and is unsatisfiable at testbed density alongside LANDMARK_VISIBILITY_MIN — general-bound citation withdrawn. Added Rule C2-testbed (no coequal crowd): max 2 attractors within a 2.0 subtended-size ratio, L0 exempt, composite POIs count once; occlude-and-reveal remains the real guarantee. Region bound unchanged.
- 09:08:2026 - 15:33:48: C2-testbed limit raised 2 -> 3 on measured evidence (seed-1 crowd is three threshold-scale marks at 8-11 px), with a tightening to 2 for large crowds (COEQUAL_LARGE_PX 24 px). Blessed core's three measurement definitions into the doc: apparent size = height/distance (not top elevation angle, one definition shared by C2/C4/R4), only >=SILHOUETTE_MIN_PX attractors compete, body-backed attractors exempt per standpoint with the raw count still reported.
- 09:08:2026 - 17:28:51: New §6.2 — dungeon entrance archetypes after a live player read a mis-sited entrance as a bug: relief-selected adit vs sunken barrow; flat-ground answer is a stamped mound + cut forecourt + lintel (generator makes the relief it needs) with four findability layers; marker/facing derived from the carve mouth (derived-only rule extended to carve-adjacent placements); attractor status ruled (assembly counts, hole never does, short-range L1 only).
- 09:08:2026 - 18:55:22: Stage-4, five user decisions ruled. §1.3a — world to 2x2 km: testbed and region contracts coexist SPATIALLY (home valley / transition band / open region), new top landmark tier LR, LANDMARK_VISIBILITY_MIN measured over each landmark's own domain, LR and L0 separated by atmospheric depth rather than angular size. §2.5 — temple mountain: 280 m relief, massif ratio, ridged noise + irregular buttress ridges + cliff bands + asymmetry, checkable anti-dome invariant (lobed slice + 60% rock slope), mandatory validated ascent. §2.6 — border mountains replace invisible walls: varied crest, lobed spurs, impassable by slope AND a traversability flood-fill, never counted as attractors. §2.7 — micro-relief octave everywhere + one plain sited to frame the LR reveal. §5.7 — tall trees worked through: oak 24-32 m, birch 16-22 m, pine 28-38 m (declines literal x4, would overtop Ravenscar), L0_RELIEF 52 -> 110-120 m proposed, sight wedges become tree-free, crown base 35-45%, TREE_SPACING_FOREST 12-18 m (~80% density cut), tri budget 700 + mandatory LOD.
- 09:08:2026 - 19:01:50: Flora's findings ruled. §1.3 — canopy occlusion is now a BAND (crown_base, crown_top) with transparent trunks up to CANOPY_TRUNK_PATH_MAX 250 m; the old solid-column model was pessimistic and was failing sightlines that exist. §1.3 C4 — recorded a boundary on the "raising the peak is a dead end" finding: dead for stamp-scaled flanks, live for fixed-height canopy occluders; one measurement requested. §5.7 — sight-wedge ban narrowed to the tall three in the NEAR half only (flora's bald-lane pushback accepted, their near/far reasoning corrected: near trees steal dominance, far trees steal sight). §5.8 — maturity tiers 15/60/25 with Elder Oak folded in as the giant tier. §5.9 — standing snag (only flora allowed at full height in a wedge) and riparian willow approved, with birch=moving water / willow=still water as the readable split.
- 09:08:2026 - 19:06:29: C1 blocking ruling. §1.3 — binary forest opacity RETIRED in favour of Beer-Lambert attenuation over expected canopy hits (T = exp(-sum n_local*w(h)*d), visible at T >= CANOPY_VISIBILITY_MIN 0.25; permits ~225 m of trunk-level or ~24 m of crown-level forest), which subsumes and retires the ad-hoc CANOPY_TRUNK_PATH_MAX; same transmittance governs C4. Stated as a physics correction allowed ONCE — if seed 1 still fails, the world changes, not the floor. Recorded core's measurements: heights cost -0.048, band recovers only +0.011, crown-base fraction is visibility-insensitive. §1.3 C4 — my "live for canopy" boundary REFUTED by measurement in 52-64 m with the mechanism now stated (the crag's own flanks dominate and it hides itself faster than it gains height); above 64 m untested due to WORLDGEN_MAX_HEIGHT saturation, so L0_RELIEF stays open pending the re-run at the raised ceiling.
- 09:08:2026 - 19:14:17: STOP-THE-LINE correction + stage-4 batch. §1.3 — "raising the peak lowers clearance" WITHDRAWN entirely: core's C1 raycast was counting the crag as an occluder of itself, so every C1 number this stage was contaminated; corrected, clearance RISES with peak (52->0.751 ... 200->0.915) and the taller canopy never broke C1 (0.751 vs 0.60 floor). Recorded why it survived — directionally plausible findings get less scrutiny than surprising ones. Beer-Lambert attenuation kept as the better model but explicitly NOT load-bearing: the one-time physics-correction budget was never spent. §1.3a — LANDMARK_MAX_DISTANCE 4 km bounds the far plane (CAMERA_FAR 1000 -> >=4000), beyond it is backdrop; 10x10 km gets one LR per 4x4 km cell. §2.5 — "7000 steps" ruled as a staged climb (1200-1800 m path, 5-7 landings), literal reading flagged to the user. §2.7 — meso relief band 25-60 m / 1.5-4 m as GENERAL terrain (forest-only would seam the forest edge) + scarps 2-5 m. §5.8 — maturity re-weighted 25/60/12/3 (sapling rare per user; sub-mature retained because young trees do mid-canopy layering, not ground fill). §5.9 — snag density split by material: 1.5-3/ha weathered grey inside forest, 0.25-0.5/ha pale bone in the open, preserving the false-L2-guide rule. §5.10 — BigBush, FallenLog (big/small, across the fall line), trees on scarp edges approved. §6.1 — castle REVISED to a real fortress per user: 80x80 curtain + 4 towers + twin-tower gatehouse + hall + keep, scalable by terraced wards A/B/C, pad 60->120 m; safe because Ravenscar's growth gives ~2x architectural headroom under the same siting mechanism. §6.3 — NEW: true-darkness places, graded AMBIENT_FLOOR, qualification by enclosure + 25 m depth, three anti-surprise layers, torch floor 4 m, C1 exempt with a findable-way-out guarantee.
- 09:08:2026 - 19:15:39: §5.7 sight wedges RE-RULED after the C1 correction (flora asked for the re-decision): near/far half-split replaced by the single crown-vs-flank test already used for the castle; giants ALLOWED in wedges (one per wedge) because an off-axis elder gives the landmark scale — repoussoir is our best depth cue and the exclusion deleted it; C4 sharpened to govern masses and built structures rather than individual near vegetation.
- 09:08:2026 - 19:19:07: Flora's root-flare finding. §5.10 — cliff-edge setback datum corrected: >=1.5 m measured from the OUTER EDGE OF THE ROOT FLARE, not the trunk axis (a 1.6x flare left only ~0.5 m of ground and the tree would still have floated); cliff lean recorded as a separate, larger parameter than the crowding lean. §2.7 — added the standing rule that terrain never flattens under vegetation: the plant absorbs the ground via root flare, because a smoothed disc under every trunk is the pool-table flatness this section exists to remove; future "floating trees" bugs are flora/render fixes, not terrain ones.
- 09:08:2026 - 19:20:45: §6.1.3 — story picked fork (a) (the Corvanes fortified because they feared what they buried), so two terrain asks folded in: ward masonry phasing carried by BLOCK SIZE AND VALUE rather than texture (invisible at 640x360), which costs nothing because the terrace order already puts the oldest ward uphill nearest the barrow; and a binding sightline from the barrow-facing corner tower's top to the barrow entrance, validated and occlusion-protected like the yard/gate sightline.
- 09:08:2026 - 19:23:49: §6.1.3 — three named masonry phases from story (panic / treaty money / fear returning), and resolved an ambiguity I created: my A/B/C were BUILD stages, story's are IN-WORLD construction generations, all present when the player arrives — the doc now means the in-world axis, with implementation minimum A+B. Phase C ruled UNFINISHED as generator rules: 0.4-0.6 partial arc with the completed arc covering the APPROACH and the gap on a flank (a gap on the approach would make the gatehouse decorative and kill the petitioner ritual), raw stepped unfaced ends, 0.6-0.75 height, no parapet, spoil heap and never-laid dressed stone. Nearly free against R3 and the silhouette budget.
- 09:08:2026 - 19:27:13: §6.1.3 — C's gap placed on the BARROW-FACING flank (story's ask; bearings checked: barrow 27 deg, peak 28 deg, approach 225 deg, so grave and road are opposite sides and the approach stays walled). Forced one refinement: since the barrow side is uphill where ward A sits, C is a contour-following perimeter wrapping A and B rather than simply the lowest terrace — also the more authentic form, since uphill outer works matter most on a hillside. Noted that the two story asks reinforce: the barrow-facing tower watches the grave THROUGH the unbuilt stretch, so sightline clearance is guaranteed by absence rather than by a height check. Gap reachable off-corridor only — a back way, never an alternative front door.
- 09:08:2026 - 19:30:26: §2.5 — "7000 steps" CLOSED by user decision: it is a name, not a step count; 1200-1800 m / 5-7 landings / ~8 min stands, fiction keeps the name. §6.1.3 — gap reachability promoted from observation to VALIDATED INVARIANT, since story's act-1 trespass route now depends on it alongside the act-3 muster: continuous traversable route from barrow ground up the NNE spur (validated like the castle ramp and summit ascent), deliberately non-corridor-grade at SCRAMBLE_SLOPE 30-45 deg, passing within 40 m of the barrow entrance so the act-1/act-3 rhyme is geometric rather than lucky, and the completion fraction recorded as having two dependents.
- 09:08:2026 - 19:34:15: Story's near-miss (they nearly attached the Steps to act 1's climax, which is a different mountain) produced three fixes. §2.5 — added a boxed TWO DIFFERENT CLIMBS warning (the Steps are the regional massif, act 2; Ravenscar's climb is the local L0, act 1); specified the Steps as a BUILT stair in four generations of disrepair, with disrepair strictly visual/routing and never impassable; landings are now STATIONS with built markers, and LR_ASCENT_LANDINGS gains narrative dependents (5-7 landings = 5-7 rite beats), so it is no longer a free pacing knob. §7.1 — filled a real gap in my own doc: Ravenscar had no validated summit route despite act 1 climaxing there; now required and validated like the temple ascent and castle ramp, and specified as an informal worn PATH rather than a stair so the two climbs never read as the same place.
- 09:08:2026 - 19:38:02: §2.5 — LR_ASCENT_LANDINGS pinned at 7 (was a 5-7 band); story ruled the count since it now carries narrative dependents, one landing per station of the naming rite, which anchors the stair's name diegetically while the user's never-count-steps rule stays intact. Verified rhythmically before pinning: 7 landings over 1200-1800 m give 57-86 s segments at WALK_SPEED, inside the testbed POI_TRAVEL_TIME band across the whole range, so the climb's cadence matches the valley's exploration cadence; ~40 m of relief per station keeps the view changing.
- 09:08:2026 - 19:44:49: New §7.0a — barrow re-sited after the L0 raise buried it (a cascade from my own proposal, owned). RULING: swing the bearing into a couloir, do NOT move the castle — core's outward-move options were rejected because moving the castle cascades into ford command, approach corridor, trespass route, ward count and the R1 check to buy what a bearing change buys outright. Measured a ~60 deg feasible arc (bearings 180-240 at radius 90-110 keep CASTLE_BARROW_DIST legal with the castle unmoved); core searches it for low terrain nearest the current 209 deg. Fallback is a high shoulder entrance, pre-cleared with story because it inverts "the seat stands over the grave". Durable rule extracted: §7.1 coordinates are stamps against a terrain state, so changing a landmark's relief invalidates every placement on its slopes and re-validation is part of the change, not a follow-up.
- 09:08:2026 - 19:59:31: NEW §2.8 — the anti-dome ruling, second pass, after the user rejected the mountain a THIRD time with a shape brief (sharp / ribs / cubes on cubes / contour lines at varying spacing / non-uniform steps). Diagnosis measured by core before ruling: the dome is in the SPEC, not the mesher — the L0 stamp is smoothstep(1-d/R)*peak with a noise term that vanishes at the summit, i.e. zero slope at the apex, max at mid-radius, zero at the hem; lobe ratio 0.80/0.81/0.80 (identical at every height = self-similar cone), 0.0% of surface over 55 deg, 33.2% in one 40-50 deg bin, and surface nets measured NOT to be losing slope. My predecessor's invariant judged honestly on both counts: it was never BINDING (written in §2.5 for the LR, never evaluated on the L0 the user is looking at) and it was also INSUFFICIENT (one slice cannot see self-similarity; "60% above 40 deg" is satisfied by a uniform cone, which is the complaint). Ruling: massifs are authored as a BANDED CONTOUR MODEL — non-uniform band elevations, per-bearing radial lobing (aretes/couloirs), per-bearing profile exponent p>1 (concave, the actual anti-dome fix; smoothstep is the breast curve), per-sector cliff-vs-ramp risers — plus nine checkable invariants (I1 concave profile, I2 sharp summit, I3 near-vertical rock exists, I4 no 10-deg slope bin over 30%, I5 riser/bench alternation, I6 band-spacing CV, I7 arete detection by aspect-turn with vertical persistence, I8 lobe ratio at three heights AND rising, I9 blocky rock coverage). Recorded a REJECTED test with its measurement so nobody re-proposes it: raw contour-spacing CV is useless, the current dome scores 0.935. §2.8.4 — «кубы на кубах» ruled PLACED MESHES (Bethesda's answer), NOT dual contouring, because DC does not raise the sampling rate and a block under ~3 m cannot survive 1 m voxels at all; the summit becomes a TOR carrying the tower ruin. §2.8.5 — cross-constraints: validated ascents survive as BREACHES in the cliff bands (which makes the route legible from the valley, a gain), castle R1-R4 re-run, the §7.0a barrow couloir search should now succeed because lobing creates couloirs by construction, trees move onto benches, render gets a hard-splat-edge exception at band lips. LR_LOBE_RATIO renamed MASSIF_LOBE_RATIO — the constant's NAME was the bug. §2.5 item 5 superseded; §7.1 Ravenscar row rebuilt on §2.8 and named the acceptance case (it fails all nine invariants today).
- 09:08:2026 - 20:01:50: §5 global rule corrected on flora's flag (stale-line class): foliage is the explicit exception to "hard-edged mesh" — alpha-cutout cards and a see-through canopy per user direction, trunks/branches/bushes/logs/snags unchanged. Recorded what does NOT change so the exception cannot widen later: the crown ENVELOPE still governs and every size band, crown-base fraction, spacing and density is untouched. Two rulings folded in with it: a porous canopy is a SMALLER w(h) in §1.3's Beer-Lambert form, i.e. a coefficient re-measurement and NOT a claim on the never-spent one-time physics-correction budget; and dappled light is a LIGHTING problem, not a geometry one, because a caster thinner than render's caster floor reads solid however open the card is.
- 09:08:2026 - 20:04:42: §2.8 amended from core's and flora's reviews, four fixes and one withdrawal. §2.8.2 — BOTH per-bearing fields and the riser-class sector index must be PERIODIC in theta (core): sampling noise on the angle VALUE puts a branch cut at +-pi and produces a vertical seam from summit to foot; sample on (cos, sin) instead, and wrap the sector index. §2.8.3 — I5 alternation is counted on radials carrying NO validated route (core): a route breaches the bands it crosses, so counting it there would make the ascent cause its own invariant to fail, which is the §6.2 pad-scorer mistake in new clothes; breaches are asserted separately. Also recorded the consequence of I3 that must be loud rather than discovered: 55-deg risers exceed PLAYER_MAX_SLOPE, so a compliant massif is unclimbable off-route and route-validation failure leaves the summit UNREACHABLE with act 1's climax on it — a hard seed failure, never a warning. §2.8.5 — my "one cluster per bench segment" WITHDRAWN as the wrong unit (flora): a count does not survive scale, one stand on a 200 m bench is a potted plant just as a continuous line is landscaping, and both are the same failure. Replaced by a duty cycle (BENCH_VEG_DUTY_MAX 0.25, BENCH_CLUSTER_LENGTH_MAX 25 m, BENCH_CLUSTER_GAP_MIN 40 m) whose load-bearing member is BENCH_BARE_FRACTION_MIN 0.40 — what makes a stand above a 12 m drop extraordinary is that the ledges above and below are BARE. Added flora's lip-bias rule (BENCH_VEG_LIP_BIAS, outer 0.40 of the legal band), which matters more than the cap: a tree at the inner edge is dark-on-dark against the riser and invisible, the same tree near the lip has SKY behind it — §1.5's skyline rule at band scale. My bench-width worry was a double-count and is dropped: only the outer lip is a drop, so vegetation floors (5.0 m / 7.0 m giant) sit BELOW the 6 m terrain minimum and no terrain change is needed. Treeline and rockline now SNAP TO THE NEAREST BAND LIP — a flat elevation across banded terrain lands mid-riser and reads as a mowing line.
- 09:08:2026 - 20:10:47: §4 and §2.8.4/§2.8.5 amended from render's review. §4 — my narrow "hard splat edge at band lips" ask came back REFRAMED AS THE GENERAL RULE and the reframing is better: DITHER WHERE THE GEOMETRY IS SMOOTH, SNAP WHERE THE GEOMETRY HAS AN EDGE. Not a carve-out from the dither rule but that rule applied to a surface with creases, so it generalises free to quarry faces, cut terraces, the castle pad cut and cave mouths. Mechanism is a screen-space slope derivative (fwidth): two ALU instructions, no new data from core, NO CONSTANT FROM DESIGN, and the threshold is explicitly render's to set by looking at a 640x360 frame — its unit is degrees per PIXEL, which is resolution-dependent, and INTERNAL_RES is a user setting, so a number derived here would be wrong at 320x180. §2.8.5 — recorded that the splat coupling §2.8.2 depends on is STRUCTURALLY protected rather than merely agreed: render's standing post-"brown wash" ruling forbids re-deriving material from raw height/distance fields, so slope-augmented rock between the §4 thresholds is the only mechanism available to them and the contour rhythm is safe by construction, not by anyone remembering. §2.8.4 — NEW CONSTRAINT I did not know: at render's shadow-map resolution anything under ~0.31 m across CASTS NO SHADOW, and unlit geometry on shadowed ground reads as pasted on. Our 1.5-4.0 m blocks clear it, but the ruling that follows is durable — the crisp read must come from the block SILHOUETTE, never from arris detail, since a chamfer under the floor costs triangles and returns no shading. Same conclusion §6.1.3 reached about masonry coursing and §1.5 about battlement teeth, now with a measured number. Explicitly NOT a NUMBERS.md constant: it is derived from a render SETTING and moves with it, so future smaller prop classes must re-ask rather than cite this line. Also recorded that placed rock needs no new batching work — stacks bake into the same per-chunk merged buffers the trees use, and render accepted the 60000-tri / ~270-stack budget unchanged.
- 09:08:2026 - 20:13:17: Flora's canopy measurement, and it REFUTED MY OWN WORRY FROM 20:01 THIS EVENING. §1.3 — measured transmittance vs depth into a real crown (user's reference photos, two exposures agreeing within 3 points): 23-24% at the rim, 3-4% halfway in, 0.5-4% in the interior, clean Beer-Lambert decay, extinction k ~= 0.84/m. A real crown is 79-86% LEAF at its core: POROSITY IS A RIM PHENOMENON, NOT A WHOLE-CROWN ONE. So the solid-crown w(h) was already close to right, no coefficient change is worth making, and the one-time physics-correction budget STAYS UNSPENT for the second time this stage. Recorded that both of us were wrong in opposite directions — I feared the model had become too opaque, flora expected foliage on twigs over a hollow interior, which was further off than my version — and that the disagreement was settled by MEASURING BEFORE EITHER BELIEF BECAME A RULE. §5 — the alpha-card exception SURVIVES BUT NOT ON THE REASON IT WAS GRANTED: measured luminance is branch 50 / leaf 135 / sky 235, so branch:leaf ~= 2.54x and the tracery in the reference reads because dark limbs sit against a bright backlit leaf field, which is §1.5 value contrast, NOT transparency. Dark limbs plus bright foliage reproduce the reference; see-through cards do not. Explicitly barred anyone from widening the exception on the transparency theory, since it buys almost none of the look. NEW §5.11 — seasonal foliage palette contract (flora flagged the shape before entries exist, which is the cheap moment): per-species palette INDEXED BY SEASON, mesh stores an index not a colour, palette entry is a RAMP not a value, winter is one has_foliage boolean, conifers season-stable apart from snow. Added the two constraints flora could not see from their side: value ORDER between species must survive every season or the forest stops reading at SILHOUETTE_MIN_PX in exactly one of them; and since leafless deciduous forest collapses w toward trunk diameter, C1/C4/sight wedges VALIDATE AGAINST THE WORST CASE, FULL SUMMER CANOPY — winter may only improve a passing seed, never rescue a failing one, so no seed passes in February and fails in July. Binding sourcing rule added, general to the whole project and not just foliage: NEVER calibrate a palette from photographs (the same tree in two frames gave leaf/dark splits of 76/10 and 53/40 purely on exposure) — reference photos are evidence about STRUCTURE, never about hue.
- 09:08:2026 - 20:41:00: Two blocking rulings for core's §2.8 step 1, plus the doc amendments their measurements earned. §2.8.3 — RULING 1: «of the surface» in I3/I4 means TRUE SURFACE AREA, never plan-view footprint, so **I3 PASSES at 16.5%** (the 6.0% footprint reading is retained as a diagnostic, never a verdict). Decisive argument: a footprint-weighted steepness test is anti-correlated with its own goal at the limit — a 70 deg face carries ~2.9x the surface of its footprint and a 90 deg face carries ZERO, so the score falls to nil exactly as the rock becomes perfectly vertical; a test that reports its own ideal as absence is not a test. The same weighting is independently right for I4 (surface inflates steep bins by 1/cos, so the single uniform 45 deg flank that I4 exists to reject is weighted UP, where footprint would discount it 0.71 and let it hide inside the valley floor) and is demanded by consistency (one phrase, one population, adjacent rows). Binding reading is triangle area on the extracted mesh — finite, needs no clamp, NO NEW CONSTANT; the field-side footprint/cos figure is the reported cross-check, because it diverges at vertical and a clamp would be a constant. I8 keeps plan-view P and A (a slice outline is a 2D object by definition); I9's denominator inherits the surface rule, which is the correct direction. Standing doctrine: print both, label the verdict — core printing both rather than resolving it in their own favour is the behaviour to keep. §2.8.1 — RULING 2: the 0.80/0.81/0.80 lobe baseline is WITHDRAWN and RE-STAMPED at 1.27; it was measured by boundary-cell counting, which §2.8.3 already banned, so it was a reading of a digitiser rather than of the terrain. Perimeter was short by ~26%, not the ~10% my predecessor estimated — mechanism recorded (boundary-cell counting measures Chebyshev length; a diagonal boundary is short by up to 1/sqrt(2) ~= 29%, so 26% sits at the theoretical floor, confirmation rather than coincidence). Consequences: I8's headroom was never 69% but 6%, so its load-bearing clause is the RISE (MASSIF_LOBE_RISE_MIN), not the level; and the banded model REGRESSED lobing 1.27 -> 1.01 rather than improving it 0.80 -> 1.01 — the bands replaced fBm crenulation with a cleaner authored outline, so the I7/I8 question is «does R_k(theta) replace the noise or ride on top of it», and I7/I8 is real work, not a tuning pass. Slope-histogram row flagged for re-stamp with its weighting labelled (MASSIF_SLOPE_BIN_MAX 0.30 was chosen against the 33.2% reading). §2.8.2 — core's four measured bugs written in as rules: CLIFF risers are PLANAR faces (a smoothstepped riser spends its width on sub-cliff slope, so I3 sees no cliff on a mountain that visibly has one — and §4's snap rule needs a real lip, so the hard-splat-edge-at-band-lips promise is now a requirement rather than an assumption); benches are neither dead flat (62% in one bin) nor pinned at MASSIF_BENCH_SLOPE_MAX (75% in another) — a bench is ground, §2.7's «terrain never flattens» is general, benches carry GROUND_MICRO_* and a seeded spread WITHIN the ceiling, which was never a target; and a RAMP band is still a band (falling back to the bare cone left half the massif unbanded). Lesson recorded: constant-gradient failures MOVE rather than disappear, three of the four were fixes for one invariant breaking another, and all four were found by measuring rather than by looking. §7.1 — Ravenscar status table: 5 of 8 applicable invariants pass (I1 -7.1 -> +15.0 deg, I3 0.0 -> 16.5%, I4 33.2 -> 24.2%, I5 0 -> 100% of radials, I6 0.518), I2/I7/I8 remain, I9 not applicable; frames still outrank numbers. §7.0a — SEQUENCING RULING: re-run the barrow couloir search AFTER I7/I8, not now. §2.8.5 said «after the reshape», but its precondition is unmet — at 1.01 the massif has FEWER re-entrants than the 1.27 stamp the search already failed on, and a second failure would wrongly promote the high-shoulder fallback, which inverts story canon. The two red carve tests are §7.0a's durable rule WORKING, not breaking; the barrow stays put with its test red. Zero new constants requested by either ruling — both are measurement definitions, not numbers.
- 09:08:2026 - 20:55:00: Core's step 2 landed (I7 and I8 now PASS, 7 of 8 applicable invariants green, only I2 the summit tor remains) and it CORRECTED ME. §2.8.1 — my mechanism for the lobing regression is WITHDRAWN: I wrote that the band model "replaced the slice outline with the authored R_k(theta), which is cleaner than the noise it displaced", i.e. that authored outlines are inherently smoother than fBm. Wrong. Core found a degenerate geometry construction in their own bearing-field helper — sampling on a circle sized so its circumference spanned `lobes` cells forces radius = lobes*CELL/2pi, which at 3 aretes on a 64 m cell is a 61 m circle INSIDE a 64 m cell, so the noise read it as one smooth patch and radius varied ±4% where MASSIF_RADIAL_LOBE_AMP asks ±18-35%. There was never structural lobing to erase. What stands: the re-stamp, the confirmation that 1.27 was fBm bleed, the 6% headroom, and the sequencing call. What I got wrong is recorded next to what I got right, because the box is quoted approvingly: RULING A NUMBER AND NARRATING ITS CAUSE ARE DIFFERENT ACTS, and a directionally plausible mechanism gets less scrutiny than a surprising one — the §1.3-withdrawal lesson, this time wearing my own face. Slope-histogram row ruled SUPERSEDED rather than re-measured (core's proposal accepted): the 33.2% was footprint-weighted on terrain that no longer exists, so any figure now would be reconstructed, which is the same class of act as the boundary-cell numbers withdrawn above; MASSIF_SLOPE_BIN_MAX 0.30 keeps honest provenance because the DIRECTION of the error is known (surface weighting would have read that flank higher, so the threshold is conservative). §2.8.2 — three new rules from core's construction. (a) The cross-section is a faceted polygon by support function, whose boundary is flat facets meeting at corners — this document's own definition of an arete rather than a proxy. A support function is CONVEX, and a near-regular convex section is capped at n*tan(pi/n)/pi = 1.65 / 1.27 / 1.16 for 3 / 4 / 5 facets, so against MASSIF_LOBE_RATIO 1.35 a convex massif with 4 or 5 aretes CANNOT PASS I8 at any amplitude: couloirs are what make the invariant satisfiable across L0_ARETE_COUNT 3-5 and LR_RIDGE_COUNT 4-7. Added the caveat core could not see from their side — the cap is for a NEAR-REGULAR polygon, and an elongated convex section beats it easily (4:1 rectangle scores 1.99), so a future implementation could pass I8 by stretching the massif; barred, because an elongated L0 is a ridge not a peak and would satisfy the invariant while destroying what it protects (elongation is a landform choice, legitimate for §2.6 border ranges, never a knob for passing a lobe test). (b) I7 and I8 pull opposite ways — measured, deepening couloirs raised I8 and dropped persistent aretes 4 -> 0 because a couloir spread across a facet curves it — resolved STRUCTURALLY: couloirs FADE toward the summit, being flank features that merge into the aretes as they rise (also what erosion does: nothing runs down a crest; and an angularly-constant couloir is ~1 m of arc at summit radius, far under MASSIF_ARETE_TURN_ARC_MAX 15 m, so up there it could only be noise to the detector). (c) NEW GENERAL RULE from a near miss: a structural feature the invariants depend on is NEVER a per-instance coin flip — core's first variant made couloir PRESENCE a seeded per-facet draw and on seed 1 all three missed, producing a bare convex polygon that looked reshaped and satisfied nothing. The seed varies a feature's CHARACTER (depth, asymmetry, bearing, spacing), never its EXISTENCE, because a seeded absence fails silently and plausibly. §2.8.3 — fourth measurement rule: A MARGINAL PASS ON ONE SEED IS NOT COMPLIANCE. I8 passes at 1.36 vs 1.35 with the rise at exactly 0.15 vs 0.15 — zero headroom on the clause identified as load-bearing — so invariants are reported as a min/median/max DISTRIBUTION across seeds, and when the median sits at the bound the SHAPE PARAMETERS move, not the threshold (the accommodation this doc has refused twice). Same failure class as §7.0a: an invariant validated on one seed is a stamp against one terrain state. §2.7 — GROUND_MICRO_* had never been implemented anywhere; core's global attempt was correctly backed out on a measurement (±0.3-0.6 m on the shoreline dropped bank dips under the water surface and they rendered as WaterBed past the §3.3 cap). Produces the missing rule rather than permanent scoping: micro-relief amplitude TAPERS TO ZERO ACROSS THE SHORE BAND via the existing dist_to_water field (no new data, no new constant), which is physically right rather than a workaround — ground beside water is flat BECAUSE water flattens it, so the rule improves the world while fixing the bug. Massif-only scoping recorded as INTERIM: a micro octave above the cliffline and nowhere else makes the cliffline a character seam, the same failure the "general, not forest-specific" ruling exists to prevent, merely relocated from the forest edge to the mountain's hem. §7.1 — status table rebuilt across all three shapes; two cautions attached so green is not read as done (I8's marginal pass pending multi-seed; I3's figure is still the field-side reading, mesh triangle area queued), and restated that seven of eight invariants with a summit that still reads as a dome would mean the invariants are wrong, not the mountain right. §7.0a — the sequencing override vindicated: core's zero-couloir variant would have made the search fail a third time on a massif that LOOKED reshaped, spending the high-shoulder inversion for nothing; precondition now met rather than assumed. Crag-tunnel carve test went GREEN unprompted when couloirs opened the flank its switchback exits — the §7.0a dependency working in both directions. Barrow stays red where it was told to stay. Zero new constants this round either.
- 09:08:2026 - 21:12:00: Three agents unblocked in one pass — core's summit tor, flora's birch, render's acceptance frames. §2.8.3 — RULING: I2's «mean slope» is SURFACE-AREA weighted, so I2 PASSES at 52.9 deg (footprint reads 32.5 and would fail). Core was right to refuse to extend my I3 ruling by analogy — I3/I4 say «of the surface» and I2 says only «mean slope» — and the ambiguity was MINE for stating a convention as a gloss on two rows instead of as a section rule. Restated section-level: every slope, area and coverage statistic in §2.8.3 is surface-area weighted (I2, I3, I4, I9), and I8 is the SOLE plan-view measure because a slice outline is a 2D curve. The decisive argument for I2 is not the analogy but that a plan-weighted I2 would REJECT THE LANDFORM §2.8.4 MANDATES — a tor is flat tops and vertical sides, so plan weighting scores a textbook tor as flat, and core measured the ideal registering as a regression (32.7 -> 32.5 when the tor was added). When a test contradicts a design ruling, the ruling says what the mountain IS and the test only says whether we got there; the test yields. Not vacuous: a dome has slope -> 0 at the apex where the two weightings agree, so it still fails. §2.8.2 — RULING on I8's two clauses fighting: COULOIR DEPTH IS ABSOLUTE (metres), COULOIR ANGULAR WIDTH IS RELATIVE (fixed fraction of its facet). Core mapped the space and found every axis traded level against rise, but all their variants were the same experiment — each varied how fast the couloir fades while leaving depth expressed as a FRACTION OF LOCAL RADIUS, which is self-similar by construction and self-similarity is exactly what the rise clause exists to detect. Hold depth in metres and the rise appears free: R shrinks toward the summit so eps = depth/R(h) grows on its own, i.e. the mountain becomes more articulated near the top not because features grow but BECAUSE THE MOUNTAIN GETS SMALLER AROUND THEM, which is what real massifs do. Angular width stays relative because absolute width would curve the facets and kill I7 again; core's own arc check confirms ~1 m of arc at summit radius, far under MASSIF_ARETE_TURN_ARC_MAX 15 m. Precedent cited so it reads as recognisable rather than novel: MASSIF_ARETE_TURN_ARC_MAX is already absolute on purpose. Clamp against existing MASSIF_RADIAL_LOBE_AMP_MAX, no new constant. Core's alternative (arete count varying with elevation) ruled NOT the first lever — geologically true but it fights I7's persistence check by construction. §2.8.4 — two tor rules from core's bugs: «the tor REPLACES the top» must not be built as «TRUNCATE the top» (capping the cone outside the slabs builds a MESA and cost 3.6 deg of summit slope — the feature meant to fix I2 made I2 worse), and L0_RELIEF IS A CONTRACT — anything stamped on a summit is measured INTO the landmark's relief, never added on top, because CASTLE_SKYLINE_MARGIN, R3, R4, C1 and the whole hierarchy are ratios and margins to the peak, so a summit feature that quietly raises it edits all of them from a zone that does not own them (core's tor drifted 115.0 -> 116.1 by reading one bound of a range as the range; now exactly 115.0). §5 NEW — CROWN ASPECT CEILING invariant, from flora's Rule 28 stop on the River Birch after four failed attempts across two sessions. THE DEFECT WAS IN MY NUMBERS: crown width 5-7 m and CROWN_BASE_FRACTION_MAX 0.45 of a 16-22 m height give a container 1.8:1 before a single cluster, and the generated foliage box measures 2.65:1 — a column, not a mass (oak 1.65 and willow 1.51 read fine, pine 4.88 is correct because a cone is meant to be narrow). Three previous fixes all changed what goes in the box while THE BOX WAS THE DEFECT — the same diagnosis §2.8.1 reached about the mountain, twice in one stage, now recorded as a cross-cutting lesson. Rule: a broadleaf crown's GENERATED foliage bounding box may not exceed CROWN_ASPECT_MAX 1.8 (предложение), measured on built geometry never on the authored container (the container passes and the tree fails — same discipline as the polyline perimeter: measure the artefact, not the intention); cone/spire species exempt by their written brief; 1.8 flagged PROVISIONAL because the band 1.8-2.65 is untested and the frame outranks the number. The structural half: CROWN_BASE_FRACTION stops being a universal cap and becomes a FLOOR plus a derivation, which dissolves flora's per-species-vs-principle question rather than answering it — the 0.35-0.45 band was carrying two unrelated jobs (clear-trunk walkability, which §1.3 measured as visibility-insensitive, and by accident of being a fraction of height, the crown's ASPECT), so MIN 0.35 stays as the walkability floor and each species' crown base is DERIVED as the smallest value above it satisfying the aspect ceiling. Oak and willow unchanged; birch lands at ~0.58-0.62, which is flora's remedy reached by principle instead of by exception, and costs nothing I value (width band untouched so TREE_SPACING_FOREST does not move, accent role strengthened, clear trunk 8.5 -> ~11 m per §5.7's own goal). §5.3 updated. Invariant lives in §5, mechanism in flora's spec — design owns the test, the zone owns the mechanism. NEW §7.1b — the two acceptance vantages, stamped so the test is reproducible: frame 1 (120,300) aimed at (830,200,y=95), ~717 m, backlit low morning sun, proves the silhouette is not a dome; frame 2 (545,165) aimed at (830,200,y=70), ~287 m, front-lit raking evening sun, proves the flank alternates cliff and bench at irregular spacing. My own acceptance text had named two standpoints that test the SAME thing; render challenged it and their far/near split is better, because the two failure modes are invisible to each other's distance. Recorded: why each frame's light differs (frame 1 tests an outline and §1.5 says landmarks read by value against sky; frame 2 tests a surface and a low sun behind camera maximises riser/bench separation — the same light would ruin the other frame); why there is no foot-of-cliff frame (from beneath a riser you see one riser, and I4/I5/I6 are about RHYTHM, which needs several bands stacked); band pairs subtend ~11 px at 717 m and ~27 px at 287 m, surviving to ~960 m before dropping under SILHOUETTE_MIN_PX; frame 2's bearing avoids the castle sector because inside 300 m §6.1.1 lets the castle fill the view and the frame would be testing the castle; Ravenscar must read SOLID not hazy at 287-717 m, well inside LANDMARK_HAZE_ONSET 800 m, and haze on it is a bug per §1.3a; the hard splat edge at band lips is INTENDED and must not be smoothed, with the real check being whether the snap TRACKS THE LIP rather than wandering across a bench on a slope-threshold contour. §7.1 status table rebuilt: 7 of 8 pass, only I8's rise clause fails, three cautions attached (the 0.01 miss and core's correct refusal to close it against one seed; the whole table is still ONE SEED with I1 at 12.7 vs 12 the next-thinnest margin; I2/I3 still print the field-side reading). One new constant requested this round: CROWN_ASPECT_MAX.
- 09:08:2026 - 21:21:00: THE FRAME REFUTED THE SUITE. I opened screenshots/massif/02_massif_verdict_400m_diagnostic.png myself and confirm render's and the lead's verdict independently: a low smooth convex arc with a grey cap and a green shoulder, ZERO crest lines against the three §7.1 requires, ONE material band instead of a rhythm. §7.1 — "7 of 8" WITHDRAWN as a description of the mountain (it stays true as a description of the tests, which is the problem); honest status is «still a dome, better instrumented», and the table now carries a box forbidding its relay as progress without §2.8.7. Recorded that my FIRST hypothesis was a lighting confound — that a backlit frame flattens internal structure into one dark mass — and THE FRAME KILLED IT: there is ample illumination and clear grey-on-green separation, the geometry is simply a smooth hump. Had I ruled from prose instead of opening the image I would have sent back a lighting question and cost another round; the measure-the-artefact rule I applied to perimeter digitisers and foliage bounding boxes applies to me. NEW §2.8.7 — THE SYSTEMATIC DEFECT: all nine invariants measure the OBJECT (contours, slope histograms, aspect turns, radial profiles, perimeter ratios, all computed on the heightfield from above or around it) and NOT ONE is evaluated from a camera at PLAYER_EYE_HEIGHT, while §7.1's acceptance criterion has always been a frame. The suite and the acceptance test were written IN DIFFERENT SPACES, which is how every row can be green while the criterion fails — not a bad threshold anywhere, a missing dimension everywhere. Two new invariants. I10 MASSIF ASPECT: every existing invariant is SCALE-FREE (ratios, angles, distributions, normalised perimeters) and all nine are satisfiable on a pancake — Ravenscar IS a pancake, 115 m of relief over a 180 m base radius, mean envelope slope ~33 deg. Rule: above MASSIF_CLIFFLINE_FRAC the mean envelope slope must reach SLOPE_ROCK_MIN, derived rather than invented — §4 paints rock at >=40 deg and grass below, so a massif under that threshold WILL BE PAINTED A GRASSY HILL BY THE MATERIAL SYSTEM whatever its geometry does, which is exactly the green shoulder with one grey cap in the frame; the shape rule and the splat rule must agree or the mountain loses the argument to the shader. Base radius must come in from 180 m, cascading onto barrow/castle spur/pine strips/ascent length and re-validated per §7.0a. THE LR IS WORSE AND IS THE CHEAP FIX: 280 m over 600-700 m is a ~23 deg envelope, flatter than Ravenscar, and it does not exist yet — building it first would have produced this same session a third time. I11 SILHOUETTE BREAKS: §7.1 has demanded "at least three crest lines readable at 640x360" since it was written and that criterion WAS NEVER IMPLEMENTED, sitting in prose while nine others ran in code; now executable as tangent breaks in the massif's horizon polyline against sky, each >= SILHOUETTE_MIN_PX, from standpoints on a ring at acceptance distance — the only invariant in the suite computed from a camera, and the one that would have failed on day one. Explained why I7 finding four aretes and the eye finding none are BOTH correct: with 3-5 aretes a rib lies near the limb for a minority of bearings, so from most viewpoints the outline is traced by a FACET whose profile is the smooth curve, and ribs therefore read as VALUE STRUCTURE ON THE BODY rather than as silhouette. Suspected second cause flagged for measurement before any tuning: §1.5 puts readable feature size at ~distance/30, so an arete must stand ~13 m proud at 400 m and ~24 m at 717 m, and NOBODY HAS MEASURED THE RAW RADIAL EXCURSION IN METRES — a lobe ratio of 1.36 says nothing about whether the lobes are visible. Owned plainly: I wrote nine invariants, ruled their weightings twice, corrected their baseline and added a marginal-pass rule, and none of it was worth as much as one screenshot; they were a proxy for a judgement and I let the proxy accumulate authority it had not earned, to the point where "7 of 8" was being relayed upward as the state of the world. §1.3a NEW BOX — LOD IS THE PRECONDITION FOR THE ENTIRE LANDMARK DOCTRINE (render's finding): CHUNK_LOAD_RADIUS 2 chunks ~512 m means the world stops existing at half a kilometre, so the 717 m verdict frame came back WITH NO MOUNTAIN IN IT (verified by walking the same bearing in until it appeared). LANDMARK_MAX_DISTANCE 4 km and CAMERA_FAR 8000 are currently fiction; a landmark at 1.4-1.6 km is not hazy but ABSENT; §1.3a's depth-separation doctrine cannot be observed in the engine today; and C1 has never been confirmed by a camera — computed analytically, which is legitimate, but every "the landmark reads from the valley floor" claim is a claim about a frame nobody can take, which is the §2.8.7 defect in a different costume. Explicitly did NOT ask for a bigger CHUNK_LOAD_RADIUS: core measured ~72 ms per chunk and CHUNK_LOAD_BUDGET is 1 because of a user freeze complaint, so a wider ring buys one screenshot and costs seconds of hitching. The frame waits for LOD. §7.1b — three corrections after the shoot, two mine: frame 1 unshootable pending LOD (the 400 m shot is a DIAGNOSTIC and render labelled it so, correctly); ACCEPTANCE VANTAGES ARE DERIVED, NEVER TABLED — I tabled (545,165) by geometry without checking it against the generated forest and a pine stand owns the frame, which is §7.1a's own trap for the third time, so a vantage is now derived as the nearest standpoint on the required bearing that C1 already credits (LANDMARK_VISIBILITY_MIN 0.6 explicitly allows 40% of ground not to see the landmark, so picking blind draws from that 40% two times in five); and the frame-2 hour was wrong because my premise about the sun's azimuth was wrong, so render publishes the DFN_TIME->azimuth table, it is recorded in the doc, and future frame requests pick their hour from it rather than from anyone's mental model — the requirement restated in geometry instead of clock time (sun roughly perpendicular to the view axis and low). Two new constants implied: MASSIF_ASPECT_MIN (derived from SLOPE_ROCK_MIN) and MASSIF_SILHOUETTE_BREAKS_MIN.
- 09:08:2026 - 21:26:00: Flora's self-correction and implementation. §5 — the aspect table I ruled on was inflated ~15% because twelve size variants were pooled into one bounding box, so the VARIANT SPREAD (birches are 16-22 m tall) was measured as if it were one crown's shape; corrected per variant, oak 1.65->1.53, willow 1.51->1.37, birch 2.65->2.30, pine 4.88->4.23. Diagnosis untouched — the birch was still a column, oak and willow still read, and the true band «1.53 reads, 2.30 does not» still brackets the ceiling. «Measure per variant, never pooled» recorded beside «measure the artefact, not the intention»: flora got the second right and the first wrong in the same table, which is how closely related the two failures are. CROWN_ASPECT_MAX landed at 2.0 rather than the 1.8 I wrote, and I ACCEPT 2.0 rather than re-litigate — the ceiling binds no species (every built tree now measures <=1.28), the entire 1.8-2.0 difference lies inside the band my own note already flagged as untested, and reopening a landed constant to move a non-binding guard rail by 0.2 costs more churn than it buys; flora implementing against the landed value rather than my prose is correct under Rule 14 and is the behaviour I want. Tightening trigger recorded so it needs no argument later: if any species lands between 1.8 and 2.0 and reads as a column, the ceiling moves to 1.8 on that evidence alone. NEW GENERAL RULE — A RANGE IS TWO ASSERTIONS, AND A SUITE THAT TESTS ONE BOUND IS GREEN WHILE THE WORLD DRIFTS OUT THE OTHER SIDE: the birch's crown had drifted to 3.6-4.5 m against this document's 5-7 m brief, a third narrower than specified, with a fully green suite, because only the width band's CEILING was ever asserted — the rule was not wrong, it was HALF-IMPLEMENTED, and that was the other half of why the tree read as a column. Fourth appearance of a range-handling failure this stage (core's tor derived its base from _MIN while drawing heights across _MIN.._MAX, plus four earlier instances they report in their own zone), so it is written as a family: every _MIN/_MAX pair in this document is two separate claims about the world. Also accepted: widths calibrated against the BUILT tree rather than the envelope (achieved width is ~0.7-0.9 of nominal because containment holds a card's CORNER inside and the widest ring sits where a card would overshoot the crown top — the same discipline one level down); and the ceiling asserted at NOMINAL size only, deliberately, because §5.8's maturity tiers scale trees x0.4-1.5 and a per-instance width band would be a rule forbidding this document's own tiers (the aspect RATIO is scale-invariant and stays meaningful per instance, the width BAND is not). Implemented result: oak 1.53->1.28, willow 1.37->1.25, birch 2.30->1.02, pine exempt; the birch reads as §5.3's brief for the first time since the species existed. Flora's two self-found bugs both came from my «measure the artefact» instruction and neither would have been found by measuring the spec — vertical card clamps used the card's HALF-HEIGHT when a tilted, rolled card reaches with its CORNER, hanging cards below the crown base and pushing the built box outside its container.
- 09:08:2026 - 21:36:00: Core's 12-seed distribution and the I7 retraction. §2.8.3 — I7 RETRACTED as ever having passed: every "4 persistent aretes" is withdrawn, including the row in my §7.1 table; the probe counted qualifying bearing SAMPLES rather than ridges and anchored persistence on the lowest slice, and corrected it FAILS ON ALL 12 SEEDS (max 2 against a floor of 3). Core validated the detector against a couloir-free faceted polygon before believing the failure. Sharpest lesson of the stage recorded: 42 aretes was absurd on its face and was caught instantly, 4 WAS NOT ABSURD AND SURVIVED THREE ROUNDS OF RULINGS — a wrong number in the plausible range buys itself unlimited time, third instance this session after the C1 self-occlusion figure and my own lobing mechanism, all three directionally reasonable. Then the failure turned out partly MINE: raw detections rise with height (2/3/5/5) because §2.8.2's eps increases with elevation, so THE MECHANISM SATISFYING I8's RISE IS THE MECHANISM PREVENTING I7's PERSISTENCE, and two of I7's four levels sit in the smooth apron below MASSIF_CLIFFLINE_FRAC that §2.8.2 explicitly describes as unbanded. RULING: I7's persistence is measured over four levels spanning the BANDED ZONE only, cliffline to summit, never the apron — not a relaxation invented to green a red test, since eps-increasing-with-elevation predates the conflict and ribs dying into a talus apron is the mirror of the couloir-fade ruling nobody objected to. GUARD MAKING IT LEGITIMATE: conditional on I11 existing, because a test whose sampling elevations I may choose is a test I can always make pass, and I11 is measured from a camera and cannot be gamed by slice elevation — if I11 is not implemented, I7 keeps its original levels and stays red. A proxy may only be loosened once the thing it proxies for is measured directly. §2.8.6 — RULING: Ravenscar's arete count is FOUR and L0_ARETE_COUNT 3-5 is retired as a range. Measured across 12 seeds: 3 fails I8 level on 3 seeds and rise on 2; 4 fails level on 1 and rise on NONE; 5 FAILS ALL TWELVE, which is the convex cap n*tan(pi/n)/pi = 1.16 for a pentagon appearing as a measurement rather than as algebra — first empirical test of that cap, and it means the authored range contained a value that CAN NEVER PASS, so a per-seed draw across it (which my own character-not-existence rule would otherwise invite) would ship guaranteed-failing worlds. Three failed for a structural reason that is not about shape: THE GENERATOR INPUT AND THE INVARIANT FLOOR WERE THE SAME NUMBER (3 corners against I7's >=3), so a single missed detection fails by construction — new general rule, a generator input must never equal the floor of the invariant that checks it, because that is a coincidence rather than a margin and every measurement error lands on the failing side. Sixth instance of the range family (arete_count pinned at L0_ARETE_COUNT_MIN, exactly as the tor derived its base from SUMMIT_TOR_HEIGHT_MIN). §2.8.2 — THE ABSOLUTE SCALE IS BAND HEIGHT, NOT MASSIF RADIUS (core's correction, repairing a hole I put in my own ruling): taking MASSIF_RADIAL_LOBE_AMP off the 180 m base radius gives 32-63 m insets, wider than the upper mountain, so MY CLAMP BOUND AT EVERY HEIGHT AND SILENTLY RESTORED the fraction-of-local-radius behaviour the unit change exists to remove (measured in that state: levels 1.50/1.50/1.60 but rise 0.10 and I7 gone). My clamp was not a safety net, it was a RE-ENTRY POINT FOR THE BUG IT GUARDED, and is withdrawn as written; scale is MASSIF_CLIFF_BAND_MIN..MAX because a couloir is a gully incising the cliff bands — same landform, same scale, same units. A feature's size comes from the feature it cuts, never from the mountain it sits on. Any surviving apex clamp must report whether and where it bound, because that failure is silent (levels keep passing while the rise dies). Added core's elongation sentence under the bar: an elongated support polygon puts its corners on the long axis so arete bearings CLUSTER, and I7's persistence would keep passing while the mountain became a ridge — the bar must be a design rule precisely because the suite is blind to it. §7.1 — single-seed table superseded by the 12-seed distribution; SEED 1 WAS ONE OF OUR BEST WORLDS, not a typical one (8/8 on seed 1 is 5/8 on seed 2), so every number this stage was measured on our luckiest draw and the marginal-pass rule earned itself within the hour. I2/I3/I4/I5 robust on every seed and called genuinely done — noted that they are, not coincidentally, the four describing LOCAL SURFACE CHARACTER rather than GLOBAL FORM: the suite is strong exactly where the frame agreed with it and weak exactly where it did not. I1 fails 2 seeds, I6 fails 1, I8 level 3 and rise 2 at the shipped 3-arete config.
-->

# LANDSCAPE.md — Landscape & World Design Bible

Owner: `design` (Rule 25). This document drives worldgen v2 and the placement
passes. Every rule here is written to be executable by a noise-based generator
or a deterministic placement pass (Q13: procedural base, hand-editing arrives
later via the editor). No rule requires hand sculpting.

Conventions used below:

- **(предложение — утвердить)** — a number invented in this document. It is a
  candidate for NUMBERS.md; the lead adds it after approval. Code must
  reference the future constant name, never the literal.
- **FUTURE** — depends on a system that does not exist yet (roads, quests,
  day cycle, swimming, region-scale biomes). Design intent recorded now so
  worldgen v2 does not paint itself into a corner.
- Existing constants are cited by NUMBERS.md name (`WALK_SPEED`, `CHUNK_SIZE`,
  `POI_TRAVEL_TIME`, ...). Units per Rule 14: meters, seconds, radians.

---

## 1. Composition principles

### 1.1 The one law: пустота — наш враг, but readable emptiness

Density beats area (DECISIONS §1, Q12/Q41). For landscape this means: the
player must **always have a reason to pick a direction**. The tool for that is
not more objects — it is *composition*: landmarks, occlusion, and reveal.
An empty meadow between two visible goals is pacing (a rest beat, per
level-design practice); an empty meadow with nothing on the horizon is a bug.

**Rule C1 — no dead horizon.** From any walkable point, at standing eye height
(`PLAYER_EYE_HEIGHT`), the player must see at least one *attractor*: the
dominant landmark, a secondary landmark, or a local guide (§1.3). On the
region this is the `POI_VISIBLE_COUNT` contract (1–3 simultaneously visible);
on the testbed the same rule holds with the tighter spacing of §1.2.
Verification: a worldgen validation pass samples a coarse grid of standpoints
and raycasts against the **occlusion heightfield** + landmark bounding shapes;
any standpoint with zero visible attractors fails the seed. The occlusion
field is terrain **plus canopy** — terrain-only raycasts pass seeds where a
pine wall buries the L0 (render's stage-3b probes).

**Canopy is a BAND, not a column (stage-4 correction, flora's finding).**
Modelling canopy as solid ground-to-treetop was correct only while crowns
started at 2.7 m. With crown base at 35–45 % of height (§5.7) the canopy is a
**slab** from ≈ 9–13 m to ≈ 24–38 m with open trunk space beneath it, and a
ray from a 1.7 m eye rises with distance: through forest 30 m out the ray sits
at ≈ 6 m and passes cleanly *under* the crowns. The column model blocked that
ray and was **pessimistic** — it was failing sightlines that exist. Rules:

- The occlusion query returns a band `(crown_base, crown_top)` per sample.

**Forest attenuates, it does not switch (stage-4 ruling — the binary model is
retired).** First-hit-opaque models a wall we have deliberately stopped
generating: after the 80 % density cut a ray crossing 150 m of forest at trunk
level expects ≈ 0.92 trunk hits. Vegetation occlusion is therefore
**probabilistic**, and the rule is Beer–Lambert over expected hits:

```
T = exp( − Σ_segments  n_local · w(h) · d_segment )
```

- `n_local` — local stem density (stems/m², from the placement lattice, so the
  maturity mix and the young sub-lattice are accounted for automatically).
- `w(h)` — mean horizontal width the ray meets at its current height: **trunk
  diameter below `crown_base`, crown diameter inside the band, 0 above**.
- The landmark counts as **visible when `T ≥ CANOPY_VISIBILITY_MIN` = 0.25**
  **(предложение — утвердить)**.

Worked from our own numbers (44 stems/ha, 1.4 m trunks, ≈ 13 m crowns):
λ_trunk = 0.0062/m, λ_crown = 0.0572/m, so the 0.25 threshold permits
**≈ 225 m of trunk-level forest or ≈ 24 m of crown-level forest**. That is the
right shape: you can see a landmark down a long colonnade, and you cannot see
it through more than a thin screen of crown. It also **subsumes and retires**
the ad-hoc `CANOPY_TRUNK_PATH_MAX` = 250 m from the previous revision — that
guess and this derivation agree to within 10 %, which is why one mechanism now
replaces both rules.

- **Same transmittance governs C4:** an occluder the ray passes with
  `T ≥ CANOPY_VISIBILITY_MIN` is not counted as an occluder for the clearance
  test either. One notion of "blocks", used consistently.
- **This is a physics correction, not an accommodation.** If a seed fails C1
  after attenuation, the floor does **not** move: we change the world — thin,
  shorten, or relocate the foothill strips. Attenuation is allowed once, as
  the fix for a model that was wrong; it is not the first of a series.
- **That budget was NOT spent.** The C1 emergency this rule was written under
  turned out to be a validation bug (see the withdrawn finding in §1.3):
  corrected, seed 1 with the full §5.7 canopy measures **0.751** against a
  0.60 floor, so attenuation was never load-bearing for the floor. It is
  implemented because it is the better model — the maturity mix falls out of
  `n_local` for free, and one consistent notion of "blocks" across C1 and C4
  is worth having on its own merits. **The contingency was never called; a
  future reader should not think the budget is gone.**

**AND THE CANOPY REALLY IS NEARLY OPAQUE — measured, and it refutes both
sides of the argument that was about to be had (flora, stage-4, from the
user's own reference photographs).** When foliage became alpha cards (§5) I
wrote down the worry that C1 would end up "measuring a wall that no longer
exists", and asked for a measured effective `w`. The answer is the opposite of
what either of us expected, so it is recorded here rather than left in a
thread:

| Depth into the crown | Transmittance |
|---|---|
| outer rim | 23–24 % |
| ≈ ¼ in | 10–14 % |
| ≈ ½ in | 3–4 % |
| interior | 0.5–4 % |

Clean Beer–Lambert decay, two exposures of the same tree agreeing within three
points, fitted extinction **k ≈ 0.84 per metre** (half-depth 0.83 m, T < 3 %
beyond ≈ 3 m of penetration). **A real crown is 79–86 % leaf at its core;
porosity is a RIM phenomenon, not a whole-crown one.** So the solid-crown `w`
is close to correct, no coefficient change is worth making yet, and **the
one-time physics-correction budget above stays unspent.**

**Both of us were wrong, in opposite directions, and that is the part worth
keeping.** I feared the model was now too opaque; flora expected foliage on
terminal twigs over a hollow interior, which is further from the truth than my
version was. Neither reading survived contact with the pixels. The lesson is
the §1.3-withdrawal lesson again in a cheaper form: **the disagreement was
settled by measuring, before either belief was written into a rule.** That is
the second finding this stage that arrived as a refusal to hand over a number
that would have moved a model, and it is the behaviour to keep.

**Measured facts to build against (core, stage-4, post-correction), so nobody
re-derives them:** crown base 35 % vs 40 % gives **identical C1 to three
decimals**, because the ray almost never threads that 9.8–11.2 m window at the
crossing points. **Do not spend tuning effort on the crown-base fraction** —
it is a walkability and feel parameter (§5.7), not a visibility one. Likewise
all three flank parameterisations (fraction-of-peak, fixed metres,
sqrt-scaled) give identical C1 to three decimals; core kept absolute metres
anyway, because coupling an occluder's size to the thing it occludes is a bad
idea even when it is not the bug.

This is computable, not editorial.

**Rule C2 — never show everything.** The complement of C1. Breath of the
Wild does this with the "triangle rule": convex terrain masses (hills, crags)
occlude what is behind them, so content is revealed a couple of items at a
time as the player moves. Our gentle-hills base already gives convex rolls;
worldgen v2 must *keep* macro convexity between POIs — do not flatten the
land between two POIs into a plane where both plus three more are visible.

**Scope — corrected 09:08:2026 (design error, my import).** The absolute
bound `POI_VISIBLE_COUNT` = 1–3 is **region-scale only**: NUMBERS.md carries
"—" in its testbed column, and Q46 forbids applying one density contract to
the other. My original C2 text cited it as a general bound; that was an
overreach and is withdrawn. Two reasons it cannot govern the testbed:

1. **It is unsatisfiable alongside constants we already approved.** C1
   requires the L0 visible from ≥ `LANDMARK_VISIBILITY_MIN` (0.6) of open
   ground, so the L0 occupies one "visible attractor" slot almost everywhere
   by construction; C3 packs POIs at 180–270 m across a 1024 m testbed —
   3× the region's spacing. Holding ≤ 3 total would demand occlusion heavy
   enough to push C1 back under its own floor. The two rules pull in opposite
   directions at testbed density, and C1 wins: a valley whose landmark you
   cannot see is the worse failure.
2. **It is not caused by any placement.** Measured 5 attractors from the west
   meadows both with and without the castle — this is structural to the
   testbed's compactness, not a defect a placement pass introduced.

**Rule C2-testbed — no coequal crowd.** What actually overwhelms a player is
several attractors of *comparable* apparent size competing as equivalent
choices; a legible hierarchy of different scales reads as one composition with
depth (this is why the stage-3b tour frames read cleanly at 5 visible). So on
the testbed:

- At most `POI_COEQUAL_VISIBLE_MAX` = **3** attractors of comparable apparent
  size — within `COEQUAL_ANGLE_RATIO` = 2.0 of each other in subtended
  height — may be visible from any standpoint **(предложение — утвердить)**.
  **Tightens to 2 when the crowd is large:** if every member subtends
  ≥ `COEQUAL_LARGE_PX` = 24 px (3× the §1.5 readability threshold), only 2
  may compete **(предложение — утвердить)**. Three marks at the readability
  floor are a vista; three masses filling the view are a menu.
- **Apparent size means subtended height = object height / distance**, never
  the elevation angle of the object's top. The angle measure conflates size
  with ground elevation (a hut on a high shoulder scores as a landmark) and
  produced a phantom crowd in the first seed-1 measurement. Same measure
  governs C4 and §6.1.1 R4 — one definition across all three rules.
- **Only attractors that clear the §1.5 readability threshold compete**
  (`SILHOUETTE_MIN_PX` = 8). What you cannot resolve as a shape cannot
  compete for your attention; sub-threshold objects are texture, not choices.
- **Body-backed attractors are exempt per standpoint:** an attractor inside
  the L0's angular footprint and nearer than the peak does not count, because
  by R1 it reads against the crag's body and cannot claim the skyline. This
  is §6.1.1's siting mechanism applied at view time. The raw unexempted count
  is reported alongside and must stay visible in validation output — the
  exemption is an interpretation, and interpretations get audited.
- The **L0 is exempt** from the count: C1 mandates its ubiquity, so counting
  it against a visibility cap is self-contradictory.
- Composite POIs (hamlet, castle+barrow) count **once**, per §6.1.2.
- The real anti-overwhelm guarantee remains §1.4's occlude-and-reveal rule
  (each POI 30–80 % hidden from its approaches), which is already validated
  and unaffected.

Region scale keeps the absolute `POI_VISIBLE_COUNT` bound unchanged.

### 1.2 Spacing derived from our metrics

All spacing follows from `POI_TRAVEL_TIME` × `WALK_SPEED` (3.0 m/s):

| Context | POI_TRAVEL_TIME | Implied POI spacing (nearest neighbor) |
|---|---|---|
| Testbed | 60–90 s | **180–270 m** |
| Region | 180–300 s | **540–900 m** |

These are *derived*, not new constants — the doc uses `POI_SPACING` as
shorthand for `POI_TRAVEL_TIME * WALK_SPEED`. The two density contracts are
different on purpose (Q46); never tune one to match the other.

**Rule C3 — the POI chain.** Every POI must have at least one neighbor POI
within the spacing band above (graph built by nearest-neighbor links). A POI
whose nearest neighbor is beyond the band is isolated → the generator must
either move it or insert a minor POI (guide-scale, §1.3) between. This makes
the "walk 60–90 s, find something" promise checkable by a validation pass.

### 1.3 Landmark hierarchy (weenies, three tiers)

Disney's "weenie" principle — a tall, high-contrast landmark with long
sightlines pulls people toward it — is the backbone of Skyrim's and BotW's
navigation. Adapted to our scales:

| Tier | Name | Count | Visible from | Examples |
|---|---|---|---|---|
| L0 | **Dominant landmark** | exactly 1 per valley (testbed = 1 valley; region: 1 per ~2–3 km valley cell, FUTURE) | ≥ 60 % of open walkable ground **(предложение — утвердить: `LANDMARK_VISIBILITY_MIN` = 0.6)** | rocky crag + tower ruin, lone mountain, great tree |
| L1 | **Secondary landmarks** = POIs | testbed 6–9 (see §7); spacing per §1.2 | 150–400 m depending on silhouette | hamlet, shrine spire, dungeon entrance, lake |
| L2 | **Local guides** | continuous fabric, every 40–80 m of travel **(предложение — утвердить: `GUIDE_INTERVAL` = 40–80 m)** | 50–150 m | rock outcrop, tree cluster edge, ford, flower patch, lone birch |

- L0 orients the whole valley ("the crag is north"). It must sit high and
  break the skyline (§1.5).
- L1 are the destinations; the POI chain (C3) runs through them.
- L2 exist so the 60–90 s walk between L1s is never featureless; they are
  produced by the meso layer (§2) and are cheap.

**Rule C4 — hierarchy contrast.** BotW's scale lesson: the three tiers must be
*unambiguous* at a glance. Enforce by silhouette height: L0 ≥ 25 m above local
terrain; L1 = 5–15 m; L2 ≤ 5 m **(предложение — утвердить, encoded in the
feature stamps of worldgen v2)**. Nothing that is not the L0 may exceed L0's
apparent height from the main travel corridors — *including canopy*.

Enforcement (added after render's stage-3b probes showed 15 m foothill pines
out-angling the 52 m crag from every western/southern ground vantage):

- **Clearance factor:** from every validation standpoint that C1 credits with
  seeing the L0, the L0's subtended angle must exceed every intervening
  occluder (terrain + canopy) by ≥ 20 %
  **(предложение — утвердить: `LANDMARK_CLEARANCE_FACTOR` = 1.2)**.
- **L0 sight wedges:** P5 precomputes 2D wedges from each L1/POI standpoint to
  the L0 footprint edges. Inside a wedge, any candidate tree whose canopy top
  would subtend ≥ `L0 angle / LANDMARK_CLEARANCE_FACTOR` from that wedge's
  standpoint is rejected (cheap: only trees inside wedges are tested,
  deterministic). Terrain-side tuning knobs if wedge filtering thins a forest
  too much: widen the L0 stamp's treeless rockline band, or reshape the
  landmark-facing forest — core's choice per seed, the invariant is the
  clearance factor. One knob is a genuine dead end: the treeline is useless
  here (foothill terrain sits below any sane treeline).

  > ### ⚠ WITHDRAWN — "raising the peak lowers clearance"
  >
  > **This finding was WRONG and is withdrawn entirely** (09:08:2026, core).
  > It was never a property of the world: the C1 raycast was **counting the
  > crag itself as an occluder of the crag**. The aim point is peak + 8 m
  > while `LANDMARK_CLEARANCE_FACTOR` (1.2) multiplies against terrain that is
  > essentially at peak height, so near-summit ground "out-angles" the summit
  > as soon as `0.2 × (peak − eye) > 8 m` — above a ~60 m peak the test
  > returned 0.000 for *every* standpoint regardless of the world, and below
  > it the measure was already dragged down.
  >
  > **Corrected (landmark excluded from its own occlusion):** clearance
  > **RISES** with peak height, which was the intuitive relationship all
  > along — peak 52 → C1 0.751, 70 → 0.783, 90 → 0.849, 115 → 0.865,
  > 150 → 0.895, 200 → 0.915.
  >
  > **Why it survived, recorded so the next one does not:** it pointed the
  > direction we half-expected, and **nobody asked why a landmark would become
  > less visible for being taller.** A finding that is directionally plausible
  > gets less scrutiny than a surprising one, which is exactly backwards. The
  > review chain is what caught it — flora challenged the boundary and read
  > the code, core re-measured and self-reported. Flora's specific hypothesis
  > (`ridge_amp_frac`) was itself refuted; the challenge was still what
  > produced the fix.
  >
  > **Consequences:** `L0_RELIEF` 110–120 m does not cost C1, it **improves**
  > it (≈ 0.865). The taller canopy never broke C1 either — seed 1 with the
  > §5.7 heights measures **0.751** against a 0.60 floor. Every C1 number
  > reported before this correction is contaminated; do not cite them.

  The effective lever on landmark visibility remains forest *shape*: a closed
  canopy annulus around an L0 can never pass canopy-C1 from valley ground —
  landmark-skirting forest must be broken into radial/ridge strips with gaps
  (see §7.1). That finding was measured independently and stands.

### 1.3a World scale, zones, and the fourth landmark tier (stage-4 ruling)

The world grows to 2×2 km now, 10×10 km once LOD exists. The valley becomes a
corner of it.

**Zones, not epochs — the two density contracts coexist SPATIALLY.** Q46 says
never cross-apply testbed and region numbers; it does not say the world may
only have one of them. Ruling: the testbed contract is a *contract*, not a
size. The original 1×1 km valley keeps it; everything beyond runs the region
contract. Neither is corrected toward the other.

| Zone | Extent | POI spacing (from `POI_TRAVEL_TIME`) |
|---|---|---|
| **Home valley** | the original 1×1 km corner | 180–270 m (testbed) |
| **Open region** | the rest of the 2×2 km | 540–900 m (region) |
| **Transition band** | `ZONE_TRANSITION_WIDTH` = 300 m ring around the valley **(предложение — утвердить)** | interpolate between the two |

The transition is a designed feeling, not a seam: leaving home should *feel*
like the world opening up. A hard density cliff would read as a bug; a
gradient reads as journey.

**Landmark tiers gain a top level.** The map now needs a landmark above the
valley's:

| Tier | Name | Count | Domain |
|---|---|---|---|
| **LR** | **Regional landmark** — the temple mountain (§2.5) | exactly 1 per world | the whole map |
| L0 | Valley-dominant — Ravenscar | 1 per valley | its valley |
| L1 / L2 | as before (§1.3) | | |

**Scale-aware visibility — the fix that keeps C1 meaningful at 2 km.**
`LANDMARK_VISIBILITY_MIN` (0.6) is measured over the landmark's **own
domain**, never the whole map: the valley L0 over the valley, the LR over the
world. This was implicit while domain == map; at 2 km it must be explicit, or
Ravenscar fails a test it was never meant to take.

**C1/C2/C3 at the new scale:** C1 holds unchanged as a floor (the LR satisfies
it across most of the region by design — that is intended, not a loophole).
C2-testbed is already scale-free (it compares angular ratios, not counts of
metres) — keep as written. C3 uses the per-zone spacing above. Border
mountains (§2.6) **never count as attractors** in any of these tests.

**The two big landmarks coexist by DEPTH, not by size.** This is the
crag-vs-castle problem again, but the map-scale answer is different: do *not*
require the near landmark to out-subtend the far one — at 2 km their
subtended heights are comparable and forcing a margin would mean deforming
one of them. Instead they separate by **atmospheric depth**: the LR always
renders beyond the haze onset from valley standpoints, the valley L0 always
inside it. Hazy-and-huge versus solid-and-detailed reads instantly as "far
goal" versus "here"; it is how Skyrim keeps the Throat of the World from
eating every local landmark. Contract for render: `LANDMARK_HAZE_ONSET` =
800 m **(предложение — утвердить)**; the LR is never sited closer than that
to the valley's main corridors, and the valley L0 never further.

**The LR is never fully occluded** from the valley's main corridors — it is
the far goal, and a goal you cannot see is not a goal. Same wedge machinery
as §1.3, applied at map scale.

> ### ⚠ EVERY RULE IN §1.3 AND §1.3a IS CURRENTLY UNSHOOTABLE — LOD IS THE
> ### PRECONDITION FOR THE ENTIRE LANDMARK DOCTRINE (render's finding, stage-4)
>
> `CHUNK_LOAD_RADIUS` is 2 chunks ≈ **512 m**, so **the world stops existing
> at about half a kilometre.** Render's attempt to shoot the §7.1b verdict
> frame at 717 m produced a picture with **no mountain in it at all** — the
> chunks were not resident — and they verified it by walking the same bearing
> in until the massif appeared. Consequences, ruled:
>
> - **`LANDMARK_MAX_DISTANCE` = 4 km and `CAMERA_FAR` = 8000 m are currently
>   fiction.** A landmark sited at 1.4–1.6 km is not hazy, it is *absent*.
>   §1.3a's whole depth-separation doctrine — hazy-and-huge far goal versus
>   solid-and-detailed near landmark — **cannot be observed in the engine
>   today**, and the LR that doctrine exists to place would be invisible from
>   the valley it is meant to pull the player toward.
> - **C1 has never been confirmed by a camera.** It is computed analytically
>   on the heightfield, which is legitimate and is not in question — but every
>   "the landmark reads from the valley floor" claim in this document is a
>   claim about a frame nobody can currently take. That is the §2.8.7 defect
>   in a different costume: **the rule and its verification live in different
>   spaces.**
> - **So LOD is not a performance nicety, it is the precondition for the
>   landmark doctrine**, and it should be scheduled as such rather than as
>   optimisation. Stated here, where the C1 rules live, so that whoever plans
>   that work finds the reason next to the rules it unblocks.
> - **Raising `CHUNK_LOAD_RADIUS` is NOT the answer and design does not ask
>   for it.** Core measured ≈ 72 ms per chunk and `CHUNK_LOAD_BUDGET` is
>   already 1 per update because of a user freeze complaint; a wider ring buys
>   one screenshot and costs seconds of hitching in play. The frame waits.

**Maximum landmark siting distance — the rule that bounds the far plane
(render's blocker, ruled).** `LANDMARK_MAX_DISTANCE` = **4 km**
**(предложение — утвердить)**. No navigational landmark is ever sited further
from reachable ground than this, at any world size. Consequences, decided once
so render and core need not revisit:

- The 2×2 km world's temple (1.4–1.6 km) sits well inside it; `CAMERA_FAR`
  must rise from 1000 m to **≥ 4000 m** or the LR is not hazy, it is *clipped*.
- At 10×10 km, "one LR visible from everywhere" does **not** scale — 14 km of
  diagonal is scenery, not navigation. The world becomes multi-region:
  **one LR per ≈ 4×4 km region cell**, each dominating its own domain
  (§1.3a's own-domain visibility rule already carries this).
- **Beyond 4 km is BACKDROP, not geometry.** Distant ranges may be drawn by
  whatever cheap mechanism render prefers (impostor layer, sky-dome
  silhouette) and need not be depth-correct. One design constraint: a backdrop
  must be *consistent* with the real terrain behind it — the ridge you see at
  6 km must be the ridge you walk into at 3 km. A backdrop that lies is worse
  than no backdrop.

This bounds depth precision at ~4 km rather than 8+, which is the difference
between a far-plane change and a depth-buffer restructure.

### 1.4 Draw-the-player rules

- **Occlude-and-reveal:** an L1 should be *partially* hidden from at least one
  main approach (behind a hill shoulder or forest edge) so rounding the bend
  produces a discovery. Implementable: when placing an L1, prefer candidate
  positions where visibility from the two nearest POIs is between 30 % and
  80 % of the approach path (raycast sampling), not 100 %.
- **Curved travel:** never let the shortest walkable line between two chained
  POIs be a straight unobstructed rule across flat ground; macro pass keeps at
  least one convex mass or water bend adjacent to each POI-graph edge so real
  paths curve (BotW "orbiting" behavior).
- **Reward the dead end:** any terrain pocket the generator creates (bowl,
  box-canyon end, lake far shore) must receive an L2-or-better reward in the
  placement pass, or be sealed off. A pocket is detectable: walkable region
  whose exits ≤ 1.
- **FUTURE (roads/quests):** roads reinforce, never replace, sightline
  guidance; quest markers do not exist — the landscape *is* the quest marker.

### 1.5 Readability under the Daggerfall look (low-res, first person)

Internal resolution is `INTERNAL_RES` = 640×360 working default, with 320×180
still on the table (sync №2). With `CAMERA_FOV_Y` = 1.309 rad (16:9 →
horizontal ≈ 1.87 rad):

- Angular resolution: ≈ **275 px/rad vertical, ≈ 340 px/rad horizontal**
  at 640×360; exactly half at 320×180.
- A silhouette needs ≥ ~8 px to read as a shape. Therefore:
  **min readable feature size ≈ distance / 30** at 640×360
  (≈ distance / 15 at 320×180) **(предложение — утвердить:
  `SILHOUETTE_MIN_PX` = 8; design math, not a runtime constant)**.
  - At 250 m (mid POI spacing): features ≥ 8 m read. A house (5–8 m) is
    marginal → hamlets read as *clusters* + smoke (FUTURE), shrine spires
    (10–14 m) read individually.
  - At 550 m (across the testbed): only ≥ 18 m features read → the L0 must be
    a terrain-scale mass (crag), with its man-made topper (tower) becoming
    legible on approach.
- **Skyline rule:** silhouettes read best against sky. L0 and L1 verticals
  (spire, tower, lone pine) are placed on convex ground so that from the main
  corridors their top third breaks the horizon line. Implementable: candidate
  scoring by "sky behind top of bounding box" raycast.
- **No thin features at distance:** sub-pixel elements (masts, thin branches,
  fences) shimmer at low res. Distant-readable assets use thick silhouettes
  (§5, §6); nothing structural thinner than ~0.5 m matters beyond 100 m.
- **Value contrast over hue:** with the limited palette, tiers separate by
  *value* (dark crag vs light sky, pale birch vs dark pines). Every landmark
  brief in §5/§6 states its value contrast against its usual backdrop.

---

## 2. Detail layers and worldgen pass order

Three layers, placed in strict pass order. Determinism per Rule 13.1: every
pass draws only from `WorldGenRng` streams keyed by (seed, cell/chunk, pass).
For scatter that must agree across chunk borders, cluster/patch centers live on
world-space jittered lattices keyed by lattice-cell coords (same trick as the
value-noise lattice) — a chunk computes any center whose radius touches it,
so neighbors agree without communication.

| # | Pass | Layer | Produces |
|---|---|---|---|
| P1 | Macro heightfield | macro | base fBm + feature stamps (crag, valley flattening, redistribution) |
| P2 | Hydrology | macro | river path + carve, lake basin, water surface data, shore mask |
| P3 | Surface classification | macro | biome/splat inputs per sample: slope, height, dist-to-water |
| P4 | Sites & structures | meso | flattened building pads, POI placement, POI-chain validation (C1–C4) |
| P5 | Meso scatter | meso | forest masses, tree clusters, clearings, rock outcrops |
| P6 | Micro scatter | micro | grass/flower/stone parameters (density maps; instances mostly render-side) |

Sites (P4) run **before** forests (P5) so settlements reserve their clearings;
forests run before micro so grass respects canopy. This mirrors Horizon Zero
Dawn's layered placement: coarse density/exclusion maps first, fine scatter
sampled against them.

### 2.1 Macro (mountains, ridgelines, water bodies, forest masses)

- **When:** P1–P2 (+ forest *mass outlines* decided in P5 from a coarse
  moisture-like noise field, but their masks are macro objects).
- **What:** current gentle-hills fBm (octaves 512/24, 128/6, 32/1.5 — already
  flagged for NUMBERS.md at sync №2) plus, in v2: one ridged-noise crag stamp
  per valley (L0), valley-floor redistribution (`pow`-curve toward flats, per
  procgen practice), river/lake carving.
- **Landform anisotropy (user feedback, feature_requests.md Запрос 1):**
  mid-scale hills must read as elongated, direction-coherent landforms —
  ridgelets with a legible long axis — never isotropic round bumps
  («холмики-сиськи» are explicitly rejected by the user). Implementable
  without hand sculpting: stretch **the mid-frequency octave only**
  (currently the 128 m / 6 m layer — it is what makes round bumps at hill
  scale) by `HILL_ANISOTROPY` (**approved in NUMBERS.md: 2.5**, stage-3
  close) along a per-valley axis field; the macro-roll and fine-texture octaves
  stay isotropic, and the ridged transform on the L0 stamp already covers
  crag flanks. **Technique decided (plan of record, core + design sync):
  anisotropic input-stretch, not domain-warp.** Input-stretch lengthens
  features *along* the axis (the requested elongation itself) while the
  cross-axis wavelength — the rhythm corridors and the C1 standpoint grid
  actually feel when crossing ridgelets — stays pinned at the current
  128 m. Domain-warp is the opposite trade (preserves average wavelength
  but wiggles crests and dilutes the shared-axis read) — rejected.
  Ping-first threshold: if axis-field drift locally compresses the
  cross-axis rhythm below ~100 m, core pings design before it lands. The axis field is a slowly-varying seeded angle: ridgelets
  share a long axis *locally* while the axis drifts across the map — a
  single global direction would read as corduroy. Recorded caution
  (core, stage-3b sync): warping the hill octave shifts drainage
  micro-shape — the seed-1 river trace WILL move; this is safe *only*
  because of the derived-only rule (§7.1a), which is exactly the case that
  rule exists for. Gates cleared at stage-3 close (sync №3):
  `HILL_ANISOTROPY` = 2.5 landed in NUMBERS.md and the P1 retune is
  scheduled — canopy-aware C1 re-validation is automatic in the suite,
  and a compliance pass over the retune tour frames follows. Acceptance: tour
  frames of open meadow show hills with an obvious long axis roughly
  agreeing with their neighbors.
- **Quantization warning (core contract):** all chunks share one quantization
  range (offset 0, scale MAX/65535). Raising the L0 above the current 31.5 m
  ceiling requires raising the shared range — **(предложение — утвердить:
  `WORLDGEN_MAX_HEIGHT` = 64 m)**. Height resolution stays ~1 mm; no contract
  change beyond the constant.
- **Density guidance:** exactly 1 L0 per valley; forest masses cover 25–40 %
  of walkable land **(предложение — утвердить: `FOREST_COVERAGE` = 0.25–0.40,
  testbed target 0.30)**; open meadow the rest minus water/rock.
- **Must never:** place macro mass so that it hides the L0 from more than
  40 % of open ground (violates C1/`LANDMARK_VISIBILITY_MIN`); create local
  minima with no hydrology resolution (§3); exceed walkable slope on all
  approaches to any POI (every POI keeps ≥ 1 approach corridor under 25°
  average slope — see critical-path rule §2.4).

### 2.2 Meso (hills, clearings, river bends, outcrops, tree clusters)

- **When:** P4–P5.
- **What & density (all предложение — утвердить):**
  - Forest interior clearings: one per 150–250 m of forest extent
    (`CLEARING_INTERVAL`), radius 15–30 m (`CLEARING_RADIUS`); clearings host
    L2 rewards (flower patch, stone, FUTURE camp).
  - Rock outcrops in open land: on a jittered lattice of cell 120 m
    (`OUTCROP_CELL` = 120 m, ~1 per 14 000 m² with 30 % skip chance); 2–6
    boulders each, 1–3 m; prefer convex ground and slope 15–35°.
  - Tree clusters outside forest masses: 3–7 trees, on a jittered lattice of
    cell 90 m in meadows (`MEADOW_CLUSTER_CELL` = 90 m, 40 % skip).
  - River bends: hydrology path smoothing keeps sinuosity ≥ 1.15 (path length
    / straight distance) so banks create pockets and reveal beats.
- **Must never:** block the POI-chain corridors (§2.4); violate the L0 sight
  wedges / clearance factor of C4 (checked by the canopy-aware raycast
  validation of C1 — terrain-only checks are insufficient, see C4); float
  above or intersect water.

### 2.3 Micro (grass, flowers, bushes, stones, sand patches)

- **When:** P6 computes deterministic *parameters* (density maps, patch
  centers); actual instancing is render-side against those maps (Horizon
  model), so micro never enters the .dfw entity list and never costs
  streaming-path ECS churn (Rule 11 friendly).
- **What & density (all предложение — утвердить):**
  - Grass cards: only within `GRASS_VIEW_DISTANCE` = 50 m of camera,
    0.5–1.5 cards/m² on grass-splat ground (`GRASS_DENSITY`); at low res more
    is visual noise.
  - Flower patches: centers on 60 m jittered lattice in open grass, 50 % skip;
    patch radius 3–8 m; 1–3 blossoms/m² inside (`FLOWER_PATCH_*`). One accent
    hue per patch (palette discipline).
  - Loose stones: 0.005–0.02 /m² on grass and dirt (`STONE_DENSITY`), size
    0.2–0.6 m.
  - Bushes: forest edges (within 10 m outside a forest mask edge) and clearing
    rims, 0.01–0.03 /m² there (`BUSH_EDGE_DENSITY`).
  - Sand patches: from the shore mask only (§3.3) — never freestanding inland
    at this stage.
- **Must never:** affect collision or the critical path (micro is
  walk-through by contract); hide interactables (Q11 highlight must stay
  visible over grass — cap grass height at 0.4 m, `GRASS_HEIGHT_MAX`);
  exceed the render micro budget (render zone owns the actual instance caps —
  these densities are the *design* ceiling).

### 2.4 Critical-path protection (applies to every layer)

The POI chain of C3 defines corridors: straight-ish bands 10 m wide
**(предложение — утвердить: `CORRIDOR_WIDTH` = 10 m)** between chained POIs,
refined by a cheap downhill-biased path trace. Inside a corridor:

- slope along the walking direction ≤ 25° average (under `PLAYER_MAX_SLOPE`
  with margin), no step > `PLAYER_STEP_HEIGHT`;
- no structures, no forest-density trees (isolated trees allowed if spacing
  ≥ 12 m), no boulders > 1 m;
- rivers crossed only at fords (§3.1).

Corridors are a *mask* consumed by P4–P6, not visible content. FUTURE: roads
will be built along a subset of corridors.

---

### 2.5 The regional landmark massif — the temple mountain (LR)

The far goal: high, cliffy, uneven, with a walkable ascent and a temple on
top. Ravenscar keeps the valley and the story; this keeps the horizon.

**Scale (предложение — утвердить).** `LR_RELIEF` = 280 m above the
surrounding plain (proposed band 250–350). `LR_BASE_RADIUS` = 600–700 m, i.e.
mean flank slope ≈ 25° — that ratio is what makes it read as a **massif**
rather than a spike; a cone steep enough to be dramatic at the summit must be
broad enough at the foot to look like it belongs to the ground. Sited ≥
`LANDMARK_HAZE_ONSET` (800 m) from the valley, in practice the far corner
(≈ 1.4–1.6 km out). Readability (§1.5): at 1500 m anything ≥ 50 m reads, so
the massif is unmistakable while its temple (15–20 m) only resolves inside
≈ 600 m — the temple is the reward for approaching, exactly like the castle.
This requires `WORLDGEN_MAX_HEIGHT` 64 → **400 m**.

**"Cliffy and uneven" as generator rules — the user explicitly rejects smooth
domes, so these are invariants, not suggestions:**

1. **Ridged noise, not fBm**, for the massif field: `r = 1 − |2n − 1|`
   summed over octaves. fBm makes domes; ridged noise makes spines.
2. **Radial buttress ridges:** `LR_RIDGE_COUNT` = 4–7 ridges descending from
   the summit with couloirs between, as an angular modulation
   `h *= 1 + A·cos(k·θ + φ(θ))` with `φ` from noise so the ridges are
   **irregular, never symmetric** (a symmetric star reads as artificial).
3. **Cliff bands:** above `LR_CLIFFLINE` (⅓ height), quantize elevation into
   bands of `LR_CLIFF_BAND` = 8–15 m spaced 30–60 m vertically, blended just
   enough to avoid stair-stepping artefacts. This is the "cliffy" read and it
   feeds §4's splat directly (rock above 40°).
4. **Asymmetry:** one flank biased steep (a scarp face), the opposite gentler
   — the gentle side carries the ascent. Real mountains are not radially
   uniform and neither is this one.
5. **THE ANTI-DOME INVARIANT — SUPERSEDED BY §2.8, AND THAT SECTION IS NOW
   THE CONTRACT.** The rule as first written here (lobed ⅔ slice at
   `LR_LOBE_RATIO` ≥ 1.35 plus ≥ 60 % of the upper surface above 40°) was
   scoped to the LR and never evaluated on anything. The user rejected the
   mountain a third time while looking at **Ravenscar**, which this section
   does not govern. Both halves of that failure — the scoping and the
   insufficiency of a single-slice plan-view test — are worked through in
   **§2.8**, which replaces this item and applies to *every* massif including
   this one. `LR_LOBE_RATIO` is renamed `MASSIF_LOBE_RATIO` there: the
   constant's **name was the bug**.

**The ascent is mandatory and validated.** A continuous walkable route from
the foot to the summit must exist: average slope ≤ 25°, nowhere exceeding
`PLAYER_MAX_SLOPE`, no step > `PLAYER_STEP_HEIGHT`. Same class of invariant
as the castle ramp (§6.1.2) — a summit temple you cannot reach on foot is a
failed placement, not a later problem. Derived from the generated massif,
never tabled.

**"7000 steps" — a staged climb, not a switchback (user requirement).** The
ascent is a *sequence*, not a ramp: `LR_ASCENT_LENGTH` = 1200–1800 m of path
(4–6× the direct horizontal distance, so the route wraps the massif rather
than attacking it) with `LR_ASCENT_LANDINGS` = **7** staged rests — a shrine,
a vista, a wind-scoured shoulder — each a place to stop and look back at how
far the valley has fallen away **(предложение — утвердить; pinned from the
former 5–7 band by story, one landing per station of the naming rite)**.

**Seven verifies rhythmically, which is why it is pinned rather than merely
accepted.** Over the 1200–1800 m path, seven landings give segments of
171–257 m, i.e. **57–86 s of walking at `WALK_SPEED`** — inside the testbed's
`POI_TRAVEL_TIME` band (60–90 s) across the whole range. The climb's internal
rhythm therefore matches the valley's exploration rhythm: the player already
knows, in their legs, how long "one stretch to the next thing" takes, and the
ascent speaks the same cadence. Each station also gains ≈ 40 m of relief, so
the view genuinely changes between them rather than repeating. Landings are what
make a climb read as long; raw distance just makes it tiring. At 1500 m that
is ≈ 8 min of walking one way, which is a journey.

**The Steps are BUILT and UNREPAIRED (story canon, and it costs nothing).**
The ascent is a **stair**, not a bare path: cut treads and revetted edges
following the route, in four generations of disrepair — worn and dished
treads, sections slumped or collapsed with the path detouring around them,
vegetation encroaching at the margins, revetment shed downslope as rubble.
Hard constraint: **disrepair is visual and routing, never impassable.** No gap
exceeds `PLAYER_STEP_HEIGHT`, every collapsed section has a walkable detour
within the ascent's slope band, and the summit stays reachable — the crown
kept the order poor, but nobody ever forbade the climb.

**Each landing is a STATION** (story: a pilgrim speaks a name at each). So
each carries a small built marker — a station stone, a niche, a lintel — sized
as an L2 guide, and the landing is a *place*, not merely a flat spot on a
path. Consequence to respect: `LR_ASCENT_LANDINGS` now has **narrative
dependents** — seven landings is seven recitation beats, and story's folk
etymology hangs the stair's name on the seven stations rather than on any step
count (which is what lets the name have a source in the world while the user's
"never count steps" rule stays intact). Changing the count changes a rite; it
is no longer a free pacing knob, and moves through story the way the castle's
completion fraction does.
> **⚠ TWO DIFFERENT CLIMBS — do not conflate them.** This world has two
> ascended landmarks and story nearly attached the wrong beat to the wrong
> mountain. **The Steps are HERE, on the regional temple massif (§2.5)** — a
> distant act-2 destination. **Ravenscar's climb is a different, local
> ascent** to the ward-tower ruin on the valley L0 (§7.1), which is act 1's
> climax. Same verb, different mountains, ~1.4 km apart. Whenever a beat says
> "the climb", check which landmark it means.

**DECIDED — user, 09:08:2026: "7000 steps" is a NAME, not a step count.**
(«НЕ буквально, 8 минут — кайф, название оставляем».) The numbers above stand
as written — 1200–1800 m, 5–7 landings, ≈ 8 min one way — and the climb keeps
its name in the fiction, which is story's to use in canon. Closed; do not
reopen on the arithmetic.

### 2.6 Border mountains — the world edge

Replaces the invisible walls. The world ends in geography, Skyrim-style.

- **Band:** `BORDER_BAND_WIDTH` = 200–300 m of mountain, preceded by
  `BORDER_FOOTHILL_WIDTH` = 100–200 m of rising ridgelets so the player
  climbs *into* it rather than meeting it **(предложение — утвердить)**.
- **Height:** crest at `BORDER_CREST_HEIGHT` = 150–250 m above local terrain,
  **varied ±30 % along its length** at a long wavelength (600–1200 m).
- **Not a wall — three shape rules:** (1) crest height varies as above, so it
  reads as a *range*; (2) spurs push inward irregularly by 100–250 m so the
  boundary is lobed, never straight — a straight edge is the tell that gives
  away a box; (3) the inner face uses the same ridged/cliff-band rules as
  §2.5, never a uniform slope.
- **Impassability: slope first, validation second.** The inner face averages
  ≥ 55° over ≥ 40 m of climb, which exceeds `PLAYER_MAX_SLOPE` (~50°) — but
  noise *will* occasionally produce a walkable saddle, so slope alone is not
  trusted. A traversability flood-fill from inside the playable area must
  fail to reach the outer edge; where it succeeds the generator raises the
  offending saddle and re-runs. Geometry plus a test, not a promise. Keep a
  hard clamp far outside the band as engineering safety — but it is a
  backstop nobody should ever touch, not a design element.
- **Border mountains are NOT attractors** (§1.3a): they are the frame. They
  never satisfy C1's "something to see" test — a wall of rock is not content,
  and letting it count would license genuinely empty ground.

### 2.7 Ground micro-relief and the plain

**Everything is slightly uneven.** The complaint is that the land reads flat;
§2.1's anisotropy gave us hill-scale ridgelets, and this is the layer below
it. Add a fourth octave, `GROUND_MICRO_WAVELENGTH` = 8–16 m at
`GROUND_MICRO_AMPLITUDE` = 0.3–0.6 m, plus an optional fifth at 2–4 m /
0.1–0.2 m for surface tooth **(предложение — утвердить)**. At 0.5 m over a
12 m wavelength the local slope is ≈ 5°, so this is free: it never threatens
`PLAYER_STEP_HEIGHT`, corridors, or building pads, and it kills the
billiard-table read at eye level. Micro-relief is **suppressed inside
building pads and the castle terrace** (they are cut flat on purpose) and
**retained everywhere else, including the plain**.

**Implementation status and the water constraint (core, stage-4 — this octave
had never actually been built anywhere).** It went in first on the massif's
benches, where §2.8.2 requires it. Core's attempt to apply it *globally* at
the same time was correctly backed out on a measurement: ±0.3–0.6 m on the
shoreline dropped bank dips below the water surface, and they rendered as
WaterBed past the §3.3 cap. That is a real finding and it produces the missing
rule rather than a reason to stay scoped:

- **Micro-relief AMPLITUDE TAPERS TO ZERO ACROSS THE SHORE BAND**, driven by
  the `dist_to_water` field that §4 and §5 already consume. No new data, no
  new constant — the taper simply reuses the shore mask.
- **This is physically right, not a workaround.** Ground beside water is flat
  *because water flattens it*: floodplains, banks and lake margins are
  deposited surfaces. A river running through a field of 0.5 m bumps is the
  artefact; the flat bank is the truth. So the rule improves the world at the
  same time as it fixes the bug, which is the shape a good constraint usually
  has.
- **The massif-only scoping is INTERIM and must not settle.** A micro octave
  that exists above the cliffline and nowhere else makes the cliffline a
  character seam — precisely the failure the "general, not forest-specific"
  ruling below exists to prevent, relocated from the forest edge to the
  mountain's hem. The general pass is its own scheduled item, gated on the
  shore taper plus a check against corridors, fords and building pads.

**Meso relief — the missing middle band (stage-4).** Between the hill octave
(128 m / 6 m, §2.1) and the micro octave above there was a gap, and it is
exactly the scale at which walking through a forest felt like a flat traverse.
Add `GROUND_MESO_WAVELENGTH` = 25–60 m at `GROUND_MESO_AMPLITUDE` = 1.5–4 m
**(предложение — утвердить)** — dips, rises and hollows you walk into and out
of. Max local slope ≈ 18°, so corridors and pads are unaffected.

**Terrain does NOT flatten under vegetation — vegetation absorbs the terrain.**
Stated because the opposite fix is the tempting one and it would undo this
whole section: micro and meso relief deliberately make the ground under a tree
uneven (a 1.2 m trunk on `TREE_SLOPE_MAX` spans ≈ 0.84 m of drop across its
own base, before roughness), and the answer is **geometry on the plant** — a
root flare that buries its own skirt (§5.10) — never a flattened disc of lawn
beneath every trunk. A forest floor smoothed under each stem is a pool table
with trees on it, which is precisely the flatness complaint that produced this
section. Any future "trees are floating" bug is a flora/render fix, not a
terrain one.

**Ruling: this is GENERAL terrain, not a forest-specific stamp.** Forests
merely sit on it. Three reasons: a forest-only stamp makes the forest edge a
seam where terrain character visibly changes — the classic tell of generated
ground; meadows want the same relief (it is the same "too flat" complaint);
and the *perception* that forests have more relief comes free, because trunks
and canopy give the eye something to measure height against, while open meadow
reads flatter at identical amplitude.

**Scarps (обрывы).** Small cliff steps `SCARP_HEIGHT` = 2–5 m, placed where
the meso field's local slope already exceeds a threshold, by a low-probability
terracing transform — `SCARP_DENSITY` = 0.5–1.5 per hectare inside forest
masses, rarer in open ground **(предложение — утвердить)**. Constraints: never
inside a corridor mask (§2.4); never enclosing a walkable region (a scarp is
an obstacle to go around, never a trap — the traversability check of §2.6
applies locally); and always with a walkable way around within 40 m, so a
scarp costs the player a decision, not a reload.

**One small plain, and it earns its flatness.** A flat area is only valuable
as contrast, so it is placed where flatness *does something*:
`PLAIN_EXTENT` = 400–600 m across, on the route from the valley toward the
LR, positioned so that **the massif is fully revealed and unobstructed from
it**. The enclosed valley opens onto the plain, and the mountain is suddenly
the whole horizon — that is the reveal beat (§1.4), and it is the reason this
is a composition and not a bald patch. Rules inside it: flat to ±1.5 m
overall, micro-relief retained (flat, not sterile), no forest mass, only
sparse L2 (standing stones, a lone skyline tree) so the openness reads as
intentional. The plain is also the natural site for a future FUTURE road and
for the act-scale muster/travel beats story may want.

### 2.8 Massif shape language — the anti-dome ruling, second pass (stage-4)

The user has rejected the mountain a **third** time, and this time gave a
shape brief rather than a complaint: «гора — это всё ещё сиська… гора она
должна быть **острая**, иметь **рёбра**, надо её **из камней собирать
местами, где-то кубы на кубах**, высоту надо задавать **линиями уровня,
которые где-то ближе, где-то дальше**, **перепады не должны быть
постоянными».** Decoded into the five things a generator must do:

1. **Sharp, not domed** — the summit is a point, not a crown.
2. **Ribs (рёбра)** — hard arêtes: flat faces meeting along visible crest
   lines, not soft radial swells.
3. **Assembled from stone in places, cubes on cubes** — blocky stacked rock
   that reads as *rock*, not as terrain that happens to be steep.
4. **Height defined by contour lines whose spacing varies** — close where it
   is steep, far where it is gentle.
5. **The steps are not uniform** — no constant gradient, and no wedding cake
   either.

They are looking at **Ravenscar** (`L0_RELIEF` 115 m). The temple massif does
not exist yet. This ruling therefore governs **every massif** — L0, LR, and
the inner faces of the border ranges — with Ravenscar as the acceptance case.

#### 2.8.1 Diagnosis — why the first invariant did not bite (measured, core)

Asked before ruling, and the answer is unambiguous.

**The dome is in the spec, not in the mesher.** The L0 stamp is
`smoothstep(1 − d/R) · peak − 13 m · (1 − prof) · (1 − ridged_noise)`. That is
a **smooth radial falloff with a little noise on its sides**: no angular
modulation, no cliff bands, no asymmetry. Worse, the noise term is multiplied
by `(1 − prof)`, so it **vanishes at the summit by construction** — the top
third of Ravenscar is a pure smoothstep surface of revolution. A smoothstep
radial profile has **zero slope at the apex, maximum slope at mid-radius, and
zero slope again at the hem**. That is not "a bit round". That is the
mathematical definition of the shape the user keeps naming.

Measured on the built crag, seed 1:

| Measure | Value | Reading |
|---|---|---|
| Lobe ratio at ½ / ⅔ / ⅚ relief | **1.27** at ½ (⅔ and ⅚ pending re-measure) — the **0.80 / 0.81 / 0.80** first recorded here is **WITHDRAWN**, see the box below | near-circular, and **identical at every height** — a self-similar cone. The clause it truly fails is the *rise*, not the level |
| Surface above mid-height over 40° | 45.9 % | passes the old 60 %-ish intent only partly, and pointlessly |
| Surface over 55° | **0.0 %** | there is no cliff anywhere on this mountain |
| Surface over 70° | **0.0 %** | |
| Slope histogram, whole crag | **FOOTPRINT-WEIGHTED — SUPERSEDED** (was: 0–10°: 45.4 %, 30–40°: 12.4 %, **40–50°: 33.2 %**, 50–60°: 0.8 %) | the reading stands — two spikes, flat ground plus **one uniform ≈45° flank** — but the *figures* are in the weighting §2.8.3 replaced, and are not reconstructed. See the box below |
| Field max slope vs mesh | field 68.7° max; mesh's 40–50° bin *higher* than the field's | **surface nets is not losing slope** |
| Raw contour spacing (5 m), CV | mean 6.9 m, σ 6.5 m, **CV 0.935** | base fBm bleeding through; the *stamp* is perfectly regular |

> ### ⚠ RE-STAMPED — the first lobe-ratio figures were produced by the
> ### digitiser this document forbids
>
> **The 0.80 / 0.81 / 0.80 above were measured by counting boundary cells.**
> §2.8.3 already bans that and requires the marching-squares contour polyline;
> the figures predate the rule they violate, so they were never legitimate
> readings of this terrain — they are readings of a digitisation choice.
> Re-measured on the **same** terrain under the binding rule: **1.27**.
>
> **The perimeter was short by ≈ 26 %, not the ≈ 10 % §2.8.3 estimated**, and
> the mechanism is worth stating so nobody re-derives it: counting boundary
> cells measures the *Chebyshev* length of an outline. A boundary running
> diagonally across the grid covers √2 of cell length per cell and is counted
> as 1, so a smooth, near-circular contour comes back short by up to
> 1/√2 ≈ 29 %. Our 26 % sits essentially at that theoretical floor — which is
> confirmation, not coincidence, since a near-circular contour is diagonal at
> most bearings. My predecessor's ≈ 10 % was itself an understatement; the
> rule was right for a **stronger** reason than the one it was written with.
>
> **Two consequences, and the second is the one that matters.**
>
> 1. **I8's headroom was never 69 %.** 1.27 against a 1.35 threshold is 6 %.
>    I8 is a far weaker test than the first pass believed, and its
>    load-bearing clause is therefore the **second** one —
>    `MASSIF_LOBE_RISE_MIN`, the requirement that lobing **grow with height**.
>    A self-similar cone can sit near 1.27 the whole way up; only the rise
>    clause can see that, which is exactly the blindness §2.8.1 diagnosed in
>    the single-slice test it replaced.
> 2. **The banded model REGRESSED lobing, and the contaminated baseline would
>    have disguised the regression as progress.** Post-band measurement:
>    **1.01, flat at every height.** Read against 0.80 that looks like a step
>    forward; read against the true 1.27 it is a step **backward**. The old
>    crenulation was base fBm bleeding through the smoothstep stamp — the same
>    bleed that scores contour-spacing CV 0.935 in the row above. It was
>    cosmetic, noise-derived and self-similar, but it was **real perimeter**,
>    and the band model replaced the slice outline with the authored `R_k(θ)`,
>    which is *cleaner* than the noise it displaced. **So the I7/I8 question is
>    not "how do we add lobing" but "does `R_k(θ)` REPLACE the fBm crenulation
>    or ride on top of it".** The mechanism is core's; the design requirement
>    is that structural lobing must exceed what noise gave us for free, at
>    every height, and must grow with height.
>
> **RESOLVED the same session — and half of point 2 was MY error, so it is
> corrected here rather than quietly left standing (core).** The re-stamp did
> its job: it stopped core accepting 1.01 and sent them looking properly. What
> they found was **a geometry bug in their own bearing-field helper, not a
> property of the band model.** They were sampling the bearing field on a
> circle sized so its circumference spanned `lobes` cells, which forces
> `radius = lobes · CELL / 2π` — at 3 arêtes on a 64 m cell that is a circle
> 61 m across sitting **inside a single 64 m cell**. The noise read it as one
> smooth patch, so contour radius varied ±4 % where `MASSIF_RADIAL_LOBE_AMP`
> asks for ±18–35 %. A circle cannot be simultaneously small enough to carry
> few lobes and large enough to cross cells: the construction was degenerate
> for **every** arête count this document specifies.
>
> - **What stands:** the re-stamp itself; that the 1.27 was fBm bleed rather
>   than structure (now *confirmed* rather than inferred); that I8's headroom
>   is 6 % and its load-bearing clause is the rise; and the sequencing call
>   that followed from all of it.
> - **WITHDRAWN — my mechanism.** I wrote that the band model "replaced the
>   slice outline with the authored `R_k(θ)`, which is *cleaner* than the noise
>   it displaced" — i.e. that authored outlines are inherently smoother than
>   fBm. That was a plausible story fitted to one number, and it was wrong:
>   `R_k(θ)` was not producing an outline at all. **There was never structural
>   lobing to erase.** The corrected sentence is narrower and duller — an
>   authored lobe term that does not actually vary *suppresses* the noise that
>   used to, and the result reads as a regression.
> - **The lesson, and it is the §1.3-withdrawal lesson wearing my own face.**
>   A directionally plausible mechanism gets less scrutiny than a surprising
>   one. "Clean authored geometry displaced dirty noise" is a satisfying
>   sentence, it explained the measurement, and it was fiction. The measured
>   number was right; my account of *why* was invented. **Ruling a number and
>   narrating its cause are two different acts, and only the first was mine to
>   make** — the mechanism should have been marked as a hypothesis for core to
>   confirm, which is exactly what this document demands of every finding it
>   receives. Recorded because the re-stamp is quoted approvingly above, and a
>   reader should see that the same box contains a correct ruling and a wrong
>   explanation attached to it.
>
> **The slope-histogram row: SUPERSEDED, not re-measured (ruled, core's
> proposal accepted).** The 33.2 % was **footprint-weighted**, measured by my
> predecessor on the old smoothstep dome — terrain that no longer exists in the
> tree. Any figure produced now would be *reconstructed*, not measured, which
> is the same class of act as the boundary-cell numbers this box withdraws. So
> the row reads **"footprint-weighted, superseded"** and carries no number in
> the weighting §2.8.3 has just replaced. `MASSIF_SLOPE_BIN_MAX` = 0.30 keeps
> its provenance stated honestly: it was chosen just under a *footprint*
> reading of 33.2 %, and under surface weighting that same flank would have
> read **higher** — so the threshold is conservative in the right direction and
> needs no revision. A constant whose provenance is "chosen against a number we
> can no longer take" is acceptable **only** when the direction of the error is
> known, which here it is.
>
> **The process point, which is the durable part.** The measurement rule was
> written before the measurement mattered, and it is what caught this. Cost:
> one paragraph in §2.8.3. Return: a baseline nobody will cite wrongly, and a
> regression that was wearing the costume of progress. Same lesson as the
> §1.3 withdrawal, arriving cheaply for once — this time the rule caught the
> number instead of the number surviving three sessions.

**Was my predecessor's invariant insufficient? Both answers are true and both
matter.**

- **It was never binding.** `LR_LOBE_RATIO` exists as a number in NUMBERS.md
  and **nothing in the pipeline evaluates it**. It was written in §2.5, which
  governs a massif that has not been built, while the user judges one that has.
  A rule authored against object A while the user looks at object B is a
  process failure, and it is the primary cause here.
- **It was also genuinely insufficient**, and saying only the first thing
  would be dodging. A **single horizontal slice** cannot see that the shape is
  identical at every height — the measured 0.80/0.81/0.80 is exactly the
  signature it is blind to. And "≥ 60 % of the upper surface above 40°" is
  **satisfied by a perfect 45° cone**, which is precisely «перепады
  постоянные». Both clauses are plan-view or scalar statistics; **neither
  constrains the vertical profile**, and the vertical profile is the entirety
  of what the user described.

**One test I intended to write, killed by the measurement.** A floor on the
*coefficient of variation of contour spacing* is a bad invariant: the current
dome already scores **0.935**. Variance is free — fBm supplies it. What the
user is asking for is not variance, it is **alternation**: risers and benches
in a rhythm that is itself irregular. The invariants below test alternation
and are recorded here with the rejected version, so nobody re-proposes CV.

#### 2.8.2 The generator model — the BANDED CONTOUR MASSIF

The user handed us the authoring model in their own sentence: *«высоту надо
задавать линиями уровня»*. Take it literally. A massif is **no longer a
radial profile with noise on it**; it is a **stack of contours**, and height
is reconstructed between them. This matters because it is *structurally
incapable* of producing a dome — there is no smooth falloff anywhere in it.

Four seeded fields, all per-sample analytic, all deterministic, all pure
height-function work (so the voxel pipeline carries them unchanged):

1. **Band elevations `e_0 … e_n`.** Non-uniform by construction:
   `e_{k+1} = e_k + Δ_k`, `Δ_k` drawn from a seeded distribution whose
   coefficient of variation is at least `MASSIF_BAND_SPACING_CV_MIN`. Bands
   start at the cliffline (`MASSIF_CLIFFLINE_FRAC` of relief) and run to the
   summit. **This is "линии уровня, которые где-то ближе, где-то дальше",
   authored rather than hoped for.**
2. **Radial extent `R_k(θ)`** — each band's outline as a function of bearing:
   `R_k(θ) = R_base(e_k) · (1 + ε(e_k) · ridged(θ))`, with
   `ε ∈ [MASSIF_RADIAL_LOBE_AMP_MIN, _MAX]` and **ε increasing with
   elevation**. Outward lobes are the **arêtes**; inward folds are the
   **couloirs**. Irregular by seeded phase — a symmetric star reads as
   artificial (§2.5.2, unchanged).
3. **Profile exponent `p(θ) ∈ [MASSIF_PROFILE_EXPONENT_MIN, _MAX]`** — the
   base falloff is `h = H·(1 − d/R)^p` with **p > 1**, which is the whole
   fix in one symbol. `p > 1` gives **steep at the summit, shallowing to the
   foot** — the concave profile every real mountain has, because talus fans
   out at the bottom. `smoothstep` gives the opposite and that is why it
   reads as a breast. Varying `p` with bearing **is** the §2.5.4 asymmetry
   rule: the low-`p` sector is the gentle flank that carries the ascent, the
   high-`p` sector is the scarp face.
4. **Riser class per (band, angular sector).** Each band's riser is either a
   **CLIFF** (≥ `MASSIF_CLIFF_SLOPE_MIN`) or a **RAMP**, chosen per sector by
   seeded noise. So a single band can be a cliff on the north side and a ramp
   on the south. This is what stops the wedding cake: the terracing is
   discontinuous *around* the mountain as well as irregular *up* it.

Between bands the surface is a **bench** (`MASSIF_BENCH_SLOPE_MAX`, width
`MASSIF_BENCH_WIDTH_MIN…MAX`). Riser heights come from
`MASSIF_CLIFF_BAND_MIN…MAX`.

**A CLIFF riser is a PLANAR face, and a bench is neither dead flat nor pinned
at its ceiling (core, found by measuring — four bugs with one lesson under
them).** Each of these is written down as a rule because each produced a
*measured* invariant failure on the first implementation of this model, and
none of them was visible by looking at the shape:

- **A smoothstepped riser spends its width on sub-cliff slope.** A riser eased
  in and out reaches `MASSIF_CLIFF_SLOPE_MIN` only at its midpoint, so most of
  its area lands in the 30–50° bins and **I3 measures no cliff on a mountain
  that visibly has them**. Cliff risers are therefore **planar faces** — full
  angle from lip to base. This is also what §4's snap rule requires: a crease
  is drawn as a crease only where there is one, and an eased riser has no lip
  for the screen-space slope derivative to find. My predecessor's promise of a
  **hard splat edge at band lips** was made assuming a planar riser; it is now
  a *requirement* rather than an assumption, and the two rules corroborate.
- **A dead-flat bench is a constant gradient too** — flat benches put **62 %**
  of the massif in a single slope bin, an I4 failure produced by the fix for a
  different I4 failure. And **pinning benches at `MASSIF_BENCH_SLOPE_MAX` is
  the same mistake wearing a different constant** (**75 %** in another bin).
  A bench is *ground*, and §2.7's rule is general and already binding:
  **terrain never flattens.** Benches carry the `GROUND_MICRO_*` octave
  (0.3–0.6 m over 8–16 m) and take a seeded slope spread **within**
  `MASSIF_BENCH_SLOPE_MAX`. That constant is a **ceiling** — the angle at
  which a bench stops being able to carry a road — and it was never a target.
- **A RAMP band must still be a band.** A sector whose riser class is RAMP
  cannot fall back to the bare underlying cone; that left half the massif
  unbanded and reads as the old dome wearing stripes down one side. The
  cliff/ramp choice varies the riser's **angle**, never whether the mountain
  has contours at that bearing.

**The lesson under all four, and it is why §2.8.3 says implement I1 and I4
first:** three of them are cases where the obvious fix for one invariant broke
another, and **constant-gradient failures move rather than disappear.** I4 is
the invariant that follows them around. All four were caught by measuring the
invariant, not by looking at the mountain — which is the whole argument for
having written the invariants down before building the shape.

**The cross-section is a FACETED POLYGON WITH RE-ENTRANT COULOIRS, and the
couloirs are load-bearing rather than decorative (core's construction, ruled
in).** `R_k(θ)` is built as an irregular rounded polygon by support function,
`r(θ) = min_i d_i / cos(θ − α_i)` — a boundary of **flat facets meeting at
corners**, which is this document's own definition of an arête («плоские
грани, сходящиеся по линии») rather than a proxy for it. Three consequences
worth having in the doc, because they constrain any future massif and not just
this one:

- **A support function is convex by construction, and a near-regular convex
  cross-section is CAPPED at `n·tan(π/n)/π`** — 1.65 for 3 facets, **1.27 for
  4, 1.16 for 5**. Against `MASSIF_LOBE_RATIO` = 1.35 that means **a convex
  massif with 4 or 5 arêtes cannot pass I8 at any amplitude**, and rounding
  the corners only lowers it further. Since `L0_ARETE_COUNT` is 3–5 and the
  LR's is 4–7, **couloirs are what make I8 satisfiable at most of the arête
  counts this document allows.** §2.8.2 asked for outward lobes *and* inward
  folds; this is the proof it needed both.
- **The convex escape route exists and is the wrong one — record it so nobody
  takes it.** The cap above is for a *near-regular* polygon; an **elongated**
  convex cross-section beats it easily (a 4:1 rectangle scores 1.99). So a
  future implementation could pass I8 convexly by stretching the massif. It
  must not: an elongated L0 is a **ridge, not a peak**, it breaks §1.5's
  skyline read and C4's "one unmistakable mass with a summit", and it would
  satisfy the invariant while destroying the thing the invariant protects.
  **Elongation is a landform choice (border ranges, §2.6, are legitimately
  elongated), never a knob for making a lobe test pass.** Core's addition, and
  it is the part worth having written down: **no invariant we have would
  notice.** An elongated support polygon puts its corners on the long axis, so
  arête bearings *cluster* — and I7's persistence check would keep passing
  while the mountain became a ridge. The bar must be a design rule precisely
  because the suite is blind to it.
- **I7 and I8 pull in opposite directions, and the resolution is that couloirs
  FADE TOWARD THE SUMMIT.** Measured by core: deepening and widening couloirs
  raised I8 and dropped persistent arêtes **4 → 0**, because a couloir spread
  across a facet *curves* that facet and I7 requires it flat. The fix is
  structural, not a tuning compromise: **couloirs are flank features that
  merge into the arêtes as they rise.** Summit contours stay clean facets (I7
  reads them); flanks keep re-entrant perimeter (I8 reads that). This is also
  what erosion actually does — a couloir is cut by what runs down it, and
  nothing runs down a crest. Second, independent reason it is correct: an
  angularly-constant couloir shrinks to ≈ 1 m of arc at summit radius, far
  under `MASSIF_ARETE_TURN_ARC_MAX` = 15 m, so near the top it could only ever
  be **noise to the arête detector**.

**COULOIR DEPTH IS ABSOLUTE (metres); COULOIR ANGULAR WIDTH IS RELATIVE (a
fixed fraction of its facet). This is the fix for I8's two clauses fighting
each other, and it is a change of UNIT rather than of value (ruling,
stage-4).** Core mapped the parameter space and found every axis they had
traded I8's level clause against its rise clause: couloirs carried to the
summit hold the level and kill the rise; couloirs that fade buy rise and lose
level; curving the blend trades them one-for-one and at `k³` collapses I7 to
zero arêtes. **They were all the same experiment**, because every one of them
varied *how fast the couloir fades* while leaving the couloir's depth
expressed as a fraction of local radius. That is the actual defect:

- **A quantity held as a fraction of local radius is self-similar by
  construction, and self-similarity is precisely what the rise clause exists
  to detect.** Lobe ratio responds to *relative* inset ε = depth / R. Hold ε
  fixed and the ratio is identical at every height — the 0.80/0.81/0.80
  signature §2.8.1 diagnosed, reproduced by a different mechanism.
- **Hold the depth in METRES and the rise appears for free.** R shrinks toward
  the summit, so a constant absolute inset is a *growing* fraction of a
  shrinking radius: ε = depth / R(h) rises on its own. The mountain becomes
  more articulated near the top not because its features grow but **because
  the mountain gets smaller around them**, which is what actually happens to
  real massifs and is exactly the read the rise clause was written to reward.
- **Angular width stays relative, and that is what protects I7.** Holding
  *width* absolute would make a couloir occupy an ever-larger angular slice of
  an ever-smaller circumference, curve the facets, and kill the arêtes again.
  Constant angular width keeps each couloir the same fraction of its facet at
  every height, and core's own arc-length check confirms it stays safe: ≈ 1 m
  of arc at summit radius, far under `MASSIF_ARETE_TURN_ARC_MAX` = 15 m, so up
  there it reads to the detector as a corner rather than as a curve.
- **Precedent, so this is recognisable rather than novel:**
  `MASSIF_ARETE_TURN_ARC_MAX` is already absolute on purpose — «гребень,
  который поворачивает 60 м, — это плечо, какой бы горы он ни был». Same
  reasoning, applied to the couloir instead of the crest.
- **THE ABSOLUTE SCALE IS THE BAND HEIGHT, NOT THE MASSIF'S RADIUS (core's
  correction, and it repairs a hole I put in my own ruling).** "Absolute"
  needs a unit, and the obvious choice is wrong: taking
  `MASSIF_RADIAL_LOBE_AMP` off the 180 m base radius gives 32–63 m insets —
  wider than the entire upper mountain — so the clamp below binds at *every*
  height and **silently restores the fraction-of-local-radius behaviour the
  unit change exists to remove.** Measured in that state: levels
  1.50/1.50/1.60 but rise 0.10 and I7 gone. My clamp was not a safety net, it
  was **a re-entry point for the bug it was guarding against**, and it is
  withdrawn as written. The scale is `MASSIF_CLIFF_BAND_MIN…MAX` (8–15 m), on
  core's reasoning and it is the right reasoning: **a couloir is a gully
  incising the cliff bands, so it is the same landform at the same scale and
  it should carry the same units.** A feature's size comes from the feature it
  cuts, never from the mountain it sits on — which is the same principle as
  `MASSIF_ARETE_TURN_ARC_MAX` being absolute.
- **A clamp against the apex is still needed but must not be able to bind
  below the summit region.** If it engages at ordinary heights it is
  re-introducing self-similarity, and that failure is silent — the invariants
  keep passing at the level while the rise quietly dies. Any implementation
  must report whether the clamp bound, and where.

**Varying arête COUNT with elevation is the other candidate and it is NOT the
first lever** (core raised it; ruled). Ribs merging as they rise is
geologically true and may earn a place later, but it fights I7's persistence
check by construction — an arête that stops existing above 0.55 relief cannot
be detected on 0.6 of four levels. Change the unit first: it is cheaper, it
touches nothing else, and it is the only axis core's sweep did not vary.

**A structural feature the invariants depend on is NEVER a per-instance coin
flip (ruled, from a near miss).** Core's first variant made couloir *presence*
a seeded per-facet draw; on seed 1 all three facets missed and the massif came
out a bare convex polygon with zero couloirs — a shape that *looks* reshaped
and satisfies nothing. Rule: **the seed varies a feature's character — depth,
asymmetry, bearing, spacing — never its existence.** Anything an invariant is
counting must be guaranteed by construction, because a seeded absence produces
a world that fails silently and plausibly, which is the most expensive kind of
failure we have. Same rule already applies to arêtes, cliff bands and benches;
it is written down here because a coin flip is such a natural way to author
variety.

**Both per-bearing fields must be PERIODIC in θ (core's catch, binding).**
`R_k(θ)`, `p(θ)` and the riser-class sector index all wrap: sampling noise on
the *angle value* puts a branch cut at ±π and produces **a vertical seam from
summit to foot** — a scar exactly where nothing should be. Sample instead on
the unit vector `(cos θ, sin θ)`, i.e. noise over a circle embedded in the
plane, which is periodic by construction and costs nothing; and let sector `0`
and sector `n−1` be neighbours so the cliff/ramp alternation has no
discontinuity at the same bearing. Recorded here rather than left in a message
so the LR massif does not rediscover it.

**Why this is cheap:** every term above is arithmetic on `(d, θ, h)` at a
sample that is already being evaluated. Core's own ranking agrees — angular
ridge modulation, non-uniform falloff, cliff-band quantisation and asymmetry
are all *free* per-sample math on a position function. No new pass, no new
storage, no mesher change.

**The bonus nobody had to pay for.** Cliff risers exceed `SLOPE_ROCK_MIN`
(40°) and benches sit under `SLOPE_GRASS_MAX` (30°), so the existing §4 splat
paints **risers as rock and benches as grass/blend automatically**. The
contour rhythm becomes a **visible horizontal stripe rhythm** at 640×360, with
zero shader work. The user asked to define height by contour lines; this is
the mechanism by which they will actually *see* them.

#### 2.8.3 The invariants — nine tests the generator runs on itself

Scope: any landform whose relief ≥ `MASSIF_RULE_MIN_RELIEF` = 40 m. Knolls
(+6 m) and the lakeshore bluff (+10 m) are bumps and are exempt. Border
ranges (§2.6) inherit **I1, I3, I4, I5, I7** on their inner face; **I2 and I8
are massif-only** (a range has no single summit and no closed slice).

| # | Invariant | Test | Current crag |
|---|---|---|---|
| **I1** | **Concave profile** (the core anti-dome test) | mean slope over the **upper third of relief** exceeds mean slope over the **lower third** by ≥ `MASSIF_PROFILE_STEEPENING_MIN` = 12° | **FAILS** — one uniform 45° flank, difference ≈ 0 |
| **I2** | **Sharp summit** | mean slope within `MASSIF_SUMMIT_RADIUS_FRAC` = 0.12 of base radius of the summit ≥ `MASSIF_SUMMIT_SLOPE_MIN` = 40° | **FAILS** — smoothstep gives slope → 0 at the apex |
| **I3** | **Near-vertical rock exists** | ≥ `MASSIF_STEEP_FRACTION_MIN` = 0.12 of the surface above the cliffline exceeds `MASSIF_CLIFF_SLOPE_MIN` = 55° | **FAILS** — measured 0.0 % |
| **I4** | **No constant gradient** (the direct «перепады не постоянные» test) | above the cliffline, **no single 10° slope bin holds more than** `MASSIF_SLOPE_BIN_MAX` = 0.30 of the surface | **FAILS** — 40–50° holds 33.2 % of the *whole* crag, far more above the cliffline |
| **I5** | **Riser/bench alternation** | on each of `MASSIF_RADIAL_SAMPLES` = 64 radials, between ⅓ and full relief, ≥ `MASSIF_BAND_ALTERNATION_MIN` = 3 transitions between cliff class (≥ 55°) and bench class (≤ `MASSIF_BENCH_SLOPE_MAX` = 25°); required on ≥ `MASSIF_RADIAL_PASS_FRACTION` = 0.7 of radials | **FAILS** — no cliff class exists, so zero transitions |
| **I6** | **Band spacing is irregular** | CV of the **vertical spacing between successive cliff bands** along a radial ≥ `MASSIF_BAND_SPACING_CV_MIN` = 0.35. **Measured on band spacing, never on raw contour spacing** (see §2.8.1 — the dome scores 0.935 on the latter) | n/a — no bands |
| **I7** | **Arêtes exist, are sharp, and persist** | on contours at 0.4/0.55/0.7/0.85 relief, an arête is a point where surface **aspect turns ≥ `MASSIF_ARETE_TURN_MIN` = 50° within `MASSIF_ARETE_TURN_ARC_MAX` = 15 m of arc**, flanked on both sides by **facets turning ≤ `MASSIF_FACET_TURN_MAX` = 15° over ≥ `MASSIF_FACET_ARC_FRAC_MIN` = 0.08 of that contour's perimeter**. Require ≥ `MASSIF_ARETE_COUNT_MIN` = 3, each detected on ≥ `MASSIF_ARETE_PERSISTENCE_MIN` = 0.6 of the four levels within `MASSIF_ARETE_BEARING_TOL` = 15° of bearing | **FAILS** — no angular structure at all |
| **I8** | **Lobed AND increasingly articulated** | lobe ratio `P²/(4π·A)` at ½, ⅔ and ⅚ relief **each** ≥ `MASSIF_LOBE_RATIO` = 1.35, **and** `lobe(⅚) − lobe(½)` ≥ `MASSIF_LOBE_RISE_MIN` = 0.15 | **FAILS** — 0.80/0.81/0.80, flat as well as low |
| **I9** | **Blocky rock present** | placed rock assemblies cover `ROCK_OUTCROP_COVERAGE_MIN…MAX` = 0.10–0.20 of the surface above the rockline, **and the summit carries a tor** (§2.8.4) | **FAILS** — no such asset class exists |

**Three measurement rules, or the invariants measure the digitiser instead of
the world.**

- **Perimeter comes from the marching-squares contour polyline**, never from
  counting boundary cells. **Measured, not estimated:** boundary-cell
  perimeter came back short by **≈ 26 %** on our own crag (the ≈ 10 % first
  written here was itself an understatement — see the re-stamp box in §2.8.1),
  which moved the same terrain's lobe ratio from 0.80 to **1.27** against a
  1.35 threshold. A threshold that a digitisation choice can flip is not a
  threshold.
- **Slope is measured on the extracted mesh normals as well as on the field**,
  and both are reported. This session's whole diagnosis turned on that pair
  disagreeing; keep the ability to ask the question.
- **"Of the surface" means TRUE SURFACE AREA, never plan-view footprint
  (ruled — it decides I3's verdict).** The two measures answer different
  questions, and only one of them is the question I3 and I4 ask. Footprint is
  the **map projection** — what a bird, or a contour sheet, sees. Surface area
  is what the mountain **presents to a player standing on the valley floor**,
  which is the standpoint this entire section was written from: the user is
  looking at Ravenscar from below, side-on, and a near-vertical face fills
  that view while contributing almost nothing to a map.
  - **The decisive argument is that footprint weighting is anti-correlated
    with I3's own goal at the limit.** A 70° face carries ≈ 2.9× the surface
    of the ground beneath it; a 90° face carries **zero** footprint. So under
    footprint weighting the score of "near-vertical rock exists" falls toward
    zero *exactly as the rock becomes perfectly vertical*. A test that reports
    its own ideal as absence is not a test, and no threshold can repair it.
  - **The same weighting is right for I4, for a second and independent
    reason.** Surface weighting inflates steep bins by 1/cos, so the one
    failure mode I4 exists to catch — a single uniform ≈ 45° flank — is
    weighted **up** against the flat ground around it. Footprint does the
    opposite: it discounts that flank by 0.71 while counting flat benches at
    full weight, so the histogram fills with valley floor and the uniform
    flank hides inside it. An invariant must be **strictest** against the
    thing it was written to reject.
  - **Consistency is the third reason.** I3 and I4 use the same phrase over
    the same population in adjacent rows of one table. Two meanings for one
    phrase in one table is a trap laid for whoever reads this next.
  - **How to compute it, without a new constant.** The **binding** reading is
    **triangle area on the extracted mesh**: finite by construction, needs no
    clamp, and it is literally the surface the player looks at. The field-side
    reading (cell footprint ÷ cos slope) is **reported alongside** as the
    cross-check — it diverges at vertical and would need a clamp, i.e. a
    constant, which is precisely why it is the check and not the verdict. The
    two disagreeing is **information**: §2.8.1's whole diagnosis came out of a
    mesh/field pair disagreeing.
  - **Where footprint legitimately still rules: I8.** A horizontal slice's
    outline is a plan-view object *by definition* — `P` and `A` there are the
    perimeter and area of a 2D curve, not of a surface. Nothing about I8's
    measure changes (only its baseline did, §2.8.1). **I9's coverage
    denominator inherits the surface rule**, which makes I9 slightly harder on
    a steep massif; that is the correct direction, because a mountain with
    more cliff on it needs **more** rock, not the same rock spread thinner.
  - **THIS IS A SECTION-LEVEL CONVENTION, NOT A GLOSS ON ONE PHRASE (my
    error, corrected on core's second asking).** I first ruled this against
    the words "of the surface", which appear in I3 and I4 and nowhere else —
    so when the tor landed, core correctly refused to extend it by analogy to
    **I2**, whose row says only "mean slope". They were right to ask rather
    than assume, and the ambiguity was mine for stating a convention as a
    footnote to two rows. Restated properly: **every slope, area and coverage
    statistic in §2.8.3 is surface-area weighted.** I2, I3, I4 and I9 all take
    it. **I8 is the sole plan-view measure**, because a horizontal slice's
    outline is a 2D curve rather than a surface. No third reading exists.
  - **I2 in particular, because the argument is stronger there than for I3.**
    A tor is flat tops and near-vertical sides — the *pure* case of the limit
    argument above — so under plan weighting the flat tops carry all the
    footprint, the vertical sides carry almost none, and **a textbook tor
    scores as FLAT**. Core measured exactly that: adding the tor moved the
    footprint reading 32.7 → 32.5, i.e. it registered the ideal as a slight
    regression. But the decisive reason is not the analogy, it is that a
    plan-weighted I2 **would reject the landform §2.8.4 mandates** — the
    document would be testing for the absence of the thing it requires. When a
    test and a design ruling contradict, the ruling says what the mountain
    *is* and the test only says whether we got there; the test yields.
    **I2 therefore PASSES at 52.9°** against `MASSIF_SUMMIT_SLOPE_MIN` = 40°.
    The test is not thereby made vacuous: a smoothstep dome has slope → 0 at
    the apex, where cos ≈ 1 and the two weightings agree, so it still fails.
  - **Reporting doctrine, standing:** print **both** readings and label which
    one is the verdict. Same discipline as C2's raw unexempted count (§1.1)
    and the mesh/field slope pair above — an interpretation is stated in the
    open, and interpretations get audited. Core printing both rather than
    resolving the ambiguity in their own favour is the behaviour to keep; this
    ruling supplies the verdict, it does not delete the second column.

**Verdict on the open reading, stated so it is not left implicit: I3 PASSES at
16.5 %** against `MASSIF_STEEP_FRACTION_MIN` = 0.12. The 6.0 % footprint
reading is retained as a **diagnostic** — the ratio between the two readings is
a free measure of how much of the massif is steep — and is never a verdict.

**I7 IS RETRACTED AS EVER HAVING PASSED, AND ITS SAMPLING RANGE IS CORRECTED
(core, stage-4).** Every "4 persistent arêtes" reported to this document is
withdrawn, including the row in §7.1: the probe counted qualifying bearing
*samples* rather than ridges, and anchored its persistence scan on the lowest
slice. Corrected, **I7 fails on all 12 seeds** (max 2 against a floor of 3),
and core validated the detector against a couloir-free faceted polygon whose
corners exist by construction before believing the failure. **The lesson is
the sharpest one this stage: 42 arêtes was absurd on its face and was caught
instantly; 4 was not absurd and survived three rounds of rulings.** A wrong
number in the plausible range buys itself unlimited time — this is the third
instance this session, after the C1 self-occlusion figure and my own lobing
mechanism, and all three were *directionally reasonable*.

**Then the failure turned out to be partly mine: I7 was sampling where the
model never promised arêtes.** Raw detections rise with height — 2/3/5/5 at
0.4/0.55/0.7/0.85 — because §2.8.2's `ε` increases with elevation, which is
the mechanism that earns I8's rise clause. So **the mechanism satisfying I8's
rise is the mechanism preventing I7's persistence**, and two of I7's four
levels sit in the smooth apron below `MASSIF_CLIFFLINE_FRAC` that §2.8.2
explicitly describes as unbanded. **RULING: I7's persistence is measured over
four levels spanning the BANDED ZONE only — from the cliffline to the summit,
never the apron.** This is not a relaxation invented to make a red test green:
`ε` increasing with elevation was written before the conflict appeared, and
ribs dying into a talus apron is what real massifs do — it is the mirror of
the couloir-fade ruling above, which nobody objected to.

> **The guard that makes this legitimate rather than a loophole: I11.** A test
> whose sampling elevations I am free to choose is a test I can always make
> pass. I11 (§2.8.7) is measured from a camera against the sky and **cannot be
> gamed by choosing slice elevations at all.** This relaxation of I7 is
> therefore conditional on I11 existing — if I11 is not implemented, I7 keeps
> its original levels and stays red, because a proxy may only be loosened once
> the thing it was proxying for is being measured directly.

**I5 is measured on radials that carry no validated route (core's catch,
ruled).** A route breaches the cliff bands it crosses (§2.8.5), which locally
destroys the alternation I5 counts on that radial — so counting it there would
make **the ascent cause its own invariant to fail**, which is the §6.2
pad-scorer mistake in new clothes (judging a feature by a metric its own
purpose contradicts). Count alternation on non-route radials; assert the
existence of breaches **separately**, as their own check.

**Consequence of I3 that must be made loud, not discovered.** Cliff risers at
≥ 55° exceed `PLAYER_MAX_SLOPE` (~50°), so a compliant massif is genuinely
unclimbable off-route. That is the intent — it is what makes the breach
legible — but it means the crag's summit route becomes **the only** way up,
and a route-validation failure leaves the summit *unreachable* rather than
awkward, with act 1's climax attached to it. Route validation failure on a
banded massif is a **hard seed failure**, reported loudly, never a warning.

**I7 is the arête test and it is worth stating why it is shaped this way.**
"Ribs" is not "bumpy in plan". A rib is **flat faces meeting along a line**.
Aspect (the compass direction of downhill) is *constant across a face* and
*flips fast at a crest*, so the distribution of aspect-turn-per-arc is what
separates a ridged mountain from a lumpy one — and it is scale-free, which is
why the same test governs a 115 m crag and a 280 m massif. Persistence across
four elevations is what stops a single noise lump from scoring as a rib.

**I1 and I4 are the two that would have caught this a session earlier**, and
neither is expensive. If only two are implemented first, implement those.

**Fourth rule, added when the invariants started passing: A MARGINAL PASS ON
ONE SEED IS NOT COMPLIANCE.** I8 first passed at **1.36 against 1.35, with the
rise at exactly 0.15 against 0.15** — zero headroom on the clause §2.8.1
identifies as the load-bearing one. A shape parameterisation that lands *on*
its bound on seed 1 will land under it on roughly half of every other seed,
and every massif in the world (LR, border inner faces, future valley L0s) is
generated from the same rules with a different seed. So:

- **Invariants are reported as a distribution across seeds, not a verdict on
  seed 1.** Generate the massif under a handful of seeds and report min /
  median / max per invariant. This is cheap — headless generation plus the
  measurement code that already exists — and it is the only way to tell a
  parameterisation that is *right* from one that is *lucky*.
- **When the median sits at the bound, the SHAPE PARAMETERS move, not the
  threshold.** Widening a threshold to admit a marginal shape is the
  accommodation this document has refused twice already (§1.3's unspent
  physics-correction budget is the precedent).
- **Reason this is a design rule and not core's implementation detail:** it is
  the same failure class as §7.0a's — a coordinate stamped against one terrain
  state, mistaken for a property of the world. An invariant validated on one
  seed is a stamp against one terrain state.

#### 2.8.4 «Кубы на кубах» — the tor ruling, and what voxels can honestly do

**Say the achievability plainly, because the answer is not the one that was
assumed.** Surface nets rounds edges — that is recorded in
VOXEL_ARCHITECTURE.md §2 as a known cut of the crunch variant. It was
reasonable to suspect the mesher of the smoothness. **It is not guilty:**
measured field max slope 68.7°, and the mesh's 40–50° bin sits *above* the
field's. At `VOXEL_SIZE` 1.0 m the rounding radius is ≈ 1 m, which against an
8–15 m cliff band is a 7–12 % softening of the lip — a *weathered* cliff top,
which is what we want anyway.

So the brief splits cleanly by feature scale:

| Scale | Feature | Mechanism | Cost |
|---|---|---|---|
| ≥ 8 m | arêtes, couloirs, cliff bands, sharp summit, non-uniform contour spacing | **terrain SDF, per-sample math** | **free** — no new pass, no new storage, no mesher change |
| 3–8 m | slab benches, stepped shoulders | terrain SDF, ≈ 1 m lip rounding accepted | **free**, reads correctly |
| < 3 m | **«кубы на кубах»** — stacked slabs with crisp arrises | **placed rock meshes** | new asset class + placement pass + collision |

**RULING: the blocky read is PLACED MESHES, not dual contouring.** Four
reasons, and the first is decisive:

1. **Dual contouring would not deliver it.** DC sharpens edges the SDF already
   contains; it does not raise the sampling rate. At 1 m voxels a block under
   ≈ 3 m does not survive Nyquist at all, so DC buys us *crisper 3–5 m
   masses*, never "cubes". The thing the user asked for is below our voxel
   floor by construction.
2. **DC is a mesher change that touches every surface in the game** (core's
   words), with non-manifold and determinism care, for a benefit confined to
   one landform class.
3. **Placed rock is reusable everywhere else** — scarp faces (§2.7), outcrops
   (§2.2), the barrow lintel and standing stones (§6.2), the castle's spoil
   heap and never-laid dressed stone (§6.1.3), quarry cuts. It is not a
   one-mountain investment.
4. **It is the reference answer.** Bethesda puts placed rock meshes over
   heightfield terrain for exactly this problem, and the user has been told as
   much. Instanced, LODs trivially, arbitrarily crisp arrises.

**The tor rule — the single highest-value item in this whole ruling.**
**A massif's summit is a TOR, not a terrain vertex.** The top
`SUMMIT_TOR_HEIGHT` = 6–12 m of Ravenscar is a **stack of tilted slabs** over
a footprint of `SUMMIT_TOR_RADIUS` = 5–10 m, with the watchtower ruin (§6,
§7.1) standing **on** it rather than on smoothed ground. This is a real
landform (a granite tor), it is literally "кубы на кубах", and it converts the
one part of the mountain the eye always lands on from a rounded crown into a
broken rock crest. Ravenscar's silhouette against sky stops being an arc.

The LR's summit carries the temple; there the tor becomes the **plinth** the
temple stands on — same rule, same geometry, the building sits on rock.

**Built as a HEIGHT STAMP, and two rules came out of building it (core,
stage-4 — both found by measuring, both general).** The tor is terrain SDF,
not the placed-mesh class: the scale table above puts ≥ 3 m features in the
height function for free, and these slabs are metres thick over a 5–10 m
footprint. Slab count derives from the ≈ 3 m Nyquist floor implied by
`VOXEL_SIZE`, so it borrows nothing from the placed-rock constants.

- **"The tor REPLACES the top" must not be implemented as "TRUNCATE the
  top".** Capping the cone at the tor base and returning a flat platform
  outside the slabs builds a **mesa**, and truncation is precisely the
  silhouette I2 exists to reject: the summit-slope measure got **3.6° worse**
  when the feature meant to fix it was added. The cone is capped only *inside*
  the stack and left alone outside. Recorded because "replace the top" is a
  natural sentence with a wrong obvious implementation.
- **`L0_RELIEF` IS A CONTRACT — anything stamped on a summit is measured INTO
  the landmark's relief, never added ON TOP of it.** Core's tor initially
  overshot (its base was derived from `SUMMIT_TOR_HEIGHT_MIN` while its slab
  heights were drawn across `_MIN…_MAX` — reading one bound of a range as if
  it were the range, the same shape as the bugs in §2.8.2) and the peak drifted
  115.0 → 116.1. That is not a cosmetic 1 m: `CASTLE_SKYLINE_MARGIN`, R3, R4,
  C1 and the whole landmark hierarchy are **ratios and margins to the peak**,
  so a summit feature that quietly raises the peak edits every one of them
  from a zone that does not own them. Peak now measures exactly 115.0.

**Assembly grammar (generator rules, not hand placement):**

- A **stack** is `ROCK_STACK_BLOCKS` = 2–5 blocks, each
  `ROCK_BLOCK_SIZE` = 1.5–4.0 m, flat-topped, near-vertical sided, hard
  arrises, ≤ `ROCK_BLOCK_TRI_BUDGET_MAX` = 60 tris per block.
- Blocks are **offset laterally by up to `ROCK_STACK_OFFSET_MAX` = 0.8 m** and
  **tilted by up to `ROCK_STACK_TILT_MAX` = 0.21 rad (12°)**. A perfectly
  level, perfectly aligned stack reads as **masonry**, and the moment it does
  the player thinks *ruin*, not *mountain*. The offset is what makes the
  silhouette stepped; the tilt is what makes it geological.
- **Placement is derived from the terrain, never tabled** (§7.1a rule,
  extended once more): stacks sit on **arête crests** at
  `ROCK_STACK_SPACING` = 15–35 m of crest length, at **cliff-band lips and
  bases**, and on the summit. A crest that resolves, on approach, into stacked
  blocks is the payoff shot of this entire section.
- **Never on a bench a route crosses**, never inside a corridor mask, never
  where they would block a validated ascent (§2.8.5).
- **Budget:** `MASSIF_ROCK_TRI_BUDGET_MAX` = 60 000 tris for a whole massif at
  LOD0 — ≈ 1.5× one chunk of today's heightfield mesh. **Instancing and LOD
  are mandatory, not optional**: at 0.15 coverage Ravenscar wants ≈ 270 stacks,
  which is fine as instances and unaffordable as unique meshes. **Render has
  accepted these numbers as-is and the machinery already exists** — stacks
  bake into the same per-chunk world-space merged buffers the trees use, so
  "instanced" here means one buffer per chunk, not per-instance draws. No new
  batching work, and the coverage number does not need to move.

**The shadow-caster floor — a constraint I did not know, and it bounds all
future rock detail (render, measured).** At our shadow-map resolution
**anything under ≈ 0.31 m across casts no shadow at all**, and an unlit block
sitting on shadowed ground reads as pasted-on geometry rather than as rock.
Consequences, ruled:

- Our 1.5–4.0 m blocks clear it comfortably; nothing changes today.
- **The crisp read must come from the block's SILHOUETTE, never from arris
  detail.** A chamfer or a stepped edge under 0.31 m contributes no shading
  information at 640×360 and costs triangles for nothing — which is the same
  conclusion §6.1.3 reached about masonry coursing and §1.5 reached about
  battlement teeth, now with a measured number attached. Spend the 60-triangle
  budget on the block's *outline*, not on its corners.
- `ROCK_STACK_OFFSET_MAX` (0.8 m) is comfortably above the floor, which is
  part of why the stepped silhouette works: the offset is what casts.
- **This figure is NOT a NUMBERS.md constant.** It is derived from shadow-map
  resolution, which is a render setting like `INTERNAL_RES` (sync №3) — it
  moves when the setting moves. It is recorded here as a *published render
  figure that design rules against*, and any future prop class smaller than
  the current rock blocks must re-ask render for the number rather than cite
  0.31 m from this line.

**Dual contouring is explicitly NOT required by this ruling** and stays where
core deferred it. Its real customers are the castle terrace, quarry cuts and
cave mouths (VOXEL_ARCHITECTURE §2). Revisit it there, on their evidence, not
on the mountain's.

#### 2.8.5 What this costs the rules we already have

A landmark's shape is load-bearing for a dozen placements (§7.0a's durable
rule: *changing a landmark's relief invalidates every placement on its
slopes*). Applying that rule to my own change:

- **The validated ascents survive by being BREACHES, and that is an
  improvement, not a concession.** Ravenscar's summit route (§7.1), the LR's
  Steps (§2.5) and the castle scramble (§6.1.3, `SCRAMBLE_SLOPE` 30–45°) must
  each cross the cliff bands. Rule: **cliff bands are broken where a validated
  route crosses them**, breach width ≤ `MASSIF_ROUTE_BREACH_WIDTH` = 12 m,
  generated as part of the route stamp and reading as a gully or a gate. The
  gain is real: when everything else is cliff, **the one way up becomes
  legible from the valley floor**. The mountain teaches its own route. A
  breach is a feature of the shape language, not an exception to it.
- **Castle R1–R4 (§6.1.1) must be re-run, and I predict they get easier.**
  The base radius is unchanged (180 m), so the crag's angular footprint at the
  horizon — what R1 tests — does not move. `p > 1` makes the *mid-body*
  slimmer, so the crag occludes itself less and R2 (flank yes, crown no) gains
  headroom. **Predicted, not assumed:** re-validate. The failure mode to watch
  is the opposite of the one that looks obvious — a slimmer upper body could
  narrow the *upper* footprint enough to push a tall element outside R1.
- **§7.0a's barrow couloir search should now succeed.** Core's search for a
  low-terrain couloir in the 180°–240° arc failed against a stamp that has no
  couloirs — angular lobing creates them by construction, and core flagged
  this connection unprompted. **Re-run the couloir search after the reshape
  before touching the high-shoulder fallback**, which stays out of scope.
- **C1 / C4 (§1.3):** less self-occlusion should raise clearance; cliff bands
  add local flank occluders where the pine strips already sit. Re-measure;
  the floor does not move (§1.3's standing rule).
- **Flora — trees move onto the benches** (§5.10 already has the machinery).
  `TREE_SLOPE_MAX` (0.61 rad) excludes cliff faces automatically, so
  vegetation collects on benches, and the **cliff-edge setback measured from
  the outer edge of the root flare** now applies at *every band lip*, not just
  at scarps. Four rulings, three of them flora's and better than my drafts:
  - **My "one cluster per bench segment" is WITHDRAWN — right intent, wrong
    unit.** A count does not survive scale: one stand on a 200 m bench reads
    as a potted plant exactly as badly as a continuous line reads as
    landscaping, and both are the same failure — *the vegetation does not
    respond to the mountain*. Replaced by a duty cycle:
    `BENCH_VEG_DUTY_MAX` = 0.25 of a bench's running length,
    `BENCH_CLUSTER_LENGTH_MAX` = 25 m, `BENCH_CLUSTER_GAP_MIN` = 40 m, and
    **`BENCH_BARE_FRACTION_MIN` = 0.40 — at least 40 % of benches carry
    nothing at all.** That last one is the load-bearing one: what makes a
    stand on a ledge above a 12 m drop extraordinary is that the ledges above
    and below it are **bare**. A mountain where every bench has its one
    dutiful cluster is still landscaped, merely at lower density.
  - **Placement is biased to the LIP, not the riser base**
    (`BENCH_VEG_LIP_BIAS` = outer 0.40 of the legal band). Flora's addition
    and it matters more than the cap: a tree at the inner edge has a cliff
    face directly behind it — dark on dark, no silhouette, invisible at any
    range. The same tree near the lip has **sky** behind it and its crown
    overhangs the drop. This is §1.5's skyline rule applied at band scale, it
    costs nothing, and it is the entire reason to vegetate benches.
  - **6 m benches are wide enough; no terrain floor changes.** My worry
    double-counted the setback: only the **outer** lip is a drop. The inner
    side is the *base* of the riser going up — not a fall hazard, needing only
    the flare's own radius so the trunk is not embedded in rock, and a tree at
    a cliff base is a good thing. Legal axis band = `W − r_flare − (1.5 +
    r_flare)`; measured from the shipped meshes a pine (`r_flare` 0.84 m) gets
    2.81 m of lateral freedom on a 6 m bench. Vegetation floors:
    `BENCH_VEG_WIDTH_MIN` = 5.0 m (below it, bushes and grass only) and
    `BENCH_VEG_WIDTH_MIN_GIANT` = 7.0 m — **both below the 6 m terrain
    minimum**, so the two systems do not fight.
  - **The treeline SNAPS TO THE NEAREST BAND LIP.** A flat elevation cutting
    across banded terrain lands mid-riser and half-vegetates cliff faces,
    which reads as a **mowing line** rather than a limit. Snapped, the tree
    limit follows a geological feature — which is what real treelines do on
    banded rock and is far easier to look at. Same reasoning for the rockline.
    Cost is core's call; it should be small, since the lips are already known.
- **Render — RESOLVED, and the rule came back better than I asked for.** I
  requested a narrow exception for a hard splat edge at band lips; render
  reframed it as the general rule — *dither where the geometry is smooth, snap
  where the geometry has an edge* — which is not a carve-out from §4 but §4
  applied to a surface with creases, and it generalises free to quarry faces,
  cut terraces and cave mouths. Now written into §4 itself. Mechanism is a
  screen-space slope derivative: a couple of ALU instructions, no new data, no
  constant from design, threshold set by looking at a frame.
- **The splat coupling this section depends on is STRUCTURALLY protected, not
  merely agreed** (render, worth recording because it is the difference
  between a promise and a guarantee). Their zone carries a standing ruling
  from the "brown wash" incident: render must never re-derive material bands
  from raw height or distance fields — material comes from core's
  `surface_class`, and the shader only *augments* rock by slope between the
  two §4 thresholds. So the mechanism §2.8.2 relies on is the only one
  available to them, and decoupling slope from material on steep ground would
  require deliberately reintroducing a banned bug class. **The contour rhythm
  is safe by construction rather than by anyone remembering.**
- **Sim — cliff faces at 55° exceed `PLAYER_MAX_SLOPE` (50°)** and are
  therefore natural barriers, which is what we want. Watch the character
  controller against the ≈ 1 m rounded band lip.
- **Voxel pipeline — nothing to do.** All of §2.8.2 is height-function work,
  and per RIVER_RESEARCH §0.3 any change expressible as a height modification
  survives the voxel pipeline unchanged. Only the placed rock of §2.8.4 is new
  geometry, and it is ordinary instanced meshes with collision.

#### 2.8.6 Constants (for NUMBERS.md, Rule 14)

Renames first, because one of them is the lesson: **`LR_LOBE_RATIO` →
`MASSIF_LOBE_RATIO`**, **`LR_CLIFF_BAND_MIN/MAX` →
`MASSIF_CLIFF_BAND_MIN/MAX`**, **`LR_CLIFFLINE` → `MASSIF_CLIFFLINE_FRAC`**.
Nothing evaluates them today, so the rename is free; scoping a shape rule to
one landmark is exactly the failure that produced this session.
`LR_RIDGE_COUNT_MIN/MAX` (4–7) stays as the LR's generator input and is now
understood as its **arête count**; Ravenscar gets `L0_ARETE_COUNT` = 3–5.

**RULING (stage-4): Ravenscar's arête count is FOUR, and `L0_ARETE_COUNT` = 3–5
is retired as a range.** Core measured all three values across 12 seeds:

| Arêtes | I8 level | I8 rise | Reading |
|---|---|---|---|
| 3 | fails 3 seeds | fails 2 seeds | and the count *equals* the invariant floor |
| **4** | fails 1 seed | **fails none** | the only workable value |
| 5 | **fails all twelve** | — | the convex cap, measured |

- **Five is not a choice, it is arithmetically excluded.** `n·tan(π/n)/π` =
  1.16 for a pentagon against a 1.35 threshold, and couloirs cannot rescue it.
  The algebra in §2.8.2 predicted this and the measurement confirmed it, which
  is the first time that cap has been tested rather than derived. **So the
  authored range 3–5 contained a value that can never pass**, and a per-seed
  draw across it — which my own character-not-existence rule would otherwise
  invite — would ship guaranteed-failing worlds. The range narrows.
- **Three fails for a structural reason that is not about shape at all: THE
  GENERATOR INPUT AND THE INVARIANT FLOOR WERE THE SAME NUMBER.** The massif
  had exactly 3 corners and I7 requires ≥ 3 detected, so a single missed
  detection fails by construction. **General rule: a generator input must
  never equal the floor of the invariant that checks it** — that is not a
  margin, it is a coincidence, and every measurement error lands on the
  failing side of it.
- **Sixth instance of the range family.** `arete_count` was pinned at
  `L0_ARETE_COUNT_MIN` — one bound of a range read as the range, exactly as
  the tor derived its base from `SUMMIT_TOR_HEIGHT_MIN`. See §5's "a range is
  two assertions".

All values **предложение — утвердить**. Full table with units and
justifications is handed to the lead with this ruling.

#### 2.8.7 THE FRAME REFUTED THE SUITE — nine invariants, none of which can see

**Seven of eight invariants passed and the mountain is still a dome. I have
looked at the frame myself and I confirm it independently of render and the
lead: it is a low, smooth, convex arc with a grey cap and a green shoulder.
Zero crest lines, not the three §7.1 requires. One material band, not a
rhythm.** Per §7.1's standing clause the frames outrank the numbers, so **the
"7 of 8" status is WITHDRAWN as a description of the mountain.** It remains
true as a description of the tests, which is now the problem.

**The lighting excuse is dead, and I record that I reached for it first.** My
first hypothesis was that a backlit frame flattens every internal structure
into one dark mass and that the read was confounded. The frame refutes it:
there is ample illumination and clear grey-on-green material separation. The
geometry is a smooth hump. **Had I ruled from render's prose instead of
opening the image, I would have sent back a lighting question and cost the
project another round.** Look at the artefact — the same rule that governs
perimeter digitisers and foliage bounding boxes governs me.

**THE SYSTEMATIC DEFECT: all nine invariants measure the OBJECT; none measures
the VIEW.** Contours, slope histograms, aspect turns along arcs, radial
profiles, perimeter ratios — every one is computed on the heightfield from
above or around it. **Not one of them is evaluated from a camera at
`PLAYER_EYE_HEIGHT`.** Meanwhile §7.1's acceptance criterion has always been a
frame. So the suite and the acceptance test were written **in different
spaces**, and a shape can satisfy every member of one while failing the other.
That is not a bad threshold anywhere; it is a missing dimension everywhere,
and it explains the whole discrepancy without any individual invariant being
wrong.

**Two new invariants. The first is the one I most regret not having.**

- **I10 — MASSIF ASPECT (scale, which every other invariant is blind to).**
  Every existing invariant is **scale-free** — ratios, angles, distributions,
  normalised perimeters. All nine are satisfiable on a pancake, and Ravenscar
  is a pancake: **115 m of relief over a 180 m base radius**, a mean envelope
  slope of **≈ 33°**. Rule: **above `MASSIF_CLIFFLINE_FRAC`, the massif's mean
  envelope slope (relief over radial run) must reach `SLOPE_ROCK_MIN`.**
  - **The derivation, so this is not a number I invented:** §4 paints rock at
    ≥ 40° and grass below it. A massif whose envelope sits under that
    threshold **will be painted as a grassy hill by the material system no
    matter what its geometry does** — and that is precisely the frame: a green
    shoulder with one grey cap. The shape rule and the splat rule must agree,
    or the mountain loses the argument to the shader.
  - **Consequence, predicted not assumed — core measures it.** At 115 m of
    relief the base radius has to come in from 180 m. Cascades onto the barrow
    (radius 103 m), the castle spur, the pine strips and the ascent length, so
    it is re-validated per §7.0a's rule, not patched.
  - **The LR is worse and is the cheap one to fix.** 280 m over a 600–700 m
    base radius is an envelope of **≈ 23°** — flatter than Ravenscar. It does
    not exist yet, so fixing `LR_BASE_RADIUS` now costs nothing, and building
    it first would have produced this same session a third time.
- **I11 — SILHOUETTE BREAKS (the eye's test, made executable).** §7.1 has
  always demanded "at least three crest lines readable at 640×360" and that
  criterion **was never implemented** — it sat in prose while nine other
  criteria ran in code. Rule: from standpoints on a ring at the acceptance
  distance, extract the massif's **horizon polyline against sky** and require
  at least three **tangent breaks** in it, each subtending at least
  `SILHOUETTE_MIN_PX`. This is the only invariant in the suite computed from a
  camera, and it is the one that would have failed on day one.
  - **Why plan-view aspect turn (I7) does not imply a visible rib.** I7 finds
    four arêtes and the eye finds none, and both are correct. With only 3–5
    arêtes, a rib lies near the **limb** for a minority of bearings; from most
    viewpoints the outline is traced by a **facet**, whose profile is the
    smooth curve. So ribs read as **value structure on the body**, not as
    silhouette — except where a break is large enough to notch the outline.
    I7 measures a property the eye cannot see from the ground; I11 measures
    the property it can.
  - **Suspected second cause, to be measured before anything is tuned:**
    §1.5 puts the readable feature size at ≈ distance/30, so an arête must
    stand **≈ 13 m proud at 400 m** and ≈ 24 m at 717 m. On paper
    `MASSIF_RADIAL_LOBE_AMP` should deliver that; whether the built surface
    does is unknown, because I8 reports a normalised perimeter ratio and
    **nobody has measured the raw radial excursion in metres.** A lobe ratio
    of 1.36 says nothing about whether the lobes are visible.

**What this costs me, said plainly.** I wrote nine invariants, ruled on their
weightings twice, corrected their baseline, added a marginal-pass rule, and
none of that was worth as much as one screenshot. The invariants were not
useless — I1 through I8 each caught something real, and the mountain is
genuinely better than it was. But **they were a proxy for a judgement, and I
let the proxy accumulate authority it had not earned**, to the point where "7
of 8" was being relayed upward as the state of the world. The rule that saved
it was one I inherited and nearly did not honour: *the frames outrank the
numbers.* It only works if someone actually shoots the frame early, and this
suite ran for two sessions before anyone did.

## 3. Water

Water does not exist in the engine yet; this section is the contract for its
first implementation (worldgen data + render/physics consumers decide their
own representations).

### 3.1 Rivers — must flow downhill

Compatible with our heightmap pipeline (fBm has no drainage; we impose it):

1. **Source:** deterministic argmax of the macro heightfield within the L0
   massif footprint, on a coarse 16 m grid.
2. **Descent trace:** greedy steepest-descent on the coarse grid with momentum
   (previous direction weighted in) to avoid zigzag; on hitting a local
   minimum: pond-and-spill (fill to the lowest saddle, continue from the
   spill) — Amit Patel's mapgen approach adapted to grids. Trace ends at the
   region/testbed edge or a lake.
3. **Smooth & resample:** Chaikin-smooth the polyline, resample stations every
   4 m; enforce target sinuosity ≥ 1.15 by amplitude of smoothing noise.
4. **Monotonic water surface:** station water height = min(previous station,
   local terrain) — **the water surface never gains height downstream. This is
   the invariant; a river that climbs is a failed seed.**
5. **Carve:** trapezoid cross-section, depth `RIVER_DEPTH` = 1.5 m, width
   `RIVER_WIDTH` = 4–8 m growing from source to mouth, bank blend 2× width
   **(предложение — утвердить)**.
6. **Fords:** wherever a corridor (§2.4) crosses the river, *raise* the bed —
   a true raise, clamping the bed into the trapezoid band, not merely a limit
   on the carve — so depth ≤ `FORD_DEPTH_MAX` = 0.4 m. The ford-shallow zone
   covers **every station inside the corridor mask**, not just the crossing
   station: oblique crossings otherwise dip into neighboring stations' blend
   zones (stage-3b measured 0.48 m exactly that way). Additionally at least
   one ford per `FORD_SPACING` = 200–400 m of river length
   **(предложение — утвердить)**. Validation invariant (tested):
   max water depth inside any corridor = `FORD_DEPTH_MAX`. Until swimming
   exists (FUTURE), a river without fords is an illegal hard wall — rivers
   *shape* routes, never sever the POI graph.

### 3.2 Lakes and ponds

- A lake is a stamped basin: radial depression to `LAKE_DEPTH_MAX` = 4 m with
  a flat water plane at its rim-min height; the river may terminate in it or
  exit at its spill point. Ponds (steps 2 pond-and-spill) are the same at
  radius 5–15 m.
- Testbed carries 1 lake (§7); region density FUTURE (needs the moisture
  field).
- Lake shores are prime real estate: the settlement rule (§3.4) and shore
  treatment (§3.3) both key off the same dist-to-water field.

### 3.3 Shoreline treatment

P3 computes per-sample `dist_to_water` (horizontal) and `height_above_water`.
Shore mask = `dist_to_water ≤ 6 m AND height_above_water ≤ 0.6 m`
**(предложение — утвердить: `SHORE_SAND_DIST` = 6 m,
`SHORE_SAND_HEIGHT` = 0.6 m)** → sand splat (§4), no grass micro, loose
stones ×2 density. Fords widen the sand band to both banks — the sand patch
*is* the ford's visual signpost (readable value contrast at distance: pale
sand vs dark water).

**Bed/mud band is hard-capped.** The full non-grass water treatment is:
submerged terrain (bed) + the sand band above — nothing else. Any
bed/mud/"WaterBed" classification band must stay within
`max(SHORE_SAND_DIST, 2 × local river width)` of the water edge
(≤ 16 m at max testbed width). Render's stage-3b probe measured the current
classifier at 2.74 % of the world against ~1 % actual water, producing
30–60 m mud flats that dominate riverside compositions — that is a violation
of this cap, fix on the classifier side (core). Wide dry flats are a *desert*
biome tool (FUTURE), never a default river treatment.

**`dist_to_water` field range.** The P3 field must be valid to at least
`SETTLEMENT_WATER_DIST` (150 m) — the P4 site scorer and §3.4 rules consume
it at that range. A 60 m saturation cap (observed in stage-3b probes) breaks
settlement scoring; saturate at ≥ 150 m **(предложение — утвердить:
`DIST_TO_WATER_RANGE` = 150 m minimum)**.

### 3.4 Water and gameplay placement

- Settlements stand within `SETTLEMENT_WATER_DIST` = 150 m of fresh water
  **(предложение — утвердить)**; the P4 site scorer weights river-bend
  outer banks and lake shores highest (flat, water-adjacent, defensible view).
- Water is a natural POI-chain edge: a ford or a bridge (FUTURE) is itself an
  L2 guide.
- Dungeons love water edges (lakeshore cave) — 1 of the 3 testbed dungeons
  keys to water (§7).

---

## 4. Terrain palette (splat rules a shader can implement)

Inputs per sample, all computable in P3 or in-shader from the heightfield:
`height` (m), `slope` (rad), `dist_to_water`, `height_above_water`.
Priority order (first match wins), thresholds **предложение — утвердить**:

| Priority | Material | Rule | Constant names |
|---|---|---|---|
| 1 | **Sand** | shore mask: `dist_to_water ≤ 6` and `height_above_water ≤ 0.6` | `SHORE_SAND_DIST`, `SHORE_SAND_HEIGHT` |
| 2 | **Rock** | `slope ≥ 0.70 rad (40°)` — hard rock, also all L0 crag stamp area above its rockline | `SLOPE_ROCK_MIN` |
| 3 | **Grass→rock blend** | `0.52–0.70 rad (30°–40°)`: dithered blend (fits the palette look better than smooth lerp at low res) | `SLOPE_GRASS_MAX` = 0.52 |
| 4 | **Grass** | everything else below the treeline band | — |
| FUTURE | Dirt/path | road pass | — |
| FUTURE | Snow | region mountains above snowline | `SNOWLINE_HEIGHT` (region) |

Design rationale, binding for render:

- **Visual = gameplay truth.** `PLAYER_MAX_SLOPE` is 0.87 rad (~50°). The
  rock material starts at 40° so that by the time ground *looks* fully rock,
  it is nearly unwalkable; grass is always walkable. The player learns the
  material language instead of testing every slope. Never let grass render on
  slopes above `PLAYER_MAX_SLOPE`.
- **Dither where the geometry is smooth, SNAP where the geometry has an
  edge.** At 640×360 a soft blend band reads as smear, so ordered dither is
  the default and it matches the retro look while keeping the palette small
  (sync №2's palette flag). But **dithering across a real discontinuity smears
  the one line that discontinuity exists to produce** — a 55° cliff riser
  meeting a 20° bench (§2.8) is a genuine crease, not a gradient, and must be
  drawn as one. **Stated as the general rule rather than as a massif
  exception (render's reframing, and it is the better statement):** this is
  not a carve-out from the dither rule, it is the dither rule applied to a
  surface that has creases in it. It therefore generalises for free to
  everything else with an edge — quarry faces, cut terraces, the castle pad's
  cut, cave mouths.
  - **Mechanism (render, measured cheap):** screen-space derivative of slope.
    Where `fwidth(slope)` is small the ordered dither runs unchanged; where it
    spikes — 35° of slope change inside a pixel or two — the material boundary
    snaps. A couple of ALU instructions, no new data from core, no memory, and
    **no constant from design**.
  - **The threshold is render's and is set by looking, not by arithmetic.**
    Its natural unit is degrees of slope change *per pixel*, which is
    resolution-dependent — and `INTERNAL_RES` is a user graphics setting
    (sync №3), not a constant. A number derived here would be wrong at
    320×180. Tune it against a 640×360 frame of a band lip.
- **Treeline (region, FUTURE for testbed):** trees stop at
  `TREELINE_HEIGHT` (region-scale, ~180 m proposal) and the grass→rock blend
  shifts 5° earlier above it, giving bald summits — the classic
  mountain-meets-sky read. The testbed's 64 m ceiling has no treeline; the
  crag gets bald via its rock stamp instead.
- Max 4 materials in the splat at once (render budget + palette discipline).

---

## 5. Flora catalog

Global rules: every species is low-poly with 2–3 flat colors and strong value
separation (readability §1.5). **Foliage is the one exception to
"hard-edged mesh" (stage-4, user direction via flora):** «кроны не шариками, а
с листвой… плоскими прозрачными большими плоскими наборами листочков… хочу
чтобы сквозь листву можно было смотреть» — tree foliage is **flat
alpha-cutout cards** carrying leaf clusters, and the canopy is **see-through**.
Trunks, branches, bushes, logs and snags stay solid hard-edged meshes. What
does **not** change: the crown *envelope* still governs, so oak/pine/birch
remain separable by outline at `SILHOUETTE_MIN_PX`, and every size band,
crown-base fraction, spacing and density in this section is unaffected —
cards are a surface treatment inside the same envelope, not a new silhouette
language.

**The exception survives, but NOT on the reason it was granted (measured,
flora — see §1.3).** It was granted on «хочу чтобы сквозь листву можно было
смотреть», read as *transparency*. Measured against the user's own reference
photographs, the tracery in those images is **not** visible because sky shows
through it: luminance is branch 50, leaf 135, sky 235, i.e.
**branch : leaf ≈ 2.54×**. The skeleton reads because it is **the darkest
thing in frame against a bright backlit leaf field** — which is §1.5's
value-contrast doctrine, not an alpha effect. So: **dark limbs plus bright
foliage reproduce the reference; see-through cards do not.** Cards remain the
right way to build a foliage surface, and **nobody may widen the exception on
the theory that more transparency buys more of the reference look — it buys
almost none of it.** This strengthens the envelope wording above rather than
straining it.

Two further consequences to keep straight: our C1 occlusion model needs **no
change at all** — measured crown interiors are 79–86 % leaf, so the
solid-crown effective width was already close to right (§1.3), and the
one-time physics-correction budget stays unspent; and **dappled light under a
canopy is a lighting problem, not a geometry one**, because a shadow caster
thinner than render's caster floor (§2.8.4) will read as solid no matter how
open the card looks. Placement is Poisson-disc /
jittered-lattice per §2 (never raw high-frequency noise threshold — it
clumps). Trees never spawn on rock or sand splat, never inside building pads,
corridors, or water. Slope limit for all trees: `TREE_SLOPE_MAX` = 0.61 rad
(35°) **(предложение — утвердить)**. All densities предложение — утвердить.

**CROWN ASPECT CEILING — a checkable invariant, and the crown-base fraction
becomes DERIVED (ruling, stage-4; flora's finding, and the defect was in my
numbers).** The River Birch crown failed to read as a mass **four times across
two sessions**; flora stopped under Rule 28 rather than attempt a fifth
arrangement, and measured instead of describing. Generated foliage bounding
boxes — **corrected figures, see the pooling note below**: oak **1.53**
tall-to-wide reads as a mass, willow **1.37** reads as a mass, birch **2.30**
reads as a *column*, pine 4.23 and correct because a cone is meant to be
narrow.

> **The first table I ruled on was inflated ≈ 15 % and flora self-reported it
> (oak 1.65 → 1.53, willow 1.51 → 1.37, birch 2.65 → 2.30, pine 4.88 → 4.23).**
> They had pooled twelve size variants into one bounding box, so the
> **variant spread** — birches are 16–22 m tall — was being measured as if it
> were one crown's shape. **The diagnosis is untouched:** the birch was still a
> column, oak and willow still read, and the true band is "1.53 reads, 2.30
> does not", which still brackets the ceiling. **Measure per variant, never
> pooled** now sits beside *measure the artefact, not the intention* — flora
> got the second right and the first wrong in the same table, which is how
> closely related these two failures are.
>
> **`CROWN_ASPECT_MAX` landed at 2.0, not the 1.8 written here, and I am
> ACCEPTING 2.0 rather than re-litigating it.** Reasons, so the discrepancy is
> closed rather than left: the ceiling **binds no species** — every built tree
> now measures ≤ 1.28 against it — so it is a guard rail for future entries,
> not a live constraint; the whole difference between 1.8 and 2.0 lies inside
> the band my own note already flagged as untested; and reopening a landed
> constant to move a non-binding guard rail by 0.2 costs more churn than it
> buys. Flora implementing against the landed value rather than my prose is
> **correct under Rule 14** and is the behaviour I want. **Tightening trigger,
> recorded so this needs no argument later: if any species lands between 1.8
> and 2.0 and reads as a column, the ceiling moves to 1.8 on that evidence
> alone.** Three previous fixes all changed *what goes in the box*
while **the box was the defect** — the same diagnosis §2.8.1 reached about the
mountain, twice in one stage: **a shape failure that lives in the authored
container cannot be fixed by anything that fills it.**

- **Rule: a broadleaf crown's GENERATED foliage bounding box may not exceed
  `CROWN_ASPECT_MAX` = 1.8 tall-to-wide (предложение — утвердить).** Measured
  on the built geometry, never on the authored container — the birch's
  container is 1.8 : 1 and its *generated* box is 2.65 : 1, so a rule checked
  against the spec would have passed the tree that fails. Same discipline as
  §2.8.3's polyline perimeter: **measure the artefact, not the intention.**
- **Species whose silhouette brief is a cone or spire are exempt** — pine at
  4.88 is not a defect, it is the anti-oak. The exemption is a property of the
  written brief, not a free pass anyone may claim later.
- **1.8 is provisional and I am saying so rather than dressing it as
  measured.** It sits above every value that reads (1.65) and well below the
  one that does not (2.65); **the band between 1.8 and 2.65 is untested.** As
  with §7.1, the frame outranks the number: if something at 1.7 still reads as
  a column, the ceiling moves, not the tree.
- **`CROWN_BASE_FRACTION` stops being a universal cap and becomes a FLOOR plus
  a derivation.** This is the real fix, and it dissolves flora's per-species /
  principle question rather than answering it. The 0.35–0.45 band was carrying
  **two unrelated jobs**: clear trunk height (a walkability and feel goal —
  §1.3 measured that crown base is *visibility*-insensitive to three decimals)
  and, by accident of being a fraction of height, the crown's aspect ratio (a
  silhouette property). Split them: `CROWN_BASE_FRACTION_MIN` = 0.35 stays as
  the walkability **floor** — more clear trunk is never worse for walking
  under — and each species' crown base is then **derived as the smallest value
  ≥ that floor which satisfies `CROWN_ASPECT_MAX`**. Oak and willow land
  inside the old band untouched; the birch lands at ≈ 0.58–0.62, which is
  flora's recommended remedy **reached by principle instead of by exception**.
  `CROWN_BASE_FRACTION_MAX` = 0.45 survives as documentation of the typical
  outcome for broad crowns, not as a binding cap.
- **Why the birch is free to change:** the 5–7 m width band is untouched, so
  `TREE_SPACING_FOREST` — which was derived *from* crown width — does not
  move; the accent role ("smallest and slimmest of the three") is strengthened;
  and clear trunk rises 8.5 → ≈ 11 m, which is §5.7's own stated goal. See
  §5.3.
- **A RANGE IS TWO ASSERTIONS, AND A SUITE THAT TESTS ONE BOUND IS GREEN WHILE
  THE WORLD DRIFTS OUT THE OTHER SIDE (general rule, from flora's second
  finding).** The birch's crown had drifted to **3.6–4.5 m against this
  document's 5–7 m brief** — a third narrower than specified — with a fully
  green suite the whole time, because only the *ceiling* of the width band was
  ever asserted. That was the other half of why it read as a column, and it
  means the rule was not wrong, it was **half-implemented**. This is now the
  fourth appearance of a range-handling failure this stage (core's tor derived
  a base from `_MIN` while drawing heights across `_MIN…_MAX`, and they report
  four earlier instances in their own zone), so it is written down as a family
  rather than as four anecdotes: **every `_MIN`/`_MAX` pair in this document is
  two separate claims about the world, and a validation that checks one of them
  is not checking the range.**
- **Widths are calibrated against the BUILT tree, not the envelope** (flora,
  accepted): foliage never reaches the envelope's widest point, because
  containment holds a *card's corner* inside and the widest ring sits where a
  card would overshoot the crown top, so achieved width is ≈ 0.7–0.9 of
  nominal. Same discipline as the aspect ceiling, one level down.
- **The aspect ceiling is asserted at NOMINAL size only, deliberately, and
  flora was right to draw that line.** §5.8's maturity tiers scale trees
  ×0.4–1.5, which takes crowns outside any absolute band *by construction and
  on purpose*; a per-instance width assertion would be a rule forbidding this
  document's own tiers. The aspect *ratio* is scale-invariant and therefore
  still meaningful per instance — the width *band* is not.
- **Where each half lives:** the invariant is a design acceptance criterion
  and lives here, exactly as §2.8.3's do. How the generator satisfies it —
  cluster placement, card layout, the derivation itself — is flora's spec.
  Same split as the massif: design owns the test, the zone owns the mechanism.

### 5.1 Dale Oak (broadleaf — the valley tree)

- **Silhouette:** short thick trunk (1/3 of height), one wide rounded crown
  mass, wider than tall overall. Reads as "ball on a stump" at 8 px.
- **Size (stage-4 revision, §5.7):** **24–32 m** tall, crown 10–16 m wide.
  Crown base at 35–45 % of height (≈ 9–13 m of clear trunk) — the height is
  only half the effect, the *space underneath* is the other half.
- **Poly budget:** 300–500 tris (`TREE_TRI_BUDGET` family).
- **Palette:** mid-green crown, near-black trunk; value: darker than meadow
  grass, lighter than pines.
- **Placement:** valley floors and slopes < 20°; the default forest-mass tree
  (S/SE forest masses); Poisson spacing 5–8 m inside masses
  (`TREE_SPACING_FOREST`), clusters of 5–15 in meadows.
- **Clustering:** high — oaks define forest interiors and edges.

### 5.2 Highland Pine (conifer — the slope tree)

- **Silhouette:** narrow triangle, 2–3 stacked cone tiers, tip must survive
  quantization (make the top cone ≥ 1.5 m wide). Tall and pointed — the
  anti-oak.
- **Size (stage-4 revision, §5.7):** **28–38 m** tall, base 6–9 m wide —
  ×2.3, deliberately **not** ×4 (see §5.7 for why 4× breaks the valley).
- **Poly budget:** 150–300 tris (cones are cheap).
- **Palette:** dark blue-green, darkest flora value in the scene — pines are
  the *dark mass* tool for composition backdrops.
- **Placement:** slopes 10–35°, higher terrain, crag foothills and northern
  ridges; spacing 4–7 m; follows ridgelines in strips 20–60 m wide (great for
  leading lines toward the L0). Pines are the main C4 hazard: 12–18 m of
  canopy on foothills out-angles the L0 from valley standpoints — pine strips
  on landmark-facing shoulders are subject to the L0 sight-wedge filter (§1.3)
  before anything else.
- **Clustering:** strips and wedges, not blobs; a lone skyline pine is a
  legitimate L2 guide.

### 5.3 River Birch (accent — the water tree)

- **Silhouette:** slim pale trunk (readable!), small loose crown, slightly
  leaning. The *light-value* accent against dark water or pines.
- **Size (stage-4 revision, §5.7):** **16–22 m** tall, crown 5–7 m — stays
  the smallest and slimmest of the three, keeping its accent role.
- **Poly budget:** 200–350 tris (trunk needs a few more sides for the pale
  read).
- **Palette:** near-white trunk (brightest flora value), light yellow-green
  crown.
- **Placement:** within 20 m of water only (`BIRCH_WATER_DIST` = 20 m),
  clusters of 3–7; marks rivers/lake at distance — a birch line = water line.
- **Clustering:** loose lines along banks; never deep forest.
- **Crown base ≈ 0.58–0.62 of height, DERIVED not authored (ruling, stage-4).**
  The birch crown failed to read as a mass four times across two sessions and
  flora stopped it under Rule 28 rather than trying a fifth arrangement. They
  were right, and **the defect was in my numbers, not their geometry** — see
  the crown-aspect rule in §5. Two of my rules multiplied: a 5–7 m crown width
  and a crown base at `CROWN_BASE_FRACTION_MAX` = 0.45 of a 16–22 m height
  give a container **≈ 5.7 m wide by 10.5 m tall — 1.8 : 1 before a single
  leaf cluster is placed**, and the fill takes the measured foliage box to
  **2.65 : 1**. No arrangement of contents can make that read as a rounded
  mass. My own silhouette brief above says "small loose crown", and a crown
  occupying the top 55 % of the tree is not small: **the written intent and
  the numbers disagreed, and the numbers won.** Raising the crown base is free
  of everything I value here — the 5–7 m width band is untouched, so
  `TREE_SPACING_FOREST` (derived from crown width) does not move, the "smallest
  and slimmest of the three" accent role is *strengthened*, and clear trunk
  rises from ≈ 8.5 m to ≈ 11 m, which is §5.7's own goal.

### 5.4 Bush

- **Silhouette:** 1–1.5 m hemisphere lump; groups of 2–4 read better than
  singles. **Poly budget:** 60–120 tris. **Palette:** oak-green, slightly
  lighter.
- **Placement:** forest-mask edges (≤ 10 m outside) and clearing rims,
  0.01–0.03 /m² there (`BUSH_EDGE_DENSITY`); softens the tree/meadow
  boundary so forest masses do not read as walls of sticks.

### 5.5 Flowers

- **Silhouette:** 2-quad cross billboards, 8–16 tris, height ≤ 0.5 m.
- **Palette:** one accent hue per patch (white/red/blue family); the only
  saturated accents in open meadow — they read as L2 guides.
- **Placement:** patch system per §2.3 (`FLOWER_PATCH_*`); open grass splat
  only, slope < 15°, never under canopy, never in corridors' driving line
  (they may edge it — flowers *beside* the path pull the eye forward).

### 5.6 Grass

- **Silhouette:** simple card tufts, ≤ 0.4 m (`GRASS_HEIGHT_MAX`), 2 tris per
  card. **Palette:** exactly the underlying splat color family ±1 value step —
  grass is texture, not information.
- **Placement:** render-side within `GRASS_VIEW_DISTANCE` = 50 m,
  `GRASS_DENSITY` 0.5–1.5 /m² on grass splat only (§2.3); density fades to 0
  at the view distance edge (no popping line at low res — dither the fade).

---

### 5.7 Tall-tree revision — working the collisions through (stage-4)

User: trees ×4 taller, forests less dense, "как в Скайриме". Taken seriously,
that request collides with four existing rules at once. Working it through
rather than approving it:

**What the request is actually about.** Our 8 m oak has its crown starting at
≈ 2.7 m: the player pushes *through* foliage, which reads as scrub. What
makes Skyrim's forests feel tall is walking **under a canopy** — clear trunk
space overhead and light between stems. So the fix is three-part and the
height is only the first part: raise the trees, **raise the crown base**, and
**drop the density**. Height alone would have given us taller scrub.

**Ruling on the numbers.** Oak 8–12 → **24–32 m** (the requested ×4 at the
low end). Birch 6–10 → **16–22 m**. Pine 12–18 → **28–38 m**, which is ×2.3
and **declines the literal ×4**, because:

- A ×4 pine is 48–72 m. Standing on a 25 m foothill that is a 73–97 m crown
  top against Ravenscar's 52 m summit: **the forest would be taller than the
  landmark it frames.** Canopy-aware C1 (§1.3) would fail everywhere and no
  strip geometry could save it.
- Skyrim's own tall conifers are ≈ 25–30 m. The literal ×4 overshoots the
  reference the request cites.

**Collision 1 — the landmark must out-top its own forest.** A 35 m tree on a
25 m foothill tops out at 60 m against a 52 m summit. Two consequences, both
binding:

- **Ravenscar must grow with its forest:** `L0_RELIEF` 52 → **110–120 m**
  — **APPROVED by the user**. The argument is composition, not validation:
  a valley heart that its own trees overtop is not a landmark. (The C1 case
  once made for this raise rested on contaminated numbers; the corrected
  measurement shows the raise *improves* C1 to ≈ 0.865 rather than costing
  anything — see the withdrawn finding in §1.3. Right answer, and now for a
  reason that survives.) All castle rules are ratios to the peak
  (`CASTLE_SKYLINE_MARGIN` etc.) and re-derive automatically — story's ~55 m
  castle/barrow geometry is unaffected.
- **L0/LR sight wedges — revised (flora's pushback accepted, with its
  reasoning corrected).** My first draft banned *all* trees in a wedge. Flora
  is right that at 12–18 m spacing this carves mown lanes radiating from every
  POI — authored-looking, the exact effect standing stones exist to create
  deliberately. The rule instead:
  - **RE-RULED after the C1 correction (flora asked for the re-decision, and
    was right to).** Both wedge constraints were justified partly by a
    headroom crisis that turned out to be a validation bug (§1.3). Rather than
    inherit rules whose premise evaporated, the wedge now uses **one test,
    the same one already governing the castle**: *no tree may occlude the
    L0/LR's **crown** (top third) from any corridor standpoint; occluding its
    **flank** is permitted.* One notion of acceptable occlusion across
    architecture and vegetation, and it needs no near/far half-split.
  - **Giants are ALLOWED in sight wedges** — reversing my exclusion. Flora's
    compositional argument is the stronger one: a single elder standing in the
    middle distance, off the axis, **gives the landmark scale**, which is the
    thing a distant landmark most needs and most rarely gets. That is
    repoussoir, and removing it deletes our best depth cue exactly where depth
    matters most. Conditions: subject to the crown rule above, and **at most
    one giant per wedge** — one elder frames, three elders screen.
  - **Sharpening of C4 that this exposed:** C4 governs **masses and built
    structures**, not individual near vegetation. A 48 m crown 100 m away
    subtends more than the mountain behind it and that is *fine* — nobody
    mistakes a nearby tree for a distant massif; the eye reads the depth cue
    correctly. C4's real target was always the foothill pine *wall*, a mass.
    Apply it to masses.
  - Bushes, saplings and anything under ≈ 8 m were never restricted and still
    are not — the wedge keeps its ground texture and reads as young growth
    under an old canopy, never as a mown lane.
  - **Fallback if per-tree crown testing proves expensive:** the previous
    conservative rule (tall three banned in the near half) is an acceptable
    approximation — but it is the fallback, not the intent.

### 5.8 Maturity tiers — restoring fullness without restoring canopy

Approved (flora's proposal): every instance carries a maturity scalar rather
than standing at species nominal size, because 44 stems/ha all identical reads
as a **plantation**, and a plantation is a different failure from an empty
field but a failure all the same.

**Revised (user: «гигантским и могучим… мелкие деревья очень редкие»):**

| Tier | Share | Size | Placement |
|---|---|---|---|
| Giant / **Elder** | **25 %** | ×1.5 nominal | the "могучий" read; also L2 guides (§1.3) — a forest with internal hierarchy is legible from inside |
| Mature | **60 %** | nominal | the main lattice |
| Sub-mature | **12 %** | ×0.7–0.85 | mid-canopy layering on a half-spacing sub-lattice |
| Sapling | **3 %** | ×0.4–0.6 | deliberately rare — a nursery is what the user rejected |

Shares предложение — утвердить. **Why not flora's 25/67/8:** their reasoning
was right that the young tier's *ground-level* fill job is better done by the
new bushes, logs and snags. But young trees at ×0.5–0.7 of a 28 m tree are
14–20 m — their crowns sit **above** eye level and **below** the main canopy,
so they were never doing ground fill; they were doing **mid-canopy layering**,
which is what makes a wood feel deep rather than like a colonnade with debris
on the floor. Deleting them wholesale would flatten the vertical section. So
the tier splits: the genuinely small (sapling, ×0.4–0.6) drops to 3 % —
satisfying «очень редкие» — while a sub-mature band survives at 12 % to keep
the middle of the section populated. Ground fill moves to §5.9's classes,
exactly as flora argued.

**The "Elder Oak" proposed as a separate species IS the giant tier** — one
system, not two: a maturity scalar on the existing parameter set, not a
catalog row. Giants are excluded from sight wedges entirely (§5.7).

### 5.9 Additional species (approved from flora's proposals)

**Standing snag (dead trunk).** Approved, and the sharpest idea in the batch:
a barkless broken column with no crown is **the only flora that may legally
stand at full height inside a sight wedge** — nothing to out-angle with.
30–60 tris, the cheapest asset in the project.

**Density revision (user endorsed dead trees; flora flagged the tension
honestly rather than letting it be overwritten).** My original rarity existed
because pale dead wood is the highest flora value in the scene and a common
snag becomes a false L2 guide. That reasoning does not stop being true because
more were requested — so the fix is to **split the material, which turns the
tension into a feature**:

| Where | Density | Value | Reads as |
|---|---|---|---|
| Inside forest masses | `SNAG_DENSITY_FOREST` = 1.5–3 / ha | weathered grey-brown | texture and atmosphere; one every ~30–80 m |
| Open ground | `SNAG_DENSITY_OPEN` = 0.25–0.5 / ha (unchanged) | pale bone | a deliberate vertical accent — a legitimate L2 guide |

**(предложение — утвердить.)** A pale snag standing alone in a meadow is a
landmark; a grey snag in a wood is weather. Same asset, two jobs, and the
composition rule survives contact with the user's request instead of being
quietly traded away. Prefer old stands, edges, clearing rims, scarp tops, and
ground near barrows and ruins where the atmosphere pays double.

**Vale willow / alder (riparian).** Approved, and it fixes a real flaw I had
left: *every* water body in the world is currently flagged by pale birch, so
all water reads the same. Two riparian species let water say two things —
ruling on the split: **birch marks moving, clear water** (river banks, fords),
**willow marks still or slow water** (lake shores, pond rims, slack bends).
Dark low mass, the value opposite of birch, 10–14 m, within
`RIPARIAN_WATER_DIST` (reuse the birch 20 m band). A player who learns that
dark drooping mass means still water has learned to read the landscape, which
is the whole point of a palette this small.

**Elder oak** — folded into §5.8's giant tier, not a separate species.

### 5.10 Forest floor classes (user-specified, stage-4)

The floor is what makes a wood feel *walked* rather than crossed, and at the
new density it is also what replaces the fill the young tier used to carry
(§5.8). All approved as distinct catalog classes.

**BigBush** — a class, **not a scaled Bush**, and flora is right about why: a
1.2 m bush is ground texture, a 3.5 m bush is an **obstacle that breaks a
sightline and makes the player pick a side**. That is forest-floor navigation.
2.5–4 m, denser silhouette, 120–180 tris. `BIGBUSH_DENSITY` = 8–15 / ha inside
masses, plus clearing rims, scarp bases and stream banks. **Never inside a
corridor mask** (they exist to be gone around, and the critical path is not
the place for that); may partially occlude but at 4 m they cannot threaten a
POI sightline at any meaningful range.

**FallenLog**, two classes as specified:

| Class | Size | Tris | Density | Rules |
|---|---|---|---|---|
| Big | 8–14 m long, ⌀ 0.8–1.4 m, half-sunk | 80–120 | `LOG_DENSITY_BIG` = 3–8 / ha | **excluded from corridors** until vaulting exists (⌀ exceeds `PLAYER_STEP_HEIGHT`, so it would be a wall on the critical path); collision on (sim) |
| Small deadfall | 2–4 m, ⌀ 0.35–0.5 m, half-sunk | ~40 | `LOG_DENSITY_SMALL` = 15–30 / ha | allowed anywhere including corridors — half-sunk it sits under step height |

**(предложение — утвердить.)** **Logs lie ACROSS the fall line, never along
it** — flora's geometric ask, approved with its reasoning made binding: a log
pointing downhill reads as a stick, a log lying across a slope reads as a
fallen tree and as something to climb over. Big logs are nearly free
geometrically (the trunk mesh, rotated and sunk) and are the cheapest "this
forest is old" signal in the medium. **FUTURE:** when sim adds vaulting, big
logs become legal in corridors and turn into pacing furniture.

**Trees on scarp edges — flora's question, ruled: YES, deliberately.** A tree
leaning out over a drop is a superb silhouette and a genuine L2 guide, and it
is exactly the kind of detail that still reads at 640×360. Constraints:
mature or giant tiers only (a sapling on a cliff reads as an accident, not a
statement); lean 10–20° outward, away from the mass; scarps ≥ 3 m only, since
below that there is no drama; **never inside a sight wedge** (a tall occluder
on a high edge is the worst case we have). Rare by design — one per scarp
segment, never a row: a row reads as planted.

**Setback corrected (flora's root-flare finding).** The ≥ 1.5 m root-plate
setback is measured **from the outer edge of the root flare, not the trunk
axis** — with a 1.6× flare on a 1.2 m trunk the flare radius is ≈ 0.96 m, so
the original wording left barely 0.5 m of ground beyond it and the tree would
still have floated the first time the scarp was voxelised. The rule was right;
the datum was wrong.

**The lean here is a SEPARATE, larger parameter than the crowding lean** —
flora's note, adopted: crowding lean caps near 0.12 rad, and reusing it on a
cliff edge produces a limp tree instead of a statement.

**Collision 2 — under-canopy walkability.** `CANOPY_CLEARANCE_MIN` = 2.2 m is
a floor, satisfied ~4× over by a crown base at 35–45 % of height. Stated
explicitly so no future species regresses: **every tree species must carry
≥ 2.2 m of clear trunk**, and the tall species must target the 35–45 % band
rather than merely clearing the floor.

**Collision 3 — density, numerically.** Crown width scales ~×1.6, not ×4 —
tall *and* slender is what makes a forest feel tall. Spacing must open up
accordingly: `TREE_SPACING_FOREST` 5–8 m → **12–18 m**
**(предложение — утвердить)**. In trees per hectare that is ≈ 240 → ≈ 44, an
**≈ 80 % density cut** — which is the user's "less dense" expressed as a
number core can implement. `FOREST_COVERAGE` (0.25–0.40) is **unchanged**:
coverage is how much land is forest, density is how many trees stand in it,
and only the second was wrong.

**Collision 4 — budgets.** A 4× taller tree does not need 4× the triangles;
it needs a silhouette that survives being large on screen. `TREE_TRI_BUDGET`
500 → **700 max** for the tall species, but the real answer is LOD, not base
budget: each species ships LOD1 at ≈ 40 % tris and a billboard beyond
`TREE_BILLBOARD_DIST` **(предложение — утвердить, value is render's call)**.
Shadow-map coverage is render's zone; design's constraint is only that a
35 m canopy must not black out the ground it shades — dappled, not sealed,
which the 80 % density cut largely delivers on its own.

**Net effect on composition:** fewer, taller, slimmer trees with light
between them. Forests stop being walls (§2.2's "trees are walls you can walk
through" was written for the dense version) and become colonnades — you see
*through* a forest now, which makes occlude-and-reveal work at a distance it
never could before.

### 5.11 Seasonal foliage — the palette contract (ruling, stage-4)

The user wants summer, autumn and winter. The seasons themselves are a future
game-design decision; **the palette SHAPE is a catalog decision and it is
cheaper to fix before there are entries in it than after** (flora's flag, and
they were right to raise it early). Ruling on the shape, adopting flora's
proposal because it adds no mechanism:

- **Foliage colour is a per-species palette indexed by SEASON.** The mesh
  stores an **index, not a colour**. A season change is then a data swap, not
  a re-export.
- **A palette entry is a RAMP, not a single value**, so "autumn varies more
  within a crown than summer does" is expressible as a wider ramp with no new
  machinery.
- **Winter costs one boolean, `has_foliage`**, in the same table: false for
  deciduous, true for conifers. **Conifers are season-stable apart from snow**
  (measured: pines stay green in every reference frame), and snow is render's
  and core's, not the catalog's.

**Two design constraints flora cannot see from their side, and they are the
reason this needed a ruling rather than an ack:**

1. **Value separation must hold in EVERY season, not just summer.** §1.5
   separates our species by *value*, not hue — pale birch, mid oak, dark pine.
   An autumn palette that turns oak and birch into two similar warm mid-values
   destroys that separation at `SILHOUETTE_MIN_PX`, and the forest stops being
   readable at distance in exactly one season. **Each season's entry must
   preserve the species' value ORDER.** That is a checkable property of the
   palette table and it is the acceptance test for any season anyone proposes.
2. **Winter opens sightlines, and C1 must not be validated against it.** With
   `has_foliage = false` a deciduous forest's effective width collapses toward
   trunk diameter, so transmittance rises sharply and landmarks become visible
   through woods that hide them in summer. That is a *lovely* seasonal read
   and we should keep it — but it makes visibility season-dependent.
   **RULING: C1, C4 and the sight wedges validate against the WORST case,
   full summer canopy.** Winter may only improve on a passing seed, never
   rescue a failing one. Decided now, in one line, so that no future seed
   passes in February and fails in July.

**Palette sourcing rule, binding:** **never calibrate a palette from
photographs** (flora's finding — the same tree in two frames gave leaf/dark
splits of 76/10 and 53/40 purely on exposure). Colour taken off a reference
photo is colour taken off the camera's metering. Reference photographs are
evidence about **structure** — density, value ratios, silhouette — and are not
evidence about hue. The same caution applies to every reference image this
project uses, not only to foliage.

## 6. Structures catalog (домики под разные задачи)

Global rules: structures are placed in P4 on flattened pads — pad = footprint
× 1.5, max original slope under pad 0.09 rad (5°)
**(предложение — утвердить: `BUILDING_PAD_SLOPE_MAX`)**; the pad flatten is a
worldgen terrain stamp (procedural, not hand sculpt). Door side faces the
hamlet common or the water/corridor. All buildings: hard-edged low-poly,
2–3 colors + roof color as the *purpose code* (readable at cluster distance).
Tri budgets предложение — утвердить (`HOUSE_TRI_BUDGET` family). Buildings
never spawn in corridors, on sand, or within 2 m of water surface height
(flood margin — `BUILDING_WATER_MARGIN` = 2 m vertical).

| Purpose | Footprint | Height / silhouette | Tris | Distance read | Placement |
|---|---|---|---|---|---|
| **Dwelling** (cottage) | 6×8 m | 5–6 m; single gable, one chimney | 200–400 | generic "house lump" — reads as part of cluster | hamlet only, 3–5 per hamlet |
| **Trader / shop** | 8×10 m | 6–7 m; gable + full-width porch awning, hanging sign | 300–500 | porch shadow line | hamlet, faces the common, adjacent to corridor entry |
| **Tavern** | 10×14 m | 8–9 m; two stories, L-shaped, two chimneys | 400–600 | the *big* roof of the cluster; FUTURE: smoke column = day/night guide | hamlet anchor: largest pad, at the common's head |
| **Storage / barn** | 8×12 m | 7–8 m; steep tall roof (roof = ⅔ of silhouette), full-height doors, no windows | 200–350 | tall dark roof triangle | hamlet edge or farmstead; rotated gable-on to prevailing view (distinct from dwellings) |
| **Shrine / temple** | 5×5 m | spire 10–14 m; smallest footprint, strongest vertical | 250–450 | breaks skyline — doubles as L1 landmark | *outside* the hamlet on a knoll within 100–250 m; skyline rule §1.5 |
| **Watchtower ruin** (L0 topper) | 4×4 m | 10–15 m broken cylinder/prism | 150–300 | crown of the L0 crag | exactly one, on the L0 (§7) |

**Grouping rules (предложение — утвердить):**

- **Hamlet** = 4–8 buildings (`HAMLET_SIZE`) around an open common of radius
  15–25 m (`HAMLET_COMMON_RADIUS`): 1 tavern + 1 trader + 3–5 dwellings +
  1–2 barns. Building spacing 4–10 m, orientations within ±30° of facing the
  common (regular enough to read as "village", irregular enough to not read
  as a grid).
- **Farmstead** = 1 dwelling + 1 barn, 8–15 m apart, common yard; the
  single-building variant in open land. Density: FUTURE for region (needs
  roads); testbed has none unless a POI-chain gap needs one.
- **Shrine** always solitary (never inside a hamlet) — it is a navigation
  instrument, not street furniture.
- The hamlet counts as **one** L1/POI regardless of building count.
- FUTURE: purpose distribution per settlement size (village, town) — after
  the gameplay grill on economy/quests.

---

### 6.1 Castle — the seat of state power (ruling, stage-3)

User request: the world needs a seat of state power, present even in the
minimal version. Story picked pitch A (*The Debt of Harrowmere*), so this is
**House Corvane's seat**, standing on the land whose barrow is a mass grave.
The crown's distant capital is referenced in fiction, not built (FUTURE).
Fiction constraint taken as given from story: castle on or beside the barrow —
that proximity is a designed asset. Interior and any town around it are **out
of scope here** (not designed, not blocked).

#### 6.1.1 The hierarchy ruling (the actual problem)

A castle is a weenie by construction, and Ravenscar Crag is our L0. Two
dominant silhouettes in one 1024×1024 valley must not compete. Ruling:

**The crag stays L0 and keeps the skyline. The castle is L1-max — the
strongest secondary landmark, staged *inside the crag's composition*, never
against it.** The mechanism is siting, not height limits alone:

- **R1 — Site the castle inside the L0's angular footprint.** From the
  valley's main standpoints (town, lake shore, corridors), the castle must lie
  within the crag's angular width, so it reads **against the crag's body,
  never against sky**. A silhouette that cannot reach the horizon cannot steal
  it. At the sited position this holds automatically: from Vaelmere the castle
  sits ≈ 85 m lateral of the town→peak line while the crag subtends ≈ ±0.38
  rad — the castle is a dark notch on the crag's flank, sub-threshold at that
  range by §1.5, and the crag alone crowns the valley.
- **R2 — The castle may occlude the L0's flank, never its crown.** Flank
  occlusion is the desired "one composition" read (fortress at the foot,
  mountain above). Crown occlusion (top third of the L0) from any
  C1-crediting standpoint is forbidden.
- **R3 — Skyline margin.** Castle top elevation ≤ L0 peak −
  `CASTLE_SKYLINE_MARGIN` = 12 m **(предложение — утвердить)**; at seed 1
  (peak 52 m) that is ≤ 40 m absolute, i.e. a pad at ≈ 24 m carries a keep of
  ≤ 15 m.
- **R4 — Dominance ratio.** From valley standpoints ≥ 300 m where both are
  visible, castle subtended height ≤ `CASTLE_SILHOUETTE_RATIO` = 0.6 × the
  L0's **(предложение — утвердить)**. Inside the final approach (< 300 m) the
  castle is *allowed* to fill the view — that is the reveal (§1.4), and the
  crag still crowns it because the castle stands on its foot.

**C1/C4 scoring — the castle counts BOTH ways, explicitly:**

1. **As an occluder:** its full mass enters the occlusion heightfield exactly
   like canopy (§1.3), and the `LANDMARK_CLEARANCE_FACTOR` = 1.2 test applies
   to it. **The castle may never be the reason the L0 fails C1.** Seed-1
   headroom is 0.018 (C1 = 0.618 vs floor 0.6) — effectively zero, so this is
   a real risk, not a formality.
2. **As an attractor:** it counts toward the C1 "≥ 1 attractor visible" test
   and toward `POI_VISIBLE_COUNT`'s upper bound of 3 (it can push a standpoint
   over the limit — check both directions).

**Fix order if inserting the castle drops C1 below the floor** (binding — do
not improvise): (1) lower the pad elevation, keeping tower height; (2) shift
the pad further around the crag's south flank, away from the town sightlines;
(3) reduce tower height last. **Never** raise the crag (proven at stage-3b to
*lower* clearance, §1.3) and never accept a C1 drop.

#### 6.1.2 Siting rules

- **Terrain:** a spur/bluff shoulder of the L0 massif — high enough to command,
  low enough for R3. Needs a terraced pad: `CASTLE_PAD_SIZE` = 60 m square,
  pad surface within `BUILDING_PAD_SLOPE_MAX` (5°), with a dedicated cut/fill
  allowance `CASTLE_PAD_CUT_MAX` = 6 m **(предложение — утвердить)** — a
  documented exception to §6's ordinary pads, because terracing a spur is what
  real fortification does. Pad edges blend over 1.5× pad size.
- **Water/ford:** the castle **commands the crossing** — its pad is sited so
  the nearest derived ford lies inside its field of view and within
  `CASTLE_FORD_COMMAND_DIST` = 250 m **(предложение — утвердить)**. It does
  not create or move the ford (fords stay derived, §7.1a).
- **Barrow:** `CASTLE_BARROW_DIST` = 40–80 m **(предложение — утвердить)**
  from the barrow entrance — close enough that both are in one frame from the
  approach. The seat literally stands over the grave.
- **Vaelmere:** ≈ 390 m as sited — deliberately **beyond** one
  `POI_TRAVEL_TIME` hop. The castle is not a neighbourhood building; you
  travel to it. Chain integrity is preserved by the composite-POI rule below.
- **Composite POI:** castle + barrow count as **one** POI ("the seat"),
  exactly as a hamlet counts as one regardless of building count (§6). This
  keeps the §7.2 chain valid without over-densifying that stretch.
- **Corridors:** a castle implies an approach. The existing watchpoint→barrow
  corridor becomes the castle approach; the gate faces it. **FUTURE:** an
  actual road along that corridor when roads exist.
- **Access invariant (story-mandated, binding on terrain):** the Ward must be
  enterable by an unarmoured commoner on foot, on ordinary business, in
  daylight. Therefore the terrace's **approach side carries a graded ramp that
  satisfies the §2.4 corridor rules end to end** — average slope ≤ 25°, no
  step > `PLAYER_STEP_HEIGHT` — from the corridor up to the gate threshold. A
  spur pad whose only approach is a scarp is a **failed placement**, not a
  detail to fix later: cut the ramp in the same terrace stamp. The remaining
  pad edges may stay steep (that is the fortification read).
- **Barrow sightline (story asset):** the line of sight from the Ward's yard
  and gate to the Backbarrow entrance must be **clear** — the terrace's own
  cut/fill may not occlude it. The beneficiary lives within sight of the
  evidence; that is a checkable raycast, not a mood note.
- **Gate orientation — settled, do not re-litigate:** the gate faces the
  valley/ford, **not** the barrow. It serves the access invariant (the gate
  sits on the walkable approach) and it is the truthful read: landlords
  watching the crossing they own, with the grave behind the house. Because the
  yard→barrow sightline above guarantees the barrow is visible from their own
  ground, they have not hidden it — they have declined to face it. Story
  confirmed this is canon (BIBLE §5.1).

**Both new invariants join the guarded set.** The approach ramp and the
yard/gate→barrow sightline are occludable by later passes — a pine retune, a
scatter change, a terrace edit — exactly as the L0 sightlines are. They are
re-validated by the same canopy-aware raycast machinery as C1 (§1.3), on every
worldgen run, not checked once at authoring time. A seed that terraces away
the ramp or grows a strip across the barrow sightline fails, like any other
C1 failure.

#### 6.1.3 Footprint, mass, readability

**REVISED — it is a real fortress (user decision, stage-4).** The gentry
hall-castle was too small: the user wants "крепость вокруг с башнями и
каменными стенами с воротами входными и замком внутри крепости", explicitly
scalable. So the hall no longer *is* the castle — it stands **inside** a
walled and towered enclosure.

**Why this is now safe, when I originally sized down to protect the
hierarchy:** Ravenscar grows to 110–120 m (§5.7). Every constraint in §6.1.1
is a **ratio or a margin to the peak**, so a 2.2× taller landmark grants
roughly 2× the architectural headroom for free. With a spur pad near 45 m and
`CASTLE_SKYLINE_MARGIN` 12 m, the ceiling is ≈ 98–108 m absolute — some 50 m
of building height available where the old design used 12. The fortress fits
*inside the same siting mechanism*; the pad does not need to move. This is the
first time the landmark has had real headroom over its own architecture.

| Element | Footprint | Height | Tris | Reads as |
|---|---|---|---|---|
| Curtain wall | **80×80 m** enclosure | `CASTLE_WALL_HEIGHT` **8–10 m** | 600–900 | the long horizontal stone band — the fortress's base read |
| Corner towers ×4 | 8×8 m | `CASTLE_TOWER_HEIGHT` **12–15 m** | 200–300 ea. | the rhythm along the wall; four verticals say "fortified" at a glance |
| Gatehouse (twin towers) | 14×8 m | `CASTLE_GATE_HEIGHT` **14–16 m** | 400–600 | the entry mass — the one asymmetry in the wall line |
| Hall | 10×22 m, inside the ward | `CASTLE_HALL_HEIGHT` 8–10 m | 400–700 | the long roof seen *over* the wall |
| Solar / keep | 10×10 m | `CASTLE_SOLAR_HEIGHT` **16–20 m** | 350–550 | the tallest element — the Ward's head, still below R3 |
| Yard / tithe-yard | ≈ 35×35 m open | — | — | the dark interior read through the gate arch |

All heights предложение — утвердить and all remain subordinate to R3 (pad
elevation + tallest element is the binding constraint, not the table).

**Scalable by TERRACED WARDS, not by a bigger box** — this is the ruling that
makes "расширяемая" real. The pad grows 60 → `CASTLE_PAD_SIZE` **120 m**, and
because a 120 m terrace cannot be cut flat into a spur within
`CASTLE_PAD_CUT_MAX`, the enclosure **steps down the slope in levels**, each
level flat within `BUILDING_PAD_SLOPE_MAX` and the wall running along the
terrace edges. This is what real hillside fortresses do, it solves the cut
budget, and it makes growth additive:

**Two different A/B/C axes — do not conflate them (ambiguity I created,
resolved).** My first table read as *build* stages (what we implement now
versus later). Story's phases are *in-world construction generations* — all
three already exist when the player arrives. They are different axes and the
doc means the **in-world** one from here on:

| Phase | In-world | Contains | Terrace | Masonry |
|---|---|---|---|---|
| **A — the panic** | first Corvane lord, on the crown's grant; built fastest, closest to the grave | curtain + 4 towers, the redoubt | upper ward (uphill, nearest crag and barrow) | largest irregular blocks, darkest weathered value |
| **B — the treaty money** | ~2 generations later; the family made respectable, a seat rather than a redoubt | hall, keep-solar, gatehouse, tithe-yard, granary | lower bailey, one terrace down | smaller regular coursing, lighter and cleaner |
| **C — the fear returning** | the dowager's time; begun, never finished | outer works, partial | contour-following perimeter wrapping A and B, stepping with the slope | B's stone, stopped mid-sentence |

**Implementation minimum is A + B** (the act-1 interior set lives in B), and C
comes along free because C is mostly *absence*.

Each stage is a ring, not a rebuild — and every stage re-runs the §6.1.1
checks, because a lower bailey extends the silhouette downhill toward the
valley where it is most visible.

**Phase C is UNFINISHED — as generator rules, not an adjective.** Story's
choice, and the cheapest of the three to build because most of it is what is
*not* there. Rules:

- **Partial arc:** the ring is built through `CASTLE_WARD_C_COMPLETION` =
  0.4–0.6 of its perimeter **(предложение — утвердить)**.
- **The completed arc covers the APPROACH; the gap faces away.** This is both
  how anyone builds (wall the threatened side first) and a necessary design
  guard: if the gap fell on the approach it would become the de-facto way in,
  the gatehouse would be decorative, and the petitioner ritual §6.1.2 exists
  to protect would quietly die. The gate stays the way in; the gap is a flank.
- **WHICH flank: the barrow-facing one** (story's ask, granted — geometry
  checked and it works). From the pad the barrow bears 27°, the crag peak 28°,
  and the approach 225°: the grave and the road are on **opposite** sides, so
  the completed arc covers approach and valley (S/W/SW) and the unbuilt stretch
  faces NNE, uphill toward the barrow. Story's reason is the same lie-in-stone
  logic that chose the fortified reading: the dowager walled the side a
  frightened family can explain — the road, brigands — and stopped before
  closing the side that faces the grave, because finishing *that* stretch
  would have admitted what the whole wall was for.
- **Consequence for C's terrace, and the one refinement this forces:** the
  barrow side is *uphill*, where ward A sits, so C is **not** simply "the
  lowest terrace". C is a **contour-following perimeter that wraps the
  complex**, stepping where the slope demands — one step outside A and B on
  every side, including above them on the crag side. That is also the more
  authentic form: on a hillside the uphill outer works are the ones that
  matter most, since an attacker holds the high ground.
- **The two story asks reinforce each other rather than compete.** The
  barrow-facing corner tower must see the barrow entrance (§6.1.3); the
  barrow-facing stretch of C is exactly the part that was never built. So the
  tower watches the grave **through the gap that shame left open**, and no
  wall-versus-sightline conflict can arise — the clearance is guaranteed by
  the absence, not by a height check.
- **Reachability — now a VALIDATED INVARIANT, not an observation.** Story's
  act-1 MQ4 uses this gap as the trespass route into the muniment room, so it
  is load-bearing in two acts and can no longer rest on "the spur is probably
  climbable". Rules:
  - **A continuous traversable route must exist** from the barrow ground up
    the NNE spur to the gap — validated like the castle ramp (§6.1.2) and the
    summit ascent (§2.5). A seed that walls it off with a 55° scarp breaks
    act 1, silently.
  - **It must NOT be corridor-grade, deliberately.** Average slope in the band
    `SCRAMBLE_SLOPE` = 30–45° **(предложение — утвердить)** — above the 25°
    corridor maximum, below `PLAYER_MAX_SLOPE`. No corridor mask, no width
    guarantee, scarps and outcrops permitted along it. A scramble, not a
    stroll: the difficulty is what makes it read as a back way rather than a
    second front door.
  - **The route passes within 40 m of the barrow entrance**, so story's rhyme
    (the trespasser takes the path the dead will take) is guaranteed by
    geometry rather than by luck.
  - **The completion fraction now has TWO dependents** — the act-1 trespass
    route and the act-3 muster gap. Moving it moves both; never tune it for
    one beat without checking the other.
- **Raw ends:** the wall terminates in a stepped, unfaced core — a ragged
  vertical break, never a clean end. At range this is the whole tell.
- **Lower and unparapeted:** `CASTLE_WARD_C_HEIGHT_FRAC` = 0.6–0.75 of the B
  wall height, flat-topped, no crenellation **(предложение — утвердить)**.
- **Unfaced core** on the last stretch: lighter, rougher value than B's facing
  — inside the same block-size-and-value grammar, so it reads at 640×360.
- **Stopped work on the ground:** a spoil heap and stacks of dressed,
  never-laid stone at the raw end. Two props that say "stopped" better than
  any amount of wall detail, and they date the stoppage to a person rather
  than to history.

Bonus the fiction did not have to pay for: at 0.6–0.75 height and partial
extent, C adds almost nothing to the silhouette budget — it is nearly free
against R3 and the §6.1.1 checks.
**FUTURE (act 3):** the gap is a hole a muster must hold. Whenever that beat
needs a specific footprint, the completion fraction is the knob — story comes
to me with the beat, not with metres.

**The phasing is legible in stone (story's ask, and the two systems already
agree).** Each ward carries a masonry generation: **phase A is the oldest and
roughest**, later wards progressively more regular. At 640×360 this must be
carried by **block size and value, never surface texture** — fine coursing
detail is invisible at our resolution, so: older = larger irregular blocks in
a darker weathered value; newer = smaller regular coursing in a lighter,
cleaner value. Read at distance it becomes a tonal difference *between wards*,
which is exactly the point.

This costs nothing because the terracing rule already puts stage A uphill,
nearest the crag and nearest the barrow, with later wards stepping down toward
the valley. So the oldest, roughest, most hurried masonry is the ring closest
to the grave — story's "phases of dread" and my terrace order are the same
geometry, arrived at independently. Free coherence; keep it.

**One corner tower overlooks the barrow (story's ask, ruled binding).** The
corner tower nearest the Backbarrow must have **clear line of sight from its
top to the barrow entrance**, validated by the same raycast as the yard/gate
sightline (§6.1.2) and equally protected from later occlusion. A garrison
posted on a grave should be visibly posted *on the grave* — and unlike most
narrative asks this one is free: the tower exists anyway, the requirement only
fixes which corner it is and forbids the terrace cut from blinding it.

**Readability changes, and that is intended.** At 80–120 m across, the
fortress *will* now read from Vaelmere (≈ 390 m) where the old hall was
deliberately sub-threshold. R1 still holds — it reads against the crag's body,
never sky — so it does not steal the skyline; what changes is the nature of
the reveal. It shifts from *"you did not know it was there"* to **"you did not
know how big it was"**: at distance a grey horizontal band at the crag's foot,
and only on approach do the gate, the four towers and the hall over the wall
resolve into a fortress. That is still occlude-and-reveal, and arguably a
better version of it.

The envelope must accommodate story's act-1 interior set (public hall, yard,
muniment room, solar) — interiors are **not** designed here, only the footprint
that leaves room for them. The access invariant (§6.1.2) is unchanged and now
applies to the **gatehouse** specifically: the graded ramp runs to the gate
threshold, and a fortress that a petitioner cannot walk into fails the same
way a scarp-only pad did.

**Value, not height** (story's ask, and already §1.5 doctrine): the Ward is the
valley's only large pale-grey built mass. Stone against the meadow greens is
what makes it state power beside Vaelmere's timber-and-thatch hamlet — and
because it shares the crag's rock value, the two read as **one composition**
rather than two competing objects, which is exactly R1's intent.

Readability per §1.5 (`≥ distance / 30` at 640×360): the castle is a
**mid-range landmark**, designed to read from its approach corridor at
150–250 m (where a 15 m keep is 2–3× threshold), *not* from town across the
valley. From Vaelmere it is deliberately sub-threshold and read against the
crag body — you learn of the seat by travelling toward it. This is
occlude-and-reveal (§1.4) doing the work, and it costs nothing: the crag was
already the thing that pulls you that way. Silhouette discipline: one solid
keep mass + two framing towers + a horizontal wall band = three value steps,
dark against the crag's rock. No thin battlement teeth at this budget (§1.5:
nothing structural thinner than ~0.5 m matters beyond 100 m).

#### 6.1.4 Testbed placement (seed 1)

Pad center target **(760, 330)** ± 20 m — the crag's south-west foot spur:
≈ 55 m from the barrow entrance (780, 290), commanding the watchpoint ford
approach, ≈ 390 m from Vaelmere, and inside the crag's angular footprint from
every western/southern valley standpoint. `CASTLE_COUNT_TESTBED` = 1. Pad
ground target ≈ 24 m so R3 holds with a 15 m keep. Core solves the exact
position against the C1 re-validation; the invariants above are the contract,
the coordinates are a starting stamp.

### 6.2 Dungeon entrances — archetypes (ruling, stage-3)

An entrance is a **terrain feature first and a prop second**. The generic
building-pad scorer (flat + dry) is the wrong tool: a cave mouth needs a
hillside to face out of, and flat dry ground is precisely where it cannot
exist. Two archetypes, selected by measured relief.

**Selection rule.** Measure relief within 25 m of the candidate site. Relief
≥ `DUNGEON_ENTRANCE_MIN_RELIEF` = 6 m → **adit** (horizontal). Below that →
**sunken barrow** (descending). **(предложение — утвердить)**

**Marker and facing are DERIVED, never tabled** — from the carve mouth
position and its outward normal. This is the §7.1a derived-only rule, which
now explicitly covers **carve-adjacent** placements as well as water-adjacent
ones: any prop whose meaning depends on generated geometry is derived from
that geometry. A marker 10 m from its mouth is not a cosmetic defect, it is
the same class of bug as a ford that isn't on the river.

#### 6.2.1 Adit (sloped ground, ≥ 6 m relief)

Mouth cut into the hillside, facing out along the slope normal; stub passage
15–20 m. Frame the mouth with a dark lintel and a 2–4 m rubble apron. The
hill itself supplies the silhouette, so no extra marking is required beyond
the scatter exclusion below.

#### 6.2.2 Sunken barrow (flat ground) — the flat-ground answer

Do **not** place a bare hole. A hole in flat ground has no silhouette, cannot
be found, and reads as a bug — which is exactly what happened. Instead, the
generator **makes** the relief it needs, in the shape the fiction already
wants: a **mound with a cut forecourt leading to a lintel in its flank**.
This is a real chambered-barrow form (mound + forecourt + portal), it is one
radial stamp plus one linear trench stamp, and it solves geometry and
readability with the same gesture:

| Element | Value (предложение — утвердить) | Purpose |
|---|---|---|
| Mound | `BARROW_MOUND_HEIGHT` 3.0 m, `BARROW_MOUND_RADIUS` 15 m | the silhouette — restores the "hillside to face out of" |
| Forecourt trench | `BARROW_FORECOURT_LENGTH` 8 m, `BARROW_FORECOURT_WIDTH` 3 m, descending to `BARROW_FORECOURT_DEPTH` 2.5 m at the portal | the descent, walkable |
| Trench slope | ≤ `BARROW_FORECOURT_SLOPE` 0.35 rad (20°) | under `PLAYER_MAX_SLOPE` with margin; no step > `PLAYER_STEP_HEIGHT` |
| Standing stones | `STANDING_STONE_COUNT` 2–4, `STANDING_STONE_HEIGHT` 2.0–2.5 m | vertical accents flanking the approach — a leading line pointing in |

Mound crest to trench floor gives ≈ 5.5 m of working relief — enough for a
portal at human scale. It is walked down, never fallen into.

**Findability layers** (a descending entrance must be *earned* by the
approach, §1.4): (1) the mound's silhouette; (2) standing stones as the
directional cue — they read as *intentional* at distance, which nothing
natural does; (3) a scatter exclusion ring, `ENTRANCE_SCATTER_EXCLUSION` =
mound radius + 5 m, with no trees or bushes and shortened grass — in a forest
clearing a bald ring reads as made ground; (4) value: the trench interior is
the darkest value in the frame, and the eye goes to the dark hole. At the
forest-ruin site this stacks with the ruin walls §7.1 already specifies —
ruin above ground, barrow beneath it, one coherent site.

#### 6.2.3 Attractor status (C1/C2)

- **The assembly counts, the hole never does.** Mound + stones + ruin are the
  attractor; the forecourt and portal contribute no silhouette and are never
  counted.
- It credits C1 **only within its readable range** — a 3 m mound at 15 m
  radius clears `SILHOUETTE_MIN_PX` out to roughly 90–110 m at 640×360, so it
  is a short-range L1. Beyond that the clearing's approach rests on the forest
  edge and corridor guides, not on the barrow.
- It counts **once** as a composite POI (§6.1.2), and being threshold-scale it
  will rarely join a coequal crowd (§1.1 C2-testbed).
- Trivially compliant with C4 at 3 m; a sunken barrow can never contest the
  L0.

### 6.3 True-darkness places (stage-4 ruling)

The user keeps night **playable** — moonlit and navigable — but wants specific
places to be pitch black: «чёрную пустоту, где даже факел освещает лишь мелкий
клочок света». So darkness becomes a property of a **place**, never of the
clock.

**Graded, not binary.** Each enclosed volume carries an `AMBIENT_FLOOR` in
[0, 1]: the minimum light that exists regardless of sources. Exterior night
sits at the moonlit floor; a crypt with light shafts sits between; true
darkness is `AMBIENT_FLOOR = 0`. A binary flag would force every dark place to
be *equally* dark and would waste the most atmospheric range we have.

**What qualifies — a rule, not a list.** True darkness is available to any
volume that is (i) **fully enclosed**, no sky access, **and** (ii) beyond
`DARKNESS_DEPTH_MIN` = 25 m of path from its nearest entrance
**(предложение — утвердить)**. Consequences that make this the right rule
rather than a naming exercise: a barrow's forecourt and first chamber are dim,
its inner chamber is black; darkness is **earned by depth**, so the player
learns the language and can predict it; and no designer has to remember to tag
a location. Story may of course *want* a specific place dark — under this rule
they get it by making it deep, which is the same thing the fiction already
says.

**No unfair surprises — three required layers:**

1. **A lit threshold.** The last space before true dark must itself be
   visibly lit, so the boundary is seen from outside as a wall of black you
   choose to enter.
2. **A gradient, never a plane.** Ambient falls off over
   `DARKNESS_FALLOFF` = 8–12 m **(предложение — утвердить)**. Walking into
   darkness is a slope, not a switch.
3. **An audible cue** at the threshold (change in reverb/ambience) — sim and
   audio own the implementation; design's requirement is that it exists.

**The torch must still work.** In true darkness the torch's usable radius
stays ≥ `TORCH_RADIUS_DARK` = 4 m **(предложение — утвердить)** — enough to
show the floor and a couple of steps ahead. "Lights only a small patch" is
atmosphere; "cannot see your own feet" is a control problem wearing
atmosphere's clothes.

**C1 does not apply inside true darkness, and something else must.** The
no-dead-horizon rule is meaningless where nothing is visible, so true-dark
volumes are **exempt from C1** — and inherit a replacement guarantee in its
place: **the way out must always be findable.** Either the passage is
unbranching within the dark zone, or the entrance direction is discoverable by
a non-visual cue (draft, sound, a faint glow at the threshold). Getting lost
in the dark is a designed feeling; being *stranded* in it is a bug. Interior
layout belongs to whoever owns dungeon interiors — this is the contract they
must satisfy, and the reason `AMBIENT_FLOOR` belongs in world data rather than
in a renderer setting.

## 7. Testbed application (worldgen v2, что core реализует первым)

Canvas: 4×4 chunks, world XZ = 0…1024 m both axes, seed 1, current surface
16–26 m. All coordinates below are *generator parameters* (a testbed layout
table in the worldgen tool's data), not hand sculpting — each is the center
of a procedural stamp/scorer, tunable and deterministic. **Все координаты и
высоты — предложение — утвердить.**

### 7.1 The plan (feature list, in pass order)

| Feature | Where (x, z) | Parameters |
|---|---|---|
| **L0: Ravenscar Crag** + watchtower ruin | peak (830, 200), footprint r ≈ 180 m | **banded contour massif per §2.8** (replaces the smoothstep radial stamp — the shape the user rejected three times); `L0_RELIEF` **115 m**; `L0_ARETE_COUNT` 3–5; cliff bands above `MASSIF_CLIFFLINE_FRAC`; **summit is a tor** (§2.8.4) carrying the tower ruin (§6); rock splat above the stamp's rockline; **validated summit route, breaching the bands — see below** |

**Ravenscar is the acceptance case for §2.8.** The nine invariants are not
abstract quality gates: the crag **fails all nine today**, measured, and the
user is looking at it. Acceptance is the tour frames from the valley floor and
from the western meadows showing a summit that is a broken rock crest rather
than an arc, visible horizontal stripe rhythm on the flanks, and at least
three crest lines readable at 640×360. If a frame still reads as a dome, the
invariant that let it through is the wrong invariant and gets fixed — the
frames outrank the numbers, because the numbers exist to predict the frames.

**Status — step 1 of §2.8 (the banded massif) is implemented, and five of the
eight currently applicable invariants pass** (core, stage-4; I9 is not
applicable until the placed-rock asset class exists):

| Invariant | Original crag | Step 1 (bands) | Step 2 (facets + couloirs) | Bound |
|---|---|---|---|---|
| I1 concave profile | −7.1° | +15.0° | **12.7°** | ≥ 12° |
| I2 sharp summit | fails | fails | **52.9°** surface / 32.5° footprint | ≥ 40° |
| I3 near-vertical rock | 0.0 % | 16.5 % | **27.4 %** surface / 13.1 % footprint | ≥ 12 % |
| I4 no constant gradient | (footprint, superseded) | 24.2 % | **18.5 %** fullest bin | ≤ 30 % |
| I5 riser/bench alternation | 0 | 100 % | **100 %** of radials | ≥ 70 % |
| I6 band-spacing CV | n/a | 0.518 | **0.464** | ≥ 0.35 |
| I7 arêtes | fails | fails | **4 persistent** | ≥ 3 |
| **I8** lobing | 1.27 (re-stamped) | 1.01, flat | 1.36 / 1.36 / 1.51 — levels pass, **rise 0.14 FAILS** | ≥ 1.35 each **and** rise ≥ 0.15 |

> ### ⚠ THIS TABLE IS NOT THE STATE OF THE MOUNTAIN — the frame is
>
> **Seven of eight pass and the mountain still reads as a dome**, confirmed
> independently by render, the lead and me from
> `screenshots/massif/02_massif_verdict_400m_diagnostic.png`: a smooth convex
> arc, **zero** crest lines against the three this section requires, one
> material band instead of a rhythm. Per the standing clause below, the frame
> governs. **Do not relay this table as progress without §2.8.7 attached** —
> the suite measures the object and never the view, which is how every row can
> be green while the acceptance criterion fails. I10 (massif aspect) and I11
> (silhouette breaks) are the answer; until they run, the honest status of
> Ravenscar is **"still a dome, better instrumented".**
>
> **Superseded by a 12-seed distribution, and SEED 1 WAS ONE OF OUR BEST
> WORLDS rather than a typical one — 8 of 8 on seed 1 is 5 of 8 on seed 2.**
> Every number this stage was measured on our luckiest draw. **I7 is retracted
> entirely and never passed on any seed** (§2.8.3).
>
> | Invariant | min | median | max | bound | failing seeds |
> |---|---|---|---|---|---|
> | I1 concave | 5.44° | 19.41° | 22.45° | ≥ 12° | 2 of 12 |
> | I2 summit | 53.0° | 56.4° | 60.3° | ≥ 40° | none |
> | I3 steep | 25.2 % | 44.5 % | 54.8 % | ≥ 12 % | none |
> | I4 fullest bin | 17.4 % | 21.6 % | 29.3 % | ≤ 30 % | none |
> | I5 radials | 93.8 % | 100 % | 100 % | ≥ 70 % | none |
> | I6 band CV | 0.22 | 0.48 | 0.80 | ≥ 0.35 | 1 of 12 |
> | **I7 arêtes** | 0 | 1 | 2 | ≥ 3 | **all twelve** |
> | I8 level | 1.24 | 1.42 | 1.99 | ≥ 1.35 | 3 of 12 |
> | I8 rise | 0.07 | 0.34 | 0.85 | ≥ 0.15 | 2 of 12 |
>
> **I2, I3, I4 and I5 are robust on every seed and I am calling those
> genuinely done.** They are also — not coincidentally — the four that describe
> *local surface character* rather than *global form*: the suite is strong
> exactly where the frame agreed with it and weak exactly where it did not.

**Three cautions attached to this table, so it is not read as "done".**

1. **I8 fails by 0.01 on the clause §2.8.1 identifies as load-bearing**, and
   core stopped rather than close it against seed 1 — which is §2.8.3's
   marginal-pass rule working one turn after it was written. The fix ruled is
   a change of *unit* (§2.8.2: absolute couloir depth), not a nudge.
2. **The whole table is still one seed.** Per §2.8.3 nothing here counts as
   compliance until the min/median/max distribution exists. I1 at 12.7° against
   a 12° floor is the next-thinnest margin after I8 and would be the first to
   go on an unlucky seed.
3. **I2 and I3 print the field-side reading**; §2.8.3 makes mesh triangle area
   binding. Neither verdict turns on it (52.9 vs 40, 27.4 vs 12), but the
   binding reading is the one that belongs here once wired.

### 7.1b Acceptance vantages — the two frames (design, binding on the tour)

§2.8's acceptance was written as "tour frames from the valley floor and from
the western meadows", which names two standpoints that test **the same
thing**. Corrected on render's challenge, and their far/near split is the
better structure: the dome failure and the constant-gradient failure are
**invisible to each other's distance**. Two frames, one clause each, stamped
here so the acceptance test is reproducible across sessions.

| | **Frame 1 — the verdict frame** | **Frame 2 — the rhythm frame** |
|---|---|---|
| Eye | (120, 300), standing | (545, 165), standing |
| Aim | (830, 200) at y = 95 m | (830, 200) at y = 70 m |
| Range | ≈ 717 m | ≈ 287 m |
| Light | low morning sun, **backlit** | low evening sun, **front-lit and raking** |
| Proves | the massif reads as a **sharp, ribbed, concave, lobed mass against sky** — not a dome — at the range the valley actually looks at it | the flank **alternates cliff and bench at irregular spacing** («перепады не должны быть постоянными»), with planar risers and hard splat lips |

- **Why frame 1 is backlit and frame 2 is not.** Each frame's light is chosen
  to make *its own* failure mode visible. Frame 1 tests an outline, and §1.5's
  doctrine is that landmarks read by **value against sky** — a dark mass
  against a bright sky is the purest possible form of the user's own
  complaint, which was a silhouette word. Frame 2 tests a surface, and a low
  sun behind the camera strikes near-vertical risers close to head-on while
  grazing the horizontal benches, which maximises the riser/bench value
  separation the band rhythm is made of. The same light would ruin the other
  frame: morning sun puts the whole west flank in shadow and no band reads.
- **Frame 1 does not test the bands, deliberately.** A riser+bench pair
  (≈ 28 m) subtends ≈ 11 px at 717 m and ≈ 27 px at 287 m. Bands survive to
  ≈ 960 m at 640×360 before dropping under `SILHOUETTE_MIN_PX`, so frame 1
  *could* carry them — but one frame, one clause.
- **No foot-of-cliff frame, and the reason is the invariant.** From directly
  beneath a riser you see one riser; I4/I5/I6 are about **rhythm**, which needs
  several bands stacked in one view. 287 m is the near end that still shows a
  rhythm.
- **Frame 2's bearing avoids the castle sector** (the castle and barrow sit
  ≈ 208° from the peak; frame 2 looks in from ≈ 280°). Inside 300 m §6.1.1
  explicitly *allows* the castle to fill the view, so a frame taken through it
  would be testing the castle rather than the mountain.
- **Ravenscar must read SOLID, never hazy.** At 287–717 m it is far inside
  `LANDMARK_HAZE_ONSET` = 800 m, and §1.3a's depth separation requires the
  valley L0 always inside the onset and the LR always beyond it. Haze on
  Ravenscar in frame 1 is a bug, not atmosphere.
- **The hard splat edge at band lips is INTENDED — do not smooth it** (§4,
  §2.8.2's planar risers). The thing to check is not whether the edge is hard
  but whether it is hard **in the right place**: the snap must track the
  geological lip, not wander across a bench along a slope-threshold contour. A
  hard edge in the wrong place is worse than a soft one.
- **These frames are shot with the palette post off**, per the user's standing
  instruction, so they are not a test of §1.5's shipped value separation.

**Three corrections after the first shoot, two of them mine.**

1. **FRAME 1 IS UNSHOOTABLE AND THAT IS THE BIGGEST FINDING OF THE SHOOT — see
   §1.3a.** At 717 m the massif's chunks are not resident and the mountain is
   simply **absent** from the frame. Not a design problem and not a camera
   problem; the world stops existing at ≈ 512 m. Frame 1 waits for LOD, and
   render has parked the vantage unchanged behind `DFN_MASSIF_PROBE` so the
   re-shoot is one command. **The 400 m shot is a DIAGNOSTIC, never the
   acceptance frame** — render labelled it so, correctly.
2. **ACCEPTANCE VANTAGES ARE DERIVED, NEVER TABLED (my error, and it is the
   third instance of this exact trap).** I tabled (545, 165) by geometry —
   right distance, bearing clear of the castle — **without checking it against
   the generated forest**, and a dense pine stand owns the frame. §7.1a's rule
   already covers this and I broke it: *any tabled coordinate that must sit on
   or near generated features is a trap.* A camera position aimed through a
   generated forest is exactly such a coordinate. **Rule: an acceptance
   vantage is derived as the nearest standpoint on the required bearing and
   range that C1 already credits with seeing the L0.** A vantage the
   composition rules do not protect can fail for reasons that have nothing to
   do with the subject — and `LANDMARK_VISIBILITY_MIN` = 0.6 explicitly allows
   40 % of the ground not to see the landmark, so picking a point blind draws
   from that 40 % two times in five.
3. **The hour on frame 2 was wrong and the sun geometry is render's to
   state.** I reasoned that a low western sun would front-light an
   east-looking camera; the frame came back backlit under a dusk sky. My
   reasoning was sound and my premise about the sun's azimuth was not, so the
   fix is not to argue it: **render publishes the sun azimuth as a function of
   `DFN_TIME`, it is recorded here, and every future frame request picks its
   hour from that table** rather than from anyone's mental model of a sunset.
   The requirement is unchanged and is stated in geometry instead of clock
   time: **frame 2 needs the sun roughly perpendicular to the view axis and
   low**, so risers and benches separate by value.

The acceptance test does not move and does not become easier for the table
being green: it is still the tour frames from the valley floor and the western
meadows, and **the frames outrank the numbers**, because the numbers exist to
predict the frames. Seven of eight invariants and a summit that still reads as
a dome would mean the invariants are wrong, not that the mountain is right.

**Ravenscar's ascent is a required invariant too (gap exposed by story's
near-miss).** Act 1's climax is the climb to the ward-tower, and I had
specified a validated route for the temple massif (§2.5) and for the castle
ramp (§6.1.2) but never for the crag itself — the landmark whose summit the
story actually uses first. Rule: a **continuous walkable route from valley
ground to the tower ruin** must exist and be validated every worldgen run,
average slope ≤ 25°, never exceeding `PLAYER_MAX_SLOPE`, no step >
`PLAYER_STEP_HEIGHT`. It is a *path*, not a stair — unbuilt, informal, the
line four generations of watchmen wore into the spur — which also keeps it
visually distinct from the Steps (§2.5), so the two climbs never read as the
same place. At 110–120 m of relief this is a real climb; the L0 sight-wedge
rules (§5.7) already keep its approaches clear of canopy.
| **River** | source (760, 300) → lake; exits lake → south edge ≈ (300, 1024) | §3.1 algorithm; width 4→7 m; sinuosity ≥ 1.15; **fords are derived, not tabled** — P2 places them where POI-chain corridors cross the *generated* trace (§3.1 step 6), plus the `FORD_SPACING` minimum |
| **Lake** | center (230, 520), ≈ 90×140 m target | basin stamp, water plane ≈ 15.0 m (`LAKE_LEVEL_TESTBED`); shore sand per §3.3 |
| **Town site (TESTBED_TOWNS = 1): hamlet "Vaelmere"** | (360, 500), east lake shore / river inflow bend | hamlet per §6: tavern head faces the lake; trader at corridor entry; pads flattened at ≈ 17–18 m |
| **Shrine knoll** | (560, 620) | knoll +6 m local bump stamp; shrine spire breaks skyline from town and from ford (430, 620) |
| **Dungeon 1: barrow in the crag** (TESTBED_DUNGEONS 1/3) | entrance (780, 290), south face of the crag | entrance pad + dark portal frame; visible from foothill watchpoint, not from town (occlude-and-reveal) |
| **Castle: House Corvane's seat** (§6.1) | pad center (760, 330) ± 20 m, crag SW foot spur, ground ≈ 24 m | terraced 60 m pad; keep ≤ 15 m (R3); composite POI with the barrow; commands the watchpoint ford; scored in C1 as occluder AND attractor |
| **Dungeon 2: forest ruin** | (620, 850), inside SE oak forest | in a clearing (r = 25 m); ruin walls = L2 from clearing edge; ground is flat here, so the entrance is the **sunken barrow** archetype (§6.2.2) — stamped mound + forecourt under the ruin |
| **Dungeon 3: lakeshore cave** | (180, 350), NW lake shore — mouth at the **foot** of the 10 m bluff, never its crown | adit (§6.2.1), 15–20 m stub; mouth ≥ 2 m above the lake plane; reachable along the sand shore; visible across the water from town (water gap = curiosity) |
| **Foothill watchpoint (minor POI)** | (660, 430) | rock outcrop cluster + lone skyline pine + ford; bridges the town↔barrow gap in the POI chain |
| **Forest masses** | oak: S+SE band (roughly z > 700 plus x > 500, z > 600); pine: **radial ridge strips** on the crag foothills (4 sectors, duty 0.25 — layout knobs `pine_strip_count`/`pine_strip_duty`; a closed annulus can never pass canopy-C1, see §1.3) + N ridge strips | total coverage ≈ 0.30 of land; clearings per §2.2; birch lines along river and lake banks (derived from `dist_to_water`, never tabled) |
| **Meadows** | center and west | flower patches, outcrops, meadow clusters per §2.2–2.3 |

### 7.0a Re-siting the barrow after the L0 raise (stage-4 ruling)

Raising Ravenscar 52 → 115 m buried the Backbarrow: at 81–105 m from the crag
centre the terrain is now 40–64 m, so there is no hillside there to open a
mouth in. **This cascade is mine** — I proposed the raise and did not check
what was anchored to the old surface.

**The durable rule it produces, worth more than the fix:**
**§7.1 coordinates are stamps against a specific terrain state.** Anything
sited *on a landmark's slopes* — entrances, pads, routes — holds an implicit
dependency on that landmark's relief. **Changing a landmark's height
invalidates every placement on it and re-validation is part of the change, not
a follow-up.** Same seam class as the missing Ravenscar ascent: the fact lived
in one zone, the dependency in another.

**RULING: swing the bearing, do not move the castle.** Rejecting core's (a)
and (b) — both move the castle, which cascades into the ford-command
distance, the approach corridor, the trespass route, the terrace/ward count
and the R1 footprint check, to buy something a cheaper change buys outright.
The barrow does not need to move *outward*; it needs to move *around*, into a
**couloir** — one of the re-entrant folds between the ridged stamp's buttress
ridges, where terrain at the same radius is still near valley level.

Measured feasible window (castle unmoved at (760, 330); barrow currently
radius 103 m, bearing 209° from the crag centre):

| Bearing from crag centre | Radii keeping `CASTLE_BARROW_DIST` 40–80 m |
|---|---|
| 180° | 100–110 m |
| 190°–230° | 90–110 m (the whole band) |
| 240° | 110 m |

So there is a **≈ 60° arc** of legal placement. Core's test: within bearings
180°–240° at radius 90–110 m, find samples where terrain ≤ ≈ 28 m (valley
level ≈ 20.4 m plus a working margin), pick the one **nearest the current
209°**, and site the mouth there by the §6.2.1 adit rules. A ridged stamp
produces couloirs by construction, so this should exist; it is a search, not a
carve.

**Fiction cost: almost none.** Proximity, relative elevation, and "the seat
stands over the grave" all survive; the gap and the barrow-facing tower are
defined *relative to the barrow*, so they follow the new bearing
automatically. The only change story absorbs is a compass direction moving up
to ~30°. A grave hidden in a fold of the mountain is also, if anything, the
better image.

**Status after the §2.8 reshape, and a SEQUENCING RULING (stage-4).** The
crag-tunnel and Backbarrow carve tests went **red** on the reshape and core
reported them rather than papering over them — correct, and worth saying
plainly: those two tests going red **is the durable rule above working, not
breaking.** Reshaping the massif invalidated the placements on its slopes,
exactly as stated, and the tests are the mechanism that says so out loud.

**Ruling: re-run the couloir search AFTER I7/I8, not now.** §2.8.5 said to
re-run it "after the reshape", on the reasoning that angular lobing creates
couloirs by construction. That reasoning is sound but its precondition is not
met yet: the reshape's measured lobe ratio is **1.01 against the smoothstep
stamp's true 1.27** (§2.8.1), i.e. the current massif has **fewer and
shallower re-entrants than the shape the search already failed on.** Searching
now would fail for the same reason it failed the first time, and — this is why
it matters — a second failure would wrongly promote the **high-shoulder
fallback**, which inverts a line of story's canon. Do not spend that
inversion on a measurement taken at the wrong moment. The barrow stays where
it is, with its test red, until arête/lobe work lands.

**The override was vindicated harder than I expected (core, same session).**
Their first faceted variant made couloir *presence* a seeded per-facet draw,
and **on seed 1 all three facets missed** — a bare convex polygon with zero
couloirs, on a massif that looked thoroughly reshaped. Re-running the search at
that moment would have failed a third time for exactly the predicted reason,
against a shape whose appearance argued it should have succeeded, and the
third failure is what would have spent the high-shoulder inversion. **The
precondition is now met rather than assumed:** couloirs exist on the flanks by
construction (§2.8.2's no-coin-flip rule came out of this near miss), so the
search may run with the next step. Also recorded: **the crag-tunnel carve test
went GREEN on its own** when the couloirs and deeper facet insets opened the
flank the switchback exits through — a placement that was invalidated by the
reshape and then repaired by it, which is the §7.0a dependency working in both
directions. Only the barrow mouth is still red, where it was told to stay.

**Fallback if no couloir clears:** core's (c) — a **high entrance on the
shoulder**, mouth 20–44 m above the valley, castle unmoved. It keeps
everything geometric but **inverts one line of story's canon**: the grave then
stands over the seat rather than under it. That inversion is arguably stronger
(the Corvanes cannot escape being overlooked by what they did) but it is
story's sentence, not mine — pre-cleared with them rather than assumed.
Options (a) and (b) remain last resorts.

### 7.1a Plan vs generated truth (seed 1, stage-3b probes)

The layout table rows are *stamp centers and targets*; the generated world is
the truth, and validation runs against it (tour v3 already aims at generated
truth). Render's probes of the actual seed-1 build recorded this drift:

- River trace: (730, 320) → (560, 500); outflow leaves the south edge at
  x 300–335.
- The originally tabled ford coordinates did not land on the generated river
  (probe at (430, 620): grass, 60 m from water) — which is why fords are now
  derived (§7.1), never tabled.
- The "flooded bend" at x 320–480 / z ≈ 560 and the apparent oversized lake
  (x 188–274 / z 460–700) measured by the first probes were **pond-and-spill
  overflow sprawl**, not the basin: the §3.3 mud-cap rule drains pond water
  beyond max(`SHORE_SAND_DIST`, 2 × local width) of the trace, after which
  the true basin sits at its 90×140 m target. Total water settled at ≈ 2.3 %
  of the world (lake 0.96 + channel 0.6 + capped bend pools ≈ 0.75).

Resolution (same day, core): fords derived at corridor × trace intersections
+ `FORD_SPACING` gap fill; corridor water depth validated ≤ `FORD_DEPTH_MAX`;
Vaelmere ring and pads dry with > `BUILDING_WATER_MARGIN` clearance against
generated water; seed-1 canopy-aware C1 = 0.618 against
`LANDMARK_VISIBILITY_MIN` = 0.6 (headroom 0.018 — retunes go *down* in
density, there is no room up). Render re-probe of the western/southern town
vantages and one riverside bend confirms the fixes on the next tour.

**Rule (learned the hard way) — water-adjacent placements are derived-only.**
Hydrology drift makes any tabled coordinate that must sit on or near water a
trap. Everything keyed to water — fords, birch lines, shore sand, lakeshore
POI *approaches* — derives from the generated trace and the `dist_to_water`
field. Only stamp centers (basin, source, POI pads) may be tabled, and they
must tolerate the trace landing where it lands.

### 7.2 Why this layout satisfies the contracts

- **POI chain (C3, 180–270 m links):** town → shrine ≈ 230 m; shrine →
  watchpoint ≈ 215 m; watchpoint → barrow ≈ 185 m; shrine → forest ruin ≈
  240 m; town → lakeshore cave ≈ 230 m (along shore). Every POI has a
  neighbor in band; total walk town→barrow ≈ 3 links ≈ 3×70 s — the farthest
  destination is a journey, near ones are hops. POI positions are stamps, so
  these distances survive hydrology drift — but links that cross the
  *generated* river count as valid only once a derived ford (§7.1a) sits on
  them; the C3 validation must use generated water, not this table.
- **C1/C2:** the crag (peak +34 m over town ground, ~560 m away, angle
  ≈ 0.06 rad — clears the ≤ 26 m intervening hills) is visible from the
  meadows, lake shore, and both fords; the SE forest and crag shoulder
  occlude the barrow and forest ruin until approached. From the town: crag +
  shrine + (across water) cave bluff = 3 attractors, the rest hidden.
- **Skyline (§1.5):** shrine on knoll and tower on crag break the horizon
  from the main corridors; birch lines flag the water; pine strips lead the
  eye up the foothills.
- **Water gameplay (§3.4):** hamlet on the lake, 3 fords keep the river from
  severing the graph, one dungeon keyed to water.
- **Density check:** 7 POIs + continuous L2 fabric on 1 km² respects the
  testbed contract without approaching region spacing (Q46 kept separate).
- **Readability check (§1.5 math):** crag mass ≈ 180 m wide reads from
  anywhere; tower (12 m) reads within ≈ 360 m (8 px at 640×360) — i.e. from
  the watchpoint, exactly where the final approach starts. At 320×180 the
  tower reads from ≈ 180 m; the crag itself carries the far read — the layout
  survives the user's pending pixel-size decision.

### 7.3 Implementation order for core (highest impact first)

1. **P1 macro v2** — feature stamps (crag ridged noise, knoll, bluff, valley
   `pow` redistribution) + `WORLDGEN_MAX_HEIGHT` = 64 m + the testbed layout
   table. Deliverable: the tour shows a valley with one unmistakable landmark.
2. **P2 hydrology** — river trace/carve, lake basin, shore mask, fords;
   P3 splat inputs (slope/height/dist-to-water) for render's splat shader.
   Deliverable: water reads on screenshots; sand marks fords.
3. **P4+P5 sites & scatter** — building pads + hamlet/shrine/dungeon-entrance
   placeholder prisms (capsule-era stand-ins are fine, silhouettes per §6),
   forest masses with the three species as cone/ball placeholders, corridor
   mask + C1/C3 validation pass. Deliverable: the closed testbed loop (Q45)
   has its stage — town, 3 dungeons, guides between them.
   Micro (P6) comes last and is mostly render-side.

---

## 8. Sources

Consulted 09:08:2026. Engine-internal grounding: NUMBERS.md, DECISIONS.md
(Q12/Q41/Q45/Q46), `engine/world/sources/Worldgen.cpp` (octaves, quantization
contract), devlog sync №2; installed skills `level-design` (pacing, critical
path, readability, blockout checklist) and `procedural-gen` (fBm,
redistribution, biome lookup, blue-noise scatter, determinism checklist).

- Breath of the Wild triangle rule (Fujibayashi/Yonezu/Dohta, GDC/CEDEC 2017):
  [GamingBolt summary](https://gamingbolt.com/the-legend-of-zelda-breath-of-the-wilds-ingenious-world-design-owes-itself-to-triangles),
  [Nintendo Life report](https://www.nintendolife.com/news/2017/10/zelda_breath_of_the_wilds_ingenious_design_is_all_about_triangles_apparently)
- Robert Yang — [Open world level design: spatial composition and flow in
  Breath of the Wild](https://www.blog.radiator.debacle.us/2017/10/open-world-level-design-spatial.html)
  (scale hierarchy, occlusion/reveal, orbiting, curved paths)
- GMTK — [How Nintendo Solved Zelda's Open World](https://gmtk.substack.com/p/how-nintendo-solved-zeldas-open-world)
  (attraction distribution, progressive revelation, motivation-based pull)
- Joel Burgess — [Skyrim's Modular Level Design, GDC 2013 transcript](https://level-design.org/?p=1643)
  and [Motivating Players in Open World Games, GDC 2011](http://blog.joelburgess.com/2011/03/gdc-2011-transcript-motivating-players.html)
  (landmark-driven motivation, modular kits; the "weenie" lineage in Bethesda
  worlds)
- The "weenie" concept — [Theory of Theme Parks: Wayfinding in Themed Design](http://theoryofthemeparks.blogspot.com/2015/08/wayfinding-in-themed-design-weenie.html)
- Amit Patel / Red Blob Games — [Polygonal Map Generation for Games](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/)
  (rivers downhill via descent, pond-and-spill, elevation redistribution,
  elevation+moisture biome lookup)
- Jaap van Muijden (Guerrilla Games) — [GPU-Based Run-Time Procedural
  Placement in Horizon Zero Dawn, GDC 2017](https://www.guerrilla-games.com/read/gpu-based-procedural-placement-in-horizon-zero-dawn)
  (layered density/exclusion maps, ecotope-driven scatter, water-proximity
  density)
- The Level Design Book — [Landscape](https://book.leveldesignbook.com/process/blockout/massing/landscape)
  (walkable slopes, bowls/ridges vocabulary, water curves, trees as walls,
  rain-shadow reasoning)
