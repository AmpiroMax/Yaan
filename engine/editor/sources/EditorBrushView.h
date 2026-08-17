/*
Created: 17:08:2026 - 20:06:53
Last updated: 18:08:2026 - 01:29:51
Module: engine/editor
File: engine/editor/sources/EditorBrushView.h

Responsibility:
- THE BRUSH PANEL: what the sculptor sets before he touches the ground — the
  mode, the size, the strength, the surface he is painting — and what he sets
  before he plants — the species, how many, how varied.

Key items:
- BrushHooks: the two things the panel cannot answer for itself.
- BrushSwatches: the ground pictures beside the surface names.
- make_brush_panel(): the EditorPanel declaration EditorUi is handed.
- draw_brush_panel(): the content alone, for a caller with its own window.

WHY THE PANEL OWNS NO STATE. It edits a TerrainBrush and a PlantBrush the app
owns, in place. A panel holding its own copy would be a second set of settings,
and the stroke would come out at the size of whichever copy the code reached
first — a defect that looks like the slider not working.

THE ONE RULE THIS PANEL EXISTS TO RESPECT: a dab must never fire while the
pointer is over these widgets. That is not enforced here — EditorUi::wants_mouse
answers it and the caller obeys it — but it is why the panel is declared through
EditorUi rather than drawn wherever it likes. «Настроил кисть и случайно выкопал
яму» happens on the first afternoon, and it happens once per builder.

Dependencies:
- Uses: EditorBrush.h (the settings it edits), EditorUi.h, EditorPaletteThumb.h
  (bake_surface_swatch), Dear ImGui (allowed in engine/editor and nowhere else).
- Used by: engine/app (App wires it into EditorUi).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE PANEL NEVER OPENS ITS OWN WINDOW. EditorUi owns Begin/End, the position
  and the size; this draws CONTENT. Three agents write panels and the editor has
  to look like one tool.
- Every string goes through EditorUi::tr() (Rule 5). A Russian literal here is
  both a rule violation and the first line that renders without glyphs.
*/
/*
UPD:
- 17:08:2026 - 20:06:53: Создан — панель кисти на контракте каркаса.
- 18:08:2026 - 01:24:18: КАРТИНКА ПЕРЕД НАЗВАНИЕМ у выбора поверхности (заказ 18.08 про
  предпросмотр). BrushSwatches печёт свотч ОДИН раз на класс и роняет текстуры
  вместе с собой — панель, перерегистрированная на смене карты, иначе течёт по
  четыре текстуры за карту. Свотч НЕ второй набор цветов земли: те же ячейки
  proc-текстур, те же веса splat_weights_of, тот же дизер 4x4, что и во
  фрагменте, — поэтому разошедшийся свотч был бы дефектом одного из них, а не
  несовпавшей картинкой. Механизм — bake_surface_swatch зоны меню объектов.
- 18:08:2026 - 01:29:51: у свотча три попытки вместо одной. Неудачная выпечка кэшировалась как
  готовый ноль и держалась ВЕЧНО — один ранний кадр, в котором текстуру создать
  нельзя, убивал картинки до конца сессии, и панель при этом выглядела рабочей.
  У миниатюр деталей такая защита есть; здесь её не было. ОГОВОРКА ЧЕСТНОСТИ:
  правка сделана по подозрению, а не по воспроизведённому отказу — я решил, что
  свотчей нет, разглядывая обрезанный кадр, и обрезал ровно ту колонку, где они
  стоят. Свотчи работали и до неё. Оставлено как защита от хрупкости, а не как
  починка: вечно закэшированная неудача плоха и без отказа на экране.
*/

#pragma once

#include "engine/editor/sources/EditorBrush.h"
#include "engine/editor/sources/EditorUi.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace dfn::app {

/// WHAT THE PANEL CANNOT ANSWER ITSELF.
struct BrushHooks {
    /// The species the shelves actually carry, for the planting list. Called
    /// once per frame while the planting section is open; the app is expected
    /// to keep the vector, since reading a directory per frame is a directory
    /// read per frame.
    std::function<const std::vector<std::string>&()> species;

    /// THE STROKE'S OWN NUMBERS, for the readout: how many lattice samples the
    /// last dab moved and by how much. It is shown because "the ground looks
    /// higher" is what a builder can see for himself, and "31 samples, worst
    /// 0.42 m" is what he can act on — and because a brush that has silently
    /// stopped biting looks exactly like a brush aimed at nothing.
    std::function<void(int& samples, float& worst_m)> last_dab;
};

/// THE GROUND SWATCHES, baked once and kept.
///
/// A surface picker that names its classes and does not show them asks the
/// builder to remember what «смесь» looks like. The swatch is not a second set
/// of ground colours: bake_surface_swatch composes the SAME proc-texture cells
/// the terrain atlas is baked from, weighted by the SAME splat table the
/// fragment shader reads, dithered by the same 4x4 threshold — so a swatch that
/// disagreed with the ground would be a defect in one of those, not a mismatched
/// picture (Rule 32).
///
/// It owns its textures and drops them with itself, which is why it holds the
/// EditorUi it uploaded through: a panel re-registered on map change destroys
/// the old callback, and textures that outlived it would leak once per map.
class BrushSwatches {
public:
    explicit BrushSwatches(EditorUi& ui) : ui_(&ui) {}
    ~BrushSwatches();
    BrushSwatches(const BrushSwatches&) = delete;
    BrushSwatches& operator=(const BrushSwatches&) = delete;

    /// The picture for this class, baked and uploaded on first ask. 0 when the
    /// bake failed, and 0 is drawable-as-nothing on purpose: a picker with no
    /// pictures is poorer, a picker that refuses to draw is broken.
    [[nodiscard]] EditorTexture surface(math::SurfaceClass surface);

private:
    /// Готовая картинка ИЛИ счётчик неудачных попыток. Разница существенная:
    /// раньше неудача запоминалась как готовый ноль и держалась ВЕЧНО, поэтому
    /// один ранний кадр, в котором текстуру создать нельзя, убивал свотчи до
    /// конца сессии — на экране оставались одни названия, а код при этом
    /// выглядел рабочим. У миниатюр деталей на этот случай есть три попытки; их
    /// не было здесь, и это ровно тот же отказ этажом ниже.
    struct Slot {
        EditorTexture texture = 0;
        int attempts = 0;
        bool given_up = false;
    };

    EditorUi* ui_ = nullptr;
    std::map<math::SurfaceClass, Slot> tex_;
};

/// Declares the brush panel for EditorUi. `terrain`, `plant` and `hooks` must
/// outlive the panel (App owns them). It starts CLOSED: a panel the builder has
/// to ask for is a panel that is not in his way while he is doing something
/// else.
///
/// `ui` is taken because the swatches have to be UPLOADED, and make_texture is
/// an instance call — the panel cannot bake its own pictures without it.
[[nodiscard]] EditorPanel make_brush_panel(EditorUi& ui, TerrainBrush& terrain,
                                           PlantBrush& plant, BrushHooks hooks,
                                           EditorPanelSide side = EditorPanelSide::Right);

/// The content alone, for a caller that owns its window (the capture door).
/// Normal callers use make_brush_panel(). `swatches` may be null, and then the
/// picker draws its rows without pictures.
void draw_brush_panel(TerrainBrush& terrain, PlantBrush& plant, const BrushHooks& hooks,
                      BrushSwatches* swatches = nullptr);

} // namespace dfn::app
