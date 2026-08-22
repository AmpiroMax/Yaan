/*
Created: 09:08:2026 - 10:52:00
Last updated: 23:08:2026 - 02:45:45
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/shaders/dfn_env.sh

Responsibility:
- Shared shader include: the frame environment uniform block (RenderEnvironment
  -> u_envParams vec4 array) with named accessor macros. Single source of the
  index layout; BgfxRenderer.cpp packs the array in exactly this order.

Key items:
- u_envParams[33]; accessor #defines (sun, ambient, fog, sky, splat, water,
  moon, stars, point light, haze) + dfn_surface_light() /
  dfn_sky_gradient() / dfn_aerial() (aerial perspective, R1).

Dependencies:
- Uses: nothing (included after bgfx_shader.sh).
- Used by: fs_terrain, fs_water, fs_sky, vs shaders needing env values.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Index layout is a contract with BgfxRenderer.cpp::apply_environment — change
  both together or not at all.
- 15:08:2026 - 15:18:00: dfn_wind_offset: три полосы движения выведены ИЗ ВЕСА
  качания, а не из нового вершинного канала — вес уже означает «насколько эта
  вершина далеко от того, что её держит», поэтому частота растёт с весом, а
  флаттер гейтится им квадратично: ствол качается медленно и широко, лист
  трепещет. Запрошено зоной flora после разделения материала (см. fs_foliage).
*/
/*
UPD:
- 09:08:2026 - 10:52:00: Stage 3 — initial environment uniform layout.
- 09:08:2026 - 19:20:00: Day/night (в1/в2): moon + stars + carried point light
  slots (11..14) and the shared dfn_surface_light() used by terrain and props,
  so sun, moon, torch and the sky-visibility ambient are computed in ONE place.
- 09:08:2026 - 19:32:00: Light ARRAY (up to 8) replaces the single point
  light, plus authored u_ambientDarkness; env block 15 -> 32 vec4s.
- 09:08:2026 - 19:58:00: Wind slots [32] + shared dfn_wind_offset (foliage now,
  grass and cloth later); env block 32 -> 33 vec4s.
- 09:08:2026 - 20:31:00: Carried lights now SHADOW: dfn_pointshadow.sh included
  here (the light loop is here, so a torch cannot shadow on terrain and not on
  props), shadow-casting lights are packed first so the slot index is the cube
  index.
- 09:08:2026 - 21:06:00: dfn_screen_door + dfn_bayer4 (DrawParams::fade as an
  ordered dissolve, the LOD cross-fade's mechanism). The pixel coordinate is a
  PARAMETER: this header is included by vertex shaders, where gl_FragCoord does
  not exist, and naming it in the body broke every one of them.
- 10:08:2026 - 02:59:00: CLOUDS (WEATHER.md W4): env block 33 -> 35 vec4s
  (slots 33/34 = the weather/cloud state + drift offset), the ONE cloud
  coverage field (dfn_cloud_field / dfn_cloud_sheet_alpha / _sheet2_alpha)
  sampled by BOTH the sky sheet (fs_sky) and the ground shadow
  (dfn_cloud_sun_vis, applied to the sun term inside dfn_surface_light so
  terrain, props and foliage all darken together and cannot disagree).
- 10:08:2026 - 10:45:06: CLOUD FIELD FIXED, after reproducing it numerically (Rule 30b).
  (1) Rule 31: the octave sum is GAUSSIAN — measured over 400k samples it held
  98% of its mass in 0.200..0.797 of the [0,1] it declared, so cover 0.10 drew
  NOTHING and the default 0.45 drew 0.19. Remapped through its own CDF, so
  cover now means coverage within 0.024 across the whole range, both ends
  asserted. (2) Per-octave LOD on an ANISOTROPIC cells-per-pixel metric (the
  radial axis runs 20x the tangential one near the horizon — the streaks), and
  convergence to the area mean past the resolution limit, which replaced the
  distance/elevation fades that had been deleting 22% of the sky. (3) Rule 33:
  layers 1200/2200 -> 2600/4400 m, because at 1200 m the whole sky above 45 deg
  saw 10.8 cells of field AREA and mid-sky had nothing to draw at any
  threshold. The wavelength could not move — it is shared with the ground.
- 10:08:2026 - 20:10:49: env block 35 -> 36 vec4s. Slot 35 carries the SUN'S
  BODY (SUN_ANGULAR_DIAMETER / SUN_GLARE_ANGULAR_DIAMETER / SUN_DISC_LUMA /
  SUN_GLARE_LUMA_MAX) from the GENERATED header rather than as #defines here:
  design derives those rows and the shader measures the frame with them, so
  they have two consumers and may exist exactly once (Rule 35). Paired with
  apply_environment per this file's own contract notice.
- 11:08:2026 - 13:38:39: AERIAL PERSPECTIVE (REFERENCE_FRAMES.md R1). env block 36 -> 37
  vec4s; slot 36 = HAZE_SCALE_LENGTH / HAZE_HEIGHT_SCALE from the generated
  header. dfn_fog_factor (a smoothstep over 0.30..0.85 of CAMERA_FAR) DELETED
  and replaced by dfn_aerial(): Beer-Lambert extinction through air whose
  density falls with height, fading into dfn_sky_gradient() at the view
  direction rather than into one flat u_fogColor. The old span began at
  2400 m in a world 1024 m across, so it had never been nonzero anywhere —
  measured, not inferred (docs/specs/render.md §R1). u_fogColor / u_fogStart /
  u_fogEnd are now unread by any shader; the RenderEnvironment fields stay
  because that header is a frozen contract (Rule 26) and removing them is a
  request to the lead, not a tidy-up.
- 11:08:2026 - 14:24:26: THE MIST BAND (R2, reference frames 02/04/12). env block 37 -> 38;
  slot 37 = MIST_BAND_HEIGHT / _THICKNESS / _DENSITY. A SECOND TERM IN THE SAME
  DENSITY INTEGRAL, never a second fog pass — a trapezoid in altitude whose
  smoothstep ramps have a closed-form antiderivative, so the mean along a ray
  stays exact and nothing is ray-marched. It reads as a BAND rather than as
  more haze up high for a geometric reason worth keeping: from an eye below the
  layer, optical depth peaks at the layer's TOP edge and falls again above it,
  because a higher surface is seen at a steeper angle and a steeper ray spends
  less length inside a horizontal slab.
- 11:08:2026 - 14:40:43: dfn_cloud_field3 — the SAME field construction read in 3D, for the
  horizon cumulus (R3.1). Its own measured mean/SD 0.5000/0.1185 against the
  2D pair's 0.4980/0.1368. NOT Rule 31 at its original size — measured, the 2D
  constants cost 1.4x worst coverage error in aggregate, but lose a THIRD of the
  cloud at cover 0.05, where this field's whole job is. Needed because the
  band read the field as a function of AZIMUTH ALONE, which made the silhouette
  single-valued — no hole was possible in it — and inverting a squared
  threshold gave vertical sides under a flat top, i.e. a mushroom cap.
  OWED: no CPU reference in CloudModel.cpp and no test yet, unlike the 2D field.
- 11:08:2026 - 14:47:30: CORRECTION to the entry above, from the control test that was owed.
  I had written that reusing the 2D mean/SD 'would have re-run Rule 31
  exactly'. Overstated. Measured: 1.4x worst coverage error in aggregate, but
  a THIRD of the cloud lost at cover 0.05. Rule 31's form at a fifth of its
  size, not its severity. CloudModelTests.cpp now asserts it on coverage --
  by decile error the two pairs barely differ, which is how it hid.
- 12:08:2026 - 00:14:02: Slot [8].y = u_groundTint, the DOSE of the R5 ground colour
  (fs_terrain + dfn_ground.sh). It rides in the terrain slot because that is
  what it is about, and it is a dose rather than a switch so that the value
  can be swept from the environment (DFN_GROUND_TINT) without a rebuild —
  and so that 0 is a genuine zero-dose control arm rather than "roughly the
  old look" (Rule 48).
- 12:08:2026 - 22:45:00: R3.3 — THE HARD BRIGHT BAND AT THE HORIZON WAS THE SHEET'S OWN
  AREA MEAN, drawn where the field was still resolvable. dfn_cloud_field now
  renormalises onto the mean and spread that SURVIVE its per-octave LOD, and
  dfn_cloud_alpha's outer convergence is keyed to that residual spread instead
  of to cells_px — so it fires where the field is dead (cells_px 0.59..0.68)
  rather than while the base octave still carries 21 % of its amplitude
  (cells_px 0.20..0.60). Two convergences were redundant and the outer one ran
  far ahead of the inner one; the gap between them, projected into the frame,
  WAS the band. Measured with the cloud-only difference image: per-row SD at
  the band 9.4 -> 30.5, and its mean 74.5 -> 47.8, so it also stops being the
  brightest thing in the frame. Rule 31 on the premise, measured first:
  predicted SD tracks measured SD within 0.03 % at every rate.
- 12:08:2026 - 23:20:00: R3.2 — THE THIRD DECK and DIRECTIONAL SELF-SHADOWING. DFN_CLOUD_DECK_LOW_M
  1500 (derived: the same wavelength there subtends 2600/1500 = 1.73x the angle
  of the middle deck, and apparent CELL SIZE is what says "nearer"; no scale
  factor, only a world-space seed shift, because a scale would destroy exactly
  that) with its cover a FRACTION of the state's. dfn_cloud_self_shade: one
  extra field tap along the sun's horizontal bearing, step and gain both DERIVED
  from the field's measured correlation (SD of F(p+d)-F(p) is 0.190 at 0.25
  wavelengths against 0.408 for independent samples, so a quarter cell is a
  gradient and a full cell is noise; the 5/95 percentiles +-0.33 set the gain
  at 3.04). dfn_cloud_sun_vis gained the low deck — one authority for the field,
  and a deck that draws in the sky but does not occlude the sun is W4's named
  reject with its sign flipped. TWO OF MY OWN MISTAKES, both caught by
  measurement and both written where they happened: the first zero-dose arm was
  not zero dose (gain 0 pins the term at 0.5 = a flat mid-grey deck), and the
  term was combined with max() (which floored the deck at half-shaded and
  deleted the directional signal wherever density was larger).
- 13:08:2026 - 18:10:00: THE FILL HAS A DIRECTION (env block 38 -> 39, slot 38).
  The user: "тёмные деревья, словно их нет, как чёрное пятно ... она должна быть
  темнее переда, но цвет одинаковый". He is right and the number is p90/p10 =
  1.01x across a shadowed bole: every term in this function was gated on the
  sun, the moon or a lamp, so by day the shadow half-space reduced to
  `u_ambientColor * sky_vis` — a constant with no normal in it, leaving the
  albedo texture times a number. Two zero-mean directions restore it: n.y (sky
  over ground, which gives a crown its top and bottom) and dot(n, sunDir) (the
  sky is brightest around the sun, the only one of the two a VERTICAL bole can
  use). MY OWN OVERCLAIM, CAUGHT BY MEASURING IT: the first version was
  zero-mean over a SPHERE of normals and I wrote that it therefore could not
  move the frame mean. A frame is not a sphere of normals, it is mostly ground
  and ground faces up — whole-frame mean 84.81 -> 87.27, open ground 88.90 ->
  98.18, a 10 % brightening of the world under a claim of preservation. Now
  divided by the fill a straight-up normal gets, so the surface u_ambientColor
  was calibrated on is the one that does not move.
- 13:08:2026 - 18:18:00: CORRECTION TO THE SHAPE ABOVE, WITHIN THE HOUR, because
  the first shipped form ANSWERED THE COMPLAINT BY DARKENING THE THING
  COMPLAINED ABOUT. Referencing the fill to an up-facing normal is physically
  right and artistically backwards here: every normal that is not up can then
  only lose light, and measured it did — bole mean 22.18 -> 19.42, crown
  68.21 -> 59.72, i.e. 12 % darker, for a bole p90/p10 of 1.01x -> 1.05x. The
  user is looking at trees that read as absences; a fix that dims them is not
  one. Now: min(n.y, 0.0) so only UNDERSIDES lose, and the sun term uses the
  sun's AZIMUTH with its vertical component removed, so it is exactly 0 on
  level ground and sweeps +-u_fillSun around a bole with zero mean. Measured,
  one binary two arms: bole 1.01x -> 1.16x with its mean going UP 22.18 ->
  23.25, whole frame 3.75x -> 4.19x with the mean held (83.22 -> 82.59).
- 13:08:2026 - 18:52:00: env block 39 -> 40, and NOTHING ELSE. Slot 39 will carry
  the CLOUD DECK ALTITUDES (R3.4), which stop being #defines because the user
  asked for the ceiling's height to be a FIELD with a legal range — that is how
  a place's weather and climate will be read off the sky. The size is raised in
  its own commit, ahead of the first read: this contract is two lines in two
  files (here and BgfxRendererImpl.h) and a shader indexing past the declared
  array is undefined behaviour that surfaces as somebody else's broken build.
- 13:08:2026 - 19:20:00: R3.4 — THE DECK ALTITUDES ARE A FIELD, NOT #defines. The user
  asked for the cloud ceiling to have a legal RANGE and to sit at different
  heights in different weather and different places, because that is how the
  weather and climate of a place get shown. So DFN_CLOUD_DECK_{LOW,MID,HIGH}_M
  become u_cloudDeck{Low,Mid,High} in slot 39, written per frame by
  engine/render's CloudModel from cloud_cover and the observer's position. They
  ride in ONE slot because fs_sky intersects the VIEW ray with these planes
  while dfn_cloud_sun_vis projects along the SUN to the same planes: two numbers
  that disagree slide the ground shadow out from under the cloud casting it.
  The shipped 1500/2600/4400 survive as RenderEnvironment's DEFAULT, which is
  this change's zero-dose arm (Rule 48).
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 19:49:07: THE MOON'S GROUND GAIN IS A UNIFORM (slot 39.w) and its value is
  a measurement solved for the gain. At the shipped 0.30 a FULL MOON left the
  ground 9.72 luma above a moonless night -- 0.49 of ONE palette shade step,
  i.e. half the quantiser's own cell, so «ночью темно и вообще ничего не видно»
  was literally true in this project's units. Worse, the moonlit ground spanned
  p10 13.13 to p90 19.98, 0.34 of a step from its darkest tenth to its
  brightest: ONE palette entry, no shape at all. 1.234 buys exactly two steps
  of separation, verified on the frame (39.99 luma) and with p10/p90 now
  1.09 steps apart. The moon term is zero at new moon by construction, and the
  DFN_MOON_GROUND=0 arm came back IDENTICAL to the new-moon frame, so a
  moonless night is untouched and the ambient floor was not raised.
- 13:08:2026 - 20:19:19: u_deckThick (slot 38.z) — the middle deck's THICKNESS in metres, so the
  main sheet stops being a plane. Half a coverage cell, derived from the field's
  own scale rather than from meteorology (a layer element is wider than it is
  deep; at the cell width the vertical structure would be as fine as the
  horizontal and read as noise). DFN_DECK_THICK is the dose and 0 gives the
  shipped sheet back to within 1/255.
- 13:08:2026 - 22:44:30: Точечный свет: снято укорачивание reach = 1 − 0.55·dark и atten² заменён smoothstep на том же линейном окне. Довод — строка реестра TORCH_RADIUS_DARK, предсказавшая ровно это прочтение («лишь клочок» — атмосфера, «не видно ног» — управление): с укорачиванием пол в 2.79 м от пламени читал 4.49 luma при шаге палитры 19.99, то есть контракт не выполнялся на ПОЛОВИНЕ контрактной дистанции. С atten² контракт недостижим при номинале 9 вовсе (нужен R ≥ 11.7 по нормировке того же бокса). После: 47.22 / 54.78 / 62.56, две руки побитово равны. Метод — две сборки одного HEAD 3403375, разница только в этих строках (двери в шейдерном пути нет); прибор — бокс feet0 из FINDING_DUNGEON_DARK, сверен с независимым замером до сотой (4.49). Тьма места остаётся тьмой потому, что ambient там закрыт, а не потому, что лампа сломана.
- 13:08:2026 - 23:18:00: Правка ЗАПИСИ, не кода: предыдущая запись была датирована завтрашним числом (14:08 02:12 при стенных 22:44) и вклинена МЕЖДУ первой и второй строками записи про толщину яруса — один документ читался как два, и ни один не читался верно. Блок UPD есть собственная хронология файла: запись, датированная позже тех, что за ней последовали, или вложенная внутрь чужой, делает порядок событий нечитаемым, а за порядком в него и приходят.
- 16:08:2026 - 22:11:47: слот 40 — ПОЛОСА ФЕЙДА ЛИСТВЫ ПО РАКУРСУ (lo/hi в |dot(N,V)|), двери
  DFN_FOLIAGE_EDGE_LO / DFN_FOLIAGE_EDGE_HI.
- 22:08:2026 - 17:10:00: третий стоп неба — горизонтное свечение в dfn_sky_gradient
  (нижние ~6°, тёплый подъём того же цвета горизонта; лента 175.8..175.4
  люма на 48 строк была насыщением двухстопного mix). Доза DFN_SKY_GLOW
  через u_envParams[40].z, 0 = прежний градиент бит-в-бит.
- 22:08:2026 - 21:00:00: гейт интерьерного света небесной видимостью приёмника (u_lightColor(i).w бит 2, доза u_lightInteriorGate). Различение «внутри/снаружи» дало запечённое AO построек.
- 22:08:2026 - 22:20:00: G3 — отскок от земли нижней полусфере (u_hemiBounce, слот 13.x):
  испод навеса терял свет дважды (fillUp + пол AO), теперь получает долю
  солнца, отражённую землёй; гейт по sky_vis не пускает отскок в
  запечатанный интерьер. DFN_AMBIENT_HEMI, 0 = бит-в-бит.
- 22:08:2026 - 22:40:00: ворота отскока сужены ((sky_vis-0.32)/0.22): широкая
  рампа душила испод навеса до долей процента (A/B: 347 пикселей из 3.7 млн);
  теперь испод крыльца +31 люма, запечатанный интерьер по-прежнему ноль.
- 23:08:2026 - 00:30:00: u_pathTiles/u_pathMatDose (слот 13) и DFN_GATE_RECEIVER — листва
  отвечает гейту интерьерного света единицей (её AO — крона, не комната;
  приёмка: пучок в тени наружной стены светился от очага, maxch 71).
- 23:08:2026 - 01:40:00: коробка комнаты интерьерного света (u_lightRoom, окно в плане с кромкой
  0.3 м) ЗАМЕНЯЕТ гейт по AO у источников с коробкой; без коробки — прежний
  гейт. Массив 41 -> 49 парой с Impl.h.
- 23:08:2026 - 04:10:00: ворота отскока прижаты к полу AO ((sky_vis-0.305)/0.08): круг 4 поймал
- 22:08:2026 - 22:58:55: мягкость точечного света: дробная часть w цвета — softness 0..1 (wrap-диффуз + пологое затухание pow(fall, 1->0.6)); u_lightSoftDose (слот 13.w, DFN_LIGHT_SOFT, 0 = бит-в-бит). Заказ владельца: разнообразные источники по яркости и мягкости.
- 22:08:2026 - 23:48:50: массив 49 -> 50; u_pathMask (слот 49) — маска троп для fs_terrain.
- 23:08:2026 - 00:28:33: скотопическая ночь в dfn_aerial (u_nightScotopic, слот 34.w): тёмное десатурируется к люме, яркое держит цвет; гейт u_moonLight, доза 0 — бит-в-бит.
- 23:08:2026 - 01:16:53: изотропная составляющая точечного света (u_lightIsoDose, слот 36.w, DFN_LIGHT_ISO): вплотную к огню поверхность горит со всех сторон — стекло, столб, чаша; 0 — бит-в-бит.
- 23:08:2026 - 02:05:00: u_cloudLitDose (слот 14.x, DFN_CLOUD_LIT) — освещённое двухтоновое облако (Э5).
- 23:08:2026 - 02:07:35: Э4 — DFN_CLOUD_CELLS_PX_ANISO (малая ось следа, доза DFN_CLOUD_ANISO, слот 14.y): рябь рабочей зоны от преждевременного площадного среднего по радиали.
- 23:08:2026 - 02:13:26: Э3 — пятиоктавное листовое поле (сверх-октавы 0.25/0.50, mean/sd ЗАМЕРЕНЫ на 409600 образцах: 0.498204/0.104267); куб кумулюсов намеренно на прежней тройке; доза DFN_CLOUD_MACRO (слот 14.z), 0 = бит-в-бит.
- 23:08:2026 - 02:15:40: u_cloudPathResDose (слот 38.w, DFN_CLOUD_PATHRES) — Э6.
- 23:08:2026 - 02:45:45: u_pathBombDose (слот 50.x, DFN_PATH_TILES_BOMB); массив 50 -> 51 — анти-повтор мостовой.
  их мёртвыми на рыночном навесе (|dRGB| 0.003 между дозами) — испод сидит на
  0.30..0.34, порог 0.32 съедал эффект; запечатанный интерьер остаётся нулём.
*/

