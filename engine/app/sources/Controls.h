/*
Created: 14:08:2026 - 19:22:10
Last updated: 27:08:2026 - 14:00:00
Module: engine/app
File: engine/app/sources/Controls.h

Responsibility:
- THE key binding table: every discrete action the app dispatches on a key
  edge, the key(s) that trigger it, where it applies, and the localization key
  describing it. One table, read by BOTH the input handlers and the controls
  screen.

Key items:
- Action: the closed set of key-dispatched actions.
- Binding / control_bindings() / binding_for(): the table and its lookups.
- MovementRow / movement_rows(): the continuous fly-mode inputs, described.

Dependencies:
- Uses: engine/platform/input (Key only).
- Used by: App (dispatch), Menu (the controls page), tests/app.

Notes:
- WHY A TABLE AND NOT A DRAWN LIST. The user asked to be able to LOOK at the
  controls, and a screen that lists them is easy; a screen that lists them
  CORRECTLY a month from now is not. A hand-written list drifts the first time
  somebody binds a key in App.cpp and does not think about a menu page -- and
  it drifts SILENTLY, because a wrong help screen looks exactly like a right
  one. So the list is not a copy of the bindings, it IS the bindings: App asks
  this table for the key belonging to an Action instead of naming a key
  literal, which means a new key cannot be dispatched without a row here, and
  a row here cannot exist without a description. The test then only has to
  check that the table is total and unambiguous -- see tests/app/ControlsTests.
- KEY NAMES ARE NOT PROSE (the same reading of Rule 5 the debug readout uses):
  "F3", "Tab", "1" are what is physically printed on the key and are read the
  same in every language, so key_name() returns them directly. Everything that
  is a SENTENCE -- what the key does, and the names of the movement inputs --
  goes through localization like all other user-facing text.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ADDING A KEY? Add its Action here and give it a row. App.cpp must dispatch it
  through action_pressed(), never through a Key literal -- a literal is how the
  screen starts lying.
*/
/*
UPD:
- 14:08:2026 - 19:22:10: Создан. Экран управления (просьба пользователя: «я должен
  уметь посмотреть на это в настройках управления»), заведённый так, чтобы
  список не мог разъехаться с кодом: обработчики спрашивают клавишу ПО
  ДЕЙСТВИЮ, поэтому новая клавиша без строки в таблице просто не диспатчится.
- 17:08:2026 - 16:27:55: Action::Fullscreen — строка таблицы, без неё клавишу не разослать.
- 17:08:2026 - 16:59:23: строки постройки (B, G) и раскладка в ДВЕ КОЛОНКИ, когда список не влезает.
- 17:08:2026 - 22:32:14: ПЯТЬ РЕЖИМОВ РЕДАКТОРА НА КЛАВИШАХ 1..5 и поле alias_scope.
  Цифры в редакторе выбирают инструмент, в теле делают то же, что делали; их
  прежние действия сохранены на F-клавишах, и снимку (5) добавлен алиас F5,
  которого у него не было. Строка теперь может иметь РАЗНУЮ область у клавиши и
  у алиаса — одной областью это невыразимо, а выбор «потерять F3 в редакторе
  или дать 2 два хозяина» плох обоими концами: драка двух хозяев за клавишу
  невидима, щелчок делает то одно, то другое.
  И область СТАЛА ДЕЙСТВОВАТЬ: до сегодня App::action_pressed её не читал
  вовсе — область была комментарием, который экран управления показывал
  человеку, а код не соблюдал.
- 18:08:2026 - 17:16:29: Action::Undo — ОДНО действие на отмену И повтор. Я завёл два, и
  ТАБЛИЦА МЕНЯ ПОПРАВИЛА: у второй строки не было бы своей клавиши, а строка
  без клавиши не рисуется на экране управления — проверка покраснела сразу.
  Отдать повтору ту же Z нельзя: два действия на одной клавише в одной области
  видимости запрещены другой проверкой, и правильно запрещены. Значит повтор —
  не вторая привязка, а то же нажатие с модификатором.
- 18:08:2026 - 18:58:40: Строка AxisLock — фиксация вертикали.
- 18:08:2026 - 20:26:30: Строка DeleteSelected.
- 18:08:2026 - 23:20:00: Четыре строки под инструменты 6..9.
- 18:08:2026 - 23:52:10: Строка GridToggle — сетка включается клавишей, а не галочкой в панели.
- 27:08:2026 - 14:00:00: controls_layout() принимает МЕРУ ШРИФТА, которым страницу
  рисуют (заказ владельца 27.08: интерфейс перешёл на антикву). Пол шага строк
  — высота СТРОКИ, а не прописной: у блочного шрифта они совпадали, у антиквы
  нет, и первый кадр дал «у» с «р» соседних строк друг на друге. Подвал
  «Esc — назад» снят вместе со всеми подсказками управления; полоса под
  списком осталась полем.
*/

