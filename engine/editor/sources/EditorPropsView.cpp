/*
Module: engine/editor
File: engine/editor/sources/EditorPropsView.cpp

Responsibility:
- The properties column's content, declared in EditorPropsView.h.

Dependencies:
- Uses: EditorPropsView.h, EditorUi.h, Dear ImGui.
- Used by: engine/app (App).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO Begin/End HERE. EditorUi owns the window; this draws content.
- EVERY VISIBLE STRING GOES THROUGH EditorUi::tr() (Rule 5).
- THIS PANEL DECIDES NOTHING. It edits numbers and asks; the judge answers.
*/

#include "engine/editor/sources/EditorPropsView.h"

#include <imgui.h>

namespace dfn::app {

void draw_props_panel(PropsModel& model, const PropsHooks& hooks) {
    if (model.object.empty()) {
        // NOT AN EMPTY PANEL. A column of zeroed fields looks like a selected
        // object standing at the origin, and the builder goes looking for it.
        ImGui::TextDisabled("%s", EditorUi::tr("props.none"));
        return;
    }

    ImGui::TextUnformatted(model.object.c_str());
    if (model.width_m > 0.0f || model.height_m > 0.0f) {
        // THE MEASURED extent, the same ruler the judge and the ghost use.
        ImGui::TextDisabled("%.2f x %.2f x %.2f m", static_cast<double>(model.width_m),
                            static_cast<double>(model.depth_m),
                            static_cast<double>(model.height_m));
    }
    ImGui::TextDisabled("%s %s", EditorUi::tr("props.group"),
                        model.group.empty() ? EditorUi::tr("props.group.alone")
                                            : model.group.c_str());
    ImGui::Separator();

    // ONE APPLY FOR THE WHOLE FORM, and it fires on any field's change. Typing
    // a coordinate and watching the world move is the entire point of the
    // column (user: «я должен уметь их менять»); a separate confirm button
    // would make every experiment a two-step transaction.
    bool touched = false;
    touched = ImGui::DragFloat(EditorUi::tr("props.x"), &model.x, 0.05f, 0.0f, 0.0f,
                               "%.3f") || touched;
    touched = ImGui::DragFloat(EditorUi::tr("props.y"), &model.y, 0.05f, 0.0f, 0.0f,
                               "%.3f") || touched;
    touched = ImGui::DragFloat(EditorUi::tr("props.z"), &model.z, 0.05f, 0.0f, 0.0f,
                               "%.3f") || touched;
    // DEGREES, because a builder types 90. The quarter turn is what the kit's
    // square joints hand out, so it also gets a button.
    touched = ImGui::DragFloat(EditorUi::tr("props.yaw"), &model.yaw_deg, 1.0f, -360.0f,
                               360.0f, "%.1f°") || touched;
    ImGui::SameLine();
    if (ImGui::SmallButton("-90")) {
        model.yaw_deg -= 90.0f;
        touched = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+90")) {
        model.yaw_deg += 90.0f;
        touched = true;
    }
    touched = ImGui::DragFloat(EditorUi::tr("props.scale"), &model.scale, 0.01f, 0.05f,
                               10.0f, "%.3f") || touched;

    if (touched && hooks.apply) {
        (void)hooks.apply();
    }

    if (!model.refusal.empty()) {
        // THE JUDGE'S OWN SENTENCE, in the refusal colour. A number that snaps
        // back without a word is indistinguishable from a field that does not
        // work, and a builder retypes it twice before he stops trusting the
        // panel. The wording comes from BuildTool's table — one definition, two
        // hands (Rule 32).
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.88f, 0.42f, 0.42f, 1.0f), "%s",
                           model.refusal.c_str());
    }

    ImGui::Separator();
    if (hooks.remove && ImGui::Button(EditorUi::tr("props.delete"))) {
        hooks.remove();
    }
}

EditorPanel make_props_panel(PropsModel& model, PropsHooks hooks, EditorPanelSide side) {
    EditorPanel panel;
    panel.id = PROPS_PANEL_ID;
    panel.title_key = "editor.panel.props";
    panel.side = side;
    panel.extent_px = 320.0f;
    // CLOSED AT STARTUP, opened by the selection: a column saying "nothing
    // selected" is a column in the way.
    panel.open = false;
    panel.draw = [&model, hooks = std::move(hooks)] { draw_props_panel(model, hooks); };
    return panel;
}

} // namespace dfn::app
