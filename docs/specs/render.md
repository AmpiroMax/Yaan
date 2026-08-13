<!--
Created: 09:08:2026 - 00:20:00
Last updated: 13:08:2026 - 16:30:00
<!--
UPD:
- 09:08:2026 - 00:20:00: Initial stage-1 spec: zone contracts, bgfx plan, boundary agreements with core/sim/lead.
- 09:08:2026 - 00:50:00: Stage 2 implementation: backend factories, embedded
  shader decision (--bin2c), exact dependency pins, TerrainMesher header,
  DFN_INTERNAL_RES override, test suite.
- 09:08:2026 - 10:32:00: "Flipped image" investigation: render orientation was
  correct end to end (Metal caps.originBottomLeft=false, identity upscale UVs,
  yflip=false passthrough verified against the pinned bgfx source). Real cause:
  Tour::default_steps assumed ground at y=0 while worldgen ground at the chunk
  center is ~24 m (seed 1) — vantages 00-02 were underground, showing the
  terrain underside. Fix: default_steps(ground_height = 0.0f), app passes the
  terrain height at the chunk center (App.cpp one-liner, lead). Defaulted
  parameter addition to the frozen Tour.h agreed with the lead (Rule 26).
- 09:08:2026 - 11:28:00: Stage 3 «Картинка»: procedural textures (ProcTexture),
  terrain splat v2, sky/fog/sun atmosphere, water plane capability, palette
  post flag (Q9б), Tour v2 six-vantage route + 4-way res/palette matrix.
  Contract sync 10:48 (RenderEnvironment/set_environment, palette_post).
  Boundary agreement with core for stage 3b (SurfaceFieldView, scatter).
- 09:08:2026 - 11:57:20: Stage 3b «Долина видима» (lead-approved batch):
  surface-truth splat from core's SurfaceFieldView (vertex weight channels +
  fs_terrain v3 ordered dither, slope band from SLOPE_GRASS_MAX/SLOPE_ROCK_MIN),
  per-body water (WaterMesher: lake planes + river ribbons from
  ChunkManager::water_bodies), scatter batching (ProcMesh §5 species +
  ScatterBatcher, GRASS_VIEW_DISTANCE micro tiles), §6 site placeholder meshes
  under blessed ids 1..7, "prop" program (backend), Tour v3 §7.1 route with
  lazy ground resolution + tour-driven streaming focus (app wiring by lead).
- 09:08:2026 - 14:11:37: Feature-requests batch (user decisions в1-в3 + design
  splat rulings): dynamic sun shadow map (backend view 0, dfn_shadow.sh, one
  hard tap; follows the app-animated sun incl. low angles, off below 0.05
  elevation), splat keyed off core's surface_class ONLY (dryness/dirt band and
  legacy height-sand REMOVED — they, not core's fields, painted the 60 m
  shore/brown washes), GrassRockBlend = ordered grass<->rock dither, chunky
  ~0.9 m stone boulder, afternoon southern look-dev sun. RenderEnvironment
  verified sufficient for the app day/night cycle — no contract change needed.
- 09:08:2026 - 18:10:00: MAP SCREEN (user request «добавь миникарты… как в
  скайриме по нажатию на клавишу»): the project's first UI. PixelCanvas (CPU
  raster primitives) + MapScreen (explored top-down map: elevation ramp over
  the explored span, hill shade with a cartographic z-factor, water from
  water_surface, site silhouettes from the blessed mesh ids 1..11, player
  arrow) + RenderSystem::draw_overlay (unlit frustum-filling quad — NO
  IRenderer change) + toggle_map/set_internal_resolution + DFN_MAP=1 and
  Tour::map_probe_steps for the one-frame evidence shoot. App wiring (Key::M
  -> toggle_map, set_internal_resolution) requested from the lead.
- 09:08:2026 - 18:50:00: THIN CASTERS CAST NOTHING (user bug: tree shadows
  showed the canopy but no trunk). Cause was shadow-map texel density, NOT a
  submit path: SHADOW_MAP_SIZE 2048 -> 4096 and SHADOW_HALF_EXTENT_M 640 ->
  320 (0.625 -> 0.156 m per texel). New acceptance vantage
  Tour::thin_shadow_probe_steps (DFN_SHADOW_PROBE) with a before/after pair.
- 09:08:2026 - 20:10:00: DAY/NIGHT STAGE, part 1 (в1/в2): SkyModel (sun+moon
  geometry from a normalized clock, phase-derived moon direction, dawn/dusk/
  night palette, stars), RenderEnvironment moon/star/point-light fields (lead
  authored the diff), env uniform block 11 -> 15 vec4s, fs_sky v3 (stars +
  phased moon disc), shared dfn_surface_light() consumed by terrain and props,
  sky visibility consumed from vertex alpha, DFN_TIME/DFN_MOON/DFN_SKY_YAW +
  Tour::sky_probe_steps. Cross-zone: LOD contract agreed with core, flora
  agent's zone split accepted, haze/far-plane finding sent to design.
- 09:08:2026 - 20:11:11: Foliage material path (alpha-cutout leaf cards, wind,
  leaf translucency) + the named PALETTE SIGNAL STRENGTH rule: in 8 ramps x 8
  shades a hue change is the strongest signal and a brightness step the
  weakest, and sub-step effects become dither, i.e. noise on small geometry.
- 09:08:2026 - 20:55:00: INTERIOR LIGHTING part 1: carried lights collected
  from components::CarriedLight, cube shadow maps for MAX_SHADOW_POINT_LIGHTS
  (one distance ATLAS, not a cube texture), caster culling from bounds
  measured at create_mesh, and the named EMPTY DRAWS EAT UNIFORMS rule.
- 09:08:2026 - 21:08:00: LOD render half (TerrainLod: derived ladder, quadtree
  selection, two-level fade window) + the DrawParams contract sync (lead
  authored; both backends implement it, fade drives an ordered dissolve).
- 09:08:2026 - 22:23:29: LOD DRAWING HALF + the crag acceptance route.
  (1) LodTerrain owns coarse-node meshes and draws them faded; the seam with
  core is "a coarse node IS a HeightFieldView" (129 samples, step = the level's
  voxel size), which core ACKed, so the splat, atlas and shader are the chunk
  path's and nothing new was invented. (2) The RESIDENT RECTANGLE is an input
  to selection, not a draw-time skip. (3) Skirts are MEASURED from the field's
  worst border step, not picked. (4) World-referenced terrain UVs. (5) Sun
  caster cull in the backend, A/B-verified bit-identical with a non-vacuous
  control. (6) THE 600 m FRAME WAS AIMED AT A MOUNTAIN THAT DOES NOT EXIST:
  LR is a NUMBERS row and a design section with no code path, and the testbed's
  only real landform is the crag, whose equivalent acceptance range is 253 m —
  inside streaming all along. See "How it is verified" for what the frames say.
- 10:08:2026 - 00:00:47: THE BITMAP FONT (the project's first glyphs, and the
  thing four finished features in three zones were blocked on) + the HUD layer
  that carries text over the world. Plus two things the font work ran into:
  (a) THE SEGFAULT ON EXIT the user reported — 17336 lake planes at one GPU
  mesh each spent bgfx's whole 4096-handle pool at startup, after which every
  terrain, scatter and site mesh silently failed to create and the stored
  invalid handles killed the process at shutdown; (b) THE CONIFER PALETTE
  FAMILY, whose ordering premise did not survive measurement — needles were
  quantising into WATER TEALS, not into grass greens, because the quantiser's
  metric weights blue 0.11 and cannot see a blue-green/green distinction.
- 10:08:2026 - 02:30:08: TECH DEBT WAVE (landscape stage opener): (1) Rule 21
  split of BgfxRenderer.cpp into four TUs over private BgfxRendererImpl.h,
  verified by identical mesh-handle counts on the same tour (c9b582e);
  (2) THE STRADDLE-RING FIX — straddling LOD nodes judged by distance to the
  ground they contribute and clipped by the mesher instead of force-split to
  level 0; measured 51 nodes/frame with 44 at L0 -> 12 nodes/frame with zero
  L0 on the app's real eye path (293bba5); (3) ScatterBatcher species_radius
  table deleted — micro-tile radius measured over baked vertices, old tabled
  values kept as the failing control (1794747); (4) the named rule CARDS BUY
  ANGULAR COVERAGE written in with the birch incident as its worked example.
