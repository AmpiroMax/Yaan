/*
Created: 09:08:2026 - 00:06:00
Last updated: 23:08:2026 - 17:59:57
Module: engine/platform/render
File: engine/platform/render/interfaces/IRenderer.h

Responsibility:
- The platform rendering contract (Rule 0). Everything the engine and game may ask
  of a renderer goes through this interface; bgfx lives only behind it.

Key items:
- IRenderer: init/shutdown, frame, resource creation, submission, screenshot capture.
- RendererInitParams: window handle + framebuffer size + low-res internal target (Q9).
- MeshHandle / TextureHandle / ProgramHandle: opaque POD handles (0 = invalid).
- Vertex: the fixed vertex layout for stage 2 (position, normal, uv, color).

Dependencies:
- Uses: C++ stdlib, glm (project-wide math vocabulary, Rule 2). Nothing else.
- Used by: engine/render (primary consumer), engine/editor, engine/app, tests
  (null backend), the screenshot tour.

Notes:
- FROZEN CONTRACT (Rule 26): authored by the lead (Q55). Changes only through a
  group sync — message the lead, do not edit.
- Deliberately thin: materials, culling, batching, lighting and the camera live in
  engine/render on top of this. The backend owns the low-res internal target and
  the integer upscale (Q9); consumers never see it.
- Skinned meshes (ozz skinning matrices) are a stage-3 extension; the contract will
  gain create_skinned_mesh + submit overload at the next sync, not ad-hoc.
- Null backend: all methods succeed, handles are valid-but-inert, save_screenshot
  writes nothing and returns false.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add bgfx types, includes, or assumptions to this header.
*/
/*
UPD:
- 09:08:2026 - 00:06:00: Initial frozen contract for stage 2 (skeleton walk).
- 09:08:2026 - 10:48:00: Stage-3 sync (Rule 26, render's batch approved):
                         RenderEnvironment + set_environment (atmosphere, splat,
                         water params as uniforms); palette_post init flag (Q9b).
- 09:08:2026 - 18:56:38: Night sky + one carried point light appended to
                         RenderEnvironment (render's diff, lead-authored per
                         Rule 26): moon direction/colour/phase/light, star
                         intensity, torch position/colour/radius. Pure
                         addition — no field changed or reordered, both
                         backends keep compiling.
- 09:08:2026 - 19:09:18: Point LIGHT ARRAY replaces the single point light
                         (user wants shadows from several sources) +
                         ambient_darkness for authored pitch-black places.
                         Render's diff, lead-authored per Rule 26.
- 09:08:2026 - 19:21:01: Deprecated single-point-light fields deleted now
                         that render's backend and tests use the array.
- 09:08:2026 - 20:01:56: Wind (direction/strength/flutter) for foliage, grass
- 09:08:2026 - 21:01:17: DrawParams — per-draw material parameters (fade,
                         highlight, two reserved). Render's diff, lead-authored
                         per Rule 26; the four-argument submit stays as a
                         convenience so no existing call site changes.
                         and cloth — one wind for the world. Render's diff,
                         lead-authored per Rule 26.
- 10:08:2026 - 02:56:25: Weather cloud slice: six additive RenderEnvironment fields (render's diff, Rule 26 sync). Defaults = the scattered state; cloud_offset_m is the ONE drift both sky and ground shadow read.
- 13:08:2026 - 19:05:00: RenderEnvironment::cloud_deck_m — the three cloud deck
                         ALTITUDES (render's diff, Rule 26 sync). They were
                         shader #defines; the user asked for the ceiling's
                         height to be a FIELD with a legal range, because that
                         is how the weather and climate of a place will be read
                         off the sky. The DEFAULT is the shipped R3.2 ladder
                         1500/2600/4400, so this field is its own zero-dose
                         control: a caller that never writes it gets the sky
                         that shipped, byte for byte.
- 10:08:2026 - 23:32:21: RendererInitParams::msaa_samples — число выборок покрытия на внутренней цели как ПОЛЬЗОВАТЕЛЬСКАЯ настройка (синк №3), а не переменная окружения.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 22:28:39: DrawParams::casts_in_point_shadows -- stand-ins and light
  holders opt out of the carried-light cube pass (see the field's comment for
  the measured defect: floor at 2.79 m from a burning sconce read 0 of 255).
- 14:08:2026 - 16:35:53: DEBUG / EDITOR INTROSPECTION SYNC (В28, render's diff at
  the lead's direction per Rule 26 — a devlog entry is owed in docs/devlog/,
  which is lead-owned). Three additive hooks for the editor's debug overlays,
  none of which changes or reorders an existing field, so both backends keep
  compiling and every existing call site is untouched:
    1. RenderFrameStats + frame_stats(): per-frame draw/triangle counters.
       HONEST about bgfx — it reports draw calls (numDraw) but NOT a primitive
       count, so triangles are summed CPU-side from index counts (see the
       struct's own comment).
    2. set_wireframe(bool): whole-scene wireframe, default off, zero cost off.
    3. RenderPick + center_pick() + DrawParams::pick_id: a CPU centre-of-screen
       ray pick (variant A) against the per-draw bounding spheres the backend
       already keeps; returns the submitter's stamped id, the drawn mesh and its
       (selected-LOD) triangle count. Pure addition to DrawParams's tail.
- 15:08:2026 - 15:23:22: DrawParams::aux_texture — второй материальный лист на дро (сейчас
  нормали коры, запрос зоны flora). Аддитивно к замороженному контракту
  (правило 26): пустой по умолчанию, никто, кроме просящего, не платит.
- 17:08:2026 - 18:29:30: set_debug_lines — дверь линий открывается В РАНТАЙМЕ: призрак решает нужны ли
  они по нажатию клавиши, а переменная окружения читается один раз при запуске.
- 17:08:2026 - 19:17:13: native_texture_handle() — имя текстуры В ТЕРМИНАХ БЭКЕНДА, 0xFFFFFFFF = «не знаю». Единственная намеренно дырявая точка контракта, и она узкая: интерфейс редактора рисует ВТОРОЙ библиотекой (Dear ImGui), чей мост к bgfx лежит рядом с этим бэкендом, и они обязаны уметь говорить об одной и той же текстуре — иначе миниатюру детали, нарисованную во внеэкранную мишень, невозможно показать в меню. Добавление с телом по умолчанию (правило 26): нулевой бэкенд и все двойники в тестах компилируются без правок и отвечают «не знаю».
- 18:08:2026 - 12:51:26: set_present_rect_norm() — куда на экране садится картинка мира.
  Заказ 18.08: «пусть инструмент рисуется не поверх игрового экрана... пусть
  игра ниже рисуется, тогда проблем с наложением не будет». Лечит ПРИЧИНУ:
  полосу и оверлеи разводили ДОГОВОРЁННОСТЬЮ — каждый рисующий сам спрашивал,
  сколько занято сверху, и сам отступал. Такую договорённость соблюдают все,
  пока не появится тот, кто о ней не знает; в этом проекте он появлялся трижды.
  Если мир физически не заходит под полосу, накладываться нечему.
- 22:08:2026 - 21:00:00: PointLight.interior (только добавление, правило 26): гейт интерьерного света небесной видимостью приёмника — 6 из 8 источников без теневого слота светили сквозь стены (occl = 1.0), волна убранства (24 очага) оживила спящий дефект.
- 23:08:2026 - 00:30:00: DrawParams.aux2_texture (стадия 5, путевой атлас террейна) и
  RenderEnvironment.path_tiles_per_m — оба ТОЛЬКО ДОБАВЛЕНИЯ (правило 26).
- 23:08:2026 - 01:40:00: PointLight.room_center_xz/room_half_xz — коробка комнаты интерьерного
- 22:08:2026 - 22:51:38: PointLight.softness — мягкость источника 0..1 (wrap-диффуз + пологое затухание, дробная часть w цвета в envParams; только добавление).
- 22:08:2026 - 23:48:18: DrawParams.aux3_texture (стадия 6, маска троп) и RenderEnvironment.path_mask_* — тропа из фрагмента; только добавления.
- 23:08:2026 - 17:59:57: MAX_POINT_LIGHTS 8 -> 16 — свет города горит на удалении (заказ владельца 24.08); слоты первых восьми прежние.
  света (свет принадлежит помещению, а не радиусу; гейт по AO оставлял течь
  6.9% на наружной кладке). Только добавление.
*/

