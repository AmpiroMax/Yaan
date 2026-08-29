/*
Module: engine/editor
File: engine/editor/sources/EditorToolPath.cpp

Responsibility:
- Рука инструмента троп, объявленного в EditorToolPath.h: куда попал щелчок,
  какой узел схвачен, что нарисовать и что показать в его панели.

Dependencies:
- Uses: EditorToolPath.h, EditorUi.h (tr), engine/world (ReliefPath и вся
  математика кривой), Dear ImGui.
- Used by: engine/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КАЖДАЯ ЗАПИСЬ ТРОПЫ ИДЁТ ЧЕРЕЗ commit(). Второе место, дописывающее точку в
  мир, — это второе место, которое обязано помнить про перепечку канала и про
  пометку чанков, а забыть его можно ровно один раз.
- ВСЕ ВИДИМЫЕ СТРОКИ ЧЕРЕЗ EditorUi::tr (правило 5).
*/

#include "engine/editor/sources/EditorToolPath.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/editor/sources/EditorUi.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace dfn::app {
namespace {

/// 0xAABBGGRR — жёлтая линия тропы. Не зелёная и не красная: у тропы нет «вверх»
/// и «вниз», а зелёный рядом с кистью высоты читался бы как «поднимаю».
constexpr std::uint32_t PATH_LINE = 0xFF44DDFFu;
/// Насколько поднять линию над землёй. Тот же довод, что у кольца кисти
/// (BRUSH_OUTLINE_LIFT_M): линия ровно на поверхности проигрывает собственной
/// земле проверку глубины и не видна вовсе.
constexpr float PATH_LIFT_M = 0.15f;
/// Полуразмер крестика узла, метры.
constexpr float HANDLE_M = 0.4f;

} // namespace

PathTool::PathTool() {
    draft_.half_width_m = 1.5f;
    draft_.edge_softness = 1.0f;
    grabbed_ = draft_.points.size();
}

ToolIdentity PathTool::identity() const {
    return ToolIdentity{"path", "editor.tool.path", "tool.hint.path", ToolIcon::Path};
}

void PathTool::commit(ToolWorld& world) {
    if (!world.commit_path) {
        return; // мир троп не держит: инструмент молчит, а не притворяется
    }
    if (draft_.points.size() < 2) {
        // ТРОПА ИЗ ОДНОЙ ТОЧКИ — ЭТО МЕСТО, А НЕ ТРОПА. Если такая уже лежала в
        // мире (человек убрал предпоследний узел), её надо убрать оттуда, иначе
        // в файле останется огрызок, которого на экране не видно.
        if (editing_ != NO_PATH) {
            (void)world.commit_path(editing_, nullptr);
            editing_ = NO_PATH;
        }
        return;
    }
    editing_ = world.commit_path(editing_, &draft_);
}

void PathTool::on_press(const ToolAim& aim, ToolWorld& world) {
    const glm::vec2 aim_xz{aim.point.x, aim.point.z};
    const std::size_t hit = world::relief_path_pick(draft_, aim_xz, static_cast<float>(config::PATH_GRAB_M));
    if (hit < draft_.points.size()) {
        // СХВАТИЛ УЗЕЛ, А НЕ ПОСТАВИЛ НОВЫЙ. Без этой ветки правка была бы
        // невозможна: щелчок по своей же точке добавлял бы вторую в том же
        // месте, и тропа тихо копила бы узлы-двойники.
        grabbed_ = hit;
        dragging_ = true;
        return;
    }
    draft_.points.push_back(aim_xz);
    grabbed_ = draft_.points.size() - 1;
    dragging_ = true;
    commit(world);
}

void PathTool::on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) {
    (void)dt_s;
    if (!dragging_ || grabbed_ >= draft_.points.size()) {
        return;
    }
    const glm::vec2 aim_xz{aim.point.x, aim.point.z};
    if (draft_.points[grabbed_] == aim_xz) {
        return; // ничего не сдвинулось — незачем перепекать канал
    }
    draft_.points[grabbed_] = aim_xz;
    // ЗЕМЛЯ ХОДИТ ЗА РУКОЙ, а не догоняет её на отпускании: перепечка канала
    // стоит тысячи операций, а показ земли частит ровно настолько, насколько
    // разрешает StrokeRefresh — то есть цена уже посчитана в одном месте.
    commit(world);
}

void PathTool::on_release(ToolWorld& world) {
    dragging_ = false;
    commit(world);
    if (world.finish_stroke) {
        world.finish_stroke();
    }
}

void PathTool::on_deselected(ToolWorld& world) {
    // ТРОПА НЕ ОСТАЁТСЯ В РУКАХ. Она уже записана в мир (каждая точка пишется
    // сразу), поэтому «положить инструмент» значит перестать её ПРАВИТЬ, а не
    // потерять. Черновик очищается, чтобы следующий щелчок начинал новую тропу,
    // а не дописывал ту, о которой человек уже забыл.
    dragging_ = false;
    (void)world;
    // Поперечник — настройка ИНСТРУМЕНТА и переживает смену руки; точки —
    // состояние ОДНОЙ тропы и не переживают.
    draft_.points.clear();
    editing_ = NO_PATH;
}

