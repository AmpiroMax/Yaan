/*
Module: engine/app
File: engine/app/sources/Controls.cpp

Responsibility:
- The binding table itself. See the header for why it is a table.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
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
    // T — ОРУЖИЕ. Область PlayingOnly, поэтому строка не спорит ни с одной
    // редакторской буквой; см. довод при Action::WeaponToggle.
    {Action::WeaponToggle, K::T, K::UNKNOWN, "controls.weapon", Scope::PlayingOnly},
    {Action::TrajectoryRecord, K::K, K::UNKNOWN, "controls.traj_record", Scope::EditorOnly},
    {Action::TrajectoryReplay, K::P, K::UNKNOWN, "controls.traj_replay", Scope::EditorOnly},
    {Action::ChatWindow, K::SLASH, K::UNKNOWN, "controls.chat", Scope::Anywhere},
    {Action::QuickRemark, K::ENTER, K::UNKNOWN, "controls.quick_remark", Scope::Anywhere},
    {Action::Map, K::M, K::UNKNOWN, "controls.map", Scope::Anywhere},
    {Action::MenuPause, K::ESCAPE, K::UNKNOWN, "controls.menu", Scope::Anywhere},
    {Action::Fullscreen, K::F11, K::UNKNOWN, "controls.fullscreen", Scope::Anywhere},
    // СУЖЕНА ДО РЕДАКТОРА (01.09) — ПОЧИНКА ПОДПИСИ, А НЕ РАЗМЕН КЛАВИШИ.
    // Строка объявляла себя Anywhere, а её тело (App::on_cursor_toggle) трогает
    // только ящик инструментов редактора и печатает «[editor] курсор»: в игре
    // нажатие R не делало ничего, а экран управления обещал, что делает. Теперь
    // область говорит то же, что и тело, и освободившаяся в игре R досталась
    // смотровой — но досталась она ей ПОТОМУ, что была свободна, а не наоборот.
    {Action::CursorToggle, K::R, K::UNKNOWN, "controls.cursor", Scope::EditorOnly},
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
    // СКОБКИ — ПОЗЫ, ОДНОЙ СТРОКОЙ НА ОБЕ. Обе свободны во всей таблице, стоят
    // рядом на клавиатуре и читаются как «назад/вперёд по списку» — та же пара,
    // которой листают кисть в редакторах изображений. Почему одна строка, а не
    // две, сказано при Action::PoseCycle: вторая не помещается на экран
    // управления при 320x180. Область — тело: в редакторе позировать некому.
    {Action::PoseCycle, K::RIGHT_BRACKET, K::LEFT_BRACKET, "controls.pose_cycle",
     Scope::PlayingOnly, Scope::PlayingOnly},
    // СТРЕЛКИ — СМОТРОВАЯ. Свободны во всей таблице, и это та пара, которой
    // листают что угодно. Область — тело: смотровая это игровой режим, а не
    // редакторский, и в редакторе стрелки принадлежат меню объектов.
    {Action::ViewerCycle, K::RIGHT, K::LEFT, "controls.viewer_cycle",
     Scope::PlayingOnly, Scope::PlayingOnly},
    // E/Q — ПОВОРОТ МОДЕЛИ. Обе заняты у ЖИВОГО тела (E — взаимодействие,
    // Q — бросить предмет), и это законно ровно потому, что ни того, ни другого
    // на смотровой нет: там нет ни одного предмета в руках и ни одной точки
    // взаимодействия. Обработчик всё равно проверяет, что смотровая открыта, —
    // иначе на боевой карте поворот молча тратил бы нажатие «взять».
    {Action::ViewerTurn, K::E, K::Q, "controls.viewer_turn",
     Scope::PlayingOnly, Scope::PlayingOnly},
    {Action::ViewerReset, K::R, K::UNKNOWN, "controls.viewer_reset",
     Scope::PlayingOnly},
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

ControlsLayout controls_layout(int width_px, int height_px, int min_pitch, int line_h) {
    (void)width_px; // the rows are two columns; only the HEIGHT is contended
    const int rows = static_cast<int>(TABLE.size() + MOVEMENT.size());
    // Мера шрифта: своя, если её передали, иначе прежняя блочная.
    const int ink = min_pitch > 0 ? min_pitch : render::FONT_INK_H;
    const int cell = line_h > 0 ? line_h : render::FONT_CELL_H;

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
    //
    // ...И ПОСЛЕДНЕЙ ЖЕРТВОЙ — ТРЕТЬЯ КОЛОНКА. Она появилась не «на вырост»:
    // двадцать восьмая строка таблицы (перебор поз) съехала с нижнего края при
    // 320x180 на ПОСЛЕДНЕЙ прежней раскладке — две колонки без подписей и без
    // подвала, — то есть запас кончился по-настоящему. Третья колонка стоит
    // после второй по той же логике, по которой вторая стоит после подвала:
    // каждая следующая жертва меняет форму страницы сильнее предыдущей.
    const int wants[6][3] = {{1, 1, 1}, {1, 0, 1}, {0, 0, 1},
                             {0, 1, 2}, {0, 0, 2}, {0, 0, 3}};
    for (const auto& want : wants) {
        L.headings = want[0] != 0;
        L.footer = want[1] != 0;
        L.columns = want[2];
        const int drawn_rows = (rows + L.columns - 1) / L.columns;
        L.rows_per_column = L.columns > 1 ? drawn_rows : 0;
        L.line_count = drawn_rows + (L.headings ? 2 : 0);
        L.first_y = L.title_y + cell + (L.headings ? cell * 2 / 3 : cell / 2);
        const int floor_y = height_px - (L.footer ? cell * 2 + 4 : 2);
        const int room = floor_y - L.first_y;
        // The pitch never goes below the glyph height: rows that overlap are
        // not a denser list, they are an unreadable one.
        L.row_h = std::max(ink, std::min(cell + cell / 8,
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
    case K::LEFT_BRACKET: return "[";
    case K::RIGHT_BRACKET: return "]";
    case K::M: return "M";
    case K::P: return "P";
    case K::R: return "R";
    case K::B: return "B";
    case K::K: return "K";
    case K::T: return "T";  // оружие: достать или убрать
    case K::G: return "G";
    // Z — отмена. Подпись обязана существовать: «?» на экране управления это
    // не косметика, а признак строки, о которой человеку никто не скажет.
    case K::Z: return "Z";
    case K::V: return "V";  // вертикаль — фиксация оси у прямой
    case K::X: return "X";
    case K::GRAVE: return "`";
    case K::DELETE: return "Del";
    case K::BACKSPACE: return "Backspace";
    // СТРЕЛКИ СМОТРОВОЙ — СЛОВАМИ, И ЭТО НЕ ВКУС, А ОХВАТ ШРИФТА. Знак «←»
    // (U+2190) в испечённом атласе антиквы ОТСУТСТВУЕТ (tools/bake_ui_font.py
    // печёт «→», но не «←»), и строка с ним потеряла бы половину пары молча:
    // ui_draw_text пропускает неизвестный знак пробелом. «Left»/«Right» стоят
    // в одном ряду с «Del», «Esc» и «Backspace» — теми же словами, которыми эта
    // таблица уже называет клавиши без печатного знака.
    case K::LEFT: return "Left";
    case K::RIGHT: return "Right";
    case K::E: return "E";
    case K::Q: return "Q";
    default: return "?"; // loud, not blank: a nameless key is a table bug
    }
}

} // namespace dfn::app