#pragma once

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <span>
#include <string>
#include <string_view>

namespace dfn::platform {

// Opaque resource handles. id == 0 means "invalid / none".
struct MeshHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct TextureHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct ProgramHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};

enum class TextureFormat : uint8_t {
    RGBA8,   // 4x uint8, sRGB sampling decided by the backend
    R8,      // single channel (masks, heightmap debug views)
};

// Fixed vertex layout for stage 2. Extended (skinning) variants arrive via a
// contract sync, not by mutating this struct.
struct Vertex {
    glm::vec3 position;   // meters, world or model space depending on submit
    glm::vec3 normal;     // unit
    glm::vec2 uv;         // 0..1
    uint32_t color_rgba;  // 0xAABBGGRR packed, white = 0xFFFFFFFF
};

struct RendererInitParams {
    void* native_window_handle = nullptr; // backend-specific (from IWindow)
    uint32_t framebuffer_width = 0;       // real window framebuffer, pixels
    uint32_t framebuffer_height = 0;
    uint32_t internal_width = 0;          // low-res internal target (Q9); the
    uint32_t internal_height = 0;         // backend integer-upscales to the framebuffer
    bool vsync = true;
    // Coverage samples on the INTERNAL target. A user graphics setting of the
    // same class as internal_resolution and palette (sync #3), so it lives in
    // settings.cfg; 0/1 = off, 2/4/8 = MSAA. It is what stopped the running
    // shimmer at the treeline (0.094% -> 0.004%), so lowering it is a real
    // visual regression and not just a performance dial.
    uint32_t msaa_samples = 4;
    bool palette_post = false;            // Q9b: quantize the final image to a fixed
                                          // palette inside the upscale pass
};

