/*
Created: 18:08:2026 - 12:06:50
Last updated: 18:08:2026 - 12:06:50
Module: engine/editor
File: engine/editor/sources/EditorToolsObject.cpp

Responsibility:
- The two tools that work on things that stand in the world: SELECT and PLACE.
  Their click, their preview, and their settings — which are, respectively, the
  properties column and the object list. Those two used to be PANELS with their
  own chips on the bar; they are the tools' own settings now, which is what let
  buttons 7 and 9 go without anything becoming unreachable.

Dependencies:
- Uses: EditorToolsBuiltin.h, EditorPaletteView.h (draw_parts_panel),
  EditorPropsView.h (draw_props_panel), Dear ImGui.
- Used by: engine/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The ghost's lifetime is PlaceTool's. If a part can survive on_deselected, the
  defect the user reported («деталь остаётся в руках») is back.
*/
/*
UPD:
- 18:08:2026 - 12:06:50: Создан — выбор и постройка как инструменты; их панели стали их
  СОБСТВЕННЫМИ настройками (заказ 18.08: «открывается меню настройки этого И
  ТОЛЬКО этого инструмента»).
*/

#include "engine/editor/sources/EditorToolsBuiltin.h"

#include <imgui.h>

namespace dfn::app {

// ================================= SELECT ===================================

SelectTool::SelectTool(PropsModel& model, PropsHooks hooks)
    : model_(&model), hooks_(std::move(hooks)) {}

ToolIdentity SelectTool::identity() const {
    return ToolIdentity{"select", "editor.tool.select", "tool.hint.select",
                        ToolIcon::Select};
}

void SelectTool::on_press(const ToolAim& aim, ToolWorld& world) {
    (void)aim;
    if (world.select_target) {
        world.select_target();
    }
}

ToolPreview SelectTool::preview(const ToolAim& aim) const {
    (void)aim;
    ToolPreview out;
    // NO GHOST WHILE SELECTING: a part standing in front of the thing being
    // picked is a second answer to «что я сейчас трогаю».
    out.target_probe = true;
    return out;
}

ToolStatus SelectTool::status(const ToolAim& aim) const {
    (void)aim;
    if (world_ != nullptr && world_->has_target && !world_->has_target()) {
        return ToolStatus{"tool.hint.nothing", {}, false};
    }
    return ToolStatus{identity().hint_key, {}, true};
}

void SelectTool::draw_settings() {
    if (model_ != nullptr) {
        draw_props_panel(*model_, hooks_);
    }
}

// ================================= PLACE ====================================

PlaceTool::PlaceTool(PaletteModel& palette, PaletteHooks hooks)
    : palette_(&palette), hooks_(std::move(hooks)) {}

ToolIdentity PlaceTool::identity() const {
    return ToolIdentity{"place", "editor.tool.place", "tool.hint.place", ToolIcon::Place};
}

void PlaceTool::on_press(const ToolAim& aim, ToolWorld& world) {
    (void)aim;
    if (world.place_part) {
        (void)world.place_part();
    }
}

ToolPreview PlaceTool::preview(const ToolAim& aim) const {
    (void)aim;
    ToolPreview out;
    out.ghost = true;
    // The delete target is wanted too: Delete removes what the crosshair is on,
    // and the probe is what finds it.
    out.target_probe = true;
    return out;
}

ToolStatus PlaceTool::status(const ToolAim& aim) const {
    (void)aim;
    if (world_ == nullptr || !world_->ghost_ready) {
        return ToolStatus{identity().hint_key, {}, true};
    }
    std::string reason;
    if (world_->ghost_ready(reason)) {
        return ToolStatus{identity().hint_key, {}, true};
    }
    // THE JUDGE'S OWN SENTENCE, carried rather than re-worded: one verdict
    // rendered twice, never two verdicts (BuildTool.h).
    if (reason.empty()) {
        return ToolStatus{"tool.hint.nopart", {}, false};
    }
    return ToolStatus{"", reason, false};
}

void PlaceTool::on_deselected(ToolWorld& world) {
    // THE PART LEAVES THE HAND WITH THE TOOL. This line is the whole of the
    // user's complaint «после выключения инструмента редактуры последний
    // выбранный объект остаётся в руках и рисуется», and it is now in the one
    // place that cannot be skipped: EditorToolbox::select() calls it always.
    if (world.clear_ghost) {
        world.clear_ghost();
    }
}

void PlaceTool::draw_settings() {
    if (palette_ != nullptr) {
        draw_parts_panel(*palette_, hooks_);
    }
}

} // namespace dfn::app
