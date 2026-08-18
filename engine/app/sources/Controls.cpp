/*
Created: 14:08:2026 - 19:22:10
Last updated: 18:08:2026 - 23:52:10
Module: engine/app
File: engine/app/sources/Controls.cpp

Responsibility:
- The binding table itself. See the header for why it is a table.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 14:08:2026 - 19:22:10: Создан вместе с заголовком — таблица привязок, из которой
  и диспатчатся клавиши, и рисуется экран управления.
- 17:08:2026 - 16:27:55: F11 в таблице и в key_name (тест поймал отсутствие подписи).
- 17:08:2026 - 16:59:23: порядок жертв: подписи -> подвал -> вторая колонка. Строки не выбрасываются никогда.
- 17:08:2026 - 22:32:14: Пять строк режимов редактора на 1..5 и разводка цифр с F-клавишами
  через alias_scope (довод — в шапке Controls.h). Снимок получил алиас F5,
  которого у него не было: иначе клавиша 5 в редакторе отняла бы его совсем.
- 18:08:2026 - 16:28:24: строка «дальность взаимодействия — колесо»; у скорости полёта теперь [ и ].
- 18:08:2026 - 17:16:29: строка отмены и подпись клавиши Z (без подписи строка рисуется как «?»).
- 18:08:2026 - 18:58:40: V — фиксация оси (не Z: Z занята отменой), и подпись клавиши V, без которой экран управления рисовал «?».
- 18:08:2026 - 20:26:30: Delete и Backspace убирают выбранное; подписи обеих клавиш.
- 18:08:2026 - 23:20:00: Режим на `, Tab отдан интерфейсу; цифры 6..9 и их подписи.
- 18:08:2026 - 23:52:10: Режим на ` (Tab отдан интерфейсу), X — сетка; подписи ` и X.
*/

#include "engine/app/sources/Controls.h"

#include "engine/render/sources/BitmapFont.h"

#include <algorithm>
#include <array>

