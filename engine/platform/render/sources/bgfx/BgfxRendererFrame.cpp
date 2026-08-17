/*
Created: 10:08:2026 - 01:47:53
Last updated: 17:08:2026 - 10:14:36
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxRendererFrame.cpp

Responsibility:
- The per-frame path of the bgfx backend: begin_frame / set_environment /
  end_frame, the environment + shadow uniform packing, debug lines, and
  screenshot scheduling. One of four translation units over BgfxRendererImpl.h
  (Rule 21 split); lifecycle is BgfxRenderer.cpp, draws BgfxRendererSubmit.cpp,
  handle bookkeeping BgfxRendererResources.cpp.

Key items:
- BgfxRenderer::begin_frame / set_environment / end_frame / debug_line /
  save_screenshot / reload_shaders.
- Impl::order_lights / touch_point_shadow_views / update_point_shadows /
  apply_environment / update_shadow / dest_rect.

Dependencies:
- Uses: BgfxRendererImpl.h, bgfx, glm.
- Used by: dfn_platform_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- EMPTY DRAWS EAT UNIFORMS (named rule, docs/specs/render.md): every
  bgfx::touch of a frame must be issued BEFORE the frame's first setUniform.
  touch_point_shadow_views runs at the top of begin_frame for exactly this
  reason; a new view that needs a clear joins that block, never clears where
  it is convenient.
*/
/*
UPD:
- 10:08:2026 - 01:47:53: Created in the Rule 21 split of BgfxRenderer.cpp.
  Frame path moved verbatim; no behaviour change.
- 10:08:2026 - 03:04:30: Cloud slots 33/34 packed in apply_environment (W4
  state + the one drift offset; paired with dfn_env.sh's 35-slot layout).
- 10:08:2026 - 20:10:49: Slot 35 packed with the sun's body straight from the
  generated constants (SUN_ANGULAR_DIAMETER / SUN_GLARE_ANGULAR_DIAMETER /
  SUN_DISC_LUMA / SUN_GLARE_LUMA_MAX), never through RenderEnvironment — they
  are NUMBERS rows with two consumers and exist once (Rule 35).
- 11:08:2026 - 13:41:41: Slot 36 packed with THE AIR (HAZE_SCALE_LENGTH / HAZE_HEIGHT_SCALE,
  REFERENCE_FRAMES.md R1), same generated-header route and same reason.
  DFN_HAZE overrides the scale length in metres so how thick the air should be
  is settled by a pair of frames rather than by an argument; it is read once
  and rejects a bad value LOUDLY, because a counterfactual arm that silently
  failed to apply is a duplicate of the other arm.
- 11:08:2026 - 14:24:26: Slot 37 = the MIST BAND (R2) and three more overrides (DFN_MIST,
  DFN_MIST_H, DFN_MIST_T) through the same loud-rejection path. Shipped rows
  moved to HAZE_SCALE_LENGTH 600 / HAZE_HEIGHT_SCALE 40 in NUMBERS; nothing
  here changed for that, which is the point of the generated-header route.
- 11:08:2026 - 23:53:45: DFN_TERRAIN_TILES — the ground tile as a counterfactual
  arm (R5). Same loud-rejection route. It answers the one question a single
  frame cannot: whether the repeating pattern on the ground is the MATERIAL
  tiling or a SCREEN-SPACE pattern (dither / palette / coverage AA).
  ANSWER: the material. The blob scale moved by exactly 4x in both directions.
- 12:08:2026 - 00:14:02: DFN_GROUND_TINT — the dose of the R5 ground tint, in
  slot [8].y. Default 1; 0 is the zero-dose control arm the R5 numbers are
  read against.
- 12:08:2026 - 23:08:22: DFN_SUN_SHADOW — the dose of the SUN SHADOW, and
  `haze_env_override` renamed `dose_env_override` because the air stopped being
  its only caller three knobs ago. Default 1, and at 1 the frame is
  bit-identical (dfn_shadow.sh does mix(1.0, s, dose)). `shadow_active` is NOT
  touched at dose 0: the casters still draw into the map and only the SAMPLING
  stops, so every submit, view and timing is identical between the arms and
  everything that is not the shadow subtracts to zero in a difference frame
  (Rule 47's structural cure). Built because two open claims need exactly this
  arm — R6b's dapple, whose absolute number is unusable without it, and the
  user's two standing shadow complaints, which live BETWEEN frames.
- 13:08:2026 - 16:10:00: update_shadow builds a SECOND, NEAR light volume
  (40 m half extent, same light orientation and same depth bracket, snapped to
  its OWN texel grid) and uploads u_lightMtxNear; u_shadowParams.w stopped
  being unused and now carries the near push-off, which has to be its own
  number or a 0.156 m offset on a 0.0195 m map would erode eight texels of
  every hole and spend the whole gain. DFN_SHADOW_NEAR is its dose:
  DFN_SUN_SHADOW=0 asks "is there any shadow", DFN_SHADOW_NEAR=0 asks the
  question this change makes — "is the new grain the cascade, or the ground
  material that moved under us this week" — and both come out of one binary.
- 13:08:2026 - 18:10:00: packed[38] = the fill's direction (DFN_FILL_UP /
  DFN_FILL_SUN, both doses, both 0 restoring the previous frame exactly).
- 13:08:2026 - 18:50:00: update_shadow snaps the light direction (azimuth and
  elevation, floor onto SHADOW_DIR_SNAP_RAD) before building the light view,
  AFTER the elevation test so a snap can never flick shadows on and off across
  the horizon. DFN_SHADOW_SNAP is its dose and 0 restores the previous frame.
- 13:08:2026 - 19:20:00: packed[39] = the CLOUD DECK ALTITUDES (R3.4). They stop
  being shader #defines because the user asked for the ceiling's height to be a
  field with a legal range — that is how a place's weather and climate get shown
  — so engine/render computes them per frame and they arrive here. One slot, not
  three: fs_sky intersects the view ray with these planes and dfn_cloud_sun_vis
  projects along the sun to the same planes, and a disagreement between the two
  slides the ground shadow out from under the cloud casting it.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 19:11:13: order_lights: THE CASTER PREDICATE IS DECIDED ONCE. It was
  recomputed inside both passes from `shadow_light_count`, a counter pass 0
  advances, so past MAX_SHADOW_POINT_LIGHTS the casters already emitted read as
  non-casters in pass 1 and were appended a SECOND time -- 8 lights with 2
  casters wrote 10 entries into `std::array<PointLight, 8>`. RenderSystem had
  capped the world at ONE shadowing flame to dodge it; that cap is gone with
  the cause.
- 13:08:2026 - 19:49:07: packed[39].w = THE MOON'S GROUND GAIN, in the deck slot's spare
  component rather than in a fortieth vec4 (the array's size is a two-file
  contract and resizing it stopped this project's build twice in one day).
  DFN_MOON_GROUND is its dose and 0 is the zero-dose control -- which is what
  let both arms of its measurement come out of ONE binary while seven other
  agents were editing this tree. Derivation and numbers: dfn_env.sh,
  u_moonGround.
- 13:08:2026 - 20:05:20: MOON_GROUND_GAIN got its NUMBERS row, so the moon's ground gain stops
  being a literal here and arrives through the generated header like every other
  number with two consumers (Rule 35). DFN_MOON_GROUND still overrides it.
- 13:08:2026 - 20:19:19: packed[38].z = the middle deck's thickness, 0.5 * WIND_FIELD_WAVELENGTH
  from the generated header, with DFN_DECK_THICK as its dose. Derivation and the
  measurement at dfn_env.sh, u_deckThick.
- 13:08:2026 - 21:32:37: DFN_PS_DEBUG dose on u_pointShadowParams.w (was shipped unused): 1/2/3
  select dfn_pointshadow.sh's diagnostic returns (sampled atlas value, compare
  value, uv-in-own-tile). Default 0.0 keeps the shipping compare bit for bit.
  Opened for the dungeon "point shadow factor is 0 with clear air" defect.
- 13:08:2026 - 22:29:00: DFN_DUMP_POINT_ATLAS=<path> -- one-shot blit + readTexture of the cube
  atlas, raw R32F. It is what convicted the culprit: every texel of all six
  faces held 0.11-0.64 m, the light holder's own mesh, drawn UNSCALED (unit
  space, Transform.scale 1) around its flame.
- 14:08:2026 - 16:35:53: В28 debug/editor hooks. begin_frame sets the global
  wireframe flag (DFN_WIREFRAME door), resets the frame-stat/pick accumulators
  and builds the centre-screen pick ray; in wireframe mode the scene view is
  retargeted at the backbuffer (the global flag would line-draw the upscale
  quad). end_frame skips the present in wireframe, and latches frame_stats
  (scene draws/tris ours, backend_draws from bgfx after frame()) + center_pick.
  set_wireframe / frame_stats / center_pick / Impl::wireframe_on defined here.
  DFN_FRAME_STATS=1 is the acceptance door for the two read-back hooks.
- 15:08:2026 - 14:07:36: dest_rect: целочисленный масштаб только для РЕТРО-сетки
  (множитель >= 2); при полнодетальной сетке картинка ВПИСЫВАЕТСЯ в окно —
  множитель 1 рамкой в чёрное был жалобой «че за черные края» при Full HD.
- 16:08:2026 - 22:11:47: packed[40] — полоса фейда листвы, умолчание 0.03/0.08 (см.
  foliage_edge_band: первая догадка 0.08/0.22 растворяла ели целиком).
- 17:08:2026 - 10:14:36: ПРИЁМОЧНЫЙ КАДР СНИМАЕТСЯ С НАШЕЙ СОБСТВЕННОЙ ЦЕЛИ, а не с бэкбуфера.
  Четыре случая за сутки купили эту правку: при спящем или запертом экране
  Metal не выдаёт drawable, бэкбуфер остаётся незаписанным, и снимок с него
  сохраняет ЧЁРНЫЙ или наполовину чёрный кадр — зоны сообщали про «землю,
  ставшую чёрной» и «полосу снизу», два имени для одной недорисованной
  поверхности, и каждое стоило времени на опровержение.
  Цель capture_fb внутреннего размера несёт ТОТ ЖЕ ПОСТ, что и экран:
  программа апскейла подаётся в неё вторым проходом, поэтому чёрный пол и
  палитра применяются ровно как для окна. Читать internal_fb напрямую было бы
  одной строкой и НЕВЕРНО — это картинка ДО поста, и всякий калибровочный кадр
  с неё тихо расходился бы с тем, что видит человек.
  И читается она через readTexture, а НЕ через requestScreenShot: на Metal тот
  обслуживает только фреймбуфер со СВОПЧЕЙНОМ (renderer_mtl.cpp ищет
  m_swapChain и молча возвращается, когда его нет), поэтому первая попытка
  дала сайдкар .txt без .png и ни строчки объяснения. readTexture — тот же
  путь, которым этот файл уже снимает атлас точечных теней.
  ЧЕГО ЭТО НЕ ЧИНИТ, чтобы никто не решил иначе: темп кадров всё ещё зависит
  от drawable, и при спящем экране прогон останется медленным. Починено то,
  что кадры, которые он ВСЁ-ТАКИ выдаёт, стали правдой, а не чернотой.
  Побочно: приёмочный кадр теперь всегда внутреннего размера, без леттербокса
  и без масштаба окна — два кадра из окон разного размера стали сравнимы.
*/

