/*
Created: 18:08:2026 - 12:04:20
Last updated: 18:08:2026 - 23:20:00
Module: engine/editor
File: engine/editor/sources/EditorToolbox.cpp

Responsibility:
- EditorToolbox declared in EditorToolbox.h: the single active pointer, the two
  gestures of the double button, the reach ceiling and the frame dispatch.

Dependencies:
- Uses: EditorToolbox.h, std. Nothing else — no ImGui, no renderer, no App.
- Used by: EditorUi, EditorToolbar, engine/app, tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- select() is the only writer of active_. Keep it that way: the moment there
  are two, on_deselected has two callers and one of them will be forgotten —
  which is the whole finding of docs/AUDIT_EDITOR_TOOLS.md.
*/
/*
UPD:
- 18:08:2026 - 12:04:20: Создан вместе с заголовком.
- 18:08:2026 - 12:51:26: «ЦЕЛЬ ДАЛЕКО» ГОВОРИТСЯ ЯЩИКОМ, а не каждым инструментом (заказ 18.08:
  «не понятно могу ли я рисовать / строить из-за расстояния, нужно индикатор
  добавить»). Потолок общий — значит и объяснение общее; инструмент, которому
  пришлось бы помнить про чужой предел, однажды про него забудет. Подпись
  несёт ДВА ЧИСЛА («46 m > 10 m»): «далеко» не говорит, насколько подойти, а
  пара говорит и заодно ловит слишком низко выставленный предел. Вместе с
  подписью гаснет ПРЕВЬЮ: зелёное кольцо на недосягаемой точке — обещание,
  которого щелчок не выполнит, и это была бы та же неясность в другом месте.
- 18:08:2026 - 18:58:40: Луч, не встретивший земли, отдаётся инструменту с фиксированной осью — иначе стойку нельзя вести вверх вообще.
- 18:08:2026 - 20:26:30: Показ зовётся ВСЕГДА, с флагом дальности: постройка перестала исчезать, когда человек отходит.
- 18:08:2026 - 23:20:00: Протаскивание не рвётся дальностью у тех, кто ведёт уже взятое.
*/

#include "engine/editor/sources/EditorToolbox.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace dfn::app {

std::size_t EditorToolbox::add(std::unique_ptr<IEditorTool> tool) {
    if (tool == nullptr) {
        return NO_TOOL;
    }
    tools_.push_back(std::move(tool));
    return tools_.size() - 1;
}

IEditorTool* EditorToolbox::at(std::size_t index) const {
    return index < tools_.size() ? tools_[index].get() : nullptr;
}

std::size_t EditorToolbox::index_of(const char* id) const {
    if (id == nullptr) {
        return NO_TOOL;
    }
    for (std::size_t i = 0; i < tools_.size(); ++i) {
        const char* have = tools_[i]->identity().id;
        if (have != nullptr && std::strcmp(have, id) == 0) {
            return i;
        }
    }
    return NO_TOOL;
}

void EditorToolbox::clear() {
    tools_.clear();
    active_ = NO_TOOL;
    settings_ = NO_TOOL;
    common_settings_ = false;
    holding_ = false;
    was_down_ = false;
}

void EditorToolbox::select(std::size_t index, ToolWorld& world) {
    if (index != NO_TOOL && index >= tools_.size()) {
        return;
    }
    if (index == active_) {
        return;
    }
    // THE ONE PLACE A TOOL IS PUT DOWN. Whatever it was holding — a half-dug
    // stroke, a part stuck to the hand — ends here, before the next tool gets
    // the button. A press in flight is dropped with it: an unfinished stroke
    // that survived the switch would bite the ground the next tool aims at.
    if (IEditorTool* prev = at(active_)) {
        if (holding_) {
            prev->on_release(world);
        }
        prev->on_deselected(world);
        // Its settings window goes with it: the user asked that putting a tool
        // down take ALL of its interface with it.
        if (settings_ == active_) {
            settings_ = NO_TOOL;
        }
    }
    holding_ = false;
    active_ = index;
    if (IEditorTool* next = at(active_)) {
        next->on_selected(world);
        std::fprintf(stderr, "[инструменты] в руке: %s\n", next->identity().id);
    } else {
        std::fprintf(stderr, "[инструменты] в руке: ничего\n");
    }
}