#ifndef DFN_ENV_SH
#define DFN_ENV_SH

// Приёмник гейта интерьерного света: по умолчанию sky_vis фрагмента;
// шейдер листвы переопределяет ЕДИНИЦЕЙ до включения этого файла (см. гейт).
#ifndef DFN_GATE_RECEIVER
#define DFN_GATE_RECEIVER(sv) (sv)
#endif

// Cube shadows for the carried lights. Included here rather than by each
// fragment shader because the light LOOP lives here: a torch that shadowed in
// terrain but not in props would be worse than one that never shadowed.
#include "dfn_pointshadow.sh"

uniform vec4 u_envParams[51]; // 41..48 — коробки; 49 — маска троп; 50 — дозы (пара с Impl.h)

#define u_sunDir         (u_envParams[0].xyz)
#define u_sunColor       (u_envParams[1].xyz)
#define u_ambientColor   (u_envParams[2].xyz)
#define u_fogColor       (u_envParams[3].xyz)
#define u_fogStart       (u_envParams[4].x)
#define u_fogEnd         (u_envParams[4].y)
#define u_envTime        (u_envParams[4].z)
#define u_skyZenith      (u_envParams[5].xyz)
#define u_skyHorizon     (u_envParams[6].xyz)
#define u_sandHeight     (u_envParams[7].x)
#define u_sandBlend      (u_envParams[7].y)
#define u_rockSlopeStart (u_envParams[7].z)
#define u_rockSlopeEnd   (u_envParams[7].w)
#define u_terrainTiles   (u_envParams[8].x)
// R5 GROUND TINT DOSE. 1 = shipped, 0 = the zero-dose control arm
// (DFN_GROUND_TINT=0), which must reproduce the pre-R5 ground exactly — a
// criterion that still passes at zero dose is measuring the light or the
// terrain and not the material (Rule 48).
#define u_groundTint     (u_envParams[8].y)
#define u_waterColor     (u_envParams[9])
#define u_waterScroll    (u_envParams[10].xy)
#define u_moonDir        (u_envParams[11].xyz)
#define u_moonPhase      (u_envParams[11].w)
#define u_moonColor      (u_envParams[12].xyz)
#define u_moonLight      (u_envParams[12].w)
#define u_starIntensity  (u_envParams[14].w)
// Authored darkness of the PLACE the player is in (0 = normal, 1 = the black
// void the user asked for in deep caves). Multiplies what survives the
// geometric sky-visibility term, and shortens carried lights.
#define u_ambientDarkness (u_envParams[15].x)
#define u_lightCount      (u_envParams[15].y)
// Доза гейта интерьерного света (DFN_LIGHT_INTERIOR; 0 = флаг игнорируется).
#define u_lightInteriorGate (u_envParams[15].z)
// Доза коробки комнаты (DFN_LIGHT_ROOM; 0 = окно игнорируется).
#define u_lightRoomGate     (u_envParams[15].w)
// Коробка комнаты источника i: (cx, cz, hx, hz); нулевые h — коробки нет.
#define u_lightRoom(i)      (u_envParams[41 + (i)])
// Отскок от земли для нижней полусферы (G3; слот 13 стоял зарезервированным
// «was the single light»). DFN_AMBIENT_HEMI, 0 = без отскока бит-в-бит.
#define u_hemiBounce        (u_envParams[13].x)
// Повторов путевого атласа на метр полотна и доза материала троп (fs_terrain).
#define u_pathTiles         (u_envParams[13].y)
#define u_pathMatDose       (u_envParams[13].z)
#define u_lightSoftDose     (u_envParams[13].w)
#define u_pathMask          (u_envParams[49])
#define u_nightScotopic     (u_envParams[34].w)
#define u_lightIsoDose      (u_envParams[36].w)
#define u_cloudLitDose      (u_envParams[14].x)
#define u_cloudAnisoDose    (u_envParams[14].y)
#define u_cloudMacroDose    (u_envParams[14].z)
#define u_cloudPathResDose  (u_envParams[38].w)
#define u_pathBombDose      (u_envParams[50].x)
// Point lights: [16+i] = position.xyz + radius, [24+i] = colour.xyz + flags.
#define DFN_MAX_LIGHTS 8
#define u_lightPosRad(i) (u_envParams[16 + (i)])
#define u_lightColor(i)  (u_envParams[24 + (i)])
// Wind: ONE wind for the world (foliage now, grass and cloth later).
#define u_windDir        (u_envParams[32].xy)
#define u_windStrength   (u_envParams[32].z)
#define u_windFlutter    (u_envParams[32].w)
// Clouds (WEATHER.md W4): the weather-state tuple's cloud slice plus the
// drift offset of the ONE coverage field. Both the sky sheet and the ground
// shadow sample the field through u_cloudOffset — never a second offset.
#define u_cloudCover      (u_envParams[33].x)
#define u_cloudCumulus    (u_envParams[33].y)
#define u_cloudShadow     (u_envParams[33].z)
#define u_cloudWavelength (u_envParams[33].w)
#define u_cloudOffset     (u_envParams[34].xy)
// The SUN'S BODY (W9). These are NUMBERS rows, not look-dev values, and they
// arrive through the uniform rather than as #defines here for one reason:
// design derives them and render measures the frame with them, so they have
// two consumers and may exist exactly once (Rule 35). The backend fills this
// slot straight from the generated header, so there is no second copy to
// drift. Radii, not diameters — the shader compares against an angle.
#define u_sunDiscRadius   (u_envParams[35].x)
#define u_sunGlareRadius  (u_envParams[35].y)
#define u_sunDiscLuma     (u_envParams[35].z)
#define u_sunGlareLumaMax (u_envParams[35].w)
// AERIAL PERSPECTIVE (REFERENCE_FRAMES.md R1). Same route as the sun's body and
// for the same reason (Rule 35): NUMBERS rows with two consumers — design
// derives them from the landmark depth-separation contract (§1.3a) and this
// shader is what makes the frame obey them — so they exist once and the
// backend hands them over from the generated header.
#define u_hazeScale       (u_envParams[36].x)
#define u_hazeHeight      (u_envParams[36].y)
#define u_hazeBase        (u_envParams[36].z)
// THE MIST BAND (REFERENCE_FRAMES.md R2): a horizontal layer of denser air at
// its own altitude, which is a different thing from the haze above and not a
// setting of it. Centre / total vertical extent / density in multiples of the
// ground air.
#define u_mistHeight      (u_envParams[37].x)
#define u_mistThickness   (u_envParams[37].y)
#define u_mistDensity     (u_envParams[37].z)
// THE FILL'S DIRECTION. Slot 38; see dfn_surface_light and the derivation with
// the constants in BgfxRendererImpl.h.
#define u_fillUp          (u_envParams[38].x)
#define u_fillSun         (u_envParams[38].y)
// THE MIDDLE DECK'S THICKNESS, metres. A deck read at ONE plane intersection is
// rule 52 exactly — a silhouette from below with no vertical dimension — and
// fs_sky now reads it as a slab. The value is derived from the FIELD's own
// scale rather than from meteorology: one coverage cell is
// WIND_FIELD_WAVELENGTH across, a layer element is wider than it is deep (real
// stratocumulus run 2-5 km across against 0.5-1 km of depth), and half the cell
// puts the deck's aspect at 2:1 — wide, which is what a deck is. Anything near
// the cell width would make the vertical structure as fine as the horizontal
// and read as noise instead of as thickness.
// DFN_DECK_THICK is its dose; at 0 the slab collapses to the plane that
// shipped, byte for byte, which is the zero-dose arm this change is read
// against (Rule 48).
#define u_deckThick       (u_envParams[38].z)
// The quantiser's own luma weights (fs_upscale.sc). Every brightness rule in
// the sky is written in THIS metric and not in Euclidean RGB, because the
// palette pass weights the channels and a difference that lives in blue is
// nearly invisible to it (Rule 36's pipeline-metric clause).
#define DFN_LUMA_WEIGHTS vec3(0.30, 0.59, 0.11)

