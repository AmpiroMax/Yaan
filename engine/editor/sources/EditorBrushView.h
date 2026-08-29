/*
Module: engine/editor
File: engine/editor/sources/EditorBrushView.h

Responsibility:
- THE GROUND SWATCHES: the picture of «трава», «камень», «смесь» и «песок»
  beside their names, baked once and kept.

Key items:
- BrushSwatches: the ground pictures beside the surface names.

ЧЕГО ЗДЕСЬ БОЛЬШЕ НЕТ, И ПОЧЕМУ. Тут жила ПАНЕЛЬ КИСТИ целиком —
make_brush_panel / draw_brush_panel / BrushHooks. После переписи инструментов на
IEditorTool (docs/AUDIT_EDITOR_TOOLS.md) настройки рисует сам инструмент
(HeightBrushTool::draw_settings, SurfacePaintTool::draw_settings, PlantTool::
draw_settings), и эту панель не звал НИКТО: ноль ссылок во всём дереве, считая
проверки и двери сдачи. Мёртвый код, который выглядит живым, — это приглашение
починить не тот файл, поэтому он удалён, а не оставлен «на всякий случай».
УНЕСЁННОЕ ВМЕСТЕ С НИМ ВЕРНУЛОСЬ: счётчик «Последний мазок: узлов N · X м»
теперь стоит в настройках самой кисти высоты и читает ToolWorld::last_dab.

Dependencies:
- Uses: EditorBrush.h, EditorUi.h, EditorPaletteThumb.h (bake_surface_swatch).
- Used by: engine/editor (SurfacePaintTool рисует ими список поверхностей).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- НЕ ВОЗВРАЩАЙ СЮДА ПАНЕЛЬ. Настройки инструмента рисует инструмент — это и
  есть свойство, купленное переписью: второго места, задающего то же
  состояние, быть не должно.
*/

#pragma once

#include "engine/editor/sources/EditorBrush.h"
#include "engine/editor/sources/EditorUi.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace dfn::app {

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

} // namespace dfn::app