void EditorToolbox::click_icon(std::size_t index, ToolWorld& world) {
    if (index >= tools_.size()) {
        return;
    }
    // CLICKING THE ONE IN HAND PUTS IT DOWN (user, 18.08: «если я кликну на
    // иконку выбранного уже инструмента, выбор сбросится»). Both directions go
    // through select(), so the cleanup runs either way.
    select(index == active_ ? NO_TOOL : index, world);
}

void EditorToolbox::click_settings(std::size_t index) {
    if (index >= tools_.size()) {
        return;
    }
    // NOTE WHAT IS NOT HERE: no write to active_. This function cannot change
    // what is in hand, which is the user's rule stated as code rather than as
    // care — «я кликаю на настройки другого, инструмент не меняется в руках».
    common_settings_ = false;
    settings_ = (settings_ == index) ? NO_TOOL : index;
}

void EditorToolbox::click_gear() {
    settings_ = NO_TOOL;
    common_settings_ = !common_settings_;
}

bool EditorToolbox::close_settings() {
    if (!settings_open()) {
        return false;
    }
    settings_ = NO_TOOL;
    common_settings_ = false;
    return true;
}

void EditorToolbox::set_reach_ceiling_m(float metres) {
    reach_ceiling_m_ = std::clamp(metres, EDITOR_REACH_MIN_M, EDITOR_REACH_MAX_M);
}

float EditorToolbox::active_reach_m() const {
    const IEditorTool* tool = active();
    if (tool == nullptr) {
        return 0.0f;
    }
    // THE SMALLER OF THE TWO. A tool may be shorter-armed than the ceiling
    // (planting a whole dab at forty metres is a different mistake), but no
    // tool can outreach the common parameter — that is what makes it common.
    return std::min(reach_ceiling_m_, tool->max_reach_m());
}

bool EditorToolbox::in_reach(const ToolAim& aim) const {
    if (active() == nullptr) {
        return false;
    }
    // A RAY THAT MET NOTHING IS OUT OF REACH. Looking at the sky is not an
    // error, but it is not a target either: the aim point in that case is an
    // arm's length of nothing, and acting on it would build in mid-air.
    //
    // ИСКЛЮЧЕНИЕ — ИНСТРУМЕНТ С ФИКСИРОВАННОЙ ОСЬЮ: он берёт точку с прямой
    // через якорь, а не с земли, и небо для него — законное направление. Без
    // этой строки стойку нельзя было бы вести вверх вообще: как только луч
    // уходит выше горизонта, ящик перестал бы отдавать протаскивание.
    if (!aim.hit && !active()->aims_without_ground()) {
        return false;
    }
    if (!aim.hit) {
        return true;
    }
    return aim.distance_m <= active_reach_m();
}

ToolTickReport EditorToolbox::update(const ToolAim& aim, float dt_s,
                                     bool button_down, ToolWorld& world) {
    ToolTickReport out;
    IEditorTool* tool = active();
    const bool was_down = was_down_;
    was_down_ = button_down;
    if (tool == nullptr) {
        holding_ = false;
        return out;
    }
    // THE INTERFACE OWNS THE CLICK WHILE IT IS THE POINTER'S TARGET. Two
    // separate reasons, one answer: the pointer is over a panel, or the user
    // has switched the mouse to the interface with R and is not aiming at all.
    const bool blocked = pointer_mode_ || aim.pointer_over_ui;
    if (!was_down && button_down) {
        if (blocked) {
            out.blocked = true;
            holding_ = false;
            return out;
        }
        if (!in_reach(aim)) {
            // SAID OUT LOUD, because this is the user's complaint made visible:
            // «я не должен уметь за 1000 км что-то строить». A click that is
            // refused for distance and says nothing looks exactly like a click
            // that was swallowed by a bug.
            out.out_of_reach = true;
            holding_ = false;
            std::fprintf(stderr,
                         "[инструменты] далеко: %.1f м при потолке %.1f м — "
                         "щелчок не принят\n",
                         static_cast<double>(aim.distance_m),
                         static_cast<double>(active_reach_m()));
            return out;
        }
        holding_ = true;
        tool->on_press(aim, world);
        out.pressed = true;
        return out;
    }
    if (button_down && holding_) {
        // ВЗЯТОЕ В РУКУ НЕ ВЫПАДАЕТ ИЗ-ЗА ДАЛЬНОСТИ. Потолок судит НАЧАЛО
        // работы — «я не должен уметь за 1000 км что-то строить», — и он уже
        // отсудил его на нажатии. Дальше человек ведёт то, что держит, и
        // спрашивать снова, далеко ли ушёл ПРИЦЕЛ, значит рвать движение на
        // ровном месте: пользователь 18.08 показал это, потянув якорь вверх, —
        // луч ушёл в небо, и якорь замер, хотя держали его в метре от лица.
        //
        // Кисти это не касается: у неё каждый шаг штриха — новый укус, и
        // ограничение ей нужно. Поэтому спрашивается сам инструмент.
        if (!in_reach(aim) && tool->stroke_needs_reach()) {
            out.out_of_reach = true;
            return out;
        }
        tool->on_drag(aim, dt_s, world);
        out.dragged = true;
        return out;
    }
    if (!button_down && was_down && holding_) {
        holding_ = false;
        tool->on_release(world);
        out.released = true;
    }
    return out;
}

