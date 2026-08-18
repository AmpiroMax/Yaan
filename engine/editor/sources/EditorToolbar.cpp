/*
Created: 18:08:2026 - 12:07:20
Last updated: 18:08:2026 - 12:07:20
Module: engine/editor
File: engine/editor/sources/EditorToolbar.cpp

Responsibility:
- draw_tool_bar() / draw_tool_settings() / ToolIconCache, declared in
  EditorToolbar.h.

Dependencies:
- Uses: EditorToolbar.h, EditorToolbox.h, EditorToolIcons.h, EditorUi.h, ImGui.
- Used by: EditorUi.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here may write the active tool directly. click_icon()/click_settings()
  are the only two verbs, and they are two DIFFERENT verbs on purpose.
*/
/*
UPD:
- 18:08:2026 - 12:07:20: Создан вместе с заголовком.
*/

#include "engine/editor/sources/EditorToolbar.h"

#include "engine/editor/sources/EditorToolbox.h"

#include <imgui.h>

#include <cstdio>
#include <vector>

namespace dfn::app {
namespace {

/// The square's side in interface pixels, and the strip's height under it. The
/// squares are EQUAL by construction: one number, used for every button.
constexpr float ICON_SIDE = 34.0f;
constexpr float ARROW_H = 12.0f;

/// The verdict's green — one colour, one meaning across the whole tool.
constexpr ImVec4 ACTIVE_BG{0.27f, 0.44f, 0.32f, 1.0f};
constexpr ImVec4 ACTIVE_HOVER{0.33f, 0.52f, 0.38f, 1.0f};
constexpr ImVec4 OPEN_BG{0.30f, 0.34f, 0.44f, 1.0f};

/// The triangle is DRAWN, not typed: a glyph would depend on the font carrying
/// it (this zone has already paid for that once — ★ came out as a blank square),
/// and the strip is 12 px tall, where a font's ▼ is a different size on every
/// display scale.
void draw_black_triangle(ImVec2 min, ImVec2 max) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cx = 0.5f * (min.x + max.x);
    const float cy = 0.5f * (min.y + max.y);
    const float w = 0.30f * (max.x - min.x);
    const float h = 0.30f * (max.y - min.y);
    dl->AddTriangleFilled(ImVec2(cx - w, cy - h), ImVec2(cx + w, cy - h),
                          ImVec2(cx, cy + h), IM_COL32(0, 0, 0, 235));
}

} // namespace

ToolIconCache::~ToolIconCache() {
    if (ui_ == nullptr) {
        return;
    }
    for (const auto& [icon, tex] : tex_) {
        (void)icon;
        if (tex != 0) {
            ui_->drop_texture(tex);
        }
    }
}

EditorTexture ToolIconCache::get(ToolIcon icon) {
    if (const auto it = tex_.find(icon); it != tex_.end() && it->second != 0) {
        return it->second;
    }
    // THREE ATTEMPTS, NOT ONE AND NOT FOREVER — the swatches' lesson, copied
    // deliberately: one attempt turns a single early frame with no texture into
    // a bar with no pictures for the rest of the session, and unlimited retries
    // turn one missing picture into a bake every frame.
    int& tries = attempts_[icon];
    if (tries >= 3) {
        return 0;
    }
    ++tries;
    std::vector<std::uint8_t> rgba;
    if (!bake_tool_icon(icon, TOOL_ICON_PX, rgba) || ui_ == nullptr) {
        return 0;
    }
    const EditorTexture tex = ui_->make_texture(static_cast<std::uint32_t>(TOOL_ICON_PX),
                                               static_cast<std::uint32_t>(TOOL_ICON_PX),
                                               rgba.data());
    tex_[icon] = tex;
    if (tex == 0 && tries == 3) {
        std::fprintf(stderr, "[инструменты] значок %d не загрузился за 3 попытки — "
                             "кнопка останется без картинки\n",
                     static_cast<int>(icon));
    }
    return tex;
}