#pragma once

#include "engine/platform/input/interfaces/IInput.h"

#include <cstdint>
#include <span>

namespace dfn::app {

// EVERY ACTION THE APP DISPATCHES ON A KEY EDGE. Closed set on purpose: it is
// what makes "did anyone add a key without telling the screen" a question with
// an answer. Continuous input (walking, looking, the fly camera) is NOT here --
// it is not an edge, it is polled, and it is described by movement_rows().
enum class Action : uint8_t {
    ThirdPerson = 0,   // 1
    DebugReadout,      // 2 / F3
    StateCapture,      // 3 / F2
    Wireframe,         // 4 / F4
    Screenshot,        // 5
    ToggleBody,        // Tab
    TrajectoryRecord,  // R
    TrajectoryReplay,  // P
    ChatWindow,        // /
    QuickRemark,       // Enter
    Map,               // M
    MenuPause,         // Esc
    Fullscreen,        // F11
    // THE BUILD HAND (editor only). Two rows, not four: the palette IS a menu,
    // so picking inside it belongs to MENU navigation (the arrows) and not to
    // this table. Placing is the left mouse button, which is not here because
    // the table binds keys. The controls screen made this call for me — four
    // rows pushed the list off a 320x180 frame and app_controls went red.
    CursorToggle,      // R — курсор: смотреть камерой или указывать мышью
    BuildMenu,         // B
    BuildRotate,       // G
    // ПЯТЬ РЕЖИМОВ РЕДАКТОРА, номерами САМОГО ПОЛЬЗОВАТЕЛЯ (17.08): 1 высота,
    // 2 поверхность, 3 выбор, 4 постройка, 5 «просто смотрю». Полоса фишек
    // показывает те же номера, поэтому «нажми 3» и «третья фишка» — одно и то
    // же. Отдельные строки, а не одна с параметром: экран управления рисуется
    // ИЗ ЭТОЙ ТАБЛИЦЫ, и режим без строки был бы режимом, о котором человеку
    // никто не скажет.
    ToolHeight,        // 1 (редактор)
    ToolPaint,         // 2 (редактор)
    ToolSelect,        // 3 (редактор)
    ToolPlace,         // 4 (редактор)
    ToolLook,          // 5 (редактор)
    // ЕЩЁ ЧЕТЫРЕ ЦИФРЫ, потому что инструментов стало девять (заказ 18.08 —
    // «инструменты забинди на цифры»). Строки отдельные по той же причине, что
    // и первые пять: экран управления рисуется ИЗ ЭТОЙ ТАБЛИЦЫ.
    Tool6,             // 6 (редактор)
    Tool7,             // 7 (редактор)
    Tool8,             // 8 (редактор)
    Tool9,             // 9 (редактор)

