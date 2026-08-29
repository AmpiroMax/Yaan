/*
Module: engine/editor
File: engine/editor/sources/EditorToolbar.h

Responsibility:
- THE BAR, drawn: a row of DOUBLE buttons — a square with the tool's picture
  and, under it, a strip with a black triangle that opens THAT tool's settings
  and nothing else — plus the gear at the end for the parameters that belong to
  no tool.

Key items:
- ToolIconCache: bakes the pictures once and uploads them through EditorUi.
- draw_tool_bar(): the row. Reads and writes nothing but the toolbox.
- draw_tool_settings(): the body of the settings window, which draws exactly
  one tool's draw_settings() — or the common parameters when the gear is on.

WHY IT IS A SEPARATE FILE FROM EditorUi.cpp: Rule 21. EditorUi.cpp was already
over the 800-line limit before this work; the bar is a self-contained drawing
and belongs in its own file rather than making that worse.

THE USER'S OWN RULES, and every one of them is a line of code below:
- «текст кнопок сверху замени на картинки одинаковых размеров»
- «кнопки сделай "двойные" квадрат с картинкой... ниже небольшой прямоугольник
   с чёрным треугольником, при нажатии на него открывается меню настройки этого
   И ТОЛЬКО этого инструмента»
- «если у меня выбран один инструмент, я кликаю на настройки другого,
   инструмент не меняется в руках»
- «если я кликну на иконку выбранного уже инструмента, выбор сбросится»

Dependencies:
- Uses: EditorToolbox.h, EditorToolIcons.h, EditorUi.h, Dear ImGui.
- Used by: EditorUi (one call per frame from draw_toolbar / the settings panel).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The bar DECIDES NOTHING. Every click here calls a toolbox method; if a rule
  about what a click means appears in this file, it is in the wrong file.
*/

#pragma once

#include "engine/editor/sources/EditorTool.h"
#include "engine/editor/sources/EditorToolIcons.h"
#include "engine/editor/sources/EditorUi.h"

#include <map>

namespace dfn::app {

class EditorToolbox;

/// NAME -> PICTURE, baked once. The same shape as the part thumbnails' cache
/// and for the same reason: the bar asks for every icon every frame, and baking
/// six of them per frame would be six drawings a second for pictures that
/// cannot change.
class ToolIconCache {
public:
    explicit ToolIconCache(EditorUi& ui) : ui_(&ui) {}
    ~ToolIconCache();
    ToolIconCache(const ToolIconCache&) = delete;
    ToolIconCache& operator=(const ToolIconCache&) = delete;

    /// 0 when the bake or the upload failed — and 0 draws as nothing, which
    /// leaves a button that still works. A bar that refuses to draw because a
    /// picture is missing is worse than a bar with a blank square.
    [[nodiscard]] EditorTexture get(ToolIcon icon);

private:
    EditorUi* ui_ = nullptr;
    std::map<ToolIcon, EditorTexture> tex_;
    std::map<ToolIcon, int> attempts_;
};

/// The row of double buttons. Called inside the toolbar window EditorUi owns.
void draw_tool_bar(EditorToolbox& box, ToolIconCache& icons, ToolWorld& world);

/// The settings window's body: ONE tool's settings, or the common parameters.
void draw_tool_settings(EditorToolbox& box);

} // namespace dfn::app