// One point light. `casts_shadow` is honoured for the first
// MAX_SHADOW_POINT_LIGHTS lights that request it; the rest light without
// shadowing.
struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{0.0f};     // linear; black = off
    float radius_m = 0.0f;     // 0 = off
    bool casts_shadow = false;
    // ИНТЕРЬЕРНЫЙ свет: шейдер гейтит его небесной видимостью приёмника —
    // очаг без теневого слота иначе светит улице сквозь кладку (22.08).
    // ДОБАВЛЕНИЕ к контракту (правило 26, только рост): существующие
    // заполнители получают false и прежний кадр бит-в-бит.
    bool interior = false;
    // КОРОБКА КОМНАТЫ интерьерного света (23.08): свет принадлежит помещению,
    // а не радиусу — приёмка гейта по AO намерила течь наружной кладки 6.9%
    // (AO не отличает «в доме» от «в тени стены»). Прямоугольник в плане,
    // мягкая кромка в шейдере; нулевые полуразмеры = коробки нет, для
    // interior-света тогда работает прежний гейт по sky_vis. Y отброшен
    // сознательно: комнаты не штабелируются, вертикаль режет затухание.
    glm::vec2 room_center_xz{0.0f};
    glm::vec2 room_half_xz{0.0f};
    // МЯГКОСТЬ источника 0..1 (23.08, заказ владельца: «разнообразные
    // источники с разным уровнем яркости и мягкости»). 0 — прежний резкий
    // профиль бит-в-бит; 1 — свет огибает форму (wrap-диффуз) и затухает
    // положе. Реализация — dfn_env.sh; добавление к контракту (правило 26).
    float softness = 0.0f;
};