    /// ОТМЕНА И ПОВТОР — ОДНО ДЕЙСТВИЕ, А НЕ ДВА (заказ 18.08: «надо добавить
    /// cmd+z cmd+shift+z отмену действия и отмену отмены»).
    ///
    /// Я СНАЧАЛА ЗАВЁЛ ДВА, и таблица меня поправила: у строки Redo не было бы
    /// СВОЕЙ клавиши, а строка без клавиши не рисуется на экране управления —
    /// проверка «каждую строку можно нарисовать» покраснела сразу. Отдать
    /// Redo ту же Z нельзя: два действия на одной клавише в одной области
    /// видимости запрещены другой проверкой, и правильно запрещены.
    ///
    /// Вывод: повтор — это НЕ вторая привязка, а то же нажатие с модификатором.
    /// Shift разбирается в обработчике, а описание строки называет оба
    /// сочетания. Таблица оказалась умнее моей первой мысли.
    Undo,

    /// ФИКСАЦИЯ ВЕРТИКАЛИ (заказ 18.08: «попробовал вверх тянуть, а она вот,
    /// на земле лежит, длинная / как в других 3д редакторах прямые вверх
    /// рисуют?»). Ответ индустрии — ось: у мыши две координаты, у точки три,
    /// и клавиша даёт недостающее условие. Строка отдельная, потому что экран
    /// управления рисуется ИЗ ЭТОЙ ТАБЛИЦЫ: клавиша без строки — клавиша, о
    /// которой человеку никто не скажет.
    AxisLock,

    /// УБРАТЬ ВЫБРАННОЕ (заказ 18.08: «не понимаю как удалить стену»). Кнопка в
    /// панели якоря была, но она умела только якорь и жила там, куда человек со
    /// стеной в выборе не заходит. Клавиша спрашивает сессию, а сессия решает
    /// сама, что сейчас выбрано.
    DeleteSelected,