#include "engine/platform/render/sources/bgfx/BgfxRendererImpl.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace dfn::platform {

namespace {

// Look-dev clear color (sky), not a gameplay constant: light steppe-sky blue.
constexpr uint32_t SKY_COLOR_RGBA = 0x87b5e0ff;

} // namespace

// Orders the frame's point lights so SHADOW CASTERS COME FIRST. The shader
// then treats the light slot as the cube-atlas index, which removes the
// only place a light->map mapping could disagree with itself. Must run
// before apply_environment and update_point_shadows, and both of those are
// driven from the same two call sites, so they cannot see different orders.
void BgfxRenderer::Impl::order_lights() {
    light_count = 0;
    shadow_light_count = 0;
    const uint32_t incoming = environment.point_light_count < MAX_POINT_LIGHTS
                                  ? environment.point_light_count
                                  : MAX_POINT_LIGHTS;
    const bool maps_ok = bgfx::isValid(point_shadow_fb)
                      && bgfx::isValid(point_shadow_program);
    // THE PREDICATE IS DECIDED ONCE, AND THAT IS THE WHOLE FIX. It used to be
    // recomputed inside both passes from `shadow_light_count`, i.e. from a
    // counter the first pass had already advanced — so once the cap was
    // reached, the casters ALREADY EMITTED in pass 0 stopped reading as casters
    // in pass 1 and were appended a SECOND time. With 8 incoming lights and 2
    // casters `light_count` reached 10 and wrote past
    // `std::array<PointLight, MAX_POINT_LIGHTS>`: a byte-write translation
    // fault, every run, which is why RenderSystem capped the world at one
    // caster. A membership test may not be a function of the emission it
    // controls.
    bool shadowing[MAX_POINT_LIGHTS]{};
    for (uint32_t i = 0; i < incoming; ++i) {
        const PointLight& l = environment.point_lights[i];
        if (l.radius_m <= 0.0f) {
            continue; // radius 0 = off (contract)
        }
        if (maps_ok && l.casts_shadow
            && shadow_light_count < MAX_SHADOW_POINT_LIGHTS) {
            shadowing[i] = true;
            ++shadow_light_count;
        }
    }
    for (uint32_t pass = 0; pass < 2; ++pass) {
        for (uint32_t i = 0; i < incoming; ++i) {
            const PointLight& l = environment.point_lights[i];
            if (l.radius_m <= 0.0f) {
                continue; // radius 0 = off (contract)
            }
            if ((pass == 0) != shadowing[i]) {
                continue;
            }
            lights[light_count] = l;
            ++light_count;
        }
    }
}

// Framebuffer/rect/clear + touch for EVERY face tile, whether a light uses
// it this frame or not. Two reasons it is separate from update_point_shadows
// and lives at the top of begin_frame:
//  1. A tile with no light must still be cleared to "unoccluded", or a
//     torch that goes out leaves its last frame baked into the atlas.
//  2. A touch is an EMPTY DRAW, and an empty draw SWALLOWS the pending
//     uniform range without ever applying it. Touching after
//     apply_environment cost this project one debugging session: the whole
//     world rendered with the default (daylit) environment the moment a
//     torch was lit, because the night env had been recorded into a touch
//     that bgfx then skipped. Every touch happens BEFORE any setUniform of
//     the frame; the uniforms then attach to the sky draw, which is real.
void BgfxRenderer::Impl::touch_point_shadow_views() {
    if (!bgfx::isValid(point_shadow_fb)) {
        return;
    }
    for (uint32_t tile = 0; tile < POINT_SHADOW_VIEWS; ++tile) {
        const uint16_t col = static_cast<uint16_t>(tile % POINT_SHADOW_ATLAS_COLS);
        const uint16_t row = static_cast<uint16_t>(tile / POINT_SHADOW_ATLAS_COLS);
        const auto vid =
            static_cast<bgfx::ViewId>(VIEW_POINT_SHADOW_FIRST + tile);
        bgfx::setViewFrameBuffer(vid, point_shadow_fb);
        bgfx::setViewRect(vid, static_cast<uint16_t>(col * POINT_SHADOW_FACE_PX),
                          static_cast<uint16_t>(row * POINT_SHADOW_FACE_PX),
                          POINT_SHADOW_FACE_PX, POINT_SHADOW_FACE_PX);
        // Clear to 1.0 = "farther than the radius" = lit, so a face with no
        // casters is transparent to light rather than black. Depth is a
        // normal LESS pass: this view is its own pipeline and owes nothing
        // to the scene's reversed-Z.
        bgfx::setViewClear(vid, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                           0xffffffff, 1.0f, 0);
        bgfx::touch(vid);
    }
}