ToolPreview EditorToolbox::preview(const ToolAim& aim) const {
    const IEditorTool* tool = active();
    if (tool == nullptr) {
        // NOTHING IN HAND DRAWS NOTHING. One return, and it is the whole of
        // «весь UI дополнительный для этого пропадет».
        return ToolPreview{};
    }
    // ЗА ПРЕДЕЛОМ ДАЛЬНОСТИ НЕ РИСУЕТСЯ ОБЕЩАНИЕ. Зелёное кольцо кисти на
    // точке, до которой щелчок не достанет, — обещание, которого инструмент не
    // выполнит: цвет говорит «подниму землю», а нажатие не сделает ничего.
    // Ровно эту неясность пользователь назвал 18.08 — «не понятно могу ли я
    // рисовать / строить из-за расстояния».
    //
    // НО НЕ ВСЁ В КАРТИНКЕ — ОБЕЩАНИЕ. У постройки в той же стопке едет сам
    // дом, и он факт: он стоит в мире и не имеет права исчезать оттого, что
    // человек отошёл («когда я удаляюсь от якорей и линий, они исчезают, хотя я
    // хочу чтобы рисовались»). Ящик сообщает ОБСТОЯТЕЛЬСТВО, а разделяет
    // обещание и факт тот, кто их различает, — сам инструмент.
    ToolAim told = aim;
    told.in_reach = in_reach(aim);
    return tool->preview(told);
}

ToolStatus EditorToolbox::status(const ToolAim& aim) const {
    const IEditorTool* tool = active();
    if (tool == nullptr) {
        return ToolStatus{"tool.hint.empty", {}, false};
    }
    // «ЦЕЛЬ ДАЛЕКО» ГОВОРИТСЯ ЗДЕСЬ, А НЕ В КАЖДОМ ИНСТРУМЕНТЕ, и по той же
    // причине, по которой здесь же живёт САМ отказ по дальности: потолок общий,
    // значит и объяснение общее. Инструмент, которому пришлось бы помнить про
    // чужой предел, однажды про него забудет — и щелчок будет молча не
    // срабатывать, что пользователь и описал 18.08: «сейчас не понятно могу ли
    // я рисовать / строить из-за расстояния, нужно индикатор добавить, что
    // далеко цель».
    //
    // ПРОВЕРКА ИДЁТ ПЕРВОЙ, до вопроса инструменту: за пределом его ответ не
    // имеет значения — «поставлю сюда» при недосягаемой точке это обещание,
    // которого щелчок не выполнит, и хуже молчания.
    if (!in_reach(aim)) {
        ToolStatus out;
        out.key = "tool.hint.too_far";
        // ДВА ЧИСЛА, А НЕ СЛОВО. «Далеко» не говорит, насколько подойти;
        // «46 м, предел 30 м» говорит, и оно же ловит случай, когда предел
        // выставлен ниже, чем человек думает.
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.0f m > %.0f m",
                      static_cast<double>(aim.distance_m),
                      static_cast<double>(active_reach_m()));
        out.text = buf;
        out.ready = false;
        return out;
    }
    return tool->status(aim);
}

} // namespace dfn::app
