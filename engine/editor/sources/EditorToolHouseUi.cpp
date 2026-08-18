/*
Created: 18:08:2026 - 18:02:11
Last updated: 18:08:2026 - 18:02:11
Module: engine/editor
File: engine/editor/sources/EditorToolHouseUi.cpp

Responsibility:
- ПАНЕЛИ трёх инструментов постройки: высота вершины над землёй, зажим длины у
  прямой, числа и лицо у поверхности. Здесь и только здесь живёт Dear ImGui —
  решения лежат в EditorToolHouse.cpp, у которого нет ни окна, ни рисования.

WHY THE SPLIT (правило 3): цель app_editor_house линкует EditorToolHouse.cpp и
НЕ линкует этот файл. Иначе рукав тянул бы за собой ImGui, а вместе с ним и
контекст окна — и ни один вопрос про нормаль, зажим и отказ нельзя было бы
задать без экрана.

Dependencies:
- Uses: EditorToolHouse.h, EditorUi.h (tr), Dear ImGui.
- Used by: engine/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ВСЕ ВИДИМЫЕ СТРОКИ ЧЕРЕЗ EditorUi::tr (правило 5). Исключение — ОТКАЗ
  МОДЕЛИ и подписи со списком держателей: это уже готовое предложение, которое
  собрал инструмент, и второй перевод на месте показа сделал бы из одного
  приговора два.
- НИ ОДНОГО РЕШЕНИЯ ЗДЕСЬ. Кнопка зовёт метод инструмента; если кнопке нужно
  что-то посчитать — считать это надо в EditorToolHouse.cpp, иначе оно окажется
  за пределами рукава.
*/
/*
UPD:
- 18:08:2026 - 18:02:11: Создан вместе с EditorToolHouse.{h,cpp}.
*/

#include "engine/editor/sources/EditorToolHouse.h"
#include "engine/editor/sources/EditorUi.h"

#include <imgui.h>

#include <cstdio>

namespace dfn::app {
namespace {

/// Мир, которого нет. Панель рисуется и тогда, когда крючки не розданы (проверка
/// без App), а кнопке «создать» мир нужен по подписи метода — пустой ToolWorld
/// честнее указателя, который иногда null.
ToolWorld& no_world() {
    static ToolWorld empty;
    return empty;
}

void draw_refusal(const std::string& text) {
    if (text.empty()) {
        return;
    }
    // ОТКАЗ ВИДЕН, А НЕ УХОДИТ В stderr. Ровно это разбирали 18.08 трижды:
    // молча не сработавший инструмент неотличим от сломанного.
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", text.c_str());
}

} // namespace

void HouseVertexTool::draw_settings() {
    // ВЫСОТА НАД ЗЕМЛЁЙ — ГЛАВНЫЙ ОРГАН ЭТОГО ИНСТРУМЕНТА. Ноль заземляет
    // вершину, больше нуля вешает её в воздухе, и тогда у неё появляется
    // пунктирный отвес: «я буду видеть, над какой точкой ставлю свой объект».
    ImGui::SliderFloat(EditorUi::tr("house.air"), &air_height_m_, 0.0f, 20.0f, "%.2f m");
    if (air_height_m_ <= HOUSE_AIR_EPS_M) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.air.ground"));
    } else {
        ImGui::TextDisabled("%s", EditorUi::tr("house.air.plumb"));
    }

    ImGui::Spacing();
    if (session_ == nullptr) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.hint.nomodel"));
        return;
    }
    ImGui::Text("%s %zu · %s %zu", EditorUi::tr("house.vertices"),
                session_->graph().vertex_count(), EditorUi::tr("house.elements"),
                session_->graph().element_count());

    const world::VertexId sel = session_->selected_vertex();
    if (sel == world::NO_VERTEX) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.nosel"));
    } else {
        const world::Vertex* v = session_->graph().vertex(sel);
        const char* how = "?";
        if (v != nullptr) {
            how = v->anchoring == world::Anchoring::OnGround  ? EditorUi::tr("house.anch.ground")
                : v->anchoring == world::Anchoring::OnEdge    ? EditorUi::tr("house.anch.edge")
                                                              : EditorUi::tr("house.anch.free");
        }
        const glm::vec3 p = session_->vertex_world(sel);
        ImGui::Text("v%u · %s · (%.2f %.2f %.2f)", static_cast<unsigned>(sel), how,
                    static_cast<double>(p.x), static_cast<double>(p.y),
                    static_cast<double>(p.z));
        // ПОДСВЕЧЕННЫЕ ЭЛЕМЕНТЫ НАЗЫВАЮТСЯ И СЛОВАМИ. Свечение на экране
        // отвечает «какие», список отвечает «сколько», и второе видно, даже
        // когда камера смотрит в другую сторону.
        ImGui::Text("%s %zu", EditorUi::tr("house.incident"),
                    session_->lit_elements().size());
        if (ImGui::Button(EditorUi::tr("house.delete"))) {
            (void)delete_selected();
        }
    }
    draw_refusal(refusal_);
}