// Defined below (the counterfactual-arm helper); declared here because the
// point-shadow debug dose needs it before its definition.
static float dose_env_override(const char* name, float fallback);

// THE POINT-SHADOW DEBUG DOSE (DFN_PS_DEBUG), riding u_pointShadowParams.w,
// which shipped unused. 0 (the default) is the shipping compare, bit for bit
// — dfn_pointshadow.sh's debug branches are all dead at 0. 1/2/3 select the
// shader's diagnostic returns (sampled atlas value / compare value /
// uv-in-own-tile), opened to split the "shadow factor is 0 with clear air"
// dungeon defect into writer, sampler and compare without a GPU debugger.
static float point_shadow_debug() {
    static const float value = dose_env_override("DFN_PS_DEBUG", 0.0f);
    return value;
}

// Builds the six 90-degree face views for each shadow-casting light and
// uploads the matrices the receivers sample with. Each face is a rectangle
// of ONE atlas framebuffer, so the tile scale/offset is baked into the
// matrix and the shader needs no tile arithmetic.
void BgfxRenderer::Impl::update_point_shadows() {
    glm::vec4 rows[MAX_SHADOW_POINT_LIGHTS * POINT_SHADOW_FACES * 4]{};
    const bgfx::Caps& caps = *bgfx::getCaps();
    for (uint32_t li = 0; li < shadow_light_count; ++li) {
        const PointLight& l = lights[li];
        for (uint32_t f = 0; f < POINT_SHADOW_FACES; ++f) {
            const uint32_t tile = li * POINT_SHADOW_FACES + f;
            const uint16_t col = static_cast<uint16_t>(tile % POINT_SHADOW_ATLAS_COLS);
            const uint16_t row = static_cast<uint16_t>(tile / POINT_SHADOW_ATLAS_COLS);
            const glm::mat4 view = glm::lookAtRH(
                l.position, l.position + POINT_SHADOW_FACE_DIR[f],
                POINT_SHADOW_FACE_UP[f]);
            const float fov = glm::radians(90.0f);
            const glm::mat4 proj =
                caps.homogeneousDepth
                    ? glm::perspectiveRH_NO(fov, 1.0f, POINT_SHADOW_NEAR_M,
                                            l.radius_m)
                    : glm::perspectiveRH_ZO(fov, 1.0f, POINT_SHADOW_NEAR_M,
                                            l.radius_m);
            const bgfx::ViewId vid = static_cast<bgfx::ViewId>(
                VIEW_POINT_SHADOW_FIRST + tile);
            // Framebuffer, rect, clear and touch were done up front by
            // touch_point_shadow_views — NEVER touch from here (see the
            // comment there: an empty draw eats the pending uniforms).
            bgfx::setViewTransform(vid, glm::value_ptr(view),
                                   glm::value_ptr(proj));

            // clip -> [0,1] uv (API y flip), then into the atlas tile.
            glm::mat4 crop(1.0f);
            crop[0][0] = 0.5f;
            crop[1][1] = caps.originBottomLeft ? 0.5f : -0.5f;
            crop[3] = glm::vec4(0.5f, 0.5f, 0.0f, 1.0f);
            const float sx = 1.0f / static_cast<float>(POINT_SHADOW_ATLAS_COLS);
            const float sy = 1.0f / static_cast<float>(POINT_SHADOW_ATLAS_ROWS);
            glm::mat4 tile_mtx(1.0f);
            tile_mtx[0][0] = sx;
            tile_mtx[1][1] = sy;
            tile_mtx[3] = glm::vec4(static_cast<float>(col) * sx,
                                    static_cast<float>(row) * sy, 0.0f, 1.0f);
            const glm::mat4 m = tile_mtx * crop * proj * view;
            // Stored as ROWS: the shader indexes them dynamically and
            // takes three dot products, which needs no matrix assembly
            // from a computed offset.
            for (uint32_t r = 0; r < 4; ++r) {
                rows[tile * 4 + r] =
                    glm::vec4(m[0][r], m[1][r], m[2][r], m[3][r]);
            }
        }
    }
    bgfx::setUniform(u_point_shadow_rows, rows,
                     MAX_SHADOW_POINT_LIGHTS * POINT_SHADOW_FACES * 4);
    const float params[4] = {static_cast<float>(shadow_light_count),
                             POINT_SHADOW_NORMAL_OFFSET_M,
                             POINT_SHADOW_BIAS_FRAC, point_shadow_debug()};
    bgfx::setUniform(u_point_shadow_params, params);
}

// A COUNTERFACTUAL ARM AS A NUMBER: one non-negative float, from the
// environment, defaulting to what ships. Every caller below is a DOSE whose
// zero is a control arm (Rule 48) or a discriminator whose sweep separates two
// explanations (Rule 30) — the air, the ground tile, the ground tint, the sun
// shadow. They are diagnostic knobs like DFN_MSAA, not second homes for the
// numbers: unset, the generated rows are what ships.
//
// It was called `haze_env_override` when the air was its only caller and kept
// the name through three more; a helper named after one of its callers is how
// the next reader concludes the knob is unavailable to them.
static float dose_env_override(const char* name, float fallback) {
    const char* e = std::getenv(name);
    if (e == nullptr || *e == '\0') {
        return fallback;
    }
    float parsed = 0.0f;
    if (std::sscanf(e, "%f", &parsed) == 1 && parsed >= 0.0f) {
        std::fprintf(stderr, "[render] %s=%.1f (default %.1f)\n", name,
                     static_cast<double>(parsed), static_cast<double>(fallback));
        return parsed;
    }
    // LOUD, never silent (Rule 30): a counterfactual arm that quietly failed to
    // apply is a DUPLICATE of the other arm, and "the haze made no difference"
    // would then be concluded from a run where the haze never moved.
    std::fprintf(stderr,
                 "[render] %s=\"%s\" REJECTED (want a non-negative number); "
                 "stays %.1f — the arm was NOT applied\n",
                 name, e, static_cast<double>(fallback));
    return fallback;
}

// THE GROUND TILE, as a counterfactual arm (Rule 30). `DFN_TERRAIN_TILES`
// overrides how many times the ground material repeats across one CHUNK_SIZE.
// It is a DISCRIMINATOR, not a look knob: a pattern in the frame is either
// world-space (it lives in the material, so its screen scale moves when this
// moves) or screen-space (dither, palette, coverage AA — it does not move at
// all). No amount of looking at ONE frame separates those two, and R5 names
// them as two different defects with two different fixes.
static float terrain_tiles_override(float fallback) {
    static const float value = dose_env_override("DFN_TERRAIN_TILES", fallback);
    return value;
}

// R5's DOSE, and it exists so the change has a ZERO-DOSE CONTROL (Rule 48).
// DFN_GROUND_TINT=0 must give back the pre-R5 ground; any R5 number that still
// looks good at 0 is measuring the light or the terrain, not the material.
static float ground_tint_dose() {
    static const float value = dose_env_override("DFN_GROUND_TINT", 1.0f);
    return value;
}

// HOW BRIGHT A FULL MOON LIGHTS THE GROUND, as a multiple of moon_color. The
// derivation is written out where the term is applied (dfn_env.sh, u_moonGround)
// and it is a MEASUREMENT solved for the gain, not a taste: at 0.30 a full moon
// left the ground 9.72 luma above a moonless night IN THE QUANTISER'S METRIC
// (0.30/0.59/0.11, fs_upscale.sc), and 9.72 of 255 is 0.49 of one
// PALETTE_SHADE_STEP_REF — half the quantiser's own cell, i.e. under this
// project's stated threshold for two things being told apart. Two steps of
// separation need 0.30 * 39.98/9.72 = 1.234.
//
// SOLVING FOR IT IS LEGITIMATE BECAUSE THE RESPONSE IS LINEAR, and that was
// CHECKED rather than assumed: the first pass shipped 1.166 (derived from the
// Rec.709 luma, the wrong metric) and predicted 37.8 luma of separation. The
// frame came back with 37.79. A gain that the frame answers linearly can be
// solved for; one that saturates cannot.
//
// IT HAS ITS ROW NOW (`MOON_GROUND_GAIN`, approved with the measurement above),
// so it arrives through the generated header like every other number with two
// consumers — never as a literal here and a literal there (Rule 35).
//
// DFN_MOON_GROUND is that dose, and 0 is the zero-dose control: at 0 the moon
// stops lighting the ground and only the night ambient is left, which is the arm
// the 7.43 above was read from.
static float moon_ground_gain() {
    static const float value = dose_env_override("DFN_MOON_GROUND",
                          static_cast<float>(config::MOON_GROUND_GAIN));
    return value;
}