// Cloud layer altitudes, meters ABOVE SEA LEVEL (world y). Look-dev pair for
// the two-sheet parallax: the sky intersects the view ray with these planes,
// the ground shadow projects along the sun to the SAME planes, so the sheet
// and its shadow line up by construction. Terrain tops out at ~400 m
// (WORLDGEN_MAX_HEIGHT), so both planes clear every landform.
//
// SIZED AGAINST THE VIEWING DISTANCE (Rule 33). These were 1200/2200 m, and at
// 1200 m the entire sky above 45 deg elevation mapped to a disc of 1.86
// wavelengths — 10.8 cells of AREA for the whole upper hemisphere. Mid-sky was
// therefore one or two cells and had nothing to draw at any threshold; it was
// a coin flip whether the frame caught a cloud overhead. The wavelength cannot
// move (it is NUMBERS' WIND_FIELD_WAVELENGTH, derived for the GROUND shadow's
// viewing distance and shared), so the SKY's distance to the field is what
// changes: 2600 m puts ~5 wavelengths of radius above 45 deg and ~9 above 30,
// which is a picture instead of a coin flip.
//
// THE ALTITUDES ARE NO LONGER #defines (R3.4). The user asked for the ceiling's
// height to be a FIELD with a legal range — «должен быть диапазон где им можно
// быть... в разную погоду на разных, будем таким образом погоду и климат
// отображать» — so the three altitudes are computed per frame by
// engine/render's CloudModel from the weather state and the observer's place,
// and arrive in slot 39. The numbers below survive as the DEFAULT of
// RenderEnvironment::cloud_deck_m, which is what makes an unchanged caller draw
// the sky that shipped, byte for byte.
//
// They ride in ONE slot because their two consumers must never disagree: fs_sky
// intersects the VIEW ray with these planes, dfn_cloud_sun_vis projects along
// the SUN to the same planes, and if those read different numbers the ground
// shadow slides out from under the cloud casting it.
#define u_cloudDeckLow  (u_envParams[39].x)
#define u_cloudDeckMid  (u_envParams[39].y)
#define u_cloudDeckHigh (u_envParams[39].z)
// The high deck samples the SAME field at a coarser scale and a fixed seed
// shift so the decks decorrelate without inventing a second field or a second
// wind.
#define DFN_CLOUD_DECK_HIGH_SCALE 0.47
#define DFN_CLOUD_DECK_HIGH_SEED  vec2(310.0, -170.0)

// --- THE LOW DECK (R3.2), and its altitude is DERIVED ----------------------
// Reference frame 12 carries at least three strata. Two decks of one tone at
// 2600/4400 m are one ceiling with extra opacity, whatever their parallax:
// what says "nearer" is APPARENT CELL SIZE, and that is set by the plane's
// distance, not by any tone. The same field wavelength at 1500 m against
// 2600 m subtends 2600/1500 = 1.73x the angle — a difference the eye reads as
// depth. The window is narrow at both ends and both ends were checked (Rule
// 30's "a range is two assertions"): below ~1000 m one cell is wider than half
// the frame and the deck stops being a deck, and by 2000 m the ratio is 1.3
// and it merges into the middle one. Real broken stratocumulus bases sit at
// 600-2000 m, so 1500 m is inside the physical range as well as the visual one.
//
// R3.4 MOVED THIS ALTITUDE INTO A RANGE and both of the bounds above became the
// bounds of that range: CLOUD_CEILING_MIN_M 400 (where one cell subtends the
// WHOLE frame rather than half of it, at CAMERA_FOV_Y = 75 deg) and
// CLOUD_CEILING_MAX_M 2000 (the 1.3 ratio, unchanged). 1500 is now the middle
// of the window and the value an unwritten env field still carries.
//
// THE LOW END STOPPED BEING A PROBLEM WHEN THE DRIVER ARRIVED, and this is
// worth writing down because the note above reads like an objection to it: a
// deck at 400 m is one cell wide, i.e. a lid with no masses in it — but the
// SAME weather state that puts the ceiling at 400 m is heavy cover, and a
// lid is what heavy cover looks like from underneath. The two co-vary by
// construction because both are driven by cloud_cover.
//
// NO SCALE FACTOR HERE, unlike the high deck: a scale would change the
// apparent cell size and destroy the one thing this deck exists to provide.
// It gets a world-space SEED SHIFT instead, which decorrelates it from the
// middle deck without touching how big its cells look.
#define DFN_CLOUD_DECK_LOW_SEED vec2(-820.0, 640.0)
// The low deck's cover is a FRACTION OF THE STATE'S, never the state's own
// value. Three independent decks at full cover close 1 - (1-c)^3 of the sky =
// 83 % at c = 0.45, and the holes this whole exercise exists for would be
// gone. At 0.45 of the state the three close 64 %, against 55 % for the two
// that shipped before — the sky gains a stratum and keeps its breaks.
#define DFN_CLOUD_DECK_LOW_COVER 0.45
// Softness of the coverage threshold, in UNIFORM field units (post-remap).
// Scale-free: after the remap the field's units are probability, so this is
// "the softest 10% of the distribution" at every cover value.
#define DFN_CLOUD_EDGE_U 0.10
// Mean and standard deviation of the raw octave sum, MEASURED over 400k
// samples (engine/render/sources/CloudModel.cpp is the reference side and
// tests/render/CloudModelTests.cpp asserts them). The remap below is this
// Gaussian's own CDF.
#define DFN_CLOUD_FIELD_MEAN 0.4980
#define DFN_CLOUD_FIELD_SD   0.1368
// sqrt(W0^2+W1^2+W2^2) at full resolution. Denominator of the residual spread
// (see dfn_cloud_lod_residual). Mirrored: CLOUD_OCTAVE_W_NORM.
#define DFN_CLOUD_W_NORM     0.640156
// The outer convergence window, stated on the RESIDUAL SPREAD rather than on
// cells-per-pixel — that change of quantity IS the R3.3 fix. Mirrored:
// CLOUD_LOD_RES_LIVE / CLOUD_LOD_RES_DEAD.
#define DFN_CLOUD_RES_LIVE   0.18
#define DFN_CLOUD_RES_DEAD   0.04