ToolPreview PathTool::preview(const ToolAim& aim) const {
    // ЗА ПРЕДЕЛОМ ДАЛЬНОСТИ ЭТОТ ИНСТРУМЕНТ НЕ РИСУЕТ НИЧЕГО: вся его картинка —
    // обещание щелчка, а щелчок туда не достанет.
    if (!aim.in_reach) {
        return ToolPreview{};
    }

    ToolPreview out;
    line_.clear();
    handles_.clear();
    const auto lift = [&](glm::vec2 p) {
        const float y = (world_ != nullptr && world_->ground_height)
                          ? world_->ground_height(p)
                          : aim.point.y;
        return glm::vec3{p.x, y + PATH_LIFT_M, p.y};
    };
    if (draft_.points.size() >= 2) {
        for (const glm::vec2& p : world::relief_path_polyline(draft_, 1.0f)) {
            line_.push_back(lift(p));
        }
    }
    for (const glm::vec2& p : draft_.points) {
        // КРЕСТИК ИЗ ДВУХ ОТРЕЗКОВ, нарисованный тем же рисовальщиком ломаных:
        // отдельного «рисуй точки» в контракте нет, и заводить его ради четырёх
        // отрезков значило бы расширять контракт под одну картинку.
        const glm::vec3 c = lift(p);
        handles_.push_back(c - glm::vec3{HANDLE_M, 0.0f, 0.0f});
        handles_.push_back(c + glm::vec3{HANDLE_M, 0.0f, 0.0f});
        handles_.push_back(c - glm::vec3{0.0f, 0.0f, HANDLE_M});
        handles_.push_back(c + glm::vec3{0.0f, 0.0f, HANDLE_M});
    }
    out.polyline = line_.empty() ? nullptr : &line_;
    out.handles = handles_.empty() ? nullptr : &handles_;
    out.line_color = PATH_LINE;
    return out;
}

ToolStatus PathTool::status(const ToolAim& aim) const {
    (void)aim;
    if (world_ == nullptr || !world_->commit_path) {
        // МИР НЕ ДЕРЖИТ ТРОП — сказать это до щелчка, а не молчать после него.
        return ToolStatus{"path.hint.noworld", {}, false};
    }
    if (draft_.points.empty()) {
        return ToolStatus{"path.hint.first", {}, true};
    }
    if (draft_.points.size() == 1) {
        return ToolStatus{"path.hint.second", {}, true};
    }
    return ToolStatus{identity().hint_key, {}, true};
}

void PathTool::draw_settings() {
    // ПОПЕРЕЧНИК. Числа те же, что уедут в файл, и никаких вторых: слайдер
    // правит поля ReliefPath напрямую.
    ImGui::SliderFloat(EditorUi::tr("path.halfwidth"), &draft_.half_width_m,
                       world::PATH_MIN_HALF_WIDTH_M, 8.0f, "%.2f m");
    ImGui::SliderFloat(EditorUi::tr("path.softness"), &draft_.edge_softness, 0.0f, 1.0f,
                       "%.2f");
    if (draft_.edge_softness * draft_.half_width_m < world::PATH_MIN_FADE_M) {
        // ПОЛ ОБЪЯСНЁН, А НЕ ПРОСТО ПРИМЕНЁН. Ползунок, который молча
        // перестаёт действовать, читается как сломанный; здесь сказано, что
        // ниже решётки земли спад выразить нечем и край станет лесенкой.
        ImGui::TextDisabled("%s", EditorUi::tr("path.softness.floor"));
    }

    ImGui::Spacing();
    if (world_ == nullptr || !world_->commit_path) {
        ImGui::TextDisabled("%s", EditorUi::tr("path.hint.noworld"));
        return;
    }

    ImGui::Text("%s %zu", EditorUi::tr("path.nodes"), draft_.points.size());
    if (ImGui::Button(EditorUi::tr("path.undo")) && !draft_.points.empty()) {
        draft_.points.pop_back();
        commit(*world_);
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("path.finish"))) {
        // ЗАКОНЧИТЬ — значит отпустить эту тропу и начать следующую. Она уже в
        // мире: точки пишутся сразу, поэтому здесь нечего сохранять.
        draft_.points.clear();
        editing_ = NO_PATH;
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("path.delete"))) {
        if (editing_ != NO_PATH) {
            (void)world_->commit_path(editing_, nullptr);
        }
        draft_.points.clear();
        editing_ = NO_PATH;
    }

    if (!world_->relief_paths) {
        return;
    }
    const std::vector<world::ReliefPath>* paths = world_->relief_paths();
    if (paths == nullptr) {
        return;
    }
    ImGui::Spacing();
    ImGui::Text("%s %zu", EditorUi::tr("path.list"), paths->size());
    if (ImGui::BeginChild("path.list", ImVec2(0.0f, 180.0f), ImGuiChildFlags_Border)) {
        for (std::size_t i = 0; i < paths->size(); ++i) {
            char label[96];
            std::snprintf(label, sizeof(label), "%s %zu — %s %zu",
                          EditorUi::tr("path.item"), i + 1, EditorUi::tr("path.nodes"),
                          (*paths)[i].points.size());
            if (ImGui::Selectable(label, i == editing_)) {
                // ВЫБРАТЬ — ЗНАЧИТ ВЗЯТЬ В ПРАВКУ. Копия в черновик, а не
                // указатель: мир перепекает свой вектор на каждой записи, и
                // указатель на его элемент прожил бы ровно до первой правки.
                draft_ = (*paths)[i];
                editing_ = i;
            }
        }
    }
    ImGui::EndChild();
}

} // namespace dfn::app