// THE MIDDLE DECK'S THICKNESS, metres: half of WIND_FIELD_WAVELENGTH, i.e. the
// coverage field's own cell at a 2:1 width-to-depth aspect. Derived at
// u_deckThick in dfn_env.sh; read from the generated header because the
// wavelength is a NUMBERS row with two consumers already.
static float deck_thickness_m() {
    static const float value = dose_env_override(
        "DFN_DECK_THICK", 0.5f * static_cast<float>(config::WIND_FIELD_WAVELENGTH));
    return value;
}

// THE FOLIAGE EDGE-FADE BAND, in |dot(N, V)|. Defaults 0.03/0.08 — kill only
// razor angles under ~5 degrees. The first shipped guess (0.08/0.22) came from
// reasoning about leaf cards and was wrong for a real tree: a spruce's fronds
// lie near-horizontal, so from eye level their whole area sits at |dot| ~ 0 and
// the fade took the ENTIRE canopy. Measured by flora on a frame, not argued.
// lo >= hi disables the fade out of the same binary (Rule 47's control arm).
static glm::vec2 foliage_edge_band() {
    static const glm::vec2 band = {dose_env_override("DFN_FOLIAGE_EDGE_LO", 0.03f),
                                   dose_env_override("DFN_FOLIAGE_EDGE_HI", 0.08f)};
    return band;
}

struct HazeParams {
    float scale_m;
    float height_m;
    float base_m;
    // The mist band (R2) — its own altitude, extent and density, because it is
    // a different layer of air and not a setting of the haze above it.
    float mist_height_m;
    float mist_thickness_m;
    float mist_density;
};

static const HazeParams& haze_params() {
    static const HazeParams p = {
        dose_env_override("DFN_HAZE",
                          static_cast<float>(config::HAZE_SCALE_LENGTH)),
        dose_env_override("DFN_HAZE_H",
                          static_cast<float>(config::HAZE_HEIGHT_SCALE)),
        dose_env_override("DFN_HAZE_BASE",
                          static_cast<float>(config::HAZE_BASE_HEIGHT)),
        dose_env_override("DFN_MIST_H",
                          static_cast<float>(config::MIST_BAND_HEIGHT)),
        dose_env_override("DFN_MIST_T",
                          static_cast<float>(config::MIST_BAND_THICKNESS)),
        dose_env_override("DFN_MIST",
                          static_cast<float>(config::MIST_BAND_DENSITY)),
    };
    return p;
}

// Packs the cached RenderEnvironment into u_envParams; the index layout is
// the dfn_env.sh contract. Called once per frame in begin_frame — uniform
// values persist across submits within the frame.
void BgfxRenderer::Impl::apply_environment() const {
    const RenderEnvironment& e = environment;
    glm::vec4 packed[ENV_PARAM_VEC4S] = {  // trailing slots zero-init
        {e.sun_direction, 0.0f},
        {e.sun_color, 0.0f},
        {e.ambient_color, 0.0f},
        {e.fog_color, 0.0f},
        {e.fog_start_m, e.fog_end_m, e.time_seconds, 0.0f},
        {e.sky_zenith_color, 0.0f},
        {e.sky_horizon_color, 0.0f},
        {e.sand_height_m, e.sand_blend_m, e.rock_slope_start, e.rock_slope_end},
        {terrain_tiles_override(e.terrain_tiles_per_chunk), ground_tint_dose(),
         0.0f, 0.0f},
        e.water_color,
        {e.water_scroll_uv, 0.0f, 0.0f},
        {e.moon_direction, e.moon_phase},
        {e.moon_color, e.moon_light},
        {0.0f, 0.0f, 0.0f, 0.0f}, // [13] reserved (was the single light)
        {0.0f, 0.0f, 0.0f, e.star_intensity},
    };
    // Lights come from the ORDERED array (order_lights), not straight from
    // the environment: shadow casters must occupy the first slots because
    // the shader uses the slot as the cube-atlas index.
    packed[15] = {e.ambient_darkness, static_cast<float>(light_count), 0.0f,
                  0.0f};
    packed[32] = {e.wind_direction, e.wind_strength, e.wind_flutter};
    // Clouds (W4): state tuple + the ONE drift offset both samplers read.
    packed[33] = {e.cloud_cover, e.cloud_cumulus, e.cloud_shadow,
                  e.cloud_wavelength_m};
    packed[34] = {e.cloud_offset_m, e.weather_wind_mult, 0.0f};
    // THE SUN'S BODY (W9). Straight from the generated header, never through
    // RenderEnvironment: these are NUMBERS rows with two consumers by
    // construction — design derives them, the shader measures the frame with
    // them — so they exist once and travel to the only place that reads them
    // (Rule 35). Radii, because the shader compares against an angle; the
    // rows are diameters because that is how a sky is described.
    packed[35] = {0.5f * static_cast<float>(config::SUN_ANGULAR_DIAMETER),
                  0.5f * static_cast<float>(config::SUN_GLARE_ANGULAR_DIAMETER),
                  static_cast<float>(config::SUN_DISC_LUMA),
                  static_cast<float>(config::SUN_GLARE_LUMA_MAX)};
    // THE AIR (R1). Same route and the same reason as the sun's body: design
    // derives these from the landmark depth-separation contract and the shader
    // is what makes the frame obey them, so they have two consumers and may
    // exist exactly once (Rule 35). They deliberately do NOT come through
    // RenderEnvironment yet — nothing varies them per frame. The day weather
    // does (a fog morning is a shorter scale length), and THAT is a contract
    // change to request from the lead rather than to smuggle in here.
    // DFN_HAZE overrides the scale length in METRES, and it exists so that
    // "how thick should the air be" is settled by two frames instead of by an
    // argument (Rule 27). It is a diagnostic knob like DFN_MSAA, not a second
    // home for the number: unset, the generated row is what ships.
    const HazeParams& haze = haze_params();
    packed[36] = {haze.scale_m, haze.height_m, haze.base_m, 0.0f};
    packed[37] = {haze.mist_height_m, haze.mist_thickness_m, haze.mist_density,
                  0.0f};
    // THE FILL'S DIRECTION (slot 38). Derivation and the before-number with the
    // constants in BgfxRendererImpl.h. Both doses read ONCE, and setting BOTH to
    // 0 restores the shipped-before-this frame exactly — 1 + 0*n.y + 0*dot() is
    // 1, so the ambient line is bit-identical to `u_ambientColor * sky`. That is
    // the control arm this claim is measured against, and it comes out of the
    // same binary as the shipped one.
    static const float fill_up = dose_env_override("DFN_FILL_UP", FILL_UP_DEFAULT);
    static const float fill_sun =
        dose_env_override("DFN_FILL_SUN", FILL_SUN_DEFAULT);
    // .z = THE MIDDLE DECK'S THICKNESS in metres, half a coverage cell (see
    // dfn_env.sh, u_deckThick). DFN_DECK_THICK is the dose and 0 restores the
    // flat sheet exactly.
    packed[38] = {fill_up, fill_sun, deck_thickness_m(), 0.0f};
    // THE CLOUD DECK ALTITUDES (slot 39), low / mid / high, meters. They were
    // shader #defines; the ceiling's HEIGHT is now a field of weather and place
    // (R3.4), so it arrives per frame. It travels as ONE slot because its two
    // consumers must never disagree: fs_sky intersects the VIEW ray with these
    // planes and dfn_cloud_sun_vis projects along the SUN to the same planes,
    // and a disagreement slides the ground shadow out from under the cloud
    // casting it.
    // .w is the MOON'S GROUND GAIN, riding in the deck slot's spare component
    // rather than in a fortieth vec4: the array's size is a two-file contract
    // and resizing it has stopped this project's build twice in one day, so a
    // free component is worth more than a tidy grouping.
    packed[39] = {e.cloud_deck_m, moon_ground_gain()};
    // Slot 40: the foliage edge-fade band (see foliage_edge_band).
    packed[40] = {foliage_edge_band().x, foliage_edge_band().y, 0.0f, 0.0f};
    for (uint32_t i = 0; i < light_count; ++i) {
        const PointLight& l = lights[i];
        packed[16 + i] = {l.position, l.radius_m};
        packed[24 + i] = {l.color, l.casts_shadow ? 1.0f : 0.0f};
    }
    bgfx::setUniform(u_env_params, packed, ENV_PARAM_VEC4S);
}

