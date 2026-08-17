/*
Created: 17:08:2026 - 19:05:00
Last updated: 18:08:2026 - 01:05:34
Module: engine/editor
File: engine/editor/sources/EditorBrush.h

Responsibility:
- THE HAND THAT SHAPES THE GROUND AND PLANTS THINGS ON IT. Holds how wide and
  how hard the brush is, what it does to the land under the crosshair, and —
  for anything planted — whether putting it there is ALLOWED.

Key items:
- TerrainBrush / BrushMode: size, strength, falloff, and the four things a
  brush can do to the land.
- brush_weight(): the falloff, and the only arithmetic in this file worth
  arguing about.
- apply_brush(): one dab into the hand-edit layer, reporting NUMBERS.
- flatten_pad(): the flatten brush's output, which is a [pad] and not samples.
- brush_outline(): the zone the next dab will change, drawn as rings — green up,
  red down. Its radii are BISECTED OUT OF brush_weight() rather than recomputed,
  so the ring cannot promise a boundary the brush does not honour.
- PlantBrush / plant_candidates(): where a dab of vegetation WANTS to go.
  Whether it MAY is EditorPlant's question, one layer up — see below.

WHY THIS EXISTS (user, 17.08.2026): «в этом же инструменте необходима
возможность менять высоту ландшафта кистями разных размеров, выбирать что за
поверхность будет рисоваться, добавлять растительность любую, удалять её,
менять или ставить с заданными параметрами».

THE TWO DESIGN DECISIONS THAT MATTER, and they are the same decision twice:

(1) THE GROUND. A stroke lands in world::ReliefLayer, which enters the world
through compose_passes — the single statement of what the finished ground is.
That is what makes the ground the player walks and the ground check_scene
measures ONE thing rather than two that agree today. Any second application
site would be a second truth about the land.

(2) THE PLANTS. Green and red come from world::check_scene itself, exactly as
they do for the build hand — but that half lives in engine/app/sources/
EditorPlant.h, one layer above this file, and the split is forced rather than
chosen: the verdict's WORDING belongs to BuildTool, BuildTool lives in
engine/app, and engine/editor may not look upward (tools/dag_check.py).
Duplicating the rule -> sentence table down here to avoid the seam would have
been the one thing worth more than the seam.

So this file says WHERE a dab wants to plant, and EditorPlant says whether it
MAY. Nothing planted may stand on a path or inside a house, and that is not
enforced in either file — it is enforced by OffPath and OutsideBuildings, in
the judge, where the tools and the agents see it too.

Dependencies:
- Uses: engine/world (ReliefLayer, Scene), glm.
- Used by: engine/editor (the brush panel), engine/app (EditorPlant, App),
  tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NEVER answer "allowed" from a rule written in this file. A new kind of
  refusal belongs in the judge.
- THE MINIMUM RADIUS IS NOT A TASTE. It is world::RELIEF_STEP_M, because the
  drawn terrain is extracted from the chunk's heightmap and a shape finer than
  that lattice is one the world has no way to hold. Do not "relax" it — the
  brush would paint edits nobody can see and the tool would look broken.
*/
/*
UPD:
- 17:08:2026 - 19:05:00: Создан — кисть рельефа и посадка растительности (заказ 17.08).
- 17:08:2026 - 20:06:53: ПЕРЕЕЗД В engine/editor и раскол надвое. Панель кисти обязана жить
  в engine/editor — ARCHITECTURE.md пускает Dear ImGui только туда, — а слой
  editor не имеет права включать engine/app (LAYERS в tools/dag_check.py).
  Значит настройки кисти и её механика обязаны быть по ту же сторону, что и
  панель, которая их правит. Уехало всё, кроме трёх функций, которым нужен
  ПЕРЕВОД ВЕРДИКТА из BuildTool: они остались в engine/app/sources/EditorPlant.h.
  Раскол вынужденный, а не задуманный, и проведён по единственному честному шву:
  «куда посадка ХОЧЕТ встать» здесь, «МОЖНО ли» — уровнем выше. Ни строки логики
  не изменено; альтернатива — копия таблицы правило→фраза внизу — была бы ровно
  тем, ради чего шов и терпится.
- 17:08:2026 - 20:09:15: BrushStroke / stroke_step — защита от «настроил кисть и случайно
  выкопал яму». Флаг wants_mouse сделан ОБЯЗАТЕЛЬНЫМ аргументом, а не
  подразумеваемой проверкой: подключить кисть, не решив про указатель, теперь
  невыразимо.
- 18:08:2026 - 01:05:34: brush_outline — зона, которую кисть ИЗМЕНИТ, кольцами на земле:
  зелёное вверх, красное вниз (слова пользователя 18.08). Радиусы НЕ считаются
  второй формулой, а бисекцией достаются из brush_weight — того же спада, по
  которому бьёт apply_brush. Кольцо, обещающее границу, которой кисть не
  держится, хуже отсутствия кольца: человек перестаёт смотреть на подсказку.
*/