float dfn_cloud_hash(vec2 c)
{
    return fract(sin(dot(c, vec2(127.1, 311.7))) * 43758.5453);
}

float dfn_cloud_vnoise(vec2 p)
{
    vec2 c = floor(p);
    vec2 f = p - c;
    f = f * f * (3.0 - 2.0 * f);
    float a = dfn_cloud_hash(c);
    float b = dfn_cloud_hash(c + vec2(1.0, 0.0));
    float d = dfn_cloud_hash(c + vec2(0.0, 1.0));
    float e = dfn_cloud_hash(c + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(d, e, f.x), f.y);
}

// An octave whose cells have gone under about two pixels contributes nothing
// but aliasing. It is replaced by its MEAN, not scaled toward zero: scaling
// toward zero shrinks the field's spread and silently moves every coverage
// threshold with it — the Rule 31 defect re-introduced by the fix for it.
float dfn_cloud_octave_lod(float cells_px, float freq)
{
    return 1.0 - smoothstep(0.22, 0.75, cells_px * freq);
}

// THE coverage field (W4): one authority, UNIFORM on [0,1] by construction.
// `p` is world x/z ON A LAYER PLANE in meters, drift NOT yet applied — every
// sampler goes through the dfn_cloud_sheet*_alpha wrappers below, which add
// u_cloudOffset, so no call site can drift its own copy. `cells_px` is how
// much field one pixel covers here, in wavelengths, along the WORST screen
// axis (see dfn_cloud_cells_px).
//
// The octave sum is Gaussian, so thresholding it at 1-cover does NOT cover
// `cover`: measured, the sum occupied 0.045..0.945 with 98% of its mass in
// 0.200..0.797, cover 0.10 drew nothing at all and the shipped default 0.45
// drew 0.19 — the first shoot's empty sky, visible in the numbers before it
// was visible in a frame. Pushing the sum through its own CDF makes the field
// uniform, and then the threshold means what it says.
//
// AND THE LOD MOVES THAT GAUSSIAN, which is R3.3. Replacing an octave by its
// mean shrinks the sum's spread, so remapping through the FULL-RESOLUTION
// mean/SD at a reduced rate walks the threshold straight off the distribution.
// Measured on that form at cells_px 0.50: cover 0.15 drew 0.0000 of the plane
// and cover 0.60 drew 1.0000 — both ends collapsed into the two constants a
// field can be, which is why the horizon needed an outer convergence to hide
// it. Renormalised onto the surviving mean and spread instead, coverage holds
// to 0.035 at EVERY rate and the surviving structure keeps its full contrast.
// Rule 31: the uncorrelated-equal-variance premise the residual rests on was
// MEASURED, not assumed (CloudModel.cpp / CloudModelTests.cpp).
// Э3 (волна 23.08): ДВЕ СВЕРХ-ОКТАВЫ 0.25/0.50 (2.4 км и 1.2 км при
// wavelength 600 м). Три октавы одной декады давали в зените клетку 11.7°
// дуги («мраморная лента» на пол-неба), а у горизонта все три уходили под
// пиксель ОДНОВРЕМЕННО — 170 строк кадра байт-в-байт константой. Макро-
// октавы переживают в 4x/2x больший cells_px и лечат оба конца одной
// правкой. Веса: прежняя тройка, сжатая в 0.6 (структура рабочей зоны
// сохраняет большинство амплитуды), макро-пара делит остаток 0.40 тем же
// соотношением ~1.96 на октаву (0.135/0.265). Mean/SD НЕ тождество, а
// ЗАМЕР: 409600 образцов CPU-зеркала (шаг 0.731, несоизмерим с решёткой)
// дали mean 0.498204, sd 0.104267 при ||w|| 0.485776; sd/||w|| = 0.21464
// против 0.21370 у тройки — некоррелированность подтверждена с точностью
// 0.44%. Куб кумулюсов (field3, кольцо на фиксированных 20 км) от болезни
// проекции не страдает и НАМЕРЕННО остаётся на прежней тройке — его
// mean/sd не перемеряются. Доза DFN_CLOUD_MACRO, 0 — прежнее поле
// бит-в-бит.
#define DFN_CLOUD_MACRO_MEAN 0.498204
#define DFN_CLOUD_MACRO_SD 0.104267
#define DFN_CLOUD_MACRO_W_NORM 0.485776
float dfn_cloud_lod_residual(float cells_px)
{
    if (u_cloudMacroDose > 0.5) {
        float wa = 0.135 * dfn_cloud_octave_lod(cells_px, 0.25);
        float wb = 0.265 * dfn_cloud_octave_lod(cells_px, 0.50);
        float w0 = 0.330 * dfn_cloud_octave_lod(cells_px, 1.00);
        float w1 = 0.168 * dfn_cloud_octave_lod(cells_px, 2.03);
        float w2 = 0.102 * dfn_cloud_octave_lod(cells_px, 4.07);
        return sqrt(wa * wa + wb * wb + w0 * w0 + w1 * w1 + w2 * w2)
             / DFN_CLOUD_MACRO_W_NORM;
    }
    float w0 = 0.55 * dfn_cloud_octave_lod(cells_px, 1.00);
    float w1 = 0.28 * dfn_cloud_octave_lod(cells_px, 2.03);
    float w2 = 0.17 * dfn_cloud_octave_lod(cells_px, 4.07);
    return sqrt(w0 * w0 + w1 * w1 + w2 * w2) / DFN_CLOUD_W_NORM;
}

float dfn_cloud_field(vec2 p, float cells_px)
{
    vec2 q = p / max(u_cloudWavelength, 1.0);
    // Пятиоктавная рука Э3 — см. блок над dfn_cloud_lod_residual.
    if (u_cloudMacroDose > 0.5) {
        float wa = 0.135 * dfn_cloud_octave_lod(cells_px, 0.25);
        float wb = 0.265 * dfn_cloud_octave_lod(cells_px, 0.50);
        float v0 = 0.330 * dfn_cloud_octave_lod(cells_px, 1.00);
        float v1 = 0.168 * dfn_cloud_octave_lod(cells_px, 2.03);
        float v2 = 0.102 * dfn_cloud_octave_lod(cells_px, 4.07);
        float raw = 0.5
                  + (dfn_cloud_vnoise(q * 0.25 + vec2(5.0, 71.0)) - 0.5) * wa
                  + (dfn_cloud_vnoise(q * 0.50 + vec2(29.0, 113.0)) - 0.5) * wb
                  + (dfn_cloud_vnoise(q) - 0.5) * v0
                  + (dfn_cloud_vnoise(q * 2.03 + vec2(17.0, 31.0)) - 0.5) * v1
                  + (dfn_cloud_vnoise(q * 4.07 + vec2(47.0, 89.0)) - 0.5) * v2;
        float sd_lod = DFN_CLOUD_MACRO_SD
                     * (sqrt(wa * wa + wb * wb + v0 * v0 + v1 * v1 + v2 * v2)
                        / DFN_CLOUD_MACRO_W_NORM);
        float mean_lod = 0.5 + (DFN_CLOUD_MACRO_MEAN - 0.5)
                                   * (wa + wb + v0 + v1 + v2);
        float z = (raw - mean_lod)
                / max(sd_lod, DFN_CLOUD_MACRO_SD * 1e-4);
        return 1.0 / (1.0 + exp(-1.702 * z));
    }
    float w0 = 0.55 * dfn_cloud_octave_lod(cells_px, 1.00);
    float w1 = 0.28 * dfn_cloud_octave_lod(cells_px, 2.03);
    float w2 = 0.17 * dfn_cloud_octave_lod(cells_px, 4.07);
    float raw = 0.5
              + (dfn_cloud_vnoise(q) - 0.5) * w0
              + (dfn_cloud_vnoise(q * 2.03 + vec2(17.0, 31.0)) - 0.5) * w1
              + (dfn_cloud_vnoise(q * 4.07 + vec2(47.0, 89.0)) - 0.5) * w2;
    // Both lines are IDENTITIES at full resolution (the weights sum to 1.0 and
    // their quadratic norm to DFN_CLOUD_W_NORM), so this generalises the
    // shipped constants rather than adding a second calibration.
    float sd_lod = DFN_CLOUD_FIELD_SD
                 * (sqrt(w0 * w0 + w1 * w1 + w2 * w2) / DFN_CLOUD_W_NORM);
    float mean_lod = 0.5 + (DFN_CLOUD_FIELD_MEAN - 0.5) * (w0 + w1 + w2);
    float z = (raw - mean_lod) / max(sd_lod, DFN_CLOUD_FIELD_SD * 1e-4);
    return 1.0 / (1.0 + exp(-1.702 * z)); // logistic ~= the normal CDF
}

// --- THE FIELD IN 3D, for the horizon cumulus (R3) -------------------------
// SAME construction, SAME octave weights, SAME CDF remap — and its OWN mean and
// SD, because they are not the 2D ones. Measured over 400k samples the 3D sum
// sits at mean 0.5000 / sd 0.1185 against 2D's 0.4980 / 0.1368. The sum is
// Gaussian and the remap is its own CDF, so a wrong SD makes `cover` mean
// something other than coverage. Reusing the 2-D pair does NOT reproduce Rule 31's
// original severity — there, cover 0.10 drew literally nothing. It reproduces
// its FORM at about a fifth of the size, and the honest numbers are: worst
// coverage error 0.0311 against the correct pair's 0.0225, i.e. 1.4x in
// aggregate, BUT at cover 0.05 the 2-D constants admit 0.022 where 0.05 was
// asked — a third of the cloud lost, concentrated exactly at the sparse end
// where a few fair-weather cumulus live. By DECILE error the two pairs are
// 0.0282 vs 0.0221 and the difference nearly disappears, which is how this
// would have gone unnoticed. Asserted on coverage in CloudModelTests.cpp.
//
// WHY 3D AT ALL, which is the whole cumulus fix. The band used to threshold a
// field of AZIMUTH ALONE against a height-rising threshold. For a fixed azimuth
// that makes alpha monotone in height, so the silhouette was a single-valued
// function of azimuth: one contiguous mass from the base upward, no holes, no
// overlap, ever — provably, not incidentally. And solving T(hn) = F for a
// squared threshold gives hn ~ sqrt(F), which has a VERTICAL tangent where a
// lobe crosses the threshold and a FLAT top at the lobe's peak. Vertical sides
// plus a flat top is a MUSHROOM CAP, and that is what the lead saw sitting on
// the horizon. Neither exponent fixes it — linear was tried before and gave
// straight-sided tents — because the defect is the dimensionality, not the
// curve. Sampled in 3D the masses get holes, overhangs and separate towers,
// which is also what R3 asks for.
#define DFN_CLOUD_FIELD3_MEAN 0.5000
#define DFN_CLOUD_FIELD3_SD   0.1185

