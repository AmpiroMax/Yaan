/*
Created: 18:08:2026 - 16:59:18
Last updated: 18:08:2026 - 23:52:10
Module: engine/app
File: engine/app/sources/AppInput.cpp

Responsibility:
- EVERY KEY HANDLER THE APP HAS, and the one walk of the dispatch table that
  reaches them. Until today these eighteen handlers were spread through 430
  lines of App::run(), each interleaved with a piece of the frame.

Key items:
- App::dispatch_actions(): the walk. Returns false when the frame is abandoned.
- App::on_*(): one method per row of AppActions.cpp's table.
- App::update_part_rotation(), App::service_chat_typing(): the two key paths
  that are polled rather than dispatched, and say so.

Dependencies:
- Uses: App.h, AppActions.h, Controls.h. Same subsystems the handlers always
  used -- nothing new was introduced by moving them.
- Used by: App.cpp (run() calls dispatch_actions once per frame).

Notes:
- WHAT THIS FILE IS AND IS NOT TESTABLE. It is NOT: it defines App methods, and
  App owns a window. What moving the handlers here buys is that the DECISIONS
  they used to carry inline are now data in AppActions.cpp, which is a separate
  translation unit with no App in it, compiled into app_controls and measured
  there. This file is the effect; the decision is next door. That split is the
  whole point of the layer, and pretending otherwise would be the "extraction
  without an arm" the plan calls out by name.
- THE ORDER OF DISPATCH CHANGED, and it is safe for a reason that is checked
  rather than assumed: the walk is in Action order, while run() ran the
  handlers in the order they had accreted. Two handlers can only care about
  their relative order if the same key can reach both, and
  tests/app/ControlsTests holds that no two actions share a key in one scope.
  The one ordering that IS load-bearing is kept explicitly: as soon as a
  handler has moved the app into the pause menu the walk stops, because the
  frame it was dispatching no longer exists.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- A NEW KEY GOES IN THREE PLACES: Controls.cpp (which key), AppActions.cpp
  (which method), here (the method). Two of the three are compile errors if
  missed; the third is app_controls going red.
*/
/*
UPD:
- 18:08:2026 - 16:59:18: Создан. Слой 1 разбора App.cpp (docs/PLAN_APP_DECOMPOSITION.md):
  обработчики клавиш (чат, ESC, полный экран, Tab, третье лицо, вывод, снимок,
  каркас, пять инструментов, курсор, меню объектов, поворот, снимок экрана,
  замечание, запись и повтор траектории, карта) уехали из run() сюда, а
  ЗАПРЕТ «пока печатают — клавиша не проходит» перестал быть восемнадцатью
  одинаковыми условиями и стал колонкой таблицы.
- 18:08:2026 - 18:02:11: ОТМЕНА ПОЛУЧИЛА МОДЕЛЬ. on_undo_redo снимал снимок со стопки и только
  печатал его имя — то есть отменял в никуда. Теперь снимок применяется к
  постройке (HouseSession::apply_snapshot), и это ЕДИНСТВЕННОЕ место, куда он
  приходит: история про модель не знает нарочно, иначе её нельзя было бы
  проверить без мира.
- 18:08:2026 - 18:58:40: on_axis_lock: переключает ось постройки и говорит об этом вслух.
- 18:08:2026 - 19:14:22: Enter в редакторе сначала подтверждает черновик инструмента и только потом открывает быструю заметку.
- 18:08:2026 - 20:26:30: on_delete_selected: сессия решает, что убрать, отказ печатается со списком держателей.
- 18:08:2026 - 22:20:15: V крутит ось вокруг выбранного якоря и называет её.
- 18:08:2026 - 23:20:00: Esc третьим шагом бросает набранное; стрелки двигают выбранный якорь шагом сетки; cmd+shift+цифра открывает меню инструмента; отсечки сетки вокруг прицела.
- 18:08:2026 - 23:52:10: on_grid_toggle; стрелки садят якорь В УЗЕЛ сетки, а не двигают на дельту.
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/AppActions.h"
#include "engine/app/sources/AppDoors.h"

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/BodyMesh.h"

#include <cstdio>
#include <cstddef>

#include <glm/gtc/constants.hpp>

namespace dfn::app {

bool App::dispatch_actions(bool chat_typing) {
    // THE CHAT WINDOW FIRST, because while it is up it owns the keyboard and
    // the table below must not fire at all. The physical keys still report
    // was_pressed() -- text_input() collects codepoints beside them, it does
    // not consume them -- so this is a decision, not a formality.
    if (chat_typing) {
        service_chat_typing();
    }

    for (const ActionRoute& r : action_routes()) {
        if (chat_typing && r.gate == Gate::TypingEats) {
            continue;
        }
        if (!action_pressed(r.action)) {
            continue;
        }
        // THE ONE SWITCH, and it has no `default:` on purpose: -Wswitch makes
        // an Action added without a case a compiler diagnostic here, which is
        // the half of "the table is total" a test cannot see (a test can read
        // the table; only the compiler can read this file).
        switch (r.action) {
        case Action::ThirdPerson: on_third_person(); break;
        case Action::DebugReadout: on_debug_readout(); break;
        case Action::StateCapture: on_state_capture(); break;
        case Action::Wireframe: on_wireframe(); break;
        case Action::Screenshot: on_screenshot(); break;
        case Action::ToggleBody: on_toggle_body(); break;
        case Action::TrajectoryRecord: on_trajectory_record(); break;
        case Action::TrajectoryReplay: on_trajectory_replay(); break;
        case Action::ChatWindow: on_chat_window(); break;
        case Action::QuickRemark: on_quick_remark(); break;
        case Action::Map: on_map(); break;
        case Action::MenuPause: on_menu_pause(); break;
        case Action::Fullscreen: on_fullscreen(); break;
        case Action::CursorToggle: on_cursor_toggle(); break;
        case Action::BuildMenu: on_build_menu(); break;
        case Action::BuildRotate: on_build_rotate(); break;
        case Action::ToolHeight:
        case Action::ToolPaint:
        case Action::ToolSelect:
        case Action::ToolPlace:
        case Action::ToolLook:
        case Action::Tool6:
        case Action::Tool7:
        case Action::Tool8:
        case Action::Tool9:
            on_tool_pick(r.arg);
            break;
        case Action::Undo: on_undo_redo(); break;
        case Action::AxisLock: on_axis_lock(); break;
        case Action::DeleteSelected: on_delete_selected(); break;
        case Action::GridToggle: on_grid_toggle(); break;
        case Action::Count: break; // not a row; route_for() cannot return it
        }
        // THE FRAME IS OVER THE MOMENT ESCAPE OPENS THE PAUSE PAGE. Nothing
        // below has a world to act on, and run() abandons the iteration.
        if (mode_ == AppMode::Menu) {
            return false;
        }
    }

    if (!chat_typing) {
        update_part_rotation();
    }
    return true;
}

// CHAT OVERLAY (В28): the typed-chat window. Opened with '/' -- Enter already
// drops a snapshot, 4/F4 are the editor's wireframe, and T is the time scale,
// so none of those is free. Enter SENDS (a remark with the frame's snapshot
// attached, through the same write_pending_chat the DFN_CHAT_MSG door uses);
// Escape closes.
void App::service_chat_typing() {
    chat_overlay_.feed_text(input_->text_input());
    if (input_->was_pressed(platform::Key::BACKSPACE)) {
        chat_overlay_.backspace();
    }
    if (input_->was_pressed(platform::Key::ENTER) && !chat_overlay_.input_empty()) {
        chat_overlay_.push_history("you", chat_overlay_.input());
        chat_pending_entry_ = ChatEntry{};
        chat_pending_entry_.who = "human";
        chat_pending_entry_.text = chat_overlay_.take_input();
        chat_pending_ = true; // serviced after render(): attaches the snapshot
    }
    if (input_->was_pressed(platform::Key::ESCAPE)) {
        chat_overlay_.close();
    }
}

void App::on_chat_window() {
    if (mode_ == AppMode::Playing || mode_ == AppMode::Editor) {
        chat_overlay_.open();
    }
}

// FULLSCREEN, AND IT REMEMBERS. Toggling the window changes the framebuffer,
// which the normal consume_resize() path forwards to the renderer -- there is
// no fullscreen-specific rendering path and there must not be one. The answer
// is written back to settings.cfg at once: a fullscreen key you have to press
// every launch is a key that does not work, it just does something.
void App::on_fullscreen() {
    if (!window_) {
        return;
    }
    const bool want = !window_->is_fullscreen();
    window_->set_fullscreen(want);
    if (window_->is_fullscreen() == want) {
        config_.fullscreen = want;
        write_settings(config_);
    } else {
        // Loud: a backend that refused must not leave settings.cfg claiming a
        // mode the window is not in.
        std::fprintf(stderr, "[window] полный экран не переключился\n");
    }
}

// ESC pauses. Cursor is released so the pointer is usable, and the world stops
// ticking because Menu mode skips the whole simulation.
//
// THERE WERE TWO ESCAPE HANDLERS HERE ONCE and the first one called
// request_close(). Both ran on the same edge, so ESC opened the pause menu AND
// asked the window to close, and the app quit on the next iteration -- the
// pause screen existed but could never be seen. It survived review because each
// half is correct on its own; only the pair is wrong. One row, one method, one
// owner is what makes that pair unrepresentable now (Rule 32).
void App::on_menu_pause() {
    if (render_system_.map_open()) {
        render_system_.set_map_open(false);
        return;
    }
    if (mode_ == AppMode::Editor
        && (editor_ui_.toolbox().close_settings() || editor_ui_.close_all_panels())) {
        // ESC СНАЧАЛА ЗАКРЫВАЕТ ОТКРЫТОЕ ОКНО (заказ 18.08: «esc будет закрывать
        // открытое окно объектов / кистей, не важно что открыто»), и только
        // потом уводит в меню паузы. Порядок именно такой, потому что ESC
        // читается как «назад на шаг»: из панели — в редактор, из редактора — в
        // меню. Вопрос задаётся каркасу, а не списку имён панелей: список
        // пришлось бы дописывать при каждой новой панели и однажды не дописать.
        //
        // СНАЧАЛА НАСТРОЙКИ ИНСТРУМЕНТА, потом всё остальное — потому что это
        // то окно, которое человек только что открыл треугольником.
        // ИНСТРУМЕНТ ПРИ ЭТОМ ОСТАЁТСЯ В РУКЕ: ESC закрывает окно, а не
        // отбирает инструмент — отбирает его щелчок по его же иконке.
        return;
    }
    if (mode_ == AppMode::Editor) {
        // ТРЕТИЙ ШАГ НАЗАД — БРОСИТЬ НАБРАННОЕ. «Когда я рисую стену и выбираю
        // точки или рисую прямые, хочу, чтобы на esc я сбрасывал выбранные
        // точки» (18.08). Порядок важен и назван человеком же: если открыто
        // меню — закрывается меню, а точки остаются; закрывать и то и другое
        // одним нажатием и есть та «казусная ситуация», которой он опасался.
        if (IEditorTool* tool = editor_ui_.toolbox().active();
            tool != nullptr && tool->has_draft()) {
            tool->on_cancel(editor_ui_.tool_world());
            std::fprintf(stderr, "[постройка] набранное сброшено\n");
            return;
        }
    }
    paused_from_ = mode_; // Resume returns here (Playing or Editor)
    // The editor rows exist only while editing: a row that cannot do anything
    // teaches the player that the menu lies.
    menu_.set_editing(mode_ == AppMode::Editor);
    menu_.open(MenuPage::Pause);
    mode_ = AppMode::Menu;
    input_->set_cursor_captured(false);
}

// TAB TOGGLES THE BODY (user В39/Л1: "and the fly-over, and out of the eyes, in
// the same field"). From the editor it possesses the player at the free camera;
// from Playing it lifts back out into the free camera at the current eye. A
// no-op in any other mode by construction.
void App::on_toggle_body() {
    if (mode_ == AppMode::Editor) {
        become_player_from_editor();
        input_->set_cursor_captured(!unattended_run());
    } else if (mode_ == AppMode::Playing) {
        enter_editor_mode();
        input_->set_cursor_captured(!unattended_run());
    }
}

// THIRD PERSON (key 1). No mode test here: the row is PlayingOnly and
// action_pressed() obeys the scope, so a second guard would be a second
// definition of where this key applies. The DFN_THIRD_PERSON door calls this
// same method, which is what keeps the door from photographing a path no user
// takes.
void App::on_third_person() {
    third_person_ = !third_person_;
    orbit_yaw_ = 0.0f;
    orbit_pitch_ = 0.0f;
    // THE HEAD COMES BACK IN THIRD PERSON. It is hidden in first person because
    // the camera sits inside the skull; from behind, a headless body is the
    // first thing he would report, and it would read as a missing mesh rather
    // than as a deliberate first-person choice.
    if (auto* rig = world_.get<anim::BodyRig>(player_)) {
        rig->hide_head = !third_person_;
        const auto head = rig->segments[anim::bone_index(anim::Bone::Head)];
        if (auto* rm = world_.get<components::RenderMesh>(head)) {
            rm->mesh_asset =
                third_person_ ? anim::body_segment_mesh_id(anim::Bone::Head) : 0u;
        }
    }
}

void App::on_debug_readout() { debug_overlay_ = !debug_overlay_; }

// The capture is deferred to AFTER render() so the .png and the sidecar
// describe the same frame; capturing here would save the state of frame N next
// to the image of frame N-1.
void App::on_state_capture() { capture_pending_ = true; }

// WIREFRAME (В28), key 4 / F4. A whole-scene toggle straight to the backend;
// works in both modes but is aimed at the editor's "why is this object so
// heavy" question. set_wireframe is a no-op cost when off.
void App::on_wireframe() {
    wireframe_ = !wireframe_;
    renderer_->set_wireframe(wireframe_);
}

// R — РЕЖИМ УКАЗАТЕЛЯ, «почти как в vim» (заказ 18.08). Состояние живёт в ящике
// инструментов, а не здесь: клавиша — часть контракта инструментов (в режиме
// указателя щелчок принадлежит интерфейсу, а не миру), и вторая копия флага в
// App разъехалась бы с первой.
//
// НИ ОТ ЧЕГО НЕ ЗАВИСИТ. Ни от открытых окон, ни от того, что в руке: клавиша,
// которая иногда не срабатывает, читается как сломанная.
void App::on_cursor_toggle() {
    editor_ui_.toolbox().toggle_pointer_mode();
    std::fprintf(stderr, "[editor] курсор: %s\n",
                 editor_ui_.toolbox().pointer_mode() ? "мышь (указываю)"
                                                     : "камера (смотрю)");
}

// ПЯТЬ КЛАВИШ — ПЯТЬ ИНСТРУМЕНТОВ, ПО ПОРЯДКУ ПОЛОСЫ. Ни одного имени
// инструмента здесь нет: клавиша называет НОМЕР, а какой это инструмент, знает
// ящик. И ЩЕЛЧОК ПО КЛАВИШЕ УЖЕ ВЫБРАННОГО ИНСТРУМЕНТА КЛАДЁТ ЕГО, как и щелчок
// по его иконке: один глагол, два способа его произнести.
void App::on_undo_redo() {
    // ОДИН ОБРАБОТЧИК НА ДВА ДЕЙСТВИЯ, потому что различает их МОДИФИКАТОР, а
    // не клавиша. Cmd обязателен: голая Z в редакторе — это буква, и если
    // однажды появится поле ввода, отмена начала бы срабатывать при наборе.
    const bool cmd = input_->is_down(platform::Key::LEFT_SUPER)
                  || input_->is_down(platform::Key::RIGHT_SUPER);
    if (!cmd) {
        return;
    }
    const bool shift = input_->is_down(platform::Key::LEFT_SHIFT)
                    || input_->is_down(platform::Key::RIGHT_SHIFT);
    // ОТМЕНЯТЬ НЕЧЕГО — ЭТО ГОВОРИТСЯ ВСЛУХ: молчащая отмена неотличима от
    // сломанной, а этот сорт молчания мы 18.08 разбирали трижды.
    const std::string state = shift ? history_.redo() : history_.undo();
    if (state.empty()) {
        std::fprintf(stderr, "[история] %s нечего\n",
                     shift ? "повторять" : "отменять");
        return;
    }
    std::fprintf(stderr, "[история] %s: %s\n", shift ? "повтор" : "отмена",
                 shift ? history_.undo_label().c_str() : history_.redo_label().c_str());
    // СНИМОК ПРИМЕНЯЕТ ТОТ, У КОГО ЕСТЬ МОДЕЛЬ. История про модель не знает и
    // знать не должна (иначе её нельзя было бы проверить без мира), поэтому
    // read_house зовётся здесь — в ЕДИНСТВЕННОМ месте, куда приходит снимок.
    if (!house_.apply_snapshot(state)) {
        std::fprintf(stderr, "[история] снимок не читается — постройка НЕ восстановлена\n");
    }
}

void App::on_axis_lock() {
    // ОСЬ КРУТИТСЯ ВОКРУГ ВЫБРАННОГО ЯКОРЯ: список того, вдоль чего можно
    // двигаться, — это прямые, приходящие ИМЕННО В НЕГО. Без выбранного якоря
    // остаются свободно и вертикаль, и это законное состояние (человек ещё
    // ничего не выбрал, а стойку вверх вести уже хочет).
    house_.cycle_axis(house_.selected_vertex());
    // СКАЗАНО ВСЛУХ, потому что фиксация оси — состояние без своей картинки на
    // кадре до первого движения мыши: молчащее переключение неотличимо от
    // непрочитанной клавиши. Подпись внизу экрана говорит то же самое, но
    // журнал остаётся и после того, как подпись сменится.
    std::fprintf(stderr, "[постройка] ось: %s\n", house_.axis_label().c_str());
}

void App::on_delete_selected() {
    const std::string why = house_.delete_selection();
    // ОТКАЗ ГОВОРИТСЯ ВСЛУХ И СО СПИСКОМ ДЕРЖАТЕЛЕЙ: молча не сработавшая
    // клавиша неотличима от сломанной, а этот сорт молчания мы разбирали
    // сегодня трижды.
    std::fprintf(stderr, "[постройка] удаление: %s\n", why.empty() ? "готово" : why.c_str());
}

void App::on_tool_pick(int index) {
    // CMD+SHIFT+ЦИФРА ОТКРЫВАЕТ МЕНЮ ЭТОГО ИНСТРУМЕНТА, не трогая руку (заказ
    // 18.08). Это то же самое, что треугольник под его фишкой, — и потому зовёт
    // тот же метод: две двери в одну комнату, а не две комнаты.
    //
    // Различает их УСЛОВИЕ, а не вторая строка в таблице: цифра одна, и
    // отдавать её двум действиям в одной области видимости запрещено проверкой.
    const bool cmd = input_->is_down(platform::Key::LEFT_SUPER)
                  || input_->is_down(platform::Key::RIGHT_SUPER);
    const bool shift = input_->is_down(platform::Key::LEFT_SHIFT)
                    || input_->is_down(platform::Key::RIGHT_SHIFT);
    if (cmd && shift) {
        wire_editor_panels();
        editor_ui_.toolbox().click_settings(static_cast<std::size_t>(index));
        return;
    }
    editor_ui_.toolbox().click_icon(static_cast<std::size_t>(index),
                                    editor_ui_.tool_world());
}

// B — СПИСОК ОБЪЕКТОВ, и это ровно то же, что треугольник под кнопкой
// постройки: одна дверь, две руки. Клавиша НЕ берёт инструмент в руку — «я не
// выбирал этот инструмент только настроил». Дверь DFN_EDITOR_PARTS зовёт этот
// же метод.
void App::on_build_menu() {
    wire_editor_panels();
    if (const std::size_t i = editor_ui_.toolbox().index_of("place"); i != NO_TOOL) {
        editor_ui_.toolbox().click_settings(i);
    }
    if (build_groups_.empty()) { // подстраховка: полка пуста
        build_groups_ = build_palette(gallery_objects_dir_);
        std::fprintf(stderr, "[build] палитра: %zu семейств(а) с полок %s\n",
                     build_groups_.size(), gallery_objects_dir_.c_str());
    }
}

// G ставит деталь прямо. After a few fine steps "back to zero" by arrow is
// arithmetic the builder should not have to do. Asked OF THE TOOL, never of a
// mode flag: only the hand that turns parts may be un-turned.
/// ШАГ СТРЕЛКАМИ ПО ВЫБРАННОМУ ЯКОРЮ. true — что-то сдвинули.
///
/// НАПРАВЛЕНИЯ ОТ КАМЕРЫ, А НЕ ОТ МИРА: «вправо» значит вправо НА ЭКРАНЕ, иначе
/// человеку пришлось бы держать в голове, куда сейчас смотрит север. Вверх и
/// вниз — по мировой вертикали: единственное направление, которое от взгляда не
/// зависит и всегда значит одно и то же.
void App::on_grid_toggle() {
    house_.set_grid_on(!house_.grid_on());
    std::fprintf(stderr, "[постройка] сетка %s, шаг %.2f м\n",
                 house_.grid_on() ? "включена" : "выключена",
                 static_cast<double>(house_.grid_step_m()));
}

void App::draw_editor_grid(const ToolAim& aim) {
    if (!house_.grid_on() || renderer_ == nullptr || !aim.hit) {
        return;
    }
    // ОТСЕЧКИ ТОЛЬКО ВОКРУГ ПРИЦЕЛА, а не по всему миру — прямое требование:
    // «сетка везде глаза зальёт». Пятно узлов идёт за прицелом и живёт в
    // МИРОВЫХ координатах: узел там, где координата кратна шагу, и он не
    // сдвинется оттого, что человек отошёл.
    const float step = house_.grid_step_m();
    constexpr int HALF = 8; // узлов в каждую сторону от прицела
    const float cx = std::round(aim.point.x / step) * step;
    const float cz = std::round(aim.point.z / step) * step;
    // ПЕРЕКРЕСТИЯ, А НЕ СПЛОШНЫЕ ЛИНИИ. Сплошная сетка на траве читается как
    // рябь и прячет саму траву; короткие крестики в узлах говорят то же самое
    // («вот куда прилипнет»), занимая на порядок меньше пикселей.
    const float tick = std::min(step * 0.18f, 0.25f);
    constexpr std::uint32_t COL = 0xFF60D8F0u;
    for (int iz = -HALF; iz <= HALF; ++iz) {
        for (int ix = -HALF; ix <= HALF; ++ix) {
            const float x = cx + static_cast<float>(ix) * step;
            const float z = cz + static_cast<float>(iz) * step;
            const float y = chunks_.height_at({x, z}).value_or(aim.point.y) + 0.03f;
            renderer_->debug_line({x - tick, y, z}, {x + tick, y, z}, COL);
            renderer_->debug_line({x, y, z - tick}, {x, y, z + tick}, COL);
        }
    }
    renderer_->set_debug_lines(true);
}

bool App::nudge_selected_anchor() {
    const float step = house_.grid_step_m();
    const float yaw = editor_cam_.yaw();
    const glm::vec3 fwd{std::sin(yaw), 0.0f, -std::cos(yaw)};
    const glm::vec3 right{std::cos(yaw), 0.0f, std::sin(yaw)};
    glm::vec3 by{0.0f};
    const bool shift = input_->is_down(platform::Key::LEFT_SHIFT)
                    || input_->is_down(platform::Key::RIGHT_SHIFT);
    if (input_->was_pressed(platform::Key::RIGHT)) { by += right * step; }
    if (input_->was_pressed(platform::Key::LEFT)) { by -= right * step; }
    // SHIFT+ВВЕРХ/ВНИЗ — ПО ВЫСОТЕ, без него — вперёд/назад по земле. Одна пара
    // клавиш на два направления: третьей пары стрелок на клавиатуре нет.
    if (input_->was_pressed(platform::Key::UP)) {
        by += shift ? glm::vec3{0.0f, step, 0.0f} : fwd * step;
    }
    if (input_->was_pressed(platform::Key::DOWN)) {
        by -= shift ? glm::vec3{0.0f, step, 0.0f} : fwd * step;
    }
    if (glm::length(by) < 1e-6f) {
        return false;
    }
    const world::VertexId id = house_.selected_vertex();
    // В УЗЕЛ, А НЕ НА ДЕЛЬТУ (заказ 18.08: «объекты при движении по сетке
    // должны не на дельту перемещаться, а чётко в координатах сетки жить, типа
    // 100 101 102 с шагом 1»). Разница видна на второй же нажатой стрелке:
    // якорь, стоявший в 100.37, от дельты уехал бы в 101.37 и остался бы кривым
    // навсегда. Сложение с шагом даёт направление, округление — само место.
    const glm::vec3 to = house_.snap_to_grid(house_.vertex_world(id) + by);
    (void)house_.mutate("сдвинул якорь стрелкой", [&](world::HouseGraph& g) {
        return g.move_vertex(id, house_.to_local(to));
    });
    std::fprintf(stderr, "[постройка] якорь v%u -> (%.2f %.2f %.2f), шаг %.2f м\n",
                 static_cast<unsigned>(id), static_cast<double>(to.x),
                 static_cast<double>(to.y), static_cast<double>(to.z),
                 static_cast<double>(step));
    return true;
}

void App::on_build_rotate() {
    const IEditorTool* held = editor_ui_.toolbox().active();
    if (held != nullptr && held->wants_part_rotation()) {
        build_yaw_ = 0.0f;
    }
}

// СТРЕЛКИ КРУТЯТ ДЕТАЛЬ, И СПРАШИВАЕТСЯ ЭТО У ИНСТРУМЕНТА. Здесь стояло «выбран
// режим постановки ИЛИ открыт список объектов» — то самое условие с ДВУМЯ
// хозяевами, из-за которого один щелчок и копал, и ставил. Теперь вопрос один и
// адресован тому, кто на него отвечает: wants_part_rotation().
void App::update_part_rotation() {
    if (mode_ != AppMode::Editor) {
        return;
    }
    // СТРЕЛКИ ДВИГАЮТ ВЫБРАННЫЙ ЯКОРЬ ШАГОМ СЕТКИ (заказ 18.08: «на стрелочки я
    // должен дискретно двигать объект, камера к нему должна быть прилиплена, то
    // есть фокус не теряется»). Фокус здесь и не может потеряться: двигается
    // ВЫБРАННОЕ, а выбор живёт в сессии и от камеры не зависит вовсе.
    //
    // ПОРЯДОК ХОЗЯЕВ: якорь перед деталью. Крутить деталь и двигать якорь одной
    // клавишей нельзя, а выбранный якорь — состояние, которое человек назначил
    // сам и видит на экране; деталь в руке он видит там же, но выбор якоря
    // адреснее.
    if (house_.selected_vertex() != world::NO_VERTEX && nudge_selected_anchor()) {
        return;
    }
    const IEditorTool* held = editor_ui_.toolbox().active();
    if (held == nullptr || !held->wants_part_rotation()) {
        return;
    }
    // THE ARROWS TURN THE PART (user, 17.08: «стрелками я должен не объекты
    // перебирать, а крутить их вокруг их центра»). Left/right is the QUARTER
    // TURN the kit is built on — a square joint hands out exactly four
    // directions. Up/down is the fine step, because a ROUND joint hands out any
    // angle, and that is what makes a house a polygon instead of a box.
    const auto turn = [this](float by) {
        build_yaw_ += by;
        while (build_yaw_ >= glm::two_pi<float>()) {
            build_yaw_ -= glm::two_pi<float>();
        }
        while (build_yaw_ < 0.0f) {
            build_yaw_ += glm::two_pi<float>();
        }
    };
    constexpr float FINE_STEP = glm::pi<float>() / 12.0f; // 15 degrees
    if (input_->was_pressed(platform::Key::RIGHT)) {
        turn(glm::half_pi<float>());
    }
    if (input_->was_pressed(platform::Key::LEFT)) {
        turn(-glm::half_pi<float>());
    }
    if (input_->was_pressed(platform::Key::UP)) {
        turn(FINE_STEP);
    }
    if (input_->was_pressed(platform::Key::DOWN)) {
        turn(-FINE_STEP);
    }
    // DELETE IS A KEY, NOT THE OTHER MOUSE BUTTON. Removing is the one action
    // here that cannot be undone yet, and putting it under a button the hand is
    // already resting on would make it the easiest thing in the tool to do by
    // accident.
    if (input_->was_pressed(platform::Key::DELETE) && build_delete()) {
        std::fprintf(stderr, "[build] удалено; в композиции %zu расстановок\n",
                     scene_doc_.placements.size());
    }
}

// SCREENSHOT (key 5, the user's request: "я хочу чтобы был скриншот... по
// нажатию кнопки 5... он должен к чату добавляться и трейсам"). It is the
// FRAMEBUFFER as presented -- overlays and all, since the HUD is composited
// into it -- not an OS screen grab, and it lands in three places: a .png beside
// its state sidecar, a line in the map's chat carrying "capture", and a
// landmark row in the telemetry trace.
//
// IT ROUTES THROUGH THE EXISTING PATH ON PURPOSE (Rule 32). The Enter remark
// already wrote a frame and attached it to the chat; a second screenshot
// pipeline beside it would be two things to keep correct and two places for the
// file naming to drift.
void App::on_screenshot() {
    if (mode_ != AppMode::Playing && mode_ != AppMode::Editor) {
        return;
    }
    chat_pending_entry_ = ChatEntry{};
    chat_pending_entry_.who = "human";
    chat_pending_ = true;
}

// QUICK CHAT SNAPSHOT (Enter, window CLOSED). Enter drops the current frame's
// capture into the active map's chat as a human remark with no text -- a one-key
// "look at this" that the player can annotate in the file, or send with text by
// opening the window ('/') and typing (which is service_chat_typing, where
// Enter SENDS). The gate column is what keeps the two Enter roles apart.
void App::on_quick_remark() {
    if (mode_ != AppMode::Playing && mode_ != AppMode::Editor) {
        return;
    }
    // ENTER СНАЧАЛА ПОДТВЕРЖДАЕТ ЧЕРНОВИК, И ТОЛЬКО ПОТОМ ОТКРЫВАЕТ ЗАМЕТКУ.
    //
    // Клавиша одна, действий два — различает их УСЛОВИЕ, а не вторая строка в
    // таблице: ровно так же Shift различает отмену и повтор на Z. Второй строки
    // и быть не может, Enter занят заметкой ВЕЗДЕ, а два действия на одной
    // клавише в пересекающихся областях запрещены проверкой — и правильно.
    //
    // Порядок именно такой: черновик поверхности живёт секунды и ждёт ответа,
    // заметку человек может открыть и следующим нажатием. Инструмент без
    // черновика отвечает has_draft() == false и клавишу не отнимает.
    if (mode_ == AppMode::Editor) {
        IEditorTool* tool = editor_ui_.toolbox().active();
        if (tool != nullptr && tool->has_draft()) {
            tool->on_confirm(editor_ui_.tool_world());
            return;
        }
    }
    chat_pending_entry_ = ChatEntry{};
    chat_pending_entry_.who = "human";
    chat_pending_ = true;
}

// TRAJECTORY RECORD (O3), editor tooling (В39: full set in the editor). K
// starts/stops recording the walk -- on stop it writes a .dftraj and remembers
// it. The deterministic, bit-for-bit-checkable path is the DFN_TRAJ_REC door
// (Rule 27); this key is the human's version.
void App::on_trajectory_record() {
    if (traj_rec_.active()) {
        char stem[64];
        std::snprintf(stem, sizeof(stem), "/trajectory_%03d.dftraj", traj_written_);
        const std::string w = traj_rec_.stop_and_write(capture_dir_ + stem);
        if (!w.empty()) {
            traj_last_path_ = w;
            ++traj_written_;
        }
    } else {
        traj_rec_.begin(active_stand_, 1u);
    }
}

// P replays the last recording made this session.
void App::on_trajectory_replay() {
    if (traj_last_path_.empty()) {
        return;
    }
    TrajectoryPlayer pl;
    if (pl.load(traj_last_path_)) {
        traj_play_then_close_ = false; // interactive replay just stops
        traj_play_ = std::move(pl);
    }
}

void App::on_map() {
    render_system_.toggle_map();
    // Free the cursor while the map is up: mouse-look under a fullscreen plate
    // spins the world behind it for no reason.
    input_->set_cursor_captured(!render_system_.map_open() && !unattended_run());
}

} // namespace dfn::app