    /// СЕТКА МИРА — КЛАВИШЕЙ, А НЕ ГАЛОЧКОЙ В ПАНЕЛИ (заказ 18.08: «сетку надо
    /// на кнопку включать, а не через интерфейс инструмента»). Ползунок шага
    /// остаётся в панели: шаг настраивают редко, а включают и выключают сетку
    /// постоянно, и лезть за этим в меню — то же, что лезть в меню за отменой.
    GridToggle,
    Count,
};

// Where an action applies. It is part of the table rather than a comment
// because two actions MAY share a key when their scopes do not overlap, and a
// test that did not know the scopes would have to either miss that or forbid
// it.
enum class Scope : uint8_t {
    Anywhere,    // both modes
    EditorOnly,  // the free-camera viewer
    PlayingOnly, // in the body
};

struct Binding {
    Action action;
    platform::Key key;
    // The second key for the same action, or UNKNOWN. The aliases are real
    // history, not decoration: F2/F3/F4 appear in frames and recipes already
    // archived, so moving them silently would make every recipe on disk wrong.
    platform::Key alias;
    const char* what;  // localization key: what the action DOES
    Scope scope;
    // WHERE THE ALIAS APPLIES, which is not always where the row does.
    //
    // WHY IT IS A SEPARATE FIELD (17.08.2026). The digits 1..5 became the
    // editor's five modes, and the four actions that already held them —
    // третье лицо, отладочный вывод, снимок состояния, каркас — kept their
    // F-keys. So those rows are PLAYING-ONLY on their digit and ANYWHERE on
    // their F-alias, and one scope cannot say both. Without this the choice
    // would have been between losing F3 in the editor and having 2 mean two
    // things in one place — and the second is invisible: the click does one or
    // the other depending on which handler ran first.
    //
    // Anywhere by default because that is what an alias is FOR: F2/F3/F4
    // appear in frames and recipes already archived, and narrowing them
    // silently would make every recipe on disk wrong. A row that wants its
    // alias narrowed says so, and the ambiguity test reads THIS field — so an
    // alias widened into a collision fails there rather than on a user's
    // keyboard.
    Scope alias_scope = Scope::Anywhere;
};

// The table, indexed so that control_bindings()[i].action == Action(i).
[[nodiscard]] std::span<const Binding> control_bindings();
[[nodiscard]] const Binding& binding_for(Action action);

// The physical label on the key ("1", "F3", "Tab", "Esc"). ASCII, not
// translated -- see the header note. Returns "?" for a key with no label,
// which is loud rather than blank.
[[nodiscard]] const char* key_name(platform::Key key);

// THE FLY CAMERA'S CONTINUOUS INPUTS. Polled every frame rather than dispatched
// on an edge, so they carry no Action and cannot be checked against a handler --
// they are documentation, and the table says so instead of pretending otherwise.
struct MovementRow {
    const char* keys;  // localization key for the key names (e.g. "WASD")
    const char* what;  // localization key for what they do
};
[[nodiscard]] std::span<const MovementRow> movement_rows();

// WHERE THE CONTROLS PAGE'S ROWS LAND, computed rather than drawn, so the
// question "does the list fit on the screen" has an answer a test can read.
//
// IT EXISTS BECAUSE THE FIRST VERSION DID NOT FIT AND NOTHING SAID SO. The page
// was written with a row pitch that tightened as the frame got shorter, which
// looked like it handled small screens and did not: the pitch cannot go below
// the glyph height without the rows printing into each other, so at 320x180 the
// last two rows ran off the bottom and the footer landed on top of a row. The
// frame showed it immediately; no assertion could, because the layout only
// existed inside the draw. Now the arithmetic is out here and tests/app checks
// it at every resolution the settings page offers.
//
// The two droppable parts are dropped in order of what costs least: the footer
// hint (Escape works whether or not it is advertised), then the section
// headings (they group rows that are already visually grouped). The ROWS are
// never dropped -- a controls screen missing controls is worse than a dense one.
struct ControlsLayout {
    int row_h = 0;        // pitch between rows
    int title_y = 0;
    int first_y = 0;      // top of the first drawn line
    int bottom = 0;       // y just past the last drawn line
    int line_count = 0;   // rows + whatever headings survived
    bool headings = true;
    /// НИЖНЯЯ ПОЛОСА. Раньше в ней стояла строка «Esc — назад»; заказ владельца
    /// 27.08 снял со всех экранов подсказки управления, и полоса осталась ПОЛЕМ:
    /// список, упирающийся в самый низ кадра, читается как обрезанный. Имя
    /// сохранено, потому что порядок жертв (подписи → полоса → две колонки) —
    /// это по-прежнему то, чем страница платит за тесноту.
    bool footer = true;
    bool fits = true;     // false = it overflowed anyway, and the page says so
    /// HOW MANY COLUMNS OF ROWS. 1 normally; 2 when the list no longer fits the
    /// frame's height. The list only grows — every tool the editor gains adds a
    /// row — so "print fewer controls" was never the answer, and dropping the
    /// pitch below the glyph height is not one either. Splitting is.
    int columns = 1;
    /// Rows in the FIRST column when columns == 2 (the second gets the rest).
    int rows_per_column = 0;
};
/// `min_pitch` / `line_h` — МЕРА ТОГО ШРИФТА, КОТОРЫМ СТРАНИЦУ РИСУЮТ. Раскладка
/// стояла на render::FONT_CELL_H/FONT_INK_H, пока другого шрифта в дереве не
/// было; после перехода интерфейса на антикву (UiFont.h, заказ 27.08) число
/// ячеек 6×9 перестало описывать нарисованное, и страница, посчитанная по нему,
/// ушла бы за низ кадра — ровно тот отказ, ради которого эта функция и появилась.
///
/// `min_pitch` — ПОЛ ШАГА СТРОК, и у настоящего шрифта это ВЫСОТА СТРОКИ, а не
/// высота прописной. У блочного шрифта они совпадали (FONT_INK_H включал ряд
/// выносных элементов), у антиквы — нет: первый кадр после перехода дал шаг
/// ровно в высоту прописной, и «у» с «р» соседних строк налезли друг на друга.
/// Умолчания — старая блочная мера, чтобы прибор мог позвать её без шрифта.
[[nodiscard]] ControlsLayout controls_layout(int width_px, int height_px,
                                             int min_pitch = 0, int line_h = 0);

} // namespace dfn::app