// Recomputes the sun-light matrices from the cached environment + frame
// eye and uploads u_lightMtx / u_shadowParams. Called from begin_frame and
// from a mid-frame set_environment (uniform values are captured per
// submit, so the app's day/night set_environment right after begin_frame
// applies to every draw of the frame; view transforms resolve at frame
// render, last call wins).
void BgfxRenderer::Impl::update_shadow() {
    shadow_active = false;
    shadow_near_active = false;
    float enabled = 0.0f;
    glm::mat4 light_mtx(1.0f);
    // The near cascade's matrix is INVALID until proven otherwise, and
    // "invalid" has to mean "sampled by nobody": a zero matrix would map every
    // fragment to uv 0 and shadow the world with one texel. Pushed far outside
    // the unit box instead, so the shader's own in-volume test rejects it even
    // if the dose ever gets out of step with the matrix.
    glm::mat4 light_mtx_near = glm::translate(glm::mat4(1.0f),
                                              glm::vec3(1e6f, 1e6f, 1e6f));
    glm::vec3 dir = environment.sun_direction;
    const float len = glm::length(dir);
    if (bgfx::isValid(shadow_fb) && bgfx::isValid(shadow_program)
        && len > 1e-5f) {
        dir /= len;
        if (dir.y > SHADOW_MIN_SUN_ELEVATION) {
            shadow_active = true;
            enabled = 1.0f;
            // THE LIGHT DIRECTION IS SNAPPED TO AN ANGULAR GRID, and this is the
            // other half of the texel snapping below — without it that snap
            // cannot work at all. Derivation and numbers in the constant's own
            // block (SHADOW_DIR_SNAP_RAD, BgfxRendererImpl.h). Short form: the
            // volume centre is snapped in LIGHT space, and light space turns
            // with the sun, so the whole world-space lattice rotates under
            // every receiver even though the sun moves 0.36 mm per frame.
            // Freezing the direction between steps makes the lattice genuinely
            // stationary, which is what the eye-centre snap was written to
            // assume.
            //
            // AFTER the elevation test on purpose: whether the sun is up at all
            // is decided by the real sun, so a snap can never flick shadows on
            // and off across the horizon.
            static const float dir_snap =
                dose_env_override("DFN_SHADOW_SNAP", SHADOW_DIR_SNAP_RAD);
            if (dir_snap > 0.0f) {
                const float az = std::atan2(dir.x, dir.z);
                const float el = std::asin(std::clamp(dir.y, -1.0f, 1.0f));
                const float qaz = std::floor(az / dir_snap) * dir_snap;
                const float qel = std::floor(el / dir_snap) * dir_snap;
                const float ce = std::cos(qel);
                dir = glm::vec3(ce * std::sin(qaz), std::sin(qel), ce * std::cos(qaz));
            }
            constexpr float H = SHADOW_HALF_EXTENT_M;
            constexpr float D = SHADOW_DEPTH_HALF_M;
            // Orientation-only light view; the volume center (the camera
            // eye) is snapped to the shadow texel grid in light space so
            // edges do not crawl as the camera moves.
            const glm::vec3 up = std::fabs(dir.y) > 0.99f
                                     ? glm::vec3(0.0f, 0.0f, 1.0f)
                                     : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::mat4 rot = glm::lookAtRH(dir, glm::vec3(0.0f), up);
            glm::vec3 c = glm::vec3(rot * glm::vec4(frame_eye, 1.0f));
            c.x = std::floor(c.x / SHADOW_TEXEL_M) * SHADOW_TEXEL_M;
            c.y = std::floor(c.y / SHADOW_TEXEL_M) * SHADOW_TEXEL_M;
            const glm::mat4 view =
                glm::translate(glm::mat4(1.0f), -c) * rot;
            // Kept so `submit` can reject casters that lie outside this
            // volume. Without it every LOD node at 1-4 km pays for a full
            // depth draw into a map that ends at 320 m — the terrain LOD
            // ladder's whole point is that distant ground is cheap, and a
            // free shadow draw per node undoes it.
            shadow_view = view;
            const bgfx::Caps& caps = *bgfx::getCaps();
            // near = -D / far = +D brackets the eye plane along the light.
            const glm::mat4 proj =
                caps.homogeneousDepth
                    ? glm::orthoRH_NO(-H, H, -H, H, -D, D)
                    : glm::orthoRH_ZO(-H, H, -H, H, -D, D);
            bgfx::setViewTransform(VIEW_SHADOW, glm::value_ptr(view),
                                   glm::value_ptr(proj));
            // NDC -> shadow-map uv/depth crop (API-dependent y flip and
            // depth range), premultiplied for the fragment shaders.
            glm::mat4 crop(1.0f);
            crop[0][0] = 0.5f;
            crop[1][1] = caps.originBottomLeft ? 0.5f : -0.5f;
            crop[2][2] = caps.homogeneousDepth ? 0.5f : 1.0f;
            crop[3] = glm::vec4(0.5f, 0.5f,
                                caps.homogeneousDepth ? 0.5f : 0.0f, 1.0f);
            light_mtx = crop * proj * view;

            // THE NEAR CASCADE (R6b). Same light orientation, same depth
            // bracket, same crop — only the lateral extent shrinks, so the two
            // maps agree about what is in front of what and differ only in how
            // finely they say it. Snapped to its OWN texel grid: snapping to
            // the far one would leave the near map free to slide by up to a far
            // texel, which is 8 near texels of crawl on the very edges this
            // exists to sharpen.
            //
            // ITS OWN DOOR, and it is not decoration — it is what let this
            // change be judged at all. DFN_SUN_SHADOW=0 answers "is there any
            // shadow here at all"; DFN_SHADOW_NEAR=1 answers the question the
            // cascade actually makes — "is the new grain the cascade, or the
            // flora and the ground that moved under us this week". Both arms
            // come out of ONE binary, and they had to: between two rebuilds an
            // hour apart the far-map-only arm moved by more than the cascade
            // was worth, so a before/after across binaries would have credited
            // this change with someone else's canopy. DEFAULT OFF — see the
            // measurement with the constants.
            if (bgfx::isValid(shadow_fb_near) && shadow_near_enabled()) {
                shadow_near_active = true;
                constexpr float HN = SHADOW_NEAR_HALF_EXTENT_M;
                glm::vec3 cn = glm::vec3(rot * glm::vec4(frame_eye, 1.0f));
                cn.x = std::floor(cn.x / SHADOW_NEAR_TEXEL_M) * SHADOW_NEAR_TEXEL_M;
                cn.y = std::floor(cn.y / SHADOW_NEAR_TEXEL_M) * SHADOW_NEAR_TEXEL_M;
                const glm::mat4 view_near =
                    glm::translate(glm::mat4(1.0f), -cn) * rot;
                shadow_view_near = view_near;
                const glm::mat4 proj_near =
                    caps.homogeneousDepth
                        ? glm::orthoRH_NO(-HN, HN, -HN, HN, -D, D)
                        : glm::orthoRH_ZO(-HN, HN, -HN, HN, -D, D);
                bgfx::setViewTransform(VIEW_SHADOW_NEAR,
                                       glm::value_ptr(view_near),
                                       glm::value_ptr(proj_near));
                light_mtx_near = crop * proj_near * view_near;
            }
        }
    }
    // THE ZERO-DOSE ARM (Rule 48). DFN_SUN_SHADOW scales the shadow term the
    // fragment shaders read; unset it is 1 and the frame is bit-identical to
    // before this knob existed. `shadow_active` is deliberately NOT touched, so
    // at dose 0 the casters are still drawn into the map and only the SAMPLING
    // stops — that keeps every submit, every view and every timing identical
    // between the arms, and everything that is not the shadow subtracts to zero
    // in a difference frame (Rule 47's structural cure). Read here rather than
    // at init so a sweep costs no relaunch.
    // Read ONCE, like the other doses: it is a property of the run, and a knob
    // re-read per frame prints its banner per frame and could in principle put
    // two shadow strengths in one shoot.
    static const float shadow_dose = dose_env_override("DFN_SUN_SHADOW", 1.0f);
    // w carries the NEAR push-off. It has to be its own number rather than the
    // far one reused: the offset is denominated in texels precisely so that it
    // never eats more than the map can resolve, and a 0.156 m push-off applied
    // to a 0.0195 m map would erode eight texels of every hole — it would spend
    // the cascade's whole gain before the first fragment.
    const float params[4] = {enabled * shadow_dose, SHADOW_NORMAL_OFFSET_M,
                             SHADOW_DEPTH_BIAS_M / (2.0f * SHADOW_DEPTH_HALF_M),
                             SHADOW_NEAR_NORMAL_OFFSET_M};
    bgfx::setUniform(u_shadow_params, params);
    bgfx::setUniform(u_light_mtx, glm::value_ptr(light_mtx));
    bgfx::setUniform(u_light_mtx_near, glm::value_ptr(light_mtx_near));
}