namespace dfn::app {
namespace {

using K = platform::Key;

// THE TABLE. Order is Action's order, and the test holds it to that: an
// out-of-order row would make binding_for() return someone else's key, which
// is the one bug in here that would look completely normal on screen.
//
// The order the ROWS are in is also the order the screen draws, so it is
// grouped the way the user learns them: what you look at, then what you record,
// then where you are, then how you leave.
constexpr std::array<Binding, static_cast<size_t>(Action::Count)> TABLE{{
    // ЦИФРЫ ПРИНАДЛЕЖАТ РЕДАКТОРУ, F-КЛАВИШИ — ВСЕМ. Пользователь занял 1..5
    // под пять режимов редактора (17.08), а эти четыре действия жили на тех же
    // цифрах. Разводка: строка судится по ТЕЛУ, алиас — везде, поэтому F3 в
    // редакторе по-прежнему включает вывод, а 2 в редакторе выбирает кисть
    // поверхности. Снимку добавлен F5: он был единственным из пяти без алиаса,
    // и без него клавиша 5 в редакторе отняла бы у человека снимок.
    {Action::ThirdPerson, K::NUM_1, K::UNKNOWN, "controls.third_person", Scope::PlayingOnly},
    {Action::DebugReadout, K::NUM_2, K::F3, "controls.debug_readout", Scope::PlayingOnly,
     Scope::Anywhere},
    {Action::StateCapture, K::NUM_3, K::F2, "controls.state_capture", Scope::PlayingOnly,
     Scope::Anywhere},
    {Action::Wireframe, K::NUM_4, K::F4, "controls.wireframe", Scope::PlayingOnly,
     Scope::Anywhere},
    {Action::Screenshot, K::NUM_5, K::F5, "controls.screenshot", Scope::PlayingOnly,
     Scope::Anywhere},
    // РЕЖИМ НА ` (заказ 18.08): Tab отдан ИНТЕРФЕЙСУ — им переходят между полями
    // и кнопками в панели инструмента, а клавиша, делающая два дела сразу,
    // делает второе неожиданно.
    {Action::ToggleBody, K::GRAVE, K::UNKNOWN, "controls.toggle_body", Scope::Anywhere},
    {Action::TrajectoryRecord, K::K, K::UNKNOWN, "controls.traj_record", Scope::EditorOnly},
    {Action::TrajectoryReplay, K::P, K::UNKNOWN, "controls.traj_replay", Scope::EditorOnly},
    {Action::ChatWindow, K::SLASH, K::UNKNOWN, "controls.chat", Scope::Anywhere},
    {Action::QuickRemark, K::ENTER, K::UNKNOWN, "controls.quick_remark", Scope::Anywhere},
    {Action::Map, K::M, K::UNKNOWN, "controls.map", Scope::Anywhere},
    {Action::MenuPause, K::ESCAPE, K::UNKNOWN, "controls.menu", Scope::Anywhere},
    {Action::Fullscreen, K::F11, K::UNKNOWN, "controls.fullscreen", Scope::Anywhere},
    {Action::CursorToggle, K::R, K::UNKNOWN, "controls.cursor", Scope::Anywhere},
    {Action::BuildMenu, K::B, K::UNKNOWN, "controls.build_menu", Scope::EditorOnly},
    {Action::BuildRotate, K::G, K::UNKNOWN, "controls.build_rotate", Scope::EditorOnly},
    {Action::ToolHeight, K::NUM_1, K::UNKNOWN, "controls.tool_height", Scope::EditorOnly},
    {Action::ToolPaint, K::NUM_2, K::UNKNOWN, "controls.tool_paint", Scope::EditorOnly},
    {Action::ToolSelect, K::NUM_3, K::UNKNOWN, "controls.tool_select", Scope::EditorOnly},
    {Action::ToolPlace, K::NUM_4, K::UNKNOWN, "controls.tool_place", Scope::EditorOnly},
    {Action::ToolLook, K::NUM_5, K::UNKNOWN, "controls.tool_look", Scope::EditorOnly},
    {Action::Tool6, K::NUM_6, K::UNKNOWN, "controls.tool_6", Scope::EditorOnly},
    {Action::Tool7, K::NUM_7, K::UNKNOWN, "controls.tool_7", Scope::EditorOnly},
    {Action::Tool8, K::NUM_8, K::UNKNOWN, "controls.tool_8", Scope::EditorOnly},
    {Action::Tool9, K::NUM_9, K::UNKNOWN, "controls.tool_9", Scope::EditorOnly},
    // Z одна на оба: отмену от повтора отличает Shift, и это УСЛОВИЕ, а не
    // вторая клавиша. Область — редактор: отменять в игре нечего.
    {Action::Undo, K::Z, K::UNKNOWN, "controls.undo", Scope::EditorOnly},
    // V — «вертикаль». Не Z, как в Blender: Z здесь уже занята отменой, а два
    // действия на одной клавише в одной области видимости запрещены — и
    // правильно запрещены. Область — редактор: строить в теле нечем.
    {Action::AxisLock, K::V, K::UNKNOWN, "controls.axis_lock", Scope::EditorOnly},
    // Delete и Backspace — обе, потому что на этой клавиатуре первой нет: на
    // ноутбуке Apple физическая клавиша одна и зовётся Backspace.
    {Action::DeleteSelected, K::DELETE, K::BACKSPACE, "controls.delete_selected",
     Scope::EditorOnly, Scope::EditorOnly},
    // X — сетка. Не G (она уже ставит деталь прямо) и не Q, которая только что
    // освободилась от спуска: рука помнит Q как «вниз», и сетка, включающаяся
    // от старой привычки, читалась бы как сбой.
    {Action::GridToggle, K::X, K::UNKNOWN, "controls.grid", Scope::EditorOnly},
}};

// THE FLY CAMERA'S CONTINUOUS INPUTS, described rather than dispatched.
// READ OFF EditorCamera, NOT off the request that asked for this screen. The
// task described the fly controls as "WASD+QE, Space/Ctrl", which reads as two
// separate pairs; EditorCamera.h says what the code does -- E/Space are BOTH
// up and Q/Ctrl are BOTH down. A help screen copied from the description
// instead of the source is the exact failure this file exists to prevent, so
// it would have been an unusually poor place to introduce one.
constexpr std::array<MovementRow, 6> MOVEMENT{{
    {"controls.fly.move.keys", "controls.fly.move"},
    {"controls.fly.up.keys", "controls.fly.up"},
    {"controls.fly.down.keys", "controls.fly.down"},
    {"controls.fly.look.keys", "controls.fly.look"},
    {"controls.fly.speed.keys", "controls.fly.speed"},
    {"controls.reach.keys", "controls.reach"},
}};

} // namespace

std::span<const Binding> control_bindings() { return TABLE; }

const Binding& binding_for(Action action) {
    // The caller passing Action::Count would be a programming error, not user
    // input, so it is clamped rather than reported: returning a valid row keeps
    // a mis-call from dispatching a WILD key, which is the worse outcome.
    const size_t i = static_cast<size_t>(action);
    return TABLE[i < TABLE.size() ? i : 0];
}

std::span<const MovementRow> movement_rows() { return MOVEMENT; }

ControlsLayout controls_layout(int width_px, int height_px) {
    (void)width_px; // the rows are two columns; only the HEIGHT is contended
    const int rows = static_cast<int>(TABLE.size() + MOVEMENT.size());

    ControlsLayout L;
    L.title_y = height_px / 12;

    // Tried in order, most generous first, taking the first arrangement whose
    // block ends above the bottom edge. Written as a loop over the two things
    // that may be given up rather than as nested ifs: the order of sacrifice is
    // then a list one can read and reorder, not a shape one has to infer.
    // ORDER OF SACRIFICE, читается сверху вниз: сначала отдаём подписи разделов,
    // потом подвал, и только потом ЛОМАЕМ СПИСОК НА ДВЕ КОЛОНКИ. Колонка идёт
    // последней, потому что она меняет форму страницы, а первые две — только
    // её украшения; но она идёт РАНЬШЕ, чем «строки налезут друг на друга»,
    // потому что нечитаемый список хуже непривычного.
    // В две колонки подписи разделов не идут: они помечают ГРАНИЦУ между
    // клавишами и полётом, а разрез пополам эту границу и так рвёт — подпись
    // над правой колонкой врала бы.
    const bool wants[5][3] = {{true, true, false}, {true, false, false},
                              {false, false, false},
                              {false, true, true}, {false, false, true}};
    for (const auto& want : wants) {
        L.headings = want[0];
        L.footer = want[1];
        L.columns = want[2] ? 2 : 1;
        const int drawn_rows =
            L.columns == 2 ? (rows + 1) / 2 : rows;
        L.rows_per_column = L.columns == 2 ? drawn_rows : 0;
        L.line_count = drawn_rows + (L.headings ? 2 : 0);
        L.first_y = L.title_y + render::FONT_CELL_H + (L.headings ? 6 : 4);
        const int floor_y =
            height_px - (L.footer ? render::FONT_CELL_H * 2 + 4 : 2);
        const int room = floor_y - L.first_y;
        // The pitch never goes below the glyph height: rows that overlap are
        // not a denser list, they are an unreadable one.
        L.row_h = std::max(render::FONT_INK_H,
                           std::min(render::FONT_CELL_H + 1,
                                    room / std::max(L.line_count, 1)));
        L.bottom = L.first_y + L.line_count * L.row_h;
        L.fits = L.bottom <= floor_y;
        if (L.fits) {
            break;
        }
    }
    return L;
}

const char* key_name(platform::Key key) {
    switch (key) {
    case K::NUM_1: return "1";
    case K::NUM_2: return "2";
    case K::NUM_3: return "3";
    case K::NUM_4: return "4";
    case K::NUM_5: return "5";
    case K::NUM_6: return "6";
    case K::NUM_7: return "7";
    case K::NUM_8: return "8";
    case K::NUM_9: return "9";
    case K::F2: return "F2";
    case K::F3: return "F3";
    case K::F4: return "F4";
    case K::F5: return "F5";
    case K::F11: return "F11";
    case K::TAB: return "Tab";
    case K::ENTER: return "Enter";
    case K::ESCAPE: return "Esc";
    case K::SLASH: return "/";
    case K::M: return "M";
    case K::P: return "P";
    case K::R: return "R";
    case K::B: return "B";
    case K::K: return "K";
    case K::G: return "G";
    // Z — отмена. Подпись обязана существовать: «?» на экране управления это
    // не косметика, а признак строки, о которой человеку никто не скажет.
    case K::Z: return "Z";
    case K::V: return "V";  // вертикаль — фиксация оси у прямой
    case K::X: return "X";
    case K::GRAVE: return "`";
    case K::DELETE: return "Del";
    case K::BACKSPACE: return "Backspace";
    default: return "?"; // loud, not blank: a nameless key is a table bug
    }
}

} // namespace dfn::app