// 16 с 24.08 (владелец: «свет должен гореть всегда на любом удалении, а он
// только при приближении работает»): восьми ближайших не хватало городу с 75
// огнями — дальние фонари стояли тёмными, пока игрок не подойдёт. Слоты
// первых восьми в envParams прежние (16..31, 41..48), хвост 51..74 —
// только добавление (правило 26).
inline constexpr uint32_t MAX_POINT_LIGHTS = 16;
inline constexpr uint32_t MAX_SHADOW_POINT_LIGHTS = 2;

// PER-DRAW material parameters. Deliberately one small struct rather than a
// field per feature: the environment block is per FRAME, vertex alpha is
// worldgen's sky visibility, and the transform's unused row is not a place to
// smuggle a float through. At least three known consumers want exactly this —
// the LOD cross-fade (both levels of the same ground on screen at once,
// dithered against each other), the interaction highlight (HoverTarget already
// names the hovered entity and render has no way to draw it differently from
// its neighbours), and later damage flashes, magic glow and wetness. Inventing
// a special case per feature is how this ends up as three incompatible hacks.
struct DrawParams {
    float fade = 1.0f;      // 1 = fully drawn; screen-door dither below that
    float highlight = 0.0f; // 0 = none; interaction hover and similar
    float aux0 = 0.0f;      // reserved, meaning is the program's
    float aux1 = 0.0f;
    // False for STAND-IN geometry that disagrees with the fine world by
    // construction — the coarse far-terrain LOD is built without the carves,
    // so inside a tunnel it is solid rock through the very air the player
    // walks. Such a draw may never enter a carried light's shadow cube: a
    // torch only shadows within its few-metre radius, where the fine world is
    // resident by definition, so the stand-in can only ever ADD occluders
    // that do not exist. Found as "the whole frame reads 0 of 255 at 2.79 m
    // from a burning sconce": every texel of every cube face held the LOD
    // mountain's interior at centimetres from the flame.
    bool casts_in_point_shadows = true;
    // OPAQUE PICK ID for the centre-screen pick (В28, RenderPick). The submitter
    // stamps whatever it wants the crosshair overlay to name — an EntityId, an
    // object-type tag — and center_pick() hands the winner's value straight
    // back. 0 = "unnamed" (the default): the draw still participates in the pick
    // and reports its geometry, it just has no id to surface. Deliberately here
    // and not a submit() argument — DrawParams is the per-draw metadata bag, and
    // the header of this struct already argues that inventing a special case per
    // feature is how this ends up three incompatible hacks.
    uint32_t pick_id = 0;
    // SECOND MATERIAL SHEET for this draw — today the BARK NORMAL MAP (flora,
    // 15.08.2026: at 1920x1080 the furrows are read per pixel, and no vertex
    // colour can stand in for that). Empty by default, so nothing that does
    // not ask for it pays anything.
    //
    // WHY HERE AND NOT A SECOND submit() ARGUMENT: DrawParams is the per-draw
    // metadata bag by construction (see the pick_id note above), and a second
    // texture argument would have to be threaded through every call site in
    // the engine to serve one material.
    //
    // WHY NOT A TANGENT IN THE VERTEX: platform::Vertex is frozen and shared
    // by terrain, water, paths, bodies and flora alike — a tangent would cost
    // every mesh producer bytes for one material's benefit. The backend's
    // shaders build the basis from screen-space derivatives instead.
    TextureHandle aux_texture{};
    // ВТОРОЙ ДОПОЛНИТЕЛЬНЫЙ ЛИСТ (стадия 5) — путевой атлас для террейна:
    // клетки материалов полотна троп (COBBLE/PACKED_EARTH/SCUFFED/CUT_SLAB).
    // ДОБАВЛЕНИЕ к контракту (правило 26, только рост); невалидный хендл —
    // прежний кадр, бэкенд подставляет нейтральный лист ради Metal.
    TextureHandle aux2_texture{};
    /// МАСКА ТРОП МИРА (стадия 6): R — износ полотна, G — класс (0/85/170/255).
    /// Кромка полотна берётся из растеризации маски, а не из решётки вершин
    /// земли (круг 5: «шевроны» — 1.8-метровая тропа на 2-метровой сетке).
    /// Пустой хендл — прежняя вершинная альфа; только добавление (правило 26).
    TextureHandle aux3_texture{};
};