// Largest integer factor of the internal target fitting the framebuffer,
// centered; never zero (Q9).
void BgfxRenderer::Impl::dest_rect(uint32_t& x, uint32_t& y, uint32_t& w,
                                   uint32_t& h) const {
    const uint32_t fx = internal_width > 0 ? fb_width / internal_width : 1;
    const uint32_t fy = internal_height > 0 ? fb_height / internal_height : 1;
    const uint32_t factor = std::min(fx, fy);
    if (factor >= 2) {
        // RETRO GRID: an integer factor, because that is the whole point of a
        // small internal target — every texel becomes an exact square block
        // and the pixel art stays pixel art.
        w = std::min(internal_width * factor, fb_width);
        h = std::min(internal_height * factor, fb_height);
    } else {
        // FULL-DETAIL GRID (internal ~ the window, e.g. the 1920x1080 default
        // since 15.08.2026): there is no pixel grid left to preserve, so the
        // image FITS the window instead. The integer rule here produced factor
        // 1 and framed a 1920x1080 picture in a 2560x1440 window with black
        // bars — the user's «че за черные края». Aspect is still preserved:
        // the letterbox exists for a mismatched aspect, not for a rounding
        // rule that stopped applying.
        const uint64_t by_w = static_cast<uint64_t>(internal_height) * fb_width;
        const uint64_t by_h = static_cast<uint64_t>(internal_width) * fb_height;
        if (by_w <= by_h) { // window is taller than the image: fill width
            w = fb_width;
            h = static_cast<uint32_t>(by_w / std::max<uint32_t>(internal_width, 1u));
        } else {            // window is wider: fill height
            h = fb_height;
            w = static_cast<uint32_t>(by_h / std::max<uint32_t>(internal_height, 1u));
        }
        w = std::min(w, fb_width);
        h = std::min(h, fb_height);
    }
    x = (fb_width - w) / 2;
    y = (fb_height - h) / 2;
}

