/*
Created: 09:08:2026 - 22:12:57
Last updated: 15:08:2026 - 14:07:36
Module: engine/render
File: engine/render/sources/RenderSystemResources.cpp

Responsibility:
- The half of RenderSystem that manages RESOURCES AND SCREENS rather than the
  frame: carried-light collection, the overlay blit, the debug water plane and
  the per-body water meshes. Split out of RenderSystem.cpp, which had reached
  817 lines against the 800-line hard limit (Rule 21) before terrain LOD added
  a line to it.

Key items:
- RenderSystem::collect_point_lights, draw_overlay, set_internal_resolution,
  set_water / clear_water, set_water_bodies / clear_water_bodies.

Dependencies:
- Uses: RenderSystem.h, Materials.h, WaterMesher, engine/core (ecs, components).
- Used by: dfn_render.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This is the same class as RenderSystem.cpp, split only for the line limit:
  members and invariants are shared, so read both before changing either.
*/
/*
UPD:
- 09:08:2026 - 22:12:57: Split out of RenderSystem.cpp (Rule 21).
- 09:08:2026 - 23:50:06: set_water_bodies merges bodies into CHUNK_SIZE buckets.
  17336 LakePlanes on the 2x2 km testbed became 17336 buffer creates and 4078
  draw calls a frame, exhausting bgfx's 4096-handle pool AT STARTUP so that no
  terrain, scatter or site mesh could upload at all. 26 bucket meshes now.
  draw_overlay gained the blended path for the HUD.
- 10:08:2026 - 02:30:08: register_mesh — the seam for caller-authored geometry
  (character zone's body segments, ids 34..49, app-ferried). Refuses loudly:
  collisions, foreign id ranges, empty geometry.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 19:11:13: The one-caster workaround REMOVED, its cause fixed in
  BgfxRendererFrame.cpp (order_lights appended a caster twice past the cap):
  the nearest MAX_SHADOW_POINT_LIGHTS flames cast again. FLAME_INTENSITY_SWING /
  FLAME_WARMTH_SWING moved to SkyModel.h beside TORCH_COLOR -- a breathing flame
  makes a torch's colour a BAND, and every caller that asserts one has to read
  the width from where the oscillator reads it.
- 13:08:2026 - 20:42:22: DFN_TORCH_RADIUS=<metres> — дверь дозы на радиус факела по умолчанию (зона dungeon, по резке ведущего на этот файл и SkyModel.h). Заведена под замер «какой радиус нужен, чтобы пол под ногами читался»: обе руки обязаны выйти из ОДНОГО бинарника (правило 47), а нерабочее значение отвергается ВСЛУХ — молчаливый откат к боевому значению при отчёте о дозе есть единственный отказ, которого этой двери иметь нельзя. Все три места, читавшие TORCH_RADIUS_M как умолчание, теперь зовут одну функцию (правило 32): переносимый свет, ручная проба DFN_TORCH=1 и жаровня DFN_TORCH=2.
- 13:08:2026 - 23:49:31: РАСТВОРЕНИЕ НА ГРАНИЦЕ БЮДЖЕТА (зона dungeon, та же резка) — правка жалобы «свет мигает, иногда снова в глазах темнеет». publish_point_lights больше не делает жёсткую отсечку nth_element+resize(8): у восьмёрки ближайших пламён самое дальнее ГАСНЕТ гладким окном POINT_LIGHT_DISSOLVE_WINDOW_M по мере приближения к границе, так что факел, пересекающий границу «ближайших восьми», НАРАСТАЕТ, а не появляется скачком. Окно взводится только при конкуренции (кандидатов ≥ бюджета). Дверь дозы DFN_LIGHT_DISSOLVE=<м>, 0 = прежняя жёсткая отсечка (контрольная рука, правило 47). Плюс прибор DFN_LIGHT_PROBE=<путь>: по строке на кадр — luma пола от точечного света у ступней, ровно как считает dfn_env.sh; дефект — СТУПЕНЬ между соседними строками (правило 53), невидимая снимку. Точечный свет НЕ гейтится тьмой (в шейдере добавляется сыро), поэтому проба и есть то, что пол реально получает. ЗАМЕРЕНО: растворение на тоннеле НЕ СВЯЗЫВАЕТ (одновременно в радиусе ≤4 факелов при бюджете 8; своп за границей касается факелов за радиусом → 0). Диагноз «мигание от отсечки-8» опровергнут контролем; правка в дереве, НЕ в main.
- 14:08:2026 - 00:23:18: DFN_SHADOW_CASTERS=<n> — дверь дозы на число теневых casters (зона dungeon, резка ведущего на теневой путь). Заведена под ПИКСЕЛЬНЫЙ замер «мигает»: как player идёт, набор «двух ближайших» casters свопится, и своп перекидывает тень стены за кадр. Частоту померил (у близких факелов ~0.3/с), а МАГНИТУДУ (виден ли прыжок в пикселях) — нет; эта дверь её меряет: n=1 изолирует всю тень 2-го caster'а (его наличие/отсутствие ограничивает величину свопа), n=2 боевое, n=0 = DFN_NO_POINT_SHADOW. Обе руки один бинарник (правило 47), кривое значение отвергается вслух.
- 14:08:2026 - 18:30:20: ПРИЗЕМЛЕНО ВЕДУЩИМ — работа зоны dungeon висела в дереве без коммита (агент оборвался посреди замера). Что именно приземляется и в каком виде, чтобы никто не прочитал это как «мигание починено». (1) РАСТВОРЕНИЕ — не починка жалобы, а снятие разрыва в самом отборе: ранг 8 светит полностью, ранг 9 — ровно нулём, и своп между ними есть скачок ПО ПОСТРОЕНИЮ. На сегодняшнем содержимом окно НЕ ВЗВОДИТСЯ (замер зоны: одновременно в радиусе ≤4 факелов при бюджете 8), поэтому кадр не меняется ни на пиксель; разрыв снят на то время, когда факелов станет больше слотов. (2) Три двери дозы — DFN_LIGHT_DISSOLVE, DFN_SHADOW_CASTERS, DFN_LIGHT_PROBE — приборы, ради которых замер вообще возможен, и они ценнее правки. (3) ЧЕСТНО ОБ ОТКРЫТОМ: диагноз «мигание от отсечки-8» ОПРОВЕРГНУТ контролем самой зоны, а магнитуда свопа теневых casters (частота у близких факелов ~0.3/с) НЕ ЗАМЕРЕНА — дверь DFN_SHADOW_CASTERS заведена ровно под неё и осталась неиспользованной; жалоба «свет мигает» остаётся ОТКРЫТОЙ. Сборка зелёная, новых падений в наборе нет.
- 14:08:2026 - 20:33:26: DFN_CASTER_SKIP=<k> — дверь дозы на то, КАКОЕ пламя владеет вторым теневым слотом, и она заведена потому, что DFN_SHADOW_CASTERS не смогла ответить на свой собственный вопрос. Своп не УБИРАЕТ второй caster, он его ЗАМЕНЯЕТ: набор идёт {ближайший, ранг 1} → {ближайший, ранг 2} за один кадр. n=1 меряет первый набор против {ближайший} — это ВЕРХНЯЯ ГРАНИЦА свопа, а не своп. Попытка померить своп двумя позами в 0.66 м ПРОВАЛИЛАСЬ и записана как провал (правило 50): контрольная рука «тот же набор, 0.6 м врозь» дала средний |ΔL| 11.61 против 11.61→12.35 у рабочей, то есть плечо прибора длиннее предмета. Обе руки обязаны сниматься с ОДНОЙ позы, и k=0 побитово боевой. ЗАМЕРЕНО (одна поза, один бинарник, 600 кадров): нулевая доза (k=0 дважды) — средний |ΔL| 0.141, 1.65 % кадра; своп ранг1→ранг2 — 3.798 и 37.41 % кадра, p90 16.04 люмы при шаге палитры 19.99; ранг1→ранг3 — 3.294 и 29.60 %. Глазами: пол ПОД НОГАМИ игрока при k=0 абсолютно чёрный, при k=1 — видимый освещённый. Своп идёт 30 раз за 13234 кадра живого прохода (0.25/с). Это и есть «мигает» в оставшейся форме, и оно ИЗМЕРЕНО, а не предположено.
- 15:08:2026 - 14:07:36: У ИНТЕРФЕЙСА СВОЯ СЕТКА. Холст HUD больше не повторяет цель сцены
  один в один: глиф 5×7 читается на 640×360 и не читается на 1920×1080, потому
  что мерится ТЕКСЕЛЯМИ, а экран вырос вокруг него в девять раз (жалоба «че с
  текстом произошло» при подъёме умолчания до Full HD). Оверлей рисуется
  полноэкранным квадом, поэтому число пикселей холста решает только КРУПНОСТЬ
  интерфейса, а не его размер. Делитель ЦЕЛЫЙ — тексели интерфейса ложатся на
  целые пиксели сцены, и текст не мерцает при ходьбе.
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/render/sources/PathMesher.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/Materials.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/render/sources/SkyModel.h" // TORCH_COLOR / TORCH_RADIUS_M
#include "engine/render/sources/WaterMesher.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <utility>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>

namespace dfn::render {

// THE DOSE DOOR FOR A TORCH'S REACH (DFN_TORCH_RADIUS=<metres>, zone dungeon
// under a cut from the lead on this file and on SkyModel.h).
//
// It exists because the two arms of "does a torch light the floor" have to come
// out of ONE BINARY (Rule 47): the shipping value and the tried value differ by
// this variable and by nothing else. It overrides the DEFAULT only -- a
// CarriedLight that names its own radius keeps it, exactly as without the door.
//
// The number it names is the NOMINAL radius. What reaches the surface is
// nominal * (1 - 0.55 * ambient_darkness) (dfn_env.sh), so in a pitch-dark
// place the effective reach is 0.45 of what is set here.
[[nodiscard]] float torch_radius_default() {
    static const float value = [] {
        if (const char* e = std::getenv("DFN_TORCH_RADIUS"); e != nullptr && *e != '\0') {
            float r = 0.0f;
            if (std::sscanf(e, "%f", &r) == 1 && r > 0.0f) {
                std::fprintf(stderr, "[torch] DFN_TORCH_RADIUS=%.3f m (default %.3f)\n",
                             static_cast<double>(r), static_cast<double>(TORCH_RADIUS_M));
                return r;
            }
            // Loud, never silent: a run that fell back to the shipping value
            // while reporting a dose is the one failure this door must not have.
            std::fprintf(stderr, "[torch] DFN_TORCH_RADIUS=\"%s\" is not a positive "
                                 "number -- REFUSED, using %.3f m\n",
                         e, static_cast<double>(TORCH_RADIUS_M));
        }
        return TORCH_RADIUS_M;
    }();
    return value;
}

// THE DOSE DOOR FOR THE BUDGET-EDGE DISSOLVE (DFN_LIGHT_DISSOLVE=<metres>).
//
// The frame has MAX_POINT_LIGHTS slots and a corridor has more torches than
// that, so a flame past the budget contributes exactly zero and the "nearest
// eight" set changes discretely as the player walks — the marginal flame pops
// from full to nothing in one frame. That pop is the user's «свет мигает».
//
// The two arms — hard cutoff and dissolve — MUST come from one binary
// (Rule 47): 0 is the control (the shipping-before behaviour), a positive
// value is the ramp width. A malformed value is refused OUT LOUD, never a
// silent fall back to the shipping default while a dose is being reported.
[[nodiscard]] float point_light_dissolve_window() {
    static const float value = [] {
        if (const char* e = std::getenv("DFN_LIGHT_DISSOLVE"); e != nullptr && *e != '\0') {
            float w = -1.0f;
            if (std::sscanf(e, "%f", &w) == 1 && w >= 0.0f) {
                std::fprintf(stderr, "[light] DFN_LIGHT_DISSOLVE=%.3f m (default %.3f)\n",
                             static_cast<double>(w),
                             static_cast<double>(POINT_LIGHT_DISSOLVE_WINDOW_M));
                return w;
            }
            std::fprintf(stderr, "[light] DFN_LIGHT_DISSOLVE=\"%s\" is not a "
                                 "non-negative number -- REFUSED, using %.3f m\n",
                         e, static_cast<double>(POINT_LIGHT_DISSOLVE_WINDOW_M));
        }
        return POINT_LIGHT_DISSOLVE_WINDOW_M;
    }();
    return value;
}

// THE DOSE DOOR FOR THE SHADOW-CASTER COUNT (DFN_SHADOW_CASTERS=<n>).
//
// The scene casts point shadows from the MAX_SHADOW_POINT_LIGHTS nearest flames.
// As the player walks, WHICH two those are swaps, and a swap flips a wall from
// one flame's shadow to another's in one frame — the candidate for «свет
// мигает». To measure whether that flip is VISIBLE (Rule 41: the frequency is
// not the magnitude), this caps how many flames cast: n=2 is shipping, n=1
// isolates the SECOND caster's whole shadow footprint (its presence vs absence
// bounds the swap's magnitude), n=0 is DFN_NO_POINT_SHADOW. Both arms out of one
// binary (Rule 47); a malformed value is refused out loud.
[[nodiscard]] uint32_t shadow_caster_cap() {
    static const uint32_t value = [] {
        if (const char* e = std::getenv("DFN_SHADOW_CASTERS"); e != nullptr && *e != '\0') {
            int n = -1;
            if (std::sscanf(e, "%d", &n) == 1 && n >= 0
                && static_cast<uint32_t>(n) <= platform::MAX_SHADOW_POINT_LIGHTS) {
                std::fprintf(stderr, "[shadow] DFN_SHADOW_CASTERS=%d (default %u)\n",
                             n, platform::MAX_SHADOW_POINT_LIGHTS);
                return static_cast<uint32_t>(n);
            }
            std::fprintf(stderr, "[shadow] DFN_SHADOW_CASTERS=\"%s\" is not an "
                                 "integer in [0, %u] -- REFUSED, using %u\n",
                         e, platform::MAX_SHADOW_POINT_LIGHTS,
                         platform::MAX_SHADOW_POINT_LIGHTS);
        }
        return platform::MAX_SHADOW_POINT_LIGHTS;
    }();
    return value;
}

// THE DOSE DOOR FOR *WHICH* FLAME OWNS THE SECOND CASTER SLOT
// (DFN_CASTER_SKIP=<k>), and it exists because DFN_SHADOW_CASTERS could not
// answer the question it was opened for.
//
// A swap does not REMOVE the second caster, it REPLACES it: the set goes from
// {nearest, rank 1} to {nearest, rank 2} in one frame. DFN_SHADOW_CASTERS=1
// measures the first set against {nearest} alone, which bounds the swap from
// above but is not the swap. Measuring the swap by standing 0.66 m apart —
// where the sets really do differ — FAILED, and the failure is the reason this
// door exists: the parallax control (same set, 0.6 m apart) moved the frame by
// mean 11.61 luma against the swap arm's 12.35, i.e. the instrument's own arm
// was longer than the subject (Rule 50). Both sets have to be photographed
// from ONE pose.
//
// k = 0 is shipping, bit for bit. k = 1 hands the second slot to the flame
// that would own it one frame after the swap. Both arms out of one binary
// (Rule 47); a malformed value is refused OUT LOUD.
[[nodiscard]] uint32_t caster_skip() {
    static const uint32_t value = [] {
        if (const char* e = std::getenv("DFN_CASTER_SKIP"); e != nullptr && *e != '\0') {
            int k = -1;
            if (std::sscanf(e, "%d", &k) == 1 && k >= 0 && k <= 8) {
                std::fprintf(stderr, "[shadow] DFN_CASTER_SKIP=%d (default 0)\n", k);
                return static_cast<uint32_t>(k);
            }
            std::fprintf(stderr, "[shadow] DFN_CASTER_SKIP=\"%s\" is not an "
                                 "integer in [0, 8] -- REFUSED, using 0\n", e);
        }
        return 0u;
    }();
    return value;
}

// FLOOR-ILLUMINATION PROBE (DFN_LIGHT_PROBE=<path>) — one line per PRESENTED
// frame, no readback, no settle, no freeze. It measures the quantity the flicker
// LIVES IN (Rule 47): the sum of the published point lights' contribution at the
// player's feet, exactly as dfn_env.sh evaluates it (smoothstep on 1 - d/radius,
// times the up-facing floor's N.L), reduced to luma. Occlusion is taken as 1 —
// only the two nearest flames cast, and in a wall-torch pass none is carried, so
// the far sconces whose SWAP causes the flicker are unshadowed. The point-light
// term is NOT gated by ambient darkness (it is added raw in the shader), so this
// value is what the floor actually receives, and its FRAME-TO-FRAME STEP is the
// flicker — invisible to any single screenshot (Rule 53, count the transitions).
void light_floor_probe(const platform::RenderEnvironment& env, const glm::vec3& eye,
                       std::FILE* out) {
    static uint64_t frame = 0;
    const glm::vec3 feet = eye - glm::vec3{0.0f,
        static_cast<float>(config::PLAYER_EYE_HEIGHT), 0.0f};
    const glm::vec3 n{0.0f, 1.0f, 0.0f}; // the floor faces up
    float lum = 0.0f;
    float min_dist = 1e9f;
    uint32_t reaching = 0;
    for (uint32_t i = 0; i < env.point_light_count; ++i) {
        const platform::PointLight& L = env.point_lights[i];
        const glm::vec3 to = L.position - feet;
        const float dist = glm::length(to);
        if (dist < min_dist) { min_dist = dist; }
        const float atten = std::clamp(1.0f - dist / std::max(L.radius_m, 1e-4f), 0.0f, 1.0f);
        if (atten > 0.0f) { ++reaching; }
        const float smooth = atten * atten * (3.0f - 2.0f * atten);
        const float ndl = std::max(n.x * to.x + n.y * to.y + n.z * to.z, 0.0f)
                          / std::max(dist, 1e-4f);
        const glm::vec3 c = L.color * (smooth * ndl);
        lum += 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
    }
    // The shadow casters are the first MAX_SHADOW_POINT_LIGHTS published lights
    // (sorted nearest-first, casts_shadow set on them). Log their positions so
    // an offline pass can count how often the CASTER SET swaps as the player
    // walks — the candidate for «свет мигает» the floor-luma probe is blind to,
    // because shadows are near-binary and the probe assumes occl = 1.
    auto caster = [&](uint32_t i) -> glm::vec3 {
        return i < env.point_light_count ? env.point_lights[i].position
                                         : glm::vec3{0.0f};
    };
    const glm::vec3 c0 = caster(0);
    const glm::vec3 c1 = caster(1);
    const float c0d = env.point_light_count > 0 ? glm::length(c0 - feet) : -1.0f;
    const float c1d = env.point_light_count > 1 ? glm::length(c1 - feet) : -1.0f;
    // luma on a 0..255 scale, the same unit the feet0 box and PALETTE_SHADE_STEP
    // are quoted in. Trailing diagnostic columns: nearest light distance, how
    // many of the published lights actually reach the feet, feet height, then the
    // two shadow casters' (x z dist) for the swap count.
    std::fprintf(out, "%llu %u %.5f %.4f %.3f %u %.3f  %.2f %.2f %.2f  %.2f %.2f %.2f\n",
                 static_cast<unsigned long long>(frame), env.point_light_count,
                 static_cast<double>(lum * 255.0f), static_cast<double>(lum),
                 static_cast<double>(min_dist), reaching,
                 static_cast<double>(feet.y),
                 static_cast<double>(c0.x), static_cast<double>(c0.z), static_cast<double>(c0d),
                 static_cast<double>(c1.x), static_cast<double>(c1.z), static_cast<double>(c1d));
    ++frame;
}

// FLAME FLICKER (the user asked for it by name: «анимацию пламени на факеле»).
// A carried flame is not a light bulb: it breathes. This modulates INTENSITY
// and COLOUR only -- never the radius -- and the reason is the defect this zone
// spent the day on: a moving radius makes the lit patch's EDGE crawl across the
// floor every frame, which is the same class of between-frames artefact as the
// darkness meander, delivered by the fix for something else.
//
// Two incommensurate sines, no state, no random: a flame reads as alive because
// it never repeats. THE RATES ARE NOT IN HERTZ, and this line used to say they
// were: `5.7f * 6.2831853f * 0.1591549f` multiplies by tau and by 1/tau, which
// CANCEL, so the argument is 5.7 rad/s and the flame breathes at 0.91 and
// 1.45 Hz with a beat of 1.85 s, not at 5.7/9.1 Hz with a beat of 0.29 s. Found
// by a test that predicted a sample pair from the numbers written here and
// measured half what it predicted (RenderSystemTests, the breathing case).
// LEFT AS MEASURED rather than "corrected" to the documented rates: whether a
// torch should flutter at 1 Hz or at 6 is a look-dev decision with an owner,
// and the two swings below are already on the same list. Driven by the VISUAL
// clock (env.time_seconds),
// which DFN_VISTIME and DFN_WIND_FREEZE already pin -- so every screenshot
// recipe in the project stays deterministic, and a flame that broke the tour's
// determinism would be a fix that costs four zones their acceptance.
//
// The phase is offset per light index so a corridor of wall torches breathes
// out of step rather than pulsing as one lamp.
//
// AMPLITUDE IS LOOK-DEV AND WANTS A ROW (Rule 14): 0.12 of intensity and 0.05
// of warmth are placeholders chosen to sit under the eye's flicker-fusion
// threshold for a light source of this size, and they are marked as such rather
// than smuggled in. Requested from the lead with the falloff numbers.
// The two swings MOVED to SkyModel.h, beside TORCH_COLOR: with a breathing
// flame a torch's colour is a BAND, and every caller that asserts one has to
// read the band's width from the same place the oscillator does.
namespace {

struct Flame {
    float intensity;
    float warmth; // >0 = toward the ember, <0 = toward the pale tip
};

[[nodiscard]] Flame flame_at(float t, uint32_t index) {
    const float phase = static_cast<float>(index) * 1.7f;
    const float a = std::sin((t + phase) * 5.7f * 6.2831853f * 0.1591549f);
    const float b = std::sin((t + phase) * 9.1f * 6.2831853f * 0.1591549f + 2.399f);
    const float mix = 0.6f * a + 0.4f * b;
    return {1.0f + FLAME_INTENSITY_SWING * mix, FLAME_WARMTH_SWING * mix};
}
} // namespace


// The frame's eight slots, given to the nearest flames. Kept out of
// collect_point_lights so the shadow rule is stated exactly once: the two
// NEAREST casters get cube maps, which after the sort means the light the
// player is standing next to -- the carried torch, when he holds one.
void RenderSystem::publish_point_lights(std::vector<PointLightCandidate>& candidates) {
    const uint32_t budget = platform::MAX_POINT_LIGHTS;
    // Full sort: the dissolve needs the budget-edge distance, and the kept set is
    // at most a few dozen candidates, so the nth_element optimisation buys
    // nothing over sorting them all.
    std::sort(candidates.begin(), candidates.end(),
              [](const PointLightCandidate& a, const PointLightCandidate& b) {
                  return a.d2 < b.d2;
              });

    // THE BUDGET DISSOLVE (the fix for «свет мигает»). With more flames than
    // slots, the "nearest eight" set changes discretely as the player walks: the
    // flame at rank 8 and the flame at rank 9 swap when their distances cross,
    // and rank 9 contributes ZERO while rank 8 contributes fully — a one-frame
    // pop. The cure is to fade the farthest KEPT light to zero as its distance
    // approaches the crossover, so a flame swapping across the boundary is
    // already dark at the instant it enters. The window is armed only when there
    // is competition (as many candidates as slots — a ninth may cross in next
    // frame); with slots to spare no light is at risk and none is dimmed.
    const float window = point_light_dissolve_window();
    const bool arm_fade = window > 0.0f && candidates.size() >= budget;
    // The edge is the distance of the last kept flame; at the swap instant it
    // equals the distance of the first dropped one, which is why fading toward
    // zero here makes the swap invisible.
    const float edge = arm_fade ? std::sqrt(candidates[budget - 1].d2) : 0.0f;

    if (candidates.size() > budget) {
        candidates.resize(budget);
    }
    uint32_t count = 0;
    for (const PointLightCandidate& c : candidates) {
        platform::PointLight& out = environment_.point_lights[count];
        out.position = c.position;
        out.radius_m = c.radius;
        out.color = c.color;
        if (arm_fade) {
            // smoothstep: 1 at edge-window (full brightness), 0 at the edge.
            const float d = std::sqrt(c.d2);
            const float t = std::clamp((edge - d) / window, 0.0f, 1.0f);
            out.color *= t * t * (3.0f - 2.0f * t);
        }
        // THE NEAREST TWO FLAMES CAST, and the cap is the contract's own.
        //
        // This line stood at one caster, not two, for as long as the backend
        // crashed on the second: `order_lights()` recomputed each light's
        // caster flag from a counter its own first pass advanced, so past the
        // cap the casters already emitted were appended a SECOND time and
        // `light_count` ran off the end of `std::array<PointLight, 8>`. That
        // defect is fixed at its source (BgfxRendererFrame.cpp, the predicate
        // is now decided once), so the workaround is gone rather than
        // documented: capping HERE would have kept the world at one shadowing
        // flame forever, in a file that has no idea why.
        // Rank 0 always casts; the SECOND slot goes to rank 1 + DFN_CASTER_SKIP,
        // which is 1 in every shipping frame and is the whole instrument for
        // the swap (see caster_skip()).
        const uint32_t second = 1u + caster_skip();
        out.casts_shadow = !point_shadows_off_
                           && (count == 0u ? shadow_caster_cap() > 0u
                                           : (count == second
                                              && shadow_caster_cap() > 1u));
        ++count;
    }
    environment_.point_light_count = count;
}

void RenderSystem::collect_point_lights(ecs::World& world,
                                        const FirstPersonCamera& camera,
                                        float alpha) {
    // THE BUDGET IS EIGHT AND A CORRIDOR NOW HAS MORE THAN EIGHT TORCHES, so
    // the slots go to the NEAREST flames rather than to whichever the ECS
    // happened to walk first. Collection order was a fair rule while the player
    // owned the only light in the world; with sconces down a tunnel it would
    // hand the budget to the far end and leave the player in the dark beside a
    // burning torch. Gathered, then partially sorted by distance to the eye.
    std::vector<PointLightCandidate> candidates;
    const glm::vec3 eye = camera.interpolated_pose(alpha).position;
    const auto add = [&](const glm::vec3& position, float radius,
                         const glm::vec3& color) {
        if (radius <= 0.0f) {
            return;
        }
        const glm::vec3 to = position - eye;
        candidates.push_back({position, color, radius, glm::dot(to, to)});
    };

    world.view<components::CarriedLight, components::Transform>().each(
        [&](ecs::EntityId id, components::CarriedLight& light,
            components::Transform& curr) {
            if (!light.active) {
                return; // held but not lit — gameplay keeps the component
            }
            // Interpolate like any other renderable (Rule 12); a light that
            // moved at the fixed rate would strobe against 60+ fps geometry.
            glm::vec3 position = curr.position;
            glm::quat rotation = curr.rotation;
            if (const auto* prev = world.get<components::PreviousTransform>(id)) {
                position = glm::mix(prev->position, curr.position, alpha);
                rotation = glm::slerp(prev->rotation, curr.rotation, alpha);
            }
            // The offset is CARRIER-LOCAL: sim writes the hand, ~1.45 m above
            // the feet and 0.35 m to the right, and rotating it by the body
            // yaw is what makes the shadows swing when the player turns.
            const float radius = light.radius_m > 0.0f ? light.radius_m
                                                       : torch_radius_default();
            glm::vec3 color = TORCH_COLOR;
            if (light.color_rgb != 0u) {
                color = {static_cast<float>((light.color_rgb >> 16) & 0xFFu) / 255.0f,
                         static_cast<float>((light.color_rgb >> 8) & 0xFFu) / 255.0f,
                         static_cast<float>(light.color_rgb & 0xFFu) / 255.0f};
            }
            // The flame breathes: intensity and warmth, never the radius.
            const Flame f = flame_at(environment_.time_seconds,
                                     static_cast<uint32_t>(candidates.size()));
            color *= f.intensity;
            color.g *= 1.0f - f.warmth * 0.5f; // toward ember / toward pale tip
            color.b *= 1.0f - f.warmth;
            add(position + rotation * light.offset, radius, color);
        });

    // Verification hook only (Rule 27): the tour freezes the player and no
    // entity carries a torch during a screenshot run, so DFN_TORCH=1 lights
    // one at the camera's hand. It stands down the moment a real CarriedLight
    // exists, so it can never quietly become the shipping path.
    if (torch_debug_ && candidates.empty()) {
        const CameraPose pose = camera.interpolated_pose(alpha);
        const glm::vec3 right = camera.right(alpha);
        if (torch_ahead_m_ > 0.0f) {
            // BRAZIER PROBE (DFN_TORCH=2): the light stands away from the eye,
            // in front of the camera. A carried flame sits 0.35 m from the eye,
            // so nearly every shadow it casts hides BEHIND its own caster —
            // true of any real hand-held light and the reason interiors, not
            // open ground, are its acceptance test. Standing the same light off
            // at a distance puts casters between eye and flame, which is what
            // makes the cube map's work visible in one open-ground frame.
            const glm::vec3 fwd = camera.forward(alpha);
            const glm::vec3 flat = glm::normalize(glm::vec3{fwd.x, 0.0f, fwd.z}
                                                  + glm::vec3{1e-4f, 0.0f, 0.0f});
            const Flame fb = flame_at(environment_.time_seconds, 0u);
            glm::vec3 bc = TORCH_COLOR * fb.intensity;
            bc.g *= 1.0f - fb.warmth * 0.5f;
            bc.b *= 1.0f - fb.warmth;
            add(pose.position + flat * torch_ahead_m_, torch_radius_default(), bc);
            publish_point_lights(candidates);
            if (dark_frozen_) {
                environment_.ambient_darkness = frozen_darkness_;
            }
            return;
        }
        // Flattened forward: a hand does not rise when the eyes look up, and
        // sim's real CarriedLight is yaw-only for exactly the same reason.
        const glm::vec3 fwd = camera.forward(alpha);
        const glm::vec3 flat = glm::normalize(glm::vec3{fwd.x, 0.0f, fwd.z}
                                              + glm::vec3{1e-4f, 0.0f, 0.0f});
        const Flame fh = flame_at(environment_.time_seconds, 0u);
        glm::vec3 hc = TORCH_COLOR * fh.intensity;
        hc.g *= 1.0f - fh.warmth * 0.5f;
        hc.b *= 1.0f - fh.warmth;
        add(pose.position + right * 0.35f + flat * 0.15f
                - glm::vec3{0.0f, 0.25f, 0.0f},
            torch_radius_default(), hc);
    }
    publish_point_lights(candidates);
    if (dark_frozen_) {
        environment_.ambient_darkness = frozen_darkness_;
    }
    // Flicker instrument (DFN_LIGHT_PROBE=<path>): one line per presented frame,
    // the floor's point-light luma at the feet. Opened once; the defect is the
    // STEP between adjacent lines, which no screenshot can show (Rule 53).
    if (const char* p = std::getenv("DFN_LIGHT_PROBE"); p != nullptr && *p != '\0') {
        static std::FILE* probe = [p] {
            std::FILE* f = std::fopen(p, "w");
            if (f != nullptr) {
                std::fprintf(f, "# frame point_light_count floor_luma_0_255 floor_luma_frac\n");
            } else {
                std::fprintf(stderr, "[light] DFN_LIGHT_PROBE=\"%s\" could not be "
                                     "opened for writing\n", p);
            }
            return f;
        }();
        if (probe != nullptr) {
            light_floor_probe(environment_, eye, probe);
            std::fflush(probe);
        }
    }
}

void RenderSystem::set_internal_resolution(uint32_t width, uint32_t height) {
    if (width > 0 && height > 0) {
        internal_res_ = {width, height};
        // THE HUD KEEPS ITS OWN GRID, and this is the fix for the user's «че с
        // текстом произошло» when the default rose to 1920x1080.
        //
        // The HUD used to follow the scene target one-for-one. That was right
        // while the scene target WAS the design grid: a 5x7 bitmap glyph is
        // legible at 640x360 and illegible at 1920x1080, because the glyph is
        // measured in TEXELS and the screen grew nine times around it. The
        // overlay is drawn as a full-screen quad textured with this canvas, so
        // the canvas' pixel count decides only how CHUNKY the interface is,
        // never how big — which means the interface can hold the grid it was
        // authored in while the world renders at full detail.
        //
        // An integer divisor (not a fixed 640x360) so the HUD's texels still
        // land on whole scene pixels: no resampling shimmer on text that the
        // player reads while walking.
        const auto design_w = static_cast<uint32_t>(config::DESIGN_RES_W);
        const uint32_t divisor = std::max(1u, (width + design_w / 2u) / design_w);
        hud_.resize(std::max(1u, width / divisor), std::max(1u, height / divisor));
        hud_.clear_transparent();
    }
}

void RenderSystem::draw_overlay(platform::IRenderer& renderer, const PixelCanvas& canvas,
                                const FirstPersonCamera& camera, float alpha,
                                bool blended) {
    // The blended path falls back to the opaque program rather than dropping
    // the draw: a HUD that silently disappears because one program failed to
    // load is exactly the "absence looks neutral" failure this project keeps
    // paying for. An opaque HUD is wrong and VISIBLE.
    const uint32_t program = blended && overlay_program_ != 0 ? overlay_program_
                                                              : unlit_program_;
    if (overlay_mesh_ == 0 || program == 0 || canvas.width() == 0) {
        return;
    }
    // One upload per frame: IRenderer has no texture update (frozen contract),
    // so the canvas texture is recreated. At 640x360 that is 900 KB — cheap,
    // and only while a screen is open.
    if (overlay_texture_ != 0) {
        renderer.destroy_texture(platform::TextureHandle{overlay_texture_});
        overlay_texture_ = 0;
    }
    const platform::TextureHandle texture =
        renderer.create_texture(canvas.width(), canvas.height(),
                                platform::TextureFormat::RGBA8, canvas.pixels());
    if (!texture.valid()) {
        return;
    }
    overlay_texture_ = texture.id;

    // Quad placed just past the near plane, sized to EXACTLY fill the frustum
    // there (half height = d * tan(fov/2)), so one canvas pixel lands on one
    // internal pixel and the point sampler stays crisp (an overscan factor
    // duplicated pixel columns and softened the map). Depth test LESS lets it
    // cover every earlier submit.
    const float depth = std::max(camera.near_plane(), 0.01f) * 1.5f;
    const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
    const float half_w = half_h * camera.aspect_ratio();
    const glm::mat4 model = glm::inverse(camera.view(alpha))
                          * glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, -depth})
                          * glm::scale(glm::mat4(1.0f), {half_w, half_h, 1.0f});
    renderer.submit(platform::MeshHandle{overlay_mesh_},
                    platform::ProgramHandle{program}, model, texture);
}

void RenderSystem::set_water(platform::IRenderer& renderer, float height_m,
                             glm::vec2 center_xz, float half_extent_m) {
    clear_water(renderer);

    // One quad; uv in water-tile units so the texture repeats every
    // LOOKDEV_WATER_UV_TILE_M meters (sampler wraps, shader scrolls).
    const float x0 = center_xz.x - half_extent_m;
    const float x1 = center_xz.x + half_extent_m;
    const float z0 = center_xz.y - half_extent_m;
    const float z1 = center_xz.y + half_extent_m;
    const float inv_tile = 1.0f / LOOKDEV_WATER_UV_TILE_M;
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const std::array<platform::Vertex, 4> vertices{{
        {{x0, height_m, z0}, up, {x0 * inv_tile, z0 * inv_tile}, 0xFFFFFFFFu},
        {{x1, height_m, z0}, up, {x1 * inv_tile, z0 * inv_tile}, 0xFFFFFFFFu},
        {{x0, height_m, z1}, up, {x0 * inv_tile, z1 * inv_tile}, 0xFFFFFFFFu},
        {{x1, height_m, z1}, up, {x1 * inv_tile, z1 * inv_tile}, 0xFFFFFFFFu},
    }};
    const std::array<uint32_t, 6> indices{0, 3, 1, 0, 2, 3}; // CCW from +Y
    const platform::MeshHandle handle = renderer.create_mesh(vertices, indices);
    if (!handle.valid()) {
        return;
    }
    water_mesh_ = handle.id;
    // Beaches: sand band sits just above the waterline (tunable via environment()).
    environment_.sand_height_m = height_m + LOOKDEV_SAND_BLEND_M * 0.5f;
}

void RenderSystem::clear_water(platform::IRenderer& renderer) {
    if (water_mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{water_mesh_});
        water_mesh_ = 0;
    }
}
void RenderSystem::set_water_bodies(platform::IRenderer& renderer,
                                    std::span<const math::LakePlane> lakes,
                                    std::span<const math::RiverStation> river_stations,
                                    std::span<const uint32_t> river_segment_offsets) {
    clear_water_bodies(renderer);

    // ONE GPU MESH PER WATER BODY WAS THE BUG. Core's hydrology emits a
    // LakePlane per pond, and the 2x2 km testbed has 17336 of them. bgfx hands
    // out 4096 vertex-buffer handles; the lakes alone consumed every one of
    // them AT STARTUP, so every terrain chunk, every scatter batch and every
    // site mesh afterwards failed to create — and the failed handles, stored as
    // if they were real, killed the process on exit. The count of bodies is
    // core's business and it is not wrong; spending a draw call and a buffer on
    // each of them was mine.
    //
    // Bodies are therefore MERGED into buckets on a CHUNK_SIZE world grid.
    // Cost becomes proportional to world area / chunk area (64 buckets at
    // 2x2 km, 1600 at 10x10 km) instead of to the number of puddles, and each
    // bucket carries an AABB so the frustum can drop the ones behind the eye.
    const auto cell = static_cast<double>(config::CHUNK_SIZE);
    std::map<std::pair<int, int>, MeshData> buckets;
    const auto bucket_for = [&](glm::vec2 xz) -> MeshData& {
        return buckets[{static_cast<int>(std::floor(static_cast<double>(xz.x) / cell)),
                        static_cast<int>(std::floor(static_cast<double>(xz.y) / cell))}];
    };
    // Appending shifts the source indices by the vertices already in the bucket.
    const auto append = [](MeshData& dst, const MeshData& src) {
        if (src.vertices.empty() || src.indices.empty()) {
            return;
        }
        const auto base = static_cast<uint32_t>(dst.vertices.size());
        dst.vertices.insert(dst.vertices.end(), src.vertices.begin(), src.vertices.end());
        dst.indices.reserve(dst.indices.size() + src.indices.size());
        for (const uint32_t i : src.indices) {
            dst.indices.push_back(base + i);
        }
    };

    for (const math::LakePlane& lake : lakes) {
        const MeshData mesh = build_lake_mesh(lake, LOOKDEV_WATER_UV_TILE_M,
                                              LOOKDEV_WATER_EDGE_MARGIN_M);
        if (mesh.vertices.empty()) {
            continue;
        }
        append(bucket_for({mesh.vertices.front().position.x,
                           mesh.vertices.front().position.z}),
               mesh);
    }
    // Segment i = stations [offsets[i], offsets[i+1]); a trailing offset equal
    // to the station count is tolerated but not required.
    const size_t station_count = river_stations.size();
    for (size_t i = 0; i < river_segment_offsets.size(); ++i) {
        const size_t begin = river_segment_offsets[i];
        const size_t end = i + 1 < river_segment_offsets.size()
                               ? river_segment_offsets[i + 1]
                               : station_count;
        if (begin >= end || end > station_count) {
            continue;
        }
        const MeshData mesh =
            build_river_mesh(river_stations.subspan(begin, end - begin),
                             LOOKDEV_WATER_UV_TILE_M, LOOKDEV_WATER_EDGE_MARGIN_M);
        if (mesh.vertices.empty()) {
            continue;
        }
        // A river segment spans many cells; it goes in the bucket of its first
        // station and its AABB stretches to cover it. One long ribbon is a
        // handful of segments, not thousands, so this costs nothing.
        append(bucket_for({mesh.vertices.front().position.x,
                           mesh.vertices.front().position.z}),
               mesh);
    }

    size_t merged = 0;
    for (const auto& [key, mesh] : buckets) {
        if (mesh.vertices.empty()) {
            continue;
        }
        const platform::MeshHandle handle =
            renderer.create_mesh(mesh.vertices, mesh.indices);
        if (!handle.valid()) {
            std::fprintf(stderr,
                         "[render] WATER BUCKET (%d,%d) FAILED TO UPLOAD — that "
                         "water is missing from the world.\n", key.first, key.second);
            continue;
        }
        math::Aabb bounds{};
        for (const platform::Vertex& v : mesh.vertices) {
            bounds.expand(v.position);
        }
        water_body_meshes_.push_back({handle.id, bounds});
        ++merged;
    }
    if (std::getenv("DFN_MESH_STATS") != nullptr) {
        std::fprintf(stderr,
                     "[render] water bodies: %zu lakes + %zu river segments -> "
                     "%zu bucket meshes.\n",
                     lakes.size(), river_segment_offsets.size(), merged);
    }
}

void RenderSystem::set_path_surface(platform::IRenderer& renderer,
                                    std::span<const math::PathStation> stations,
                                    std::span<const uint32_t> route_offsets) {
    clear_path_surface(renderer);
    std::vector<PathPiece> pieces = build_path_pieces(stations, route_offsets);
    // THE TREAD IS RAISED OFF core's PROFILE BY ONE VOXEL. See
    // PATH_SURFACE_LIFT_M for why, and for why it is a stopgap rather than a
    // material constant: the ground the player sees is the 1 m voxel surface,
    // not the height field core flattens the tread into, so at the profile
    // itself the road is buried under its own ground for most of its length.
    // DFN_PATH_LIFT overrides it — the measurement hook that produced the
    // number and the one that will retire it.
    float lift = PATH_SURFACE_LIFT_M;
    if (const char* lenv = std::getenv("DFN_PATH_LIFT")) {
        lift = std::strtof(lenv, nullptr);
    }
    if (lift != 0.0f) {
        for (PathPiece& piece : pieces) {
            for (platform::Vertex& v : piece.mesh.vertices) {
                v.position.y += lift;
            }
            piece.bounds.min.y += lift;
            piece.bounds.max.y += lift;
        }
    }
    std::size_t failed = 0;
    for (const PathPiece& piece : pieces) {
        if (piece.mesh.vertices.empty() || piece.mesh.indices.empty()) {
            continue;
        }
        const platform::MeshHandle handle =
            renderer.create_mesh(piece.mesh.vertices, piece.mesh.indices);
        if (!handle.valid()) {
            ++failed;
            continue;
        }
        path_meshes_.push_back({handle.id, piece.bounds});
    }
    if (failed > 0) {
        // The silent half of the water crash, not repeated: a path that failed
        // to upload is a path the player walks down and cannot see.
        std::fprintf(stderr,
                     "[render] %zu OF %zu PATH PIECES FAILED TO UPLOAD — that "
                     "tread is missing from the world.\n",
                     failed, pieces.size());
    }
    if (std::getenv("DFN_MESH_STATS") != nullptr) {
        std::size_t tris = 0;
        for (const PathPiece& piece : pieces) {
            tris += piece.mesh.triangle_count();
        }
        std::fprintf(stderr,
                     "[render] path surface: %zu stations -> %zu pieces, "
                     "%zu triangles.\n",
                     stations.size(), path_meshes_.size(), tris);
    }
}

void RenderSystem::clear_path_surface(platform::IRenderer& renderer) {
    for (const WaterBucket& piece : path_meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{piece.mesh_id});
    }
    path_meshes_.clear();
}

void RenderSystem::clear_water_bodies(platform::IRenderer& renderer) {
    for (const WaterBucket& bucket : water_body_meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{bucket.mesh_id});
    }
    water_body_meshes_.clear();
}

bool RenderSystem::register_mesh(platform::IRenderer& renderer, uint32_t mesh_asset,
                                 std::span<const platform::Vertex> vertices,
                                 std::span<const uint32_t> indices) {
    // Every refusal is LOUD (stderr) — absence presenting as a neutral state
    // is this project's most expensive recurring bug (the invisible castle),
    // and a refused registration is an absence the caller must hear about.
    if (mesh_asset == 0 || vertices.empty() || indices.empty()) {
        std::fprintf(stderr,
                     "[render] register_mesh REFUSED id %u: %s.\n", mesh_asset,
                     mesh_asset == 0 ? "id 0 is not a mesh id"
                                     : "empty geometry");
        return false;
    }
    // Ranges owned by other mechanisms are refused even when unoccupied: a
    // typo must not shadow a blessed site id, the view model, or an item id.
    const bool foreign =
        (mesh_asset >= SITE_MESH_ID_FIRST && mesh_asset <= 31)
        || mesh_asset == VIEWMODEL_MESH_ID_HAND
        || mesh_asset == VIEWMODEL_MESH_ID_TORCH
        || (mesh_asset >= ITEM_MESH_ID_FIRST && mesh_asset <= ITEM_MESH_ID_LAST);
    if (foreign) {
        std::fprintf(stderr,
                     "[render] register_mesh REFUSED id %u: inside a range "
                     "owned by another mechanism (sites 1..31, view model "
                     "32..33, items 64..127). See the id map in ProcMesh.h.\n",
                     mesh_asset);
        return false;
    }
    if (mesh_cache_.contains(mesh_asset)) {
        std::fprintf(stderr,
                     "[render] register_mesh REFUSED id %u: already "
                     "registered. A collision means two zones disagree about "
                     "the RenderMesh id map; nothing was replaced. Live "
                     "replacement is a separate, not-yet-needed API.\n",
                     mesh_asset);
        return false;
    }
    const platform::MeshHandle handle = renderer.create_mesh(vertices, indices);
    if (!handle.valid()) {
        // create_mesh already reported the failure with the pool counters.
        return false;
    }
    mesh_cache_.emplace(mesh_asset, handle.id);
    return true;
}

} // namespace dfn::render