void draw_tool_bar(EditorToolbox& box, ToolIconCache& icons, ToolWorld& world) {
    for (std::size_t i = 0; i < box.count(); ++i) {
        IEditorTool* tool = box.at(i);
        if (tool == nullptr) {
            continue;
        }
        if (i > 0) {
            ImGui::SameLine();
        }
        const ToolIdentity id = tool->identity();
        const bool active = box.active_index() == i;
        const bool settings_open = box.settings_index() == i;

        ImGui::BeginGroup();
        ImGui::PushID(static_cast<int>(i));

        // ---- the square: WHAT IS IN MY HAND ---------------------------------
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_BG);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACTIVE_HOVER);
        }
        const EditorTexture tex = icons.get(id.icon);
        bool hit = false;
        if (tex != 0) {
            hit = EditorUi::image_button("##icon", tex, ICON_SIDE, ICON_SIDE);
        } else {
            // NO PICTURE IS NOT NO BUTTON. The caption is the fallback, and the
            // button keeps its size so the row does not jump.
            hit = ImGui::Button(EditorUi::tr(id.title_key),
                                ImVec2(ICON_SIDE + 8.0f, ICON_SIDE + 8.0f));
        }
        if (active) {
            ImGui::PopStyleColor(2);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", EditorUi::tr(id.title_key));
        }
        if (hit) {
            // ONE VERB: pick it up, or — if it is already in hand — put it down.
            box.click_icon(i, world);
        }

        // ---- the strip: SETTINGS OF THIS TOOL ONLY ---------------------------
        if (settings_open) {
            ImGui::PushStyleColor(ImGuiCol_Button, OPEN_BG);
        }
        const float width = ImGui::GetItemRectSize().x;
        const bool arrow = ImGui::Button("##settings", ImVec2(width, ARROW_H));
        draw_black_triangle(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        if (settings_open) {
            ImGui::PopStyleColor();
        }
        if (arrow) {
            // AND IT DOES NOT TOUCH THE HAND. That is the user's rule, and it
            // is enforced by the method, not by this call site.
            box.click_settings(i);
        }
        ImGui::PopID();
        ImGui::EndGroup();
    }

    // ---- the gear: WHAT BELONGS TO NO TOOL ----------------------------------
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushID("gear");
    if (box.common_settings_open()) {
        ImGui::PushStyleColor(ImGuiCol_Button, OPEN_BG);
    }
    const EditorTexture gear = icons.get(ToolIcon::Settings);
    const bool gear_hit = gear != 0
                              ? EditorUi::image_button("##gear", gear, ICON_SIDE, ICON_SIDE)
                              : ImGui::Button(EditorUi::tr("editor.tool.common"),
                                              ImVec2(ICON_SIDE + 8.0f, ICON_SIDE + 8.0f));
    if (box.common_settings_open()) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", EditorUi::tr("editor.tool.common"));
    }
    if (gear_hit) {
        box.click_gear();
    }
    ImGui::Dummy(ImVec2(ICON_SIDE, ARROW_H));
    ImGui::PopID();
    ImGui::EndGroup();

    // ---- what state am I in, in words ---------------------------------------
    ImGui::SameLine();
    ImGui::BeginGroup();
    const IEditorTool* held = box.active();
    ImGui::TextUnformatted(held != nullptr ? EditorUi::tr(held->identity().title_key)
                                           : EditorUi::tr("editor.tool.none"));
    // R, AND THE BAR SAYS WHICH SIDE OF IT WE ARE ON. The user asked for a
    // vim-like mode; a mode you cannot see is a mode you have to guess.
    ImGui::TextDisabled("%s", EditorUi::tr(box.pointer_mode() ? "editor.mode.pointer"
                                                             : "editor.mode.camera"));
    ImGui::EndGroup();
}

void draw_tool_settings(EditorToolbox& box) {
    if (box.common_settings_open()) {
        // THE COMMON PARAMETERS. One so far, and it is the one the user asked
        // for: how far a tool may reach. It lives here rather than in each tool
        // because he said so — «он общий для всех» — and because five copies of
        // a ceiling are five ways to have no ceiling.
        ImGui::TextUnformatted(EditorUi::tr("editor.common.title"));
        ImGui::Separator();
        float reach = box.reach_ceiling_m();
        if (ImGui::SliderFloat(EditorUi::tr("editor.common.reach"), &reach,
                               EDITOR_REACH_MIN_M, EDITOR_REACH_MAX_M, "%.0f m")) {
            box.set_reach_ceiling_m(reach);
        }
        ImGui::TextDisabled("%s", EditorUi::tr("editor.common.reach.note"));
        if (const IEditorTool* held = box.active()) {
            // WHAT IS ACTUALLY IN FORCE, which is the smaller of the two — and
            // saying it here is the difference between a slider and a promise.
            ImGui::Text("%s %.0f m", EditorUi::tr("editor.common.reach.now"),
                        static_cast<double>(box.active_reach_m()));
            ImGui::TextDisabled("%s %.0f m", EditorUi::tr(held->identity().title_key),
                                static_cast<double>(held->max_reach_m()));
        }
        return;
    }
    if (IEditorTool* tool = box.at(box.settings_index())) {
        // EXACTLY ONE TOOL'S SETTINGS. Not the active one — the one whose
        // triangle was pressed: «я не выбирал этот инструмент только настроил».
        ImGui::TextUnformatted(EditorUi::tr(tool->identity().title_key));
        ImGui::Separator();
        tool->draw_settings();
    }
}

} // namespace dfn::app