// ---- Debug / editor introspection (В28) -------------------------------------

// Per-frame draw statistics for the editor's debug overlay. Valid after
// end_frame; describes the LAST completed frame.
//
// HONEST ABOUT WHAT bgfx GIVES: bgfx::getStats() reports a DRAW-CALL count
// (numDraw, covering every view — sun + near cascades, the carried-light cube
// faces, sky, scene, the upscale) but it does NOT report a primitive/triangle
// count. So `scene_triangles` is summed CPU-side from the index counts of the
// meshes submitted into the SCENE view (exact for indexed meshes). That sum is
// the geometry the crosshair overlay is about and deliberately EXCLUDES the
// shadow re-draws of the same meshes; `backend_draws` is the honest all-views
// total straight from bgfx for when the whole GPU cost is the question.
struct RenderFrameStats {
    uint32_t scene_draws = 0;      // draws issued into the scene view this frame
    uint32_t scene_triangles = 0;  // triangles in those scene draws (indices / 3)
    uint32_t backend_draws = 0;    // bgfx total draw calls, ALL views/passes
};

// Result of the centre-of-screen pick (В28: "what is under the crosshair").
// Valid after end_frame; describes the LAST completed frame.
//
// VARIANT A: a CPU ray from the camera centre tested against the per-draw
// bounding spheres the backend already measures at create_mesh (the same
// spheres the shadow culls use), nearest hit wins. `mesh` is the exact geometry
// that was drawn, so `triangles` is the SELECTED LOD's triangle count — the LOD
// selection happens per frame at submit time and this reads it off the real
// draw. The LOD LEVEL LABEL (0/1/2…) is engine/render's to map from `pick_id`
// or `mesh`; the platform has no LOD vocabulary and does not invent one.
// Spheres are loose, so under heavy overlap this is approximate; an id-buffer
// readback (variant B) is the escalation if that is ever shown to mislead.
struct RenderPick {
    bool hit = false;
    uint32_t pick_id = 0;       // DrawParams::pick_id of the winning draw (0 = unnamed)
    MeshHandle mesh;            // the drawn mesh (== selected LOD's mesh)
    uint32_t triangles = 0;     // that mesh's triangle count
    float distance_m = 0.0f;    // ray distance to the sphere hit, meters
    glm::vec3 position{0.0f};   // world-space hit point
};

// Per-frame environment + shared material parameters (atmosphere, splat
// thresholds, water). Values come from engine/render (look-dev constants now,
// design-doc-driven later); the backend maps them to shader uniforms, so they
// are adjustable without recompiling shaders.
struct RenderEnvironment {
    glm::vec3 sun_direction{0.35f, 0.8f, 0.45f}; // TOWARD the sun, normalized
    glm::vec3 sun_color{1.0f};
    glm::vec3 ambient_color{0.35f};
    glm::vec3 fog_color{0.63f, 0.71f, 0.80f};    // == sky horizon for seamless blend
    float fog_start_m = 300.0f;
    // Повторов путевого атласа на МЕТР полотна (материал троп в земле, 22.08).
    // Едет из look-dev (PATH_TILES_PER_M) через окружение, а не зеркалом в
    // бэкенде: на зеркале 320/160 эта волна уже обожглась.
    float path_tiles_per_m = 0.45f;
    /// Маска троп: начало композиции в плане, спан в метрах (0 = маски нет)
    /// и разрешение маски в текселях. Шейдер получает их слотом 49.
    glm::vec2 path_mask_origin_xz{0.0f};
    float path_mask_span_m = 0.0f;
    float path_mask_res_px = 0.0f;
    float fog_end_m = 850.0f;
    glm::vec3 sky_zenith_color{0.25f, 0.42f, 0.66f};
    glm::vec3 sky_horizon_color{0.63f, 0.71f, 0.80f};
    // Terrain splat (slope = 1 - normal.y):
    float sand_height_m = 0.0f;   // full sand below this height
    float sand_blend_m = 2.0f;    // sand->grass fade band
    float rock_slope_start = 0.45f;
    float rock_slope_end = 0.75f;
    float terrain_tiles_per_chunk = 32.0f; // texture repeats per CHUNK_SIZE
    // Water:
    glm::vec4 water_color{0.16f, 0.30f, 0.34f, 0.62f}; // rgba, a = opacity
    glm::vec2 water_scroll_uv{0.02f, 0.013f};          // uv units / second
    float time_seconds = 0.0f;    // render-side visual time (not sim time)