void HouseLineTool::draw_settings() {
    ImGui::SliderFloat(EditorUi::tr("house.radius"), &radius_m_, 0.02f, 1.0f, "%.3f m");

    // ЗАЖИМ ДЛИНЫ — ТРИ ПОЛОЖЕНИЯ, А НЕ ГАЛОЧКА. «Механика клипа длины прямой
    // до ближайшего сверху / снизу НА ВЫБОР якоря»: направление выбирает
    // человек, потому что ближайший вперёд и ближайший назад — разные ответы, и
    // угадывать за него значит промахиваться в половине случаев.
    ImGui::Text("%s", EditorUi::tr("house.clamp"));
    int mode = static_cast<int>(clamp_);
    ImGui::RadioButton(EditorUi::tr("house.clamp.none"), &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton(EditorUi::tr("house.clamp.above"), &mode, 1);
    ImGui::SameLine();
    ImGui::RadioButton(EditorUi::tr("house.clamp.below"), &mode, 2);
    clamp_ = static_cast<HouseClamp>(mode);

    if (clamp_hit_.found) {
        ImGui::Text("%s %.2f m (v%u)", EditorUi::tr("house.clamp.now"),
                    static_cast<double>(clamp_hit_.length_m),
                    static_cast<unsigned>(clamp_hit_.at));
    } else {
        ImGui::TextDisabled("%s", EditorUi::tr("house.clamp.free"));
    }

    ImGui::Spacing();
    if (last_ != world::NO_ELEMENT && session_ != nullptr) {
        ImGui::Text("%s e%u", EditorUi::tr("house.last"), static_cast<unsigned>(last_));
    }
    draw_refusal(refusal_);
}

void HouseSurfaceTool::draw_settings() {
    if (session_ == nullptr) {
        ImGui::TextDisabled("%s", EditorUi::tr("house.hint.nomodel"));
        return;
    }
    ImGui::Text("%s %zu", EditorUi::tr("house.walk"), refs_.size());

    // КУДА СМОТРИТ ЛИЦО — ДО ПОДТВЕРЖДЕНИЯ И ЧИСЛАМИ. Стрелка в мире отвечает
    // на тот же вопрос, но её видно не с каждого ракурса, а порядок обхода
    // назад не отматывается: текстура ляжет на изнанку, и узнается это на
    // готовом доме.
    glm::vec3 n{0.0f};
    if (draft_normal(n)) {
        ImGui::Text("%s (%.2f %.2f %.2f)", EditorUi::tr("house.facing"),
                    static_cast<double>(n.x), static_cast<double>(n.y),
                    static_cast<double>(n.z));
    } else {
        ImGui::TextDisabled("%s", EditorUi::tr("house.facing.none"));
    }
    ImGui::Checkbox(EditorUi::tr("house.flip"), &flipped_);

    ImGui::SliderFloat(EditorUi::tr("house.thickness"), &thickness_m_, 0.02f, 1.0f, "%.3f m");
    ImGui::SliderFloat(EditorUi::tr("house.height"), &height_m_, 0.1f, 12.0f, "%.2f m");
    ImGui::TextDisabled("%s", EditorUi::tr("house.height.chain"));
    ImGui::SliderFloat(EditorUi::tr("house.tex"), &tex_deg_, 0.0f, 360.0f, "%.0f°");

    ImGui::Spacing();
    ToolWorld& w = world_ != nullptr ? *world_ : no_world();
    if (ImGui::Button(EditorUi::tr("house.make.chain"))) {
        (void)confirm(w);
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("house.make.contour"))) {
        // ЗАМКНУТЬ И СОЗДАТЬ — ТОТ ЖЕ ЖЕСТ, ЧТО ЩЕЛЧОК ПО ПЕРВОМУ ЯКОРЮ, и он
        // тот же метод: closed выставляется здесь, а создаёт confirm(). Второе
        // место, которое само собирает элемент, забыло бы про facing или про
        // толщину — вопрос только в том, когда.
        closed_ = true;
        (void)confirm(w);
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("house.walk.back"))) {
        undo_last();
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorUi::tr("house.walk.drop"))) {
        clear_draft();
    }
    draw_refusal(refusal_);
}

} // namespace dfn::app
