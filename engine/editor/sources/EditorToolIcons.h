/*
Module: engine/editor
File: engine/editor/sources/EditorToolIcons.h

Responsibility:
- THE PICTURES ON THE TOOL BUTTONS, drawn by us on the CPU. One function, one
  size argument, every icon the same number of bytes — which is the user's
  requirement stated as a property of the baker rather than as a promise about
  six hand-made files.

Key items:
- bake_tool_icon(): RGBA8, size_px * size_px * 4, straight alpha.
- ICON_PX: the size the bar asks for.

WHY DRAWN AND NOT SHIPPED (user, 18.08.2026): «текст кнопок сверху замени на
картинки одинаковых размеров... кнопки сделай "двойные" квадрат с картинкой
описывающий инструмент». Six .png files would be six things that can be the
wrong size, six things a build has to find on disk, and six things no test can
look at. Here "the same size" is `assert(rgba.size() == px*px*4)` for every
icon, and the suite reads the pixels back — the same reason the part
thumbnails are baked on the CPU (EditorPaletteThumb.h).

Dependencies:
- Uses: EditorTool.h (ToolIcon), std. No ImGui, no renderer: it produces bytes,
  and whoever has a texture uploader uploads them (Rule 3).
- Used by: EditorToolbar (uploads through EditorUi::make_texture), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- KEEP THEM DISTINGUISHABLE. The suite asserts that no two icons are the same
  bytes; two tools wearing one picture is exactly the confusion the icons were
  asked for to remove.
*/

#pragma once

#include "engine/editor/sources/EditorTool.h"

#include <cstdint>
#include <vector>

namespace dfn::app {

/// The bar's icon size in interface pixels. One number, so the squares are
/// squares and the row is a row.
inline constexpr int TOOL_ICON_PX = 32;

/// Draws `icon` into `rgba` as size_px * size_px * 4 bytes, straight alpha,
/// transparent where the drawing is not. Always true for a valid icon; false
/// only for ToolIcon::Count, which is not a picture.
[[nodiscard]] bool bake_tool_icon(ToolIcon icon, int size_px,
                                  std::vector<std::uint8_t>& rgba);

} // namespace dfn::app