    // Night sky. The app's clock drives all of these (render::apply_sky_time).
    glm::vec3 moon_direction{0.0f, -1.0f, 0.0f}; // TOWARD the moon, normalized;
                                                 // below the horizon = not drawn
    glm::vec3 moon_color{0.72f, 0.76f, 0.90f};   // disc colour (cold white)
    float moon_phase = 0.5f;      // 0 = new, 0.5 = full, wraps at 1.0
    float moon_light = 0.0f;      // 0..1 directional moonlight on the ground;
                                  // separate from moon_color so an overcast
                                  // night can dim the light, not the disc
    float star_intensity = 0.0f;  // 0 = day, 1 = clear night. Explicit rather
                                  // than derived from sun elevation: overcast
                                  // means stars off with the sun untouched

    // Point lights (torch, braziers, lit windows). The user asked for shadows
    // cast by SEVERAL sources, so the single light this replaced was outgrown
    // before it shipped. Cost of the shadowing half, measured by render: a
    // point light needs a CUBE map (6 faces), so casters in range are
    // submitted 6x more — affordable only because casters are culled to the
    // light's sphere (in a tunnel ~6 casters = ~36 extra draws), which is why
    // the shadow-casting RADIUS is capped rather than the honesty.
    PointLight point_lights[MAX_POINT_LIGHTS];
    uint32_t point_light_count = 0;


    // Wind. ONE wind for the world: foliage now, grass and cloth later, so a
    // second wind can never be invented alongside this one and diverge.
    glm::vec2 wind_direction{1.0f, 0.0f}; // normalized, world x/z
    float wind_strength = 0.0f;           // 0..1, CURRENT value INCLUDING gusts.
                                          // Computed CPU-side per frame and
                                          // shipped as one scalar ON PURPOSE:
                                          // if gusts were derived from time
                                          // inside the vertex shader, nothing
                                          // outside the GPU could know when a
                                          // gust peaks, and an audio rustle
                                          // could never be synced to the
                                          // picture except by luck. Gameplay
                                          // can read it too ("wait for the wind
                                          // to drop").
    float wind_flutter = 1.0f;            // multiplier on the fast component

    // Authored darkness of the PLACE the player is in (user: night stays
    // playable, but some interiors are a black void where a torch lights only
    // a small patch). 0 = normal, 1 = void. The GEOMETRIC half of darkness is
    // separate: per-vertex sky visibility written by worldgen. This is the
    // authored half — the app sets it from the darkness zone and lerps across
    // the boundary, so it is deliberately NOT per-vertex.
    float ambient_darkness = 0.0f;

    // BLACK FLOOR -- the user's minimum brightness ("абсолютно чёрным рисовать
    // хорошо, но в таких местах надо сделать их видными"). Applied in the
    // UPSCALE pass as out = c + black_floor * (1 - c)^BLACK_FLOOR_FALLOFF, per
    // channel, BEFORE the dither and the palette lookup -- after them the floor
    // would round into the entry it was meant to lift off. 0 = off, honest
    // black, and that is the control arm of its own acceptance.
    //
    // IT RIDES IN THE PER-FRAME BLOCK AND NOT IN RendererInitParams because the
    // calibration screen turns it live; a second home for a setting that
    // changes in play is how two copies of one number start disagreeing.
    float black_floor = 0.0f;