float dfn_cloud_hash3(vec3 c)
{
    return fract(sin(dot(c, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float dfn_cloud_vnoise3(vec3 p)
{
    vec3 c = floor(p);
    vec3 f = p - c;
    f = f * f * (3.0 - 2.0 * f);
    float x00 = mix(dfn_cloud_hash3(c + vec3(0.0, 0.0, 0.0)),
                    dfn_cloud_hash3(c + vec3(1.0, 0.0, 0.0)), f.x);
    float x10 = mix(dfn_cloud_hash3(c + vec3(0.0, 1.0, 0.0)),
                    dfn_cloud_hash3(c + vec3(1.0, 1.0, 0.0)), f.x);
    float x01 = mix(dfn_cloud_hash3(c + vec3(0.0, 0.0, 1.0)),
                    dfn_cloud_hash3(c + vec3(1.0, 0.0, 1.0)), f.x);
    float x11 = mix(dfn_cloud_hash3(c + vec3(0.0, 1.0, 1.0)),
                    dfn_cloud_hash3(c + vec3(1.0, 1.0, 1.0)), f.x);
    return mix(mix(x00, x10, f.y), mix(x01, x11, f.y), f.z);
}

float dfn_cloud_field3(vec3 p)
{
    float raw = 0.5
              + (dfn_cloud_vnoise3(p) - 0.5) * 0.55
              + (dfn_cloud_vnoise3(p * 2.03 + vec3(17.0, 31.0, 7.0)) - 0.5) * 0.28
              + (dfn_cloud_vnoise3(p * 4.07 + vec3(47.0, 89.0, 23.0)) - 0.5) * 0.17;
    float z = (raw - DFN_CLOUD_FIELD3_MEAN) / DFN_CLOUD_FIELD3_SD;
    return 1.0 / (1.0 + exp(-1.702 * z)); // logistic ~= the normal CDF
}

// Coverage -> opacity. cover 0 = empty sky (alpha exactly 0 everywhere: the
// Rule 30 control — DFN_CLOUD=0 must erase sheet AND shadows in one move);
// cover 1 = solid, with no holes left by an edge hanging off the end of the
// range (the other of the two assertions a range is).
float dfn_cloud_alpha(vec2 p, float cover, float cells_px)
{
    if (cover <= 0.0) {
        return 0.0;
    }
    float u = dfn_cloud_field(p, cells_px);
    float edge = min(DFN_CLOUD_EDGE_U, min(cover, 1.0 - cover));
    float a = smoothstep(1.0 - cover - edge, 1.0 - cover + edge, u);
    // Once the field is DEAD — every octave replaced by its mean, nothing left
    // to threshold — the honest value is the area average, which for a uniform
    // field thresholded at 1-cover is `cover`.
    //
    // R3.3: this used to read `smoothstep(0.20, 0.60, cells_px)`, i.e. the
    // sheet was thrown away for its average while the base octave still
    // carried 21% of its amplitude. Two redundant convergences with the outer
    // one running far ahead of the inner one, and the gap between them,
    // projected into the frame, WAS the hard bright band at the horizon.
    // Keyed to the residual spread it fires only where there is genuinely
    // nothing left: res 0.18 is cells_px 0.59, res 0.04 is 0.68.
    float dead = 1.0 - smoothstep(DFN_CLOUD_RES_DEAD, DFN_CLOUD_RES_LIVE,
                                  dfn_cloud_lod_residual(cells_px));
    return mix(a, cover, dead);
}

// How much field one pixel covers, in wavelengths, at a sampled layer point.
// Taken from the SCREEN DERIVATIVES of the point itself, which is anisotropic
// for free: tangentially a pixel spans dist/px_per_rad, but RADIALLY the plane
// runs away as dist/dir_y — near the horizon that is twenty times more field
// per pixel, and it is the radial axis, not the tangential one, that smeared
// the first shoot's horizon into streaks. The worst axis wins.
//
// Derived rather than computed from the projection: the analytic version read
// px_per_rad off u_proj/u_viewRect, which came back as garbage in the sky pass
// and drove the ENTIRE sky past the resolution limit — every pixel converged
// to the area mean and the sky went out as a flat 45% wash. Caught by the
// cover-0 control, which was clean blue beside it. FRAGMENT SHADERS ONLY.
#define DFN_CLOUD_CELLS_PX(p) \
    (max(length(dFdx(p)), length(dFdy(p))) / max(u_cloudWavelength, 1.0))

// Э4 (волна 23.08): АНИЗОТРОПНАЯ мера — по МАЛОЙ оси следа пикселя. «Худшая
// ось побеждает» валила поле в площадное среднее радиально уже на средних
// высотах (сжатие 1/dir.y = 3.9x на 15°), пока тангенциально структура ещё
// резолвилась — рабочая зона неба рассыпалась в горизонтальную рябь из
// чёрточек. Усреднение вдоль БОЛЬШОЙ (радиальной) оси уже делает слэб:
// DECK_SLICES сэмплов вдоль луча ложатся в поле по той же радиали. Доза
// DFN_CLOUD_ANISO, 0 — прежний max() бит-в-бит.
#define DFN_CLOUD_CELLS_PX_ANISO(p) \
    (u_cloudAnisoDose > 0.5 \
         ? (min(length(dFdx(p)), length(dFdy(p))) / max(u_cloudWavelength, 1.0)) \
         : DFN_CLOUD_CELLS_PX(p))

// The two sheets, as the ONLY two ways to read the field. Layer 1 is the main
// sheet (full cover weight); layer 2 is the high thin sheet (reduced cover).
float dfn_cloud_sheet_alpha(vec2 p_on_layer1, float cells_px)
{
    return dfn_cloud_alpha(p_on_layer1 + u_cloudOffset, u_cloudCover, cells_px);
}

float dfn_cloud_sheet2_alpha(vec2 p_on_layer2, float cells_px)
{
    return dfn_cloud_alpha((p_on_layer2 + u_cloudOffset)
                               * DFN_CLOUD_DECK_HIGH_SCALE
                           + DFN_CLOUD_DECK_HIGH_SEED,
                           u_cloudCover * 0.75,
                           cells_px * DFN_CLOUD_DECK_HIGH_SCALE);
}

// The LOW deck (R3.2), third and nearest. Same field, same drift, its own seed
// and a FRACTION of the state's cover.
float dfn_cloud_sheet_low_alpha(vec2 p_on_deck_low, float cells_px)
{
    return dfn_cloud_alpha(p_on_deck_low + u_cloudOffset
                               + DFN_CLOUD_DECK_LOW_SEED,
                           u_cloudCover * DFN_CLOUD_DECK_LOW_COVER,
                           cells_px);
}

// --- DIRECTIONAL SELF-SHADOWING (R3.2), ONE TAP ----------------------------
// Returns -1 (turned to the sun, lighter) .. +1 (behind more cloud, darker) for
// a point on a deck: the SIGNED slope of the field along
// the SUN'S HORIZONTAL BEARING. Where the field is thicker toward
// the sun this point stands behind more cloud and darkens; where it is thinner
// the point is on a sun-facing slope and lightens.
//
// WHY THIS AND NOT MORE DENSITY SHADING. The deck's only shading before was
// `smoothstep(1 - cover*0.55, 1, F)`, which at the default cover 0.45 is
// smoothstep(0.7525, 1.0, F) while cloud exists wherever F > 0.55 — so the
// great majority of every mass was flat and the darkening only ever found the
// middle of a blob. And it is NON-DIRECTIONAL: it cannot move with the sun, so
// it cannot read as self-shadowing at any strength. This term costs one sample
// and moves with the hour for free.
//
// THE STEP IS DERIVED FROM THE FIELD'S OWN CORRELATION, not picked. Measured
// over 200k samples, the SD of F(p + d) - F(p) runs 0.088 / 0.190 / 0.295 /
// 0.397 at d = 0.10 / 0.25 / 0.50 / 1.00 wavelengths, against 0.408 for two
// INDEPENDENT samples of a uniform field. At a full cell the term is therefore
// 97 % noise — the neighbouring cell's shadow, not this one's slope — while at
// a quarter cell it is 47 %, i.e. the two samples still share most of their
// structure and the difference is a gradient. 0.25 it is.
//
// THE GAIN IS DERIVED FROM THE SAME MEASUREMENT. At 0.25 wavelengths the 5th
// and 95th percentiles of the difference are -0.324 and +0.329, so 1/0.329 =
// 3.04 maps the middle 90 % of the distribution onto the full -1..+1 range and
// clips a tenth of it at each end — which is what gives the deck blacks and
// whites instead of a grey wash.
//
// THE RETURN IS SIGNED, and the first version was not — it returned 0..1
// centred on 0.5 and each deck took max(density, shade). MEASURED, that was
// wrong twice over: the max() floored the whole deck at half-shaded (mid deck
// mean 227.6 -> 207.8 luma, a uniform darkening) and it DELETED the directional
// signal everywhere the density term was the larger of the two, so the deck's
// body actually lost variation (SD 23.87 -> 22.54, i.e. -6 % where the whole
// point was more). Signed and ADDED to the density term instead, the mean is
// preserved by construction (the difference of two samples of one field is
// symmetric about zero) and what is added is variation and only variation.
#define DFN_CLOUD_SHADE_STEP 0.25
#define DFN_CLOUD_SHADE_GAIN 3.04

// THE ZERO-DOSE ARM, and the FIRST one written for this term was wrong in a way
// worth keeping (Rule 48). Setting the GAIN to 0 does not remove the term: it
// pins it at 0.5, i.e. paints every deck a flat mid-grey, which is a different
// strong effect and not an absence. Measured against that fake control the
// shading looked like it did nothing (SD 26.15 -> 26.18 on the low deck)
// because both arms were being shaded, one of them uniformly. A dose of zero
// has to mean FULLY LIT.
#define DFN_CLOUD_SHADE_OFF 0

float dfn_cloud_self_shade(vec2 p_on_deck, float field_here, float cells_px)
{
#if DFN_CLOUD_SHADE_OFF
    return 0.0;
#else
    vec2 sun_h = normalize(u_sunDir.xz + vec2(1e-5, 0.0));
    vec2 q = p_on_deck + sun_h * (u_cloudWavelength * DFN_CLOUD_SHADE_STEP);
    float toward_sun = dfn_cloud_field(q, cells_px);
    return clamp((toward_sun - field_here) * DFN_CLOUD_SHADE_GAIN, -1.0, 1.0);
#endif
}

// Sun visibility through the cloud sheets at a WORLD point: project along the
// sun to each layer plane and read the same alphas the sky draws. Applied to
// the sun term of dfn_surface_light, so terrain, props and foliage darken
// together as the shadow crawls (the "мир живёт" frame). Fades out at low
// sun: near the horizon a crawling shadow degenerates into kilometers of
// smear, and dusk attenuation belongs to the state's sun_attenuation, not to
// this projection.
float dfn_cloud_sun_vis(vec3 wpos)
{
    if (u_cloudShadow <= 0.0 || u_cloudCover <= 0.0) {
        return 1.0;
    }
    float sun_y = u_sunDir.y;
    float low_sun = smoothstep(0.08, 0.20, sun_y);
    if (low_sun <= 0.0) {
        return 1.0;
    }
    vec2 p0 = wpos.xz
            + u_sunDir.xz * ((u_cloudDeckLow - wpos.y) / sun_y);
    vec2 p1 = wpos.xz
            + u_sunDir.xz * ((u_cloudDeckMid - wpos.y) / sun_y);
    vec2 p2 = wpos.xz
            + u_sunDir.xz * ((u_cloudDeckHigh - wpos.y) / sun_y);
    // Fully resolved: the shadow is read at a GROUND point, where one pixel
    // covers metres, not the kilometres a grazing sky ray covers. The sheet's
    // LOD exists for the sky's perspective and would only blur the shadow.
    // The high sheet is thin: half occlusion weight.
    //
    // ALL THREE DECKS, and the LOW one is not optional (R3.2). This file's
    // standing rule is one authority for the field, and a deck that draws in
    // the sky but does not occlude the sun is exactly the "shadow crossing
    // land with no cloud above it" reject W4 was built to prevent — with the
    // sign flipped: cloud overhead casting no shadow at all.
    float transmit = (1.0 - dfn_cloud_sheet_low_alpha(p0, 0.0))
                   * (1.0 - dfn_cloud_sheet_alpha(p1, 0.0))
                   * (1.0 - 0.5 * dfn_cloud_sheet2_alpha(p2, 0.0));
    return 1.0 - u_cloudShadow * low_sun * (1.0 - transmit);
}

// Sway offset for a wind-affected vertex, in WORLD space.
//   sway_weight: 0 at the attachment (branch/ground), 1 at the free edge.
//   phase:       per-INSTANCE, so a stand ripples instead of pulsing as one.
// u_windStrength already contains the CPU-side gust envelope, which is what
// lets audio and gameplay read the same number the leaves are moving to; this
// function only adds per-instance and per-place variation on top of it.
vec3 dfn_wind_offset(vec3 wpos, float sway_weight, float phase)
{
    if (u_windStrength <= 0.0 || sway_weight <= 0.0) {
        return vec3(0.0, 0.0, 0.0);
    }
    float tau = 6.2831853;
    // Gusts TRAVEL along the wind direction: without this term every tree in
    // a stand peaks at the same instant and the forest breathes as one object.
    float travel = dot(wpos.xz, u_windDir) * 0.06;
    // THREE BANDS FROM ONE WEIGHT (flora's ask, 15.08.2026: trunk low, branch
    // mid, leaf flutter). The band is not a new vertex channel — it is the
    // sway weight itself, which already means "how far this vertex is from
    // what holds it": a trunk vertex is near its root and sways slowly through
    // a big arc, a leaf is far from everything and flutters. Frequency
    // therefore RISES with the weight and the flutter term is gated by it, so
    // a bole cannot buzz.
    float band = clamp(sway_weight, 0.0, 1.0);
    float sway = sin(u_envTime * (0.75 + 0.55 * band) + phase * tau + travel);
    float flutter = sin(u_envTime * (2.6 + 2.4 * band) + phase * tau * 2.0)
                  * 0.35 * u_windFlutter * band * band;
    float amp = u_windStrength * sway_weight;
    vec2 horizontal = u_windDir * (amp * (sway + flutter) * 0.6);
    // A pushed card also DIPS. Pure horizontal translation reads as the card
    // sliding; the dip is what sells it as bending about its attachment.
    float dip = -abs(amp * sway) * 0.15;
    return vec3(horizontal.x, dip, horizontal.y);
}

// SCREEN-DOOR FADE (DrawParams::fade, arriving in u_params.y — passed as an
// argument because each fragment shader declares its own u_params).
//
// A LOD cross-fade draws the SAME GROUND at two levels for a moment. Alpha
// blending would need sorting and would double-darken; a dissolve costs one
// discard and needs neither. It also happens to be the only fade that survives
// this project's 64-colour post: a half-transparent surface would land between
// palette entries and be dithered anyway, so we do the dithering ourselves, at
// the internal-pixel grid, where it reads as texture rather than as mush.
//
// The 4x4 ordered matrix is computed rather than tabled (no dynamic array
// indexing on any backend): M4 = 4 * M2(high bits) + M2(low bits), with
// M2(x,y) = 2x + 3y - 4xy reproducing [[0,2],[3,1]] exactly.
float dfn_m2(float x, float y)
{
    return 2.0 * x + 3.0 * y - 4.0 * x * y;
}

float dfn_bayer4(vec2 p)
{
    vec2 q = mod(floor(p), 4.0);
    float xl = mod(q.x, 2.0);
    float yl = mod(q.y, 2.0);
    float xh = floor(q.x * 0.5);
    float yh = floor(q.y * 0.5);
    return (4.0 * dfn_m2(xh, yh) + dfn_m2(xl, yl)) / 16.0;
}

// Discards the fragment if this pixel is not part of the surviving pattern.
// fade >= 1 keeps everything (the maximum threshold is 15/16), fade <= 0 keeps
// nothing. The pixel coordinate is a PARAMETER, not gl_FragCoord read inside:
// this header is included by vertex shaders too (dfn_wind_offset), and
// gl_FragCoord does not exist there — naming it in the body fails the build of
// every vertex shader that includes this file.
void dfn_screen_door(float fade, vec2 pixel)
{
    if (fade < dfn_bayer4(pixel) + 0.03125) {
        discard;
    }
}

// Ground brightness of a FULL moon, as a multiple of moon_color. A full moon is
// ~400,000x dimmer than the sun, so this is a look-dev number and not a
// physical one; what it has to satisfy is that a full moon can be TOLD APART
// from a moonless night, and until now it could not.
//
// IT IS A UNIFORM NOW (slot 39.w) rather than the 0.30 that stood here. Two
// reasons, and the second is why it moved rather than just changing value:
// retuning it must not recompile shaders (this zone's rule), and the two arms
// of its own measurement have to come out of ONE BINARY (Rule 47) — with a
// #define they would be two builds an edit apart, and this tree has seven other
// agents editing it. DFN_MOON_GROUND is the dose; the default is derived below.
//
// MEASURED, at the player's eye (1.7 m over the treeline vantage), midnight,
// the moon on the meridian at 32.5 deg — ground rows only, mean luma of 255:
//     new moon  (moon_light 0)   7.01
//     full moon (moon_light 1)  16.73   -> the moon's own contribution 9.72
// and 9.72 of 255 is 0.49 of ONE PALETTE_SHADE_STEP_REF (0.0784 = 19.99).
// So a FULL MOON WAS HALF A SHADE STEP BRIGHTER THAN NO MOON AT ALL. This
// project's own rule (NUMBERS, SUN_GLARE_LUMA_MAX) is that one step IS the
// quantiser's cell and a one-step difference may round into the same palette
// entry, so a claim needs TWO. The moon was a factor of four under the
// threshold of being visible at all, which is «ночью темно и вообще ничего не
// видно» in the units this project measures light in.
//
// The default is that requirement solved for the gain: two steps of separation
// = 39.98 luma where 0.30 bought 9.72, i.e. 0.30 * 39.98/9.72 = 1.234. It is
// a number the frame HANDS BACK, not one picked to look right.
//
// AND THE GROUND HAD NO SHAPE, WHICH IS THE STRONGER HALF OF THE REPORT: the
// ground rows spanned p10 13.13 to p90 19.98, i.e. 0.34 of a shade step from
// the darkest tenth to the brightest. Under the palette that is ONE ENTRY —
// the moonlit ground was a single flat colour, and no amount of staring at it
// resolves a slope. After: p10 33.69, p90 55.43, 1.09 steps of internal
// contrast.
//
// NOT THE AMBIENT FLOOR, and the decomposition says why: of the 16.73 a full
// moon leaves on the ground, 9.72 is the moon and 7.01 is the night ambient.
// The moon is already the larger half — it is not missing, it is small, and so
// is everything else at night. Raising the FLOOR would brighten moonless nights
// too, which is the opposite of what a moon is for (and the floor is the
// light zone's). This term is zero at new moon by construction, so a moonless
// night is byte-identical before and after.
#define u_moonGround (u_envParams[39].w)

// THE FOLIAGE EDGE FADE (slot 40): the band over which a leaf card is dissolved
// as it turns edge-on to the eye, in |dot(N, V)|. A LIVE KNOB and not a shader
// #define because its right value is a LOOK judgement flora has to make against
// real frames, and the first guess was already wrong in a way no reasoning
// would have caught: at 0.08/0.22 a spruce's near-horizontal fronds read ~0
// from eye level over their whole area and the tree stood BARE. Rebuilding the
// shader per guess would have made that loop cost minutes instead of seconds.
// Doors: DFN_FOLIAGE_EDGE_LO / DFN_FOLIAGE_EDGE_HI. lo >= hi disables the fade
// entirely, which is the control arm out of the same binary (Rule 47).
#define u_foliageEdgeLo (u_envParams[40].x)
#define u_foliageEdgeHi (u_envParams[40].y)
// Доза горизонтного свечения неба (DFN_SKY_GLOW, 0 = прежний двухстопный
// градиент бит-в-бит). Свободная компонента слота 40 — заведена ПАРОЙ с
// упаковкой в BgfxRendererFrame.cpp, как того требует контракт слоя.
#define u_skyGlowDose   (u_envParams[40].z)
// Доза мелкого дизера (8x8 Байер против прежних 4x4; 0 = 4x4 бит-в-бит).
// Живёт здесь, потому что порог читают и террейн, и path — двум шейдерам
// нельзя дать разойтись в решётке на общей кромке.
#define u_ditherFine    (u_envParams[40].w)

// Surface lighting shared by terrain and props, so night, moonlight and the
// carried torch can never disagree between them.
//   sun_vis: sun shadow-map visibility (dfn_shadow_factor), 1 = unshadowed.
//   sky_vis: how much sky the surface sees, 0 = sealed interior, 1 = open.
//            Voxel meshes carry it in vertex ALPHA (core writes it at build
//            time); surfaces without the data pass 1.0.
vec3 dfn_surface_light(vec3 wpos, vec3 n, float sun_vis, float sky_vis)
{
    // Ambient is SKY light: an enclosed volume must not receive it, which is
    // what stops caves from reading as flatly daylit. Two independent terms
    // gate it — the GEOMETRIC one (sky_vis, from the voxel mesh) and the
    // AUTHORED one (u_ambientDarkness, from the darkness zone the player is
    // in). Geometry cannot express "this place is unnaturally dark", and
    // authoring should not have to describe a cave's shape.
    float dark = clamp(u_ambientDarkness, 0.0, 1.0);
    float sky = sky_vis * (1.0 - dark);
    // THE FILL HAS A DIRECTION, AND WITHOUT ONE THE SHADOW SIDE HAS NO SHAPE.
    // Every term below this line is gated on the sun, the moon or a lamp, so in
    // the shadow half-space by day the whole function used to reduce to
    // `u_ambientColor * sky` — a constant with no surface normal in it. Measured
    // consequence, on a bole standing in its own canopy's shadow: p90/p10 of
    // luma across 228 pixels of bark = 1.01x, sixteen distinct tones, i.e. the
    // bark texture times a number. The user's words for it were "как чёрное
    // пятно ... она должна быть темнее переда, но цвет одинаковый".
    //
    // Two directions, because they answer two different shapes:
    //   n.y            — the sky above outshines the ground below. This is what
    //                    gives a boulder or a crown a top and a bottom.
    //   dot(n, sunDir) — the sky is brightest around the sun and the sunlit
    //                    ground bounces from that side. This is the only one of
    //                    the two that a VERTICAL bole can use, its normals being
    //                    horizontal, and it is what turns a black stick back
    //                    into a cylinder.
    //
    // THE SHAPE OF THE TWO TERMS IS SET BY ONE REQUIREMENT: NEITHER MAY DARKEN
    // THE THING THE USER IS COMPLAINING ABOUT. He is looking at trees that read
    // as absences — "словно их нет" — so a fill that adds form by taking light
    // away answers the letter of the complaint and not one word of its point.
    // Two earlier shapes were built and measured and both failed on exactly
    // that, which is why they are written down instead of quietly replaced:
    //
    //   1 + up*n.y + sun*dot(n,s)          zero-mean over a SPHERE of normals,
    //   and I claimed it therefore could not move the frame mean. A frame is
    //   not a sphere of normals, it is mostly GROUND and ground faces up:
    //   whole-frame mean 84.81 -> 87.27, open ground 88.90 -> 98.18. A 10 %
    //   brightening of the world under a claim of preservation.
    //
    //   the same over fill(up)             fixes that by referencing the
    //   surface u_ambientColor was calibrated on — and then every normal that
    //   is NOT up can only lose: the bole went 22.18 -> 19.42 mean and the
    //   crown 68.21 -> 59.72, i.e. 12 % darker, while the bole's p90/p10 moved
    //   1.01x -> 1.05x. Physically defensible, and backwards for the defect.
    //
    // What is here now cannot darken an up-facing surface OR a vertical one:
    //   min(n.y, 0.0)          only UNDERSIDES lose light. That is the sky-over-
    //                          ground cue, and an overhang is the one place it
    //                          should read as shade.
    //   dot(n, sun_horizontal) the sun's AZIMUTH only, its vertical component
    //                          removed. On the ground this is exactly 0, so the
    //                          calibration surface does not move at all; around
    //                          a bole it sweeps +-u_fillSun and averages to
    //                          zero, so the trunk gains a lit side and a dark
    //                          side without losing a single luma of its mean.
    //
    // The clamp is not decoration: a hand-set DFN_FILL_* can exceed 1, and a
    // negative fill would make the ambient SUBTRACT light.
    vec3 sun_h = vec3(u_sunDir.x, 0.0, u_sunDir.z);
    float sun_h_len = length(sun_h);
    sun_h = sun_h_len > 1e-4 ? sun_h / sun_h_len : vec3(0.0, 0.0, 0.0);
    float fill = 1.0 + u_fillUp * min(n.y, 0.0) + u_fillSun * dot(n, sun_h);
    vec3 light = u_ambientColor * (sky * max(fill, 0.0));
    // ОТСКОК ОТ ЗЕМЛИ (G3, 22.08, приёмка [N9]: испод навеса — сплошной
    // чёрный лист). Нижняя полусфера теряла свет ДВАЖДЫ — fillUp гасит
    // min(n.y, 0), запечённое AO прижимает к полу 0.30, — а в жизни её
    // кормит освещённая земля. Член добавляет НИЖНИМ граням долю солнца,
    // отражённую землёй; верхним (max(-n.y, 0) = 0) не даёт ничего, среднее
    // кадра не двигает — кадр в основном земля. Ворота от утечки в
    // запечатанный интерьер: гейт по sky_vis — потолок закрытой комнаты
    // сидит на полу AO (0.30) и получает ноль, испод уличного навеса с
    // открытыми боками (AO выше пола) — долю. Затенение облаками — то же,
    // что у прямого солнца: отскок это его отражение, а не второй источник.
    // u_hemiBounce — доза (DFN_AMBIENT_HEMI, 0 = прежний кадр бит-в-бит).
    // Ворота узкие нарочно: запечатанный интерьер сидит РОВНО на полу AO
    // (0.30) и обязан получить ноль, а испод уличного навеса с открытыми
    // боками живёт на 0.4-0.6 — широкая рампа (until 22:20 — /0.70) душила
    // его до долей процента, и A/B из 3.7 млн пикселей сдвинул 347.
    // Приёмка круга 4 поймала ворота МЁРТВЫМИ на рыночном навесе (|dRGB|
    // 0.003 между дозами 0 и 0.22): его испод сидит на 0.30..0.34, и порог
    // 0.32 съедал эффект целиком. Порог прижат вплотную к полу AO (0.302 —
    // ровно запечатанный интерьер, обязан остаться нулём), рампа короткая.
    float bounce_gate = clamp((sky_vis - 0.305) / 0.08, 0.0, 1.0);
    light += u_sunColor * (u_hemiBounce * max(-n.y, 0.0) * bounce_gate
                           * dfn_cloud_sun_vis(wpos));
    // Cloud shadow (W4): the same coverage field the sky draws, projected
    // along the sun. Lives HERE so every surface-lit thing — terrain, props,
    // foliage — darkens under the same crawling shadow (Rule 32).
    light += u_sunColor * (max(dot(n, u_sunDir), 0.0) * sun_vis
                           * dfn_cloud_sun_vis(wpos));
    // Moonlight: directional and unshadowed — the shadow map belongs to the
    // sun, and a second cascade for the moon is not worth the frame.
    light += u_moonColor * (u_moonLight * u_moonGround
                            * max(dot(n, u_moonDir), 0.0) * sky);
    // Point lights (torch, braziers, lit windows). Radius 0 = off.
    //
    // WHAT STOOD HERE, AND WHY IT IS GONE. `reach = 1.0 - 0.55 * dark` — the
    // darker the place, the SHORTER every lamp, defended by "лишь мелкий
    // клочок". The registry row it fought predicted this exact reading and
    // rejected it in advance: TORCH_RADIUS_DARK is the MINIMUM useful radius
    // IN THE DARK — «освещает лишь клочок» — атмосфера, «не видно собственных
    // ног» — проблема управления. Measured on the feet0 box (one instrument,
    // FINDING_DUNGEON_DARK): with the shortening plus quadratic falloff the
    // floor 2.79 m from a burning flame read 4.49 luma against a palette step
    // of 19.99 — a quarter of the threshold of visibility, i.e. the contract
    // was unmet at HALF the contract distance. The dark place stays dark
    // because ambient is gated to zero there, not because the lamp is broken.
    //
    // The falloff is smoothstep on the same linear window, not atten², for a
    // measured reason: with atten² a nominal radius of 9 m cannot reach one
    // palette step at the 4 m contract distance at all (it needs R >= 11.7 by
    // the same box's own normalisation), while smoothstep clears it with
    // margin and still lands at zero slope at the radius edge.
    for (int i = 0; i < DFN_MAX_LIGHTS; ++i)
    {
        if (float(i) >= u_lightCount) {
            break;
        }
        vec4 pos_rad = u_lightPosRad(i);
        vec3 to_light = pos_rad.xyz - wpos;
        float dist = length(to_light);
        float atten = clamp(1.0 - dist / max(pos_rad.w, 0.0001), 0.0, 1.0);
        // Shadow-casting lights are packed FIRST (BgfxRenderer orders them), so
        // the slot index IS the cube-atlas index and no second lookup table
        // exists to fall out of sync.
        float occl = 1.0;
        if (float(i) < u_pointShadowParams.x && atten > 0.0) {
            occl = dfn_point_shadow_factor(i, wpos, n, pos_rad.xyz, pos_rad.w);
        }
        // ИНТЕРЬЕРНЫЙ СВЕТ НЕ ПРОБИВАЕТ КЛАДКУ (22.08). Теневой слот есть
        // только у двух ближайших источников; остальные жгут occl = 1.0
        // сквозь любую стену, и очаг дома светил улице. Флаг interior (бит 2
        // в w цвета) гейтит источник НЕБЕСНОЙ ВИДИМОСТЬЮ ПРИЁМНИКА: уличная
        // земля (sky_vis = 1) не получает ничего, пол в доме (sky_vis -> 0,
        // запечённое AO) — всё. Различение появилось вместе с AO построек —
        // до него у фрагмента не было понятия «я внутри». Остаточная утечка
        // «очаг дома A в интерьер дома B в паре метров» ослаблена затуханием
        // и признана в плане; уличные жаровни флаг не ставят.
        // МЯГКОСТЬ ИСТОЧНИКА (владелец 23.08: «свет от фонарей скудный,
        // рисуется неправильно — нужны разнообразные источники с разной
        // яркостью и мягкостью»). Дробная часть w цвета — softness 0..1
        // (целая часть остаётся битами флагов): мягкий свет ОГИБАЕТ форму
        // (wrap-диффуз — терминатор не режет цилиндр столба пополам) и
        // затухает ПОЛОЖЕ (степень окна 1 -> 0.6). soft = 0 не трогает ни
        // одной операции — прежний кадр бит-в-бит; доза DFN_LIGHT_SOFT
        // (слот 13.w, 0 = мягкость игнорируется целиком).
        float soft = fract(u_lightColor(i).w) * (1.0 / 0.9) * u_lightSoftDose;
        float ndl = max(dot(n, to_light / max(dist, 0.0001)), 0.0);
        float fall = atten * atten * (3.0 - 2.0 * atten);
        if (soft > 0.01) {
            float wrapv = 0.5 * soft;
            ndl = max((dot(n, to_light / max(dist, 0.0001)) + wrapv)
                          / (1.0 + wrapv), 0.0);
            fall = pow(fall, mix(1.0, 0.6, soft));
        }
        // ИЗОТРОПНАЯ СОСТАВЛЯЮЩАЯ (круг 6: «фонарь не светит ни себе, ни
        // столбу», «жаровня — чёрный столб при горящей чаше», «сбоку короб
        // гаснет»). Чистый NdL — модель ДАЛЬНЕГО света; вплотную к пламени
        // поверхность ловит свет со всех сторон переотражением и рассеянием,
        // а её нормаль почти перпендикулярна лучу — dot душил в ноль ровно
        // то, что глаз ждёт горящим. Ненаправленная доля растёт с мягкостью
        // (стекло фонаря — само рассеяние) и есть даже у резкого огня.
        // Гейты (interior/коробка) умножают её же — сквозь кладку не течёт
        // больше прежнего. Доза DFN_LIGHT_ISO, 0 — прежний кадр бит-в-бит.
        if (u_lightIsoDose > 0.01) {
            ndl = ndl + (0.10 + 0.25 * soft) * u_lightIsoDose * (1.0 - ndl);
        }
        float gate = 1.0;
        if (u_lightColor(i).w >= 2.0) {
            vec4 room = u_lightRoom(i);
            if (u_lightRoomGate > 0.5 && (room.z > 0.0 || room.w > 0.0)) {
                // КОРОБКА КОМНАТЫ (23.08): свет принадлежит помещению, а не
                // радиусу. Гейт по AO оставался течью 6.9% на наружной кладке
                // (AO не отличает «в доме» от «в тени стены»); окно в плане с
                // мягкой кромкой 0.3 м режет класс целиком. ЗАМЕНЯЕТ гейт по
                // sky_vis, а не умножается на него: двойное наказание душило
                // бы законный свет у дверного проёма.
                vec2 dxz = abs(wpos.xz - room.xy) - room.zw;
                float outside = max(dxz.x, dxz.y);
                gate = mix(1.0, clamp(1.0 - outside / 0.3, 0.0, 1.0),
                           u_lightInteriorGate);
                light += u_lightColor(i).rgb * (fall * occl * gate * ndl);
                continue;
            }
            // DFN_GATE_RECEIVER: чем фрагмент отвечает гейту. По умолчанию —
            // тем же sky_vis, что кормит ambient. ЛИСТВА переопределяет его
            // единицей (fs_foliage): её v_color0.a — затенение кроны, не
            // замкнутость помещения, и пучок в тени наружной стены читался
            // интерьером ровно там, где за стеной горит очаг (приёмка гейта:
            // maxch 71 на траве у таверны). Флора законным интерьерным
            // приёмником не бывает — трава в доме это баг, а не случай.
            gate = mix(1.0, clamp(1.0 - DFN_GATE_RECEIVER(sky_vis), 0.0, 1.0),
                       u_lightInteriorGate);
        }
        light += u_lightColor(i).rgb * (fall * occl * gate * ndl);
    }
    return light;
}

// THE SKY'S OWN GRADIENT, as a function, because TWO things draw it now: the
// sky itself (fs_sky) and the air in front of every surface (dfn_aerial). If
// they were written twice a distant ridge would melt into a colour the sky
// above it does not have, which is the seam this whole file exists to avoid.
vec3 dfn_sky_gradient(vec3 dir)
{
    float up = clamp(dir.y, 0.0, 1.0);
    // ТРЕТИЙ СТОП — ГОРИЗОНТНОЕ СВЕЧЕНИЕ (22.08, приёмка [11]: нижние 5°
    // неба держали 175.8..175.4 люма на 48 строк кадра — двухстопный mix у
    // горизонта насыщается по построению, pow(1-up,3) там всюду ~1 и лента
    // мертва; это же записано в docs/specs/render.md DEFERRED п.3). Узкий
    // тёплый подъём в нижние ~6°: 1-up*9 зануляется выше 0.11 по y, квадрат
    // делает край мягким. Тёплая доля лежит В ЦВЕТЕ, а не в отдельной
    // палитре: свечение — то же небо у земли, гуще и теплее, и dfn_aerial
    // рисует дальнюю гряду ровно этим же цветом — шов невозможен по
    // построению. DFN_SKY_GLOW — доза, 0 = прежний градиент бит-в-бит.
    vec3 base = mix(u_skyZenith, u_skyHorizon, pow(1.0 - up, 3.0));
    float glow = pow(clamp(1.0 - up * 9.0, 0.0, 1.0), 2.0);
    vec3 glow_col = u_skyHorizon * vec3(1.10, 1.04, 0.95);
    return mix(base, glow_col, glow * u_skyGlowDose);
}

// AERIAL PERSPECTIVE (REFERENCE_FRAMES.md R1) — HOW MUCH OF A SURFACE SURVIVES
// THE AIR IN FRONT OF IT. Returns transmittance: 1 = nothing between us, 0 =
// the surface has become sky.
//
// WHY THIS IS NOT A FAR-PLANE FADE, MEASURED RATHER THAN ASSERTED. What stood
// here was smoothstep(u_fogStart, u_fogEnd, dist) with the span set to
// 0.30..0.85 of CAMERA_FAR, i.e. 2400..6800 m. The world is TESTBED_SIZE
// 1024 m across, so its longest sightline is a 1448 m diagonal and the factor
// was EXACTLY ZERO at every point of it — not weak, never once nonzero. The
// frame said so: the same crag shot at 250/500/900 m held its luma at
// 97.4/91.8/91.9 while its separation from the sky ROSE 12.1 -> 46.9, because
// the sky brightens toward the horizon and the crag did not. The picture was
// asserting the OPPOSITE of every reference frame. A smoothstep between two
// distances cannot be the fix at any setting: it is flat on both sides of its
// ramp by construction, and R1 asks for a fall that is continuous from the eye
// outward. See docs/specs/render.md §R1.
//
// The law is Beer-Lambert through air whose density falls off with height:
//   density(y) = exp(-max(y - HAZE_BASE_HEIGHT, 0) / HAZE_HEIGHT_SCALE)
//   transmittance = exp(-(mean density along the ray) * distance / SCALE_LENGTH)
//
// THE TWO HEIGHT ROWS ARE ONE MECHANISM AND THE BASE IS THE LOAD-BEARING HALF.
// Without it the density is anchored at y = 0, which is BELOW the ground the
// player walks on — and then shortening the height scale thins the valley's own
// air along with the summit's, so the lever pushes both ends the same way and
// there is no second lever at all. Anchored at the valley floor instead, with
// everything below it clamped to full density, the two rows finally separate:
// SCALE_LENGTH says how thick the air the player stands in is, HEIGHT_SCALE
// says how fast a mountain climbs out of it. That is the split reference frames
// 02 and 12 show as a hazed base under a lit crown.
// --- THE MIST BAND (R2) ----------------------------------------------------
// A horizontal layer of denser air with its own altitude, thickness and
// density. Reference frames 02, 04 and 12 all carry one, and it is what gives
// their clouds a sense of VOLUME: it intersects the terrain partway up, cutting
// a mountain into a lit crown and a hazed base. It costs a few lines here
// against a volumetric renderer for the same picture.
//
// WHY THIS PRODUCES A BAND AND NOT JUST MORE HAZE UP HIGH — the geometry is
// worth stating, because the naive expectation is wrong. For an eye BELOW the
// layer, the optical depth to a surface RISES as the surface climbs through the
// layer, peaks when the surface sits at the layer's TOP (the ray has just
// crossed the whole layer, and it crossed at the shallowest angle any such ray
// will), and then FALLS again for anything higher, because a higher surface is
// seen at a steeper angle and a steeper ray spends less length inside a
// horizontal slab. So the profile is: clear base, a bright stripe at the
// layer's top edge, a relatively clearer crown. That is the reference picture,
// and it emerges from the geometry rather than being painted on.
//
// The profile in altitude is a trapezoid — plateau half the total extent, a
// ramp of a quarter on each side — and the ramps are smoothstep, whose
// antiderivative is closed form. So the mean over a ray is exact here too, the
// same way the exponential term is exact, and neither needs ray marching.
float dfn_mist_profile(float y)
{
    float T = max(u_mistThickness, 1.0);
    float lo0 = u_mistHeight - T * 0.5;
    float lo1 = u_mistHeight - T * 0.25;
    float hi0 = u_mistHeight + T * 0.25;
    float hi1 = u_mistHeight + T * 0.5;
    return smoothstep(lo0, lo1, y) * (1.0 - smoothstep(hi0, hi1, y));
}

// Antiderivative of the profile, zero below the layer. The smoothstep piece
// integrates to r*(t^3 - t^4/2), which reaches half the ramp width at t = 1 —
// the check that keeps the pieces joined.
float dfn_mist_integral(float y)
{
    float T = max(u_mistThickness, 1.0);
    float r = T * 0.25;
    float lo0 = u_mistHeight - T * 0.5;
    float lo1 = u_mistHeight - r;
    float hi0 = u_mistHeight + r;
    float hi1 = u_mistHeight + T * 0.5;
    if (y <= lo0) {
        return 0.0;
    }
    if (y <= lo1) {
        float t = (y - lo0) / r;
        return r * (t * t * t - 0.5 * t * t * t * t);
    }
    float base = 0.5 * r;               // the whole rising ramp
    if (y <= hi0) {
        return base + (y - lo1);
    }
    base += (hi0 - lo1);                // the plateau
    if (y <= hi1) {
        float u = (y - hi0) / r;
        return base + (y - hi0) - r * (u * u * u - 0.5 * u * u * u * u);
    }
    return base + 0.5 * r;              // the whole falling ramp
}

float dfn_aerial_transmittance(vec3 eye, vec3 wpos)
{
    float L = max(u_hazeScale, 1.0);
    float H = max(u_hazeHeight, 1.0);
    float B = u_hazeBase;
    float d = length(wpos - eye);

    // Order the endpoints: the mean over a segment does not care which way it
    // is travelled, and one ordering removes every sign case below.
    float ya = min(eye.y, wpos.y);
    float yb = max(eye.y, wpos.y);
    float span = yb - ya;

    float mean;
    if (span < 1.0) {
        // Level ray: the whole segment sits at one height. Also the limit of
        // the branch below, so there is no seam along the horizon — which is
        // exactly where a seam would be seen.
        mean = exp(-max(0.5 * (ya + yb) - B, 0.0) / H);
    } else {
        // EXACT, not approximated, and the split is why. The clamped density
        // is not exponential along the whole ray — it is flat below B and
        // exponential above — so feeding clamped endpoints to one exponential
        // formula understates a ray that starts below B (measured ~10 % at the
        // massif vantage). Splitting the segment at the crossing costs three
        // lines and removes the question.
        float c = clamp(B, ya, yb);
        float below = (c - ya) / span;        // density is exactly 1 in here
        float above = 0.0;
        if (yb > B) {
            float a = max(ya, B);
            above = (H / span) * (exp(-(a - B) / H) - exp(-(yb - B) / H));
        }
        mean = below + above;
    }

    // The mist band rides ON TOP of the haze, in the same units (multiples of
    // the ground air), so one exponential still carries both. Adding it as a
    // second mix() would double-count the in-scatter and wash the frame.
    if (u_mistDensity > 0.0) {
        float mist = span < 1.0
            ? dfn_mist_profile(0.5 * (ya + yb))
            : (dfn_mist_integral(yb) - dfn_mist_integral(ya)) / span;
        mean += u_mistDensity * mist;
    }
    return exp(-mean * d / L);
}

// The whole of R1 in one call: a lit surface colour, put behind the air that
// is actually between it and the eye. The colour it fades INTO is the sky in
// the direction we are looking, never one flat fog colour, so a ridge high in
// the frame and a ridge on the horizon each melt into the sky that is behind
// THEM.
vec3 dfn_aerial(vec3 wpos, vec3 lit)
{
    // СКОТОПИЧЕСКАЯ НОЧЬ (круг 6, топ-2: «трава держит полную дневную
    // зелень и остаётся единственным цветным пятном в кадре»). Умножение
    // альбедо на серо-синий лунный свет сохраняет ОТТЕНОК — газон ночью
    // зелен, как днём, чего глаз не делает: палочки не различают цвет.
    // Тёмное десатурируется к люме (до 65% при дозе 1), ЯРКОЕ сохраняет
    // цвет (мезопика: пламя, стёкла фонарей и пятна у огня остаются
    // тёплыми — без списка исключений по материалам). Гейт — u_moonLight:
    // днём ноль операций над цветом, доза DFN_NIGHT_SCOTOPIC 0 — прежний
    // кадр бит-в-бит.
    float scot = u_nightScotopic * clamp(u_moonLight, 0.0, 1.0);
    if (scot > 0.001) {
        float lum = dot(lit, vec3(0.2126, 0.7152, 0.0722));
        float keep = smoothstep(0.10, 0.45, lum);
        lit = mix(vec3_splat(lum), lit,
                  mix(1.0 - 0.65 * scot, 1.0, keep));
    }
    vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 to = wpos - eye;
    float t = dfn_aerial_transmittance(eye, wpos);
    return mix(dfn_sky_gradient(normalize(to)), lit, t);
}

// The same number as a 0..1 "how gone is it" factor, for the few call sites
// that need to fade something else with it (water alpha).
float dfn_aerial_factor(vec3 wpos)
{
    vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    return 1.0 - dfn_aerial_transmittance(eye, wpos);
}

#endif // DFN_ENV_SH
