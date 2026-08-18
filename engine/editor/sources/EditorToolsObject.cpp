/*
Created: 18:08:2026 - 12:06:50
Last updated: 19:08:2026 - 02:34:20
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
- 18:08:2026 - 13:17:00: SelectTool сам просит показать свои свойства
  (ToolWorld::open_own_settings) и только при попадании. Раньше это делал App
  условием, которое не спрашивало, чей сейчас ход, поэтому попытка посадить
  дерево распахивала меню посадки — жалоба 18.08 «бредовое поведение», и она
  верна. Пустой щелчок по траве не открывает пустую колонку: это шум, а не
  отклик.
- 18:08:2026 - 19:44:10: Выбор якоря и прямой тем же инструментом, что выбирает объекты; часть постройки имеет приоритет над объектом сцены.
- 18:08:2026 - 20:26:30: Выбор стены по её контуру; за пределом дальности инструменты прячут своё обещание сами.
- 19:08:2026 - 01:20:45: Инструмент выбора рисует проволоку постройки (подсветка выбранного видна и с рамкой в руке), открывает свои настройки и показывает в них СВОЙСТВА ВЫБРАННОГО.
- 19:08:2026 - 02:34:20: Заголовок «Выбрано сейчас» и в панели инструмента выбора.
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

SelectTool::HouseTarget SelectTool::house_target(const ToolAim& aim) const {
    HouseTarget out;
    if (house_ == nullptr) {
        return out;
    }
    // ЯКОРЬ ПЕРЕД ОСЬЮ. Они соседи: якорь сидит на конце оси, и щелчок у конца
    // обязан достаться якорю — иначе выбрать конечную вершину нельзя вовсе.
    out.vertex = house_->pick_vertex_ray(aim.origin, aim.direction(), HOUSE_GRAB_M);
    if (out.vertex != world::NO_VERTEX) {
        return out;
    }
    // ЛЮБОЙ ЭЛЕМЕНТ, А НЕ ТОЛЬКО ПРЯМАЯ: стену человек тоже хочет выбирать и
    // убирать, и целится он в её контур — в то, чем она сейчас нарисована.
    out.element = house_->pick_element_ray(aim.origin, aim.direction(), HOUSE_EDGE_GRAB_M);
    return out;
}

void SelectTool::on_press(const ToolAim& aim, ToolWorld& world) {
    // ЧАСТЬ ПОСТРОЙКИ ИМЕЕТ ПРИОРИТЕТ НАД ОБЪЕКТОМ СЦЕНЫ, и это не вкус: якорь
    // с осью — тонкие цели в полметра, объект под ними — дом целиком. Обратный
    // порядок означал бы, что попасть в якорь на фоне стены нельзя никогда.
    if (const HouseTarget h = house_target(aim); h.any()) {
        if (h.vertex != world::NO_VERTEX) {
            house_->select_vertex(h.vertex);
        } else {
            house_->select_element(h.element);
        }
        // И СВОИ НАСТРОЙКИ ОТКРЫВАЮТСЯ: draw_settings этого инструмента
        // показывает СВОЙСТВА ВЫБРАННОГО (заказ 19.08 — «мне надо видеть
        // свойства объекта, а не меню инструмента»), так что панель — не чужая.
        if (world.open_own_settings) {
            world.open_own_settings();
        }
        return;
    }
    if (!world.select_target) {
        return;
    }
    // ВЫБРАЛ — ПОКАЗЫВАЮ СВОЙСТВА, и просит это ИНСТРУМЕНТ ВЫБОРА, а не App.
    // Раньше открывал App условием, которое не спрашивало, чей сейчас ход, и
    // потому открывало настройки ЛЮБОГО инструмента: попытка посадить дерево
    // распахивала меню посадки (жалоба 18.08 — «бредовое поведение», и она
    // верна). Здесь забыть об этом некому: настройки открывает тот, кому они
    // нужны, из своего же обработчика.
    if (world.select_target() && world.open_own_settings) {
        world.open_own_settings();
    }
}

ToolPreview SelectTool::preview(const ToolAim& aim) const {
    ToolPreview out;
    // ПРОВОЛОКА ПОСТРОЙКИ РИСУЕТСЯ И У ИНСТРУМЕНТА ВЫБОРА — И ЗА ПРЕДЕЛОМ
    // ДАЛЬНОСТИ ТОЖЕ: дом и подсветка выбранного — ФАКТЫ, а не обещания щелчка.
    // Без этого выбранная стена не подсвечивалась ВОВСЕ (заказ 19.08: «не
    // подсвечивает контуры выбранного объекта, сложно понять, выбрал ли я
    // что-то»): подсветка жила в стопке проволоки, а проволоку строили только
    // инструменты постройки — с рамкой в руке её не строил никто.
    if (house_ != nullptr) {
        wire_.clear();
        HouseGroundFn ground;
        if (world_ != nullptr && world_->ground_height) {
            ground = world_->ground_height;
        }
        build_house_wire(*house_, ground, wire_);
        out.handles = wire_.plain.empty() ? nullptr : &wire_.plain;
        out.accent = wire_.accent.empty() ? nullptr : &wire_.accent;
        out.line_color = HOUSE_WIRE_COLOR;
        out.accent_color = HOUSE_ACCENT_COLOR;
    }
    // Проба цели — обещание, и она одна здесь подчиняется дальности.
    out.target_probe = aim.in_reach;
    return out;
}

ToolStatus SelectTool::status(const ToolAim& aim) const {
    if (const HouseTarget h = house_target(aim); h.any()) {
        return ToolStatus{h.vertex != world::NO_VERTEX ? "house.hint.select.vertex"
                                                       : "house.hint.select.element",
                          {}, true};
    }
    if (world_ != nullptr && world_->has_target && !world_->has_target()) {
        return ToolStatus{"tool.hint.nothing", {}, false};
    }
    return ToolStatus{identity().hint_key, {}, true};
}

void SelectTool::draw_settings() {
    // СНАЧАЛА ВЫБРАННОЕ В ПОСТРОЙКЕ: если выбран якорь или элемент, панель
    // показывает ИХ свойства. Иначе — свойства расстановки, как раньше.
    if (house_ != nullptr
        && (house_->selected_vertex() != world::NO_VERTEX
            || house_->selected_element() != world::NO_ELEMENT)) {
        ImGui::SeparatorText(EditorUi::tr("house.head.selected"));
        draw_house_selection_panel(*house_);
        return;
    }
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
    // ЗА ПРЕДЕЛОМ ДАЛЬНОСТИ ЭТОТ ИНСТРУМЕНТ НЕ РИСУЕТ НИЧЕГО: вся его картинка —
    // обещание щелчка, а щелчок туда не достанет.
    if (!aim.in_reach) {
        return ToolPreview{};
    }

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