    // Weather state tuple, cloud slice (W1/W4 of WEATHER.md; render's diff,
    // lead-authored). Defaults are the "scattered" state so the sky is alive
    // before core's schedule function exists; the app will later write these
    // from the schedule without any render change.
    float cloud_cover = 0.45f;      // 0..1 layered-sheet coverage (W1 cloud_layer_cover)
    float cloud_cumulus = 0.5f;     // 0..1 horizon cumulus density (W1 cloud_cumulus)
    float cloud_shadow = 0.65f;     // 0..1 ground-shadow darkening strength (W1 cloud_shadow_cover)
    float weather_wind_mult = 1.0f; // W1 wind_strength: state multiplier on the SHARED wind (W3)
    glm::vec2 cloud_offset_m{0.0f, 0.0f}; // world-space drift of the ONE coverage field, meters.
                                    // Written per frame by render (CloudModel) from the shared
                                    // wind; BOTH the sky sheet and the ground shadow read THIS
                                    // value, which is what makes two drifting copies impossible
                                    // (W4's named reject).
    float cloud_wavelength_m = 600.0f; // coverage feature size (NUMBERS WIND_FIELD_WAVELENGTH)
    // THE THREE DECK ALTITUDES, in meters, low -> mid -> high. They were shader
    // #defines until the user asked for the ceiling's HEIGHT to be a field:
    // "должен быть диапазон где им можно быть... в разную погоду на разных,
    // будем таким образом погоду и климат отображать". A #define cannot carry
    // that, so the altitude is computed per frame by render (CloudModel) from
    // the weather state already in this struct and handed to the shaders in one
    // slot — ONE definition, which matters more here than anywhere else in this
    // struct: fs_sky intersects the view ray with these planes and
    // dfn_cloud_sun_vis projects along the SUN to the same planes, and if those
    // two ever read different numbers the ground shadow slides out from under
    // the cloud casting it.
    //
    // THE DEFAULT IS THE SHIPPED R3.2 LADDER, so a caller that never writes it
    // draws exactly the sky that shipped — the zero-dose arm of this change is
    // the struct's own default (Rule 48).
    glm::vec3 cloud_deck_m{1500.0f, 2600.0f, 4400.0f};
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Lifecycle ----------------------------------------------------------------
    [[nodiscard]] virtual bool init(const RendererInitParams& params) = 0;
    virtual void shutdown() = 0;
    virtual void resize(uint32_t framebuffer_width, uint32_t framebuffer_height) = 0;

    // Frame --------------------------------------------------------------------
    // view/proj are the interpolated camera matrices for this render frame (Rule 12:
    // simulation is fixed-step; interpolation happens in engine/render, not here).
    virtual void begin_frame(const glm::mat4& view, const glm::mat4& proj) = 0;
    virtual void end_frame() = 0;

    // Sets the frame environment; applies to subsequent submits until changed.
    // Null backend: accepted and ignored.
    virtual void set_environment(const RenderEnvironment& env) = 0;

    // Resources ----------------------------------------------------------------
    [[nodiscard]] virtual MeshHandle create_mesh(std::span<const Vertex> vertices,
                                                 std::span<const uint32_t> indices) = 0;
    virtual void destroy_mesh(MeshHandle mesh) = 0;

    [[nodiscard]] virtual TextureHandle create_texture(uint32_t width, uint32_t height,
                                                       TextureFormat format,
                                                       std::span<const uint8_t> pixels) = 0;
    virtual void destroy_texture(TextureHandle texture) = 0;

    // Loads a compiled shader pair by logical name (e.g. "terrain", "unlit").
    // Compiled artifacts are produced by the CMake shaderc step (Q50); the backend
    // resolves the per-API binary. Names, not paths — consumers never know the layout.
    [[nodiscard]] virtual ProgramHandle load_program(std::string_view name) = 0;
    virtual void destroy_program(ProgramHandle program) = 0;