void BgfxRenderer::begin_frame(const glm::mat4& view, const glm::mat4& proj) {
    Impl& im = *impl_;
    if (!im.initialized) {
        return;
    }
    im.in_frame = true;

    // В28 WIREFRAME + the per-frame stats/pick reset. The wireframe flag is
    // GLOBAL in bgfx (it would also line-draw the fullscreen upscale quad, i.e.
    // present a black screen with two diagonals), so in wireframe mode the scene
    // view is retargeted at the backbuffer here and the upscale is skipped in
    // end_frame. The app's projection already carries the framebuffer aspect
    // (App.cpp), so the full-backbuffer scene is aspect-correct with no fix.
    const bool wire = im.wireframe_on();
    bgfx::setDebug(wire ? BGFX_DEBUG_WIREFRAME : BGFX_DEBUG_NONE);
    im.scene_draws_accum = 0;
    im.scene_tris_accum = 0;
    im.pick_accum = RenderPick{};
    im.pick_best_t = 3.0e38f; // ~FLT_MAX; any real hit distance is nearer

    if (wire) {
        bgfx::setViewFrameBuffer(VIEW_SCENE, BGFX_INVALID_HANDLE);
        bgfx::setViewRect(VIEW_SCENE, 0, 0, static_cast<uint16_t>(im.fb_width),
                          static_cast<uint16_t>(im.fb_height));
    } else {
        bgfx::setViewFrameBuffer(VIEW_SCENE, im.internal_fb);
        bgfx::setViewRect(VIEW_SCENE, 0, 0,
                          static_cast<uint16_t>(im.internal_width),
                          static_cast<uint16_t>(im.internal_height));
    }
    // REVERSED-Z: the far plane is depth 0 and the near plane depth 1, so the
    // buffer clears to 0 and comparisons are GREATER. With CAMERA_FAR at 8 km
    // against a 0.1 m near plane this is what stops distant mountains from
    // z-fighting: floating-point depth has its precision where the exponent is
    // small, which reversed-Z lines up with the far distances instead of
    // wasting it all in the first metre.
    bgfx::setViewClear(VIEW_SCENE, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       SKY_COLOR_RGBA, 0.0f, 0);
    // Reverse the depth range here rather than in the camera: the depth buffer
    // format and its comparison direction are backend concerns, and
    // FirstPersonCamera stays a plain RH_ZO perspective that tests can reason
    // about. z' = 1 - z on a 0..1 clip range.
    glm::mat4 reversed = proj;
    reversed[0][2] = -proj[0][2];
    reversed[1][2] = -proj[1][2];
    reversed[2][2] = -proj[2][2] + proj[2][3];
    reversed[3][2] = -proj[3][2] + proj[3][3];
    bgfx::setViewTransform(VIEW_SCENE, glm::value_ptr(view), glm::value_ptr(reversed));
    bgfx::touch(VIEW_SCENE); // clear even on an empty frame

    // Shadow view: renders (view id order) before the scene consumes the map.
    const glm::mat4 inv_view = glm::inverse(view);
    im.frame_eye = glm::vec3(inv_view[3]);
    // В28 centre-of-screen pick ray: camera eye + forward (view space -Z). Set
    // here, consumed per draw in submit.
    im.pick_ray_origin = im.frame_eye;
    im.pick_ray_dir =
        glm::normalize(glm::vec3(inv_view * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    if (bgfx::isValid(im.shadow_fb)) {
        bgfx::setViewFrameBuffer(VIEW_SHADOW, im.shadow_fb);
        bgfx::setViewRect(VIEW_SHADOW, 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
        bgfx::setViewClear(VIEW_SHADOW, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
        bgfx::touch(VIEW_SHADOW);
    }
    // Skipped entirely when the cascade is off: an untouched view is a view
    // bgfx never submits, so a disabled cascade does not even pay a clear.
    if (bgfx::isValid(im.shadow_fb_near) && shadow_near_enabled()) {
        bgfx::setViewFrameBuffer(VIEW_SHADOW_NEAR, im.shadow_fb_near);
        bgfx::setViewRect(VIEW_SHADOW_NEAR, 0, 0, SHADOW_NEAR_MAP_SIZE,
                          SHADOW_NEAR_MAP_SIZE);
        bgfx::setViewClear(VIEW_SHADOW_NEAR, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
        bgfx::touch(VIEW_SHADOW_NEAR);
    }
    // Every empty draw of the frame happens HERE, before the first setUniform:
    // bgfx attaches pending uniforms to the next submitted draw, and a touch is
    // a draw that gets skipped, taking the uniforms with it.
    im.touch_point_shadow_views();
    im.update_shadow();

    // Frame environment (cached; struct defaults are valid pre-first-set).
    // order_lights runs FIRST: both the uniform packing and the cube-face
    // views read the ordered array, and they must never see different orders.
    im.order_lights();
    im.apply_environment();
    im.update_point_shadows();

    // Sky background: fullscreen quad, no depth test/write; everything solid
    // draws over it (sequential view — this is always the first draw).
    if (bgfx::isValid(im.sky_program)) {
        bgfx::setVertexBuffer(0, im.quad_vb);
        bgfx::setIndexBuffer(im.quad_ib);
        bgfx::setState(BGFX_STATE_WRITE_RGB);
        bgfx::submit(VIEW_SCENE, im.sky_program);
    }
}

void BgfxRenderer::set_environment(const RenderEnvironment& env) {
    impl_->environment = env;
    // Uniforms are (re)applied at the next begin_frame; if we are mid-frame,
    // update them now so this frame's remaining submits see the new values —
    // including the shadow matrices, so the app-animated sun (day/night, в2)
    // moves the shadows in the same frame it moves the light.
    if (impl_->initialized && impl_->in_frame) {
        impl_->order_lights();
        impl_->apply_environment();
        impl_->update_shadow();
        // The carried light moves with the player every frame, so its faces
        // are rebuilt here as well — a torch whose shadow map lagged the flame
        // by a frame would smear on every step.
        impl_->update_point_shadows();
    }
}

void BgfxRenderer::end_frame() {
    Impl& im = *impl_;
    if (!im.initialized || !im.in_frame) {
        return;
    }
    // В28: in wireframe mode the scene already rendered to the backbuffer
    // (begin_frame), so the letterbox + upscale present is skipped below.
    const bool wire = im.wireframe_on();

    // One-frame debug lines (accumulated by debug_line).
    if (!im.debug_lines.empty() && bgfx::isValid(im.debug_program)) {
        const auto count = static_cast<uint32_t>(im.debug_lines.size());
        if (bgfx::getAvailTransientVertexBuffer(count, im.debug_layout) >= count) {
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, count, im.debug_layout);
            std::memcpy(tvb.data, im.debug_lines.data(), count * sizeof(DebugVertex));
            bgfx::setVertexBuffer(0, &tvb, 0, count);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                           | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER
                           | BGFX_STATE_PT_LINES);
            bgfx::submit(VIEW_SCENE, im.debug_program);
        }
    }
    im.debug_lines.clear();

    // Letterbox clear, then the integer-scaled point-sampled upscale (Q9).
    // Skipped whole in wireframe mode: the scene is already on the backbuffer
    // and the upscale quad would be line-drawn by the global wireframe flag.
    if (!wire) {
        bgfx::setViewFrameBuffer(VIEW_BACKBUFFER, BGFX_INVALID_HANDLE);
        bgfx::setViewRect(VIEW_BACKBUFFER, 0, 0, static_cast<uint16_t>(im.fb_width),
                          static_cast<uint16_t>(im.fb_height));
        bgfx::setViewClear(VIEW_BACKBUFFER, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);
        bgfx::touch(VIEW_BACKBUFFER);

        if (bgfx::isValid(im.upscale_program)) {
            uint32_t dx = 0;
            uint32_t dy = 0;
            uint32_t dw = 0;
            uint32_t dh = 0;
            im.dest_rect(dx, dy, dw, dh);
            bgfx::setViewFrameBuffer(VIEW_UPSCALE, BGFX_INVALID_HANDLE);
            bgfx::setViewRect(VIEW_UPSCALE, static_cast<uint16_t>(dx),
                              static_cast<uint16_t>(dy), static_cast<uint16_t>(dw),
                              static_cast<uint16_t>(dh));
            const float post[4] = {im.palette_post ? 1.0f : 0.0f,
                                   static_cast<float>(PALETTE_SIZE),
                                   static_cast<float>(im.internal_width),
                                   static_cast<float>(im.internal_height)};
            bgfx::setUniform(im.u_post_params, post);
            // The floor rides the environment (it changes while the player turns
            // the calibration dial), the falloff is a project constant, so the two
            // halves of the curve come from the two places that own them.
            const float floor_params[4] = {im.environment.black_floor,
                                           static_cast<float>(config::BLACK_FLOOR_FALLOFF),
                                           0.0f, 0.0f};
            bgfx::setUniform(im.u_black_floor, floor_params);
            if (im.palette_post) {
                bgfx::setUniform(im.u_palette, im.palette.data(), PALETTE_SIZE);
            }
            bgfx::setTexture(0, im.s_tex_color, bgfx::getTexture(im.internal_fb, 0));
            bgfx::setVertexBuffer(0, im.quad_vb);
            bgfx::setIndexBuffer(im.quad_ib);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::submit(VIEW_UPSCALE, im.upscale_program);
        }
    }

    // AN ACCEPTANCE FRAME IS TAKEN FROM OUR OWN TARGET, NEVER FROM THE
    // BACKBUFFER. Four incidents in one day bought this: with the display
    // asleep or locked, Metal hands out no drawable, the backbuffer is left
    // unwritten, and requestScreenShot on it saves a BLACK or half-black
    // image. The zones then reported ground that "went black" and a frame with
    // a "letterbox bar" — two names for one undrawn surface, and both cost
    // real time to disprove.
    //
    // The capture target is the INTERNAL size and carries the SAME post the
    // screen gets: the upscale program is submitted a second time into it, so
    // the black floor and (when on) the palette are applied exactly as they
    // are for the window. Reading `internal_fb` directly would have been one
    // line and WRONG — it is the pre-post image, and every calibration frame
    // taken from it would quietly disagree with what the user sees.
    //
    // What it deliberately drops is the window's letterbox and its scaling: an
    // acceptance frame is now always internal-sized, whatever the window is.
    // Two frames from two window sizes become comparable, which they were not.
    if (!im.pending_screenshot.empty()) {
        if (!bgfx::isValid(im.capture_fb) && bgfx::isValid(im.upscale_program)) {
            const bgfx::TextureHandle tex = bgfx::createTexture2D(
                static_cast<uint16_t>(im.internal_width),
                static_cast<uint16_t>(im.internal_height), false, 1,
                bgfx::TextureFormat::RGBA8,
                BGFX_TEXTURE_RT | BGFX_TEXTURE_READ_BACK | BGFX_SAMPLER_U_CLAMP
                    | BGFX_SAMPLER_V_CLAMP);
            im.capture_fb = bgfx::createFrameBuffer(1, &tex, true);
        }
        if (bgfx::isValid(im.capture_fb) && !wire) {
            bgfx::setViewFrameBuffer(VIEW_CAPTURE, im.capture_fb);
            bgfx::setViewRect(VIEW_CAPTURE, 0, 0,
                              static_cast<uint16_t>(im.internal_width),
                              static_cast<uint16_t>(im.internal_height));
            const float post[4] = {im.palette_post ? 1.0f : 0.0f,
                                   static_cast<float>(PALETTE_SIZE),
                                   static_cast<float>(im.internal_width),
                                   static_cast<float>(im.internal_height)};
            bgfx::setUniform(im.u_post_params, post);
            const float floor_params[4] = {im.environment.black_floor,
                                           static_cast<float>(config::BLACK_FLOOR_FALLOFF),
                                           0.0f, 0.0f};
            bgfx::setUniform(im.u_black_floor, floor_params);
            if (im.palette_post) {
                bgfx::setUniform(im.u_palette, im.palette.data(), PALETTE_SIZE);
            }
            bgfx::setTexture(0, im.s_tex_color, bgfx::getTexture(im.internal_fb, 0));
            bgfx::setVertexBuffer(0, im.quad_vb);
            bgfx::setIndexBuffer(im.quad_ib);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::submit(VIEW_CAPTURE, im.upscale_program);
            // READ THE TEXTURE, do not ask bgfx for a "screen shot" of it.
            // requestScreenShot on Metal only serves a framebuffer that owns a
            // SWAP CHAIN (renderer_mtl.cpp: it looks up m_swapChain and RETURNS
            // SILENTLY when there is none), so pointing it at an offscreen
            // target writes nothing at all and says nothing — the first attempt
            // at this produced a sidecar .txt with no .png beside it. readTexture
            // is the path this file already uses for the point-shadow atlas.
            im.capture_data.assign(
                static_cast<std::size_t>(im.internal_width) * im.internal_height * 4u, 0u);
            im.capture_ready_frame = bgfx::readTexture(
                bgfx::getTexture(im.capture_fb, 0), im.capture_data.data());
            im.capture_path = im.pending_screenshot;
            im.capture_waiting = true;
        } else {
            // Wireframe skips the upscale entirely (the scene is already on the
            // backbuffer), so there is nothing of ours to photograph and the
            // backbuffer is the only truthful source. Said out loud rather than
            // silently producing a different kind of frame.
            std::fprintf(stderr, "[render] capture from the BACKBUFFER (wireframe): "
                                 "a blank frame here means the display gave no "
                                 "drawable\n");
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, im.pending_screenshot.c_str());
        }
        im.pending_screenshot.clear();
    }

    // POINT-SHADOW ATLAS READBACK (DFN_DUMP_POINT_ATLAS=<path>): one-shot blit
    // + readTexture of the cube-face atlas, written as raw R32F once ready.
    // Diagnostic door in the same family as DFN_PS_DEBUG: it answers "what do
    // the tiles actually HOLD" when the frame and the CPU-side geometry
    // disagree about what the map should contain. Unset, this whole block is
    // three loads and a branch.
    {
        static const char* dump_path = std::getenv("DFN_DUMP_POINT_ATLAS");
        static uint32_t dump_state = 0; // 0 idle, 1 scheduled, 2 done
        static uint32_t ready_frame = 0;
        static bgfx::TextureHandle dump_tex = BGFX_INVALID_HANDLE;
        static std::vector<float> dump_data;
        static uint32_t frame_no = 0;
        ++frame_no;
        if (dump_path != nullptr && *dump_path != '\0'
            && bgfx::isValid(im.point_shadow_atlas)) {
            // Wait until the world has streamed in and the captured pose has
            // settled: half the capture door's counted frames.
            if (dump_state == 0 && frame_no >= 300) {
                dump_tex = bgfx::createTexture2D(
                    POINT_SHADOW_ATLAS_W, POINT_SHADOW_ATLAS_H, false, 1,
                    bgfx::TextureFormat::R32F,
                    BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
                dump_data.resize(static_cast<std::size_t>(POINT_SHADOW_ATLAS_W)
                                 * POINT_SHADOW_ATLAS_H);
                bgfx::blit(VIEW_UPSCALE, dump_tex, 0, 0, im.point_shadow_atlas);
                ready_frame = bgfx::readTexture(dump_tex, dump_data.data());
                dump_state = 1;
                std::fprintf(stderr,
                             "[render] point atlas dump scheduled frame %u, "
                             "ready %u -> %s\n", frame_no, ready_frame, dump_path);
            }
        }
        const uint32_t done = bgfx::frame();
        // The pixels arrive a few frames after the read was scheduled; the
        // capture door already waits for the flush before closing the app.
        if (im.capture_waiting && done >= im.capture_ready_frame) {
            im.capture_waiting = false;
            im.callback.screenShot(im.capture_path.c_str(), im.internal_width,
                                   im.internal_height, im.internal_width * 4u,
                                   bgfx::TextureFormat::RGBA8, im.capture_data.data(),
                                   static_cast<uint32_t>(im.capture_data.size()), false);
        }
        if (dump_state == 1 && done >= ready_frame) {
            if (FILE* f = std::fopen(dump_path, "wb")) {
                std::fwrite(dump_data.data(), sizeof(float), dump_data.size(), f);
                std::fclose(f);
                std::fprintf(stderr, "[render] point atlas dump written: %s "
                                     "(%ux%u R32F)\n", dump_path,
                             POINT_SHADOW_ATLAS_W, POINT_SHADOW_ATLAS_H);
            }
            bgfx::destroy(dump_tex);
            dump_tex = BGFX_INVALID_HANDLE;
            dump_state = 2;
        }
    }

    // В28: LATCH the frame stats and the centre pick for the frame just
    // submitted. scene_draws/triangles are our own CPU counters; backend_draws
    // is read from bgfx AFTER frame() above, so it reflects the frame that was
    // just rendered (all views — shadows, sky, scene, upscale). bgfx exposes no
    // primitive count, which is why the triangle number is ours, not its.
    im.frame_stats.scene_draws = im.scene_draws_accum;
    im.frame_stats.scene_triangles = im.scene_tris_accum;
    im.frame_stats.backend_draws = bgfx::getStats()->numDraw;
    im.pick = im.pick_accum;

    // THE ACCEPTANCE DOOR for hooks 1 and 3 (Rule 27): DFN_FRAME_STATS=1 prints
    // the latched stats and pick every 30 frames, so both are checkable from the
    // shipped binary without an app-side overlay yet.
    static const bool stats_log = [] {
        const char* e = std::getenv("DFN_FRAME_STATS");
        return e != nullptr && e[0] != '\0' && e[0] != '0';
    }();
    if (stats_log) {
        static uint32_t sl_frame = 0;
        if ((sl_frame++ % 30) == 0) {
            std::fprintf(stderr,
                         "[render] frame_stats: scene_draws %u scene_tris %u "
                         "backend_draws %u | center_pick hit=%d id=%u tris=%u "
                         "dist=%.2f m\n",
                         im.frame_stats.scene_draws, im.frame_stats.scene_triangles,
                         im.frame_stats.backend_draws, im.pick.hit ? 1 : 0,
                         im.pick.pick_id, im.pick.triangles,
                         static_cast<double>(im.pick.distance_m));
        }
    }
    im.in_frame = false;
}

void BgfxRenderer::debug_line(const glm::vec3& from, const glm::vec3& to,
                              uint32_t color_rgba) {
#if defined(NDEBUG)
    (void)from;
    (void)to;
    (void)color_rgba;
#else
    impl_->debug_lines.push_back({from.x, from.y, from.z, color_rgba});
    impl_->debug_lines.push_back({to.x, to.y, to.z, color_rgba});
#endif
}

bool BgfxRenderer::save_screenshot(const std::string& path) {
    Impl& im = *impl_;
    if (!im.initialized) {
        return false;
    }
    // Scheduled into the next end_frame (bgfx captures during frame
    // processing); the Tour renders flush frames after scheduling.
    im.pending_screenshot = path;
    return true;
}

void BgfxRenderer::reload_shaders() {
    // Debug no-op this stage: shaders are embedded at build time. Disk-based
    // hot-reload (Q50) arrives with the stage-3 material work.
}

// В28 debug / editor introspection -------------------------------------------

// DFN_WIREFRAME=1 forces wireframe on regardless of set_wireframe (read once):
// the shipped app/tour binary is verifiable without an app change (Rule 27).
bool BgfxRenderer::Impl::wireframe_on() const {
    static const bool env = [] {
        const char* e = std::getenv("DFN_WIREFRAME");
        return e != nullptr && e[0] != '\0' && e[0] != '0';
    }();
    return wireframe || env;
}

void BgfxRenderer::set_wireframe(bool enabled) {
    impl_->wireframe = enabled;
}

const RenderFrameStats& BgfxRenderer::frame_stats() const {
    return impl_->frame_stats;
}

const RenderPick& BgfxRenderer::center_pick() const {
    return impl_->pick;
}

} // namespace dfn::platform
