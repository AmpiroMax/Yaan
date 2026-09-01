/*
Module: engine/editor
File: engine/editor/sources/EditorMorphView.cpp

Responsibility:
- Содержимое панели ползунков тела. См. EditorMorphView.h — там весь довод.

Dependencies:
- Uses: EditorMorphView.h, Dear ImGui.
- Used by: dfn_editor.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/editor/sources/EditorMorphView.h"

#include <imgui.h>

#include <cstdio>
#include <string>

namespace dfn::app {
namespace {

/// Ключ подписи ручки: "morph.slider.<имя цели>". Имена целей — ASCII и лежат
/// в файле; локализация подхватывает их по этому ключу, а незнакомое имя
/// покажет сам ключ — что и правильно: это видно и чинится одной строкой в
/// таблице, а не молчит.
[[nodiscard]] std::string label_key(const std::string& name) {
    return "morph.slider." + name;
}

} // namespace

void draw_morph_panel(MorphModel& model, const MorphHooks& hooks) {
    if (!model.body_ready) {
        ImGui::TextDisabled("%s", EditorUi::tr("morph.no.body"));
        return;
    }
    if (model.sliders.empty()) {
        // ДВА РАЗНЫХ «НИЧЕГО». Тело загружено, а ручек нет — это тело БЕЗ
        // секции MORF (например, уже выпеченное), и сказать об этом надо
        // словами: пустая панель читается как сломанная панель.
        ImGui::TextDisabled("%s", EditorUi::tr("morph.no.targets"));
        return;
    }

    // КНОПКИ ПРИКОЛОЧЕНЫ КО ДНУ, А СПИСОК РУЧЕК ПРОКРУЧИВАЕТСЯ. Правая колонка
    // редактора делится с панелью «Свойства», и одиннадцати ручек в неё не
    // влезает; без этой рамки «Выпечь» уезжала за нижний край — то есть кнопка
    // существовала для того, кто знает, что её надо прокрутить.
    const float footer = ImGui::GetFrameHeightWithSpacing()
                         + ImGui::GetTextLineHeightWithSpacing()
                         + ImGui::GetStyle().ItemSpacing.y * 2.0f;
    ImGui::BeginChild("morph.sliders", ImVec2(0.0f, -footer), false);
    for (std::size_t i = 0; i < model.sliders.size(); ++i) {
        MorphSliderModel& s = model.sliders[i];
        ImGui::PushID(static_cast<int>(i));
        // ХОД РУЧКИ — В ЕЁ ПОДПИСИ, А НЕ ОТДЕЛЬНОЙ КОЛОНКОЙ СПРАВА. Приписанный
        // через SameLine, он вылезал за край докнутой колонки и обрезался ровно
        // у тех ручек, у которых длиннее имя; ImGui сам верстает подпись внутри
        // окна, и одна строка туда влезает, а две колонки — нет.
        std::string label = std::string(EditorUi::tr(label_key(s.name).c_str()));
        if (s.travel_mm > 0.0f) {
            char tail[32];
            std::snprintf(tail, sizeof(tail), " · %.0f мм",
                          static_cast<double>(s.travel_mm));
            label += tail;
        }
        // ПОЛОСА — ИЗ ФАЙЛА. SliderFloat зажимает сам, но зажим-истина живёт у
        // тела: сюда полоса приходит затем, чтобы ручка не ЕЗДИЛА туда, откуда
        // её всё равно вернут, — человек не должен тянуть в пустоту.
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.44f);
        if (ImGui::SliderFloat(label.c_str(), &s.value, s.lo, s.hi, "%.2f")
            && hooks.set) {
            hooks.set(i, s.value);
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::Separator();
    if (ImGui::Button(EditorUi::tr("morph.reset")) && hooks.reset) {
        hooks.reset();
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("morph.preset.save")) && hooks.save_preset) {
        hooks.save_preset();
    }
    ImGui::SameLine();
    // ВЫПЕЧКА СТОИТ ТРЕТЬЕЙ И ПОДПИСАНА ИНАЧЕ. Она пишет ТЕЛО, а не числа, и
    // тело это едет в мир уже без ползунков — необратимость надо видеть до
    // нажатия, а не узнавать после.
    if (ImGui::Button(EditorUi::tr("morph.bake")) && hooks.bake) {
        hooks.bake();
    }
    if (!model.status.empty()) {
        ImGui::TextWrapped("%s", model.status.c_str());
    }
}

} // namespace dfn::app