#pragma once

#include "engine/world/sources/ReliefLayer.h"
#include "engine/world/sources/Scene.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <string>
#include <vector>

namespace dfn::app {

// =============================== THE GROUND =================================

/// THE SMALLEST BRUSH THE WORLD CAN SHOW, and it is a fact about the terrain
/// rather than a preference. The drawn surface is extracted from the chunk's
/// decoded heightmap, whose samples are RELIEF_STEP_M apart; a stroke narrower
/// than that moves one lattice sample or none, so the ground either jumps by a
/// whole cell or does not move at all.
///
/// The tool says this out loud instead of quietly clamping a slider. A builder
/// who is told WHY the number stops does not conclude the tool is broken.
inline constexpr float BRUSH_MIN_RADIUS_M = world::RELIEF_STEP_M;

/// What a brush does to the land under the crosshair.
enum class BrushMode : uint8_t {
    Raise,   ///< pull the ground up, hardest at the centre
    Lower,   ///< push it down
    Smooth,  ///< pull each sample towards the average of its neighbours
    /// LEVEL IT TO ONE HEIGHT — and this mode does not write samples at all.
    /// It authors a ScenePad, because "here the ground is this high, blending
    /// back over N metres" is precisely what a pad already says. A second way
    /// of saying it would be the drift Rule 32 exists to forbid, and it would
    /// cost the composer the thing a pad gives him: a statement he can move,
    /// re-read and judge afterwards.
    Flatten,
    /// PAINT WHAT THE GROUND IS MADE OF. It does not touch heights.
    Paint,
};

/// The brush, as the builder set it.
struct TerrainBrush {
    BrushMode mode = BrushMode::Raise;
    /// Metres. Clamped to BRUSH_MIN_RADIUS_M on use — see that constant.
    float radius_m = 8.0f;
    /// Metres per second at the very centre. PER SECOND and not per dab: a
    /// brush measured per dab digs twice as fast on a machine running twice the
    /// frame rate, and the builder would blame his own hand.
    float strength_m_s = 2.0f;
    /// 0 = soft, the falloff starts at the centre; 1 = flat-topped, full
    /// strength out to the rim. Clamped to [0, HARDNESS_MAX].
    float hardness = 0.5f;
    /// Where Flatten levels to, absolute metres.
    float flatten_height_m = 0.0f;
    /// What Paint paints.
    math::SurfaceClass paint = math::SurfaceClass::Grass;
};

/// The hardest a brush may be. Not 1: at exactly 1 the falloff has no width at
/// all, and every stroke would leave a cylinder with a vertical wall — which
/// the lattice then renders as a single-sample cliff.
inline constexpr float BRUSH_HARDNESS_MAX = 0.95f;

/// The falloff: 1 at the centre, 0 at the rim and beyond, monotone between.
///
/// `hardness` moves where the fade STARTS, not how steep it is: the fade
/// itself is a smoothstep, so the stroke meets untouched ground with a matching
/// slope. A linear fade leaves a visible crease around every dab — the ground
/// is C1-discontinuous at the rim and lit terrain shows exactly that as a ring.
[[nodiscard]] float brush_weight(float dist_m, float radius_m, float hardness);

/// The ground WITHOUT the hand-edit layer — what the generator says. Supplied
/// by the caller so this header depends on no generator: the app passes the
/// worldgen sampler, a test passes a plane, and both exercise the same brush.
struct BrushGround {
    float (*base_at)(void* ctx, glm::vec2 world_xz) = nullptr;
    void* ctx = nullptr;
};

/// WHAT ONE DAB ACTUALLY DID, in numbers. "The ground moved" is an opinion; "31
/// samples moved, the furthest by 0.42 m" is a measurement, and it is the only
/// way to prove a brush works without trusting a screenshot of a bump.
struct BrushDabReport {
    int samples_touched = 0;
    float max_abs_delta_m = 0.0f; ///< largest single-sample movement this dab
    float sum_abs_delta_m = 0.0f; ///< total movement, for a stroke's budget
    bool any = false;             ///< false leaves the bounds untouched
    glm::vec2 min_xz{0.0f};       ///< world box the dab changed...
    glm::vec2 max_xz{0.0f};       ///< ...padded, ready for invalidate_area
};

/// Applies ONE DAB at `center` into `layer` and reports what moved.
///
/// `dt_s` is the frame's time step for Raise/Lower/Smooth — see
/// TerrainBrush::strength_m_s for why it is time and not a count. Flatten
/// touches nothing here and reports zero: its output is flatten_pad().
[[nodiscard]] BrushDabReport apply_brush(world::ReliefLayer& layer,
                                         const TerrainBrush& brush, glm::vec2 center,
                                         float dt_s, const BrushGround& ground);

/// The flatten brush's real output: a pad the composition can carry, move and
/// re-read. Square-cornered pads exist (ScenePad::half_extents) but a BRUSH is
/// round, so this authors the circular form.
[[nodiscard]] world::ScenePad flatten_pad(const TerrainBrush& brush, glm::vec2 center);

/// THE STROKE, as a state machine, and it exists for ONE reason: the interface
/// guard must not be something a caller can forget.
///
/// «Настроил кисть и случайно выкопал яму» happens on the first afternoon and
/// it happens once per builder — he drags the SIZE slider, the pointer leaves
/// the panel still holding the button, and the brush bites the ground he was
/// only looking at. The guard against it is EditorUi::wants_mouse(), and a
/// comment saying "remember to check it" is worth nothing, so it is a REQUIRED
/// ARGUMENT of the only function that can start a stroke. Wiring the brush
/// without deciding about the pointer is not expressible.
struct BrushStroke {
    bool active = false;   ///< a stroke is in progress
    /// True for the ONE step on which the stroke ended. The caller spends its
    /// chunk-rebuild budget here: a stroke covers the same chunk a hundred
    /// times as the mouse travels, and rebuilding per dab would pay the cost of
    /// the whole stroke a hundred times over.
    bool just_ended = false;
    /// True while the stroke was BLOCKED at its own start. A stroke that began
    /// on a panel stays blocked until the button is let go — otherwise dragging
    /// a slider off the panel edge starts digging halfway through the drag,
    /// which is the accident above wearing a different hat.
    bool blocked = false;
};

/// Advances the stroke by one frame and answers: apply a dab now?
///
/// `ui_wants_mouse` is EditorUi::wants_mouse() — the pointer is over a panel or
/// a widget is being dragged. It is a parameter and not a member so it cannot
/// go stale, and it has no default so it cannot go unconsidered.
[[nodiscard]] bool stroke_step(BrushStroke& stroke, bool pointer_down,
                               bool ui_wants_mouse);

// ========================= THE ZONE, MADE VISIBLE ===========================
//
// «для кисти объектов нужно добавить отрисовку той зоны, которую я буду
// изменять, зелёным вверх строю, красным вниз» — user, 18.08.2026.
//
// THE ONE THING THAT MAKES THIS HONEST: every number below is ASKED OF
// brush_weight() — the same function apply_brush() calls on every sample — and
// none of it is re-derived from radius_m and hardness. A second formula would
// agree today and drift the first time the falloff was tuned, and the symptom
// would be the worst kind there is: a ring on the screen that says the ground
// under it is safe while the brush is quietly moving it. An outline that lies
// is worse than no outline, because the builder stops looking.
//
// So the rim and the core are found by BISECTING brush_weight, which means the
// clamp (a radius under BRUSH_MIN_RADIUS_M is widened, not obeyed) and the
// shape of the fade are inherited rather than copied.
//
// WHAT THE RING STILL DOES NOT SHOW, said out loud: the brush moves LATTICE
// SAMPLES, RELIEF_STEP_M apart, so the ground that actually changes is a
// staircase inscribed in this circle rather than the circle. The rim is the
// exact boundary of the weight field; no sample outside it ever moves, and the
// outermost sample that does move is within one lattice step of it.

/// WHICH WAY THE GROUND IS ABOUT TO GO — the only thing the colour says.
enum class BrushDirection : uint8_t {
    Up,   ///< green: the ground rises
    Down, ///< red: the ground sinks
    /// NO SINGLE DIRECTION, and this is a real answer rather than a fallback:
    /// Smooth pulls some samples up and others down in one dab, and a level set
    /// exactly at the ground moves nothing at all. Painting either of those
    /// green or red would be the lie this whole section exists to avoid.
    Mixed,
    None, ///< Paint: it moves no ground whatsoever
};

/// 0xAABBGGRR, the byte order IRenderer::debug_line takes.
inline constexpr uint32_t BRUSH_UP_GREEN = 0xFF44FF44u;
inline constexpr uint32_t BRUSH_DOWN_RED = 0xFF4444FFu;
/// Neither up nor down. NOT a shade of the other two: a dim green would read as
/// "raising, weakly" at a glance, which is the reading that must not happen.
inline constexpr uint32_t BRUSH_MIXED_BLUE = 0xFFFFCC44u;

/// HOW FAR THE RING FLOATS OVER THE GROUND IT TRACES. Zero would put the line
/// exactly on the surface it belongs to, where it loses the depth test to its
/// own terrain and shows nothing — which is indistinguishable from a feature
/// that was never written (the collider view learned this twice).
inline constexpr float BRUSH_OUTLINE_LIFT_M = 0.15f;

/// The outline of the zone one dab would change, ready to be drawn as lines.
struct BrushOutline {
    /// Where the brush stops biting: brush_weight() is exactly 0 here and past
    /// it. This is the clamped radius, not the slider's.
    float rim_m = 0.0f;
    /// Where the fade STARTS: brush_weight() is still exactly 1 here. Zero on a
    /// brush soft to its own centre, and then there is no inner ring to draw.
    float core_m = 0.0f;
    BrushDirection direction = BrushDirection::None;
    uint32_t color_rgba = 0;
    /// Rings on the ground, lifted by BRUSH_OUTLINE_LIFT_M. THE FIRST POINT IS
    /// REPEATED AS THE LAST, so a caller draws points.size() - 1 segments and
    /// cannot leave the ring open by an off-by-one.
    std::vector<glm::vec3> rim;
    std::vector<glm::vec3> core; ///< empty when core_m is 0
};

/// The outer edge of the zone this brush changes, from the falloff itself.
///
/// EXPECT A FRACTION OF A MILLIMETRE OFF THE ALGEBRA, and that is the feature
/// rather than a tolerance: near the rim the smoothstep's `1 - s` rounds to
/// exactly 0 while the distance is still a third of a millimetre short of the
/// radius, so the brush genuinely stops biting there. These functions measure
/// the FUNCTION, not the formula, which is the only way the ring can promise
/// what the samples actually do.
[[nodiscard]] float brush_rim_m(const TerrainBrush& brush);

/// The edge of the flat top — where full strength ends and the fade begins.
/// Zero-ish rather than zero on a brush with no flat top at all, for the same
/// rounding reason; brush_outline() drops an inner ring narrower than the
/// ground's own lattice, since a ring around one sample separates nothing.
[[nodiscard]] float brush_core_m(const TerrainBrush& brush);

/// `ground_m` is the FINISHED ground under the crosshair, and only Flatten
/// reads it: its direction is the sign of (target height - the ground there),
/// which is the pad's own claim rather than a second copy of anything.
[[nodiscard]] BrushDirection brush_direction(const TerrainBrush& brush, float ground_m);

/// Green up, red down, blue for neither — and for Paint the surface's own
/// swatch, because there is no up or down in painting what the ground is made
/// of, and green there would answer a question nobody asked.
[[nodiscard]] uint32_t brush_outline_color(const TerrainBrush& brush,
                                           BrushDirection direction);

/// Segments in a ring of this radius: about a metre of arc each, bounded both
/// ways. A fixed count makes a 64 m brush a visible polygon and a 2 m one a
/// waste of lines.
[[nodiscard]] int brush_outline_segments(float rim_m);

/// The whole outline, computed WITHOUT A RENDERER (Rule 3) so the geometry can
/// be measured by a test rather than judged from a screenshot. `ground` is the
/// finished ground — hand edits included — since that is the surface the ring
/// has to hug and the surface the builder is looking at.
[[nodiscard]] BrushOutline brush_outline(const TerrainBrush& brush, glm::vec2 centre,
                                         const BrushGround& ground);

// ============================ THE VEGETATION ================================

/// One thing about to be planted. It is a world::Placement in all but name and
/// converts to one — kept separate only so the caller can hold a dab's worth of
/// candidates before any of them is committed to the composition.
struct PlantCandidate {
    std::string object;       ///< registry name, from the trees/glade shelves
    glm::vec3 position{0.0f};
    float yaw = 0.0f;         ///< radians
    float scale = 1.0f;
};

/// The planting brush: what to plant, how many, how varied.
struct PlantBrush {
    /// The species this dab may draw from. More than one is the point — a
    /// clump of one object at one size reads as a texture, not as vegetation.
    std::vector<std::string> species;
    float radius_m = 6.0f;
    int count = 5;
    float scale_min = 0.9f;
    float scale_max = 1.1f;
    bool random_yaw = true;
    /// How far apart two things from THIS dab must stand. It is not the judge's
    /// job: the judge measures trunks and calls two birches two metres apart a
    /// wood, correctly. This is about the dab looking scattered rather than
    /// heaped, and it is the composer's taste, so it lives here.
    float min_spacing_m = 1.5f;
};

/// Generates a dab's candidates, DETERMINISTICALLY from `seed`. The same seed
/// and the same brush give the same dab, which is what lets a test say "these
/// eleven candidates" instead of "about ten, somewhere".
///
/// Positions are sat on the ground by `ground` — the FINISHED ground including
/// any hand edits, so a tree planted on a hill the composer just raised stands
/// on the hill and not inside it.
[[nodiscard]] std::vector<PlantCandidate> plant_candidates(const PlantBrush& brush,
                                                           glm::vec2 center,
                                                           uint64_t seed,
                                                           const BrushGround& ground);

/// The placement under the crosshair: the NEAREST one whose own measured reach
/// contains `aim_xz`, so a small fern wins over the oak it stands in front of
/// rather than the other way round. Returns doc.placements.size() for "nothing".
///
/// `radius_of` reports an object's footprint radius; false for one the registry
/// does not carry, and such a placement is skipped rather than guessed at.
[[nodiscard]] std::size_t pick_placement(
    const world::SceneDoc& doc, glm::vec2 aim_xz,
    bool (*radius_of)(void* ctx, const std::string& name, float& radius_m), void* ctx);

} // namespace dfn::app