- 10:08:2026 - 03:20:00: CLOUDS (WEATHER.md W4, user в4/в10 — all three kinds
  at once): the ONE coverage field in dfn_env.sh sampled by the sky sheets
  (fs_sky, two parallax layer planes) AND the ground shadow (dfn_cloud_sun_vis
  inside dfn_surface_light, so terrain/props/foliage/water darken together);
  cumulus impostors on the horizon ring, upwind-biased (W2.3 announcement);
  CloudModel drives the drift off the SHARED wind; RenderEnvironment gained
  the six-field cloud slice (lead-authored, 043e473); WIND_FIELD_DRIFT_SPEED /
  WIND_FIELD_WAVELENGTH entered NUMBERS with derivations. Found and fixed:
  apply_wind had NO live call site (wind_strength 0.0 read as calm — absence
  presenting as neutral); it now runs in RenderSystem::render each frame.
  Hooks: DFN_CLOUD (0 = the pass's control), DFN_VISTIME (drift pair),
  Tour::cloud_probe_steps (DFN_CLOUD_PROBE).
- 10:08:2026 - 10:55:09: THE SKY VIEW RAY WAS INVERTED, and had been since stage 3.
  vs_sky built v_dir as far-minus-near on the assumption that "any two points
  on the ray give its direction" — they give its LINE; the sign was never
  checked. Under this project's depth convention the z=1 unprojection lands
  NEARER, so v_dir pointed back down the ray: measured dir.y = -0.702 at the
  top of a frame pitched +0.12 UP. Everything downstream clamped silently —
  the gradient's `up` was 0 for the whole visible sky so u_skyZenith had never
  been drawn (the sky was a flat horizon colour), star_fade was 0 so the STAR
  FIELD had never appeared, the moon drew at the mirror of its direction, and
  the W4 sheets, gated on dir.y > 0, could only reach the narrow band where the
  inverted ray still read positive. That band IS the first cloud shoot's
  "materialises only near the horizon, mid-sky empty". Found by instrumenting
  the shader after a numeric harness had cleared the field math — the picture
  named three plausible field defects and the actual cause was in neither the
  field nor the projection of it.
- 10:08:2026 - 10:55:09: Clouds, second pass, all measured before touching a pixel
  (Rule 30b). (1) Rule 31: the coverage field's octave sum is GAUSSIAN and was
  thresholded as uniform — 98% of its mass sat in 0.200..0.797 of the [0,1] it
  declared, so cover 0.10 drew NOTHING, cover 0.20 drew 0.0005 and the default
  0.45 drew 0.19. Remapped through its own CDF; cover now means coverage within
  0.024 across the whole range and both ends are asserted. Constants that were
  fitted while it was broken: none shipped — DFN_CLOUD_EDGE (0.16, raw units)
  was replaced by DFN_CLOUD_EDGE_U (0.10, probability units). (2) The sheet's
  elevation and distance fades deleted 22.4% of sky pixels and cut a hard shelf
  at dir.y ~ 0.07; replaced by per-octave LOD on an ANISOTROPIC cells-per-pixel
  metric (the radial axis runs ~20x the tangential near the horizon) plus
  convergence to the area mean, so the sheet becomes a haze veil rather than a
  speckle band. (3) Rule 33: cloud layers 1200/2200 -> 2600/4400 m, because at
  1200 m the whole sky above 45 deg saw 10.8 cells of field AREA; the
  wavelength could not move, it is shared with the ground shadow. (4) Cumulus
  rebuilt on the same field with a threshold rising in elevation, on a 20 km
  ring at real cloud altitudes. engine/render/sources/CloudModel.cpp now
  carries the field's CPU reference so the distribution can be asserted, with
  the pre-remap form kept as the tests' control.
- 10:08:2026 - 23:24:48: Named rule «A CUTOUT IS NOT AN EDGE THE FRAMEBUFFER
  CAN SEE» — the running-shimmer fix, its measurement matrix (MSAA alone
  0.819/0.080 and 8x = 4x; +mipped mask +alpha-to-coverage 0.621/0.004
  against 0.864/0.094), the palette-on arm (0.712/0.017), and the evidence
  that the canopy did not thin (mean luma +1.8 % / +0.2 %).
- 11:08:2026 - 13:44:30: R1 — AERIAL PERSPECTIVE. MEASURED FIRST: fog began at 2400 m
  (0.30 of CAMERA_FAR) in a world 1024 m across, so the fog factor had been
  EXACTLY zero everywhere, always. The same crag at 250/500/900 m held its luma
  (97.4/91.8/91.9) while its standout from the sky ROSE 19.71 -> 30.40, i.e. the
  frame asserted the opposite of the reference. FIXED: dfn_fog_factor deleted,
  dfn_aerial() = Beer-Lambert extinction through height-falling density fading
  into the shared dfn_sky_gradient() at the view direction; env block 36 -> 37
  (slot 36 = HAZE_SCALE_LENGTH 1400 / HAZE_HEIGHT_SCALE 250, generated-header
  route); DFN_HAZE override. AFTER: 18.45 -> 20.67 -> 20.45, the rise is gone.
  HONEST: flat is not yet the reference, which FALLS; the 600 m arm reaches it
  (-21 %) but contradicts design §1.3a, so that choice goes to the lead as a
  pair of frames. New instrument tools/measure_aerial.py (its first version
  broke on the strong arm and the breakage is recorded in it).
- 11:08:2026 - 14:07:31: §10.9 MEASURED. Three arms + a NO-HAZE CONTROL. The two findings
  that matter are both about the floors, not about the length: H2 at the hem is
  failed by the CONTROL (0.61 of 1.00), so it is a terrain deficit and cannot
  arbitrate haze; and H1's floor leaves 0.36 steps over a haze-free render
  (control 2.36 of 2.00), so it forbids nearly all aerial perspective at 360 m.
  The height lever is confirmed independent — C vs B moves H1 +31 %, H2 -32 %,
  and the lowland NOT AT ALL (+19.78 both), because the valley floor sits inside
  the clamped layer. C doubles the lowland cue for 0.09 steps of silhouette.
  Rows NOT flipped: design's condition was that all three hold, and none of them
  holds at any length. Also: design's §7.1b frame-2 vantage is occluded by
  forest and H2 was shot from a clear bearing at the same range; and the
  instrument reproduced its own defect twice more, now stated as a rule in it.
- 11:08:2026 - 14:23:04: SHIPPED C (HAZE_SCALE_LENGTH 600, HAZE_HEIGHT_SCALE 40) on the lead's
  decision; default build reproduces arm C exactly. H2 handed to design+core as a
  terrain defect (it fails at zero dose), H1's floor back to design for
  re-derivation on p05, H3 recorded as computed and not a gate. THEN R2 — the
  mist band: a trapezoid-in-altitude second term in the same exact density
  integral (MIST_BAND_HEIGHT 70 / THICKNESS 32 / DENSITY 4), env block 37 -> 38.
  Measured against a mist-OFF control: +0.00 at the hem, +32.43 partway up,
  +0.00 above — a BUMP, which no R1 setting can produce. The `profile`
  instrument's first version passed its own control by running off the peak into
  sky; fourth instance of this file's rule, fixed by differencing.
- 11:08:2026 - 14:39:40: R3.1 — THE HORIZON DOMES REMOVED. Cause was dimensionality: the band
  read the field as a function of AZIMUTH ALONE, so the silhouette was
  single-valued and could not hold a hole, and inverting a squared threshold
  gave vertical sides under a flat top — a mushroom cap. Now read in 3D on the
  ring (dfn_cloud_field3, its OWN measured mean/SD 0.5000/0.1185), threshold
  back to linear. Measured: columns with a
  hole 0 -> 11, and zero was the structural prediction. Owed: no CPU reference
  or test for the 3D field yet. Flagged: the elevated sky vantage now sits
  inside the mist band.
- 11:08:2026 - 14:47:30: R3.1 debt CLOSED — cloud_field3 has a CPU reference and four tests,
  one of them the control the lead asked for. The control FAILED as first
  written and corrected my own claim: reusing the 2D constants costs 1.4x
  worst coverage error, not a catastrophe — but loses a third of the cloud at
  cover 0.05, the sparse end where cumulus live. Spec corrected to match.

- 11:08:2026 - 14:52:34: R2 band height DERIVED, and the two constraints DO NOT CONVERGE:
  above the world's highest vantage (99.6 m) and below the crown (135 m) leaves
  a 1 m crown. Constraint 1 is the wrong one — the reference observer is in
  clear air because he is in a VALLEY. Player vantages top out at 25.44 m and
  the shipped 70 m already clears them by 28 m. The SKY PROBE was the fault: it
  stood 1.6 m inside the layer. Pinned to the band constants at 106 m.
- 11:08:2026 - 15:08:17: R3.2 / R3.3 DIAGNOSED AND MEASURED, NOTHING SHIPPED (session wrap-up; a
  half-finished env-block change was reverted on purpose). THE INSTRUMENT IS THE
  INHERITANCE: per-row SD of the CLOUD-ONLY DIFFERENCE (frame minus the
  DFN_CLOUD=0 arm), which needs no brightness or colour mask at all because
  everything that is not cloud is bit-identical in both arms and subtracts to
  zero -- the first sky instrument here that Rule 47 has nothing to bite, and it
  is zero-dose-safe by construction (the difference is identically 0, so the
  criterion cannot pass without the subject). MEASURED: the hard bright band at
  the horizon is the sheet's own AREA MEAN, drawn where the field is STILL
  RESOLVED -- SD collapses 38..51 -> 8.2..13.0 over rows 144..152 while the MEAN
  stays the highest in the frame (72..78), i.e. a bright strip with the texture
  taken out. Cause located: dfn_cloud_alpha's outer mix converges at cells_px
  0.60 while the base octave's own LOD still carries 21% of its amplitude --
  two redundant convergences, the outer one running far ahead of the inner. A
  row-mean instrument would have declared the band absent (the mean profile has
  no step at all). Fix designed and NOT written: renormalise the field by the
  spread that survives the LOD (sd_lod = SD * sqrt(sum w_i^2)/0.6402, mean_lod
  interpolated), then drive the outer convergence off the residual spread. Rule
  31 flagged on it: the uncorrelated-equal-variance premise must be MEASURED in
  CloudModel.cpp with the current fixed-SD form as the failing control, BEFORE
  the shader half. R3.2 likewise designed and unwritten (three decks, the low
  one dark and in front at a DERIVED 1500 m = 1.73x apparent cell size,
  directional self-shadowing from one sun-side field tap, the high deck made the
  brightest so holes show it) with its per-deck-arm measurement specified.
  Deferred list added at the end of this file.
- 12:08:2026 - 22:52:00: R3.3 ОТГРУЖЕНА — жёсткая светлая полоса у горизонта убрана, и вместе с ней
  жёсткий срез SHEET_HAZE_LO/HI, одной правкой (иначе срез стал бы следующей
  видимой кромкой ровно после починки — это было записано предшественником и
  подтвердилось). Диагноз предшественника воспроизвёлся на сегодняшнем дереве
  ЦЕЛИКОМ, до цифры. (1) Поле перенормировано на среднее и разброс, ПЕРЕЖИВШИЕ
  собственный LOD; (2) внешняя сходимость переведена с cells_px на остаточный
  разброс res (окно 0.18 -> 0.04, то есть cells_px 0.59..0.68 вместо
  0.20..0.60); (3) срез заменён экспоненциальным ослаблением по СОБСТВЕННОЙ
  дальности листа, длина 60 км выведена из геометрического горизонта слоя
  2600 м (182 км), а НЕ через dfn_aerial_transmittance — та с длиной 600 м даёт
  на 20 км оптическую толщу 4.4 и стирает кучевые. Правило 31 выполнено ДО
  шейдера: посылка о некоррелированных октавах равной дисперсии ИЗМЕРЕНА
  (предсказание против замера 0.9996..1.0003 на всём диапазоне), и контроль
  оказался ХУЖЕ, чем говорил диагноз — на 0.50 ячеек/пиксель прежняя форма при
  запрошенном покрытии 0.15 рисовала 0.0000, а при 0.60 — 1.0000. Кадр:
  построчное СКО разностного изображения в полосе 9.4 -> 30.5 при среднем
  74.5 -> 47.8 (полоса перестала быть и самым ярким местом кадра). ЧЕСТНО:
  вторая плоская полоска на рядах 180-183 структурно НЕ починена (СКО 7.6 ->
  6.2) — там поля уже нет вовсе; починена её ЗАМЕТНОСТЬ (среднее 62.7 -> 21.3).
  Прибор `structure` заведён в tools/measure_aerial.py (был только в блокноте).
- 12:08:2026 - 22:49:49: R6a (ТЁПЛЫЙ КЛЮЧ / ХОЛОДНАЯ ТЕНЬ) ИЗМЕРЕН НА ЕГО КАДРАХ И
  ОТКЛОНЁН. Разделение по цвету у нас ЕСТЬ (+14.02) и оно больше, чем у любого
  из четырёх референсов (-8.55..+0.52, все внутри ±6 — оценки вклада их
  тонмаппинга). Тень в референсе того же тона, что и солнце, просто темнее.
  Прибор `tools/measure_light_split.py` калиброван: на нашем кадре он выдаёт
  +14.02 против +14.01, предсказанных LOOKDEV_SUN_COLOR/LOOKDEV_AMBIENT_COLOR.
  Его первая версия (по децилям) дала -36.5 на тех же камнях — это была
  ТЕКСТУРА, шестой случай правила 47 в этой зоне. Кода не менял.
- 12:08:2026 - 23:01:25: R6b (ПЯТНИСТАЯ ТЕНЬ) ИЗМЕРЕНА И РАЗМЕЧЕНА, не починена.
  Прибор `tools/measure_dapple.py` — ЛОКАЛЬНЫЙ контраст в окне, потому что
  очевидная величина (размах яркости по боксу) даёт нашей подстилке 1.81x против
  2.28x у рефа 03, то есть 79 % «прохода» для кадра, где пятнистости нет вовсе:
  этот размах — гладкий градиент по дальности. Синтетический контроль: чистый
  градиент 1.144x, чередование 4.000x. По локальному контрасту реф 03 = 1.65x,
  наша подстилка = 1.31x при нуле 1.14x, то есть впятеро меньше НАД нулём. И
  главное: наша подстилка под лесом набирает столько же, сколько наша ОТКРЫТАЯ
  трава с одной тенью обелиска. Тени не пропали (крона большого дуба на земле
  видна, foliage лежит в CUTOUT_PROGRAMS) — пропало ЗЕРНО. Первый подозреваемый
  арифметический: SHADOW_TEXEL_M 0.156 м при собственном правиле «кастер >= 2
  текселя ~ 0.31 м» — мы ровно на полу, ближний каскад назван в самом файле.
  СЛЕДУЮЩЕЕ, ЧЕГО НЕТ: рука нулевой дозы DFN_SUN_SHADOW=0.
- 12:08:2026 - 23:01:25: R3.2 ОТГРУЖЕНА — «облака квадратные и плоские» закрыто со второй стороны: потолок
  стал ТРЕМЯ ЯРУСАМИ, самозатенёнными, с прорехами, сквозь которые видно более
  яркое небо. (1) Нижний ярус на 1500 м — редкий, тёмный, рисуется ПОСЛЕДНИМ;
  высота выведена: та же длина волны на 1500 м против 2600 м подпирает 1.73x
  угла, и «ближе» говорит именно ВИДИМЫЙ РАЗМЕР ЯЧЕЙКИ, а не тон. (2)
  Направленное самозатенение одним отсчётом поля в сторону солнца, в общей
  функции dfn_cloud_self_shade (её читают оба нижних яруса). (3) Лестница тонов
  в ЕДИНИЦАХ ПАЛИТРЫ: каждая ступень ровно один PALETTE_SHADE_STEP_REF, и
  несущая пара — освещённый тон нижнего яруса на ступень НИЖЕ затенённого тона
  среднего, чтобы никакая часть ближнего яруса не путалась ни с какой частью
  дальнего. (4) dfn_cloud_sun_vis получил третий ярус — иначе облако над землёй
  не давало бы тени. ДВА СОБСТВЕННЫХ ПРОМАХА, найденных ИЗМЕРЕНИЕМ, а не глазом,
  и оба записаны в шейдере: (а) первая нулевая доза самозатенения была НЕ нулевой
  — обнуление УСИЛЕНИЯ прижимает член к 0.5, то есть красит ярус ровным серым,
  и на фоне такого «контроля» эффект выглядел нулевым (СКО 26.15 -> 26.18);
  (б) член складывался через max() с плотностным, что клало на весь ярус пол в
  полтени и УНИЧТОЖАЛО направленный сигнал везде, где плотностный был больше —
  тело среднего яруса теряло разброс (СКО 23.87 -> 22.54 при задаче наоборот).
  Знаковый и СЛОЖЕННЫЙ — среднее сохраняется по построению.
- 12:08:2026 - 23:22:28: R6b: РУКА НУЛЕВОЙ ДОЗЫ ПОСТРОЕНА И СНЯТА, и она решает, какой
  это дефект. DFN_SUN_SHADOW — доза (dfn_shadow.sh делает mix(1.0, s, доза), при
  1 кадр побитово прежний); при 0 кастеры ВСЁ РАВНО пишут в карту, останавливается
  только чтение, поэтому руки отличаются тенью и больше ничем. Один бинарник, обе
  руки: вся система солнечной тени добавляет к локальному контрасту +0.034 /
  +0.075 / +0.106 на 8/16/24 px и +0.402 на 40 px. ФОРМА ОБРАТНАЯ референсу: наш
  вклад РАСТЁТ с масштабом (низкочастотный, размер целой кроны), пятнистость
  рефа 03 ПАДАЕТ (1.780 -> 1.591, живёт на мелком конце). Два спектра смотрят в
  разные стороны — значит дело не в «мало тени», а в ЗЕРНЕ, и остаётся
  подозреваемый 1 (SHADOW_TEXEL_M). Неожиданное: на 8 px руки 1.269 против
  1.235 — почти весь мелкий контраст нашей подстилки это МАТЕРИАЛ земли, а не
  тень. Кадры: docs/acceptance/render-R6b-dapple-SHADOW-{ON,OFF}-b7bc7fe+ss.png.
- 12:08:2026 - 23:22:28: БЕЛЁСАЯ ДАЛЬНЯЯ ЛИНИЯ ЛЕСА ДИАГНОСТИРОВАНА, И ЭТО НЕ ДЫМКА. Воспроизведено на
  сегодняшней сборке на пробе DFN_FLORA_PROBE=2, а не разобрано по архивным
  кадрам. ДЫМКА ПРОВАЛИВАЕТ СОБСТВЕННЫЙ КОНТРОЛЬ НУЛЕВОЙ ДОЗЫ (правило 48):
  при DFN_HAZE=1e8 линия леса выглядит ТАК ЖЕ. Причина — НАШ СОБСТВЕННЫЙ путь
  alpha-to-coverage, то есть починка беговой ряби: на документированной
  бит-точной руке DFN_MSAA=0 та же линия леса выходит СПЛОШНЫМ ТЁМНО-ЗЕЛЁНЫМ
  лесом. Почему приёмка той починки прошла честно и всё-таки это пропустила:
  она мерила СРЕДНЕЕ («полог не проредился, +1.8 % люмы»), и среднее
  действительно сохраняется (77.18 против 76.33), а дефект живёт в
  РАСПРЕДЕЛЕНИИ — доля пикселей в СРЕДНЕЙ полосе (не лист и не небо) 6.1 % ->
  13.1 %, и при РАВНОЙ ЯРКОСТИ появляется синесдвинутая популяция, которой у
  контрольной руки нет (полоса 96..128: 31 -> 172 пикс, b-r -1.32 -> +20.42).
  Это РАЗМЕН, а не дефект к откату: тот же рычаг купил падение мерцания при
  беге 0.094 % -> 0.004 %. Решение — ведущему.
- 12:08:2026 - 23:22:28: ДВЕ ЛУНЫ (W9) — ОРБИТАЛЬНАЯ ПОЛОВИНА ОТГРУЖЕНА, ВТОРУЮ ЛУНУ ПОКА НЕЧЕМ
  НАРИСОВАТЬ. Блок W9 лежал двое суток без единого потребителя; теперь его
  читает SkyModel (MoonElements/masser/secunda/moon_state_at) и проверяют шесть
  тестов, у каждого — контроль, названный самой строкой реестра, включая
  НАСТОЯЩУЮ ОТГРУЖЕННУЮ ИГРУ (отношение 6:5 у Скайрима повторяет конфигурацию
  пары за 140 суток, у нас внутри 400 не повторяется ни разу). ВЫВЕДЕН ЗНАК
  ЭПОХИ: при `+эпоха` Массер в первом же кадре под горизонтом — ровно тот дефект,
  ради которого строка заведена; при `−эпоха` сходятся ПЯТЬ независимо
  заявленных design чисел сразу. ЧЕСТНО: два из пяти на ОТГРУЖЕННОЙ наклонной
  дуге смещаются (часовые углы +20.25/+46.65 против +18/+48, разнос 25.54° против
  30°) — SKY_ARC_TILT 0.45 в плоском выводе design не учтён; строки менять не
  надо, но эти два числа — идеализация. ЗАБЛОКИРОВАНО КОНТРАКТОМ: в
  RenderEnvironment одна луна, нужен диф ведущего (форма расписана в спеке).
  В дереве НИЧЕГО НЕ НЕДОПИСАНО: apply_sky_time не тронут.
- 13:08:2026 - 16:30:00: R6b, ВТОРОЙ ЗАХОД: БЛИЖНИЙ КАСКАД ПОСТРОЕН, ИЗМЕРЕН И
  ОСТАВЛЕН ВЫКЛЮЧЕННЫМ — а ВОРОТА ДЕФЕКТА УЕХАЛИ ИЗ ЗОНЫ. Арифметика суспекта 1
  подтверждена и в более резкой форме, чем была: дело не в том, что мы «на полу»
  0.31 м, а в том, что КАРТА ТЕНИ НЕДОСЭМПЛИРУЕТ МАСКУ ЛИСТА В 2-3 РАЗА —
  собственный тексель маски 0.047-0.086 м (тайл 64 px на карточке 3.0-5.5 м)
  против SHADOW_TEXEL_M 0.156 м, поэтому всё, что маска рисует на своём
  разрешении, до земли не доходит вовсе; отбрасывает только то, что flora
  СПЕЦИАЛЬНО нарисовала выше нашего пола (надрезы 0.4-0.9 м, дыры ~1 м). Каскад
  4096 на 40 м = 0.0195 м, пол 0.039 м — маска стала пересэмплированной.
  И ОН ПОКУПАЕТ +0.010 ЗА +22 % КАДРА. Три руки из ОДНОГО бинарника, n=3:
  собственный вклад каскада +0.010 / +0.000 / +0.010 / +0.046 на 8/16/24/40 px
  при разбросе прогонов 0.003-0.016 (две из четырёх неотличимы от нуля), и его
  СОБСТВЕННЫЙ спектр тоже РАСТЁТ с масштабом — ровно та форма, которую он
  строился перевернуть. Цена: точка держала потолок 120 Гц в 3/7 прогонов с ним
  и в 6/7 без. По умолчанию ВЫКЛЮЧЕН (DFN_SHADOW_NEAR=1 включает; при 0 карта
  создаётся 4x4, вид не трогается, кадр — прежний отгруженный).
  ПОЧЕМУ, И ЭТО РЕЗУЛЬТАТ, А НЕ ФИЧА: НА НАШЕЙ ПОДСТИЛКЕ НЕТ СОЛНЦА, КОТОРОЕ
  МОЖНО БЫЛО БЫ ПРЕРВАТЬ. Беспороговое p90/p10 по ближней полосе: реф 03 —
  3.23x (36 -> 116, свет и тень перемешаны по всему диапазону), у нас с тенью
  1.15x (27.5 -> 31.7, ровный ТЁМНЫЙ лист), без тени 1.15x (49.6 -> 57.3, ровный
  СВЕТЛЫЙ). Наша тень кроны ДВОИЧНАЯ И ПОЛНАЯ: 97.3 % полосы выше люмы 45 без
  тени и 4.6 % с тенью. Никакое разрешение карты не выдумает средний тон, у
  которого нет источника; каскад поднял освещённую долю 4.6 % -> 5.2 % — это
  арифметика, работающая ровно как обещано, и это всё ещё не пятнистость.
  Остаток R6b — НЕ РЕНДЕРА: (а) сколько солнца пропускает полог (flora; и
  FloraCards мерил свои детали против «пола 0.31 м», который эта правка делает
  устаревшим), (б) собственный тон материала земли, плоский на 1.15x.
  ПРАВИЛО 47 ЧУТЬ НЕ ВЗЯЛО СЕДЬМОЙ СКАЛЬП: первое чтение было до/после между
  двумя бинарниками с разницей в час и выглядело как перелом спектра
  (8 px 1.259 -> 1.441) — а всё это была чужая работа по кроне, прилетевшая в ту
  же пересборку: рука DFN_SHADOW_NEAR=0 из ТОГО ЖЕ бинарника давала уже 1.417.
  Разрешённая высота глаза за день ездила 17.42 -> 17.54 -> 16.23 м. В ЭТОМ
  ДЕРЕВЕ ДО/ПОСЛЕ МЕЖДУ БИНАРНИКАМИ МЕРЯЕТ НЕДЕЛЮ, А НЕ ПРАВКУ. Кадры и обе
  таблицы: docs/acceptance/render-R6b-near-cascade.md.
-->

# Spec — render agent

Written per Q35 / `rules/documentation.md`: seven sections, aimed at a newcomer
who must continue the work from this document alone.

## Zone of responsibility

Per Rule 25:

- `engine/platform/window` — `IWindow` interface + `glfw/` and `null/` backends.
- `engine/platform/input` — `IInput` interface + `glfw/` and `null/` backends.
- `engine/platform/render` — **sources only**: `bgfx/` and `null/` backends.
  `interfaces/IRenderer.h` is the lead-authored FROZEN contract (Q55, Rule 26);
  this zone implements it and never edits it.
- `engine/render` — the render layer on top of `IRenderer`: first-person camera
  with fixed-step interpolation (Rule 12), RenderSystem facade (ECS view ->
  submissions), terrain meshing from chunk heightfields, screenshot tour
  harness (Rule 27), debug-draw helpers. Later: materials, post-process
  (palette), LOD.

## Public interface

Stage-1 headers (all frozen for the stage per Rule 26):

- `engine/platform/window/interfaces/IWindow.h` — `dfn::platform::IWindow`:
  `init(WindowInitParams)` / `shutdown`, `poll_events` (once per frame),
  `should_close` / `request_close`, `native_handle()` (feeds
  `RendererInitParams::native_window_handle`; NSWindow* on macOS, HWND on
  Windows, nullptr from null), `framebuffer_size()` (physical pixels,
  HiDPI-aware), `consume_resize()` (one-shot flag consumed by the app, which
  forwards to `IRenderer::resize`). Polling model, no callbacks — trivially
  implementable by any backend (Rule 4) and by null (Rule 3).
- `engine/platform/input/interfaces/IInput.h` — `dfn::platform::IInput`:
  `update()` once per frame after `poll_events`; `is_down` / `was_pressed` /
  `was_released` over engine-owned `Key` / `MouseButton` enums (stable values,
  append-only — the future rebinding layer serializes them, Q58);
  `mouse_position` (free-cursor mode), `mouse_delta` (pixels, +x right / +y
  down — the mouse-look source), `scroll_delta`, `set_cursor_captured` /
  `is_cursor_captured`. Device level only: action mapping / rebinding is a
  later engine-layer module ON TOP of this interface; gamepad arrives as
  additive `Gamepad*` methods via group sync. Nothing here breaks for either.
- `engine/render/sources/FirstPersonCamera.h` — `dfn::render::FirstPersonCamera`:
  `set_poses(prev, curr)` with shared `dfn::components::CameraPose` snapshots
  (one call per fixed step), `set_projection(fov_y, aspect, near, far)`,
  `interpolated_pose(alpha)`, `view(alpha)` / `proj()` for
  `IRenderer::begin_frame`, `forward`/`right` accessors. Conventions:
  radians/meters (Rule 14); right-handed, Y up, +X east, +Z south; yaw 0
  looks toward -Z, positive yaw turns right; positive pitch looks up; yaw
  blends over the shortest arc; pitch clamped.
- `engine/render/sources/RenderSystem.h` — `dfn::render::RenderSystem`:
  `init`/`shutdown` (program + resource lifetime), `render(world, renderer,
  camera, alpha)` — iterates `world.view<Transform, PreviousTransform,
  RenderMesh>()`, interpolates, culls against `LocalBounds`, submits;
  `upload_terrain(renderer, HeightFieldView)` / `drop_terrain(renderer,
  chunk_coord)` for the chunk pipeline. Platform interfaces are parameters,
  never stored (Rule 9); internal maps are resource bookkeeping (asset id ->
  handle), not game state (Rule 10).
- `engine/render/sources/Tour.h` — `dfn::render::Tour` + `TourStep{label,
  position, yaw, pitch, wait_frames}`: `enabled_by_env()` (`DFN_TOUR=1`,
  `DFN_TOUR_DIR` for output), `begin(steps, dir)`, `apply(camera)` at frame
  start, `post_frame(renderer)` after `end_frame` (waits, saves `NN_label.png`
  via `save_screenshot`, advances; returns true when done -> app calls
  `IWindow::request_close`), `default_steps()` = the stage-2 acceptance route.
  Modeled on Quicky's gloom Tour, reduced to full first-person poses; the game
  itself stays tour-free — only `engine/app` knows the tour exists.
- `engine/render/sources/DebugDraw.h` — free functions over
  `IRenderer::debug_line`: `debug_draw_axes` / `debug_draw_aabb` /
  `debug_draw_grid` / `debug_draw_arrow`. Colors 0xAABBGGRR as in `IRenderer`.

Stage-2 additions (my zone, agreed with the lead at stage kickoff):

- Backend factories (integration convention): `create_glfw_window()`,
  `create_null_window()`, `create_glfw_input(IWindow&)` (requires a
  GlfwWindow), `create_null_input()`, `create_bgfx_renderer()`,
  `create_null_renderer()` — each in
  `engine/platform/<module>/sources/<backend>/Create*.h`.
- `engine/render/sources/TerrainMesher.h` — `TerrainMeshData` +
  `build_terrain_mesh(HeightFieldView)`: pure, deterministic, GPU-free.
- `Tour::internal_res_from_env(fallback)` — parses `DFN_INTERNAL_RES=WxH`.

Stage-3 additions (contract sync 09:08:2026 10:48, applied by the lead):

- `IRenderer::set_environment(const RenderEnvironment&)` — per-frame
  atmosphere + shared material parameters (sun/ambient, fog span+color, sky
  gradient colors, terrain splat thresholds, water color/scroll, visual time).
  Values originate in `engine/render/sources/Materials.h` (look-dev constants,
  explicitly temporary until the design doc; NUMBERS.md migration flagged);
  backends map them to shader uniforms — retuning never recompiles shaders.
  Null backend: accepted and ignored.
- `RendererInitParams::palette_post` (Q9б) — optional post pass in the upscale
  shader: 4x4 Bayer dither in internal-pixel space + nearest-color
  quantization to a fixed 64-color palette (8 ramps x 8 shades,
  `BgfxPalette.{h,cpp}`). `DFN_PALETTE=1` -> AppConfig (lead wiring). OFF by
  default = bit-exact stage-2 passthrough.
- `engine/render/sources/ProcTexture.h` — procedural textures (Q4в): periodic
  integer-hash value-noise fBm (`tileable_fbm`, exact wrap), non-periodic
  `value_noise01`, per-kind recipes (grass/rock/sand/dirt/water, quantized
  5-8 shade ramps), `generate_terrain_atlas` 2x2 layout contract with
  fs_terrain (grass|rock / sand|dirt). Deterministic and byte-stable; cached
  by parameters in RenderSystem under registry-assigned dense asset ids.
- `RenderSystem::set_water/clear_water/water_enabled` + `environment()`
  accessor; `DFN_WATER=<height_m>` debug env in init. Water renders via the
  "water" logical program: backend gives it alpha blend + read-only depth
  (name->state convention, acknowledged by the lead; see the platform README
  table) and RenderSystem submits it after all opaques (scene view is
  sequential since stage 3).
- Backend-internal: "sky" program (fullscreen gradient + sun, drawn first, no
  depth), `u_envParams[11]` uniform layout shared via `shaders/dfn_env.sh`
  (change only together with `apply_environment`), point-sampled material
  textures.

**Boundary agreement for stage 3b (with core, in-session 09:08:2026):**
core adds an ADDITIVE `dfn::math::SurfaceFieldView` (same grid/lifetime as
HeightFieldView; float spans dist_to_water + water_surface with NO_WATER
sentinel, uint8 SurfaceClass mask) exposed via `ChunkManager::surfacefield`,
app ferries it like heightfields; render will rebake per-vertex splat weights
from it (real beaches via dist_to_water, class mask as design truth) and
generalize water to explicit body primitives (lake plane, river ribbon) when
core exposes them. Scatter (P5): per-chunk `ScatterInstance` arrays, drawing
is render's (per-instance submits of shared meshes first; instancing via
contract sync only if profiling demands). Optional future: river flow
direction for current-following water scroll.

## Internal design