    // Submission ---------------------------------------------------------------
    // Queues one draw for the current frame. texture may be invalid (untextured).
    // The five-argument form takes per-draw material parameters; the shorter one
    // is a convenience that passes the defaults, so existing call sites are
    // unchanged. Backends implement the virtual.
    virtual void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                        TextureHandle texture, const DrawParams& params) = 0;
    void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                TextureHandle texture = {}) {
        submit(mesh, program, transform, texture, DrawParams{});
    }

    // Immediate debug line in world space, lives one frame. No-op in release builds.
    // TURN LINE RECORDING ON FOR THIS SESSION. debug_line() costs nothing when
    // the door is shut, and the door is normally an environment variable read
    // at launch. A tool that decides mid-session that it needs lines — the
    // editor's build ghost, opened by a keypress — raises it here instead.
    // Default false; env doors still open it.
    virtual void set_debug_lines(bool enabled) = 0;

    /// КУДА НА ЭКРАНЕ САДИТСЯ КАРТИНКА МИРА, долями кадрового буфера.
    /// (0,0,1,1) — весь экран, и это умолчание.
    ///
    /// Заказ пользователя 18.08 дословно: «интерфейс инструмента заехал на
    /// дебажный текст. пусть инструмент рисуется не поверх игрового экрана, а
    /// как часть виджета приложения, пусть игра ниже рисуется, тогда проблем с
    /// наложением не будет».
    ///
    /// ЭТО ЛЕЧИТ ПРИЧИНУ, А НЕ СЛУЧАЙ. Полоса инструментов и отладочный вывод
    /// делили пиксели, и до сих пор их разводили ДОГОВОРЁННОСТЬЮ: каждый
    /// рисующий спрашивал, сколько занято сверху, и отступал сам. Договорённость
    /// соблюдают все, пока не появится тот, кто о ней не знает, — а появлялся
    /// он в этом проекте трижды. Если картинка мира физически не заходит под
    /// полосу, накладываться нечему, и ни одному будущему оверлею не придётся
    /// об этом помнить.
    ///
    /// Доли, а не пиксели, по той же причине, по которой доли выбрал HUD: мир
    /// компонуется во ВНУТРЕННЮЮ цель, а число в пикселях было бы верным для
    /// одной и тихо неверным для другой.
    virtual void set_present_rect_norm(float x, float y, float w, float h) = 0;

    virtual void debug_line(const glm::vec3& from, const glm::vec3& to,
                            uint32_t color_rgba) = 0;

    // Tooling ------------------------------------------------------------------
    // Captures the final upscaled framebuffer to a PNG after the current end_frame.
    // Backbone of the screenshot tour (Rule 27). Null backend returns false.
    virtual bool save_screenshot(const std::string& path) = 0;

    // Hot-reloads shader programs from the compiled artifacts on disk (Q50).
    // Debug-build convenience; a backend may implement it as a no-op.
    virtual void reload_shaders() = 0;

    // Debug / editor introspection (В28) --------------------------------------
    // Cheap, always available. frame_stats() and center_pick() report the LAST
    // completed frame (valid after end_frame); the null backend returns
    // zeroed/empty values (Rule 3). set_wireframe toggles a whole-scene
    // wireframe overlay; default off, and off costs nothing.
    virtual void set_wireframe(bool enabled) = 0;
    [[nodiscard]] virtual const RenderFrameStats& frame_stats() const = 0;
    [[nodiscard]] virtual const RenderPick& center_pick() const = 0;

    // THE BACKEND'S OWN NAME FOR A TEXTURE. 0xFFFFFFFF means "no answer".
    //
    // This is the ONE place the abstraction is deliberately leaky, and it is
    // narrow on purpose: the editor's interface layer draws with a SECOND
    // library (Dear ImGui) whose bgfx bridge lives beside this backend, and the
    // two have to be able to talk about the same texture — otherwise a part
    // thumbnail rendered into an offscreen target could never be shown in a
    // menu. Nothing outside a backend-matched UI layer may interpret the value;
    // anything that does is coupling itself to bgfx through a keyhole.
    //
    // Defaulted (not = 0) so this addition is purely additive (Rule 26): the
    // null backend and every test double keep compiling and answer "no", which
    // is exactly right for a renderer with no textures.
    [[nodiscard]] virtual uint32_t native_texture_handle(TextureHandle) const {
        return 0xFFFFFFFFu;
    }
};

} // namespace dfn::platform
