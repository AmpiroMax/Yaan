<!--
Created: 09:08:2026 - 10:45:06
Last updated: 10:08:2026 - 00:10:27
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
- 09:08:2026 - 21:29:00: Core's 12-seed distribution and the I7 retraction. §2.8.3 — I7 RETRACTED as ever having passed: every "4 persistent aretes" is withdrawn, including the row in my §7.1 table; the probe counted qualifying bearing SAMPLES rather than ridges and anchored persistence on the lowest slice, and corrected it FAILS ON ALL 12 SEEDS (max 2 against a floor of 3). Core validated the detector against a couloir-free faceted polygon before believing the failure. Sharpest lesson of the stage recorded: 42 aretes was absurd on its face and was caught instantly, 4 WAS NOT ABSURD AND SURVIVED THREE ROUNDS OF RULINGS — a wrong number in the plausible range buys itself unlimited time, third instance this session after the C1 self-occlusion figure and my own lobing mechanism, all three directionally reasonable. Then the failure turned out partly MINE: raw detections rise with height (2/3/5/5) because §2.8.2's eps increases with elevation, so THE MECHANISM SATISFYING I8's RISE IS THE MECHANISM PREVENTING I7's PERSISTENCE, and two of I7's four levels sit in the smooth apron below MASSIF_CLIFFLINE_FRAC that §2.8.2 explicitly describes as unbanded. RULING: I7's persistence is measured over four levels spanning the BANDED ZONE only, cliffline to summit, never the apron — not a relaxation invented to green a red test, since eps-increasing-with-elevation predates the conflict and ribs dying into a talus apron is the mirror of the couloir-fade ruling nobody objected to. GUARD MAKING IT LEGITIMATE: conditional on I11 existing, because a test whose sampling elevations I may choose is a test I can always make pass, and I11 is measured from a camera and cannot be gamed by slice elevation — if I11 is not implemented, I7 keeps its original levels and stays red. A proxy may only be loosened once the thing it proxies for is measured directly. §2.8.6 — RULING: Ravenscar's arete count is FOUR and L0_ARETE_COUNT 3-5 is retired as a range. Measured across 12 seeds: 3 fails I8 level on 3 seeds and rise on 2; 4 fails level on 1 and rise on NONE; 5 FAILS ALL TWELVE, which is the convex cap n*tan(pi/n)/pi = 1.16 for a pentagon appearing as a measurement rather than as algebra — first empirical test of that cap, and it means the authored range contained a value that CAN NEVER PASS, so a per-seed draw across it (which my own character-not-existence rule would otherwise invite) would ship guaranteed-failing worlds. Three failed for a structural reason that is not about shape: THE GENERATOR INPUT AND THE INVARIANT FLOOR WERE THE SAME NUMBER (3 corners against I7's >=3), so a single missed detection fails by construction — new general rule, a generator input must never equal the floor of the invariant that checks it, because that is a coincidence rather than a margin and every measurement error lands on the failing side. Sixth instance of the range family (arete_count pinned at L0_ARETE_COUNT_MIN, exactly as the tor derived its base from SUMMIT_TOR_HEIGHT_MIN). §2.8.2 — THE ABSOLUTE SCALE IS BAND HEIGHT, NOT MASSIF RADIUS (core's correction, repairing a hole I put in my own ruling): taking MASSIF_RADIAL_LOBE_AMP off the 180 m base radius gives 32-63 m insets, wider than the upper mountain, so MY CLAMP BOUND AT EVERY HEIGHT AND SILENTLY RESTORED the fraction-of-local-radius behaviour the unit change exists to remove (measured in that state: levels 1.50/1.50/1.60 but rise 0.10 and I7 gone). My clamp was not a safety net, it was a RE-ENTRY POINT FOR THE BUG IT GUARDED, and is withdrawn as written; scale is MASSIF_CLIFF_BAND_MIN..MAX because a couloir is a gully incising the cliff bands — same landform, same scale, same units. A feature's size comes from the feature it cuts, never from the mountain it sits on. Any surviving apex clamp must report whether and where it bound, because that failure is silent (levels keep passing while the rise dies). Added core's elongation sentence under the bar: an elongated support polygon puts its corners on the long axis so arete bearings CLUSTER, and I7's persistence would keep passing while the mountain became a ridge — the bar must be a design rule precisely because the suite is blind to it. §7.1 — single-seed table superseded by the 12-seed distribution; SEED 1 WAS ONE OF OUR BEST WORLDS, not a typical one (8/8 on seed 1 is 5/8 on seed 2), so every number this stage was measured on our luckiest draw and the marginal-pass rule earned itself within the hour. I2/I3/I4/I5 robust on every seed and called genuinely done — noted that they are, not coincidentally, the four describing LOCAL SURFACE CHARACTER rather than GLOBAL FORM: the suite is strong exactly where the frame agreed with it and weak exactly where it did not. I1 fails 2 seeds, I6 fails 1, I8 level 3 and rise 2 at the shipped 3-arete config.
- 09:08:2026 - 21:32:00: Core measured before cutting; two of my four hypotheses FALSIFIED and the real cause found. §2.8.7 I10 — RULED the reading core flagged before it became a third round: I10 is measured FROM THE CLIFFLINE CONTOUR TO THE SUMMIT and is an ENVELOPE measure, never a surface mean; NUMBERS.md's whole-cone definition is corrected to match my text, because I10's derivation is §4's rock threshold and a grassy APRON is not a defect but what §2.8.2's p>1 profile is for (talus fans out). Measured: whole cone 26.9 deg, above cliffline 35.0 deg, both failing the 40 deg floor, bracketing my ~33 deg prediction — I10 SURVIVES and core is not pushing back. WITHDRAWN, both of my suspected causes: radial excursion is 87.9/76.3/63.4/34.5 m against a 13.3 m readable scale, so lobes are THREE TO SIX TIMES larger than needed and MASSIF_RADIAL_LOBE_AMP is delivering; and the tightest corner radius is 2.8 m against that same 13.3 m, so corners are sharper than the eye can resolve. The ribs are big AND sharp AND still invisible, which kills both comfortable explanations. THE REAL FINDING, and it is the most important of the stage: I1 — the invariant I called «the core anti-dome test» — IS MEASURING THE WRONG THING. Built outline slope summit-outward is 20.1/18.2/20.9/20.9/31.8/23.9/30.0 deg, SHALLOWEST AT THE SUMMIT AND STEEPEST AT THE FOOT, the exact inverse of concave and the textbook dome signature, with the silhouette bulging up to 14.8 m above a straight cone — while I1 passes at 12.7-19.4 deg on the same mountain. Both numbers correct: I1 averages SURFACE slope, and on a banded massif that average is set by benches and risers (the sawtooth texture), not by the form the sawtooth sits on. RULING: I1 re-specified as an ENVELOPE measure, same threshold and intent, correct basis. General rule recorded in core's words — AVERAGING OVER A SURFACE HIDES THE SHAPE OF ITS ENVELOPE: a staircase of any overall form has the same mean tread-and-riser slope, so any invariant meant to constrain FORM must measure the envelope, and surface means may only constrain TEXTURE — which is exactly what I3/I4/I5 do and exactly why those are the ones robust across all twelve seeds. Sharpest version, because it will recur: A MODEL CHANGE CAN INVALIDATE AN INVARIANT'S MEASUREMENT BASIS WITHOUT CHANGING ITS NUMBER — before §2.8.2 the massif was smooth so surface mean and envelope agreed and I1 was sound; adding benches and risers decoupled them, so THE FEATURE THAT FIXED I3/I4/I5 SILENTLY BROKE I1'S VALIDITY while I1 kept reporting a healthy number. Nobody introduced a bug; when the model changes, every invariant's BASIS is re-opened, not just its value. THE SUMMIT TOR IS INVISIBLE AND I CERTIFIED IT WITH AN INVARIANT I RULED MYSELF: core disabled it and the outline was identical to the decimal, because SUMMIT_TOR_RADIUS 5-10 m on a ~190 m massif hides inside the cone tip; it passes I2 at 52.9 deg ONLY because I ruled I2 surface-area weighted and near-vertical slab sides dominate that average, so we built an invariant certifying a summit feature the camera cannot see — core calls it partly theirs for asking, it is mine for ruling. Weighting still stands for I3/I4 (the limit argument is untouched and a plan-weighted I2 would still reject a tor outright), but RULING: the tor is SIZED AGAINST THE ACCEPTANCE DISTANCE, its silhouette clearing SILHOUETTE_MIN_PX from §7.1b's frames (~13 m at 400 m, ~24 m at 717 m), making SUMMIT_TOR_RADIUS derived and landing near MASSIF_SUMMIT_RADIUS_FRAC of base radius — «the summit IS a tor» was always the ruling, a 5 m ornament on a 190 m mountain never was. I2 means nothing until I11 runs, recorded as a dependency. §7.1 — THE MOUNTAIN IS 16% SHORTER THAN THE CONSTANT THE USER APPROVED: L0_RELIEF 115 is documented as «перепад» (relief above the foot) and THE CODE USES IT AS ABSOLUTE PEAK ELEVATION, so with the valley at 18.8 m the true relief is 96.2 m. The castle-dominance 0.285 and C1 0.903 that justified the raise were measured on this build and stand as measurements, but THE USER APPROVED 115 m OF RELIEF AND THE WORLD HAS BEEN SHOWING THEM 96 — every rejection of this mountain has been a rejection of a shorter mountain than the one signed off. It also inflated the aspect failure since relief is the numerator: fixing the meaning moves aspect 0.507->0.606 for free and lifts the required base radius from ~99 m to ~137 m, so the bug was costing 20 m of footprint. Seventh instance of a constant's MEANING read wrong rather than its value — the range family's close cousin, and the reason NUMBERS.md prose is a contract and not a comment. §7.1 Ravenscar ascent — story's condition folded in as binding: UNBUILT IS NOT DECORATIVE, and if steepening makes the <=25 deg natural line hard to find the answer is a longer traverse or more switchback, NEVER masonry, because a built stair collapses §2.5's TWO DIFFERENT CLIMBS distinction and act 2's Seven Thousand Steps stop being singular. Recorded why the condition is cheap: 115 m of climb at <=25 deg needs ~247 m of run, more than any radial on a contracted massif, so the route MUST wrap — and §2.8.2 already builds the structure, since THE BENCHES ARE THE TRAVERSES AND THE BAND BREACHES ARE THE RISERS. The steeper mountain does not threaten the route, it supplies it. Watch item: I5 counts alternation on non-route radials, so a longer wrapping route shrinks that sample; report the non-route radial count alongside I5. §2.8.7 I10 — story's second condition folded in: THE CASTLE IS THE PLACEMENT AT RISK and my §2.8.5 prediction that R1 was safe «because the base radius is unchanged» expires with the radius; the Ward sits ~148 m from the crag centre, so a contracting hem could leave it on flat ground beside the massif rather than on a spur, failing §6.1.2 outright and shrinking the angular envelope that keeps the fortress reading AGAINST ROCK rather than against sky — the mechanism protecting the tower's skyline monopoly, which is the arc's central image. Re-measure R1-R4 and the castle's ground after the reshape rather than assuming 115 m of relief still buys the margin it bought at 180 m of radius.
- 09:08:2026 - 21:35:00: Story's consult trigger made binding on terrain, plus their framing carried into §2.8.7 for the sync. §2.8.7 I10 — THE APRON FLARE IS THE RELEASE VALVE, spend it before moving the castle: since I10 constrains only the body above the cliffline, the hem may stay where it is, and preserving the Ward's spur is a legitimate reason to let it. Binding order of preference on the re-siting pass: (1) flare the apron so the spur survives, (2) stop the contraction, (3) move the castle — and (3) REQUIRES A STORY CONSULT BEFORE IT LANDS, not after. Recorded the asymmetry story caught, general and not just about this castle: when the BARROW moves its satellites follow for free because the ward gap and the barrow-facing tower are DEFINED RELATIVE TO THE BARROW (§7.0a), but when the CASTLE moves nothing follows — the ~55 m barrow proximity, the yard/gate->barrow sightline and the act-1 trespass route are each defined between the castle and something NOT moving with it, so all three break independently. A landmark whose dependents are defined relative to it is CHEAP to move; a landmark that is itself the fixed end of other relations is EXPENSIVE. Check that distinction before relocating anything in this document. §2.8.7 — THE SAME DEFECT EXISTS IN EVERY ZONE and story named it better than I did, carried in their words: their equivalent of nine invariants that measure the object and never the view is «canon that is true in the document and never checked from where the player stands». Same defect, two zones, both found the same way — by someone finally looking at the thing rather than at the numbers about the thing. Recorded as the pair for the sync, each being the other's proof: (1) a test that measures the artefact instead of the experience — core disabled the summit tor entirely and the outline was identical to the decimal while my own invariant scored that summit 52.9 deg against a 40 deg floor, which is not a weak test but one measuring something the view cannot contain; (2) canon true on the page and unverified from the ground — story's barrow-visibility condition, which is why they restate it as a refusal rather than a preference. Both pass right up until someone looks. Counter-measure: not better thresholds, but that EVERY ZONE NEEDS AT LEAST ONE CRITERION EVALUATED FROM THE PLAYER'S POSITION, and that criterion outranks the rest of its suite — I11 is terrain's, story's is a raycast from Vaelmere. Also logged: the ledge/breach ascent finding is now how ACT1 specifies the climb, making the unbuilt worn path TRUE BY CONSTRUCTION rather than by assertion — the stronger form of the same guarantee, since the geometry produces it whether anyone remembers the rule or not. TIMESTAMP CORRECTION, self-reported: the two preceding entries were stamped 21:36:00 and 21:52:00 without re-reading system time, and both were AHEAD OF IT — 21:52 by seventeen minutes. That is precisely the drift that blocked the team's commits once before, and I did it while holding the rule in my own brief. Corrected to 21:29:00 and 21:32:00, inside the window the work actually happened in; no UPD entry was removed (Rule 17) and only the incorrect stamps changed. The cause is the session's own lesson in miniature: I took one real reading and then EXTRAPOLATED the next three from it rather than measuring again — a stamp derived from a model of the clock instead of from the clock. Take a fresh reading per entry.
- 09:08:2026 - 21:38:00: TWO STALE-STATE ERRORS OF MY OWN, both found by one grep the lead told me to run. (1) I reported to the lead THREE TIMES that core was «holding pending the lead's radii». They were not: LR_BASE_RADIUS 260-310, L0_BASE_RADIUS 120, MASSIF_ASPECT_MIN 0.84, MASSIF_SILHOUETTE_BREAKS_MIN 3, L0_ARETE_COUNT 4-4 and the amended L0_RELIEF meaning had all landed in NUMBERS.md before my last two reports. I was reporting a state I had INFERRED rather than one I had CHECKED — my own measure-the-artefact rule, applied to another zone's status. New personal rule: before writing «X is blocked on Y», look at Y. (2) §5 — my paragraph «CROWN_ASPECT_MAX landed at 2.0 and I am accepting 2.0 rather than re-litigating» is WITHDRAWN. The lead landed 2.0 at 21:12:24 and refined it to 1.8 at 21:13:05, ONE MINUTE LATER, per my own earlier verdict. I read the registry between those two edits, inferred a disagreement that had already been resolved, and spent a ruling reconciling myself to a value nobody was holding — then told flora to keep 2.0. Nothing downstream broke (every built tree measures <=1.28 and clears either number), but the argument was sound applied to a false premise, which is the third time this session I have produced a well-reasoned conclusion on an unchecked one. The tightening trigger that paragraph proposed is moot: the ceiling is already 1.8. OUTSTANDING against NUMBERS.md, noted so it is not lost: SUMMIT_TOR_RADIUS is still 5-10 m, which §2.8.4 now rules must be DERIVED from the acceptance distance (silhouette clearing SILHOUETTE_MIN_PX at §7.1b's ranges, ~13 m at 400 m and ~24 m at 717 m); at 5-10 m the tor is invisible and core measured the outline identical with it disabled. STORY'S COUNTER-MEASURE, general form, recorded because it is better than the instance I gave them and it binds beyond terrain: EVERY LOAD-BEARING CANON CLAIM ABOUT THE WORLD MUST NAME A STANDPOINT FROM WHICH THE PLAYER CAN VERIFY IT, AND THE STANDPOINT CHECK BEATS THE PROSE — a canon claim with no standpoint is prose, not design, and either gets a standpoint or gets cut. Terrain's instance is I11; theirs includes the NEGATIVE case (the barrow mouth must NOT read from Vaelmere), which my formulation had missed: a criterion from the player's position must be able to fail in both directions. THE SESSION'S ONE SHAPE, in story's words: every failure caught tonight — nine invariants measuring the object, canon unverified from any standpoint, both our clocks, stale text anchors, and this entry's two — is REASONING FROM A MODEL OF THE THING INSTEAD OF FROM THE THING. The counter-measure is not more care; it is keeping a cheap channel to the actual artefact and using it every time. For terrain that channel is a camera. For story it is asking where the player stands when the sentence is true. For a constant it is a grep, and for a timestamp it is the clock.
- 09:08:2026 - 21:41:00: The cascade landed and produced three rulings, one of which is my own I3 fix colliding with my own I4 rule. §2.8.3 NEW MEASUREMENT RULE — EVERY INVARIANT SHIPS WITH A CONTROL: THE SHAPE IT EXISTS TO REJECT MUST FAIL IT. Core ran the I11 detector against a smooth analytic cone before trusting it («I have now been burned twice by trusting a detector») and THE CONE SCORED EXACTLY 3 AGAINST A FLOOR OF 3. Third time this section has produced a test its own reject-case passes — contour-CV scored 0.935 on the dome, footprint-weighted I3 scores its ideal at zero, now I11 — and three instances is a missing step, not bad luck. A new invariant is not believed until the dome fails it. §2.8.3 — second new rule, core's, adopted: A VIEW-SPACE TEST IS EVALUATED AT THE RESOLUTION OF THE EYE IT STANDS IN FOR; their first I11 returned 17-31 breaks whose count DID NOT FALL AS THE THRESHOLD ROSE, which is the signature of sampling jitter rather than structure and is a keeper as a diagnostic. Window is the readability scale: distance/30 metres is 1/30 rad at every distance. §2.8.7 I11 — RULED, count breaks on the INTERIOR of the horizon only, excluding apex and the two hem junctions, which is what §7.1's «three readable crest lines» always meant; cone scores 0, reshaped massif 4-11. Break threshold 20 deg of tangent turn, bracketed rather than picked off the curve: above by MASSIF_ARETE_TURN_MIN 50 deg (a plan-view aspect turn always projects to a SMALLER silhouette tangent break, so the silhouette threshold sits below it), below by core's measured 10-15 deg noise floor; 20 deg takes the wider margin, 6 breaks against a floor of 3, rather than 30 deg's 4-against-3 which would violate the never-equal-the-floor spacing. Provisional; frames outrank it. Recorded that I WROTE I11 WITH THIS HOLE IN IT ONE MESSAGE AFTER WITHDRAWING TWO INVARIANTS FOR THE IDENTICAL DEFECT. §2.8.2 — CLIFF RISERS ARE PLANAR BUT NOT IDENTICAL, the angle varies between bands. I4 now fails 8 of 12 because a steep massif concentrates surface in the 60-70 deg bin, and the cause is PLANARITY PLUS A SINGLE CLIFF ANGLE: I ruled risers planar to stop them spending width on sub-cliff slope (I3), and a planar face puts ALL its area at ONE angle, so a single cliff angle lands every riser in the same bin. My two rulings were fighting and I4 noticed. Fix moves no threshold — the CLIFF class becomes a BAND of angles, each riser still planar and still above MASSIF_CLIFF_SLOPE_MIN but drawing from a seeded spread; I3 unaffected, I4 satisfied by VARIETY rather than shallowness, and it is the truer landform since a real banded scarp carries 55, 70 and overhanging faces in one massif. MASSIF_CLIFF_SLOPE_MIN is a FLOOR and a floor was never a target — the same sentence I wrote about MASSIF_BENCH_SLOPE_MAX being a ceiling. Ruled explicitly that I4 is SOUND and not the thing to relax: «перепады не должны быть постоянными» is exactly the complaint a uniformly 65 deg massif re-creates at a steeper angle, and this is the THIRD variant of the identical failure after dead-flat benches and ceiling-pinned benches, each fixed by variety rather than by a different constant. CORE'S SECOND BUG, and it is why the profile was inverted: the stamp computed h = H*(1-t)^p, which decays to ZERO at the rim rather than to the valley floor, so the entire concave tail sat below base terrain and was discarded by the max() — only the steep crossing where the cone cuts the valley survived, which is precisely why the envelope measured shallowest at the summit and steepest at the foot. Now datum + relief*(1-t)^p. My I1 envelope re-spec reads 42.5 deg where the surface basis reported 6.4 deg on the same mountain, so the re-spec was right AND there was a bug underneath it. CASTLE NEEDS NO RE-SITING: R1-R4 pass, crown unoccluded, ratio within CASTLE_SILHOUETTE_RATIO, ramp within budget, Backbarrow visible from BOTH yard and gate — option (1) of the preference ladder happened for free because I10 constrains only the body above the cliffline and the p>1 profile fans the hem naturally. Distribution after the cascade: I1 30.3-52.0, I2 64.8-74.3, I3 62.0-71.7, I5 92.2-100, I10 1.23-1.63 all passing on every seed; I6 fails seed 1 only; I4 fails 8, I8 level 6, I8 rise 8, I7 0-2 everywhere. Core did not patch the regressions and reported them as my own model-change rule arriving on schedule.
- 09:08:2026 - 21:50:00: I11 WORKS AND REPRODUCES THE USER'S COMPLAINT FROM A CAMERA. Interior breaks at 20 deg, floor 3, cone control reading 0 everywhere: 300 m gives 5/8/12/4 and passes from every bearing; 400 m gives 1/4/5/4 and fails from one; 600 m gives 1/1/0/0 and FAILS FROM EVERY BEARING. The massif reads as broken rock up close and as a smooth mass from the valley — the sentence the user has been saying for four sessions, now produced by the only invariant computed from a camera, while nine object-space invariants report a healthy mountain on the same build (I1 47.3, I2 70.2, I3 67.9%, I10 1.46, robust across twelve seeds). The §2.8.7 thesis measured rather than argued: the frame and the camera-side invariant agree with each other and disagree with everything else. §2.8.2 MODEL RULING — I7 FAILING ON EVERY SEED AND I11 FAILING AT 600 m ARE ONE FAILURE. A tangent break is scale-free: a genuine corner between two flat faces reads at any distance, because the chord either side lies along a straight facet however wide the window grows. Breaks can only wash out with distance if THE FACETS THEMSELVES ARE CURVED, which is precisely what I7's MASSIF_FACET_TURN_MAX says directly and it has never passed. Three rulings follow: (a) seeded variation perturbs the polygon's PARAMETERS — each facet's support distance d_i and bearing alpha_i drawn once per facet — and NEVER modulates R(theta) continuously across a facet, which is what bends a flat face into an arc and why a support-function construction that is polygonal by definition has been producing curved facets; (b) A COULOIR IS A PAIR OF FACETS, NOT A DENT IN ONE — a notch with its own two planar walls preserves flatness and ADDS corners where a smooth re-entrant subtracts them, which also retires the tension core measured earlier (deepening couloirs dropped aretes 4->0 BECAUSE they were dents; as facet pairs they raise I8 and I11 together), leaving everything on the massif as flat faces meeting along lines at every scale — the user's «рёбра» and this document's own definition of an arete, finally built the way both are written; (c) CREST STRUCTURE IS SIZED AGAINST THE ACCEPTANCE DISTANCE, facet arc exceeding the readability window at the far range (~20-24 m at 600-717 m). SECOND INSTANCE of that rule after the summit tor, so it is now general: DETAIL SIZED AGAINST THE OBJECT SHRINKS OUT OF LEGIBILITY AS THE OBJECT RECEDES; anything required to read at distance is sized against the distance. Corner count follows for free since only limb-facing facets contribute breaks — four aretes alone put barely two corners on the outline, and couloirs as facet pairs clear I11's floor without touching L0_ARETE_COUNT and without hitting the convexity cap, because a re-entrant notch is exactly what makes the section non-convex. §2.8.2 NEW CONTROL — A PER-BEARING FIELD MUST BE VERIFIED UNIFORM OVER ITS DECLARED RANGE (core's proposal, adopted; the distribution-shaped version of the reject-case control). Core measured 0% of samples below 0.4, then lumps of 26% at 0.6 and 30% at 0.8, against a raw lattice uniform to a tenth of a percent: every seeded spread in the model was silently using the top 60% of its range, lumpily. Cost, all invisible until measured — the profile exponent never approached MASSIF_PROFILE_EXPONENT_MIN so THE GENTLE-FLANK HALF OF §2.8.2'S ASYMMETRY RULE NEVER EXISTED AT ALL; cliff risers were never drawn near MASSIF_CLIFF_SLOPE_MIN, leaving 4% in the 50-60 deg bin, a hole exactly where my band-of-angles ruling expected mass; and the 0.5 cliff/ramp split was arithmetic about a coin that was never fair. A spread that is secretly peaked passes every review and no invariant in this document names it. MY I4 DIAGNOSIS WAS WRONG ABOUT CORE'S BUILD: the band-of-angles was ALREADY implemented per (band, bearing), so single-angle risers were never the cause. Fourth time this session I reasoned soundly from an unchecked premise, and the worst instance because I asserted a fact about another zone's code IN THE SAME MESSAGE where I refused to reason about I8 without asking — I applied the lesson to one item and not the other, one message after the lead taught it to me. The ruling survives as a rule (a floor was never a target) but not as a diagnosis. Core's own lesson recorded beside mine: they fixed the degenerate-circle geometry once for the lobe field and left the broken helper feeding four other consumers — FIXING A SYMPTOM IS NOT FIXING A MECHANISM, and they had already written the mechanism down. I8 rise: unit change VERIFIED IN SOURCE and rise still fails 9 of 12, so the measurement is honest — but it predates the facet-planarity change, which alters how eps varies with height entirely, so DO NOT DIAGNOSE THE MECHANISM YET; re-measure after facets and couloirs land. I6 now passes on every seed (the harmonic fix did that). Six of ten robust across twelve seeds.
- 09:08:2026 - 22:20:46: THE ACCEPTANCE DOCTRINE, and it invalidates the number this whole stage was measured against. NEW §1.6 — an acceptance frame is a VERDICT only if five conditions hold (residency, budget, authorship, standpoint, light) and is otherwise a DIAGNOSTIC that is labelled and never relayed as the state of the world; four frames were shot this stage and not one of them was a verdict. §1.6.1 F2 — AN ACCEPTANCE DISTANCE IS A PROPERTY OF THE LANDMARK, NEVER OF THE PROJECT: readable units U(d)=60R/d, I11 demands six silhouette features (3 interior breaks + apex + 2 hem junctions) at ~2 units apiece = 12 units with zero slack, and this document has twice refused zero slack, so LANDMARK_ACCEPTANCE_UNITS_MIN = 20 and d_accept = 3R. Derived first and checked after, not fitted: I11 fails from one bearing at 400 m (18 units), passes everywhere at 300 m (24) and reads 9-12 at 253 m (28). Ravenscar's acceptance distance is 360 m; the LR's is 780-930. THE 600 m FIGURE WAS WRITTEN FOR THE LR AND APPLIED TO THE CRAG — conservative for the mountain it was written for, impossible for the one it was used on, and the evening's headline sentence is that mismatch measured rather than a finding about Ravenscar's shape. Honest cost recorded: at 3R the verdict frame lands at 360 m and the rhythm frame is at 287 m, so RAVENSCAR IS TOO SMALL TO HAVE A FAR FRAME DISTINGUISHABLE FROM ITS NEAR ONE — the two frames now differ by clause and light, not by range, and a far frame is a thing only a large landmark has. Metric structure (band pair ~28 m, readable to ~840 m) and angular structure (3R) have different acceptance distances: THE BINDING DISTANCE IS THE CLAUSE'S, NEVER THE FRAME'S. §1.6.2 F3 — measured in the generator rather than assumed: the world is 4x4 chunks = 1024 m closed by physics walls, and authored influence is a UNION OF STAMP FOOTPRINTS (massif 162 m, troughs 96-128 m, bumps and lake at their radii) outside which the terrain is three octaves of value noise at 0-31.5 m relief, INSIDE the box as much as outside it. Rulings: unauthored FOREGROUND does not invalidate a frame (ground is allowed to be ground); unauthored BACKDROP invalidates every COMPOSITION clause (C2, C4, §1.3a depth separation, §6.1's rock-not-sky envelope are all claims about relationships between two AUTHORED masses); unauthored terrain touching the subject's outline contaminates the silhouette clause specifically, so I11's azimuth guard becomes general — every view-space test states the angular window within which it attributes structure to its subject; a frame whose subject is unauthored is a RENDER test. Therefore EXTENDING THE WORLD DOES NOT EXTEND THE AUTHORED WORLD: a 2x2 km map with the same five stamps has exactly as much design in it and four times as much backdrop. §1.6.3 — NO RULE IN §1.3/§1.3a IS PASSING OR FAILING BEYOND THE AUTHORED EXTENT, IT IS UNSHOT. LANDMARK_MAX_DISTANCE 4000 m is a siting ceiling and a depth-precision bound, never a legibility specification, and it has certified nothing because nothing has ever been sited past 1 km — there is nowhere to put it. Every C1/C2/C3/C4 figure is a statement about a 1024 m world and is reported with that extent attached. THE LR DOES NOT EXIST IN THE GENERATOR (core established by search: no code path reads any LR_ constant), so every temple-massif ruling I have made is unvalidated by construction and DESIGN STOPS REFINING LR NUMBERS UNTIL AN LR STAMP EXISTS — tuning a constant for an absent object is the purest instance of this stage's defect and I did it all evening. NEW STATUS CATEGORY, the reporting half of Rule 30: PASSING / FAILING / UNSHOT, and UNSHOT never enters a count. §1.6.4 F1 — §1.3a's box CORRECTED on a measurement: ChunkManager loads by CHEBYSHEV radius in CHUNK units and clips to extent, so 'the world stops at 512 m' is the right order and the wrong shape; for Ravenscar in chunk (3,0) the legal standpoints are x >= 256 and z < 768 and frame 1's (120,300) is illegal on its BEARING, not its range (flagged as a PREDICTION from reading ChunkManager.cpp, to be checked by render — I have not read how a probe sets its streaming focus and will not assume it). LOD IS NOT THE PRECONDITION FOR RAVENSCAR'S ACCEPTANCE: at 360 m the verdict frame is shootable tonight. CHUNK_LOAD_RADIUS still must not rise for a screenshot. NEW §2.8.8 — THE DOME FRAME IS ATTRIBUTED AND IT WAS RECORDED, no reconstruction needed: screenshots/massif/02_massif_verdict_400m_diagnostic.png, 21:14, L0 at 400 m on frame 1's bearing (eye ~(434,256), due west of the peak), backlit. 400 m is 18 readable units, INSIDE the budget, so the comfortable reading 'photographed from too far away' is available and is FALSE for this frame — I looked at the mountain from very nearly its own acceptance distance and saw a dome. But the frame PREDATES THE PROFILE-CLIPPING FIX BY HALF AN HOUR: the stamp was computing h = H*(1-t)^p, decaying to zero at the rim so the whole concave tail was discarded by the max(), which core describes as precisely why the envelope measured shallowest at the summit and steepest at the foot — the textbook dome signature. THE FRAME WAS RIGHT; IT WAS A PICTURE OF A BUG, AND THE BUG IS FIXED. So MY dome verdict is CLOSED and THE USER'S COMPLAINT IS NOT: he said it while playing, four sessions running, and no frame has been shot since relief +19, radius 180->120, the profile fix, faceted couloirs, arete 4->3 and a noise field returning a third of its range. An explanation that dissolves the measurement while leaving the complaint standing is the move this project keeps making. RULING — I7 MEASURES RIDGE PERSISTENCE, NOT RIDGE COUNT. The floor of 3 against a generator input of 3 is my own never-equal-the-floor rule broken, but the deeper fault is that I7 WAS MEASURING AN INPUT: L0_ARETE_COUNT is a number design hands the generator, and counting it back out checks the detector rather than the mountain. Measured quantity becomes DESCENT DEPTH — the span of relief over which a crest survives before the flank swallows it — over eight levels across the banded zone. MASSIF_ARETE_DESCENT_MIN = 0.50 of relief, DERIVED: a rib must run from near the summit (0.85) to the cliffline (MASSIF_CLIFFLINE_FRAC 0.33) because below that §2.8.7 permits a grassy apron and ribs are supposed to die into talus; span 0.52 rounded to 0.50. The count survives only as a guard against the coincidence and as a FRACTION, ceil(2/3 * L0_ARETE_COUNT) = 2 of 3 — accepting core's measured 2-of-3 detector limit is not accommodation ONLY BECAUSE THE SUBSTANCE MOVED TO PERSISTENCE; lowering the floor and still counting would have been. Both controls per Rule 30 and the lead's corollary — MUST FAIL: a cone, and the case the old I7 could not reject, A SMOOTH CONE WITH A FACETED CAP whose corners appear at 0.85 and are gone by 0.70 (descent ~0.15), which is exactly the dome-with-a-sharp-hat the frames kept showing; MUST BE ABLE TO PASS: three ribs each running summit to cliffline. Why this version predicts I11: a rib dying above 0.55 leaves the lower two-thirds of the silhouette smooth and that is most of what a standing eye sees, so descent depth is the object-space quantity whose failure PRODUCES I11's. The 21:29 guard is discharged — re-scoping I7's levels was conditional on I11 existing, and it exists. THE CONVEXITY CAP PROTECTS NOTHING and there is no veto to lift: n*tan(pi/n)/pi is arithmetic (the isoperimetric ratio of a regular n-gon) that arrived as a CONSEQUENCE of core's support-function construction, not a rule design imposed, and I have never ruled that a massif must be convex. What I barred is ELONGATION as a knob for passing I8, which stands and is about ridge-versus-peak, not convexity. The model is ALREADY non-convex in plan; what it cannot express is PROTRUSION. So protrusion gets a BUDGET in quantities we already measure: (1) SINGLE SUMMIT via topographic prominence, MASSIF_SPUR_PROMINENCE_MAX = 0.20 of relief (23 m on Ravenscar) — above that the massif reads as a cluster of hills, which C4 forbids; (2) C1 visibility, no new constant, spurs may grow until LANDMARK_VISIBILITY_MIN 0.6 binds and Ravenscar has 0.751 to spend; (3) elongation, unchanged and the one hard bar; (4) the castle spur is a BENEFICIARY — protrusion IS §2.8.7's release valve done deliberately. Where: the south-west three-quarters, since the crag sits 194 m from the east edge and 200 m from the north edge and has no long sightlines from those bearings at all. CONSTANTS RE-DERIVED AFTER THE FIELD FIX. MASSIF_PROFILE_EXPONENT_MIN 1.3 -> 1.5, and the rule it belonged to was WRONG ON THE ARITHMETIC: h = H*(1-d/R)^p runs from H to 0 for EVERY p, so mean envelope slope is H/R = 43.8 deg regardless and p CANNOT MAKE A FLANK GENTLER OR STEEPER — it only moves where the steepness sits along the radius. §2.8.2's 'the low-p sector is the gentle flank that carries the ascent' is false in both halves: the low-p sector is the most UNIFORM flank, the one closest to the constant gradient I4 exists to reject. The gentle flank must come from R(theta) or from the benches, and §7.1b already says the benches carry the ascent. The broken field did not merely hide the low-p half, it hid the fact that the low-p half was never going to do its assigned job. Nothing pulls p down and the anti-dome argument pulls it up; derived against I1's own 12 deg floor on the envelope basis at H=115/R=120: p=1.0 gives 0.0 deg (the cone control failing as it must), 1.2 gives 9.5 (fails), 1.3 gives 13.4 (passes by 1.4 — no margin, and it survived only because the field never generated it), 1.5 gives 19.8 (1.65x the floor), 2.2 gives 34.4. _MAX 2.2 untainted and unchanged. AND THE BOUNDS ARE NOT ENOUGH — Rule 31 in full: the fixed bearing_field is a normalised sum of cosines and is BELL-SHAPED about 0.5, not uniform, so a [1.5, 2.2] range drawn from it concentrates p near 1.85 and delivers little asymmetry — 'only the top 60%' replaced by 'mostly the middle'. Design does not get to specify the noise function, so the requirement is stated on the OUTCOME: MASSIF_PROFILE_ASYMMETRY_MIN = 10 deg of spread between the steepest and shallowest per-bearing I1 steepening across the 64 radials (middle-half field yields ~7 and fails; the full range yields ~14.6 and passes). It has a control — a cone reads 0 deg of spread — and it cannot be satisfied by a peaked field pretending to be a spread. THE 20 DEG BREAK THRESHOLD RE-CONFIRMED ON A DIFFERENT BASIS: the upper bracket (below MASSIF_ARETE_TURN_MIN 50 deg, since a plan-view aspect turn always projects to a smaller silhouette break) is geometry and is untouched; THE LOWER BRACKET IS WITHDRAWN AS A BASIS because core's 10-15 deg noise floor was measured on the broken field and 'where counts stop responding' is a property of sub-readable band structure drawn from that same field. Re-derived perceptually instead: at the 1/30 rad window a break is a direction change between two ~17.5 px segments, where orientation discrimination is unambiguous well below 20 deg, so 20 sits above the perceptual floor and below the 50 deg ceiling with no measured curve in it. Standing control: the cone must read 0 AND the threshold sweep must show counts FALLING as the threshold rises — counts that do not fall are core's own jitter signature and mean the detector is measuring itself. Rule 14 gap flagged: the 20 deg is a LITERAL in the probe while MASSIF_SILHOUETTE_BREAKS_MIN gives a count with no magnitude — requested MASSIF_SILHOUETTE_BREAK_TURN_MIN = 0.35 rad. MASSIF_SLOPE_BIN_MAX 0.30 STANDS with its PROVENANCE replaced rather than its value moved: re-derived from the landform (a banded scarp puts risers across three steep bins at ~22% each and benches across two at ~17%, so the intended mountain's fullest bin lands near 22%) instead of from the dead 33.2% dome reading. I4 is not the thing to relax and its eight-seed failure is NOT to be diagnosed until re-measured on the fixed field, since the band-of-angles spread that was supposed to satisfy it has never actually been exercised. THE SUITE IS NOT A SCOREBOARD — the object-space invariants are NOT retired, and the honest answer is more uncomfortable than retirement: they are nine correct measurements of nine different things, SEVEN OF WHICH THE USER NEVER COMPLAINED ABOUT. The complaint has two halves, 'siska' (convex profile: I1, I10) and 'rebra' (angular structure: I7, I11), and A PERFECTLY CONCAVE PERFECTLY SMOOTH MOUNTAIN PASSES I1/I3/I4/I5/I10 AND READS AS A SMOOTH MASS — concave and smooth are not exclusive, so there was never a contradiction between suite and frame to resolve. THE TWO INVARIANTS THAT ENCODE THE ACTUAL COMPLAINT ARE EXACTLY THE TWO THAT FAIL ON EVERY SEED. The defect was in the REPORTING FORM: 'seven of eight', 'nine of eleven', 'six of ten robust' weight every invariant equally when only some protect the thing under complaint. RULING: a suite is reported as a list with its LOAD-BEARING member named for the complaint in hand, never as a score. STANDING DEBT, mine: NINE OF TEN INVARIANTS HAVE NO CONTROL and Rule 30 is retroactive or it is decoration — requested of core as one cheap batch, the whole suite against a cone and against a smoothstep dome, with my predictions recorded BEFORE the measurement so they can be wrong (I1/I3/I4/I5/I6/I8 and the repaired I7 reject the cone; I2 and I10 PASS it, I2 because a cone's flank satisfies a summit-slope test with no summit in it and I10 because its proper reject case is a pancake). THE I7/I11 TRADE needed no ruling: I11 read 1,1,0,0 without ruling 3 and 2,1,2,0 with it, failing from every bearing either way, so there was never a trade because ONE ARM NEVER CROSSED THE BAR. General form recorded — a trade between two invariants is only a trade if BOTH readings cross their thresholds; two failing numbers moving in opposite directions is one regression and one coincidence. And §2.8.2 ruled I7 and I11 are ONE failure, so A CHANGE THAT MOVES ITS TWO SYMPTOMS IN OPPOSITE DIRECTIONS HAS NOT TOUCHED THE MECHANISM (Rule 32) — the test to apply to the next candidate fix before it is measured. Ruling 3's PRINCIPLE stands (it is Rule 33 and is upstream of §1.6.1's whole derivation); what is withdrawn is the implementation and the distance it was sized against — it was sizing crest structure for 600 m, a range belonging to a mountain that does not exist.
- 09:08:2026 - 22:46:49: THE COMPLAINT IS CLOSED AND THE ANSWER IS NOT THE MOUNTAIN. I opened both west 300 m frames myself. Trees off: a pointed tor with its tower nub, a concave left flank carrying band lips at ~2/3 height, a long straight right ridge with a distinct shoulder break, the castle reading on its spur — THE GEOMETRY IS RIGHT. Trees on: a low featureless hump. NEW §5.12 — RULED FOR LEVER 2 (the apron), and it is not a clearing, it is a landform. TWO FAILURES, NOT ONE: (1) THE FOOT IS EATEN — a mountain missing its bottom third loses the bench and the flare and what survives is the upper cap, WHICH IS CONVEX ON ANY MOUNTAIN WHATSOEVER, so this is the dome and no shape change can fix it; (2) VALUE MERGING — canopy and backlit rock on one value, so the eye sees one dark mass whose outline is the union of both. Failure 1 produces the user's word, and HUE SEPARATION CANNOT FIX IT: telling tree from rock does not give back the bottom third. MEASURED MECHANISM: the pine annulus begins at 140 m, INSIDE the 120-162 m hem where the massif is still climbing — pines do not start at the foot, they start ON it — while the only treeless rule, on_crag_treeless, fires only at d < 120 AND h >= 57.5 m, an elevation gate high on the mountain. THE BAND BEING EATEN HAS NO RULE AT ALL; the strip duty cycle is an ANGULAR gap, not a radial standoff. Rule is DERIVED, never a tabled radius (§7.1a's trap, my fourth): no tree is placed where its canopy top would obscure the massif's silhouette below MASSIF_CLIFFLINE_FRAC from any acceptance standpoint — C1-B restated as a placement predicate on machinery that already exists. IT ADDS CONTENT: the apron is exactly where §5.10's floor classes belong (scree, boulder fields, big bushes, snags, deadfall, stunted sub-cliffline pines), and a bare ring would be worse than the forest — a talus apron with scrub and stone is a better landscape than closed pine to the hem INDEPENDENTLY OF ANY INVARIANT, because that is what erosion puts there. The §7.1b ascent benefits: a worn path across open scree reads as a path, through closed pine as nothing. LEVER 1 (density) RULED NOT THE LEVER because it is ALREADY BUILT and the frame still fails — TREE_SPACING_FOREST 12-18 is consumed (oak 15 m lattice, pine 14 m), which against the previous 5-8 m is 5.3x sparser BY AREA, more than the user's «не менее чем в трое»; the user's ruling landed and was not enough near a landmark, said plainly so nobody spends the fix twice. Density is not the binding constraint, PROXIMITY TO THE MASSIF is. LEVER 3 (hue) NECESSARY BUT NOT SUFFICIENT and re-scoped into two different defects: the source colours ALREADY differ strongly in hue (PINE_DARK is teal-green at saturation 0.45, every rock tone neutral at 0.05), but (a) CHROMA DISCRIMINATION COLLAPSES AT LOW LUMINANCE so lever 3 is weakest exactly where the problem is, at the backlit hour where both surfaces are lit by ambient alone; and (b) THE 64-COLOUR PALETTE HAS NO CONIFER RAMP — its eight ramps are grass greens, dry olive, dirt browns, rock greys, sand tans, sky blues, water teals, neutrals, so PINE_DARK (0.12,0.22,0.19) must quantise into GRASS GREENS whose dark end is a yellow-green with B=0.04 against pine's 0.19. THE SINGLE MOST COMMON DARK MASS IN THE WORLD HAS NO SHADE OF ITS OWN AND THE PALETTE DESTROYS THE ONE AXIS THAT SEPARATES IT FROM ROCK. Design requirement to render: the shipped palette carries a conifer ramp. Ranked so implementation order is not a judgement call: (1) the apron, which restores the mountain; (2) the conifer ramp, which stops the merge and is cheap; (3) nothing further on density. §5.10 IS UNBUILT — THE LR's POSITION A SECOND TIME, checked in source: snags, big bushes, fallen logs, deadfall, floor scarps and maturity tiers all have constants (16 rows) and meshes and ZERO consumers; the scatter alphabet has five members and bushes and stones are BOTH barred from inside a forest mass, so THE FOREST FLOOR TODAY IS BARE TERRAIN SPLAT AND NOTHING ELSE. Unlike the LR_ rows none carries a «НЕ ПОСТРОЕНО» marker, so the registry reads as though this is built — requested of the lead that they be marked, because a numbers table that overstates what exists is how a zone spends three sessions tuning an absent object. NEW §1.3b — C1 MEASURES OCCLUSION, NOT LEGIBILITY. A landmark can be 100% unoccluded and invisible and we now have the frame that proves it; C1 has been certifying a property its raycast cannot see, and the 0.751 I offered as spur budget one message ago is denominated in the wrong currency. C1 is NOT retired but RE-SCOPED: LANDMARK_VISIBILITY_MIN 0.6 remains a floor on OCCLUSION and stops being cited as a legibility figure; until the new instrument exists every C1 number is treated as UNSHOT — recorded, not certifying. THRESHOLDS FOR THE TWO-NUMBER INSTRUMENT, mine: C1-A OUTLINE FIDELITY — per screen column compare the DRAWN horizon to core's TERRAIN-REFERENCE horizon, faithful within one readability window, LANDMARK_OUTLINE_FIDELITY_MIN = 0.90 AND no contiguous unfaithful run longer than one readable unit; THE RUN-LENGTH CLAUSE IS LOAD-BEARING AND THE FRACTION IS THE GUARD, because a fraction alone is satisfiable by a tree wall that eats one whole flank while 90% of the outline stays clean, which is exactly the west 300 m frame — derived from §1.6.1's feature budget, since a contiguous loss over one unit deletes a break together with its flanking run and an unflanked break is not detected. C1-B BODY EXPOSURE — the failure no horizon test can see, because trees shorter than the crest never touch the horizon and eat the body while C1-A reads clean: the silhouette must be exposed, neither occluded NOR value-merged, continuously from MASSIF_CLIFFLINE_FRAC of relief to the summit across >= LANDMARK_EXPOSURE_COLUMNS_MIN = 0.90 of columns, foreground permitted below the cliffline. Derived from constants already here rather than invented — §2.8.7 ruled the body the eye reads as mountain BEGINS at the cliffline and §2.8.8's I7 requires ribs to descend to that same line, so the cliffline is already this document's boundary between mountain and hem. THE VALUE TEST: two regions are SEPARATE at >= LANDMARK_SEPARATION_STEPS_MIN = 2 palette steps, or >= 1 step across a ramp CHANGE. Two and not one, by the same doctrine as I11's 20 deg — one step is the quantisation floor, two surfaces within one step are LITERALLY THE SAME COLOUR after the post, so a one-step criterion measures the quantiser rather than the image. Computed before any frame: PINE_DARK luminance 0.197 against the darkest rock stop 0.192 — ZERO STEPS. And THE TEST IS RUN WITH THE PALETTE ON: settings.cfg has palette=0, so every frame this stage was shot without the quantiser and all of them are the OPTIMISTIC case — the shipped look is worse than what we have been judging. §1.5 — THE MISSING HALF OF THE SKYLINE RULE, and its absence cost this stage a session: «value against sky» governs the landmark's OUTLINE, where the competing surface is bright sky and separation is free, and says NOTHING about the landmark's BODY, where the competing surface is another dark mass in front of it and THE SAME DOCTRINE INVERTS — value becomes the weakest axis available, and backlit dark against backlit dark is the weakest separation there is, which is precisely the hour §7.1b's verdict frame deliberately picks. Every landmark brief now states its separation from its usual FOREGROUND as well as its backdrop, and where that foreground is vegetation the separation is carried by hue and silhouette scale, never by value. §1.6 — NEW CONDITION F6, RESOLUTION: an acceptance frame is judged at INTERNAL_RES (640x360), and the crag frames are 2560x1440 — four times the linear resolution, sixteen times the pixels — so every readability judgement in this document, all of it angular and calibrated to 640x360, has been made with 4x the resolution the player has. It cuts toward FLATTERY: the band lips visible on the trees-off upper left flank are one to two pixels at INTERNAL_RES. Frames are captured at or downsampled to INTERNAL_RES before any acceptance judgement and record the resolution they were shot at; whether these were captured at INTERNAL_RES and upscaled is RENDER'S TO STATE and I have not read that path. Same defect as §1.6.1 in the other axis — there I measured at a distance nobody derived, here we judge at a resolution nobody declared. §1.3 — NEW BOX, C4 IS NOT A DOCTRINE GAP BUT AN UNENFORCED RULE WITH A STALE CONSTANT (regression, reported not patched): in the west 300 m frame the near pines stand THREE TO FOUR TIMES the L0's apparent height, against a rule that says in as many words that nothing which is not the L0 may exceed its apparent height INCLUDING CANOPY. The world's occlusion model — the sight-wedge filter that rejects trees, and the canopy height field feeding the C1 raycast — hard-codes OAK_MAX_H 12, PINE_MAX_H 18, BIRCH_MAX_H 10, while the world is built with 32 / 38 / 22. EVERY OCCLUDER IS MODELLED AT ROUGHLY HALF ITS DRAWN HEIGHT, pine at 2.1x under. §5.7's tall-tree ruling landed in render and never reached the world's occlusion model, which is Rule 32 exactly. So C1 = 0.751 carries TWO independent defects: wrong currency (§1.3b) and a world model half the height of the world. Every tree standing inside an L0 sight wedge is there because the filter thought it was 18 m tall — THE WEDGES DID NOT FAIL, THEY WERE LIED TO. No new rule and no threshold change requested; the heights are core's to source from the same constants render uses, and C1 and the wedge rejections are re-measured afterwards rather than assumed to scale.
- 09:08:2026 - 22:55:05: Three corrections from the lead, two of them to me, plus the sequencing worked through. §1.6 F6 — SATISFIED, and the lead settled it by CHECKING rather than routing it: settings.cfg carries internal_resolution=640x360, tools/run_tour.sh shoots at DFN_INTERNAL_RES=640x360, and the PNGs are 2560x1440 = exactly 4x in both axes, an integer framebuffer upscale, so THE FILES CONTAIN NO DETAIL THE PLAYER DOES NOT HAVE — a one-pixel band lip is four file pixels, magnified rather than invented. The frames stand, the sweep is not re-run, the condition stays because it would have caught a native capture. MY «CUTS TOWARD FLATTERY» READING IS WITHDRAWN AND THE WAY I GOT IT WRONG IS WORTH MORE THAN THE CLAIM WAS: I correctly refused to assert the premise — I wrote that the capture path was render's to state and that I had not read it — AND THEN BUILT A CONCLUSION ON IT IN THE SAME BREATH. Flagging a premise as unchecked does not make it checked. Rule 34 in its subtlest costume: not reasoning from a premise I believed, but from one I had explicitly labelled unknown, as though labelling discharged it. The rule is CHECK IT OR DRAW NOTHING FROM IT. What survives is a different and better point: the band lips are ~1 internal pixel, far under SILHOUETTE_MIN_PX 8, so they are genuinely VISIBLE AS VALUE TEXTURE ON THE BODY and are NOT READABLE STRUCTURE — exactly the distinction §2.8.7 drew about ribs reading as value rather than silhouette. VISIBLE IS NOT READABLE, and only the second satisfies a criterion. §1.3b — MY «THE SHIPPED LOOK IS WORSE» FRAMING WAS WRONG (lead's correction): settings.cfg ships palette=0 and that is the default the game writes on first run, so THE FRAMES ARE NOT OPTIMISTIC RELATIVE TO WHAT SHIPS — THEY ARE WHAT SHIPS TODAY. What is actually unresolved is whether the quantiser is meant to ship on at all, which nobody has ever decided and which is now a user call. DESIGN'S POSITION ON THE RECORD FOR THAT DECISION: it should ship ON, because §1.5's entire readability doctrine («with the limited palette, tiers separate by value») is WRITTEN ASSUMING A LIMITED PALETTE, so with the quantiser off that doctrine has no premise and several rules here lose their basis — BUT THE CONIFER RAMP IS A PRECONDITION, NOT A FOLLOW-UP, since turning it on today would make the pine/rock merge WORSE because pine has no ramp of its own to quantise into. Order: ramp, then quantiser. And the separation threshold is made independent of that pending decision: THE TEST RUNS WITH THE PALETTE ON AND THAT CERTIFIES BOTH CONFIGURATIONS, because the quantiser can only ever MERGE neighbouring colours and never split them, so separation measured with it on is a LOWER BOUND on separation with it off. §1.5 — AND THE LIMITED PALETTE ARGUES FOR HUE, NOT AGAINST IT, a correction to this document's own «value contrast over hue» from reading what the palette actually is: the 64-colour post is 8 RAMPS x 8 SHADES, so it quantises HUE into eight large well-separated families and VALUE into eight fine steps within each. A RAMP CHANGE IS THEREFORE THE COARSEST AND MOST ROBUST SIGNAL THE PALETTE CAN CARRY AND A STEP CHANGE IS THE FINEST AND MOST FRAGILE. «Value contrast over hue» is sound as general low-res art direction and BACKWARDS FOR THIS PALETTE: under quantisation two things on different ramps can never merge, while two things on the same ramp merge as soon as they land within a step. That is the whole argument for the conifer ramp — a ramp is the strongest separation available and the single most common dark mass in the world does not have one. §5.12 — THE APRON IS NOT BARE BY CONSTRUCTION, IT IS A HEIGHT RULE AND NOT A CLEARING, so it does NOT have to wait for §5.10. Sequencing was raised on the reasonable fear that an apron shipped early would be the bare ring I warned against; the arithmetic does not support the fear. C1-B requires the silhouette exposed above MASSIF_CLIFFLINE_FRAC, so a tree is illegal only if ITS CANOPY TOP SUBTENDS MORE THAN THE CLIFFLINE DOES from an acceptance standpoint — a real constraint rather than a formality, because a near tree sits much closer to the eye than the mountain behind it. Worked at d_accept as illustration and NOT as a tabled number: cliffline 0.33 x 115 = 38 m, subtending 38/360 = 0.106 rad from 360 m; a tree on the near hem (~162 m from centre) stands ~198 m from the eye; legal canopy top 0.106 x 198 = ~21 m. PINE IS 28-38 m AND IS EXCLUDED; EVERYTHING UNDER ~21 m IS ADMITTED — big bushes, scrub, stunted and young pines, stone. So the apron is populated in its very first version and §5.10 ENRICHES it rather than being its precondition, and blocking the one fix that addresses the user's complaint behind an entirely unbuilt feature set would be the wrong trade. Revised order: tree heights fixed in the occlusion model -> apron (interim, with the classes that already exist) -> §5.10 floor classes enriching it -> conifer ramp -> quantiser decision. ONE REQUIREMENT ON CORE, stated because I checked and it is not free: Bush and Stone are BOTH currently barred from inside a forest mass and the apron band lies inside the pine annulus, so THE APRON IS A DISTINCT GROUND-COVER CLASS, NOT AN ABSENCE OF FOREST, and bush and stone placement must be admitted there — whether that is a small change is core's to say, not mine. The interim apron is honest rather than a stopgap: a hem of scrub, stone and young pine under a cleared skyline IS what a talus apron looks like, and nothing about it has to be undone when snags and deadfall arrive.
- 09:08:2026 - 23:14:47: NEW §2.9 PALE SPIRES — flora's white-rock reuse ACCEPTED as a landform class, REJECTED as proposed on two counts. The world genuinely lacks a spire and pale rock is the one value role nothing else in the palette occupies, so the class is right. NOT ON RAVENSCAR'S TALUS APRON: talus is loose, angular, actively-moving debris and nothing tall and thin stands in it — a spire is IN-SITU bedrock left by differential erosion, i.e. the survivor, and talus is the pile of what it survived. Deeper objection is procedural: I ruled for the apron in §5.12 because it is right INDEPENDENTLY OF ANY INVARIANT, so putting hoodoos in it because they happen to be available and happen to solve a contrast measurement would be the exact thing I spent this stage arguing against — placing a feature because it answers a number. The apron gets scree, stone, scrub and stunted pine. NOT AT 16-22 m ON A 0.4-0.9 m BASE: that is ~25:1, which §1.5 already forbids (nothing structural thinner than ~0.5 m matters beyond 100 m; sub-pixel verticals shimmer), and a 0.8 m spire subtends 0.008 rad at 100 m against a 3.3 m readable size — FOUR TIMES UNDER THRESHOLD while being the brightest value in the scene, the worst possible thing to alias. The user's own word was «острые пики»; a real hoodoo is nearer 5:1. THE GEOMETRY CANNOT BE SCALED INTO READABILITY BECAUSE THE DEFECT IS THE RATIO, NOT THE SIZE. THE RULE THAT SETTLES BOTH OF FLORA'S CONSTRAINTS AT ONCE, computed from the palette before anything is placed: pale spire luminance 0.869 against bright sky 0.790 is 1.10x and UNUSABLE, against mid rock 0.371 is 2.3x, against PINE_DARK 0.197 is 4.4x and maximal. RULING — a pale spire is sited only where it reads against ROCK or CANOPY and MAY NEVER BREAK THE SKYLINE. It is the exact inverse of the crag: Ravenscar reads against sky at 3-4x and merges into dark foreground, the spire reads against dark foreground at 4.4x and vanishes against sky at 1.10x — two objects, two opposite backdrops, one rule each, and §1.5's «every landmark brief states its value contrast against its usual backdrop» finally earns its keep. This makes flora's constraint 2 AUTOMATIC (something that may never break the skyline can never out-angle the L0 crown — no new test, no new constant) and bounds the rarity problem STRUCTURALLY rather than by budget, since a spire in front of dark canopy is a local guide exactly where the forest has none and it is IMPOSSIBLE to site it as a false weenie, a weenie being a thing that breaks the horizon. THE CLASS: name «pale spires», always a GROUP and a single spire is never generated (my own character-not-existence rule); tier L1 minor; SPIRE_HEIGHT_MIN/MAX 8-14 m; SPIRE_GROUP_SPAN_MIN/MAX 10-16 m; SPIRE_COUNT_MIN 3; quantises to the neutrals ramp, alone among landforms. THE GROUP IS THE READABLE UNIT AND IT IS THE GROUP THAT IS SIZED AGAINST THE DISTANCE — Rule 33's third instance after the summit tor and the crest structure — so individual spires may stay thin, exactly as a stand of trees reads as a mass while every trunk is sub-readable, but the GROUP's span must clear SILHOUETTE_MIN_PX; 10-16 m is readable to ~300-480 m, squarely in §1.3's L1 band. Everything else flora measured is KEPT: concave taper 0.70, pentagonal faceting, 0.18 rad per-stem sweep, x1.6 flare, and especially the 1 m SELF-BURIAL, since a rock that plants itself into a slope is precisely right and is a property a tree only barely needed; the stepped lead-stem ratio 0.74-0.96 is what makes a group read as a group rather than a fence and is the best thing in the proposal. Siting derived and never tabled: candidates drawn where the backdrop test passes — inside and at the edges of forest masses, at the foot of the lakeshore bluff, along the river's cut banks; never on a ridgeline, never on the apron, never inside an L0 sight wedge. IT IS STONE, and three properties must not follow it across — no wind (a swaying rock is a bug that will ship), no seasonal palette (§5.11's foliage contract must never reach it), and not placed by the vegetation scatter pass, since its siting predicate is the backdrop test and no tree has one. Flora keeps the generator as the cheapest home; if those three are easier to guarantee in render's rock family, hand the parameters over — the requirement is the separation, not the owner. TWO FLAGS: story hears about it BEFORE it lands per the §2.8.7 precedent, since a distinctive pale formation acquires a name and then acquires canon; and the user may have meant «белые скалы» LITERALLY — white CLIFFS, not spires, since «скала» carries both — so a PALE ROCK BAND IN THE SPLAT PALETTE is recorded as a different, cheaper, complementary idea answering the same sentence, a §4 item and not a mesh.
- 09:08:2026 - 23:23:29: User answered both open calls. «БЕЛЫЕ СКАЛЫ» MEANS BOTH — spire groups AND a pale rock surface — so the §4 item I recorded rather than resolved in my own favour is now authorised work, and NEW §4.1 THE PALE ROCK STRATUM is written. IT IS NOT DECORATION, IT IS THE MISSING MATERIAL HALF OF §2.8: the user's original brief was «высоту надо задавать линиями уровня», §2.8.2 answered it in GEOMETRY, and the frame that refuted the suite complained of «ONE material band, not a rhythm» — which §4 has never had an answer to, because rock has been a single grey since it was written. A pale stratum makes the contour lines visible as MATERIAL, the layer the complaint was actually about. RULE: pale rock is a STRATUM, exposed where terrain cuts through its elevation, selected by slope >= SLOPE_ROCK_MIN AND height inside a band — no new shader input, height is already there. STRATA ARE DEFINED IN ABSOLUTE WORLD HEIGHT, GLOBALLY, NEVER AS A FRACTION OF EACH LANDFORM: the same layer must appear at the same elevation on the crag, the lakeshore bluff and the river's cut banks, because A BAND AT A FIXED HEIGHT EVERYWHERE READS AS GEOLOGY AND A BAND AT A FRACTION OF EACH LANDFORM READS AS PAINT. Third instance of absolute-versus-relative after the couloir scale and the summit tor. It is a MODULATION OF THE ROCK MATERIAL, not a fifth splat layer, so §4's four-material budget is untouched. It survives quantisation BY CONSTRUCTION — grey rock on the rock-greys ramp, pale rock on neutrals, so a stratum boundary is a RAMP CHANGE, the strongest separation the palette carries and one that two things on the same ramp could never have. Sized by Rule 33: at d_accept 360 m readable is 12 m so a stratum thinner than ~12 m is stripe noise; Ravenscar's banded zone is 77 m, two to three pale bands give rhythm without corduroy, hence ROCK_STRATUM_PERIOD 28-36 m and ROCK_STRATUM_PALE_FRAC 0.35-0.45, SEEDED AND NON-UNIFORM with the same CV discipline as MASSIF_BAND_SPACING_CV_MIN because a fixed period would rebuild the wedding cake in paint. ROCK_PALE sits between grey rock (~0.37) and spire white (~0.87) and at least one palette step below the spire, since THE SPIRES MUST REMAIN THE BRIGHTEST VALUE IN THE WORLD or a cliff face of spire-white drowns the L1 formation the brightness was doing work for — C4's hierarchy argument applied to the palette instead of to height. NEW §4.2 THE RAMP BUDGET, answering render: NEITHER ANSWER THEY OFFERED IS THE FIRST THING TO TRY. THE BUDGET IS 64 ENTRIES, NOT EIGHT FAMILIES OF EIGHT — ramp depth should follow the lighting range a family carries and the screen area it covers, which are wildly unequal; grass, rock, neutrals and sky need their depth, sand serves a shore mask and water a 90x140 m lake, so reclaiming two shades each from the small families funds a conifer ramp WITHOUT DELETING ANYTHING. Follows directly from §1.5's correction: if a ramp change is the coarsest signal and a shade step the finest, TRADING SHADES FOR RAMPS IS FAVOURABLE BY DEFAULT and the uniform 8x8 grid is the one thing in the palette nobody has justified. IF uniform depth is structural in the shader the sacrifice is DRY OLIVE, and the reason is not aesthetics: §4's material list has no dry or upland grass, so DRY OLIVE IS A RAMP RESERVED FOR A BIOME THAT DOES NOT EXIST — capacity held for an unbuilt thing while a built thing goes without, the LR's mistake in colour space. Render's own argument (its dark end sits 0.046 from grass dark, the closest cross-ramp pair) is correct and I verified it, but «serves nothing that exists» is the stronger reason. Biome objection answered in advance: biomes will need several new families and the palette is re-derived wholesale, so holding one ramp today does not prepay that. ALL RAMPS CONVERGE AT THE DARK END AND THAT IS A DOCTRINE, NOT A DEFECT — checked arithmetically against the actual failing case before ruling: with a conifer ramp PINE_DARK quantises to a conifer entry 0.027 away rather than a rock entry 0.090 away, so the class of merge IS genuinely fixed and separation against lit and mid-tone rock goes from zero to ~3.1 shade steps; BUT against rock in shadow it is ~0.9 steps and still merges, because every ramp runs toward black and the darks are crowded by construction. THE CONIFER RAMP WILL NOT FIX THE BACKLIT VERDICT FRAME AND NOTHING IN THE PALETTE CAN — which confirms §5.12's ranking from an independent direction: the apron is the fix, the ramp is the hardening, and a colour cannot un-hide a mountain whose base has trees in front of it. General rule: VALUE AND HUE SEPARATION BOTH VANISH AS LUMINANCE GOES TO ZERO, SO IN DEEP SHADOW THE ONLY THING THAT SEPARATES TWO SHAPES IS SILHOUETTE — which is why §1.5's skyline rule exists and why a landmark's read must never depend on its foreground being a different colour, only on there being no foreground. §1.3b — SEPARATION CRITERION TIGHTENED and render's question is what exposed the hole: «2 steps OR 1 step across a ramp change» treats a ramp change as a guarantee when it is only a heuristic, since ADJACENT RAMPS TOUCH AT THEIR DARK ENDS (dry olive sits 0.046 from grass green there, less than a single shade step), so «different ramp» can be a label rather than a distance. Restated as the distance between the two QUANTISED ENTRIES in mean shade steps, >= 2 — same constant, same value, correct basis, the identical act as I1's re-spec from surface mean to envelope. §1.5 — MEASURE WITH THE QUANTISER ON, CERTIFY BOTH, now the general rule after the user made the 64-colour post a MENU SETTING: the player may select either configuration so every readability rule is written for the worse of the two, which is always the quantised one because THE QUANTISER CAN ONLY MERGE AND NEVER SPLIT, so one measurement in the conservative configuration certifies both. This repairs a claim I made too strongly — I wrote that with the quantiser off the doctrine «has no premise»; with a SETTING the premise is CONDITIONAL RATHER THAN ABSENT and, because the rules are written for the worse case, THEY HOLD UNCONDITIONALLY. The doctrine is stronger as an option than it would have been as a default, which is not what I expected. The conifer ramp stops being a precondition for a decision and becomes simply REQUIRED, since some players will turn the quantiser on. §1.3b — C1 RE-MEASURED ON HONEST TREE HEIGHTS: 0.751 -> 0.6429 against a floor of 0.60, with three readings and the third must travel with it. (1) THE FIX LANDED — the number moved DOWN, the predicted direction once every occluder doubled in modelled height, and a figure that moves as predicted is evidence while one that does not is a second bug. (2) It passes by 0.043, a 7% margin, which is MARGINAL by this document's own standard — «a marginal pass on one seed is not compliance» — so the min/median/max across the twelve seeds is REQUESTED as §2.8.3 requires of every other invariant; if the median sits near the bound the forest moves, not the threshold, and the apron should raise it clear regardless. (3) IT IS STILL AN OCCLUSION NUMBER AND MUST NOT BE RELAYED AS «THE LANDMARK READS» — it is now an honest measurement of what it always measured, the legibility question is untouched and stays UNSHOT, AND THE FRAME THAT STARTED ALL OF THIS HAD C1 PASSING COMFORTABLY. NEW §1.6.5 — two conduct rules in transmissible form, since «I caught it in myself» does not transmit. A HEDGE IS A DEBT, NOT A LICENCE: naming a premise unverified obliges one to verify it or drop every conclusion resting on it, and A MESSAGE CONTAINING BOTH «I HAVE NOT CHECKED X» AND A CONCLUSION DEPENDING ON X IS SELF-REFUTING — mechanically detectable in one's own draft before sending. It cost this document one wrong finding which the lead settled in about a minute by reading the file I had declined to read. A CITATION IS A CLAIM ABOUT A DOCUMENT AT A MOMENT AND IT GOES STALE IN SILENCE: the stale tree heights carried CORRECT citations of a superseded ruling — the code said «§5.1: 8-12 m» and §5.1 had said exactly that before §5.7 moved it — so the code documented its provenance faithfully and was wrong anyway, and THE CITATION MADE IT HARDER TO SPOT, because a bare literal invites suspicion while a cited literal buys trust it has not earned. Design's share: a ruling that supersedes a numeric range says so explicitly and names the section it replaces, so a grep for the old section number finds the correction; and a number in this document is never the source of truth for code, so a reader who finds one in code has found a shadow, not a reference.
- 10:08:2026 - 00:00:28: RENDER MEASURED AND THREE OF MY CLAIMS DID NOT SURVIVE, INCLUDING THE PREMISE THE CONIFER FAMILY WAS ORDERED ON. They built a CPU mirror of the actual shader quantiser and re-ran every claim against both palettes; I REPRODUCED THEIR NUMBERS INDEPENDENTLY BEFORE ACCEPTING THEM and got the same figures to two decimals (2.18 lit / 0.74 shadow on the old palette, PINE_DARK -> water). (1) «PINE_DARK must quantise into grass greens» — FALSE, it lands on WATER TEALS, and it does so under the weighted AND the unweighted metric, so this was never a subtlety I missed but A CLAIM I NEVER COMPUTED AT ALL: I took it from a search report and made it load-bearing, which is the exact debt §1.6.5 names, incurred in the same document that names it. (2) «the three needle tones are merged» — FALSE, they land on three ADJACENT water entries, cleanly resolved. (3) «separation goes 0 -> 3.1 shade steps» — FALSE, it is 2.18 -> 2.24 lit and 0.74 -> 0.66 shadowed; the 3.1 used EUCLIDEAN RGB while the quantiser weights R/G/B at 0.30/0.59/0.11, and the «0» was the SHADOWED case relabelled as the general case, when lit rock already cleared the floor of 2 before any change. SO THE CONCLUSION I DREW — «nothing in the palette can fix the backlit frame» — IS CONFIRMED AND BOTH NUMBERS I USED TO REACH IT WERE WRONG; getting the right answer for the wrong reasons is not being right, and the only reason it cost nothing is that the ruling it supported (apron first) was load-bearing on other grounds. §1.5 + §4.2 — RENDER'S AMENDMENT ADOPTED: A SEPARATOR MUST MOVE RED OR GREEN, because hue that lives in BLUE is invisible to the quantiser at 0.11 weight — 0.2 of blue is 0.9 shade steps against §1.3b's floor of 2, while 0.2 of green is 2.1; green is 5.4x more effective per unit than blue and red 2.7x. THIS IS WHY NEEDLES AND WATER COLLIDED: blue-green water and green needles sit at nearly the same point in the (r,g) plane and their separation is almost entirely in blue, which the metric discards — TWO COLOURS THAT LOOK COMPLETELY DIFFERENT CAN BE IDENTICAL TO THE QUANTISER. It AMENDS §1.5 rather than contradicting it: «a ramp change is the coarsest signal the palette can carry» is a claim about THE EYE and it stands, but the quantiser decides which entry a colour reaches, it runs FIRST, and it does not use the eye's metric — so a separator must pass two tests, will the eye see it (favours hue) and will the quantiser preserve it (favours R/G), and a blue-only difference passes the first and fails the second. General transferable rule: THE PIPELINE'S OWN METRIC IS PART OF THE DESIGN VOCABULARY AND BELONGS IN THIS DOCUMENT, not discovered per-feature by whoever implements next. Checked in consequence: §4.1's pale-vs-grey rock is a VALUE change across near-neutral families, moving R and G together, so it holds up under the metric; same for the §2.9 spires. ALL FAMILIES CONVERGE AT THE DARK END — pine vs shadowed rock is ~0.7 steps on BOTH palettes, so NOTHING IN THE PALETTE CAN FIX THE BACKLIT VERDICT FRAME, now confirmed by measurement rather than argued, and render has PINNED IT AS AN ASSERTION that the shadowed case is BELOW 2 so the limit is recorded rather than quietly hoped away and the next agent does not spend a night in colour space. Value and hue separation both vanish as luminance goes to zero, so in deep shadow the ONLY separator is silhouette. AND THE LIT CASE IS ITSELF MARGINAL at 2.18 against a floor of 2 — 9% headroom by this document's own standard — so if pine/rock separation ever needs improving THE LEVER IS PINE_DARK'S OWN R/G POSITION, NOT THE PALETTE, and flora rebuilding conifers now is the cheap moment to move it. RULING — THE CONIFER FAMILY STAYS AND THE REASON IS ENTIRELY DIFFERENT. It was ordered to fix the pine/rock merge; it does not, and that merge was never as broken as I said. Still worth six entries, on a ground that survives measurement: AUTHORSHIP OF APPEARANCE — any element covering a significant fraction of the screen has its palette family chosen DELIBERATELY, because a family arrived at by nearest-colour accident is not a decision, moves whenever anything near it moves, and couples two unrelated materials so that changing one drags the other. THE FOREST WAS SHARING A FAMILY WITH WATER: a water look-dev change would have restyled every conifer in the world and nobody would have known why, and it is not hypothetical here since the river's source sits ~122 m from the crag centre and its trace runs out through the pine foothills, making pine-against-water a PRESENT frame case in this testbed. What it does NOT buy, stated so it is not re-claimed later: conifer and broadleaf already separated (oak -> grass greens on both palettes) and the three needle tones already resolved cleanly. DEPTH ALLOCATION — one measured amendment offered, render's to take or leave: their split is sand 8->5 and water 8->5, but WATER IS THE WORST PLACE TO SPEND IT, being the largest smooth gradient in the world where banding is most visible, while sand is a thin dithered shore strip and dry olive serves only bright-grass highlights on already-dithered ground. Measured per-shade spacing on the water family (smaller is smoother) — sand5/water5 gives 0.105 with pine/lit-rock 2.19; sand4/water6 gives 0.084 and 2.14; DRY OLIVE 5 / SAND 6 / WATER 7 gives 0.070 and 2.22, better on both axes at the same 64 entries. Banding visibility is a readability question and therefore mine; ramp construction is render's craft. AND I WITHDRAW THE REASON I GAVE FOR PICKING DRY OLIVE: I wrote that it «serves nothing that exists», and measurement shows BRIGHT GRASS AND DRY GRASS BOTH LAND ON IT — no material targets it, but the quantiser runs on the final image and pixels reach it, so it functions as the lit-grass extension. Third unverified claim in one section, which is why the amendment proposes REDUCING dry olive rather than deleting it. Render also declined my offer to steepen the pow(t,1.25) dark-end weighting, correctly: it changes every family at once and would invalidate every frame read tonight for a gain I myself called small.
- 10:08:2026 - 00:10:27: Two rulings and one provenance flag. §5 — BIRCH_CROWN_BASE_FRACTION 0.58-0.62 -> 0.40-0.45, AND THIS IS NOT A CHANGE OF RULING BUT THE FIRST APPLICATION OF MY OWN RULE. §5's derivation says «the SMALLEST value >= the walkability floor which satisfies CROWN_ASPECT_MAX», and 0.58 has NEVER been that value: it was derived when aspect was measured on the authored CONTAINER (2.30:1), flora then corrected the basis to GENERATED GEOMETRY where the birch measures 1.02-1.27 against a ceiling of 1.8, and the derived value was never recomputed against the corrected basis. My own NUMBERS note records the gap without my noticing what it implied — «берёза 0.58 при выведенных 0.09», with the ceiling only binding near 1.2. FOURTH INSTANCE OF THE FAMILY: A MODEL CHANGE CAN INVALIDATE A CONSTANT'S DERIVATION WITHOUT CHANGING ITS NUMBER, after I1's surface-mean->envelope re-spec, MASSIF_SLOPE_BIN_MAX's dead provenance and the profile exponent. FLORA'S SENTENCE IS THE ONE TO KEEP — THE MARGIN IS WHERE THE PALM LIVES: a value chosen «с огромным запасом» over its derivation is not safe, it is UNEXAMINED, and here the surplus confined foliage to the top 42% of the tree and built a palm. I OPENED BOTH FRAMES BEFORE RULING: at 0.58 the birch is a pale pole with a tuft on top, at 0.40 a slender light-crowned tree with visible branch structure inside the crown. The aspect ceiling cannot see the difference because A PALM AND A BIRCH CAN HAVE IDENTICAL CROWN ASPECT — what separates them is STRUCTURAL, which is why flora's limb-spread invariant is the right instrument and the aspect ceiling never was. MAX drops to 0.45 too (a range is two assertions), giving a 0.05-wide band whose lower end is the tested value and whose upper end is CROWN_BASE_FRACTION_MAX — so THE BIRCH EXCEPTION VERY NEARLY DISSOLVES, becoming simply the top of the general 0.35-0.45 band, which is what a slender water-margin tree should be; both ends measured before it ships, per the rule flora's own pine just demonstrated. Walkability untouched: 0.40 x 16 m = 6.4 m clear trunk against CANOPY_CLEARANCE_MIN 2.2, nearly 3x over. CORRECTED ONE FIGURE IN FLORA'S CASE because it will be quoted: 0.40 gives ~7.6 m of clear trunk on a 19 m birch, which is LESS than the 8.5 m the old 0.45 gave, not more — it does not change the ruling since 8.5->11 was a bonus I claimed and never a requirement, but the argument should not travel with an arithmetic slip in it. Recorded even though it does not apply: flora's principle that A SPECIES NOBODY WILL DEFEND BY EYE SHOULD NOT HAVE A CATALOG SLOT, which is «an invariant nothing fails is not an invariant» pointed at content instead of tests. §5 NEW — RULE 30 SHARPENED, THE CONTROL SHOULD BE THE REAL REJECTED ARTEFACT: flora's limb-spread invariant ships with a synthetic palm control at 0.06 against a 0.15 floor, which is Rule 30 done correctly, AND IT STILL PASSED THE TREE THE USER REJECTED (birch 0.17-0.19). A synthetic worst case is the EASY reject; the hard one is the artefact actually turned down, so when a real rejected instance exists IT is the control and the floor must sit above it. Recommended to flora, not ruled in their zone: re-measure limb spread on the repaired birch and if it lands at or above ~0.22 (the lowest accepted species) raise the floor between the rejected version and the accepted ones. A FLOOR PLACED BELOW EVERY REAL FAILURE IS A DESCRIPTION, NOT A TEST. §4.2 — RULING: TAKE CONIFER 8, paying with dry olive 4 and sand 4. Render built my proposed allocation, found WATER 7 STEALS THE LIT NEEDLE TONE BACK INTO THE WATER FAMILY, searched the space rather than guessing, and landed olive5/sand5/water8/conifer6 better than my proposal on both of my own axes — MY PRINCIPLE HELD AND MY ARITHMETIC DID NOT, the correct division and the second time tonight it ran that way. They then recorded the part that matters: water 7 fails, water 8 passes, and nothing about that is robust. Three reasons to pay for conifer 8. (1) THE FRAGILE VERSION DOES NOT DELIVER WHAT THE CHANGE WAS BOUGHT FOR — the justification is AUTHORSHIP OF APPEARANCE, that the forest's family must be chosen deliberately rather than fall out of a nearest-colour accident, and an allocation holding only because water happens to be 8 is STILL LEAVING THE FOREST'S FAMILY TO ACCIDENT: a different accident, not the absence of one. (2) THE INPUT IS ABOUT TO MOVE — flora is rebuilding conifers and the atlas tone is the knob the ramp is derived from, so a configuration holding only for today's exact tones BREAKS SILENTLY WHEN THEY SHIP, and breaks toward «the forest quietly becomes water-coloured again», which is the original defect; a silent regression into the bug a change was made to prevent is the worst available failure mode. (3) The cost lands where banding is least visible — a thin dithered shore strip and a highlight extension on already-dithered grass — and the gain lands on the largest dark mass in the world, which is my own banding-visibility principle applied consistently rather than only when cheap. Come back only if it costs something visible (sand banding on the shore is a readability regression I would rather hear than have absorbed); re-verify after flora's new needle tones land, since a derivation is only as current as what it was derived from. AND I COULD NOT CHECK THEIR TONE ARITHMETIC AND DID NOT FAKE IT: my reconstruction of their ramp disagrees with their measurements in BOTH directions, which tells me my reconstruction is unfaithful rather than that theirs is wrong; what I could verify structurally holds in every allocation (pine lands on conifer, oak stays on grass). §3.3 — PROVENANCE FLAG: core fixed a 16.6x DUPLICATION IN THE POND FILL, 17,336 lake planes -> 1,043, with 94.5% of water cells carrying multiple coplanar planes at CONFLICTING HEIGHTS, so the drawn water surface could disagree with water_surface_at and EVERY §3 FIGURE MEASURED AGAINST DRAWN WATER BEFORE THAT FIX IS PROVENANCE-DEAD, exactly like the slope histogram measured on the old dome — the 2.74% WaterBed coverage, anything derived from height_above_water near the shore including §4's SHORE_SAND_HEIGHT 0.6 m band, and the §2.7 finding that micro-relief «dropped bank dips under the water surface», which may have been reading a DUPLICATED plane at the wrong height. The cap itself does not move, being derived from SHORE_SAND_DIST and river width rather than from the measurement; what is withdrawn is the EVIDENCE OF VIOLATION, and it must be re-taken before anyone tunes the classifier against it. Report the re-measured coverage; do not assume it merely shrank.
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

> ### ⚠ C4 IS NOT A DOCTRINE GAP — IT IS AN UNENFORCED RULE WITH A STALE
> ### CONSTANT (regression, reported not patched, stage-4)
>
> In `screenshots/crag/06_crag_w_300m.png` the near pines stand **three to four
> times the L0's apparent height.** C4 says in as many words that *nothing
> which is not the L0 may exceed L0's apparent height from the main travel
> corridors — including canopy.* The rule is correct, it is implemented, and
> it is being violated grossly. **The reason is a stale constant.**
>
> The world's occlusion model — the sight-wedge filter that rejects trees, and
> the canopy height field that feeds the C1 raycast — hard-codes
> `OAK_MAX_H = 12`, `PINE_MAX_H = 18`, `BIRCH_MAX_H = 10`. The world is built
> with `OAK_HEIGHT_MAX` = 32, `PINE_HEIGHT_MAX` = **38**, `BIRCH_HEIGHT_MAX` =
> 22. **Every occluder is modelled at roughly half its drawn height — pine at
> 2.1× under.**
>
> - **§5.7's tall-tree ruling landed in render and never reached the world's
>   occlusion model.** That is Rule 32 exactly: a shared quantity was changed
>   for one consumer and left stale for the others.
> - **So C1 = 0.751 is not merely denominated in the wrong currency
>   (§1.3b) — it was computed on a world model half the height of the world.**
>   Two independent defects in one number, and the second is fixable tonight.
> - **Every tree currently standing inside an L0 sight wedge is there because
>   the filter thought it was 18 m tall.** The wedges did not fail; they were
>   lied to.
> - **Design does not ask for a new rule here and asks for no threshold change.
>   The heights are core's to source from the same constants render uses.**
>   Reported, not patched, and re-measure C1 and the wedge rejections
>   afterwards — do not assume the ratio scales.

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
>
> **PARTLY WITHDRAWN — §1.6.4 corrects this box on a measurement.** Residency
> is **chunk-granular, not metric** (Chebyshev radius in chunk units, clipped
> to extent), so the 717 m vantage failed on its **bearing**, not its range.
> And §1.6.1 shows Ravenscar's acceptance distance is **360 m, not 717 m** —
> so **LOD is not the precondition for the L0's own frames**, which are
> shootable tonight. The box stays true for the LR and for anything beyond
> ≈ 800 m, and §1.6.3 adds the harder fact: **the LR does not exist in the
> generator at all.**

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

### 1.3b C1 MEASURES OCCLUSION, NOT LEGIBILITY — the two-number instrument (ruling, stage-4)

**A landmark can be 100 % unoccluded and invisible, and we now have the frame
that proves it.** `LANDMARK_VISIBILITY_MIN` is a raycast test: it asks whether
a line from the eye to the landmark is interrupted. Nothing in it can see that
two unoccluded masses have merged into one dark shape. So **C1 has been
certifying a property it cannot measure**, and every figure it has produced —
including the 0.751 I offered as spur budget one message ago — is denominated
in the wrong currency.

**C1 is NOT retired.** Occlusion is a real and necessary condition, and the
raycast measures it correctly. It is **re-scoped**: `LANDMARK_VISIBILITY_MIN` =
0.6 remains a floor on occlusion and **stops being cited as a legibility
figure.** Until the instrument below exists, every C1 number is treated the way
§1.6.3 treats UNSHOT rules — recorded, not certifying.

> **C1 RE-MEASURED ON HONEST TREE HEIGHTS (core, stage-4): 0.751 → 0.6429
> against a floor of 0.60.** Three readings of that number, and the third is
> the one that must travel with it:
>
> 1. **The fix landed.** The number moved *down*, which is the predicted
>    direction once every occluder doubled in modelled height. A figure that
>    moves as predicted is evidence; one that does not is a second bug.
> 2. **It passes with 0.043 — a 7 % margin — and that is MARGINAL by this
>    document's own standard.** «A marginal pass on one seed is not
>    compliance», and I do not know how many seeds this is. **Request: the
>    min/median/max across the twelve, as §2.8.3 requires of every other
>    invariant.** If the median sits near the bound, the forest moves, not the
>    threshold. The apron (§5.12) should raise it well clear regardless.
> 3. **IT IS STILL AN OCCLUSION NUMBER AND MUST NOT BE RELAYED AS «THE LANDMARK
>    READS».** It is now an honest measurement of the thing it always measured.
>    The legibility question is untouched and stays UNSHOT until C1-A/C1-B
>    below are built — and the frame that started all of this had C1 passing
>    comfortably.

**The instrument, approved by the lead and built jointly by render and core:
core's terrain-only horizon as the REFERENCE curve, the drawn frame's horizon
measured in VALUE. Two numbers, because occlusion and merging are demonstrably
different failures. The thresholds are mine and are below.**

##### C1-A — OUTLINE FIDELITY (does the drawn outline belong to the mountain?)

Per screen column across the landmark's angular span, compare the **drawn**
horizon to the **terrain-reference** horizon. A column is FAITHFUL when they
agree within one readability window (1/30 rad).

> **`LANDMARK_OUTLINE_FIDELITY_MIN` = 0.90 of columns faithful (предложение —
> утвердить), AND no contiguous unfaithful run longer than one readable unit.**

**The run-length clause is the load-bearing one and the fraction is the guard.**
A fraction alone is satisfiable by a tree wall that eats one whole flank while
90 % of the outline elsewhere stays clean — which is exactly the failure in
`screenshots/crag/06_crag_w_300m.png`. Derived from the feature budget of
§1.6.1: at d_accept the landmark spans ≈ 20 readable units and must carry six
silhouette features, so a contiguous loss of more than one unit can delete a
break together with its flanking run, and a break that is not flanked is not
detected.

##### C1-B — BODY EXPOSURE (is the mountain's *body* there, or only its cap?)

**The failure this exists to catch:** a mountain missing its bottom third loses
the bench and the flare of the base, and what survives is the upper cap — which
is convex on **any** mountain. **You do not need a domed mountain to get a
domed silhouette; you need a mountain with its base hidden.** No horizon test
can see this, because trees shorter than the crest do not touch the horizon at
all — they eat the body while C1-A reads clean.

> **The landmark's silhouette must be exposed — neither occluded NOR
> value-merged — continuously from `MASSIF_CLIFFLINE_FRAC` of its relief to its
> summit, across ≥ `LANDMARK_EXPOSURE_COLUMNS_MIN` = 0.90 of its columns
> (предложение — утвердить).** Below the cliffline, foreground is permitted.

**Derived from constants already in the document, not invented.** §2.8.7 ruled
that the body the eye reads as mountain **begins at the cliffline** and that the
apron below may flare; §2.8.8's I7 requires ribs to descend to that same line.
The cliffline is already this document's boundary between *mountain* and *hem*,
so it is the right place to put the exposure floor. Forest at the very hem is
legitimate — forest above the cliffline is not.

##### The value test — when are two masses "merged"?

> **Two adjacent regions are SEPARATE when the two colours they QUANTISE TO lie
> ≥ `LANDMARK_SEPARATION_STEPS_MIN` = 2 mean shade steps apart in the shipped
> palette.**

**TIGHTENED from my original wording («2 steps, or 1 step across a ramp
change»), and render's question is what exposed the hole.** A ramp change is a
strong *heuristic* for separation, not a guarantee of one: **adjacent ramps
touch at their dark ends** — dry olive sits 0.046 from grass green there, which
is **less than a single shade step** — so «different ramp» can be a label
rather than a distance. Measuring between the **quantised entries** is what the
criterion always meant, it subsumes the ramp-change case (a genuine ramp change
clears two steps easily), and it closes the loophole. **Same constant, same
value, correct basis** — the identical act as I1's re-spec from surface mean to
envelope, and it cost nothing because the number was never the problem.

**Two, not one, and the reason is the same doctrine as I11's 20°:** one step is
the quantisation floor — two surfaces within one step are *literally the same
colour* after the post, so a one-step criterion measures the quantiser rather
than the image. A threshold must sit above its own noise floor.

- **The measured case, computed before any frame was shot:** `PINE_DARK`
  luminance **0.197**, darkest rock stop **0.192**. **Zero steps.** In shadow,
  in a crack, or on a north-facing riser, pine canopy and rock are the same
  value by construction. The hue axis *does* separate them at source (pine
  saturation 0.45 against rock's 0.05) — see §5.12 for why that does not save
  the backlit frame.
- **THE TEST IS RUN WITH THE PALETTE ON, AND THAT CERTIFIES BOTH
  CONFIGURATIONS.** The quantiser can only ever *merge* neighbouring colours,
  never split them, so **separation measured with the palette on is a lower
  bound on separation with it off.** Passing with it on implies passing with it
  off. Running it in the conservative configuration therefore makes this
  threshold **independent of the undecided question below**, which is the
  property a threshold should have.
- **MY «THE SHIPPED LOOK IS WORSE» FRAMING WAS WRONG — the lead's correction,
  and it matters.** `settings.cfg` ships `palette=0`, which is the default the
  game writes on first run. **So the frames are not optimistic relative to what
  ships; they ARE what ships today.** What is actually unresolved is whether
  the quantiser is meant to ship on at all, which nobody has ever decided.
  **Design's position, for the record and for that decision: it should ship
  ON** — §1.5's entire readability doctrine («with the limited palette, tiers
  separate by value») is *written assuming a limited palette*, so with the
  quantiser off that doctrine has no premise and several rules in this document
  lose their basis. **But the conifer ramp is a precondition, not a follow-up:
  turning the quantiser on today would make the pine/rock merge worse, because
  pine has no ramp of its own to quantise into** (§5.12). Order: ramp, then
  quantiser.
- **Two shapes can occlude nothing and still merge.** That sentence is the
  reason there are two numbers and not one.

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
- **THE MISSING HALF OF THE SKYLINE RULE (added stage-4, and its absence cost
  this stage a session).** «Value against sky» governs the landmark's
  **OUTLINE**, where the competing surface is bright sky and value separation
  is enormous and free. It says nothing about the landmark's **BODY**, where
  the competing surface is **another dark mass in front of it** — and there the
  same doctrine inverts: **value is the weakest axis available, not the
  strongest.** Backlit dark against backlit dark is the weakest separation
  there is, which is precisely the hour §7.1b's verdict frame deliberately
  picks. **Rule: every landmark brief states its separation from its usual
  FOREGROUND as well as from its backdrop**, and where that foreground is
  vegetation the separation is carried by **hue and by silhouette scale**,
  never by value (§5.12, §1.3b).
- **AND THE LIMITED PALETTE ARGUES FOR HUE, NOT AGAINST IT — a correction to
  the bullet above, from reading what the palette actually is.** The 64-colour
  post is **8 ramps × 8 shades**. So the palette quantises *hue* into eight
  large, well-separated families and *value* into eight fine steps within each.
  **A ramp change is therefore the coarsest and most robust signal the palette
  can carry, and a step change is the finest and most fragile.** «Value
  contrast over hue» is sound as general low-resolution art direction and it is
  **backwards for this palette**: under quantisation, two things on different
  ramps can never merge, while two things on the same ramp merge as soon as
  they land within a step. **This is the whole argument for the conifer ramp
  (§5.12): a ramp is the strongest separation available, and the single most
  common dark mass in the world does not have one.**
  - **PRECONDITION, and without it the sentence above is a trap (render's
    amendment, measured, §4.2): A SEPARATOR MUST MOVE RED OR GREEN.** The
    quantiser weights R/G/B at **0.30 / 0.59 / 0.11**, so a hue difference
    living in blue moves almost nothing — 0.2 of blue is 0.9 shade steps, 0.2
    of green is 2.1. **A ramp change is the strongest signal to the EYE; it is
    only a signal at all to the QUANTISER if it moves R or G, and the quantiser
    runs first.** Two colours that look utterly different can be identical
    after the post. Every separation claim in this document is checked against
    the metric, not against the hue wheel.
- **MEASURE WITH THE QUANTISER ON, CERTIFY BOTH — the general rule (user
  decision, stage-4: the 64-colour post is a MENU SETTING, neither on nor off
  by fiat).** The player may select either configuration, so **every
  readability rule in this document is written for the worse of the two.** That
  is always the quantised one, because **the quantiser can only ever MERGE
  neighbouring colours and never split them** — so any separation measured with
  it on is a **lower bound** on the same separation with it off. **One
  measurement, in the conservative configuration, certifies both.** §1.3b's
  separation threshold was already arranged this way; it now carries the whole
  doctrine rather than one number.
  - **This repairs a claim I made too strongly.** I wrote that with the
    quantiser off this section's doctrine «has no premise» and several rules
    lose their basis. With a *setting* the premise is **conditional rather than
    absent**, and because the rules are written for the worse case **they hold
    unconditionally.** The doctrine is stronger as an option than it would have
    been as a default — not what I expected, and recorded as such.
  - **The conifer ramp stops being a precondition for a decision and becomes
    simply required.** Some players will turn the quantiser on, so shipping
    without it ships a configuration in which the most common dark mass in the
    world has no shade of its own.

### 1.6 THE ACCEPTANCE FRAME — what a frame certifies, and from how far (doctrine, stage-4)

§7.1's oldest clause is **the frames outrank the numbers.** That clause is only
safe while a frame is a *verdict*. This stage shot four frames and **not one of
them was**: the 717 m verdict frame had no mountain in it, the 287 m rhythm
frame was owned by a pine stand, the frame-2 hour lit the subject backwards,
and the 400 m frame I ruled a dome from turned out to be attributable to a
generator bug (§2.8.8). Four for four is not bad luck; it is a missing
definition. This section supplies it, so the clause keeps the authority it has
earned.

**A frame is a VERDICT on its subject only if all five conditions hold. A frame
that fails any of them is a DIAGNOSTIC — useful, often decisive, but labelled,
and never relayed upward as the state of the world.** Render labelled the 400 m
shot a diagnostic correctly and I then reasoned from it as a verdict anyway;
the label only works if the reader honours it.

| # | Condition | Fails when |
|---|---|---|
| **F1** | **RESIDENCY** — the subject exists in the world and in memory at the standpoint | the 717 m frame: chunks not loaded |
| **F2** | **BUDGET** — the subject subtends enough readable units to *afford* the features under test | the 600 m I11 row: twelve units for a six-feature test |
| **F3** | **AUTHORSHIP** — everything the frame credits or blames is authored | any frame whose composition clause rests on unauthored fBm |
| **F4** | **STANDPOINT** — derived, C1-credited, on reachable ground, at eye height | the tabled (545, 165) inside a pine stand |
| **F5** | **LIGHT** — chosen to expose *this frame's own* failure mode | frame 2's first hour |
| **F6** | **RESOLUTION** — judged at the resolution the player plays at | every frame this stage, judged at 2560×1440 |

F4 and F5 were ruled in §7.1b and are unchanged. F1–F3 and F6 are new.

##### F6 — AN ACCEPTANCE FRAME IS JUDGED AT `INTERNAL_RES`, NOT AT WINDOW SIZE

`INTERNAL_RES` is **640×360**, and every readability judgement in this document
is angular and calibrated to it (`SILHOUETTE_MIN_PX` = 8 px ⇒ readable size =
distance/30). A frame judged at a higher resolution than the game draws credits
the subject with structure the player's screen cannot resolve.

- **RULING: an acceptance frame is captured at, or downsampled to,
  `INTERNAL_RES` before any acceptance judgement is made**, and the frame
  records the internal resolution it was shot at. A frame that does not state
  its resolution is a diagnostic.
- **SATISFIED by this stage's sweep — checked by the lead, not assumed.**
  `settings.cfg` carries `internal_resolution=640x360`, `tools/run_tour.sh`
  shoots at `DFN_INTERNAL_RES=640x360`, and the PNGs are 2560×1440 = **exactly
  4× in both axes**, i.e. an integer framebuffer upscale. **The files contain
  no detail the player does not have — a one-pixel band lip is four file
  pixels, magnified rather than invented.** The frames stand and the sweep is
  not re-run.
- **MY «CUTS TOWARD FLATTERY» READING IS WITHDRAWN, and the way I got it wrong
  is worth more than the claim was.** I correctly refused to assert the premise
  — I wrote that the capture path was render's to state and that I had not read
  it — **and then built a conclusion on it in the same breath.** Flagging a
  premise as unchecked does not make it checked. That is Rule 34 in its
  subtlest costume: not reasoning from a premise I believed, but reasoning from
  one I had explicitly labelled unknown, as though labelling it discharged it.
  The rule is *check it or draw nothing from it.*
- **What survives, and it is a different point:** the band lips on the
  trees-off flank are ≈ 1 internal pixel, far under `SILHOUETTE_MIN_PX` = 8.
  They are genuinely **visible as value texture on the body** and they are
  **not readable structure**, which is exactly the distinction §2.8.7 drew when
  it explained why ribs read as value on the body rather than as silhouette.
  Visible ≠ readable, and only the second one satisfies a criterion.
- **Keep the condition.** It would have caught a native-resolution capture, and
  the failure it guards against is the same one as §1.6.1 in the other axis:
  there I measured at a distance nobody derived; here the risk was judging at a
  resolution nobody declared.

#### 1.6.1 F2 — AN ACCEPTANCE DISTANCE IS A PROPERTY OF THE LANDMARK, NEVER OF THE PROJECT

**This is the ruling the stage was missing, and it invalidates the number the
whole evening was measured against.**

§1.5 fixes the readable feature size at `distance / 30`. Define one **readable
unit** as that size. A landmark of base radius `R` seen from `d` subtends
`2R/d` radians, which is

> **U(d) = 60·R / d readable units.**

A view-space invariant does not demand a *shape*, it demands a **feature
count**, and every feature costs readable units — its own, plus the flanking
run on each side that the detector needs to establish a tangent before and
after it. I11 demands three interior breaks, and its outline also carries an
apex and two hem junctions that consume budget without being counted: **six
features.** At roughly two units apiece that is **twelve units with zero
slack**, and this document has twice refused zero slack («a marginal pass on
one seed is not compliance», «a generator input must never equal the floor of
the invariant that checks it»). Taking the same ≈ 1.6× margin those rulings
took:

> **`LANDMARK_ACCEPTANCE_UNITS_MIN` = 20 readable units (предложение —
> утвердить)**, whence the acceptance distance for a silhouette-structure
> frame is **d_accept = 60·R / 20 = 3·R.**

**Derived first, then checked — not fitted.** The measurement was run
afterwards and agrees: I11 fails from one bearing at 400 m (18 units), passes
everywhere at 300 m (24 units) and reads 9–12 against a floor of 3 at 253 m
(28 units). The break-even sits between 18 and 24 units and 20 is inside it.

| Landmark | R | d_accept = 3R | The number that was actually used |
|---|---|---|---|
| **L0 Ravenscar** | 120 m | **360 m** | **600 m** |
| **LR temple massif** | 260–310 m | **780–930 m** | 600 m |

**So the 600 m acceptance distance was written for the LR and then applied to
the crag, and the LR does not exist** (§1.6.3). Read straight down that table:
600 m is *conservative* for the mountain it was written for and *impossible*
for the one it was used on. The evening's headline — «the massif reads as
broken rock up close and as a smooth mass from the valley» — is that mismatch,
measured. It is not a finding about Ravenscar's shape.

- **Corollary, and it is the honest cost of this ruling: RAVENSCAR IS TOO SMALL
  TO HAVE A FAR FRAME DISTINGUISHABLE FROM ITS NEAR FRAME.** §7.1b's two frames
  were split 717 m / 287 m on the premise that «the dome failure and the
  constant-gradient failure are invisible to each other's distance». At 3R the
  verdict frame lands at 360 m and the rhythm frame stays at 287 m, so the two
  frames now differ by **their clause and their light, not by their range.**
  That is not a defect in the frames; it is what a 120 m landmark is. A far
  frame is a thing only a large landmark has.
- **Metric structure and angular structure have DIFFERENT acceptance
  distances, and the frames must not be given a shared one.** A band pair
  (riser + bench ≈ 28 m) is sized in metres and is readable to 28 × 30 ≈ 840 m;
  a silhouette break is sized in units of the landmark and is readable to 3R.
  Frame 2's clause therefore survives far past frame 1's. **The binding
  distance is always the clause's, never the frame's.**
- **This does NOT retire Rule 33; it completes it.** Rule 33 says detail is
  sized against the viewing distance. F2 says the viewing distance is itself
  derived — from the landmark and from the invariant's feature count. Sizing
  detail against a distance nobody derived is how a 5 m tor came to sit on a
  190 m mountain.
- **A landmark photographed beyond its own d_accept certifies nothing about its
  shape.** It certifies that the engine can draw it. That is a render result
  and it is reported as one.
- **Acceptance distances are DERIVED, NEVER TABLED** — the same rule §7.1b
  already imposed on vantages, for the same reason, now extended to the range.
  A tabled distance is a tabled coordinate wearing a different hat.

#### 1.6.2 F3 — what a frame of unauthored backdrop actually certifies

**Measured in the generator, not assumed** (`WorldgenMacro.cpp`,
`TestbedLayout.h`, `App.cpp`): the world is 4 × 4 chunks of 256 m = **1024 m
square**, closed by physics walls at its edge. Inside it, authored influence is
**a set of stamps with hard finite footprints** — the massif reaches 162 m from
its centre and returns exactly zero beyond that; the troughs reach 96–128 m;
the bumps and the lake basin end at their own radii. **Everywhere else, inside
the box as much as outside it, the terrain is three octaves of value noise with
a valley redistribution, 0–31.5 m of relief, and nothing else.** «Authored» is
not a region of the map. It is a union of footprints, and it is small.

Rulings:

- **Unauthored FOREGROUND does not invalidate a frame.** Ground is allowed to
  be ground; §2.7 governs it and it is not the subject. A verdict frame on
  Ravenscar crosses hundreds of metres of fBm and is unharmed by it.
- **Unauthored BACKDROP invalidates every COMPOSITION clause.** Hierarchy
  contrast (C4), dominance (C2), depth separation (§1.3a) and «reads against
  rock rather than against sky» (§6.1) are all claims about the *relationship
  between two authored masses*. A frame containing one authored mass and a
  field of noise cannot carry any of them, whatever it looks like.
- **Unauthored terrain touching the subject's OUTLINE contaminates the
  silhouette clause specifically**, because a noise knoll behind the crag
  enters the horizon polyline and is read as the crag's own crest. I11 already
  guards this by limiting its azimuth sweep to the subject's own angular extent
  × 1.35; **that guard is hereby general — every view-space test states the
  angular window within which it attributes structure to its subject.**
- **A frame whose subject is unauthored is a RENDER test, not a design test.**
  It certifies draw distance, streaming, fog and palette. Legitimate, valuable,
  and not evidence about any rule in this document.
- **Therefore: extending the world does not extend the authored world.** A
  2 × 2 km map with the same five stamps in it has exactly as much design in it
  as the 1 km map does, and four times as much backdrop. Growth of the map is a
  render and streaming milestone; growth of the *authored* world is a placement
  pass, and only the second one moves any rule in this bible.

#### 1.6.3 F3, second half — a landmark rule written for 4 km, in a world 1 km across

**Ruled plainly, because the alternative is to keep quoting numbers that mean
nothing: NO RULE IN §1.3 OR §1.3a IS CURRENTLY EITHER PASSING OR FAILING BEYOND
THE AUTHORED EXTENT. IT IS UNSHOT.**

- **`LANDMARK_MAX_DISTANCE` = 4000 m is a SITING CEILING and a depth-precision
  bound. It is not, and never was, a legibility specification.** Its derivation
  — beyond this a landmark is backdrop, and this bounds what the depth buffer
  must resolve — is untouched and stands. What it cannot do is certify
  anything, because nothing has ever been sited past 1 km: **there is nowhere
  to put it.**
- **Every C1 / C2 / C3 / C4 figure in this document is a statement about a
  1024 m world** and is to be reported with that extent attached. They are not
  wrong. They are narrower than they read.
- **THE LR DOES NOT EXIST IN THE GENERATOR.** Core established by search that
  no code path reads any `LR_` constant; the world contains three raised
  landforms — the crag, a +6 m knoll and a +10 m bluff. Every ruling I have
  made about the temple massif — its relief, its base radius, its ascent, its
  seven landings, its haze separation from the L0 — is **unvalidated by
  construction.** §1.3a's whole depth-separation doctrine needs two authored
  landmarks and there is one.
- **Design stops refining LR numbers until an LR stamp exists.** Tuning a
  constant for an absent object is the purest instance of the defect this whole
  stage is about, and I have been doing it all evening. `LR_BASE_RADIUS` landed
  tonight against a shape nobody has generated. The §2.8.7 line «the LR is
  worse and is the cheap one to fix» was right that it is cheap and wrong about
  what it was: not a fix, a specification.
- **NEW STATUS CATEGORY, and it is the reporting half of Rule 30.** An
  invariant or rule is **PASSING**, **FAILING**, or **UNSHOT** — and **UNSHOT
  never enters a count.** «Seven of eight» and «nine of eleven» were both
  produced by counting rules of unequal standing and unequal shootability as
  interchangeable tallies. A suite is reported as a list with its load-bearing
  member named (§2.8.8), never as a score.

#### 1.6.4 F1 — residency is chunk-granular, and it does NOT block Ravenscar

**Correcting §1.3a's box on a measurement rather than leaving it to stand.**
`ChunkManager` loads chunks within `CHUNK_LOAD_RADIUS` = 2 measured as
**Chebyshev distance in chunk units** around the focus, and clips everything
outside the 4 × 4 extent. So «the world stops existing at 512 m» is the right
order of magnitude and **the wrong shape**: residency is a square in chunk
space, not a radius in metres.

- For a subject in chunk (3, 0) — Ravenscar — the residency-legal standpoints
  are **x ≥ 256 m and z < 768 m**, and the greatest legal distance to it is
  ≈ 808 m, only from the south-west.
- **Frame 1's tabled vantage (120, 300) is illegal on its BEARING, not on its
  RANGE** (chunk x = 0 is three chunks from the subject). I record this as a
  **prediction from reading `ChunkManager.cpp`, to be checked by render before
  anyone relies on it** — I have not read how a probe sets its streaming focus,
  and whether the focus follows the camera or the player is render's to state,
  not mine to assume.
- **LOD is NOT the precondition for Ravenscar's acceptance.** At the derived
  d_accept of 360 m the verdict frame is shootable tonight, and render's
  253 m / 300 m acceptance sweep already is. §1.3a's box stays true for the
  LR and for anything beyond ≈ 800 m, and is **withdrawn as a blocker on the
  L0's own frames** — it was blocking them at a distance that was never theirs.
- **`CHUNK_LOAD_RADIUS` still must not rise for a screenshot**, and design
  still does not ask for it. That part of the box was right.

#### 1.6.5 Two conduct rules this stage earned, in transmissible form

Both were learned by being caught, and «I caught it in myself» does not
transmit. These are the greppable versions.

**A HEDGE IS A DEBT, NOT A LICENCE.** Naming a premise as unverified creates an
obligation to verify it or to drop every conclusion resting on it. **A message
containing both «I have not checked X» and a conclusion that depends on X is
self-refuting**, and that is mechanically detectable in one's own draft before
sending. This is Rule 34's subtlest form: not reasoning from a premise one
believes, but from one already labelled unknown, as though the label discharged
it. It cost this document one wrong finding (F6) which the lead settled in
about a minute by *reading the file I had declined to read*.

**A CITATION IS A CLAIM ABOUT A DOCUMENT AT A MOMENT, AND IT GOES STALE IN
SILENCE.** The stale tree heights in the occlusion model carried **correct
citations of a superseded ruling** — the code said «§5.1: 8–12 m» and §5.1 had
said exactly that, before §5.7 moved it to 24–32 m. **The code documented its
provenance faithfully and was wrong anyway**, and the citation made it *harder*
to spot: a bare literal invites suspicion, a cited literal buys trust it has not
earned. Design's share of the fix, since this document is what gets cited:

- **When a ruling supersedes a numeric range that has appeared in this
  document, the ruling says so explicitly and names the section it replaces**,
  so a grep for the old section number finds the correction.
- **A number in this document is never the source of truth for code — NUMBERS.md
  is** (Rule 14). Where a §5 or §2 brief quotes a value, it is quoting, and a
  reader who finds it in code has found a shadow, not a reference.

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

**FACETS ARE PLANAR BY CONSTRUCTION, AND A COULOIR IS A PAIR OF FACETS RATHER
THAN A DENT IN ONE (ruling, stage-4 — this is the model answer to I11's
distance failure, and it is the same failure as I7).**

**I7 failing on every seed and I11 failing at 600 m are ONE failure, not two.**
A tangent break is scale-free: a genuine corner between two flat faces reads as
a corner at any distance, because the chord on either side lies along a
straight facet however wide the measurement window grows. **Breaks can only
wash out with distance if the facets themselves are curved** — then a wider
window swallows the corner into the surrounding curvature and the outline
becomes one smooth mass. That is exactly what 5/8/12/4 breaks at 300 m
collapsing to 1/1/0/0 at 600 m means, and it is exactly what I7 says directly:
`MASSIF_FACET_TURN_MAX` is the flatness test, and it has never passed. **The
facets are not flat.** Everything else follows.

- **Seeded variation perturbs the polygon's PARAMETERS, never the radius
  continuously across a facet.** Draw each facet's support distance `d_i` and
  bearing `α_i` once, per facet, from the seeded field. Do **not** modulate
  `R(θ)` with a continuous per-bearing term — that is precisely what bends a
  flat face into an arc, and it is why the support-function construction, which
  is polygonal by definition, has been producing curved facets anyway.
- **A couloir ADDS two facets; it does not dent one.** A notch with its own two
  planar walls preserves flatness and **adds corners**, where a smooth
  re-entrant subtracts them by curving the face it sits in. This also retires
  the tension core measured earlier — deepening couloirs dropped arêtes 4 → 0
  *because* they were dents. As facet pairs they raise I8 and I11 together.
  Everything on the massif is then flat faces meeting along lines at every
  scale, which is the user's «рёбра» and this document's own definition of an
  arête, finally built the way both are written.
- **Crest structure is sized against the ACCEPTANCE DISTANCE, not against the
  massif.** A facet's arc must exceed the readability window at the far
  acceptance range (≈ 20–24 m at 600–717 m) so a corner survives as a corner
  out to where the valley looks at it. **Second instance of this rule — the
  summit tor was the first**, and two is a pattern: **detail sized against the
  object shrinks out of legibility as the object recedes. Anything required to
  read at distance is sized against the distance.**
- **The corner count follows for free:** only limb-facing facets contribute
  breaks, so four arêtes alone put barely two corners on the outline. Couloirs
  as facet pairs raise that above I11's floor without touching
  `L0_ARETE_COUNT`, and without hitting §2.8.2's convexity cap, because a
  re-entrant notch is exactly what makes the section non-convex.

**A PER-BEARING FIELD MUST BE VERIFIED UNIFORM OVER ITS DECLARED RANGE (core's
proposal, adopted — it is the distribution-shaped version of the reject-case
control).** Core measured **0 % of samples below 0.4**, then lumps of 26 % at
0.6 and 30 % at 0.8, against a raw lattice uniform to a tenth of a percent.
Every "seeded spread" in this model was silently using the top 60 % of its
declared range, lumpily. What that cost, all of it invisible until measured:
the profile exponent never approached `MASSIF_PROFILE_EXPONENT_MIN`, **so the
gentle-flank half of §2.8.2's asymmetry rule never existed at all**; cliff
risers were never drawn near `MASSIF_CLIFF_SLOPE_MIN`, leaving 4 % in the
50–60° bin — a hole exactly where mass was expected; and the 0.5 cliff/ramp
split was arithmetic about a coin that was never fair. **A spread that is
secretly peaked passes every review, and no invariant in this document names
it.** Assert the distribution, not just the bounds.

**CLIFF RISERS ARE PLANAR BUT NOT IDENTICAL — THE ANGLE VARIES BETWEEN BANDS
(ruling, stage-4; this is my own I3 fix colliding with my own I4 rule).** After
the aspect cascade, I4 fails on eight seeds of twelve: a genuinely steep massif
concentrates surface in the 60–70° bin. The cause is not the steepening, it is
**planarity plus a single cliff angle.** I ruled risers planar so they would
stop spending their width on sub-cliff slope (I3) — and a planar face puts
*all* of its area at *one* angle, so if every cliff riser uses the same angle,
every riser's surface lands in the same 10° bin. My two rulings were fighting,
and I4 is the invariant that noticed.

**Fix, and it moves no threshold: the CLIFF class is a BAND OF ANGLES, not an
angle.** Each riser stays planar and stays above `MASSIF_CLIFF_SLOPE_MIN`, but
draws its angle from a seeded spread across the steep range. I3 is unaffected —
every riser is still a cliff — while the surface spreads across several bins
and I4 is satisfied by *variety* rather than by *shallowness*. This is also the
truer landform: a real banded scarp has faces at 55°, 70° and overhanging in
the same massif, not one repeated angle. It is the bench rule from below,
applied above: **`MASSIF_CLIFF_SLOPE_MIN` is a floor, and a floor was never a
target** — precisely what I said about `MASSIF_BENCH_SLOPE_MAX` being a ceiling.

**I4 is sound and is not the thing to relax.** Core asked whether I4 might now
be a texture rule being asked to constrain a legitimately uniform form. It is
not: «перепады не должны быть постоянными» is *exactly* the complaint that a
uniformly 65° massif re-creates at a steeper angle. A constant gradient is a
constant gradient whatever its value — which is the same conclusion §2.8.2
reached when dead-flat benches and ceiling-pinned benches each put most of the
mountain in one bin. **This is the third variant of that identical failure, and
each time the fix was variety rather than a different constant.**

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
| **I7** | **RE-SPECIFIED — see §2.8.8. I7 now measures RIDGE DESCENT DEPTH, not ridge count**, because a floor of 3 against a generator input of 3 was checking the detector rather than the mountain. Text below is superseded. ~~Arêtes exist, are sharp, and persist~~ | on contours at 0.4/0.55/0.7/0.85 relief, an arête is a point where surface **aspect turns ≥ `MASSIF_ARETE_TURN_MIN` = 50° within `MASSIF_ARETE_TURN_ARC_MAX` = 15 m of arc**, flanked on both sides by **facets turning ≤ `MASSIF_FACET_TURN_MAX` = 15° over ≥ `MASSIF_FACET_ARC_FRAC_MIN` = 0.08 of that contour's perimeter**. Require ≥ `MASSIF_ARETE_COUNT_MIN` = 3, each detected on ≥ `MASSIF_ARETE_PERSISTENCE_MIN` = 0.6 of the four levels within `MASSIF_ARETE_BEARING_TOL` = 15° of bearing | **FAILS** — no angular structure at all |
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

- **EVERY INVARIANT SHIPS WITH A CONTROL: THE SHAPE IT EXISTS TO REJECT MUST
  FAIL IT (core's practice, ruled into the document — and it is the strongest
  process rule of this whole stage).** Core ran the I11 detector against a
  smooth analytic cone before trusting it, "because I have now been burned
  twice by trusting a detector", and **the cone scored exactly 3 against a
  floor of 3.** This is the *third* time this section has produced a test its
  own reject-case passes: raw contour-spacing CV scored 0.935 on the dome,
  footprint-weighted I3 scores its ideal at zero, and now I11. Three
  instances is not bad luck, it is a missing step. **A new invariant is not
  believed until the dome fails it.** The control is cheap — an analytic cone,
  a uniform cone, a pancake — and it is the only thing that distinguishes "my
  test passes" from "my test discriminates".
- **A VIEW-SPACE TEST IS EVALUATED AT THE RESOLUTION OF THE EYE IT STANDS IN
  FOR (core's rule, adopted verbatim in intent).** Their first I11 returned
  17–31 breaks whose count **did not fall as the threshold rose** — the
  signature of sampling jitter rather than structure, and a diagnostic worth
  keeping on its own. The window is the readability scale: a readable feature
  is `distance/30` metres, so its **angular** size is 1/30 rad at every
  distance, and structure finer than that cannot be something the player sees.
  This generalises past I11 to every future camera-side check.

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
  - **RULED, because the constant and my text disagreed by 8° (core caught it
    before it became a third round): I10 is measured FROM THE CLIFFLINE
    CONTOUR TO THE SUMMIT, and it is an ENVELOPE measure, never a surface
    mean.** NUMBERS.md defines `MASSIF_ASPECT_MIN` over the whole cone; my text
    said above the cliffline; those are different mountains. **My text wins and
    the constant's definition is corrected to match**, because I10's whole
    derivation is §4's rock threshold — and a grassy *apron* is not a defect,
    it is what §2.8.2's `p > 1` profile is for. Talus fans out. What must reach
    the rock threshold is the body the eye reads as mountain, which begins at
    the cliffline. **Envelope, not surface mean, for the reason I1 has just
    taught us the hard way** (below): a surface average over benches and risers
    can be steep while the outline bulges.
  - **SCOPE — I10 constrains the massif ABOVE the cliffline, and the apron
    below it may still flare.** Stated explicitly because the naive reading
    (115 m of relief over a total base radius under ≈ 137 m) is both more
    destructive than intended and geologically wrong: §2.8.2's `p > 1` profile
    exists precisely because **talus fans out at the bottom**. The upper cone
    must reach the rock threshold; the hem may lie where it lies. This
    materially shrinks the cascade below.
  - **Consequence, predicted not assumed — core measures it.** The upper
    radius comes in; how far the hem follows is a measurement, not an
    inference. Cascades onto the barrow (radius 103 m), the castle spur, the
    pine strips and the ascent length, re-validated per §7.0a's rule.
  - **THE CASTLE IS THE PLACEMENT AT RISK, and my earlier prediction about it
    is void.** §2.8.5 reasoned that R1 was safe because "the base radius is
    unchanged (180 m)". It is changing now, so that reasoning expires with it.
    The Ward sits ≈ 148 m from the crag centre: **if the hem contracts far
    enough, the castle is no longer on a spur of the massif at all but on flat
    ground beside it**, which fails §6.1.2's siting rule outright and — story's
    condition, and they are right to make it one — shrinks the angular
    envelope that keeps the fortress reading **against rock rather than
    against sky**. That envelope is the mechanism protecting the tower's
    skyline monopoly, which is the arc's central image. **Re-measure R1–R4 and
    the castle's ground from the valley standpoints after the reshape; do not
    assume 115 m of relief still buys the margin it bought at 180 m of
    radius.**
  - **THE APRON FLARE IS THE RELEASE VALVE — spend it before moving the
    castle.** Since I10 constrains only the body above the cliffline, the hem
    is free to stay out where it is, and **preserving the Ward's spur is a
    legitimate reason to let it.** Order of preference, binding on the
    re-siting pass: (1) flare the apron so the spur survives; (2) stop the
    contraction; (3) move the castle — **and (3) requires a story consult
    BEFORE it lands, not after.**
  - **Why moving the castle is the expensive option, and it is an asymmetry
    worth understanding rather than a preference (story's catch).** When the
    *barrow* moves, its satellites follow for free: the ward gap and the
    barrow-facing tower are **defined relative to the barrow** (§7.0a). When
    the *castle* moves, nothing follows — the ≈ 55 m barrow proximity, the
    yard/gate→barrow sightline and the act-1 trespass route are each defined
    between the castle and something that is **not moving with it**, so all
    three break independently. **A landmark whose dependents are defined
    relative to it is cheap to move; a landmark that is itself the fixed end
    of other relations is expensive.** That distinction should be checked
    before relocating anything in this document, not only here.
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
  - **I11 AS I FIRST WROTE IT WAS VACUOUS, AND I WROTE IT ONE MESSAGE AFTER
    DIAGNOSING THIS EXACT DEFECT TWICE (core's control, ruled).** A smooth
    analytic cone scores **exactly 3** at every standpoint and every distance —
    apex plus the two hem junctions — so `MASSIF_SILHOUETTE_BREAKS_MIN` = 3 was
    **satisfied by the dome the invariant exists to reject.** I had just
    finished withdrawing contour-CV and footprint-I3 for precisely this, and
    then built a third one. That is why the control rule above is now standing
    procedure rather than advice.
  - **RULING — count breaks on the INTERIOR of the horizon only**, excluding
    the apex and the two hem junctions: crest lines that meet sky *between* the
    outline's endpoints, which is what §7.1's "three readable crest lines"
    always meant. Under this reading the cone scores **0** and the reshaped
    massif scores **4–11** by standpoint. The reject case now fails at every
    threshold, which is the property the first version lacked.
  - **RULING — a break counts at ≥ 20° of tangent turn**, and the bracket is
    reasoned rather than picked off core's curve. Above: `MASSIF_ARETE_TURN_MIN`
    is 50° for a plan-view *aspect* turn, and a rib of given aspect turn always
    projects to a *smaller* tangent break in silhouette, so the silhouette
    threshold must sit below 50°. Below: core measured the noise floor around
    10–15°, where counts stop responding to the threshold. That brackets
    20–30°, and 20° takes the wider margin — 6 breaks against a floor of 3 at
    400 m, rather than 4 against 3 at 30°, which would violate the
    never-equal-the-floor spacing this document just adopted. **Provisional and
    the frames outrank it**, exactly as with `CROWN_ASPECT_MAX`.
  - **Why plan-view aspect turn (I7) does not imply a visible rib.** I7 finds
    four arêtes and the eye finds none, and both are correct. With only 3–5
    arêtes, a rib lies near the **limb** for a minority of bearings; from most
    viewpoints the outline is traced by a **facet**, whose profile is the
    smooth curve. So ribs read as **value structure on the body**, not as
    silhouette — except where a break is large enough to notch the outline.
    I7 measures a property the eye cannot see from the ground; I11 measures
    the property it can.
  - **BOTH OF MY SUSPECTED CAUSES ARE FALSIFIED, and I withdraw them as
    readily as I withdrew the lobing mechanism (core measured).** Radial
    excursion is **87.9 / 76.3 / 63.4 / 34.5 m** at the four levels against a
    13.3 m readable scale at 400 m — **the lobes are three to six times larger
    than they need to be**, so `MASSIF_RADIAL_LOBE_AMP` is delivering and the
    built surface is expressing it. Do not touch it. And the tightest corner
    radius of curvature is **2.8 m** against that same 13.3 m — the support
    polygon's corners are sharper than the eye can resolve. I7 is not passing
    on a rounded corner. **The ribs are big and sharp and still invisible**,
    which kills the two comfortable explanations and leaves the real one.
  - **Superseded — the original text of this bullet is kept below for the
    record.** Suspected second cause, to be measured before anything is tuned:
    §1.5 puts the readable feature size at ≈ distance/30, so an arête must
    stand **≈ 13 m proud at 400 m** and ≈ 24 m at 717 m. On paper
    `MASSIF_RADIAL_LOBE_AMP` should deliver that; whether the built surface
    does is unknown, because I8 reports a normalised perimeter ratio and
    **nobody has measured the raw radial excursion in metres.** A lobe ratio
    of 1.36 says nothing about whether the lobes are visible.

**I1 IS MEASURING THE WRONG THING, AND IT IS THE INVARIANT I CALLED "THE CORE
ANTI-DOME TEST" (core, measured — the single most important finding of the
stage).** The built outline's slope, summit outward, is
**20.1 / 18.2 / 20.9 / 20.9 / 31.8 / 23.9 / 30.0°** — **shallowest at the
summit, steepest at the foot**, the exact inverse of a concave profile and the
textbook dome signature. The silhouette bulges above a straight cone by up to
14.8 m at every radius. Meanwhile **I1 passes at 12.7–19.4°** on the same
mountain.

Both numbers are correct. I1 averages **surface** slope over the upper versus
lower third, and on a banded massif that average is set by **benches and
risers** — the sawtooth texture — not by the form the sawtooth sits on. So:

- **RULING: I1 is re-specified as an ENVELOPE measure.** It compares the
  *outline's* slope over the upper third against the lower third, not a mean
  of surface normals. Same threshold, same intent, correct basis. I10 is
  written the same way for the same reason.
- **The general rule, and it is core's sentence: AVERAGING OVER A SURFACE
  HIDES THE SHAPE OF ITS ENVELOPE.** A staircase of any overall form has the
  same mean tread-and-riser slope. Any invariant meant to constrain *form*
  must measure the envelope; surface means may only constrain *texture* —
  which is exactly what I3, I4 and I5 do, and exactly why those four are the
  ones robust across all twelve seeds.
- **And the sharpest version, because it is the one that will recur: A MODEL
  CHANGE CAN INVALIDATE AN INVARIANT'S MEASUREMENT BASIS WITHOUT CHANGING ITS
  NUMBER.** Before §2.8.2 the massif was smooth, so surface mean and envelope
  agreed and I1 was sound. Adding benches and risers decoupled them. **The
  feature that fixed I3, I4 and I5 silently broke I1's validity, and I1 kept
  reporting a healthy number throughout.** Nobody introduced a bug. When the
  model changes, every invariant's *basis* is re-opened, not just its value.

**THE SUMMIT TOR IS INVISIBLE, AND I CERTIFIED IT WITH AN INVARIANT I RULED
MYSELF.** Core disabled the tor entirely and re-measured the outline: identical
to the decimal, including the innermost band. At `SUMMIT_TOR_RADIUS` 5–10 m on
a ~190 m massif it hides inside the cone tip and cannot be resolved from any
acceptance distance. It passes I2 at 52.9° **only because I ruled I2
surface-area weighted**, and near-vertical slab sides dominate that average —
so we built an invariant that certifies a summit feature the camera cannot
see. Core calls this partly theirs for asking; it is mine for ruling it.

- **The weighting ruling still stands for I3 and I4** — the limit argument is
  untouched, and a plan-weighted I2 would still reject a tor outright.
- **RULING: the tor is SIZED AGAINST THE ACCEPTANCE DISTANCE, not against
  taste.** Its silhouette must clear `SILHOUETTE_MIN_PX` from §7.1b's frames —
  ≈ 13 m at 400 m, ≈ 24 m at 717 m — which makes `SUMMIT_TOR_RADIUS` a
  *derived* quantity, naturally landing near `MASSIF_SUMMIT_RADIUS_FRAC` of the
  base radius. **"The summit IS a tor" was always the ruling; a 5 m ornament
  on a 190 m mountain was never it.**
- **I2 means nothing until I11 runs.** Recorded as a dependency, not a
  criticism of I2: a summit-sharpness test with no camera in it can be
  satisfied by geometry no camera receives.

**THE SAME DEFECT EXISTS IN EVERY ZONE, AND STORY NAMED IT BETTER THAN I DID
— carried here in their words for the sync.** Their equivalent of nine
invariants that measure the object and never the view is **canon that is true
in the document and never checked from where the player stands**. Same defect,
two zones, and both of us found it the same way: *by someone finally looking
at the thing rather than at the numbers about the thing.*

The pair worth putting in front of the team, because each is the other's
proof:

1. **A test that measures the artefact instead of the experience.** Core
   disabled the summit tor entirely and the outline was identical to the
   decimal, while my own invariant scored that summit at 52.9° against a 40°
   floor. That is not a weak test — **it was measuring something the view
   cannot contain.**
2. **Canon that is true on the page and unverified from the ground.** Story's
   barrow-visibility condition is exactly that class, which is why they keep
   restating it as a refusal rather than a preference.

**Both pass right up until someone looks.** The counter-measure is not better
thresholds; it is that **every zone needs at least one criterion evaluated
from the player's position**, and that criterion outranks the rest of its
suite. I11 is terrain's. Story's is a raycast from Vaelmere.

**I11 WORKS, AND IT REPRODUCES THE USER'S COMPLAINT EXACTLY (core, measured on
the fully fixed build).** Interior breaks at 20°, floor 3, cone control reading
0 at every standpoint:

| Distance | Breaks by bearing | Verdict | Readable units (§1.6.1) |
|---|---|---|---|
| 253 m | 9, 9, 12, 10 | passes 3–4× over | 28 |
| 300 m | 5, 8, 12, 4 | passes from every bearing | 24 |
| **360 m — Ravenscar's DERIVED acceptance distance** | | | **20** |
| 400 m | 1, 4, 5, 4 | fails from one | 18 |
| **600 m** | **1, 1, 0, 0** | **fails from every bearing** | **12** |

> **THE 600 m ROW IS NOT A VERDICT ON RAVENSCAR — §1.6.1.** 600 m was written
> for the LR (base radius 260–310 m), which **does not exist in the generator**
> (§1.6.3). Applied to a 120 m crag it asks a six-feature test to fit in twelve
> readable units. The 300 → 400 → 600 decay everyone read as a shape failure is
> substantially an **angular-size curve**: a geometrically perfect mountain
> produces the same shape of curve. Ravenscar's acceptance distance is
> **360 m**, and the rows above it pass.
>
> **What that does NOT excuse, and it is the part to carry forward:** the decay
> is steeper than the budget alone predicts (12 → 2 breaks for a 2× range, once
> the fixed guard cost is taken out), and **the frame I ruled a dome from was
> shot at 400 m — inside the budget** (§2.8.8). The budget explains the
> *invariant*. It does not explain the *complaint*.

**~~The massif reads as broken rock up close and as a smooth mass from the
valley.~~ — THIS SENTENCE IS WITHDRAWN AS A FINDING ABOUT THE MOUNTAIN
(§1.6.1, §2.8.8).** It was produced by measuring a 120 m crag at a distance
written for a 280 m mountain that was never generated. It remains an accurate
description of *the user's complaint*, which is why it was so persuasive, and
that complaint is still open. Retained below as written, because the reasoning
it carried about spaces and cameras is sound and only its subject was wrong.

That was the sentence the user has been saying for four sessions,
produced by the only invariant computed from a camera — while nine object-space
invariants report a healthy mountain on the same build (I1 47.3°, I2 70.2°,
I3 67.9 %, I10 1.46, all robust across twelve seeds). **This is the §2.8.7
thesis measured rather than argued**, and it is worth stating that the frame
and the camera-side invariant agree with each other and disagree with
everything else. The model answer is in §2.8.2: the facets are not flat, which
is simultaneously why I7 has never passed and why breaks wash out with
distance.

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

#### 2.8.8 AFTER THE FIELD FIX — the frame attributed, two constants re-derived, I7 repaired

Everything in §2.8.7 above was reasoned on a build with a broken per-bearing
field and an inverted profile stamp. This section says which of it survives.

##### THE DOME FRAME IS ATTRIBUTED, AND IT INDICTED A BUG THAT IS NOW FIXED

The lead asked which landmark was in the frame I called a dome and at what
range. **It was recorded, and I did not have to reconstruct it:**

> `screenshots/massif/02_massif_verdict_400m_diagnostic.png`, shot
> 09:08:2026 21:14, **L0 Ravenscar at 400 m**, on frame 1's verdict bearing —
> render's parked vantage (120, 300) walked in along the same line, so the eye
> sits ≈ (434, 256), roughly due west of the peak and slightly south. Backlit,
> `DFN_TIME` 0.30. My verdict on it is the UPD entry at 21:21.

Three things follow, and the first two are mine to own:

1. **400 m is 18 readable units — inside Ravenscar's budget, not outside it**
   (§1.6.1 puts d_accept at 360 m; 400 m is one bearing's worth beyond it, and
   I11 duly failed from exactly one bearing at 400 m). **So the budget does not
   excuse that frame.** I looked at a mountain from very nearly its own
   acceptance distance and saw a dome. Recorded plainly because the comfortable
   reading — «we photographed it from too far away» — is available and is
   **false for this frame.**
2. **The frame predates the profile-clipping bug fix by roughly half an hour.**
   At 21:14 the stamp still computed `h = H·(1−t)^p`, decaying to zero at the
   rim rather than to the valley floor, so the entire concave tail sat below
   base terrain and was discarded by the `max()`. Core's own description of the
   consequence is «precisely why the envelope measured shallowest at the summit
   and steepest at the foot» — **which is the textbook dome signature.** The
   frame was right. It was a picture of a bug, and the bug was fixed at ≈ 21:41.
3. **Therefore MY dome verdict is CLOSED and the USER's complaint is NOT.** The
   frame is attributed to a defect that no longer exists; that closes the frame
   and nothing else. The user said «гора — это всё ещё сиська» while *playing*,
   from wherever he was standing, four sessions running, and no frame has been
   shot since **relief +19 m, base radius 180 → 120, the profile-clipping fix,
   faceted couloirs, arête count 4 → 3, and a noise field that had been
   returning a third of its range.** Nobody has looked at this mountain since
   any of that landed. **The complaint stays open until a frame closes it, and
   an explanation that dissolves the measurement while leaving the complaint
   standing is exactly the move this project keeps making.**

##### RULING — I7 MEASURES RIDGE PERSISTENCE, NOT RIDGE COUNT

I7 requires ≥ 3 detected arêtes and the massif now has exactly 3. My own rule
from §2.8.6 forbids that outright: *a generator input must never equal the
floor of the invariant that checks it.* Worse than a missing margin — **I7 was
measuring an input.** `L0_ARETE_COUNT` is a number design hands the generator;
counting it back out and comparing it to a floor checks the detector, not the
mountain. That is why it has never passed and could never have passed
informatively.

**RULING, and it is what «persistent arêtes» meant before the test turned it
into a count:**

- **I7's measured quantity is DESCENT DEPTH** — for each detected crest, the
  span of relief over which it survives continuously before the flank swallows
  it. Sampled over eight levels across the banded zone (cliffline → summit),
  not four.
- **`MASSIF_ARETE_DESCENT_MIN` = 0.50 of relief (предложение — утвердить),
  derived, not chosen.** A rib must structure the part of the outline the eye
  actually reads. The eye reads from the hem up; below `MASSIF_CLIFFLINE_FRAC`
  = 0.33 §2.8.7 explicitly permits a grassy apron and ribs are *supposed* to
  die into talus. So a rib must run from near the summit (≥ 0.85) down to the
  cliffline (0.33) — a span of 0.52, rounded to 0.50.
- **The count clause survives only as a guard against the coincidence, and it
  is a FRACTION: ≥ ⌈2/3 · `L0_ARETE_COUNT`⌉**, i.e. 2 of 3 today. Core measured
  that the detector reliably finds 2 of 3 corners, and this is the rare case
  where accepting a measured detector limit is not accommodation — **because
  the substance moved to persistence, the count is no longer what the test is
  about.** Had I lowered the floor and kept counting, that would have been the
  accommodation this document has refused twice.
- **Both controls, per Rule 30 and the lead's corollary.** *Must fail:* a
  smooth cone (zero detections at any level, descent depth undefined) — and,
  the case the old I7 could not reject, **a smooth cone with a faceted cap**,
  whose corners appear at 0.85 and are gone by 0.70, descent depth ≈ 0.15.
  That shape is exactly the dome-with-a-sharp-hat the frame kept showing, and
  the count-based I7 would have passed it. *Must be able to pass:* three ribs
  each running summit to cliffline — constructible, and it is what §2.8.2
  already builds when the facets stay flat.
- **Why this is the version that predicts I11.** A rib that dies above 0.55
  leaves the lower two-thirds of the silhouette smooth, and the lower
  two-thirds is most of what a standing eye sees. **Descent depth is the
  object-space quantity whose failure produces I11's failure**; ridge count is
  not. §2.8.2 already ruled that I7 and I11 are one failure — this is the
  version of I7 that makes that true rather than asserted.
- **The §2.8.3 guard from 21:29 is discharged.** I7's sampling levels were
  allowed to be re-scoped only «conditional on I11 existing», because a test
  whose slice elevations I may choose is a test I can always make pass. I11
  exists and is measured from a camera, so the condition is met.

##### THE CONVEXITY CAP PROTECTS NOTHING — THE BAR WAS ALWAYS ON ELONGATION

Asked what the cap protects, so that non-convexity can have a budget rather
than a veto. **The answer is that there is no veto to lift, and I should say so
before anyone spends a build on removing one.**

- **`n·tan(π/n)/π` is not a design rule. It is arithmetic** — the isoperimetric
  ratio of a regular n-gon — and it appeared in this document as a *consequence*
  of core's support-function construction, not as something design imposed. I
  have never ruled that a massif must be convex.
- **What I did bar is ELONGATION as a knob for passing I8**, and that bar has
  nothing to do with convexity: an elongated L0 is a **ridge, not a peak**. It
  stands, unchanged, and core's addition to it is the important part — no
  invariant we have would notice.
- **The model is already non-convex in plan** (couloirs are re-entrant notches
  cut into the hull). **What it cannot express is PROTRUSION**: nothing sticks
  out past the hull far enough to occlude the flank behind it. That is the gap,
  and it is a different word from concavity.

**So protrusion gets a BUDGET, denominated in things this document already
measures rather than in a new taste rule:**

1. **SINGLE SUMMIT — the binding one, and it is standard topography.** §2.8's
   first decoded requirement is «the summit is a point». A spur becomes a
   second mountain when its **prominence** — its height above the highest col
   connecting it to the main summit — gets large. Rule: **no spur's prominence
   may exceed `MASSIF_SPUR_PROMINENCE_MAX` = 0.20 of relief (предложение —
   утвердить)**; on Ravenscar that is 23 m, enough for a spur that occludes and
   far short of a subpeak. Above that the massif reads as a cluster of hills,
   which is precisely what C4's «one unmistakable mass» forbids and what a
   ribbed mountain is not.
2. **C1 VISIBILITY — already measured, no new constant.** Protrusion buys
   occlusion, and occlusion is what C1 counts. Spurs may grow until
   `LANDMARK_VISIBILITY_MIN` = 0.6 binds. Ravenscar currently measures 0.751
   with headroom to spend.
3. **ELONGATION — unchanged, and it is the one hard bar.** A spur programme
   must not become an axis. Corners clustering on a long axis is the failure
   mode; it is a design rule precisely because the suite is blind to it.
4. **The castle spur is a BENEFICIARY, not a casualty.** §2.8.7's release-valve
   ladder wanted the hem to flare so the Ward keeps its spur. Protrusion is
   that, done deliberately.

**No veto. «How much, and where.» And the answer to «where» is: on the flanks
the acceptance frames look at, which for Ravenscar is the south-west
three-quarters** — the crag sits 194 m from the east edge and 200 m from the
north edge of a 1024 m world, so it has no long sightlines from those bearings
at all (§1.6.2).

##### THE TWO TAINTED CONSTANTS, RE-DERIVED

**1. `MASSIF_PROFILE_EXPONENT_MIN` — 1.3 → 1.5, and the rule it belonged to was
wrong on the arithmetic.**

The mapping is `p = MIN + f·(MAX − MIN)` with `f` the per-bearing field, so the
broken field's `f ∈ [0.4, 1.0]` gave `p ∈ [1.66, 2.20]` lumped at 1.84 and
2.02. Nothing below 1.66 has ever been generated.

**First, the finding that matters more than the number: `p` CANNOT PRODUCE THE
ASYMMETRY §2.8.2 CLAIMS TO GET FREE FROM IT.** The profile is
`h = H·(1 − d/R)^p`, which runs from `H` at the centre to 0 at `R` **for every
value of p**. The mean envelope slope is `H/R` = 43.8° regardless. `p` does not
make a flank gentler or steeper; **it only moves where the steepness sits along
the radius** — high `p` gives a steep cap over a gentle apron, low `p` gives a
more uniform cone. §2.8.2's «the low-`p` sector is the gentle flank that
carries the ascent» is therefore false in both halves: the low-`p` sector is
the *most uniform* flank, not the gentlest, and it is the one closest to the
constant gradient I4 exists to reject. **The gentle flank has to come from
`R(θ)` — a longer run at that bearing — or from the benches, and §7.1b already
says the benches carry the ascent.** The broken field did not merely hide the
low-`p` half; it hid the fact that the low-`p` half was never going to do the
job it was assigned.

**So nothing pulls `p` downward, and the anti-dome argument pulls it up.**
Derived against I1's own floor (`MASSIF_PROFILE_STEEPENING_MIN` = 12°), with
the envelope basis I1 now uses, at H = 115 m and R = 120 m:

| `p` | upper-third envelope | lower-third envelope | I1 steepening | verdict |
|---|---|---|---|---|
| 1.0 (cone) | 43.8° | 43.8° | **0.0°** | control: a cone must fail, and does |
| 1.2 | 48.1° | 38.6° | 9.5° | **fails I1** |
| **1.3 (current)** | 50.0° | 36.6° | **13.4°** | passes by 1.4° — no margin |
| **1.5 (ruled)** | 53.4° | 33.6° | **19.8°** | 1.65× the floor |
| 1.8 | 57.7° | 30.5° | 27.2° | |
| 2.2 (MAX, unchanged) | 62.2° | 27.8° | 34.4° | |

**RULING: `MASSIF_PROFILE_EXPONENT_MIN` = 1.5.** The old 1.3 sat 1.4° above the
floor of the invariant that checks it — the never-equal-the-floor rule in its
marginal form, and it survived only because the field never generated it.
1.5 takes the same ≈ 1.6× margin §1.6.1 takes. `_MAX` = 2.2 is untainted (it
was always reachable) and unchanged.

**And the bounds are not enough — Rule 31 in full.** The fixed `bearing_field`
is a normalised sum of cosines, which is **bell-shaped about 0.5**, not
uniform: extremes require all harmonics to align. A range of [1.5, 2.2] drawn
from a peaked field concentrates `p` near 1.85 and delivers **little asymmetry
around the mountain**, which is a milder rerun of the same defect — «only the
top 60 %» replaced by «mostly the middle». Design does not get to specify the
noise function, so the requirement is stated on the **outcome**, where it is
checkable and field-agnostic:

> **`MASSIF_PROFILE_ASYMMETRY_MIN` = 10° (предложение — утвердить).** Across
> the 64 radial bearings, the spread between the steepest and shallowest
> per-bearing I1 steepening must reach 10°. A field returning only its middle
> half yields ≈ 7° and fails; the full [1.5, 2.2] range yields ≈ 14.6° and
> passes.

That is an asymmetry rule that is actually about asymmetry, it has a control (a
cone reads 0° of spread), and it cannot be satisfied by a peaked field
pretending to be a spread.

**2. The 20° silhouette break threshold — RE-CONFIRMED, on a different basis,
and it needs a name.**

The original bracket was: below 50° (`MASSIF_ARETE_TURN_MIN`, since a plan-view
aspect turn always projects to a *smaller* silhouette tangent break) and above
core's measured 10–15° noise floor. **The upper bracket is geometry and is
untouched. The lower bracket is withdrawn as a basis** — it was measured on the
broken field, and worse, «where counts stop responding to the threshold» is a
property of the terrain's sub-readable band structure, which is itself drawn
from the field that was broken. Re-deriving instead of re-measuring, so the
number stops depending on it:

- **Perceptual derivation.** At the readability window (1/30 rad ≈ 17.5 px of
  outline at 640×360), a break is a direction change between two ≈ 17.5 px
  segments. Orientation discrimination over segments that long is unambiguous
  well below 20°, so **20° is comfortably above the perceptual floor and below
  the 50° geometric ceiling** — the bracket holds without any measured curve
  in it.
- **20° also keeps the wider margin**, which was the original tie-break and
  still applies: it clears the floor of 3 by a factor rather than by a unit,
  where 30° gave 4-against-3.
- **CONTROL, standing, per Rule 30:** the analytic cone must read 0 at 20° —
  it does — **and the threshold sweep must show counts FALLING as the
  threshold rises.** Counts that do not fall are core's own jitter signature
  and mean the detector is measuring itself, not the mountain. That check is
  now part of the invariant, not a one-off diagnostic.
- **Rule 14 gap, and it is load-bearing: the 20° lives as a literal in the
  probe.** `MASSIF_SILHOUETTE_BREAKS_MIN` gives a count with no magnitude.
  Requested: **`MASSIF_SILHOUETTE_BREAK_TURN_MIN` = 0.35 rad (20°)**.

**3. `MASSIF_SLOPE_BIN_MAX` = 0.30 — STANDS, with its provenance replaced
rather than its value moved.** It was chosen just under the 33.2 % the old dome
scored, and that terrain no longer exists, so the provenance is dead even
though the number is untainted by the field. Re-derived from the landform
instead: a banded scarp puts its risers across three steep bins (≈ 22 % each,
surface-weighted) and its benches across two shallow ones (≈ 17 % each), so the
fullest bin of the intended mountain lands near 22 % — **0.30 rejects the
single uniform flank and is clear of the landform we are building.** Derived
from what the massif is, not from what the dome was. **I4 is not the thing to
relax, and its eight-seed failure is not to be diagnosed until it is
re-measured on the fixed field** — the band-of-angles spread that was supposed
to satisfy I4 was drawing from the broken field and has never actually been
exercised. That is a prediction, and it stays a prediction until core measures.

##### THE SUITE IS NOT A SCOREBOARD — the object-space invariants are NOT retired

Asked to consider seriously whether the object-space invariants measure the
wrong thing entirely, and to retire them if so. **They do not, and the honest
answer is more uncomfortable than retirement would be: they are nine correct
measurements of nine different things, seven of which the user never complained
about.**

- The complaint has two halves: «сиська» — a convex profile — and «рёбра» —
  angular structure. **I1 and I10 measure the first. I7 and I11 measure the
  second.** I3/I4/I5/I6 measure surface texture, which nobody disputed. I2 and
  I8 measure the summit and the plan outline.
- **A perfectly concave, perfectly smooth mountain passes I1, I3, I4, I5, I10
  and reads as a smooth mass.** Concave and smooth are not exclusive. There is
  no contradiction between the suite and the frame to resolve, because there
  never was one.
- **The two invariants that encode the actual complaint are exactly the two
  that fail on every seed.** I7 fails everywhere; I11 fails at the distance it
  was (wrongly) asked about. The suite has been agreeing with the frame all
  evening and being read wrong.
- **So the defect was in the REPORTING FORM, not in the tests.** «Seven of
  eight», «nine of eleven», «six of ten robust» — counting weights every
  invariant equally, when they were authored to protect different things and
  only some of them protect the thing under complaint. **RULING: a suite is
  reported as a list with its LOAD-BEARING member named for the complaint in
  hand, never as a score.** For «сиська» the load-bearing member is I1
  (envelope). For «рёбра» it is I7 (descent depth) and I11 (breaks). Everything
  else is context. This is the same act as §2.8.7's «every zone needs one
  criterion evaluated from the player's position», applied to how the result is
  written down rather than to how it is measured.
- **STANDING DEBT, and it is mine: nine of the ten invariants have no control.**
  Only I11 has ever been run against a shape it must reject. Rule 30 is
  retroactive or it is decoration. Requested of core as one cheap batch — run
  the whole suite against **a smooth analytic cone** and against **a
  `smoothstep` dome**, and publish the table. My predictions, recorded before
  the measurement so they can be wrong: I1, I3, I4, I5, I6, I8 and the repaired
  I7 all reject the cone; **I2 and I10 PASS it** — I2 because a cone's flank
  slope satisfies a summit-slope test that has no summit in it, I10 because it
  is a scale test whose proper reject case is a pancake, not a cone. If those
  two predictions hold, I2 needs a second control and probably needs retiring
  on the §2.8.7 grounds already recorded; if they do not hold, I was wrong
  about which ones are weak and that is worth more than being right.

##### THE I7 / I11 TRADE — no ruling was needed, and the reason generalises

Recorded because the shape of it will recur. Ruling 3 (crest structure sized
against the acceptance distance) was reverted on evidence: I11 read 1,1,0,0
without it and 2,1,2,0 with it — **failing from every bearing either way** —
while it dropped I7 from a first-ever pass to 1. There was never a trade to
adjudicate, because **one arm of it never crossed the bar.** The general form:

> **A trade between two invariants is only a trade if BOTH readings cross their
> thresholds. Two failing numbers moving in opposite directions is not a
> trade-off, it is one regression and one coincidence** — and the way to find
> out is to run the arm nobody ran, which is what the lead asked for and what
> settled it in one measurement.

Second, and it survives the revert: **§2.8.2 ruled that I7 and I11 are ONE
failure. A change that moves its two symptoms in opposite directions has
therefore not touched the mechanism** (Rule 32), whatever it does to either
number. That is the test to apply to the next candidate fix, before it is
measured rather than after.

**Ruling 3's PRINCIPLE is not withdrawn** — detail required to read at distance
is sized against the distance, which is now Rule 33 and is upstream of §1.6.1's
whole derivation. What is withdrawn is the *implementation* it bought, and the
distance it was sized against: **it was sizing crest structure for 600 m, a
range that belongs to a mountain that does not exist.**

## 2.9 Pale spires — the white rock formation (ruling, stage-4)

The user rejected the birch and proposed reusing its geometry: «белое дерево
выглядит как пальма… **их лучше переиспользовать как ассеты для каких-нибудь
белых скал, красиво будет**». Flora measured the geometry and handed the siting
to me, which is the right split.

**ACCEPTED as a landform class — the world genuinely lacks a spire, and pale
rock is the one value role nothing else in the palette occupies. Rejected as
proposed on two counts: not on the talus apron, and not at those proportions.**

#### 2.9.1 The two rejections, because both are instances of rules this document already has

**1. NOT ON RAVENSCAR'S TALUS APRON.** Flora's argument for it is that §5.12's
apron needs populating and a pale spire against dark rock is the inverse of the
value merge that ate the mountain. The second half is true and is why the class
is accepted at all. The siting is still wrong:

- **Talus is loose, angular, actively-moving debris. Nothing tall and thin
  stands in it.** Spires are *in-situ* rock left by differential erosion —
  bedrock that resisted while its surroundings went. Talus is the surroundings
  after they went. Putting a spire in talus is putting the survivor in the pile
  of what it survived.
- **And the deeper objection is procedural.** I ruled for the apron in §5.12
  because it is right «independently of any invariant, because that is what
  erosion actually puts there». **If I now put hoodoos in the talus because
  they happen to be available and happen to solve a contrast measurement, I do
  the exact thing I spent this stage arguing against** — placing a feature
  because it answers a number. The apron gets scree, stone, scrub and stunted
  pine, which is what belongs there.

**2. NOT AT 16–22 m ON A 0.4–0.9 m BASE.** That is an aspect ratio of ≈ **25:1**,
and §1.5 already forbids it: *nothing structural thinner than ~0.5 m matters
beyond 100 m*, sub-pixel verticals shimmer at low resolution. A 0.8 m spire
subtends 0.008 rad at 100 m against a readable size of 3.3 m — **four times
under threshold, and it is the brightest value in the scene, which is the
worst possible thing to alias.** The user's own word for the birch was «острые
пики», spikes; a real hoodoo is nearer 5:1. **The geometry cannot be scaled
into readability, because the defect is the ratio and not the size.**

#### 2.9.2 THE RULE THAT SETTLES BOTH OF FLORA'S CONSTRAINTS AT ONCE

Flora asked for a rarity budget and a no-out-angling-the-L0 rule. **One siting
rule supplies both, and it falls straight out of §1.5 rather than being
invented.** Computed from the palette before anything is placed:

| Pale spire (0.88, 0.87, 0.82), luminance **0.869**, against… | Ratio | Read |
|---|---|---|
| bright sky (top of the sky-blues ramp, 0.790) | **1.10×** | **unusable** |
| mid rock (0.371) | 2.3× | strong |
| pine canopy `PINE_DARK` (0.197) | **4.4×** | maximal |

> **RULING: a pale spire is sited only where it reads against ROCK or CANOPY.
> It may never break the skyline.**

- **It is the exact inverse of the crag.** Ravenscar reads against sky at a 3–4×
  value ratio and merges into dark foreground; the spire reads against dark
  foreground at 4.4× and **vanishes against sky at 1.10×**. Two objects, two
  opposite backdrops, one rule each — and §1.5's «every landmark brief states
  its value contrast against its usual backdrop» finally earns its keep.
- **This makes flora's constraint 2 automatic:** something that may never break
  the skyline can never out-angle the L0 crown. No new test, no new constant.
- **And it bounds the rarity problem structurally rather than by budget:** a
  pale spire in front of dark canopy is a local guide exactly where the forest
  currently has none, and it is *impossible* to site it as a false weenie,
  because a weenie is a thing that breaks the horizon.

#### 2.9.3 The class, as a brief

| Field | Value |
|---|---|
| **Name** | **Pale spires** (a *group*; a single spire is never generated) |
| **Tier** | L1 minor — a findable formation, not a dominant landmark |
| **Spire height** | **8–14 m** (предложение — утвердить: `SPIRE_HEIGHT_MIN/MAX`) |
| **Group span** | **10–16 m** at the base (`SPIRE_GROUP_SPAN_MIN/MAX`) |
| **Count per group** | **≥ 3** (`SPIRE_COUNT_MIN`), stepped in height as flora's table already does |
| **Material** | near-white, faintly warm; quantises to the **neutrals** ramp, alone among landforms |
| **Backdrop** | rock or canopy, **never sky** (§2.9.2) |

- **THE GROUP IS THE READABLE UNIT, AND IT IS THE GROUP THAT IS SIZED AGAINST
  THE DISTANCE** (Rule 33, third instance after the summit tor and the crest
  structure). Individual spires may stay thin — a stand of trees reads as a
  mass while every trunk is sub-readable — but **the group's span is what must
  clear `SILHOUETTE_MIN_PX`**. A 10–16 m group is readable to ≈ 300–480 m,
  which puts it squarely in §1.3's L1 band of 150–400 m.
- **Keep everything else flora measured.** The concave taper (0.70), the
  pentagonal faceting, the 0.18 rad per-stem sweep, the ×1.6 flare, and
  especially the **1 m self-burial** — a rock that plants itself into a slope is
  precisely right and is a property a tree only barely needed. **The stepped
  lead-stem ratio (0.74–0.96) is what makes a group read as a group rather than
  a fence, and it is the best thing in the proposal.**
- **Siting, derived and never tabled** (§7.1a): candidate positions are drawn
  where the backdrop test passes — inside and at the edges of forest masses,
  at the foot of the lakeshore bluff, and along the river's cut banks. **Never
  on a ridgeline, never on the massif's apron, never inside an L0 sight wedge**
  (existing machinery).

#### 2.9.4 It is STONE, and three properties must not follow it across

Flora keeps the generator — it is their trunk code with a different table row,
and that is the cheapest place for it to live. But it is **registered as a rock
class, not a flora species**, and the split is mechanical rather than
taxonomic. Requirements, not claims about anyone's code:

1. **No wind.** A swaying rock is a bug that will ship.
2. **No seasonal palette.** §5.11's foliage contract must never reach it; stone
   does not turn in autumn.
3. **Not placed by the vegetation scatter pass** — its siting predicate is the
   backdrop test above, which no tree has.

If those three are cheaper to guarantee by handing the parameters to render's
rock family, do that instead; the requirement is the separation, not the owner.

#### 2.9.5 Two flags

- **STORY CONSULT, not a veto.** A distinctive pale formation in the valley is
  the kind of thing that acquires a name and then acquires canon. Per the
  §2.8.7 precedent, story hears about it **before** it lands, not after.
- **THE USER MAY HAVE MEANT «БЕЛЫЕ СКАЛЫ» LITERALLY — white *cliffs*, not
  spires.** «Скала» carries both. Flora's reading matches the geometry and I
  have ruled on that reading, but a **pale rock band in the splat palette** is a
  different, cheaper and complementary idea that this document does not have
  either, and it would answer the same sentence. Recorded so it is not lost;
  it is a §4 item, not a mesh.

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

> **⚠ THE 2.74 % READING IS SUSPECT AND MUST BE RE-MEASURED (core's pond-fill
> fix, stage-4).** Core found and fixed a **16.6× duplication** in the pond
> fill — the testbed's 17,336 lake planes are now **1,043**, and **94.5 % of
> water cells were carrying multiple coplanar planes at conflicting heights.**
> So the drawn water surface could disagree with `water_surface_at`, and
> **every §3 figure measured against drawn water before that fix is
> provenance-dead**, exactly like the slope histogram measured on the old dome:
> - the **2.74 % WaterBed coverage** above,
> - anything derived from `height_above_water` near the shore, including §4's
>   `SHORE_SAND_HEIGHT` = 0.6 m band,
> - the §2.7 finding that ±0.3–0.6 m of micro-relief «dropped bank dips under
>   the water surface» — which may have been reading a *duplicated* plane at
>   the wrong height rather than the real one.
>
> **The cap itself does not move** — it is derived from `SHORE_SAND_DIST` and
> river width, not from the measurement. What is withdrawn is the *evidence of
> violation*, and it must be re-taken before anyone tunes the classifier
> against it. **Report the re-measured coverage; do not assume it merely
> shrank.**

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
| 2a | **Pale rock stratum** | a **modulation of Rock**, not a fifth material — see §4.1 | `ROCK_STRATUM_*` |
| FUTURE | Dirt/path | road pass | — |
| FUTURE | Snow | region mountains above snowline | `SNOWLINE_HEIGHT` (region) |

### 4.1 THE PALE ROCK STRATUM — «белые скалы», and it is the material half of the banded massif

User-authorised (he wanted «белые скалы» in **both** senses — the spire groups
of §2.9 *and* a pale rock surface, which the world does not have at all).

**This is not decoration. It is the missing half of §2.8.** The user's original
massif brief was «высоту надо задавать **линиями уровня**», and §2.8.2 answered
it in *geometry* — bands, risers, benches. The frame that refuted the suite
complained of **«ONE material band, not a rhythm»**, and §4 has never had an
answer to that, because rock has been a single grey since it was written.
**A pale stratum makes the contour lines visible as MATERIAL, which is the
layer the complaint was actually about.**

- **RULE: pale rock is a STRATUM — a layer in the bedrock, exposed wherever
  terrain cuts through its elevation.** Selection is `slope ≥ SLOPE_ROCK_MIN`
  **and** sample height inside a stratum band. It needs no new input: `height`
  is already there.
- **STRATA ARE DEFINED IN ABSOLUTE WORLD HEIGHT, GLOBALLY — never as a fraction
  of each landform.** This is the whole ruling. The same layer must appear at
  the same elevation on the crag, on the lakeshore bluff, in the river's cut
  banks and on any future quarry face. **A band at a fixed height everywhere
  reads as geology; a band at a fraction of each landform's height reads as
  paint.** Third instance of the absolute-versus-relative lesson, after the
  couloir scale («a feature's size comes from the feature it cuts, never from
  the mountain it sits on») and the summit tor.
- **It is a MODULATION OF THE ROCK MATERIAL, not a fifth splat layer**, so §4's
  four-material budget is untouched — rock's albedo lerps between its grey
  stops and `ROCK_PALE` on the stratum mask. No new splat channel, no new
  memory.
- **And it survives quantisation by construction**, which is the payoff of
  §1.5's ramp correction: grey rock sits on the **rock-greys** ramp and pale
  rock on the **neutrals** ramp, so a stratum boundary is a **ramp change** —
  the strongest separation the palette can carry, and one that two things on
  the same ramp could never have. The bands cannot merge at any quantiser
  setting.

**Sizing, derived (Rule 33 — the strata must be readable from the acceptance
distance, not merely present):**

- At Ravenscar's d_accept of 360 m the readable size is **12 m**, so **a
  stratum thinner than ≈ 12 m cannot read** and is stripe noise.
- Ravenscar's banded zone is cliffline (38 m) to summit (115 m) = **77 m**.
  Two to three pale bands across it give rhythm without corduroy.
- Therefore **`ROCK_STRATUM_PERIOD` = 28–36 m** with **`ROCK_STRATUM_PALE_FRAC`
  = 0.35–0.45** (⇒ 10–16 m of pale per period) **(предложение — утвердить)**.
- **The period is SEEDED AND NON-UNIFORM, with the same coefficient-of-variation
  discipline as `MASSIF_BAND_SPACING_CV_MIN`** — «линии уровня, которые где-то
  ближе, где-то дальше» applies to the material bands for exactly the reason it
  applied to the geometric ones, and a fixed period would rebuild the wedding
  cake in paint.

**Value, stated as a constraint so render picks the triple:**

- `ROCK_PALE` sits **between** the grey rock stops (luminance ≈ 0.37) and the
  spire white (≈ 0.87), and **at least one palette step below the spire white**.
- **The spires must remain the brightest value in the world** (§2.9), or a
  cliff face of spire-white drowns the L1 formation the brightness was doing
  work for. **A material must never out-value the landmark whose legibility
  depends on being the brightest thing in its frame** — C4's hierarchy
  argument, applied to the palette instead of to height.
- Pale rock is **not snow** and must not read as it; the FUTURE snow material
  is a separate row and a separate ramp position.

**Where it appears follows from the rule rather than from a table** (§7.1a):
every rock face the strata pass through — the crag's risers, the lakeshore
bluff, the river's cut banks, dungeon portal cuts. That consistency is the
point: **a stratum you can trace from the mountain down into the river bank is
the cheapest possible statement that this world has bedrock.**

### 4.2 The display palette — the ramp budget (ruling, stage-4)

The 64-colour post is **8 ramps × 8 shades**, quantised by nearest colour over
all 64 entries. Render asked which family gives up a slot for the conifer ramp
§5.12 requires. **Neither answer they offered is the first thing to try.**

- **RULING: the budget is 64 ENTRIES, not eight families of eight.** Ramp depth
  should follow **the lighting range a family carries and the screen area it
  covers**, and those are wildly unequal. Grass, rock, neutrals and sky span
  deep shadow to full light across most of the screen and need their depth.
  Sand serves a shore mask; water serves a 90 × 140 m lake and a 4–7 m river.
  **Reclaiming two shades each from the small families funds a conifer ramp
  without deleting anything.**
- **This follows directly from §1.5's correction.** If a ramp change is the
  coarsest and most robust signal the palette can carry and a shade step the
  finest, then **trading shades for ramps is favourable by default** and the
  uniform 8 × 8 grid is the one thing in the palette nobody has justified.
- **IF uniform ramp depth is structural in the shader, the sacrifice is DRY
  OLIVE, and the reason is not that it is the least pretty.** §4's material
  list is Sand / Rock / Grass-blend / Grass, plus two FUTURE rows. **There is
  no dry or upland grass material in this world, so dry olive is a ramp
  reserved for a biome that does not exist** — capacity held for an unbuilt
  thing while a built thing goes without, which is the LR's mistake in the
  colour space. Render's own argument (its dark end sits 0.046 from grass dark,
  closer than any other cross-ramp pair) is correct and I verified it; **but
  «serves nothing that exists» is the stronger reason and it is the one to
  record.**
- **The biome objection is answered in advance:** when biomes arrive they will
  need several new families and the palette is re-derived wholesale. Holding
  one ramp today does not meaningfully prepay that.

##### ⚠ THREE OF MY CLAIMS DID NOT SURVIVE MEASUREMENT — render measured, I reproduced

Render built a CPU mirror of the actual shader quantiser and re-ran every claim
against both palettes. **I reproduced their numbers independently before
accepting them and got the same figures to two decimals.** Recorded in full,
because the premise the whole change was ordered on is one of them.

| Claim of mine | Reality |
|---|---|
| «`PINE_DARK` must quantise into grass greens» | **It lands on WATER TEALS** — under the weighted *and* the unweighted metric |
| «the three needle tones are merged» | They land on **three adjacent water entries, cleanly resolved** |
| «separation goes from 0 → 3.1 shade steps» | **2.18 → 2.24** lit; **0.74 → 0.66** shadowed |

**Each failed for a different reason, and only the second is subtle.**

1. **The grass-greens claim I never computed at all.** I took it from a search
   report and made it load-bearing. It is wrong under *any* metric, so I cannot
   even plead the weighting. **This is the exact debt §1.6.5 names, incurred in
   the same document that names it.**
2. **The 3.1 figure used the wrong metric**, and that one is instructive: I
   measured Euclidean distance in RGB, and **the quantiser weights R/G/B at
   0.30 / 0.59 / 0.11.**
3. **The «0» was the shadowed case relabelled as the general case.** Lit rock
   already cleared the floor of 2 before any change.

**So the conclusion I drew — «nothing in the palette can fix the backlit
frame» — is CONFIRMED, and both numbers I used to reach it were wrong. Getting
the right answer for the wrong reasons is not being right**, and the only
reason it did not cost a build is that the ruling it supported (the apron
first) was load-bearing on other grounds.

##### THE METRIC IS A CONSTRAINT ON THE DESIGN VOCABULARY (render's amendment, ADOPTED)

**A separator must move RED or GREEN. Hue that lives in BLUE is invisible to
the quantiser.** At weights 0.30 / 0.59 / 0.11, a 0.2 difference in blue is
**0.9 shade steps** — under §1.3b's floor — while the same 0.2 in green is
**2.1 steps**. Green is 5.4× more effective per unit than blue, red 2.7×.

- **This is why needles and water collided**: blue-green water and green
  needles sit at nearly the same point in the (r, g) plane. Their measured
  separation is almost entirely in blue, which the metric all but discards.
  **Two colours that look completely different can be identical to the
  quantiser.**
- **It amends §1.5 rather than contradicting it.** «A ramp change is the
  coarsest signal the palette can carry» is a claim about **the eye**, and it
  stands. But **the quantiser decides which entry a colour reaches, and it runs
  first, and it does not use the eye's metric.** So a separator must pass two
  tests: *will the eye see it* (favours hue) and *will the quantiser preserve
  it* (favours R/G). **A blue-only difference passes the first and fails the
  second.**
- **General rule, and it is the transferable part: THE PIPELINE'S OWN METRIC IS
  PART OF THE DESIGN VOCABULARY AND BELONGS IN THIS DOCUMENT**, not discovered
  per-feature by whoever happens to implement next. Render's amendment is
  checkable arithmetic and it would have caught both of our proposals before
  either was written.
- **Consequence for §4.1, checked: pale rock vs grey rock is a VALUE change
  across near-neutral families, i.e. it moves R and G together.** It holds up
  under the metric. Same for the §2.9 spires.

##### ALL FAMILIES CONVERGE AT THE DARK END — doctrine, not defect

**Against rock in shadow, pine sits at ≈ 0.7 steps and still merges**, on both
palettes, because every family runs toward black and the darks are crowded by
construction.

- **NOTHING IN THE PALETTE CAN FIX THE BACKLIT VERDICT FRAME.** Confirmed by
  measurement rather than argued. **The apron is the fix; the palette is the
  hardening.** Nobody should spend a night in colour space on this — render has
  pinned it as an assertion that the shadowed case is *below* 2, so the limit is
  recorded rather than quietly hoped away.
- **Value and hue separation both vanish as luminance → 0. In deep shadow the
  ONLY thing that separates two shapes is silhouette.** Which is why §1.5's
  skyline rule exists, and why a landmark's read must never depend on its
  foreground being a different colour — it must depend on there being no
  foreground.
- **And the lit case is itself marginal: 2.18 steps against a floor of 2.** By
  this document's own standard that is a pass with 9 % of headroom. **If pine /
  rock separation ever needs to improve, the lever is `PINE_DARK`'s own R/G
  position, not the palette** — flora is rebuilding conifers now, which is the
  cheap moment to move it.

##### RULING — THE CONIFER FAMILY STAYS, AND THE REASON IS ENTIRELY DIFFERENT

It was ordered to fix the pine/rock merge. **It does not, and that merge was
never as broken as I said.** Asked whether it is still worth six entries:
**yes**, on a ground that survives measurement.

> **AUTHORSHIP OF APPEARANCE. Any element covering a significant fraction of
> the screen has its palette family chosen DELIBERATELY. A family arrived at by
> nearest-colour accident is not a decision: it moves whenever anything near it
> moves, and it couples two unrelated materials so that changing one drags the
> other.**

- **The forest was sharing a family with WATER.** A water look-dev change would
  have restyled every conifer in the world and nobody would have known why.
  That is a structural coupling defect, and it is not hypothetical here: **the
  river's source sits ≈ 122 m from the crag centre and the trace runs out
  through the pine foothills**, so pine against water is a present frame case in
  this testbed, not a future one.
- **The forest's colour was decided by an accident of the metric rather than by
  anyone.** That alone justifies the entries.
- **What it does NOT buy, stated so it is not re-claimed later:** conifer and
  broadleaf already separated (oak → grass greens on both palettes), and the
  three needle tones already resolved cleanly. Those are not gains.
- **DEPTH ALLOCATION — one measured amendment, render's call to take or leave.**
  Their split is sand 8→5, water 8→5. **Water is the worst place to spend it:
  it is the largest smooth gradient in the world, where banding is most
  visible, while sand is a thin dithered shore strip and dry olive serves only
  bright-grass highlights on already-dithered ground.** Measured per-shade
  spacing on the water family — smaller is smoother:

  | allocation | water step | pine vs lit rock |
  |---|---|---|
  | sand 5 / water 5 (as landed) | 0.105 | 2.19 |
  | sand 4 / water 6 | 0.084 | 2.14 |
  | **dry olive 5 / sand 6 / water 7** | **0.070** | **2.22** |

  The third is better on both axes at the same 64 entries. **Offered, not
  mandated** — banding visibility is a readability question and therefore mine,
  but ramp construction is render's craft.
- **AND I MUST WITHDRAW THE REASON I GAVE FOR PICKING DRY OLIVE.** I wrote that
  it «serves nothing that exists». **Measured: bright grass and dry grass both
  land on it.** No material *targets* it, but the quantiser is applied to the
  final image and pixels reach it — so it is functioning as the lit-grass
  extension. **That is my third unverified claim in one section**, and it is
  why the table above proposes *reducing* dry olive rather than deleting it.

##### RULING — TAKE CONIFER 8. FRAGILITY DEFEATS THE PURPOSE OF THE CHANGE

Render built my proposed allocation, found that **water 7 steals the lit needle
tone back into the water family**, searched the space rather than guessing, and
landed olive 5 / sand 5 / water 8 / conifer 6 — better than my proposal on both
of my own axes. **My principle held and my arithmetic did not**, which is the
correct division of labour and the second time tonight it has run that way.

They then recorded the part that matters: **water 7 fails, water 8 passes, and
nothing about that is robust.** Which family wins a given tone is decided by
where entries happen to fall. Ruling:

> **Take `conifer` = 8, paying for it with dry olive 4 and sand 4.**

1. **The fragile version does not deliver what the change was bought for.** The
   justification is AUTHORSHIP OF APPEARANCE — the forest's family must be
   chosen deliberately rather than fall out of a nearest-colour accident. **An
   allocation that holds only because water happens to be 8 is still leaving
   the forest's family to accident.** It is a different accident, not the
   absence of one.
2. **The input is about to move.** Flora is rebuilding conifers now and the
   atlas tone is the knob the ramp is derived from. A configuration that holds
   only for today's exact tones **breaks silently when they ship — and breaks
   toward «the forest quietly becomes water-coloured again», which is the
   original defect.** A silent regression into the bug a change was made to
   prevent is the worst available failure mode.
3. **The cost lands where banding is least visible** — a thin dithered shore
   strip and a highlight extension on already-dithered grass — **and the gain
   lands on the largest dark mass in the world.** That is the same
   banding-visibility principle that produced my first amendment, applied
   consistently rather than only when it is cheap.

- **Come back only if it actually costs something visible** — if sand at 4
  bands on the shore, that is a readability regression and I would rather hear
  it than have it absorbed. Otherwise ship it.
- **Re-verify after flora's new needle tones land.** The tones are the input to
  the ramp; a derivation is only as current as what it was derived from.
- **I could NOT check their tone arithmetic and did not try to fake it.** My
  reconstruction of their ramp disagrees with their measurements in *both*
  directions, which tells me my reconstruction is unfaithful — not that theirs
  is wrong. What I could verify structurally holds in every allocation: pine
  lands on conifer, oak stays on grass. **Fragility tolerance and banding
  visibility are design calls; ramp arithmetic is render's, and the honest
  answer to «whose number is right» was that mine was not computable from
  here.**

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
> **`CROWN_ASPECT_MAX` IS 1.8 — and my paragraph "accepting 2.0 rather than
> re-litigating" is WITHDRAWN, because it was written against a stale read of
> NUMBERS.md.** The lead landed 2.0 at 21:12 and refined it to **1.8 at 21:13,
> one minute later, per my own verdict.** I read the file between those two
> edits, inferred a disagreement that had already been resolved, and then
> spent a ruling reconciling myself to a value nobody was holding. **Checking
> the registry would have cost one grep.** Nothing downstream broke — flora's
> trees measure ≤ 1.28 and clear either number — but the reasoning in that
> withdrawn paragraph was sound applied to a fact that was not true, which is
> the third time this session I have produced a well-argued conclusion on an
> unchecked premise. **The tightening trigger it proposed is moot: the ceiling
> is already at 1.8.** Three previous fixes all changed *what goes in the box*
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

> ### RULING — `BIRCH_CROWN_BASE_FRACTION` 0.58–0.62 → **0.40–0.45**, and this
> ### is not a change of ruling but the FIRST APPLICATION OF THE RULE ABOVE
>
> **The rule says «the SMALLEST value ≥ the floor which satisfies
> `CROWN_ASPECT_MAX`». 0.58 has never been that value.** It was derived when
> the aspect was measured on the authored **container** (2.30:1). Flora then
> corrected the basis to **generated geometry**, where the birch measures
> **1.02–1.27 against a ceiling of 1.8** — and the derived value was never
> recomputed against the corrected basis. My own NUMBERS note records the gap
> without my noticing what it implied: *«берёза 0.58 при выведенных 0.09»*, and
> the ceiling would only bind at a crown base near 1.2.
>
> **Fourth instance of the family: A MODEL CHANGE CAN INVALIDATE A CONSTANT'S
> DERIVATION WITHOUT CHANGING ITS NUMBER** — after I1's surface-mean → envelope
> re-spec, `MASSIF_SLOPE_BIN_MAX`'s dead provenance, and the profile exponent.
> The measurement basis moved and the constant fitted to the old basis stayed.
>
> **Flora's sentence is the one to keep: THE MARGIN IS WHERE THE PALM LIVES.**
> A value chosen «с огромным запасом» over its derivation is not safe, it is
> *unexamined* — the surplus does work nobody specified, and here the surplus
> confined the foliage to the top 42 % of the tree and built a palm.
>
> **I looked at both frames before ruling** (`screenshots/flora_grown/`). At
> 0.58 the birch is a pale pole with a tuft on top; at 0.40 it is a slender
> light-crowned tree with visible branch structure inside the crown. The
> difference is not subtle and the aspect ceiling cannot see it, because **a
> palm and a birch can have identical crown aspect** — what separates them is
> structural, which is why flora's limb-spread invariant is the right
> instrument and the aspect ceiling was never going to be.
>
> - **MAX drops to 0.45 too — a range is two assertions.** 0.40–0.45 is a
>   0.05-wide band whose lower end is the tested value and whose upper end is
>   `CROWN_BASE_FRACTION_MAX`, so **the birch exception very nearly dissolves**:
>   it is now simply the top of the general 0.35–0.45 band, which is what a
>   slender water-margin tree should be. **Both ends are measured before it
>   ships**, per the rule flora's own pine just demonstrated.
> - **Walkability is untouched:** 0.40 × 16 m (shortest birch) = 6.4 m of clear
>   trunk against `CANOPY_CLEARANCE_MIN` = 2.2 m — nearly 3× over.
> - **Correcting one figure in flora's case, because it will be quoted:** 0.40
>   gives ≈ 7.6 m of clear trunk on a 19 m birch, which is **less** than the
>   8.5 m the old 0.45 gave, not more. It does not change the ruling — 8.5 → 11
>   was a bonus I claimed, never a requirement — but the argument should not
>   travel with an arithmetic slip in it.
> - **And the principle flora offered if I refused is right, so it is recorded
>   even though it does not apply: a species nobody will defend by eye should
>   not have a catalog slot.** That is «an invariant nothing fails is not an
>   invariant» pointed at content instead of at tests.

> ### RULE 30, SHARPENED — THE CONTROL SHOULD BE THE REAL REJECTED ARTEFACT
>
> Flora's limb-spread invariant ships with a control (a synthetic palm scoring
> 0.06 against a 0.15 floor), which is Rule 30 done correctly. **And it still
> passed the tree the user rejected: the birch measured 0.17–0.19.**
>
> **A synthetic worst case is the EASY reject. The hard one — the one that
> matters — is the artefact that was actually turned down.** When a real
> rejected instance exists, it is the control, and the floor must sit above it.
> Recommendation to flora, not a ruling in their zone: **re-measure limb spread
> on the repaired birch; if it lands at or above ≈ 0.22 (the lowest accepted
> species), raise the floor to sit between the rejected version and the
> accepted ones**, so the invariant would have caught what the eye caught.
> A floor placed below every real failure is a description, not a test.
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

### 5.12 THE FOREST WAS EATING THE MOUNTAIN — the apron ruling (stage-4)

Render re-shot the west 300 m frame with scatter suppressed and nothing else
changed. **The geometry is right**: a pointed tor with its tower nub, a concave
left flank carrying visible band lips at about two-thirds height, a long
straight right ridge with a distinct shoulder break, and the castle reading on
its spur. **With the trees on, that mountain is a low featureless hump.** I
opened both frames myself.

**Three candidate levers were put to me. I rule for the second, and the reason
is that it is the only one that addresses the mechanism that actually produces
the dome.**

##### THE MECHANISM, and why only one lever touches it

There are two failures, not one:

1. **THE FOOT IS EATEN.** A mountain missing its bottom third loses the bench
   and the flare of the base, and what survives is the upper cap — **which is
   convex on any mountain whatsoever.** This is the dome. It is not a shape
   defect and no shape change can fix it.
2. **VALUE MERGING.** Canopy and backlit rock land on the same value, so the
   eye does not see trees in front of a mountain; it sees one dark mass whose
   outline is the union of both (§1.3b).

**Failure 1 is the one that produces the user's word.** Hue separation makes
you able to *tell* tree from rock; it does not give you back the bottom third
of the mountain. **Only clearing the foot does.** So:

##### RULING — LEVER 2. AND IT IS NOT A CLEARING, IT IS A LANDFORM

**A massif's apron is talus, scree and scrub. Closed forest does not grow on
it.** The forest standing on Ravenscar's hem was never right; it is a placement
that no rule in this document ever asked for and no rule ever forbade.

- **Measured, so this is a mechanism and not a preference:** the pine annulus
  begins at **140 m** from the crag centre, which is *inside* the 120–162 m hem
  where the massif surface is still climbing. **Pines do not start at the foot;
  they start ON it.** Meanwhile the only treeless rule that exists,
  `on_crag_treeless`, fires **only** at `d < 120 m` **and** `h ≥ 57.5 m` — an
  elevation gate high on the mountain. **The band that is being eaten has no
  rule at all.** The one thing protecting the sightline is the strip duty
  cycle, which is an *angular* gap, not a radial standoff.
- **THE RULE IS DERIVED, NEVER A TABLED RADIUS** — §7.1a's trap, and I have
  fallen into it three times already. **No tree is placed where its canopy top
  would obscure the massif's silhouette below `MASSIF_CLIFFLINE_FRAC` from any
  acceptance standpoint.** That is C1-B (§1.3b) restated as a placement
  predicate, it uses the sight-wedge machinery that already exists, and it
  produces a radius per seed instead of a number in a table.
- **This ADDS content rather than deleting it, which is why it is the ruling
  the landscape actually wants.** The apron is exactly where §5.10's forest
  floor classes belong: scree and boulder fields, big bushes, snags, deadfall,
  and scattered stunted pines that are *below* the cliffline and therefore
  legal. A bare ring would be worse than the forest. **A talus apron with scrub
  and stone is a better landscape than closed pine to the hem, independently of
  any invariant** — real massifs look like that because that is what erosion
  puts there.
- **The ascent (§7.1b) benefits:** a worn watchmen's path across open scree
  reads as a path. Through closed pine it reads as nothing at all.

##### THE OTHER TWO LEVERS, ruled rather than surveyed

**LEVER 1 — density near sightlines: NOT the lever, because it is already
built and the frame still fails.** `TREE_SPACING_FOREST` is 12–18 m and the
scatter consumes it: oak on a 15 m lattice, pine on 14 m. Against the previous
5–8 m that is **5.3× sparser by area**, more than the «не менее чем в трое» the
user ordered. **The user's ruling landed and it was not enough near a
landmark**, which is worth saying plainly so nobody spends the fix twice.
Density is not the binding constraint; *proximity to the massif* is.

**LEVER 3 — hue separation: NECESSARY, NOT SUFFICIENT, and re-scoped.** The
source colours already differ strongly in hue — `PINE_DARK` is a teal-green at
saturation 0.45, every rock tone is neutral at 0.05. The lever is therefore not
«add hue separation», it is two different defects:

- **Chroma discrimination collapses at low luminance.** At the backlit hour
  both surfaces are lit by ambient alone, and at those levels the hue
  difference that exists on paper is not available to the eye. **Lever 3 is
  weakest exactly where the problem is.**
- **The 64-colour palette has no conifer ramp.** Design requirement handed to
  render: **the shipped palette carries a conifer family**, and §1.3b's
  separation test is run with the palette ON.

  > **⚠ MY STATED CAUSE WAS FALSE AND THE MEASUREMENT IS IN §4.2.** I wrote
  > that `PINE_DARK` «must quantise into *grass greens*, whose dark end is a
  > yellow-green». **It quantises into WATER TEALS**, and it does so under both
  > the weighted and the unweighted metric — so this was never a subtlety I
  > missed, it was **a claim I never computed at all.** I took it from a search
  > report and made it load-bearing. The conifer family is still right; the
  > reason it is right changed completely (§4.2).

**Ranked, so implementation order is not a judgement call: (1) the apron, which
restores the mountain; (2) the conifer family, for the reason in §4.2 — which is
NOT that it fixes the pine/rock merge, because measurement shows that merge was
never as broken as I claimed and the family does not move it; (3) nothing
further on density.**

##### THE APRON IS NOT BARE BY CONSTRUCTION — it is a HEIGHT rule, not a clearing

**Sequencing was raised on the reasonable fear that an apron shipped before
§5.10 exists would be the bare ring I warned against. Worked through, that fear
does not survive the arithmetic, and the reason is worth having: the derived
rule never removes vegetation, it caps its HEIGHT.**

C1-B (§1.3b) requires the silhouette exposed above `MASSIF_CLIFFLINE_FRAC`. A
tree is illegal only if **its canopy top subtends more than the cliffline
does** from an acceptance standpoint — and because a near tree sits much closer
to the eye than the mountain behind it, that is a real constraint rather than a
formality. Worked at Ravenscar's d_accept, as an illustration of the derivation
and **not as a tabled number**:

- cliffline elevation = 0.33 × 115 m ≈ **38 m** above the crag's base;
- from 360 m, the cliffline subtends 38/360 ≈ **0.106 rad**;
- a tree on the near hem (≈ 162 m from the crag centre) stands ≈ **198 m** from
  the eye;
- so its legal canopy top is 0.106 × 198 ≈ **21 m**.

**Pine is 28–38 m and is excluded. Everything at or under ≈ 21 m is admitted.**
Big bushes, scrub, stunted and young pines, and stone all pass. **So the apron
is populated in its very first version**, and the §5.10 classes enrich it
rather than being the precondition for it.

- **Consequently the apron does NOT have to wait for §5.10**, and blocking the
  one fix that addresses the user's complaint behind an entirely unbuilt
  feature set would be the wrong trade. Revised order: **tree heights fixed in
  the occlusion model → apron (interim, with the classes that already exist) →
  §5.10 floor classes enriching it → conifer ramp → quantiser decision.**
- **One requirement this places on core, stated because I checked and it is not
  free:** `Bush` and `Stone` are both currently barred from inside a forest
  mass, and the apron band lies inside the pine annulus. **The apron is a
  distinct ground-cover class, not an absence of forest**, so bush and stone
  placement must be admitted there. Whether that is a small change is core's to
  say, not mine.
- **The interim apron is honest, not a stopgap:** a hem of scrub, stone and
  young pine under a cleared skyline is what a talus apron looks like. Nothing
  about it has to be undone when snags and deadfall arrive.

##### §5.10 IS UNBUILT — the LR's position, a second time

Checked in source rather than assumed, and this is the answer to «check which,
because §5 has been in the same position the LR was»:

| §5.10 / §5.8 item | Constants | Mesh | Placed in world |
|---|---|---|---|
| Snags | `SNAG_DENSITY_*` (6 rows) | exists | **never** |
| Big bushes | `BIGBUSH_DENSITY_*` | exists | **never** |
| Fallen logs / deadfall | `LOG_DENSITY_*` (4 rows) | exists | **never** |
| Floor scarps / elevation change | `SCARP_*` (4 rows) | — | **no generator at all** |
| Maturity tiers 25/60/12/3 | `TREE_MATURITY_*_PCT` | — | **never — scale is a uniform 0.8–1.2** |
| Boulders on the forest floor | — | exists | **explicitly excluded from forest masses** |

**The scatter alphabet has five members — oak, pine, birch, bush, stone — and
bushes and stones are both barred from inside a forest mass. So the forest
floor today is bare terrain splat and nothing else.** Every constant above has
**zero consumers**, and unlike the `LR_*` rows **none of them carries a
«НЕ ПОСТРОЕНО» marker**, so the registry currently reads as though this is
built. Requested of the lead: mark them, exactly as the LR rows are marked. A
numbers table that overstates what exists is how a zone spends three sessions
tuning an absent object — §1.6.3, in a different zone, on the same evening.

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
> > **AND THE MOUNTAIN IS 16 % SHORTER THAN THE CONSTANT THE USER APPROVED
> (core, stage-4).** `L0_RELIEF` = 115 m is documented in NUMBERS.md as
> «перепад» — relief above the foot — and **the code uses it as an absolute
> peak elevation.** With the valley floor at 18.8 m the peak sits at 115.0
> absolute, so the true relief is **96.2 m**. Consequences, and the first is
> the one that matters upward: the castle-dominance 0.285 and C1 0.903 that
> justified raising `L0_RELIEF` to 115 were measured on *this* build, so they
> stand as measurements — **but the user approved 115 m of relief and the world
> has been showing them 96.** Every rejection of this mountain has been a
> rejection of a shorter mountain than the one that was signed off. It also
> inflated the aspect failure, since relief is the numerator: fixing the
> meaning moves aspect 0.507 → 0.606 for free and lifts the required base
> radius from ≈ 99 m to ≈ 137 m. **The bug was costing us 20 m of footprint.**
> Seventh instance this stage of a constant's *meaning* being read wrong rather
> than its value — the range family's close cousin, and the reason NUMBERS.md
> prose is a contract and not a comment.

**I2, I3, I4 and I5 are robust on every seed and I am calling those
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
| Range | ~~≈ 717 m~~ → **360 m, DERIVED (§1.6.1)** | ≈ 287 m |
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

**UNBUILT IS BINDING, NOT DECORATIVE — escalate before laying a single step
(story's condition on clearing the I10 reshape, and it survives them in
ACT1_VALLEY §2).** If steepening ever makes the ≤ 25° natural line hard to
find, **the answer is a longer traverse or more switchback, never masonry.**
The reason is not fussiness about materials: a built stair up Ravenscar
collapses §2.5's TWO DIFFERENT CLIMBS distinction, both mountains become
stairs, and **act 2's Seven Thousand Steps stop being singular.** The whole
point of specifying this route as a worn watchmen's line was that no frame
could confuse the two. A graded ramp is masonry by another name here; the
castle's approach ramp (§6.1.2) is a *pad* feature and is unaffected.

**And the banded model already supplies the ascent's structure, which is why
this condition is cheap rather than a constraint we have to fight.** On a
massif whose upper cone reaches the rock threshold, a ≤ 25° line **cannot go
straight up** — 115 m of climb needs ≈ 247 m of horizontal run, more than a
contracted radius provides on any radial. So the route must wrap. That is
exactly what §2.8.2 already builds: **the benches ARE the traverses and the
band breaches (§2.8.5) ARE the risers between them.** A path zig-zagging bench
to bench through breaches is a worn line by construction, it is what real
mountain paths do on banded rock, and it makes the ascent *more* legible from
the valley rather than less. The steeper mountain does not threaten the route;
it supplies it.

**One measurement to watch as the route wraps further:** I5 counts alternation
on non-route radials (§2.8.3), so a longer wrapping route shrinks that sample.
The pass *fraction* is unaffected — the denominator is the non-route set — but
if the route consumes most bearings there is too little left to measure
honestly. Report the non-route radial count alongside I5.
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