**bgfx integration (stage 2, implemented).** bgfx lives exclusively in
`engine/platform/render/sources/bgfx/`, fetched via CMake FetchContent from
`bkaradzic/bgfx.cmake` (bundles bgfx + bimg + bx) pinned to release tag
**v1.153.9398-566**; GLFW pinned to tag **3.4** (Rule 24; both recorded in
this zone's CMakeLists headers). The BgfxRenderer runs bgfx single-threaded
(renderFrame before init), Metal on macOS, and uses three views: 0 = scene
into the internal low-res target (RGBA8 + D24S8, point-sampled), 1 =
backbuffer clear (letterbox black), 2 = integer-scaled upscale quad.
`save_screenshot` schedules a bgfx backbuffer capture into the NEXT end_frame;
the custom `bgfx::CallbackI` writes the PNG via `bimg::imageWritePng`; the
Tour renders flush frames after scheduling so files land before advancing.
Shader compilation per Q50: `shaderc` (from bgfx.cmake tools) runs as CMake
custom commands over `sources/bgfx/shaders/*.sc` with `--bin2c`, generating
embedded headers in the build tree (`shaders_gen/<name>_mtl.h`);
`load_program("terrain"/"unlit")` resolves logical names against the embedded
table ("debug" and "upscale" are backend-internal). DECISION (stage 2):
shaders are embedded rather than loaded from disk — zero runtime path issues
for the tour; `reload_shaders()` is therefore a documented debug no-op this
stage (accepted by the lead), and disk artifacts + real hot-reload arrive with
the stage-3 material work. The null renderer implements the whole contract
inert-but-valid: monotonically increasing handles, all calls succeed,
`save_screenshot` returns false — a runnable headless mode, not a stub
(Rule 3); it carries the headless tour smoke test.

**Low-res target + integer upscale (Q9).** The internal target is fixed at
`INTERNAL_RES` regardless of the window framebuffer; the backend upscales by
the largest integer factor that fits the framebuffer (letterboxed if needed) —
point-sampled, so pixels stay square and crisp. Consumers never see the
internal target: `IRenderer::resize` only re-derives the upscale factor. The
limited palette (Q9) is an optional post-process flag on the upscale pass —
palette-quantization in the blit shader, off by default, toggled by config;
planned for stage 3+, but the two-view structure that makes it a one-shader
change is built in stage 2.

**Camera interpolation (Rule 12).** Sim writes `CameraPose` +
`PreviousCameraPose` (shared components) at each fixed tick; the app computes
alpha = accumulator / SIM_DT and calls `camera.set_poses` + `view(alpha)`.
The camera stores snapshots only — no clock, no ECS access. Sub-tick mouse
latency (render-side view-only rotation offset) is a flagged stage-3 topic
with sim.

**Terrain meshing (stage 2).** A TerrainMesher (internal to `engine/render`,
new file at stage 2) consumes `dfn::math::HeightFieldView`: vertices at each
of resolution×resolution samples, `height_m = height_offset + raw *
height_scale`, normals by central differences, UVs = sample position / chunk
size, vertex color white; indices as two triangles per cell. Shared edge rows
between neighbor chunks (agreed with core) make meshes stitch without cracks.
Output goes straight to `IRenderer::create_mesh` (Vertex layout is the frozen
contract). LOD and skirts are stage-3 topics.

**File discipline.** Each backend file targets ~300 lines, hard cap 800
(Rule 21). BgfxRenderer IS split (10:08:2026): four TUs over the backend-
private `BgfxRendererImpl.h` — lifecycle + embedded shader table in
`BgfxRenderer.cpp`, frame path in `BgfxRendererFrame.cpp`, draws in
`BgfxRendererSubmit.cpp`, handle bookkeeping in `BgfxRendererResources.cpp`.
Nothing outside `sources/bgfx/` may include the Impl header. C++23 with C++20
fallback guards (Rule 19); no C++23-only feature is currently used in the
public headers.

## Dependencies

Uses: `engine/core` (ecs, math, shared components — from `engine/render` only;
platform interfaces include nothing but stdlib + glm per Rule 1/2),
`engine/platform/render/interfaces/IRenderer.h` (frozen). Third-party (stage
2, backends only): bgfx via bgfx.cmake, GLFW — both pinned FetchContent
(Rule 24). Used by: `engine/app` (loop wiring), `engine/editor`, tests, the
tour.

**Boundary agreements (Rule 26), all confirmed in-session 09:08:2026:**

1. **Chunk geometry handoff (with core).** Agreed type
   `dfn::math::HeightFieldView` in `engine/core/math/sources/HeightField.h`
   (core zone; placed in core because world and render are DAG siblings):
   `glm::ivec2 chunk_coord; glm::vec2 origin` (world x,z of sample 0,0);
   `uint32_t resolution` (= HEIGHTMAP_RESOLUTION 129); `float step`
   (= HEIGHTMAP_STEP 2.0 m); `std::span<const uint16_t> heights` (row-major, x
   fastest, `heights[z * resolution + x]`); `float height_scale` (meters per
   raw unit, precomputed offline as (max−min)/65535); `float height_offset`
   (meters). `height_m = height_offset + raw * height_scale`. Conventions:
   right-handed, Y up, +X east, +Z south. Edge rows are shared between
   neighbors (sample 128 of chunk x == sample 0 of chunk x+1). Triangulation
   is render's job. Lifetime: view valid from `ChunkLoaded{coord}` until after
   `ChunkUnloaded{coord}` (both on the core EventBus, batched per streaming
   tick; unload fires before the memory is freed so the mesh is destroyed
   first). Render cannot include world, so the app layer (lead) subscribes and
   routes events to `RenderSystem::upload_terrain` / `drop_terrain` — wiring
   flagged for the lead at the sync.
2. **Shared renderable components (proposed to lead, authored by lead,
   ACKed by core).** `engine/core/components/sources/Components.h`
   (`dfn::components`): `Transform` / `PreviousTransform` (position, quat
   rotation, scale — sim writes both each tick), `CameraPose` /
   `PreviousCameraPose` (eye position + yaw/pitch radians; PENDING sim's
   formal ACK at the sync — sim proposed the identical shape under the name
   EyePose, semantics already agreed with sim directly), `RenderMesh`
   (uint32 mesh_asset / texture_asset name-hash ids; hashes shared from
   core's fnv1a-64 truncated to 32 unless the lead widens the fields),
   `LocalBounds` (glm min/max; may migrate to core's `Aabb` at the sync).
   Components never hold platform handles (Rule 8); `engine/render` maps
   asset id -> `MeshHandle`/`TextureHandle` internally.
3. **ECS surface (with core).** Namespace `dfn::ecs`, include
   `"engine/core/ecs/sources/World.h"`, `World::view<T...>()` yields
   `(EntityId, T&...)`, iteration order unspecified. RenderSystem signature
   `render(ecs::World&, platform::IRenderer&, const FirstPersonCamera&, float)`
   confirmed compatible with core's batch-ops design.
4. **Camera/controller interpolation (with sim, via lead relay + direct
   confirmation).** Sim owns the fixed-tick controller (capsule via IPhysics)
   and writes the eye pose + previous snapshot only at fixed tick; render
   reads (prev, curr, alpha), interpolates (lerp position, shortest-arc yaw,
   clamped pitch) and never writes controller state; sim never sees alpha.
   Angle/axis conventions mirrored in sim's spec.
5. **Skinning (with sim, informational for stage 3).** `IAnim::evaluate`
   yields `std::span<glm::mat4>` — column-major, model-space joint × inverse
   bind pose, ordered by joint index, size = `joint_count`, evaluated at fixed
   tick; null anim gives identity palettes (bind pose in headless tours).
   `create_skinned_mesh` + `submit` overload enter `IRenderer` via contract
   sync at stage 3 (per the frozen header's note), not ad hoc.

**Constants referenced (NUMBERS.md).** `CAMERA_FOV_Y`, `CAMERA_NEAR`,
`CAMERA_FAR`, `INTERNAL_RES` (all provisional, lead-added on render's request —
to confirm at the sync; 640×360 vs 320×180 is a user decision), plus
`CHUNK_SIZE`, `HEIGHTMAP_RESOLUTION`, `HEIGHTMAP_STEP`, `SIM_TICK_RATE` /
`SIM_DT`. No render-zone numeric constant may bypass NUMBERS.md (Rule 14).

## Step-by-step plan

Stage 1 (done): `IWindow.h` / `IInput.h` contracts, `engine/render` public
headers, boundary agreements, module docs, this spec.

Stage 2 (done in this changeset — skeleton, Q37/Q51):
1. Null backends for window/input/render (headless trio, Rule 3) + factory
   headers per the lead's integration convention
   (`Create<Backend><Module>.h`, `create_*` free functions).
2. Zone CMakeLists: `dfn_platform_window` (GLFW 3.4 pin),
   `dfn_platform_input`, `dfn_platform_render` (bgfx.cmake v1.153.9398-566
   pin + shaderc step), `dfn_render`.
3. `glfw/` window + input backends (macOS tested; Win32 branches
   compile-clean, untested).
4. `bgfx/` renderer (see Internal design) + four shader pairs: terrain
   (lambert over vertex ground tint), unlit, debug (lines), upscale.
5. TerrainMesher over `HeightFieldView` (crack-free shared edges);
   FirstPersonCamera; RenderSystem; Tour with `default_steps()` and
   `internal_res_from_env` (DFN_INTERNAL_RES override for the 640x360 vs
   320x180 user decision).
6. Tests: `tests/render.cmake` — mesher, camera, tour (headless), null
   backends.
7. Integration: the LEAD's engine/app drives window -> input -> renderer ->
   RenderSystem -> Tour and shoots the Q51 acceptance frames at both internal
   resolutions.

Stage 3 «Картинка» (done in this changeset):
1. ProcTexture module + tests (determinism, exact tiling, atlas layout,
   low color count).
2. Contract sync batch (lead applied 10:48): RenderEnvironment +
   set_environment, palette_post; both backends implement.
3. Shaders v2: terrain atlas splat (slope/height/dryness uniforms) + fog;
   new sky and water pairs; upscale palette+dither; shared dfn_env.sh.
4. RenderSystem v2: atlas/water textures (dense-id registry cache), frame
   environment (Materials.h), water plane API + DFN_WATER debug env.
5. Tour v2 six-vantage route; 4-way matrix (2 res x palette on/off) shot via
   tools/run_tour.sh; all frames read and checked (Rule 27) — see devlog
   img for the sync.
6. Look-dev calibration measured, not guessed: seed-1 slope statistics probed
   (p99 = 0.005) -> rock band 0.0025-0.0060 documented as flat-worldgen
   placeholder; fog span 0.25-0.70 of CAMERA_FAR hides the streaming edge.

Stage 3b (done in this changeset — «make the generated valley visible»):
1. TerrainMesher v3: vertex color re-purposed to splat weights (R sand /
   G rock / B water-bed / A dryness) from SurfaceFieldView.surface_class;
   upload_terrain 3-arg overload (2-arg = slope-only fallback).
2. fs_terrain v3: weight+slope splat, ordered 4x4 Bayer transitions in
   internal-pixel space (§4 "dither, not gradients"); slope thresholds derive
   from config SLOPE_GRASS_MAX / SLOPE_ROCK_MIN via 1 - cos (Materials.h).
3. WaterMesher (pure): lake ellipse fans + river ribbon strips (per-station
   width, descending surface); RenderSystem::set_water_bodies/clear_water_
   bodies over ChunkManager::water_bodies() (app ferry, lead). set_water +
   DFN_WATER demoted to debug fallback.
4. ProcMesh (pure): §5 species (oak ball-on-stump / pine cones / pale leaning
   birch / bush / stone) + §6 silhouette-coded structures, blessed RenderMesh
   ids 1..7 registered at init (site entities render via the ECS path, now on
   the lit+fogged "prop" program — backend pairs vs_terrain + fs_prop).
5. ScatterBatcher (pure): world-space baked per-chunk batches — trees always,
   bush/stone in 4x4 micro tiles culled by GRASS_VIEW_DISTANCE; RenderSystem
   upload_scatter/drop_scatter mirror the terrain pair (app ferry, lead).
6. Tour v3: testbed_steps() at the §7.1 layout coords (crag money shot, river
   ford, lake bluff, hamlet approach, forest species, overview); additive
   TourStep::ground_relative + begin(..., ground_at) lazy resolution +
   focus_position() so the app streams around the tour camera (far vantages
   are not resident at arm time — the single-height lesson, generalized).
7. Tests: 10 render suites (added proc mesh budgets/bounds/determinism,
   scatter batching, water meshing, splat weight channels, tour v3 shape).

Feature-requests batch (done in this changeset, after stage 3b):
1. Splat v4 (design ruling, binding): weights bake from core's
   `surface_class` ONLY (TerrainMesher). The render-side "dryness" dirt
   mottling and the shader's legacy height-based sand band are REMOVED — the
   04 probe showed the whole "brown wash" sightline classified Grass by core;
   the wash was render-invented. Do not re-derive material bands from raw
   dist/height fields — that bug class is now structurally impossible.
   GrassRockBlend renders as an ordered grass<->rock dither (class weight 0.5
   + per-pixel slope smoothstep between the same design thresholds).
   Vertex alpha is reserved (255).
2. Dynamic sun shadows (в1, backend-internal — NO contract change): view 0
   depth-only 2048 map (D16 compare LEQUAL, D32F fallback, off-if-neither),
   eye-centered texel-snapped ortho from environment.sun_direction (half
   extent 640 m = loaded chunk ring, depth half 700 m), every opaque submit
   double-submitted with the internal "shadow" program, one hard compare tap
   in dfn_shadow.sh (PCF off — user chose hard pixel edges) with normal
   offset 1 texel + 0.25 m depth bias. Below 0.05 sun elevation shadows
   switch off (night = ambient only; ndotl already 0). Matrices rebuilt in
   begin_frame AND mid-frame set_environment, so the app's day/night cycle
   (в2) moves shadows the same frame. u_lightMtx/u_shadowParams packing is a
   dfn_shadow.sh <-> update_shadow contract.
3. Day/night support (в2): verified RenderEnvironment already carries
   everything the app-driven cycle needs (sun dir/color, ambient, fog, sky);
   app animates via RenderSystem::environment(). Moon light / stars / shadow
   settings would need a contract sync — flagged, not built (open user
   questions on night visuals).
4. Micro-relief (в3): stone rebuilt as a ~0.9 m chunky asymmetric boulder
   (position-hash crush + re-derived flat normals; yaw varies silhouettes).
   Look-dev sun lowered/rotated south (Materials.h) so shadows are long
   enough to ground objects in the tour frames. Advanced material techniques
   (normal maps etc.) stay future TOGGLEABLE graphics settings — the
   name->state program convention and per-frame env uniforms leave room; no
   contract obstacle identified.

Map screen batch (done in this changeset — the first UI in the project):
1. `PixelCanvas` (`sources/PixelCanvas.{h,cpp}`) — clipped CPU raster surface
   in INTERNAL-resolution pixels: clear/put/fill_rect/frame_rect/hline/vline,
   `draw_stamp` (1-bit silhouette + optional 8-way dark halo) and
   `fill_triangle`. RGBA8 out, row 0 = top. No font system exists, so screens
   speak in silhouette and value only. Deliberately screen-agnostic — the
   start menu the user asked for later draws through the same primitives.
2. `MapScreen` (`sources/MapScreen.{h,cpp}`) — pure, GPU-free:
   - `note_chunk(HeightFieldView, SurfaceFieldView*)` bakes one MAP_TILE_PX^2
     tile per chunk: per map pixel the block-AVERAGED height, a quantized
     hill-shade factor, and a water flag that is an OR over the block (a 4-8 m
     river is one map pixel — averaging turns it into dashes). Called from
     `upload_terrain`, so the map records exactly what the player streamed in.
   - `note_site(mesh_asset_id, position)` — marker memory. Cheap enough to
     call for every site entity every frame: a quantized cell key is the fast
     path, and a new cell is still distance-merged against known markers
     (cell keys alone split the castle across a cell border). Mesh ids 1..7
     map to their own silhouettes, 8..11 collapse into one castle mark.
   - `compose(w, h, eye, yaw)` — backdrop, plate (unexplored stays dark),
     tiles, 1 px frame, north tick, markers (dwelling dots first, special
     sites over them), player arrow with a dark halo.
3. `RenderSystem`: `toggle_map/set_map_open/map_open`,
   `set_internal_resolution` (canvas size = internal target, 1 canvas pixel =
   1 screen pixel), and the generic `draw_overlay` — uploads the canvas as one
   RGBA8 texture (recreated per frame: the frozen IRenderer has no texture
   update) and submits an unlit quad placed at 1.5 * near, sized exactly to
   the frustum there. Culling is off in the backend, depth test LESS, so the
   quad covers everything submitted earlier. NO contract change (Rule 26).
4. Verification hooks: `DFN_MAP=1` opens the map at RenderSystem::init, and
   `Tour::testbed_steps()` returns the single-vantage `map_probe_steps()`
   under the same variable — one frame, not seven copies of one overlay.
5. Tests: `render_map_screen` (marker id mapping, castle merge, explored-chunk
   memory, composed size/water/player/backdrop, integer downscale at 320x180).

Decisions and measured findings of this batch (for whoever continues):
- The world keeps rendering behind the OPAQUE map. Freezing or skipping the
  world would need an app-loop change (lead's zone) and buys nothing at this
  frame cost; translucency was rejected outright — at 640x360, and under the
  64-colour palette post, a see-through map destroys both layers.
- Normalizing the elevation ramp over `0..WORLDGEN_MAX_HEIGHT` (the
  quantization range) put the whole valley into one green band: the crag
  outlier owns the top of the ramp. The ramp is stretched over the EXPLORED
  height span instead, updated as chunks arrive.
- True-scale hill shading is invisible here: over a 3.2 m map pixel the valley
  floor turns by a couple of degrees, so every normal shades the same. Shading
  applies the standard cartographic z-factor (4.0) and is normalized against
  FLAT ground (which lands on 1.0) — that is what made ridges, terraces and
  the river gorge read in the accepted frame.
- The overlay quad must NOT be overscanned: a 0.5% factor duplicated pixel
  columns under the point sampler and visibly softened the plate.
- CUT deliberately (scope): no zoom, no panning, no fast travel, no corner
  minimap, no labels/legend (there is no font system — markers carry meaning
  by silhouette alone), no map-specific input beyond the toggle, no
  compass/quest markers.

Thin-caster shadow fix (user bug, after the map batch):
1. **Diagnosis — it was NOT a missed submit path.** A tree's trunk and crown
   are the same vertices in the same `MeshData` (ProcMesh oak/pine/birch), and
   ScatterBatcher merges whole trees into ONE per-chunk batch submitted with a
   single `submit(..., "prop", ...)`. `BgfxRenderer::submit` double-submits
   every opaque draw into the shadow view, so trunk and crown are physically
   the same draw call in the same vertex buffer — the shadow pass has no way
   to take one and drop the other. The same holds for the §6.2 standing stones
   (ScatterSpecies::Stone instances inside the same micro-tile batches).
2. **Real cause — shadow-map texel density.** The map was 2048 texels over a
   640 m half extent = 0.625 m per texel. A caster narrower than a texel only
   darkens it when it happens to cover the texel center, so it drops out
   entirely. Measured against the actual meshes: oak trunk 1.1 m = 1.8 texels
   (dashed at best), pine 0.6 m = 0.96, birch 0.28-0.44 m = 0.45-0.7 — while
   the 8 m oak crown covered 12.8 texels and shadowed solidly. That is exactly
   "canopy only, no trunk". The receiver normal offset made it worse: defined
   as 1 texel, it pushed every receiving sample 0.625 m along its normal —
   wider than a birch trunk's entire shadow.
3. **Fix**: SHADOW_MAP_SIZE 2048 -> 4096, SHADOW_HALF_EXTENT_M 640 -> 320 m
   => 0.156 m per texel (and the normal offset, being in texels, drops with
   it). Thinnest trunk ~1.8 texels, 2 m standing stones ~13. Cost: the shadow
   volume no longer covers the whole loaded chunk ring; it ends at 320 m,
   where fog (start = 300 m) begins hiding the difference. Covering near
   detail AND the full ring at once needs a second cascade — a feature, not a
   constant. Depth texture is 4096^2 D16 (~33 MB, D32F fallback ~67 MB).
4. **THE RULE FOR EVERYTHING THIN ADDED LATER** (fences, railings, standing
   stones, castle detail, ladders): a caster only shadows if it is at least
   ~2 x SHADOW_TEXEL_M wide, i.e. ~0.31 m today. Anything thinner needs either
   a denser map or geometry that is honestly wider. Check the number before
   assuming a shadow bug is a pipeline bug.
5. Verification: `Tour::thin_shadow_probe_steps()` (DFN_SHADOW_PROBE=1) — one
   vantage at a dungeon entrance with trunks and standing stones on open
   ground, sun behind the camera.

Day/night stage, part 1 — sky, stars, moon (user decisions в1/в2):
1. `SkyModel.{h,cpp}` — the app-facing call is ONE function:
   `apply_sky_time(env, day_fraction, lunar_phase)`. The app owns the clock
   (48 real minutes per in-game day, plus a 50x debug key); this owns the look.
   It takes NORMALIZED fractions on purpose: the day-length question (в13) was
   still open when it was designed, and the answer could not invalidate it.
2. Everything is keyed off SUN ELEVATION, never off the clock — the sky must
   look identical at a given sun height whatever the day length, so the debug
   50x key changes the speed and nothing else.
3. The moon's DIRECTION is derived from its PHASE: elongation from the sun is
   the phase angle, so a full moon rises at sunset and a new moon rides with
   the sun. Phase and position are one thing, not two fields that can disagree.
   They are not exact antipodes: both ride the same south-tilted arc (our
   stand-in for the ecliptic), which the test documents rather than "fixes".
4. Look decisions that came from reading frames, not from theory:
   - The moon disc is ~4 deg across, ~15x life size. A true 0.5 deg moon is ONE
     pixel at 640x360 — a flickering dot. Phases have to be legible as
     silhouette or they are not a feature.
   - The star field has NO time term. Twinkle at this resolution, under the
     64-colour palette, reads as sensor noise rather than as sky.
   - Fog colour is pinned to the sky horizon colour at every hour. That is what
     keeps the streaming edge and the world edge invisible at dusk, when a
     fixed grey fog would draw a hard band across the landscape.
5. `dfn_surface_light()` in dfn_env.sh is now the ONLY place surface lighting
   is computed (sun with shadow, moon unshadowed, carried point light, ambient
   scaled by sky visibility). Terrain and props both call it, so they cannot
   drift apart the way a duplicated lighting expression always eventually does.
6. SKY VISIBILITY: ambient AND moonlight are multiplied by vertex ALPHA. On
   heightfield terrain that channel is the reserved 1.0, so this is a no-op
   above ground; core writes real sky visibility into the voxel meshes' alpha,
   at which point interiors go dark with no further render change. This is the
   geometric half of "true darkness"; the authored half is a per-place float.
7. Verification hooks: `DFN_TIME` (day fraction), `DFN_MOON` (phase),
   `DFN_SKY_YAW` (heading — the sun and moon ride an east/south/west arc, so
   the default northward valley shot can never contain them), and
   `Tour::sky_probe_steps` (DFN_SKY_PROBE). One route covers every hour.

Interior lighting, part 1 — the carried torch casts (user's "свет в пещерах"):
1. `RenderSystem::collect_point_lights` walks
   `world.view<CarriedLight, Transform>()` into
   `RenderEnvironment::point_lights` every frame: position interpolated at
   alpha (Rule 12 — a light stepping at the fixed rate strobes against 60+ fps
   geometry), the CARRIER-LOCAL offset rotated by the interpolated rotation,
   radius 0 -> TORCH_RADIUS_M, colour 0 -> TORCH_COLOR. WHICH lights cast is
   render's decision and gameplay never sets it: the first
   MAX_SHADOW_POINT_LIGHTS collected get `casts_shadow`.
2. THE OFFSET IS THE FEATURE. A light at the eye casts no visible shadow BY
   CONSTRUCTION — every surface you can see from the light's position is lit
   by definition. sim writes the hand: {0.35, PLAYER_EYE_HEIGHT - 0.25, 0},
   i.e. 1.45 m ABOVE THE FEET, because a character's Transform.position is the
   capsule bottom. An offset of -0.25 (the value that "reads right" if you
   assume the origin is the eye) puts the flame under the floor, lighting the
   underside of the terrain. sim pinned that in a test; do not "simplify" the
   Y back to a negative number.
   sim's answer on rotation (recorded so it is not re-asked): the player's
   Transform.rotation DOES carry the look yaw (angleAxis(-yaw, +Y), written
   every fixed tick), yaw only, never pitch — a hand does not pitch with the
   eyes. Do NOT special-case CameraPose; that would break an NPC lantern
   carrier for nothing.
3. CUBE SHADOWS, as an ATLAS rather than a cube texture. Six 90-degree faces
   per light live in tiles of one 2D R32F atlas (4x3 of 512 px, RGBA8
   fallback) that stores LINEAR DISTANCE / radius. Two consequences worth
   keeping: the receiver compares world metres (no depth linearization, no
   per-face near/far in the shader), and the face lookup is the MAJOR AXIS of
   the direction to the fragment — exactly the region a 90-degree face
   covers — so neither side ever depends on a cube-map sampling convention.
   The tile scale/offset is baked into the CPU matrix, so the shader does no
   tile arithmetic. Face order (+X,-X,+Y,-Y,+Z,-Z) and the row-major packing
   are the dfn_pointshadow.sh <-> update_point_shadows contract.
4. AFFORDABILITY IS THE CULL, not the resolution. Casters are rejected
   against the light SPHERE and then against each face's four side planes
   (inward normals normalize(fwd +/- right), normalize(fwd +/- up)), which
   takes a prop from 6 draws to 1-2 and a 256 m terrain chunk to 2-3. The
   sphere comes from per-mesh bounds MEASURED IN create_mesh: `submit` carries
   no bounds and IRenderer is frozen (Rule 26), so the one place that already
   has every vertex keeps a bounding sphere. No contract change was needed for
   any of this.
5. Texel density (the thin-caster rule, restated for this map): a 90-degree
   face at distance d has texels of d / (FACE_PX/2) = d/256, i.e. 1.6 cm at a
   tunnel wall 4 m away. Nothing we build is too thin for the TORCH map — the
   0.31 m floor is the SUN map's alone.
6. Alpha-CUTOUT casters are skipped in the point pass on purpose: a leaf card
   would punch its RECTANGLE into torchlight, which is worse than a canopy
   that casts nothing. flora agreed and asked for no change; the case that
   would reopen it is a torch under a canopy at night.
7. Verification hooks (Rule 27), none of them the shipping path: DFN_TORCH=1
   holds a flame at the camera's hand while the tour freezes the player,
   DFN_TORCH=2 stands it 6 m ahead (the BRAZIER PROBE — see the acceptance
   note on why a hand light cannot prove itself on open ground),
   DFN_NO_POINT_SHADOW=1 is the A/B half, DFN_DARK=<0..1> pins
   ambient_darkness. The torch hook stands down the instant a real
   CarriedLight exists, so it cannot quietly become the feature.

## Named rule — EMPTY DRAWS EAT UNIFORMS (bgfx submission vs view order)

Cost one debugging session; will cost the next agent a day if it is not
written down.

`bgfx::setUniform` does not set a value on the GPU. It appends to a per-frame
uniform buffer, and the range accumulated since the last submit is attached to
THE NEXT SUBMITTED DRAW. At render time bgfx replays those ranges in VIEW
order, which is not submission order. Two things follow:

1. **`bgfx::touch` is a draw.** It has no program, so bgfx skips it — and the
   uniforms attached to it are skipped with it. A touch issued after
   `apply_environment` silently DESTROYS that frame's environment.
2. Therefore: **every touch of a frame must be issued before the frame's first
   `setUniform`** (`Impl::touch_point_shadow_views` runs at the top of
   `begin_frame` for exactly this reason), and any new view that needs a clear
   must join that block rather than clearing where it is convenient.

The symptom this produced is worth recognizing: lighting a torch turned the
whole world DAYLIT — not "wrong shadows", not "no light", but the entire scene
rendering with the RenderEnvironment struct's default values, because the
night environment had been recorded into a touch that was never applied. If a
render bug ever looks like "the environment reverted to defaults", look for a
new empty draw before suspecting the environment code.

Terrain LOD, render half (`TerrainLod.{h,cpp}`, pure and GPU-free):
1. The LADDER is core's contract: voxel 1/4/8/16/32/64 m for levels 0..5, node
   = 128 VOXELS a side at every level, which is what makes the triangle budget
   per node constant (~33k, inside the agreed 30-40k). In metres a node is
   128 at level 0 and 8192 at level 5.
2. SELECTION IS DERIVED. A node is good enough when the eye is at least
   110 * (its voxel size) metres away, and 110 is the whole pixel budget in one
   number: 275 px per radian at 640x360, 2.5 px per triangle edge, so the
   useful edge at distance d is d/110. Level 0 must reach 440 m — not because
   440 looks right, but because the ladder's first jump is 1 -> 4 m and level 1
   is not competent until 440.
3. NODE IDS ROOT ON A FIXED WORLD GRID, not on the world rectangle. When the
   world grows from 2x2 to 10x10 km nothing is renumbered and nothing core
   cached is invalidated. Free today, unaffordable later; a test pins it.
4. MEASURED (inherit these instead of re-deriving): 2x2 km world, eye at the
   centre -> 76 nodes, 64 at level 0. 10x10 km -> 184 nodes, 128 at level 0.
   25x the AREA costs 2.4x the draw list. At ~33k triangles a node that is
   ~2.1M triangles of terrain BEFORE frustum culling, which is the next lever
   and is not built (a 75-degree frustum keeps roughly a third).
5. NO POPPING, structurally: a node cannot begin fading in before its mesh
   exists, and is released only when it is BOTH deselected AND fully faded. A
   pending request therefore cannot become a hole in the ground. This only
   works because core confirmed a node may be resident at two levels at once
   with an explicit render-called release_node.
6. The fade is a SCREEN-DOOR dissolve (dfn_screen_door), not alpha: the two
   levels are the same ground, blending would need sorting and would
   double-darken, and under the 64-colour post a half-transparent surface gets
   dithered anyway — so we dither deliberately, on the internal-pixel grid.
   It needed the per-draw channel that did not exist, which produced the
   DrawParams sync below. Explicitly NOT requested from core: morph targets /
   geomorphing — the dissolve needs no extra vertex data.

Contract sync — DrawParams (Rule 26, lead-authored 09:08:2026 21:02):
`submit` gained `const DrawParams&` (fade, highlight, aux0, aux1); the
four-argument form stays as a non-virtual convenience so no call site changed.
Argued for and granted on the ground that it is NOT a LOD field: three known
consumers already exist — the cross-fade, sim's interaction highlight
(components::HoverTarget names the hovered entity and render had no way to draw
it differently), and later damage flashes, magic glow, wetness. One named
struct beats three incompatible special cases invented months apart.
The lead rejected the alternative `set_draw_params(vec4)`-applies-to-the-next-
submit for a reason worth keeping: sticky per-draw state is a bug generator —
set a fade, hit an early-out that skips the submit, and an unrelated draw
inherits it. Passing params WITH the draw makes that unrepresentable.

Cross-zone agreements made in this stage (recorded so a successor inherits the
reasoning, not just the result):
- LOD with core: node delivery is additive (`coarse_mesh(NodeId)`, app ferry);
  level selection, cross-fade and skirts are render's. Core confirmed a node
  may be resident at TWO levels at once with an explicit `release_node` from
  render — that answer is what makes "no popping" achievable at all, because a
  streamer that frees the old level immediately guarantees a pop whatever the
  renderer does. Triangle budget derived, not picked: 275 px per radian at
  640x360, 2.5 px per triangle edge => max useful edge = distance / 110, which
  gives voxel sizes 1/4/8/16/32/64 m per level and 30-40k triangles per node at
  every level (~1.4M for terrain per frame, ~2.8M with the shadow pass).
- Flora agent owns ProcFlora/FloraSpecies in this directory; `pack/tri/quad`
  were promoted out of ProcMesh.cpp's anonymous namespace into the header so
  the two zones share primitives instead of duplicating them. Per-instance tree
  geometry is free at the draw-call level because scatter is already baked into
  per-chunk world-space buffers. Tree LOD distances derived the same way as
  above: billboard beyond 350 m (a 32 m tree is then under ~25 px, where a
  branching silhouette stops being distinguishable from a card).
- Design's landmark separation by HAZE rather than by angular size is adopted.
  BLOCKER raised to design, core and the lead: CAMERA_FAR is 1000 m and the new
  temple mountain is sited at 1.4-1.6 km, so it would be clipped, not hazy.
  Raising it past ~4 km also forces the near plane up (~0.25 m) or a reversed-Z
  depth setup, or distant geometry z-fights — decide the number once.

Stage 4 (next): skinned meshes (contract sync), frustum culling with core's
math types, LOD/skirts, grass cards (P6 micro, §5.6), flower patches,
sub-tick mouse-look offset, editor render hooks, shader hot-reload from disk,
instancing sync if scatter profiling demands, NUMBERS.md migration of
Materials.h + backend shadow look-dev constants (messaged to lead, Rule 14),
ProcMesh placeholder dims -> content data files (Rule 5, lead-coordinated).

## How it is verified

- **Font acceptance (Rule 27, ONE frame read 10:08:2026 - 00:00:47):**
  `DFN_TOUR=1 DFN_FONT_PROBE=1 DFN_TOUR_DIR=screenshots/font
  DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 <build>/engine/app/dfn_app` ->
  `screenshots/font/00_font_specimen.png`. The vantage is the hamlet approach,
  chosen for the READABILITY case and not for the chart: sky, lit grass, a dark
  pine mass and a building wall are all in the lower half. A chart on a black
  screen proves the glyphs exist and proves nothing about whether they can be
  read. What the frame shows: all 95 printable ASCII on three rows, all 33
  Cyrillic capitals and all 33 lowercase on two more, « » — on the sixth, then
  FOUR SOLID BLOCKS where Greek alpha, a CJK character, an accented Latin e and
  a stray 0x80 byte were asked for — the font saying "no" in the same frame as
  the letters. Below, unplated over grass with the 1 px shadow, a Russian
  interaction prompt and a mixed Latin/Cyrillic item name, both legible.
  Re-shot with `DFN_PALETTE=1` (`screenshots/palette_conifer/`): every glyph
  survives the 64-colour quantiser, which matters because the quantiser is now
  a user-facing setting.
- **GPU buffer budget (measured, not asserted, 10:08:2026 - 00:00:47):** the same
  route with `DFN_MESH_STATS=1` prints live/peak/created/destroyed mesh handles.
  Healthy today: peak 606 of 4096, 682 created / 682 destroyed, 0 live at exit.
  A leak shows as created != destroyed; exhaustion warns at 3072.
- **Crag shape acceptance (Rule 27, 8 frames read 09:08:2026 - 22:23:29):**
  `DFN_TOUR=1 DFN_CRAG_PROBE=1 DFN_TIME=0.30 DFN_TOUR_DIR=screenshots/crag
  DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 <build>/engine/app/dfn_app`.
  Ravenscar from four bearings (180/225/270/300 compass, peak -> eye) at 253 m
  and 300 m, eye 1.7 m on the valley floor. THE FRAMES DO NOT AGREE, and that
  disagreement is the result:
  - FROM THE SOUTH the crag is SHARP. A pointed summit tor with sky on both
    sides, asymmetric flanks, a shoulder on the descending right ridge, a rock
    bench with a hard splat lip. Not a dome.
  - FROM THE WEST at 300 m it IS A DOME — a single smooth convex arc, no crest,
    no bench, and the summit tor invisible behind the mass. Both western frames
    are backlit, which is the purest form of the "reads by value against sky"
    test, and it is the reading that fails.
  - FROM THE WNW the crag is UNREADABLE at BOTH ranges: a pine stand owns the
    frame. Two standpoints 47 m apart on the same bearing fail identically, so
    this is a property of the sector, not of one unlucky coordinate.
  INFERENCE (marked as such): with arete_count = 3 a bearing looking INTO a
  couloir sees two lobes and a notch, while a bearing looking flat-on at one
  lobe sees that lobe's own convex profile as the whole outline. Roughly a
  third of all bearings are flat-on. A per-bearing invariant that passes on
  average can therefore read 9-12 against a floor of 3 while a third of the
  valley sees a heap.
  WHY THIS FRAME WAS THOUGHT UNSHOOTABLE: the 600/717 m vantage was sized for
  LR, the temple mountain, which exists in NUMBERS.md and in LANDSCAPE §2.5 and
  in NO CODE PATH. It was never a streaming problem.
- **Sun caster cull (Rule 27, A/B + control, 09:08:2026 - 22:23:29):** three shadow-heavy
  crag vantages are BIT-IDENTICAL with the cull enabled and disabled. On its
  own that proves nothing — it is also what a cull that never fires looks like.
  The control is the same test INVERTED (only the rejected draws cast): all
  three frames then differ, so both sides of the test are non-empty.
- **Carried-light shadow acceptance (Rule 27, A/B read 09:08:2026):**
  `screenshots/point_light/00_point_shadow_on.png` vs
  `01_point_shadow_off.png` — same vantage, midnight, new moon, the light 6 m
  ahead (`DFN_TORCH=2`), the second shot adding `DFN_NO_POINT_SHADOW=1`. WITH
  the cube map the boulders lay a dark wedge across the lit grass toward the
  camera and the far trunk's base goes dark; WITHOUT it the same grass is lit
  through the boulders and the trunk edge stays warm. That difference IS the
  cube map.
  WHY THE PROBE STANDS THE LIGHT OFF, and why the obvious shot fails: with the
  flame at the HAND (0.35 m from the eye) almost every shadow it casts is
  hidden BEHIND its own caster from the player's viewpoint. That is true of
  any real hand-held light, it is not a bug, and it is why the acceptance
  frame for carried light must be an INTERIOR — a place where walls, corners
  and ceilings sit between the flame and the far surfaces — rather than open
  ground with props.
  STILL OWED (blocked, not forgotten): the frame from INSIDE the crag tunnel.
  core is mid-repair on the carves after the §2.8 massif reshape (tunnel down
  to 3 open stations, Backbarrow with no mouth) and will send the axis
  endpoints and a yaw when it is walkable; core also owns the per-vertex sky
  visibility that makes an interior dark in the first place (see below).
- **Day/night acceptance (Rule 27, 3 frames read 09:08:2026):** one vantage,
  three STATES (not variants of one image, per the single-variant rule):
  `screenshots/sky_dusk/00_sky.png` — orange horizon burn into violet, first
  stars at the zenith, far hills dissolving into the burn (fog pinned to the
  horizon colour); `screenshots/sky_night_full_moon/00_sky.png` — deep blue,
  star field, and the forest/river/lake/hamlet still READABLE, which is the
  "playable-dark" requirement; `screenshots/sky_moon/00_sky.png` — a quarter
  moon with a crisp terminator over a dark ridge, with the ground visibly
  darker than under the full moon, which is the requirement that phases change
  real brightness and not just the picture.
- **Thin-caster shadow acceptance (Rule 27, A/B read 09:08:2026):**
  `DFN_TOUR=1 DFN_SHADOW_PROBE=1 DFN_TOUR_DIR=screenshots/shadow
  DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 <build>/engine/app/dfn_app` ->
  `screenshots/shadow/00_thin_caster_shadows.png`. Shot twice (old constants
  vs new) and compared on a 2x crop of the trunk contact area: BEFORE, the
  foreground birch trunk and both foreground stones cast nothing at all —
  only the broad crown shadow existed; AFTER, the trunk lays a hard shadow
  band from its base, the mid-ground trunks show their own streaks, and every
  stone has a contact shadow. That A/B is the proof that the cause was texel
  density and not the submit path.
- **Map screen acceptance (Rule 27, frame read 09:08:2026):**
  `DFN_TOUR=1 DFN_MAP=1 DFN_TOUR_DIR=screenshots/map DFN_INTERNAL_RES=640x360
  DFN_PALETTE=0 <build>/engine/app/dfn_app` -> `screenshots/map/
  00_map_screen.png` (ONE frame). Checked: north up (crag NE, lake W, outflow
  river S — matches the probed seed-1 world); the corridor valley, terraces
  and crag dome read by shading; river and lake read as continuous water; the
  Vaelmere cluster (dwelling dots + tavern + trader + barn), 3 dungeon marks,
  the shrine cross, the tower-ruin bar on the crag and the castle block all
  distinguishable by silhouette; the player arrow reads position AND facing
  (yaw 2.36 -> south-east). All 16 testbed chunks were resident at the shot,
  so the frame does NOT demonstrate the unexplored plate — that path is
  covered by the unit test instead.
- **Verification cadence (user instruction via lead, 09:08:2026):**
  `bash tools/run_tour.sh build_render` shoots ONE variant (640x360, palette
  off); the 4-way matrix runs only behind `run_tour.sh build_render matrix`
  for actual look decisions. Read the 2-3 frames that prove the change, not
  all seven.
- **Feature-requests batch acceptance (Rule 27, frames read 09:08:2026):**
  04_hamlet_approach — brown wash gone, grass to the pads, thin §3.3 sand at
  the river; 00_crag_from_vaelmere — no red-brown midground mottling, tavern
  casts a readable shadow; 02/03/05 — canopy/birch/pine shadows ground the
  trees, chunky stones sit with shadow contact; 06 — per-crown shadows across
  the forest, no shadow-range cutoff line. Shadow mechanism verified with a
  temporary red-tint debug shoot (not shipped). Known core-side finding: wide
  dark WaterBed flats along the river bends (dirt*0.68) are core's classifier
  band (§3.3 cap is core's), render draws it faithfully — for design's
  close-out.
- **Stage-3b acceptance (Rule 27):** `bash tools/run_tour.sh <build>` shoots
  the 4-way matrix (640x360 / 320x180 x palette on/off) over the Tour v3
  route (7 frames: crag-from-hamlet, crag final approach, river, lake shore,
  hamlet approach, forest species, overview) — 28 frames, all read in the
  09:08:2026 look-dev loop (6 iterations, vantages corrected against the
  GENERATED world probed via scratch tools, not the §7.1 plan table).
  Findings recorded in the DONE report: pine strips out-angle the L0 from
  town ground (C4 violation -> design/core); hydrology drifted from the §7.1
  coordinates (fords/lake); WaterBed mud margins are 2.7% of the world and
  read very wide near bends.
- **Stage-3 acceptance (Rule 27):** `DFN_WATER=15 bash tools/run_tour.sh
  <build>` shoots the 4-way matrix (640x360 / 320x180 x palette on/off), six
  frames each. Checklist per frame: textures tile without obvious repetition
  at eye level; slope splat reads (grey rock on the steepest ground — faint by
  design on the flat stage-2 worldgen, see Materials.h); horizon fog blends
  terrain into the sky horizon color with no visible world edge (all vantages
  aim into the testbed interior); sky gradient + sun disc; water plane with
  beach band; palette mode visibly quantizes (Bayer dither) without destroying
  readability; both resolutions consistent. All 24 frames read by the agent in
  this stage's look-dev loop; the rock-splat mechanism additionally verified
  with a temporary aggressive threshold shoot (not shipped).
- **Stage-2 acceptance criterion (Q51, kept green):** `DFN_TOUR=1` run captures
  screenshots, none black, with visible ground and a correct horizon. Checked
  frame by frame against the checklist (Q24); golden-image pixel comparison
  later.
- Rule 27: any image-affecting change re-runs the tour before review.
- Headless: the same tour under null window/render must walk all steps and
  exit 0 (Rule 3) — CI smoke test without a GPU.
- doctest suites (stage 2): camera interpolation math (alpha 0/0.5/1,
  shortest-arc yaw across ±pi, pitch clamp), TerrainMesher (vertex count,
  edge-row equality between neighbor views, normals on a known slope), input
  edge detection semantics on the null backend.
- `python3 tools/header_check.py --all` passes; zero warnings with
  `-Wall -Wextra -Wpedantic` on both toolchains (build gates).

## The bitmap font (`BitmapFont.{h,cpp}`)

There were no glyphs anywhere in the engine, and four FINISHED, TESTED features
in three zones could not draw a pixel without them: every interaction prompt,
every item name in the inventory, every label on the map, and the whole start
menu. The font is deliberately small.

1. **A fixed-cell atlas, and the cell IS the advance.** 6x9 px cells (5x8 ink,
   the remaining column and row are the gaps), 16x11 = 176 slots, baked once at
   first use into one coverage mask. No caller ever adds spacing.
2. **Printable ASCII + the entire Cyrillic alphabet + « » —** (166 codepoints).
   The user writes Russian and Rule 5 puts every user-facing string in a
   localization file, so a Latin-only font would have shipped a placeholder.
3. **Cyrillic letters whose shape IS the Latin letter (А В Е К М Н О Р С Т Х,
   а е о р с у х) are ALIASES, not second drawings.** Two drawings of one shape
   drift; one drawing cannot.
4. **A MISSING GLYPH DRAWS A SOLID BLOCK.** Nothing else in the font fills its
   cell, so an unmapped codepoint, malformed UTF-8 or an authoring hole is
   impossible to read as a letter or as a space. The bake starts by filling
   EVERY slot with the block and then overwrites with real art, so a codepoint
   the mapping can reach but nobody drew ships visibly broken. A newline is not
   handled on purpose — it draws the block, because passing a multi-line string
   to a single-line drawer is a caller bug that should be in the frame.
5. **UTF-8 decoding reports malformed input rather than skipping it**: a bad
   byte advances exactly one byte and returns the replacement codepoint, which
   maps to the block. A test walks all 256 single-byte inputs to prove the
   decoder always terminates.
6. **There is NO layout engine.** No wrapping, no kerning, no bidi. `draw_text`
   and `text_width_px` are the whole surface; callers draw line by line.
7. **The 1 px drop shadow is not decoration.** At five pixels tall, unshadowed
   text vanishes over grass. The acceptance frame is what established that, and
   a full 8-way halo (which the map's markers use) closes the counters of a, e
   and о and turns a string to mush at this size.
8. **No user-facing string exists in `engine/render`.** Every entry point takes
   UTF-8 the caller already resolved. The glyph chart under `DFN_FONT_PROBE` is
   a verification hook in the same family as `DFN_TORCH`, not shipped text.

**The HUD layer** that carries a prompt over the world: `PixelCanvas` gained
`clear_transparent()`, the backend gained an `"overlay"` logical program (the
unlit shaders with an alpha blend — the name->state convention, no new shader
and no `IRenderer` change), and `RenderSystem::hud()` hands the caller a canvas
sized to the internal target which is composited over the world and under the
map. **The caller draws into it, not render** — the prompt's text is a
localization string resolved from `Highlightable::prompt_key`, so render owns
the surface and the blit and never the words.

STILL MISSING, AND IT IS NOT RENDER'S: a key-hash -> string table. Requested
from the lead with the exact shape (uint64 keys, a visibly-wrong placeholder on
a miss, one log line per missing key).

## The crash the user hit — ONE MESH PER PUDDLE

`set_water_bodies` uploaded one GPU mesh per water body. Core's hydrology emits
a `LakePlane` per pond and the 2x2 km testbed has **17,336** of them. bgfx hands
out 4096 vertex-buffer handles, so the water spent the entire pool AT STARTUP:

    pre-fix:  4078 water meshes, peak 4091 of 4096, 13903 creates FAILED,
              uploads: terrain 0, voxel 0, scatter 34 chunks -> 0 meshes
    post-fix: 26 bucket meshes, peak 606 of 4096, 0 failed, 682 created /
              682 destroyed, 0 still live

Three separate lessons, and the crash is the least of them.

1. **A HANDLE WRAPPER THAT VALIDATES ITS OWN BOOKKEEPING IS NOT A VALIDITY
   CHECK.** `MeshHandle::valid()` means "our id != 0", and the id is ours, not
   bgfx's. `create_mesh` stored `BGFX_INVALID_HANDLE` (idx 0xFFFF) and returned
   a "valid" handle for it; `bgfx::destroy` only asserts in debug, so in release
   the shutdown sweep indexed `m_vertexBufferRef[0xFFFF]` and walked off the
   array. Now: a failed create is refused and reported, every destroy is guarded
   by `bgfx::isValid`, and live/peak/created/destroyed are counted with a
   warning at three quarters of the budget so the cliff is reported BEFORE it is
   walked off (`DFN_MESH_STATS=1` prints the summary unconditionally).
2. **THE SILENT HALF WAS WORSE THAN THE CRASH.** The user was walking around a
   world drawing almost nothing that came after the water, and the process
   reported success. `upload_terrain` used to `return;` on a failed create. It
   now reports the first failure of the run with the per-path counts.
3. **COST MUST SCALE WITH SCREEN AREA, NOT WORLD AREA.** Water bodies are now
   merged into buckets on a `CHUNK_SIZE` world grid, each with an AABB, and
   frustum-culled: 64 buckets at 2x2 km, 1600 at 10x10 km, regardless of how
   many puddles there are. It also deleted 4078 draw calls a frame that nobody
   had profiled. The lake COUNT is core's and is not being called wrong.

## The conifer palette family — and the premise that did not survive it

Design ruled (LANDSCAPE §4.2) that the budget is **64 ENTRIES, not eight
families of eight**: ramp depth follows the lighting range a family carries and
the screen area it covers. Sand 8->5 and water 8->5 funded a 6-shade conifer
family with nothing deleted. `BgfxPalette` also gained the CPU mirror of the
shader's quantiser (`palette_quantise`, `palette_mean_shade_step`,
`palette_separation_steps`) so separation is MEASURED in design's unit rather
than argued about.

**Measurement corrected two things, and both corrections matter more than the
ramp:**

1. **Needles never quantised into grass greens. They quantised into WATER
   TEALS — on the old palette too, and cleanly resolved across three adjacent
   entries.** The stated cause of the whole change was false.
2. **THE QUANTISER'S METRIC WEIGHTS BLUE 0.11.** (0.30 R / 0.59 G / 0.11 B,
   the `fs_upscale.sc` contract.) So a hue change that lives in blue moves
   almost nothing, and "blue-green water" versus "green needles" is a
   distinction this quantiser essentially cannot see. The first conifer
   endpoints were picked by hue, looked obviously distinct to a human, and
   FAILED: the needle tones still landed on water. The family had to be
   re-derived along the ray through flora's measured needle albedo
   {0.12, 0.22, 0.19}, 0.17x to 1.66x. **A separator must move RED or GREEN.**

Measured gains, stated honestly: pine vs LIT rock 2.18 -> 2.24 steps (it already
cleared the floor of 2); pine vs SHADOWED rock 0.74 -> 0.66 and still merged,
which agrees with design's own conclusion that in deep shadow only silhouette
separates — the test ASSERTS that case stays under 2 so the limit is recorded
rather than re-attacked. What the family really buys is that the most common
dark mass in the world no longer shares a family with the water, and that its
colours are now derived from what flora draws instead of chosen by an accident
of the metric. flora's requirement is pinned: the three baked form tones land on
three ADJACENT conifer entries (60, 61, 62), so the whorls keep reading as
separate branch layers.

## Named rule — PALETTE SIGNAL STRENGTH (applies to every shading decision)

The 64-colour palette post is 8 ramps x 8 shades. That geometry decides which
shading effects can be SEEN at all, and it is not intuitive:

1. **A ramp change (hue) is the strongest signal available. A shade step
   (brightness) is the weakest.** An effect that needs to survive quantization
   should move HUE, not value. Pushing a back-lit leaf from green toward gold
   crosses into another ramp and reads instantly; making it 10% brighter lands
   on the same index and is literally wasted work.
2. **Sub-step differences do not vanish — they become Bayer dither.** On large
   surfaces that is a usable gradient. On few-pixel geometry (leaf cards, thin
   props, distant detail) it reads as NOISE rather than as the effect, which is
   worse than not doing it.
3. Therefore: **exaggerate deliberately or do not build it.** An effect tuned
   to look "subtle and tasteful" in a full-colour preview will read as either
   nothing or noise once the palette pass runs.

This applies to everything shading-related we have not built yet — magic glow,
torch light on walls, wet surfaces, blood, snow — not only to foliage. It was
first derived for leaf translucency, where it agreed independently with the
user's reference photos: both the palette arithmetic and the photographs say
"push warm, not bright".

## Named rule — CARDS BUY ANGULAR COVERAGE (the foliage-card floor)

Binding for every card-based (fixed-orientation plane) foliage build in this
zone and flora's: **a card cluster uses AT LEAST 3 planes, and card count is
chosen against the WORST azimuth, never the average.**

The reasoning, so nobody re-litigates it from intuition:

1. A fixed card presents its area times |sin(view angle to its plane)|. Any
   set of ONE or TWO planes has azimuths where every plane is near edge-on
   simultaneously (two planes fail where the view bisects them); THREE planes
   at distributed orientations cannot all be edge-on at once, which puts a
   floor under presented area at every azimuth.
2. **A thing that vanishes at some viewing angles is indistinguishable from a
   thing that is not there.** The player does not see the average azimuth;
   they walk around the tree. An artefact that exists only from a third of
   bearings is not a third of a defect — it is a full defect that hides from
   two thirds of verification vantages (Rule 27's "a vantage that cannot fail
   is not evidence" applies with force here: card coverage MUST be checked at
   the worst azimuth, which is computable — the bisector of the largest
   angular gap between plane normals).
3. Per-plane statistics do not rescue an under-carded build. A species with
   one card per spray survives "on average" because many sprays at random
   yaws rarely all align — but a low-spray specimen (young pine, sparse
   crown) aligns often enough to strobe as the camera orbits.

**The worked example — THE BIRCH INCIDENT (flora, 09:08:2026, 2553ae7).** The
birch carried TWO crossed cards per leaf cluster, under a comment saying "a
narrow crown does not need a third plane". In the tour frame the birches were
a line of bare white poles with a few flecks: at the tour's bearing both
planes presented nearly edge-on and the crown vanished — the user-rejected
silhouette resurfacing as a pure viewing-angle artefact AFTER the shape
itself had been fixed and every shape invariant passed. No isolated render,
no per-mesh invariant, and no average-case statistic caught it; only a frame
from the unlucky azimuth could. The pine had the same exposure at one card
per spray and "survived only statistically" — flora has since moved its
sprays to 3 planes under this rule.

Corollary for review: when a foliage build sets its card count, the number to
ask for is the presented-area MINIMUM over azimuth (relative to the maximum),
not the count itself. Three planes is the floor, not the target; dense
species may need more, and the check is the same either way.

## Named rule — A CUTOUT IS NOT AN EDGE THE FRAMEBUFFER CAN SEE

The user's oldest complaint — *«при беге трясет»*, *«всё дергает и
перерисовывается очень рябью»* — was handed to this zone with the diagnosis
already made by two others: not camera motion (`head_bob=0` leaves the frame
pair bit-identical), not the shadow map, and not foliage detail (flora
measured every geometric lever they own; the canopy is at the bottom of that
curve). The remaining variable was EDGE COVERAGE. That was right. The part
that was not yet known is WHICH edges, and it is the whole content of this
section.

**MSAA on the internal target was the obvious answer and it is worth almost
nothing on its own.** Measured with `DFN_FLORA_PROBE` + `DFN_WIND_FREEZE=120`,
one 0.05 m stride at `RUN_SPEED`, 640x360, palette off, share of screen
flipping by more than 64 luma, against a control that comes back 0.000 %:

| arm | near canopy | treeline |
|---|---|---|
| before | 0.864 % | 0.094 % |
| MSAA 4x on the internal target | 0.819 % | 0.080 % |
| MSAA 8x | 0.819 % | 0.080 % |
| MSAA 4x + mipped mask + alpha-to-coverage | **0.621 %** | **0.004 %** |

**8x equalling 4x to three digits is the finding.** If the residual were
coverage QUANTISATION — an edge whose partial coverage the target cannot
express finely enough — doubling the samples would have moved it. It did not
move at all, which says the residual pixels were never partially covered in
the first place: the shader wrote them or discarded them, and no number of
samples changes a `discard`.

Where the flicker actually lives, and both halves were measured, not reasoned:

- **The treeline is the leaf MASK.** A leaf card at 70 m is minified about
  30:1, so one screen pixel covers ~900 mask texels and the point sampler
  picks ONE. A 0.05 m step — one 120 fps frame at `RUN_SPEED` — picks a
  different one and the pixel flips leaf/sky at full contrast. That edge is
  inside the texture fetch, so the framebuffer never sees it and MSAA cannot
  touch it. A mip chain turns the fetch into the AVERAGE over those texels,
  and alpha-to-coverage lets that average survive as a fraction of the pixel
  instead of being rounded to 0 or 1 by the cutout. Cards alone, near canopy:
  0.382 % → 0.043 %.
- **The near residual is the TRUNKS, and it is not a defect.** Wood alone goes
  1.070 % → 0.891 % and stops. Under the crowns the boles stand at 2-5 m,
  where a silhouette translates 11.7/d ≈ 2-6 pixels per frame. That is not an
  edge failing to be antialiased, it is an edge that genuinely moved several
  pixels; no coverage scheme suppresses it and none should. The 11.7/d rule
  that located this fix also bounds it: it says nothing can move a whole pixel
  BEYOND 12 m, and the near vantage is standing inside that radius.

**THE MEAN IS PRESERVED, WHICH IS WHY THIS DOES NOT THIN THE CANOPY.**
Point-sampling a minified mask and testing it against 0.5 draws the pixel with
probability equal to the mean alpha; using that same mean as coverage draws
the same expected area, deterministically instead of by dice. Measured on the
frames: mean luma over the treeline band 109.33 → 111.35 (+1.8 %), over the
whole near-canopy frame 104.80 → 105.06 (+0.2 %).
`FLORA_PRESENTED_AREA_FLOOR_M2` is untouched and no card changed size.

**The palette quantiser eats some of it and the fix still wins.** This was the
open question when the work started — antialiasing the 64-colour post then
throws away is wasted work. The same matrix with `DFN_PALETTE=1`: near canopy
0.863 % → 0.712 %, treeline 0.098 % → 0.017 %. So the quantiser costs about
0.09 points near and turns the treeline's 0.004 into 0.017 — real, and not a
choice anyone has to make, because both numbers go down in both modes and the
shipping default is palette off.

**And it does not un-pixelate the game.** The pixel GRID is unchanged: 640x360
internal, integer upscale, point sampling. MSAA decides what colour each of
those same pixels gets; it adds shades, not resolution. `MAG` filtering stays
POINT everywhere, so every texture magnified on screen keeps its hard edges —
only MINIFICATION, where the sampler was picking one texel out of hundreds,
changed.

The parts, all inside this zone: `BgfxRenderer.cpp` creates the internal
colour+depth target with `BGFX_TEXTURE_RT_MSAA_X4` (`DFN_MSAA=0|2|4|8`
overrides; note the MSAA bits are a 3-bit FIELD, `BGFX_TEXTURE_RT` is that
field's value 1, so the depth attachment ORs the same flags in whole);
`BgfxRendererResources.cpp` builds an alpha-weighted mip chain for cutout
masks ONLY — discriminated by the DATA, an RGBA8 texture containing any alpha
< 255, so the terrain atlas can never be mipped and bleed across its cell
borders; `BgfxRendererSubmit.cpp` binds those masks `MIN_LINEAR|MAG_POINT` and
adds `BGFX_STATE_BLEND_ALPHA_TO_COVERAGE`; `fs_foliage.sc` writes coverage
into alpha. `fs_shadow_cutout.sc` pins mip 0 explicitly — the shadow pass has
no coverage to spend, and an averaged alpha against its hard 0.5 threshold
would have thinned the canopy's SHADOW as a side effect of fixing the colour
pass.

**`DFN_MSAA=0` is a BIT-EXACT before-arm, deliberately.** The mip chain is
built only when the target is multi-sampled, because a mipped mask sampled
through the old 0.5 cutout is the classic distant-canopy dissolve — an off
switch that shipped a second, different defect would not be a control
(Rule 30). Verified: `DFN_MSAA=0` reproduces 0.864 % / 0.094 %.

## R1 — AERIAL PERSPECTIVE, AND THE MEASUREMENT THAT SAYS WE HAVE NONE

`docs/REFERENCE_FRAMES.md` R1: in every wide reference frame the far field
loses contrast and shifts toward the sky colour, CONTINUOUSLY over the whole
visible range. The user's word for what we have instead is «плоский».

### The finding, in one line

**Fog begins at 2400 m. The world is 1024 m across. Nothing in it is ever
hazed at all — the fog factor is not small, it is exactly zero everywhere.**

`dfn_env.sh::dfn_fog_factor` is `smoothstep(u_fogStart, u_fogEnd, dist)`, and
`Materials.h` sets the span from `CAMERA_FAR`:

    fog_start = LOOKDEV_FOG_START_FRAC (0.30) * CAMERA_FAR (8000) = 2400 m
    fog_end   = LOOKDEV_FOG_END_FRAC   (0.85) * CAMERA_FAR (8000) = 6800 m

`smoothstep` is 0 below its first edge. The testbed is `TESTBED_SIZE` 1024 m
square, so the longest sightline inside the world is its diagonal, 1448 m —
40 % of the way to the point where haze switches on. A ridge at 500 m and a
ridge at 2 km therefore differ by NOTHING, and so do a pebble at 3 m and the
crag at the world's edge. This is not a tuning error in a working system; the
system has never run.

The zone's own Tour comment already recorded the symptom without naming the
cause: "outward aims put the unloaded world edge inside the fog-free range and
break the horizon". The fog-free range is the entire world.

### The measurement (`tools/measure_aerial.py`)

THE SAME LANDFORM AT THREE RANGES, one variable. The L0 crag (peak 830,200,
`L0_RELIEF` 115 m) shot from bearing 240 deg at 250 / 500 / 900 m — the widest
spread of ranges that keeps every standpoint on open ground inside the testbed.
Noon (`DFN_TIME=0.5`) and `DFN_CLOUD=0`, so the sky behind the crest is a clean
gradient and rock/sky segmentation cannot be argued with. Luma is the
quantiser's own metric (0.30/0.59/0.11), 0..255.

| range | L(rock) | L(sky) | SEPARATION \|dL\| | TEXTURE sd(L) |
|---|---|---|---|---|
| 250 m | 97.36 | 109.45 | 12.09 | 23.06 |
| 500 m | 91.76 | 125.83 | 34.07 | 22.76 |
| 900 m | 91.93 | 138.79 | **46.86** | 21.54 |

Read it twice, because it says two different things:

1. **The crag's own value does not move: 97.4 / 91.8 / 91.9.** Over a 3.6x
   change of range it varies by 5.4 luma units, and that variation is which
   faces are turned toward the eye, not distance. Its internal texture contrast
   falls 23.06 -> 21.54, i.e. 6.6 %, which is what downsampling a 215-px-wide
   mass to 45 px does on its own. Aerial perspective would have moved both.
2. **The separation from the sky goes the WRONG WAY — 12.1 -> 46.9, a factor
   of 3.9.** The far crest stands out nearly four times harder than the near
   one, because the sky brightens toward the horizon (`fs_sky`'s haze band)
   while the crest does not brighten at all. In every reference frame the far
   ridge stands out LESS than the near one. Ours is a dark cut-out pasted on a
   pale horizon.

The second row is the one to keep: it is not that we are missing an effect, it
is that the frame currently asserts the OPPOSITE of the reference — distance
increases contrast here.

### Consequences

- The 0.30/0.85-of-`CAMERA_FAR` span is not a look-dev value that needs
  retuning. A far-plane fade cannot produce R1 at all: R1 asks for a
  CONTINUOUS fall from the eye outward, and a `smoothstep` between two
  distances is by construction flat on both sides of the ramp. It has to be
  replaced by extinction, not moved.
- Extinction is `exp(-optical_depth)`, which is never flat anywhere and needs
  no far plane in it — so it also stops being coupled to `CAMERA_FAR`, which is
  a depth-buffer number and has no business setting how thick the air is.
- The in-scatter colour must be the SKY AT THE VIEW DIRECTION, not one flat
  horizon colour, or a ridge high in the frame melts into a colour the sky
  above it does not have.
- Haze density falls with height, or the mountain crown hazes as hard as its
  foot and the frame gains no vertical information. This is also the cheap half
  of R2.

Rows landed in NUMBERS.md: `HAZE_SCALE_LENGTH`, `HAZE_HEIGHT_SCALE`.

### After: what the fix did, and what it did not do

`dfn_fog_factor` is gone. `dfn_aerial()` is Beer-Lambert extinction through air
whose density falls with height, fading into `dfn_sky_gradient()` AT THE VIEW
DIRECTION — so a ridge high in the frame and a ridge on the horizon each melt
into the sky that is actually behind them, and the sky's gradient now exists in
exactly one place because `fs_sky` calls the same function.

`standout` (mean |L - L(sky)| over the whole box, classifying nothing):

| range | BEFORE | AFTER (1400 m) | counterfactual (600 m) |
|---|---|---|---|
| 250 m | 19.71 | 18.45 | — |
| 500 m | 27.19 | 20.67 | — |
| 900 m | 30.40 | **20.45** | **14.57** |
| trend | **+54 %** | **+11 %** | **-21 %** |

**The defect is closed and the reference is not yet reached, and those are two
different sentences.** BEFORE, contrast ROSE with distance — the frame asserted
the opposite of every reference. AFTER, the column is flat. But flat is not what
the reference does: in all 16 frames the far ridge stands out LESS than the near
one, and only the 600 m arm actually gets there (-21 %).

**Why the shipped value is 1400 m and not 600 m, and why that is the lead's call
rather than mine.** 1400 m is derived FROM design's existing contract, not
against it: §1.3a separates the two landmarks by depth, §7.1b fixes Ravenscar's
acceptance ranges at 287-717 m and §2.5 sites the LR at 1.4-1.6 km, so defining
"hazy" as 1/e puts the scale length at the LR's nearest siting. Ravenscar then
runs 0.82 -> 0.60 across its whole range (solid, as §1.3a demands) and the LR
sits at 0.37-0.32. At 600 m Ravenscar would read hazy at its own verdict
vantage, which §7.1b calls a bug in as many words. So the conflict is real,
it is between design's approved contract and the user's reference frames
delivered today, and it is handed over as a PAIR OF FRAMES rather than as an
argument (`docs/acceptance/render-aerial-STRONG600-900m-*.png`).

**The next lever, and it may dissolve the conflict entirely.**
`HAZE_HEIGHT_SCALE` is 250 m, and the crag's crown sits at 115-155 m where the
air is already 0.6 of its ground density — so the height falloff is protecting
exactly the thing design wants protected while also weakening exactly the thing
R1 wants strengthened. A SHORTER height scale hazes the low ground and the tree
line hard (which is the user's «плоский») while leaving a summit comparatively
clear. That is not a dodge; it is literally what reference frames 02 and 12
show — a hazed base and a lit crown, cut by the mist band. Untested, and it
needs a second override before it can be shot as a pair.

**What was NOT touched.** `u_fogColor` / `u_fogStart` / `u_fogEnd` are now read
by no shader, but the `RenderEnvironment` fields stay: that header is a frozen
contract (Rule 26) and deleting fields is a request to the lead. Same reason the
haze rows travel from the generated header rather than through the environment —
which is fine until WEATHER wants a fog morning, and that IS a contract change
to ask for.

### The premise under 1400 m was withdrawn by its own author

`HAZE_SCALE_LENGTH` 1400 m was derived from §7.1b's "Ravenscar solid at
287-717 m". §1.6.1 had already ruled `d_accept` = 3R = **360 m** for Ravenscar,
and ruled that a landmark shot beyond its `d_accept` certifies nothing about
shape — but that correction was never propagated into the clause that names
717 m. So the derivation was sound and the input was a stale cross-reference:
Rule 39's shadow copy, wearing the costume of a citation rather than of a
duplicated value. Worth recording as a defect class, because "derive it from the
contract instead of inventing it" is what this project asks for, and here doing
exactly that produced a wrong number.

Recomputed on `exp(-d/L)` with the correct verdict distance:

| | 287 m (rhythm) | **360 m (verdict)** | 717 m (no longer a test) |
|---|---|---|---|
| L = 1400 | 0.82 | **0.77** | 0.60 |
| L = 600 | 0.62 | **0.55** | 0.30 |

The only cell where 600 m looked alarming stopped being a test.

### The three propositions, and what each instrument had to be

§10.9 replaced "haze on Ravenscar is a bug" with three clause-specific
propositions in units of `PALETTE_SHADE_STEP_REF` = 0.0784 (= 20.0 of 255 in
the quantiser's luma metric):

- **H1 — silhouette.** 360 m, backlit: |body - adjacent sky| >= **2 steps
  (40.0)** ALONG THE WHOLE CONTOUR.
- **H2 — band rhythm.** 287 m, raking: riser/bench separation >= **1 step
  (20.0)**, read AT THE LOWEST VISIBLE BAND PAIR, never on the flank mean.
- **H3 — depth separation.** contrast(L0 at 360 m) >= **1.7x**
  contrast(far landmark at 1400 m).

Each got its own instrument and its own boxes, because Rule 41 is exactly the
trap here: `standout` was aimed at a ridge against sky and cannot accept a claim
about a valley floor, and neither can accept a claim about the weakest column of
a contour. So `tools/measure_aerial.py` grew three more modes, and two of them
report an EXTREMUM rather than a mean on purpose:

- `contour` (H1) reports the **minimum over columns**. A mean would pass a
  mountain whose shoulder has dissolved as long as its peak stayed dark — which
  is the precise shape of the failure haze produces.
- `bands` (H2) lists adjacent extremum pairs **hem-first**. Under the height
  lever the crown is protected and the hem is starved, so a flank average
  reports a pass exactly when the failure is sitting at the bottom of the frame.
- `ground` (the lowland) reports the depth cue between two bands of the SAME
  surface, because a valley floor stands against no sky and «плоско» means the
  far ground looks like the ground underfoot.

**H3 does not choose between 1400 and 600**, and that was design's own finding:
§1.3a asked for a RATIO, and a shorter scale length is what makes ratios large,
so H3 is satisfied 2.7x better at 600 m. Only H1 and H2 can choose, and both
live on frames nobody had taken.

### The three propositions, measured — and the two floors nobody can meet yet

Three arms, plus a NO-HAZE CONTROL (Rule 30), all from one binary with only the
two haze rows moved. `Z` is `DFN_HAZE=100000` — air so thin it is not there.

| arm | scale L | height H | H1 min (need 2.00) | H1 p05 | H2 hem (need 1.00) | lowland dL |
|---|---|---|---|---|---|---|
| **Z** control, no haze | — | — | **2.36** | 2.77 | **0.61** | — |
| A shipped | 1400 | 250 | 1.84 | 2.25 | 0.45 | +10.12 |
| B one lever | 600 | 250 | 1.34 | 1.69 | 0.25 | **+19.78** |
| C two levers | 600 | 40 | **1.75** | 1.96 | 0.17 | **+19.78** |

Steps are `PALETTE_SHADE_STEP_REF`; the lowland cue is luma of 255.

**1. H2 IS FAILED BY THE NO-HAZE CONTROL — 0.61 of the 1.00 it needs.** The band
rhythm at the hem is not there at zero air, so H2 is not currently a fact about
atmosphere at all and cannot arbitrate between scale lengths. Haze then makes it
worse (0.61 -> 0.45 -> 0.25 -> 0.17), but the deficit exists before any air is
added. Design named H2 at the hem as its one non-negotiable; it is unmet by the
terrain, and no choice of haze can meet it.

**2. H1's floor leaves 0.36 steps of headroom over a haze-free render.** The
control reaches 2.36 against a floor of 2.00, so H1 as written forbids very
nearly all aerial perspective at 360 m — every haze arm fails it on the minimum.
The statistic matters here and design should say which it meant: over 105
columns of a 640x360 frame, a hard minimum is ONE PIXEL COLUMN, and Rule 30a
says a threshold at the instrument's resolution has no margin. Read at the 5th
percentile instead, A passes (2.25), C misses by 2 % (1.96) and B fails (1.69).

**3. THE HEIGHT LEVER IS REAL, AND IT MOVES THE THREE EXACTLY AS DESIGN
PREDICTED.** C over B: H1 +31 % (1.34 -> 1.75), H2 at the hem worse
(0.25 -> 0.17), lowland **identical**. That the lowland numbers are not merely
close but EQUAL is the structural point the pair was shot to establish: with the
density anchored at `HAZE_BASE_HEIGHT`, the valley floor sits inside the clamped
full-density layer, so `HAZE_HEIGHT_SCALE` cannot reach it. The two rows are
independent levers and not two settings of one.

**4. H3 passes everywhere and discriminates nothing — and it is UNSHOOTABLE.**
The far landmark it compares against does not exist: NUMBERS records that no
`LR_` row is referenced by the generator. Analytically, ratio T(360)/T(1400) is
2.1 at L=1400 and 5.6 at L=600 — both over the 1.7 floor, the short length 2.7x
better, matching design's own arithmetic. Recorded as computed, not as shot.

**Where that leaves the choice.** C buys the user's R1 outcome — the lowland
depth cue DOUBLES, +10.12 -> +19.78 luma, chroma drop 11.66 -> 24.33 — for
0.09 steps of silhouette against A (1.84 -> 1.75 on the minimum, 2.25 -> 1.96 on
p05). That is the pair the lead asked for, and on the evidence it argues for C.
But design's condition was "all three hold at the shorter length", and they do
not hold at ANY length, including no haze at all. So the shipped rows are NOT
flipped here: this goes back with the frames, because the thing that changed is
not which length wins, it is that two of the three floors are currently
unreachable and one of them is not about haze.

**A vantage defect found on the way.** Design's own §7.1b frame-2 standpoint
(545,165) can no longer test band rhythm: the massif is occluded by a pine stand
that has grown across the sightline, leaving only slivers of flank between
trunks (`render-haze-H2-287m-DESIGN-VANTAGE-OCCLUDED-*.png`). H2 was therefore
shot from (581,344) — the same 287 m and the same raking hour, on a bearing with
a clear sightline. Design owns the vantage and should re-stamp it.

**The instrument failed twice in the same way, and both are recorded in it.**
`standout`'s first version segmented the landform by colour and, in the strong
arm, re-found 85 pixels of 328. The `contour` mode was then written with the
same flaw wearing new clothes — it walked each column down to the first non-sky
pixel, and in the strong arm the hazed mountain classified AS SKY, so the walk
continued into the rock and reported a median of 2.61 luma for a mountain
plainly visible in the frame. `bands` made it a third time, finding the splat's
dither instead of the benches. The general rule, now written at the top of the
tool: A METRIC MAY NOT LOCATE ITS SUBJECT BY THE PROPERTY UNDER TEST. Edges and
band rows are geometry, so both are detected once on the control arm and every
arm is then read at the same pixels.

### SHIPPED: C, and then R2 on top of it

The lead took the decision on the frames rather than on H1's arithmetic, and
recorded the reason: at 900 m the A and C arms are barely distinguishable by eye,
while the ground in BOTH is a flat green plane with a visible repeating
smoothing pattern, identical pebbles and a palisade of identical trees — the
user's «плоско как в майнкрафте» literally, and none of it atmosphere. So the
arm with the honest physics and twice the depth cue ships.

`HAZE_SCALE_LENGTH` 1400 -> **600**, `HAZE_HEIGHT_SCALE` 250 -> **40**.
Verified: the default build with NO overrides reproduces arm C exactly — H1
1.75 min / 1.96 p05, lowland +19.78 / 24.33, pixel difference 0.050 % (streaming
jitter, per pngdiff's own caveat).

Closed with it, and none of these are mine to fix:
- **H2 goes to design and core** as a terrain/splat defect. A threshold that
  fails at zero dose is not a threshold on the dose.
- **H1's floor is re-derived by design**, on p05 rather than on a hard minimum
  over 105 columns — a bound sitting flush against its own control is a
  coincidence, not a design.
- **H3 is recorded as computed, never shot, and is not a gate.** A quantity with
  no counterpart in the generator cannot exclude anything.

## R2 — THE MIST BAND BELOW THE CLOUD LAYER

Reference frames 02, 04 and 12: a horizontal layer of mist INTERSECTS the
terrain partway up, cutting a mountain into a lit crown and a hazed base. That
band, not the clouds, is what gives the reference its sense of cloud volume, and
it costs a few lines against a volumetric renderer.

It is a second term in the SAME density integral, not a second fog pass: a
trapezoid in altitude (plateau half the extent, quarter-width smoothstep ramps)
whose antiderivative is closed form, so the mean along a ray stays exact and
nothing is ray-marched. Rows: `MIST_BAND_HEIGHT` 70 m, `MIST_BAND_THICKNESS`
32 m, `MIST_BAND_DENSITY` 4.

**Why an altitude slab makes a BAND and not merely more haze up high**, which is
the part worth keeping: for an eye below the layer, optical depth to a surface
RISES as the surface climbs through the layer, PEAKS when the surface sits at the
layer's top — the ray has just crossed the whole layer, at the shallowest angle
any such ray will — and FALLS again above it, because a higher surface is seen at
a steeper angle and a steeper ray spends less length inside a horizontal slab.
Clear base, bright stripe, clearer crown. It emerges from the geometry; nothing
is painted on.

**Measured** (`profile` mode, flank strip x270..320, mist ON against a mist-OFF
control, 360 m):

| row (hem first) | ON | control | delta |
|---|---|---|---|
| 199 / 191 / 183 | 75.53 / 75.49 / 73.11 | same | **+0.00** |
| 175 | 69.57 | 69.03 | +0.54 |
| 167 | 87.66 | 67.88 | +19.78 |
| **159** | 111.03 | 78.61 | **+32.43** |
| 151 | 116.52 | 90.75 | +25.77 |
| 143 | 119.08 | 104.64 | +14.44 |
| 135 | 123.19 | 117.53 | +5.66 |
| 119 (sky) | 123.77 | 123.77 | **+0.00** |

**Zero at the hem, +32.43 at row 159, zero again above.** A BUMP — and a bump is
the one thing no setting of the R1 rows can produce, because a haze change moves
a whole flank monotonically. `DFN_MIST=0` erases it and nothing else, which is
the Rule 30 control.

### The instrument's own rule went four for four

`profile`'s first version asked whether the flank's luma profile TURNS AROUND,
reasoning that haze varies monotonically with height while a layer puts a maximum
in the middle. True of the mountain, false of the measurement: the strip runs off
the summit into SKY, sky is the brightest thing in the frame, and the mist-OFF
CONTROL passed. That is the fourth time in one file that an instrument included
something that was not its subject — after `standout` segmenting by colour,
`contour` walking into the rock, and `bands` finding the splat's dither.
Measuring against the control cures it structurally: the sky is identical in both
arms and subtracts to zero, so only the layer survives.

## R3.1 — THE HORIZON DOMES, WHICH WERE A BUG AND NOT A STYLE

The lead's report on the shipped vista: half a dozen hemispherical caps sitting
on the horizon like mushroom tops. His reading is the right one — a flat cloud
reads as a stylistic choice, a dome reads as breakage.

**The cause was DIMENSIONALITY, not tuning.** The band read the coverage field
on a ring at a fixed distance, so `F` was a function of AZIMUTH ALONE, and the
threshold rose with height. For a fixed azimuth that makes alpha monotone in
height: the silhouette was a single-valued function of azimuth, so **no hole and
no overlap was POSSIBLE anywhere in it, provably rather than incidentally.**
Then solving `T(hn) = F` for a squared threshold gives `hn ~ sqrt(F)` — a
vertical tangent where a lobe crosses the threshold, and a flat top at the lobe's
peak. Vertical sides under a flat top is a mushroom cap. The previous pass had
already tried the linear exponent and got straight-sided tents; both are
symptoms of inverting a 1-D function, which is why neither exponent could win.

**Fix:** read the field in 3D at the point where the view ray meets the ring —
ring position horizontally (continuous all the way round, no azimuth seam),
altitude vertically, stretched by `CUMULUS_VERTICAL_STRETCH` 1.6 so one field
cell is about one band tall and 1.6x wider, the proportion a real fair-weather
cumulus has. The threshold goes back to linear, because in 3D the shape comes
from the field instead of from the inversion.

`dfn_cloud_field3` reuses the octave weights and the CDF remap of the 2D field
and **its own mean and SD**: measured over 400k samples the 3D sum is
0.5000 / **0.1185** against 2D's 0.4980 / 0.1368.

**I first wrote that reusing the 2D pair "would have re-run Rule 31 exactly".
That was overstated, and the control test caught me.** Measured properly, the 2D
constants cost:

| | worst coverage error | at cover 0.05, admits |
|---|---|---|
| correct 3D pair | 0.0225 | 0.042 |
| 2D pair reused | 0.0311 | **0.022** (asked 0.05) |

So it reproduces Rule 31's *form* at roughly a fifth of its size — 1.4x in
aggregate, but **a third of the cloud lost at the sparse end**, which is exactly
where a few fair-weather cumulus on a clear day live. By DECILE error the pairs
are 0.0282 vs 0.0221 and the difference nearly vanishes; that is why the control
asserts on coverage, which is what the constant is *for*. Real, systematic,
worth its own constant — but not the catastrophe I claimed.

**Measured** (`runs` mode, cumulus band y160..204, mask b-r <= 5):

| | columns with cloud | of those, with a HOLE | cloud fraction |
|---|---|---|---|
| BEFORE | 196 | **0 (0.0 %)** | 17.6 % |
| AFTER | 363 | **11 (3.0 %)** | 13.5 % |

Zero is not a small number here, it is the structural prediction: a single-valued
skyline cannot have a hole. Any positive count proves the silhouette stopped
being one. The band also went BROADER and THINNER — 363 columns carrying cloud
against 196, on less total cloud — which is the "nearly continuous along the
horizon, varying in how much survives with height" the code always claimed.

**Owed:** `CloudModel.cpp` carries the CPU reference for the 2D field and
`CloudModelTests.cpp` asserts its constants. The 3D field has no CPU reference
and no test yet; its two constants come from a scratch measurement recorded in
the shader. That is a real gap, not a footnote.

**A consequence to flag rather than fix:** the tour's elevated sky vantage sits
at ground+70 m = 84.3 m, and `MIST_BAND` spans 54-86 m — so that standpoint is
now INSIDE the mist layer and its frames are milky. Physically right (you are
standing in it) but it makes that vantage a poor acceptance viewpoint for
anything else, and it is a hint that 70 m may be low for a world whose vantages
reach 100 m. Not touched: the lead's instruction was to leave the knob alone
until R3 is done.

## R2 — THE BAND'S HEIGHT, DERIVED. THE TWO CONSTRAINTS DO NOT CONVERGE.

Asked to derive the band height from two constraints — the layer must sit above
the highest legal vantage, and it must cut the massif between foot and crown —
and to report rather than fudge if they disagree. **They disagree.**

Every vantage in the world, measured rather than assumed (all four probe sets):

| vantage | eye, m | |
|---|---|---|
| river_ford | 14.42 | player |
| lake_bluff | 16.35 | player |
| crag_final_approach | 20.36 | player |
| forest_species | 20.78 | player |
| hamlet_approach | 21.96 | player |
| crag_from_vaelmere | 23.23 | player |
| massif_verdict | **25.44** | player — the highest |
| sky | 84.43 | inspection camera |
| overview / cloud_shadows_valley / cumulus_upwind | **99.62** | inspection camera |

Massif: foot ~20 m, crown ~135 m (`L0_RELIEF` 115).

**Taking constraint 1 at the world's highest vantage, 99.62 m:** the band bottom
must clear ~102 m, and the crown is at 135 m. That leaves 33 m for a band that
must ALSO leave a readable crown beneath 135 m. At the current 32 m thickness the
crown would be **1 m tall** — the layer becomes a cap on the summit, which is
this spec's own named reject ("a band that leaves no crown is just fog").
Irreconcilable, and not narrowly: the highest vantage sits 69 % of the way up
the massif's own relief.

**The finding is that constraint 1 is wrong.** Standing inside mist at
mid-altitude is correct physics, not a defect. In references 02/04/12 the
observer is in clear air because the observer is standing in a **valley**, not
because the layer floats above every standpoint in the world. A layer at a fixed
altitude that no one can ever walk into is a contradiction in terms — and a
player climbing this massif *should* pass up through it and out onto a clear
crown, which is the second thing reference 12 shows.

Restated against the vantages players actually occupy, the constraints converge
easily and **the shipped 70 m already satisfies them**: highest player eye
25.44 m, band bottom 54 m — 28 m of clear air — band top 86 m, crown 86..135 m,
49 m of it, 42 % of the relief.

**So the camera moved, not the world.** The sky probe stood at ground+70 =
84.43 m against a band of 54..86 m: **1.6 m inside the layer**, which is the
entire reason its frames came back milky. It is now pinned to
`MIST_BAND_HEIGHT + THICKNESS/2 + 20` = 106 m, absolute rather than
ground-relative so it cannot drift when those constants move.

Frames: `render-mist-R2-eye-INSIDE-band-84m.png` against
`render-mist-R2-eye-ABOVE-band-106m-*.png`. The second shows both halves of what
was asked in one image — the observer's air is clear, and the massif is cut, its
base buried in the layer and its crown standing dark against the sky.

### Acceptance

`docs/acceptance/render-aerial-{BEFORE,AFTER}-{250,500,900}m-*.png`, and the
recipe is in the acceptance README. The three ranges ARE the control (Rule 30):
a change that makes the picture prettier without making 900 m differ from 250 m
has not done R1, and this instrument reports that as a flat column.


## R3.2 / R3.3 — DIAGNOSED AND MEASURED. **R3.3 IS NOW SHIPPED** (12.08.2026)

**Status: R3.3 is in the build; R3.2 is still only designed.** The diagnosis
below stood up in full when it was finally acted on — the BEFORE arm, reshot on
today's tree, reproduced every number of it — so what follows is kept as
written, with the shipped result appended at the end of the R3.3 section.

### The instrument, and it is the one thing here worth inheriting

Everything below is measured as **the per-row standard deviation of the
CLOUD-ONLY DIFFERENCE image** — the frame with clouds minus the frame with
`DFN_CLOUD=0`, pixel by pixel, at the same vantage in the same run conditions.

Why this shape and not another (Rule 47's structural cure, applied before rather
than after being caught by it): the subject is *how much structure the cloud
layer has at a given elevation*. Every earlier sky instrument in this file tried
to find cloud by being bright, and the horizon's pale sky defeated all of them —
`runs` had to be moved onto a NEUTRALITY mask (b−r ≤ 5) for exactly this reason,
and even that is a colour test. Differencing against the cover-0 arm needs no
mask at all: the sky gradient, the sun, the haze, the mist band and the terrain
are bit-identical in both arms and subtract to **zero**, so what is left in the
difference image IS the cloud and nothing else. Rows are fixed geometry, read in
both arms alike.

**Rule 48 check, done before the measurement and not after:** at zero dose the
difference image is identically zero, so the SD is 0 at every row and the
criterion cannot pass. It is an existence criterion on a quantity that only the
subject can produce — which is the property Rule 48 asks for, stated positively.

Script (scratchpad only, ~40 lines, not landed as a tool — see the deferred
list): decode both PNGs, take `0.30/0.59/0.11` luma, subtract per pixel, report
mean and SD per internal row.

### R3.3 — THE HARD BRIGHT BAND IS THE SHEET'S OWN AREA-MEAN, AND IT IS DRAWN WHERE THE FIELD IS STILL RESOLVED

Measured on `render-sky-R3-structure-CLOUD-{ON,OFF}-4deffcd.png`, all 640
columns, internal rows (horizon sits at row ~206 at this vantage's +0.06 pitch):

| internal rows | what is there | mean of diff | **SD of diff** |
|---|---|---|---|
| 110–133 | resolved sheet (speckle) | 61–80 | **38–51** |
| 144–152 | **THE BAND** | 72–78 | **8.2–13.0** |
| 155–178 | horizon cumulus (R3.1) | 30–47 | **42–49** |
| 180–183 | second flat strip | 58–64 | **1.7–3.3** |
| 193+ | below the sheet fade | 0.00 | 0.00 |

**The band is not dim and it is not a seam in the gradient — it is a bright
strip of cloud tone with the structure taken out of it.** Its mean cloud
contribution (~75 luma) is the HIGHEST anywhere in the frame; its SD is a sixth
of the sheet's just 15 rows above. A constant next to a texture is read as a
join between two pictures, which is precisely the user's report. Note also that
the mean profile alone shows NOTHING: it declines smoothly from 75 to 0 across
the whole region with no step at all. An instrument that averaged rows would
have declared the band absent.

**Cause, located exactly.** `dfn_cloud_alpha` ends with

```
return mix(a, cover, smoothstep(0.20, 0.60, cells_px));
```

i.e. the sheet is replaced by its area average — the constant `cover` — once one
pixel covers 0.6 wavelengths. But `dfn_cloud_field` ALREADY has a per-octave LOD,
and at `cells_px` 0.60 the base octave's own LOD term is still `1 −
smoothstep(0.22, 0.75, 0.60) = 0.21`, i.e. **21 % of the largest octave is still
alive and fully resolvable when the outer convergence has already discarded
everything.** The two convergences are redundant with each other and the outer
one runs far ahead of the inner one. That gap in `cells_px`, projected into the
frame, is the band.

The outer convergence is not gratuitous, and this is why the fix is not "delete
it": when octaves are replaced by their MEAN the sum's spread shrinks, so the
field stops being uniform on [0,1], and thresholding it at `1 − cover` stops
covering `cover`. At full LOD collapse the field is the constant 0.5 and alpha
would be a hard 0 or 1 rather than `cover`. The outer mix is a blunt patch for a
real problem.

**The fix that was designed and NOT written (a successor should start here).**
Renormalise the field by the spread that actually survives the LOD, instead of
throwing the survivors away:

- weights after LOD: `w_i = W_i * l_i` for `W = (0.55, 0.28, 0.17)`;
- the octaves are the same construction at incommensurate frequencies, so treat
  them as uncorrelated with equal marginal variance — **the same assumption the
  shipped `CLOUD_FIELD_MEAN/SD` pair already rests on**;
- then `sd_lod = CLOUD_FIELD_SD * sqrt(Σ w_i²) / sqrt(Σ W_i²)`, with
  `sqrt(Σ W_i²) = 0.6402`;
- and `mean_lod = 0.5 + (CLOUD_FIELD_MEAN − 0.5) * Σ w_i` (the weights sum to
  exactly 1.0 at full resolution, so this is the identity there);
- `z = (raw − mean_lod) / sd_lod`.

Coverage then equals `cover` at **every** LOD by construction, the surviving
large-scale structure keeps its full contrast instead of being flattened, and the
outer convergence can be driven by the residual spread `res = sd_lod /
CLOUD_FIELD_SD` rather than by `cells_px` — so it fires only where the field is
genuinely dead, which is much nearer the horizon and inside the sheet's existing
haze fade. Computed values of `res` against `cells_px`, for whoever tunes the
window: 0.20 → 0.914, 0.30 → 0.809, 0.50 → 0.394, 0.60 → ~0.28, 0.70 → 0.022,
0.75 → 0. Suggested window `res` 0.18 → 0.04, with a floor on `sd_lod` so the
division cannot blow up when every octave is gone.

**Rule 31 applies to this and it must be MEASURED, not assumed.** The
uncorrelated-equal-variance step above is exactly the kind of statistical premise
that cost this file a day the first time. `CloudModel.cpp` already mirrors the
2-D field; the renormalisation belongs there first, with a test asserting that
`P(field > 1 − cover) ≈ cover` at several `cells_per_pixel` values — and with the
CURRENT fixed-SD form kept as the control that FAILS it. Do not ship the shader
half before that test exists; the 3-D field shipped in that order once already
and the owed control corrected its author's own claim when it finally ran.

**A second flat strip exists at rows 180–183 (SD 1.7–3.3, mean 64)** and it is
the same mechanism at the very bottom of the sheet, just below the cumulus base
(`CUMULUS_BASE_Y` = 0.055 lands at row 182 at this vantage — the arithmetic
matches to a row). One fix removes both.

### R3.3 SHIPPED — what was built, what it measured, and what it did not fix

The diagnosis above was acted on unchanged. Three things landed together, and
the second one is not optional: the moment the band gets its structure back the
`SHEET_HAZE_LO/HI` cut becomes the next visible edge, so it was fixed in the
same change rather than after it (deferred item 5, now closed).

**(1) The field is renormalised onto the distribution that survives its LOD.**
`dfn_cloud_field` / `CloudModel::cloud_field` now compute `w_i = W_i·l_i`,
`sd_lod = SD·√(Σwᵢ²)/0.6402` and `mean_lod = 0.5 + (MEAN−0.5)·Σwᵢ`, and remap
through *that* Gaussian's CDF. Both lines are identities at full resolution, so
this generalises the shipped constants instead of adding a second calibration.

**(2) The outer convergence is keyed to the RESIDUAL SPREAD, not to
`cells_px`.** `res = √(Σwᵢ²)/0.6402`, window 0.18 → 0.04, which lands at
`cells_px` 0.59 → 0.68 against the old 0.20 → 0.60.

**(3) `SHEET_HAZE_LO/HI` deleted, replaced by an extinction in the sheet's own
distance:** `exp(−dist / SHEET_EXTINCTION_M)` with `SHEET_EXTINCTION_M` 60 km.
Derived, not picked: a 2600 m deck seen from the valley floor physically ends at
its geometric horizon, √(2·R⊕·h) = 182 km, so a third of that puts the sheet at
1/e by 60 km, at 5 % where it ceases to exist, and at 96 % overhead — a fall
that is continuous from the zenith outward and therefore has no edge to see.
**Explicitly NOT `dfn_aerial_transmittance`, and the arithmetic is the reason:**
`HAZE_SCALE_LENGTH` is 600 m, calibrated so a ridge at 250–900 m reads its
distance; over a 20 km sightline the same law gives optical depth 4.4 and erases
the cumulus bank, and over the 2.6 km straight up it still takes a fifth off the
zenith. The sheet is a SKY element and `dfn_aerial`'s target colour IS the sky
gradient, so aerial perspective applied to the sky is a term applied to itself.

**Rule 31, done BEFORE the shader half, and it passed.** The
uncorrelated-equal-variance premise was measured over 200k samples: predicted
`sd_lod` against the measured SD of the LOD'd sum comes back at ratio
0.9996–1.0003 across 0.0–0.80 cells/px. `CloudModelTests.cpp` asserts it by
inverting the field's own logistic — SD of the recovered z is 1.000 at every
rate — with the shipped-until-now form as the control, whose z collapses *by the
residual* (0.63 / 0.39 / 0.17 at rates 0.40 / 0.50 / 0.60), i.e. the control
does not merely fail, it fails by exactly the amount the diagnosis predicted.

**The control turned out WORSE than the diagnosis claimed.** Measured on the
shipped form at `cells_px` 0.50: a requested cover of 0.15 drew **0.0000** of the
plane and 0.60 drew **1.0000** — both ends of the range collapsed into the two
constants a field can be. That is the bright flat strip, in numbers, before any
pixel is looked at.

**The frame result** (`structure` mode, box `0,100,640,200`, both arms shot from
one binary 30 seconds apart, differing only in `DFN_CLOUD`):

| internal rows | what is there | SD before | SD after | mean before | mean after |
|---|---|---|---|---|---|
| 108–111 | resolved sheet | 49.8 | 48.7 | 81.1 | 74.4 |
| 144–147 | band, upper edge | 15.8 | **38.2** | 76.4 | 53.4 |
| 148–151 | **THE BAND** | **9.4** | **30.5** | **74.5** | **47.8** |
| 152–155 | band, lower edge | 15.5 | 21.1 | 69.6 | 48.8 |
| 180–183 | second flat strip | 7.6 | 6.2 | 62.7 | **21.3** |
| 184–187 | below it | 9.4 | 3.8 | 57.1 | 13.0 |

Two claims, and they are different claims. The band's **structure** is back
(SD ×3.25 where the collapse was deepest), and the band has also stopped being
the **brightest** thing in the frame — its mean drops below the resolved
sheet's, where before it stood above everything. The second was not a separate
fix: in the converged region the shading term `core1` read a field pinned at
0.5, so every band pixel was drawn at full `cloud_bright` with no dark core at
all. Renormalising restores the field's spread and the shading with it.

**HONEST — the second strip at rows 180–183 is NOT structurally fixed.** Its SD
is still 6.2. There the sheet is genuinely dead (`cells_px` ≈ 3.5, every octave
past its own LOD), so a uniform veil is the correct answer and no renormalisation
can produce structure from a field that no longer exists. What changed is its
amplitude: mean 62.7 → 21.3, and 57.1 → 13.0 in the rows below. It reads as a
faint veil rather than as a second picture joined to the first, and that is the
extinction's doing, not the field's.

**The zero-dose arms came back BYTE-IDENTICAL before and after** — `cmp` on the
two `DFN_CLOUD=0` frames. So a single archived control frame serves both arms,
and the change provably touches nothing but cloud.

**Owed / flagged.** `SHEET_EXTINCTION_M` is a shader literal like
`DFN_CLOUD_LAYER1_M`; it has one consumer today, so Rule 35 does not force it
into NUMBERS yet, but it should travel with the deck altitudes when R3.2 moves
those (they have two consumers each — `fs_sky` and `dfn_cloud_sun_vis`).

### R3.2 — THE CEILING IS ONE TONE ON ONE PLANE. What was designed, and why each piece.

Reference 12 carries three claims and we satisfy none of them: at least three
strata, self-shadowed, with holes that show BRIGHTER sky behind. Read against
the shipped `fs_sky`:

1. **The high sheet is invisible as a deck.** It is composited
   `mix(sky, cloud_bright, a2)` — the SAME white the main sheet's lit body uses.
   Two decks of one tone are one deck with extra opacity, whatever their
   parallax.
2. **The main sheet is nearly untoned.** Its only shading is
   `core1 = smoothstep(1 − cover*0.55, 1, F)`, which at the default cover 0.45 is
   `smoothstep(0.7525, 1.0, F)` — while cloud exists wherever `F > 0.55`. So the
   great majority of every mass is flat `cloud_bright` and the darkening only
   ever finds the middle of a blob. It is also non-directional: it cannot move
   with the sun, so it cannot read as self-shadowing.
3. **Nothing is in front.** Both planes are at 2600/4400 m — a ratio of 1.7 in
   distance, but with the same tone and the same construction that reads as one
   ceiling.

The intended change, in the order of decreasing value:

- **(a) Directional self-shadowing on the main deck.** One extra field tap, at
  `p + normalize(u_sunDir.xz) * step`: where the field is HIGHER toward the sun,
  this point stands behind a thicker mass and darkens; where it is lower, the
  point is on a sun-facing slope and lightens. This is the single biggest
  flat→modelled move available and it costs one sample. It also moves with the
  hour for free, which the density-only term never could.
- **(b) A third, LOW deck at 1500 m — sparse, ragged and DARK — drawn LAST.**
  The altitude is derived, not chosen: the same field wavelength at 1500 m
  against 2600 m subtends **1.73×** the angle, and it is the difference of
  apparent cell size that reads as "nearer". Below ~1000 m one cell is wider
  than half the frame; by 2000 m the ratio is down to 1.3 and the deck merges
  into the middle one. Real broken stratocumulus bases sit at 600–2000 m.
- **(c) Make the high deck the BRIGHTEST of the three.** Then a hole in the dark
  low deck shows the bright high deck behind it, which is R3's wording made
  literal rather than approximated.
- Cover for the low deck must be a FRACTION of the total (0.45 was the derived
  value): three independent decks at full cover close `1 − (1−c)³` of the sky =
  83 % at c = 0.45, and the holes the whole exercise exists for would be gone.
- `dfn_cloud_sun_vis` must gain the third deck too. It is the file's own
  standing rule — one authority for the field — and a deck that draws in the sky
  but does not occlude the sun is the "shadow crossing land with no cloud above
  it" reject W4 was built to prevent.

**The measurement that was designed for it, since "the sky looks deeper" is not
a criterion.** Per-deck arms, each shot with exactly one deck disabled in the
shader (a 3-second rebuild — timed), all read against the full arm:

- a deck is PRESENT iff the pixels that change when it alone is switched off are
  a substantial set — the subject is located by an ARM DIFFERENCE, never by
  brightness, so Rule 47 has nothing to bite;
- the three sets must be largely DISJOINT, which is what "three strata" means
  operationally;
- "holes show brighter sky behind": inside the sky box, compare the luma of
  pixels the FRONT deck owns against the luma of pixels it does not — with
  ownership decided by the arm difference and only the VALUE read from the
  frame. The claim passes only if the not-owned set is brighter.
- self-shadowing: the SD of luma WITHIN the front deck's own (arm-located)
  pixels, before against after. Flat white gives near zero by construction.

### The NUMBERS rows that were drafted and reverted with the code

`CLOUD_DECK_LOW_M` 1500, `CLOUD_DECK_MID_M` 2600, `CLOUD_DECK_HIGH_M` 4400,
`CLOUD_DECK_LOW_COVER` 0.45, with the derivations above, replacing the shader
literals `DFN_CLOUD_LAYER1_M`/`2_M`. They belong in NUMBERS rather than in
`dfn_env.sh` for the Rule 35 reason the haze rows already travel that way: each
altitude has TWO consumers — `fs_sky` intersects the view ray with the plane and
`dfn_cloud_sun_vis` projects along the sun to the same plane — and if those ever
read different numbers the shadow slides out from under the cloud casting it.
The route is the established one: NUMBERS row → generated header →
`apply_environment` slot 38 (env block 38 → 39) → `u_cloudDeck*` accessors. The
draft was written and reverted intact; re-doing it is mechanical.

## R3.2 SHIPPED — THREE DECKS, SELF-SHADOWED, WITH HOLES THAT SHOW BRIGHTER SKY

Built to the design above, with two changes forced by measurement (below). What
the reference asks for and what the frame now says, claim by claim.

### The three decks are PRESENT and largely DISJOINT

Located by ARM DIFFERENCE — each arm is the same frame with exactly one deck
dropped by a `#define`, so the subject is found by what changes and never by
being bright (Rule 47's cure by construction). Sky box `0,0,640,180`:

| deck | owns | mean \|dL\| over the box |
|---|---|---|
| low, 1500 m | 36.4 % | 13.76 |
| mid, 2600 m | 56.4 % | 23.51 |
| high, 4400 m | 45.2 % | 9.01 |

Overlap / union: low∩mid **30.4 %**, low∩high **27.6 %**, mid∩high **43.6 %**.

**HONEST, and it is the one claim that does not fully pass.** For independent
sets at these coverages, chance overlap is 28.0 % (low∩mid) and 33.0 %
(mid∩high). The low deck is therefore genuinely independent of both — measured
overlap sits *at* chance — but the two FAR decks are correlated well above it.
That is not new and not the low deck's doing: the mid and high decks read one
field with only a scale and a seed between them, and both are modulated by the
same extinction and the same cover, which correlates their envelopes near the
horizon. Three strata by the operational definition is satisfied where it was
asked for (a near deck against a far one); "three mutually independent decks"
is not, and the honest fix would be a second seed axis on the high deck rather
than anything in this change.

### Holes DO show brighter sky behind

Ownership from the arm, VALUE from the frame. Pixels the front (low) deck owns
read **146.28** luma; pixels it does not read **180.90** — the not-owned set is
brighter by **34.62 luma = 1.74 palette shade steps**. The claim passes, and it
passes by more than the one step below which the palette turns a difference into
dither.

### The decks are SELF-SHADOWED, and the criterion is the difference image

`SD of luma within the deck's own pixels` was the designed criterion and it
turned out to measure the wrong thing: the owned set mixes opaque deck with soft
edges, so its SD reads deck-against-sky contrast more than within-deck
modelling, and it fell 3–4 % on a change that plainly added shading. Replaced by
the R3.3 lesson applied again — **the SHADE-ONLY DIFFERENCE image** (shading on
minus shading off), which is identically zero at zero dose and needs no mask:

- mean signed dL **−0.06** — the term preserves the deck's mean brightness by
  construction, so it adds modelling rather than just darkening;
- **SD 12.99 luma = 0.65 palette steps**, and a UNIFORM darkening scores SD 0
  here by construction — that is the discriminator;
- 23.0 % of the box darkened, 24.2 % lightened. Both signs at comparable counts
  is what "the light comes from a direction" means operationally;
- p90 |dL| **20.89 = 1.04 steps**, max 89 luma = 4.4 steps.

### The ceiling's flattest place

The R3.3 instrument over the whole sky box, against the shared cloud-off arm:
the **flattest row still carrying cloud** goes from SD **15.93** (row 154) to
**25.05** (row 164). There is no longer a row anywhere in the sky where the
cloud layer is one tone.

### Two mistakes of mine that measurement caught, both recorded in the shader

**The zero-dose arm was not zero dose.** Setting `DFN_CLOUD_SHADE_GAIN` to 0
does not remove the directional term — it pins it at 0.5 and paints every deck a
flat mid-grey, which is a different strong effect. Against that fake control the
shading measured as doing nothing at all (low deck SD 26.15 → 26.18). A dose of
zero has to mean FULLY LIT, and it is now a separate `DFN_CLOUD_SHADE_OFF`.

**`max(density, direction)` deleted the thing it was combining.** It floored the
whole deck at half-shaded (mid deck mean 227.6 → 207.8 luma, a uniform
darkening) and discarded the directional signal everywhere the density term was
the larger — so the mid deck's body LOST variation, SD 23.87 → 22.54, on a
change whose entire purpose was more. Signed and added instead.

### What did not change, and the proof

The `DFN_CLOUD=0` frame is **byte-identical to the one archived for R3.3**
(`cmp`). Three decks, a new shading term and a third occluder in
`dfn_cloud_sun_vis`, and the cloudless world is bit-for-bit the same picture.

### Owed

- `DFN_CLOUD_DECK_{LOW,MID,HIGH}_M` and `DFN_CLOUD_DECK_LOW_COVER` are shader
  `#define`s. Both of their consumers (`fs_sky` and `dfn_cloud_sun_vis`) live in
  this one file, so there is exactly one definition and Rule 35 is satisfied
  today; they become NUMBERS rows the moment anything outside the shaders reads
  an altitude. `SHEET_EXTINCTION_M` travels with them. **Proposed to the lead,
  not written by me.**
- `CloudModel.cpp` has no CPU mirror of `dfn_cloud_self_shade`. It needs none to
  be correct (it is a difference of two `cloud_field` calls, both mirrored) but
  the deck ALTITUDES are not asserted anywhere, and the 1.73× ratio that the
  whole design rests on is arithmetic no test currently reads.
- The mid∩high correlation above.


## THE WHITE TREELINE — DIAGNOSED, AND IT IS NOT HAZE. IT IS OURS.

The complaint: the near canopy is dark green and the far one is white as
hoarfrost, from about 400 m out. Flora eliminated two of the three candidate
causes from their own files (the placeholder is painted the same dark green;
texels outside the leaf contour are black, so averaging can only DARKEN). The
third was handed to render as "light or haze lands on the foliage program
differently at distance". It is neither light nor haze.

**Reproduced on today's build** at `DFN_FLORA_PROBE=2` (the treeline vantage:
open ground, forest edge at reading distance), not argued from the archived
frames.

**Haze FAILS ITS OWN ZERO-DOSE CONTROL (Rule 48).** `DFN_HAZE=100000000` — air
so thin that `dfn_aerial` is the identity everywhere — renders a treeline that
is *visually indistinguishable* from the shipped one. A lever that changes
nothing at zero dose is not the lever. That closes the question the way it
should be closed, in one run, and it also retires the reasoning that pointed
here in the first place ("haze goes toward the SKY colour, not white" was a
correct objection to a cause that was not operating anyway).

**The cause is the ALPHA-TO-COVERAGE path — our own running-shimmer fix.**
`DFN_MSAA=0` is documented in `BgfxRendererResources.cpp` as a bit-exact control
arm: it takes the foliage back to the hard alpha cutout. On that arm the same
treeline, same second, same everything else, is a **solid dark-green forest**.
The pair at 8x — `render-treeline-ZOOM8-{coverage,cutout}-cf6f4ae.png` — is not
a subtle comparison: one is a canopy, the other is the same canopy filled with a
fine grey-white pepper.

**Why the change's own acceptance passed and still missed this, which is the
part worth keeping.** The coverage fix measured "the canopy did not thin: mean
luma +1.8 %", and that measurement was honest and is still true — over the tight
canopy box the two arms read **77.18** and **76.33** luma, 1.1 % apart. The
defect is not in the mean, it is in the DISTRIBUTION, and preserving a mean is
not preserving a picture:

- 30 % of pixels fully dark-green over sky, and every pixel a 30 % blend of
  dark-green with sky, have the SAME mean and are different pictures. The second
  is a pale wash, and against a bright sky a pale wash is frost.
- Measured: pixels in the MIDDLE band (80..150 luma — neither leaf nor sky)
  **6.1 % -> 13.1 %**, a factor of 2.16.
- And at MATCHED LUMA (so nothing is located by the property under test) a
  blue-shifted population appears that the cutout arm does not have: in the
  96..128 band, **31 px -> 172 px**, with b-r **-1.32 -> +20.42**, and in the
  64..96 band greenness falls **18.80 -> 11.76**. Those are the frost pixels:
  leaf blended with sky, at leaf-ish luma and sky-ish hue.

**This is a TRADE, not a bug to revert, and the choice is the lead's.** The
coverage path is the fix for the user's oldest complaint («при беге трясет»,
«всё дергает и перерисовывается очень рябью») and it bought 0.094 % -> 0.004 %
of screen flipping per running stride. Reverting it re-opens that. What is now
established is that the two are the SAME knob, that the treeline's colour is
what it cost, and that the cost was invisible to the acceptance that shipped it.

**And the reason it matters more than it looks.** The user called the treeline
«частокол одинаковых деревьев». Half of that report may not be sameness at all
but this whiteness — variety is being fixed by flora, and the whiteness would
have remained.

Frames: `docs/acceptance/render-treeline-*-cf6f4ae.png` (shipped, the two
controls, and the 8x pair), recipe in the acceptance README.


## TWO MOONS (W9) — THE ORBITAL HALF IS SHIPPED, THE SECOND MOON CANNOT BE DRAWN YET

**What landed** (`SkyModel.h/.cpp`, 6 tests): `MoonElements`, `masser()`,
`secunda()`, `moon_state_at(elements, day_fraction, elapsed_days)` — elongation
from the WORLD CLOCK through each moon's own synodic period, inclination about a
retrograde node line, the equation of centre, and the apparent radius that swings
with it. Every constant is an existing NUMBERS row; the W9 block had had no
consumer for two days.

**The one thing that had to be derived rather than copied: the epoch's sign.**
With `+ epoch` Masser sits 162 deg from the meridian in the first frame — below
the horizon — which is the exact defect `MASSER_ELONGATION_EPOCH` was written to
fix. With `- epoch`, five of design's stated numbers come out at once (lit
fractions 0.500 / 0.750 exactly, flat-arc hour angles +18 / +48, 30 deg apart),
and it is the only sign under which moonrise is DELAYED at the row's 53.33
in-game min/day.

**Two of design's five numbers move on the SHIPPED arc, and no row needs
changing.** Hour angles read +20.25 / +46.65 and the separation 25.54 deg rather
than 30, because `SKY_ARC_TILT` 0.45 is not in design's flat derivation. Both
moons stand at 65 and 41 deg elevation, 2.3x the separation floor. The row's
claim holds; its two quoted angles are a flat-arc idealisation and should be
read as such.

**BLOCKED, and the block is a contract, not a difficulty.** `RenderEnvironment`
carries ONE moon (`moon_direction` / `moon_color` / `moon_phase` /
`moon_light`). The second needs its own set plus the angular radius and disc
luma, and `IRenderer.h` is frozen (Rule 26). **Requested of the lead**, with the
exact shape:

- `moon2_direction`, `moon2_color`, `moon2_phase`, `moon2_light`, and for BOTH
  moons `moonN_angular_radius` and `moonN_disc_luma` — the last two because
  `fs_sky.sc`'s `MOON_COS_INNER/OUTER` are hardcoded 2.0/2.45 deg half-angles
  and the whole point of `MASSER_ANGULAR_DIAMETER` is that the two discs differ
  by 2.15x. Two more `u_envParams` slots (block 38 -> 40).
- `apply_sky_time` needs `elapsed_days` as well as `day_fraction`; a defaulted
  parameter keeps the app compiling, but the app must eventually pass the real
  world clock or both moons stand still.

**Then the shader half is mechanical**: `dfn_moon()` in `fs_sky.sc` already does
the phase terminator and the limb; it needs to be called twice with per-moon
radius and luma, plus `MOON_LIMB_OUTLINE_LUMA`'s 1 px ring (the row exists
because a daytime moon's luma inevitably CROSSES the sky's, and at that hour the
separation rule is unsatisfiable by construction — the outline holds the day,
the disc holds the night) and `MOON_SOLAR_EXCLUSION` (already computed as
`MoonState::observable`).


## DEFERRED — found on the way, NOT fixed, deliberately left

Recorded per the user's instruction to write new bugs down and close our eyes on
them rather than forget them. None of these was touched.

1. ~~**R3.3 — the hard bright band at the horizon.**~~ **CLOSED 12.08.2026** —
   shipped, see the R3.3 section above for what it measured.
2. ~~**R3.2 — the sky is one flat speckled ceiling.**~~ **CLOSED 12.08.2026** —
   three decks, self-shadowed, holes showing brighter sky. See the R3.2 section
   above, including the two claims it does NOT make.
3. **The second flat strip at rows 180–183** — PARTLY closed by R3.3, and R3.2
   moved it further: the flattest row anywhere in the sky now measures SD 25.05
   against 15.93. Its amplitude had already fallen by two thirds under R3.3
   (mean 62.7 → 21.3) while its own SD did not move (7.6 → 6.2), and at
   `cells_px` ≈ 3.5 it cannot — there is no field left there to have structure.
4. ~~**The cloud-structure instrument lives only in the session scratchpad.**~~
   **CLOSED 12.08.2026** — landed as `tools/measure_aerial.py structure
   <ON> <OFF> <box> [group_rows]`, next to `profile` and `runs`. It prints the
   flattest row still carrying cloud, and it announces its own zero-dose case.
5. ~~**`SHEET_HAZE_LO/HI` (0.004..0.030) is a hard cut, not a fade.**~~
   **CLOSED 12.08.2026** — replaced by an exponential extinction in the sheet's
   own distance, in the same change as R3.3 exactly as this entry demanded.
6. **THE SECOND MOON is computed and cannot be DRAWN** — the orbital half and
   its tests are shipped; the contract change is requested and listed above.
   Nothing is half-written in the tree: `apply_sky_time` is untouched and still
   writes the one moon it always did, so this is dormant code with tests, not a
   half-finished feature.
7. **THE WHITE TREELINE is diagnosed and NOT fixed** — see the section above.
   It is the alpha-to-coverage path, i.e. a trade against the running-shimmer
   fix, and which way to take it is the lead's call, not a defect to silently
   revert.
8. **The mid and high decks are correlated above chance** (43.6 % overlap
   against 33.0 %): they read one field with only a scale and a seed between
   them. The low deck is independent of both, so R3.2's claim holds where it
   was made, but "three mutually independent strata" does not.
9. **`dfn_cloud_field3` still has no LOD term at all.** It was written for a
   fixed ring where that was defensible; if the cumulus band ever moves off the
   ring it will alias immediately.
10. **Other zones' working tree was dirty throughout this session** (core's
   worldgen split, new `WorldgenOutcrop`/`WorldgenRelief`/`GroundReliefTests`).
   Nothing of mine touched it and no test of mine depends on it; noted only so
   the next agent does not read a red core test as render's.

## R6a — WARM KEY / COOL SHADE: MEASURED ON HIS FRAMES, AND REFUSED

Full recipe, boxes and table: `docs/acceptance/render-R6-warm-cool-split.md`.
Instrument: `tools/measure_light_split.py`. **No code was changed.**

### The finding, in one line

**We already have the warm/cool split, it is the largest in the comparison, and
the reference frames do not have one at all.**

| | luma range | WARM_SPLIT |
|---|---|---|
| OURS, obelisk cast shadow vs sunlit grass | 1.79x | **+14.02** |
| ref 14, Whiterun cobbles, cast shadow vs sun 1 m away | 1.90x | **+0.32** |
| ref 03 forest floor / ref 01 plateau / ref 10 plaza | 1.7-4.1x | -8.55 / +0.52 / -0.10 |

`WARM_SPLIT` is the yellow-blue chromaticity, in percent of luma, of the
per-channel RATIO between a lit and a shaded patch of ONE material — the hue of
the light the key adds, over the light already there. Zero means the shadow is
the sunlight's colour and merely darker. A chromaticity DIFFERENCE will not do:
grass albedo under one white light gives a +27 "split" out of a colourless
scene, so a difference criterion passes at zero dose (Rule 48).

Ref 14's three channels come back **1.923 / 1.889 / 1.900** — equal to within a
percent. What separates Skyrim's shadow from its sunlight is depth and edge, not
colour.

### Why this reading is trustworthy

- **Calibrated against a known answer.** Our renderer has no tonemap and no
  gamma anywhere, so the 8-bit frame IS the light arithmetic.
  `LOOKDEV_SUN_COLOR` 1.00/0.96/0.88 and `LOOKDEV_AMBIENT_COLOR` 0.34/0.36/0.40
  at NdotL 0.62 predict **+14.01**; the instrument reads **+14.02** off the
  frame. Two decimals on real pixels.
- **The reference bias is bounded, not ignored.** Reference frames are
  tonemapped and sRGB-encoded and ours is neither. `selftest` pushes a
  zero-dose scene through Reinhard + sRGB and gets +6.19 / -1.12 depending on
  albedo. Every reference number sits inside that band — all consistent with
  exactly zero. Ours does not: +14.02 is more than twice it.
- **Two independent modes agree** (`pair` across a cast shadow edge, `scan` over
  coarse blocks), on four frames and five boxes.

### The instrument's first version was wrong, and it is Rule 47's sixth instance here

Version one binned a box's pixels into luma DECILES and compared the ends. On
ref 14's cobbles it reported **-36.5** — a large cool-KEY split, the opposite of
this section and of R6. All of it was the material: within one cobble texture
the dark pixels are mortar and crevice and the bright ones are stone tops, so
the two arms were **different albedos** and the delta was a texture statistic
wearing the light's clothes. The same cobbles across the frame's own cast shadow
edge gave +0.32.

It survived the Rule 48 zero-dose control because that control was built on ONE
albedo and the defect needs two. Both facts are written into the tool. The cure
was Rule 47's own: hold everything that is not the subject identical between the
arms — here the material — which on a photographed frame means a cast shadow
edge, or blocks coarse enough to average the texture away. `pair` and `scan`
also REFUSE any pair whose luma range is under 1.5x, because two boxes that are
both in sun cannot fail (Rule 27); the first pair tried on our own frame came
back at 0.99x and would have read +1.20.

### Consequence, and where the frame is actually lost

Warming the key and cooling the fill moves `WARM_SPLIT` further above +14, i.e.
**further from every reference frame in the set**. Refused under Rule 45's
stopping condition: refuse the quantity and write nothing rather than fit a
number through it.

Two things the same table says DO differ, neither of them this claim:

- **Our ground is far more chromatic than any reference ground** — box warmth
  46.8/51.8 against 24.4-38.5 across the whole reference set. R5's axis, not
  R6's; belongs to whoever owns the ground material.
- **Ref 03's forest floor carries a 2.28x luma range at 24 px blocks from
  DAPPLE ALONE.** Ours is 1.66x and all of it is one obelisk shadow across an
  otherwise evenly lit field. That is R6's second half, and it is the half that
  is missing.

## R6b — THE DAPPLE. DIAGNOSED AND SIZED, NOT FIXED.

Instrument: `tools/measure_dapple.py`. **Diagnostic, not acceptance** — no code
was changed and there is no before/after pair yet.

### The quantity, and why the obvious one is wrong

LOCAL LIGHT/DARK: inside a window of 4x4 blocks, the brightest quarter of the
blocks over the darkest quarter, averaged over windows.

The obvious quantity — luma range over a ground box — was tried first and is
**wrong**, in the same family as everything else recorded on this page. Our
forest floor scores **1.81x** on it against reference 03's 2.28x, a 79 % "pass"
for a picture with no dapple anywhere in it: the range is a smooth gradient from
the near ground to the treeline, aerial perspective plus slope shading. **A
criterion a gradient satisfies is not measuring dapple.** A window removes it —
a smooth trend is nearly constant across one window and divides out. `selftest`
puts a 4x gradient at **1.144x** and a 4x interleaved dapple at **4.000x**, on
synthetic pixels, which is the null this number is read against.

### The gap

Block sizes are fractions of the box width, so a 1200 px reference and our
640 px frame are read at the same fraction of the surface.

| surface | box_w/80 | box_w/40 | box_w/27 |
|---|---|---|---|
| ref 14 Whiterun cobbles | 1.974x | 1.682x | 1.549x |
| **ref 03 forest floor — the target** | **1.780x** | **1.646x** | **1.591x** |
| ref 01 plateau ground | 1.706x | 1.495x | 1.488x |
| *(synthetic pure-gradient null)* | *1.144x* | *1.144x* | *1.144x* |
| **ours, near forest floor** | **1.275x** | **1.310x** | **1.410x** |
| **ours, under the great oak** | **1.238x** | **1.350x** | **1.439x** |
| ours, open grass + the obelisk's shadow | 1.206x | 1.296x | 1.331x |

Measured above the null, reference 03 is **0.64** and ours is **0.13** — about
**five times**. And the row that settles what kind of defect this is: **our
forest floor scores the same as our open grass with one obelisk on it.** Standing
under a canopy currently adds nothing to the ground.

Frames: ref03 `image copy 12.png` box `540,430,1190,600`; ref14
`image copy 9.png` box `560,330,1010,570`; ref01 `image copy 10.png` box
`700,400,1180,600`; ours `render-coverage-aa-near-AFTER-c15d930.png` box
`0,292,640,357`, `flora-great-oak-under-669f1a7b.png` box `0,570,1280,715`,
`render-mist-R2-360m-ON-3d37ef3+r2.png` box `0,240,400,358`.

### Shadows are NOT missing — looked at, not assumed

The great oak's canopy shadow is plainly on the ground in
`flora-great-oak-under-669f1a7b.png`, and the obelisk's is in the mist frame.
The caster path is intact too: `foliage` is in `CUTOUT_PROGRAMS`, so leaf cards
punch their mask through the depth map via `shadow_cutout`, and `DrawParams::fade`
defaults to 1 so the `SHADOW_CASTER_MIN_FADE` gate passes. **What is missing is
not the shadow, it is its GRAIN.**

### Three candidate mechanisms, ranked, none of them measured yet

1. **The shadow map cannot represent dapple, arithmetically.** `SHADOW_TEXEL_M`
   = 2 x 320 / 4096 = **0.156 m**, and this file's own thin-caster rule
   (`BgfxRendererImpl.h:96`) is that a caster needs **>= 2 texels ~ 0.31 m** or
   it flickers out. Reference 03's dapple patches are of that order on the
   ground, so we sit exactly on the floor: the coarsest dapple can exist and
   nothing finer can. `SHADOW_NORMAL_OFFSET_M` is a further 0.156 m of receiver
   push-off, which erodes thin shadows from both sides. The remedy is already
   named in that file — **a near cascade**; at 40 m half extent the same 4096
   map gives 0.0195 m per texel, 16x finer, and that is a feature, not a
   constant.
2. **The canopy may have too few holes to cast a dapple through.** Leaf cards
   with large solid mask regions cast large blobs. Not checked; check the leaf
   mask's hole size against `SHADOW_TEXEL_M` before touching anything else.
3. **One hard tap, no PCF** (user decision в1). R6's own wording is "shadows are
   soft AND stable" and reference 03's dapple is soft-edged. Softness at the
   dapple's scale is a different question from the hard pixel edges the user
   asked for, and conflating them would be Rule 43.

### THE CONTROLLED ARM — built, shot, and it settles which mechanism

`DFN_SUN_SHADOW` (dose, default 1; `dfn_shadow.sh` does `mix(1.0, s, dose)` so
dose 1 is bit-identical to the flag it replaced). At dose 0 the casters STILL
draw into the map and only the sampling stops, so the two arms differ in the
shadow and in nothing else.

Recipe — ONE binary, both arms, the sun shadow the only variable:

```
DFN_TOUR=1 DFN_TOUR_DIR=<dir> DFN_INTERNAL_RES=640x360 DFN_PALETTE=0 \
DFN_FLORA_PROBE=1 DFN_FLORA_PITCH=-0.30 DFN_WIND_FREEZE=3.0 DFN_CLOUD=0 \
DFN_SUN_SHADOW=1|0  build_render/engine/app/dfn_app
then tools/archive_frame.py <shot> <out> 640   (4x4 box average, README rule)
```

Frames: `docs/acceptance/render-R6b-dapple-SHADOW-{ON,OFF}-b7bc7fe+ss.png`.
Ground box `0,140,640,355`.

| block px | shadow ON | shadow OFF | **what the shadow ADDS** |
|---|---|---|---|
| 8 | 1.269x | 1.235x | **+0.034** |
| 16 | 1.386x | 1.311x | **+0.075** |
| 24 | 1.418x | 1.312x | **+0.106** |
| 40 | 1.694x | 1.292x | **+0.402** |

**The whole sun shadow system adds 0.03-0.11 of local contrast at the scales
where dapple lives, and only becomes substantial at 40 px — the size of a whole
canopy blob.** Reference 03 sits about 0.5 above the null at those same fine
scales.

And the SHAPE is the finding, not the size. Our shadow's contribution RISES with
block size (0.034 -> 0.402): it is entirely low-frequency. Reference 03's dapple
FALLS with block size (1.780 -> 1.591): it lives at the fine end. **The two
spectra point in opposite directions**, which is what tells a canopy-sized blob
apart from a dapple and rules out "we just need more shadow". Suspect 1 —
grain, not presence — is the one standing.

Second thing the control says, and it was not expected: at 8 px our ON and OFF
arms are 1.269 vs 1.235. **Nearly all the fine local contrast on our forest
floor is the ground MATERIAL, not the shadow at all.**

## R6b, SECOND PASS — THE NEAR CASCADE IS BUILT, AND THE GATE HAS MOVED

Full recipe, the three arms, both tables and the cost:
`docs/acceptance/render-R6b-near-cascade.md`. Frames
`docs/acceptance/render-R6b-cascade-{NULL,FAR,NEAR}-d9aeb0e+nc.png`.

**Suspect 1 was right about the arithmetic and it is now fixed.** The sharp form
of the finding is not "we sit on the 0.31 m floor" but: **the shadow map
undersamples the leaf mask by 2-3x.** The mask's own texel is 0.047-0.086 m
(64 px tile on a 3.0-5.5 m card) against `SHADOW_TEXEL_M` 0.156 m, so nothing
the mask draws at its own resolution can reach the ground; what casts is only
what flora authored above render's floor on purpose (0.4-0.9 m rim bites, 1 m
interior gaps). The near cascade — 4096 over 40 m = 0.0195 m, floor 0.039 m —
puts the mask from undersampled to oversampled.

**And it is off by default, because it buys +0.010 for +22 % of the frame.**
Cascade's own contribution to local contrast, three arms out of ONE binary,
n=3: **+0.010 / +0.000 / +0.010 / +0.046** at 8/16/24/40 px, against a run-to-run
spread of 0.003-0.016 — two of the four are not distinguishable from zero, and
the cascade's own spectrum RISES with block size, which is the shape it was
built to invert. Cost: the vantage held the 120 Hz cap in 3/7 runs with it on
against 6/7 with it off. `DFN_SHADOW_NEAR=1` turns it on; at the default the
near target is 4x4, its view is never touched, and the frame is the shipped one.

**WHY, and this is the result rather than the feature. There is no sun on our
forest floor to interrupt.** Threshold-free, luma p90/p10 over the near band:
reference 03's floor **3.23x** (36 -> 116, light and shade interleaved
throughout); ours with the shadow ON **1.15x** (27.5 -> 31.7, a flat DARK
sheet); ours with it OFF **1.15x** (49.6 -> 57.3, a flat BRIGHT sheet). Our
canopy shadow is binary and total — an evenly lit floor becomes an evenly dark
one with nothing between. 97.3 % of that band is above luma 45 unshadowed and
4.6 % shadowed. **No shadow-map resolution can invent a middle tone that has no
source**; the cascade moved the sunlit fraction 4.6 % -> 5.2 %, which is the
arithmetic working exactly as predicted and is still not a dapple.

So R6b's remaining half is NOT render's. It is (a) how much sun the canopy lets
through — flora's card density and stand spacing, and `FloraCards.cpp` sized
its mask features against "render's ~0.31 m mask-feature floor", a premise this
change makes stale — and (b) the floor material's own tone, flat at 1.15x.

**RULE 47 ALMOST TOOK A SEVENTH SCALP HERE.** The first reading of this change
was a before/after across two binaries an hour apart and it looked like the
break in the spectrum: 8 px 1.259 -> 1.441. All of it was someone else's canopy
work landing in the same rebuild — the `DFN_SHADOW_NEAR=0` arm out of the SAME
binary already scored 1.417. The eye's own resolved height moved 17.42 ->
17.54 -> 16.23 m across three rebuilds in one afternoon. **In this tree a
before/after across binaries measures the week, not the change.** The control
has to vary inside one binary, which is the only thing `DFN_SHADOW_NEAR` is for.

## What this zone does NOT do

- Does not edit `IRenderer.h` or any frozen contract — change requests go to
  the lead for a group sync (Rule 26).
- Does not generate or own world data: no heightmap generation, no chunk
  streaming policy, no world file IO (core's world zone). Render only
  consumes `HeightFieldView`s handed to it.
- Does not decide WHERE it is dark. Darkness has two halves and render owns
  neither: the GEOMETRIC half is core's per-vertex sky visibility (now an
  additive `sky_visibility` span on `VoxelMeshView`; an empty span means
  render's 255 fallback, so it can be filled whenever) and the AUTHORED half
  is `ambient_darkness`, written by the app from the LANDSCAPE §5.9 rule
  (enclosed AND >= DARKNESS_DEPTH_MIN from an entrance, ramped over
  DARKNESS_FALLOFF). Render applies them — ambient *= (1-d), carried-light
  reach *= (1 - 0.55d) — and derives neither. Same standing ruling as the
  material bands: a render-side approximation of a world fact is how the
  invented 60 m brown wash happened.
- Does not own shared components — proposals go to the lead
  (`engine/core/components`).
- Does not simulate: no camera physics, no collision, no controller logic
  (sim). The camera never integrates motion, it only blends sim snapshots;
  gameplay code never calls `IRenderer` directly.
- Does not do UI/editor drawing (Dear ImGui is `engine/editor`'s documented
  exception) and does not own the app loop (`engine/app`, lead).
- Does not leak third-party types: bgfx/GLFW appear only under
  `engine/platform/*/sources/`; no other layer sees them (Rule 1).
- Does not hardcode constants that belong in NUMBERS.md (Rule 14) and does
  not put user-facing strings in C++ (Rule 5 — window titles are localized by
  the caller).
- Does not install anything; all dependencies are pinned FetchContent
  (Rule 24).
