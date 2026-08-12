<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §2.9–§2.9.5. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

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

