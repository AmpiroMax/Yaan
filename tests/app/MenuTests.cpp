/*
Created: 13:08:2026 - 19:44:00
Last updated: 27:08:2026 - 15:10:00
Module: tests/app
File: tests/app/MenuTests.cpp

Responsibility:
- Proves the settings page behaves like a settings page rather than like a list
  that happens to draw: every row lands on a LEGAL value, every exit saves, and
  the "needs a restart" warning fires for the rows that need one and stays
  silent for the rows that do not.

Dependencies:
- Uses: engine/app Menu (model only -- no canvas, no window), doctest.
- Used by: ctest.

Notes:
- EVERY CASE SHIPS ITS CONTROL (Rule 30), and here the controls are the ones
  that would catch the plausible bug rather than the impossible one:
  * the restart warning's control is a LIVE setting (head_bob). A warning that
    fires for everything is the same as no warning, and it is the easy bug.
  * the ladder's control is a row that is NOT a value (Back): adjust() there
    must do nothing, or "Enter cycles the value" would eat the exit.
  * the calibration page's control is entering it from the ROOT: a return that
    always goes back to settings would pass the settings case and strand
    everyone who came from the start screen.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone ui owns this file.
*/
/*
UPD:
- 13:08:2026 - 19:44:00: Created with the settings page.
- 13:08:2026 - 19:50:00: Случай про паузу: настройки достижимы с неё и возвращают
  в неё, контроль — вход с корня.
- 13:08:2026 - 20:05:00: Метки времени приведены к часам — были написаны вперёд.
- 14:08:2026 - 18:37:45: Красный app_menu. Корень меню вырос на строку
  «Редактор» (браузер карт), а КОНТРОЛЬ случая про паузу продолжал ходить туда
  счётом нажатий — жал вторую строку, открывал браузер и сообщал об этом как
  «настройки недостижимы с корня»: правдивый красный, называющий не тот
  предмет и не тот файл. Поведение было право, устарел тест.
  Починено МЕХАНИЗМОМ, а не индексом (правило 32): строки корня названы
  константами там же, где их индексируют, и заведён случай, который эти строки
  и их действия утверждает НАПРЯМУЮ. Его контроль — последняя строка, не
  открывающая страницы: без неё корень, выросший с КОНЦА, оставил бы все
  проверки зелёными, потому что каждая по-прежнему приходит на свою страницу.
  Теперь следующая перестановка корня падает одной строкой у причины.
- 14:08:2026 - 19:37:40: ROW_BACK уехал с 5 на 6: на странице настроек появилась
  строка «Управление» перед «Назад». Ровно тот дрейф индексов, который прошлая
  запись сделала видимым, — и на этот раз он всплыл сразу и у причины.
- 17:08:2026 - 16:35:20: две руки страницы паузы — с редактором и без; отличаются одним вызовом set_editing.
- 27:08:2026 - 02:50:00: ПАУЗА БОЛЬШЕ НЕ ИМЕЕТ ДВУХ РУК, и это не «тест подогнан
  под код», а снятое свойство: владелец 26.08 потребовал, чтобы состав меню не
  зависел от состояния игрока, и две руки прошлого случая были ровно этой
  зависимостью. Случай переписан в утверждение противоположного: шесть строк,
  один порядок, и КОНТРОЛЬ — что «Сохранить карту» и «Выйти без сохранения»
  доступны на той же странице всегда (раньше их не было вовсе).
  Корень вырос до семи строк (RootRow), и все они названы, а не отсчитаны.
  Новые случаи: раскладка строк (menu_row_boxes) — внутри кадра, без наложений,
  по одной коробке на пункт; и попадание мышью (menu_row_at) — в центр коробки
  попадает её строка, а мимо списка не попадает ничего.
- 27:08:2026 - 14:00:00: Строки страницы настроек больше не сосчитаны здесь, а
  названы в заголовке (SettingsRow): страница выросла на две строки (окно и
  полный экран, заказ владельца 27.08), «яркость» уехала с 4 на 6, и рукав
  начал жать «палитру», сообщая об этом как «страница калибровки не
  открывается». Тот же отказ, что был у строк корня 14.08, и та же починка.
  Новые случаи: лестницы окна и полного экрана с КОНТРОЛЕМ «они не трогают
  сетку рендера», и второй контроль предупреждения о перезапуске.
- 27:08:2026 - 15:10:00: СЛУЧАЙ СИНХРОНИЗАЦИИ (заказ владельца 27.08: «оно в игре
  и меню должно синхронизироваться»). Утверждение про ОТСУТСТВИЕ второго
  состояния нельзя проверить, оставаясь на одной точке входа: строка крутится в
  паузе и читается ИЗ КОРНЯ, потом наоборот. КОНТРОЛЬ проверен, а не обещан —
  сборка, в которой open(Settings) перечитывает настройки из «с чем запущены»
  (то есть по копии на точку входа), проходит все остальные случаи этого файла
  и падает ровно здесь. Второе утверждение случая — что узкий вход
  set_live_fullscreen НЕ глушит предупреждение о перезапуске: без него починка
  «страница видит F11» через set_settings прошла бы молча.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/Menu.h"

#include <utility>

using dfn::app::BrowseTarget;
using dfn::app::MenuAction;
using dfn::app::MenuModel;
using dfn::app::MenuPage;
using dfn::app::MenuSettings;

namespace {

// The page as the app hands it over: what the game is running with.
MenuModel launched() {
    MenuModel m;
    MenuSettings s;
    s.internal_w = 640;
    s.internal_h = 360;
    s.msaa = 4;
    s.palette = false;
    s.head_bob = 1.0f;
    m.set_settings(s);
    m.open(MenuPage::Settings);
    return m;
}

// THE START SCREEN'S ROWS, NAMED HERE BECAUSE THIS IS WHERE THEY ARE INDEXED.
// Cases that walk in from the root count move()s, and a counted walk is a magic
// number that goes stale in silence: when the root grew a "Редактор" row the
// pause case's CONTROL started pressing it, opened the map browser, and
// reported itself as "settings are not reachable from the root" -- a true red
// naming the wrong defect in the wrong file.
// НАЗВАНЫ В ЗАГОЛОВКЕ (RootRow), а не здесь: раньше эти константы жили в тесте,
// и рост корня на строку означал молча уехавший индекс. Теперь имя одно на код и
// на прибор, и «строка переехала» — это ошибка компиляции, а не тихий зелёный.
constexpr int ROOT_CONTINUE = static_cast<int>(dfn::app::RootRow::Continue);
constexpr int ROOT_NEW_GAME = static_cast<int>(dfn::app::RootRow::NewGame);
constexpr int ROOT_LOAD = static_cast<int>(dfn::app::RootRow::Load);
constexpr int ROOT_SETTINGS = static_cast<int>(dfn::app::RootRow::Settings);
constexpr int ROOT_EDITOR = static_cast<int>(dfn::app::RootRow::Editor);
constexpr int ROOT_CREDITS = static_cast<int>(dfn::app::RootRow::Credits);
constexpr int ROOT_QUIT = static_cast<int>(dfn::app::RootRow::Quit);
constexpr size_t ROOT_ROW_COUNT = static_cast<size_t>(dfn::app::RootRow::Count);

// СТРОКИ СТРАНИЦЫ НАСТРОЕК — ИЗ ЗАГОЛОВКА, а не сосчитанные здесь. Эти семь
// чисел были написаны от руки, и 27.08 страница выросла на две строки (окно и
// полный экран): «яркость» уехала с 4 на 6, рукав начал жать «палитру» и
// сообщил об этом как «страница калибровки не открывается» — правдивый красный,
// называющий не тот предмет. Ровно тот же отказ, что уже был у строк корня
// 14.08 и записан выше. Починено тем же механизмом: имя одно на код и на прибор.
constexpr int ROW_RESOLUTION = static_cast<int>(dfn::app::SettingsRow::Resolution);
constexpr int ROW_MSAA = static_cast<int>(dfn::app::SettingsRow::Msaa);
constexpr int ROW_PALETTE = static_cast<int>(dfn::app::SettingsRow::Palette);
constexpr int ROW_HEAD_BOB = static_cast<int>(dfn::app::SettingsRow::HeadBob);
constexpr int ROW_BRIGHTNESS = static_cast<int>(dfn::app::SettingsRow::Brightness);
constexpr int ROW_CONTROLS = static_cast<int>(dfn::app::SettingsRow::Controls);
constexpr int ROW_BACK = static_cast<int>(dfn::app::SettingsRow::Back);
constexpr int ROW_WINDOW = static_cast<int>(dfn::app::SettingsRow::Window);
constexpr int ROW_FULLSCREEN = static_cast<int>(dfn::app::SettingsRow::Fullscreen);

void select(MenuModel& m, int row) {
    m.open(MenuPage::Settings); // selection resets to 0
    for (int i = 0; i < row; ++i) {
        m.move(1);
    }
}

void select_root(MenuModel& m, int row) {
    m.open(MenuPage::Root); // selection resets to 0
    for (int i = 0; i < row; ++i) {
        m.move(1);
    }
}

// ЛЕСТНИЦА СЕТКИ РЕНДЕРА. Выросла 27.08 до Full HD: заказ владельца требует
// поднять базу качества, а страница, не умеющая назвать разрешение, на котором
// игра идёт, — это страница, которая про него врёт. Все ступени 16:9.
constexpr int RESOLUTION_RUNGS = 6;
bool legal_resolution(const MenuSettings& s) {
    return (s.internal_w == 320 && s.internal_h == 180)
        || (s.internal_w == 640 && s.internal_h == 360)
        || (s.internal_w == 960 && s.internal_h == 540)
        || (s.internal_w == 1280 && s.internal_h == 720)
        || (s.internal_w == 1600 && s.internal_h == 900)
        || (s.internal_w == 1920 && s.internal_h == 1080);
}

// ЛЕСТНИЦА ОКНА (новая строка страницы, 27.08).
bool legal_window(const MenuSettings& s) {
    return (s.window_w == 1280 && s.window_h == 720)
        || (s.window_w == 1600 && s.window_h == 900)
        || (s.window_w == 1920 && s.window_h == 1080)
        || (s.window_w == 2560 && s.window_h == 1440);
}

} // namespace

// THE ROOT'S ROWS, ASSERTED WHERE THEY ARE DEFINED RATHER THAN WHERE THEY ARE
// COUNTED. Every case below that starts from the start screen walks it by
// pressing down N times, so the root's shape is a premise of all of them --
// and a premise nobody states is a premise that fails somewhere else. It
// already did: the root gained "Редактор" as its second row, the pause case's
// control walked into the map browser, and the red it produced said "settings
// are not reachable from the root". This case exists so the NEXT rearrangement
// reports itself as "the root changed", one line, at the cause.
TEST_CASE("the root screen's rows are what the walk-in cases count on") {
    MenuModel m;
    m.set_settings(MenuSettings{});
    m.open(MenuPage::Root);
    CHECK(m.item_count() == ROOT_ROW_COUNT);

    // A ROW WHOSE SYSTEM DOES NOT EXIST IS STILL A ROW (owner, 26.08). Both of
    // them land on the stub page rather than on nothing: a row that swallows
    // Enter is indistinguishable from a broken menu.
    select_root(m, ROOT_CONTINUE);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Stub);
    CHECK(m.stub_message() != 0); // it says WHICH thing is missing

    select_root(m, ROOT_LOAD);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Stub);

    // New game and Editor open the SAME browser (docs/MAP_LAYOUT.md: neither
    // jumps straight into a map); only the target the app reads back differs.
    select_root(m, ROOT_NEW_GAME);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Categories);
    CHECK(m.browse_target() == BrowseTarget::Play);

    select_root(m, ROOT_EDITOR);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Categories);
    CHECK(m.browse_target() == BrowseTarget::Editor);

    select_root(m, ROOT_SETTINGS);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Settings);

    select_root(m, ROOT_CREDITS);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Credits);

    // THE CONTROL: the last row opens no page at all. Every check above lands
    // ON a page, so all of them together still pass a root whose FINAL row --
    // the one the player reads as "ВЫХОД" -- has quietly become something else.
    //
    // RUN, NOT ASSUMED. Three counterfactual roots were compiled against this
    // case: (A) the row count bumped alone, (B) editor and settings swapped,
    // (C) a real new row inserted before Выход. A fails on item_count only,
    // B on the two rows that moved, C on item_count AND on this line. Worth
    // writing down because the first draft of this comment claimed the Quit
    // line guarded case A, and arm A refuted it: `activate()` returns Quit by
    // FALLTHROUGH, so a bare count bump walks straight past it.
    select_root(m, ROOT_QUIT);
    CHECK(m.activate() == MenuAction::Quit);
}

TEST_CASE("every page with nothing to select leaves by BOTH keys") {
    // The splash, the credits and the stub have no rows, so Enter and Escape
    // must mean the same thing there. A page that answers only one of two
    // equally reasonable keys is a page a player gets stuck on.
    for (const MenuPage page : {MenuPage::Splash, MenuPage::Credits, MenuPage::Stub}) {
        CAPTURE(static_cast<int>(page));
        MenuModel m;
        m.open(page);
        CHECK(m.item_count() == 0);
        CHECK(m.activate() == MenuAction::None);
        CHECK(m.page() == MenuPage::Root);

        MenuModel e;
        e.open(page);
        CHECK(e.back() == MenuAction::None);
        CHECK(e.page() == MenuPage::Root);
    }
    // THE CONTROL: a page that DOES have rows must not go home on Enter --
    // otherwise the check above would pass a menu where every key exits.
    MenuModel r;
    r.set_settings(MenuSettings{});
    r.open(MenuPage::Root);
    CHECK(r.item_count() > 0);
}

TEST_CASE("settings rows land only on legal values, and wrap") {
    MenuModel m = launched();
    select(m, ROW_RESOLUTION);
    for (int i = 0; i < 9; ++i) { // more presses than rungs: the wrap is the point
        m.adjust(+1);
        CHECK(legal_resolution(m.settings()));
    }
    // Полный круг по лестнице возвращает туда, откуда начали.
    const MenuSettings before = m.settings();
    for (int i = 0; i < RESOLUTION_RUNGS; ++i) {
        m.adjust(+1);
    }
    CHECK(m.settings().internal_w == before.internal_w);
    CHECK(m.settings().internal_h == before.internal_h);

    // ОКНО И ПОЛНЫЙ ЭКРАН — НОВЫЕ СТРОКИ (заказ владельца 27.08), и у них тот же
    // вопрос: ступень или что угодно. КОНТРОЛЬ у обеих — что они НЕ трогают
    // сетку рендера: две строки, названные в игре похоже, легче всего спутать
    // именно в коде.
    select(m, ROW_WINDOW);
    const MenuSettings grid = m.settings();
    for (int i = 0; i < 9; ++i) {
        m.adjust(+1);
        CHECK(legal_window(m.settings()));
        CHECK(m.settings().internal_w == grid.internal_w);
    }
    select(m, ROW_FULLSCREEN);
    const bool fs = m.settings().fullscreen;
    m.adjust(+1);
    CHECK(m.settings().fullscreen != fs);
    m.adjust(-1);
    CHECK(m.settings().fullscreen == fs);

    select(m, ROW_MSAA);
    for (int i = 0; i < 9; ++i) {
        m.adjust(-1); // backwards too: a ladder with one working direction is half a ladder
        const uint32_t v = m.settings().msaa;
        CHECK((v == 0 || v == 2 || v == 4 || v == 8));
    }
}

TEST_CASE("a hand-edited settings.cfg value lands on the nearest legal rung") {
    MenuModel m;
    MenuSettings s;
    s.msaa = 3;         // nobody offers 3; a text editor does
    s.head_bob = 0.77f; // nor 0.77
    m.set_settings(s);
    m.open(MenuPage::Settings);

    select(m, ROW_MSAA);
    m.adjust(+1);
    const uint32_t v = m.settings().msaa;
    CHECK((v == 0 || v == 2 || v == 4 || v == 8));

    select(m, ROW_HEAD_BOB);
    m.adjust(+1);
    const float b = m.settings().head_bob;
    CHECK(b >= 0.0f);
    CHECK(b <= 2.0f);
    // On a quarter-step grid: 0, 0.5, 1, 1.5, 2 are all multiples of 0.5.
    CHECK(doctest::Approx(b * 2.0f).epsilon(1e-4) == static_cast<float>(static_cast<int>(b * 2.0f + 0.5f)));
}

TEST_CASE("the restart warning fires for the renderer's rows and not for the live one") {
    MenuModel m = launched();
    CHECK_FALSE(m.needs_restart()); // nothing turned yet

    select(m, ROW_HEAD_BOB);
    m.adjust(+1);
    CHECK(m.settings().head_bob != 1.0f);
    CHECK_FALSE(m.needs_restart()); // THE CONTROL: a live setting must stay silent

    // ВТОРОЙ КОНТРОЛЬ, заведённый 27.08 вместе со строками окна: они тоже
    // применяются живьём, и предупреждение о перезапуске обязано молчать. Без
    // него «предупреждение срабатывает на всё» прошло бы незамеченным — а это
    // и есть лёгкая ошибка.
    select(m, ROW_WINDOW);
    m.adjust(+1);
    CHECK_FALSE(m.needs_restart());
    select(m, ROW_FULLSCREEN);
    m.adjust(+1);
    CHECK_FALSE(m.needs_restart());

    select(m, ROW_RESOLUTION);
    m.adjust(+1);
    CHECK(m.needs_restart());

    // ...and it goes quiet again when the row is turned back to what launched.
    for (int i = 0; i < RESOLUTION_RUNGS - 1; ++i) {
        m.adjust(+1);
    }
    CHECK_FALSE(m.needs_restart());
}

TEST_CASE("every exit from the settings page saves") {
    MenuModel m = launched();
    select(m, ROW_BACK);
    CHECK(m.activate() == MenuAction::SettingsDone);
    CHECK(m.page() == MenuPage::Root);

    m = launched();
    CHECK(m.back() == MenuAction::SettingsDone); // Escape, from any row
    CHECK(m.page() == MenuPage::Root);
}

TEST_CASE("Enter on a value row is the same verb as right, and Back is not a value") {
    MenuModel m = launched();
    select(m, ROW_PALETTE);
    const bool before = m.settings().palette;
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.settings().palette != before);
    CHECK(m.page() == MenuPage::Settings); // still here: Enter turned a dial

    // THE CONTROL: adjust() on a row that is not a value must be a no-op, or
    // "Enter cycles" would have eaten the exit.
    select(m, ROW_BACK);
    const MenuSettings snapshot = m.settings();
    m.adjust(+1);
    CHECK(m.settings().internal_w == snapshot.internal_w);
    CHECK(m.settings().msaa == snapshot.msaa);
    CHECK(m.settings().palette == snapshot.palette);
    CHECK(m.settings().head_bob == snapshot.head_bob);
}

TEST_CASE("the calibration page returns to whichever page opened it") {
    MenuModel m = launched();
    select(m, ROW_BRIGHTNESS);
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Calibrate);
    CHECK(m.back() == MenuAction::CalibrationDone);
    CHECK(m.page() == MenuPage::Settings);

    // THE CONTROL: opened from anywhere else, it goes back to the root.
    MenuModel r;
    r.open(MenuPage::Calibrate);
    CHECK(r.activate() == MenuAction::CalibrationDone);
    CHECK(r.page() == MenuPage::Root);
}

TEST_CASE("the brightness dial cannot leave its range from either side") {
    MenuModel m;
    m.set_black_floor(999.0f);
    CHECK(m.black_floor() == doctest::Approx(dfn::app::black_floor_max()));
    m.set_black_floor(-1.0f);
    CHECK(m.black_floor() == doctest::Approx(0.0f));

    // ...including through the page's own keys: up is brighter, and holding it
    // stops at the ceiling rather than running past it.
    m.open(MenuPage::Calibrate);
    for (int i = 0; i < 100; ++i) {
        m.move(-1);
    }
    CHECK(m.black_floor() == doctest::Approx(dfn::app::black_floor_max()));
    for (int i = 0; i < 100; ++i) {
        m.move(+1);
    }
    CHECK(m.black_floor() == doctest::Approx(0.0f));
}

TEST_CASE("settings are reachable from pause, and come back to pause") {
    MenuModel m;
    m.set_settings(MenuSettings{});
    m.open(MenuPage::Pause);
    CHECK(m.item_count() == static_cast<size_t>(dfn::app::PauseRow::Count));
    m.set_selection(static_cast<size_t>(dfn::app::PauseRow::Settings));
    CHECK(m.activate() == MenuAction::None);
    CHECK(m.page() == MenuPage::Settings);
    CHECK(m.back() == MenuAction::SettingsDone);
    CHECK(m.page() == MenuPage::Pause); // NOT the start screen: the world is still there

    // And the pause page's other rows still do what they did.
    m.open(MenuPage::Pause);
    CHECK(m.activate() == MenuAction::Resume);
    m.set_selection(static_cast<size_t>(dfn::app::PauseRow::ToRoot));
    CHECK(m.activate() == MenuAction::ToRoot); // leaving does NOT close the game
    m.open(MenuPage::Pause);
    m.set_selection(static_cast<size_t>(dfn::app::PauseRow::Quit));
    CHECK(m.activate() == MenuAction::Quit);

    // THE CONTROL: entered from the root, it still returns to the root. The
    // row is NAMED rather than counted -- this walk is what silently started
    // pressing "Редактор" when the root grew a row, and the case above is what
    // now fails first if the name and the row part company again.
    MenuModel r;
    r.set_settings(MenuSettings{});
    select_root(r, ROOT_SETTINGS);
    CHECK(r.activate() == MenuAction::None);
    CHECK(r.page() == MenuPage::Settings);
    CHECK(r.back() == MenuAction::SettingsDone);
    CHECK(r.page() == MenuPage::Root);
}

TEST_CASE("настройка, повёрнутая в паузе, ВИДНА из главного меню, и наоборот") {
    // ЗАКАЗ ВЛАДЕЛЬЦА 27.08 ДОСЛОВНО: «оно в игре и меню должно
    // синхронизироваться». Это утверждение про ОТСУТСТВИЕ второго состояния, и
    // проверить его можно только так: повернуть строку, ВЫЙДЯ и войдя с ДРУГОЙ
    // стороны, и прочитать её там. Правильная реализация делает это даром —
    // страница одна; неправильная (по копии настроек на точку входа) прошла бы
    // все остальные случаи этого файла и упала бы здесь.
    MenuModel m;
    MenuSettings launched;
    launched.window_w = 1280;
    launched.window_h = 720;
    launched.fullscreen = false;
    m.set_settings(launched);

    // Игрок посреди игры открывает настройки С ПАУЗЫ и меняет две строки.
    m.open(MenuPage::Pause);
    m.set_selection(static_cast<size_t>(dfn::app::PauseRow::Settings));
    REQUIRE(m.activate() == MenuAction::None);
    REQUIRE(m.page() == MenuPage::Settings);
    select(m, ROW_WINDOW);
    m.adjust(+1);
    select(m, ROW_FULLSCREEN);
    m.adjust(+1);
    const MenuSettings from_pause = m.settings();
    CHECK(from_pause.window_w != launched.window_w);
    CHECK(from_pause.fullscreen != launched.fullscreen);
    CHECK(m.back() == MenuAction::SettingsDone);
    CHECK(m.page() == MenuPage::Pause);

    // Он выходит в главное меню и открывает настройки ОТТУДА. Обе строки стоят
    // там, где он их оставил.
    m.open(MenuPage::Root);
    select_root(m, ROOT_SETTINGS);
    REQUIRE(m.activate() == MenuAction::None);
    REQUIRE(m.page() == MenuPage::Settings);
    CHECK(m.settings().window_w == from_pause.window_w);
    CHECK(m.settings().window_h == from_pause.window_h);
    CHECK(m.settings().fullscreen == from_pause.fullscreen);

    // И ОБРАТНО: повёрнутое в главном меню видно с паузы.
    select(m, ROW_MSAA);
    m.adjust(+1);
    const uint32_t from_root = m.settings().msaa;
    CHECK(m.back() == MenuAction::SettingsDone);
    m.open(MenuPage::Pause);
    m.set_selection(static_cast<size_t>(dfn::app::PauseRow::Settings));
    REQUIRE(m.activate() == MenuAction::None);
    CHECK(m.settings().msaa == from_root);

    // КОНТРОЛЬ: F11 (единственный орган управления полным экраном ВНЕ страницы)
    // доезжает до страницы — и НЕ глушит предупреждение о перезапуске. Без
    // второго утверждения починка «страница видит F11» через set_settings
    // прошла бы: она обновляет и «с чем ЗАПУЩЕНЫ», то есть стирает
    // предупреждение о строке, которую игрок повернул минуту назад.
    CHECK(m.needs_restart()); // сглаживание уже повёрнуто выше
    m.set_live_fullscreen(!m.settings().fullscreen);
    CHECK(m.needs_restart());
}

TEST_CASE("the pause page is the same page whatever the player was doing") {
    // THE OWNER'S ORDER, 26.08: «меню должно все свои кнопки одинаково всегда
    // отображать в соответствующих режимах игры, вне зависимости от состояния
    // игрока (редактирует или нет)».
    //
    // WHAT THIS REPLACED, and why the replacement is not "the test was adjusted
    // to the code". The previous case built TWO models differing by one call --
    // set_editing -- and asserted that they had DIFFERENT rows: six with the
    // editor, four without, and "press Down twice" landing on Settings in one
    // and somewhere else in the other. That difference was the defect. The
    // property now held is that no such call exists to make: the model has one
    // pause page, six rows, in one order.
    MenuModel m;
    m.set_settings(MenuSettings{});
    m.open(MenuPage::Pause);
    REQUIRE(m.item_count() == 6);

    const MenuAction expected[] = {
        MenuAction::Resume, MenuAction::SaveMap,        MenuAction::None,
        MenuAction::ToRoot, MenuAction::DiscardToRoot,  MenuAction::Quit,
    };
    for (size_t row = 0; row < 6; ++row) {
        CAPTURE(row);
        m.open(MenuPage::Pause);
        m.set_selection(row);
        CHECK(m.selection() == row); // set_selection must actually point
        CHECK(m.activate() == expected[row]);
    }

    // SAVING STAYS ON THE PAGE. A save that navigated away would make a second
    // save impossible and hide its own answer.
    m.open(MenuPage::Pause);
    m.set_selection(static_cast<size_t>(dfn::app::PauseRow::SaveMap));
    CHECK(m.activate() == MenuAction::SaveMap);
    CHECK(m.page() == MenuPage::Pause);

    // THE CONTROL: pointing at a row that does not exist changes nothing. Half
    // the case above drives the page through set_selection, so a setter that
    // clamped or wrapped would make every row above reachable by accident.
    m.open(MenuPage::Pause);
    m.set_selection(99);
    CHECK(m.selection() == 0);
}

TEST_CASE("the rows are laid out inside the frame, one box each, without overlap") {
    // THE LAYOUT IS THE MOUSE'S ONLY MAP. Nothing in a screenshot can show that
    // a row's click box is off the bottom of the frame or on top of its
    // neighbour -- the picture looks right either way, and only the pointer
    // finds out.
    MenuModel m;
    m.set_settings(MenuSettings{});
    // Both shipping extremes AND the chunky preset: the block is sized from the
    // frame's height, so the case that fails is the SMALL one, where seven rows
    // at four times the font would run off the screen.
    for (const auto [w, h] : {std::pair{1920, 1080}, std::pair{640, 360}, std::pair{320, 180}}) {
        CAPTURE(w);
        CAPTURE(h);
        for (const MenuPage page : {MenuPage::Root, MenuPage::Pause}) {
            m.open(page);
            const auto boxes = dfn::app::menu_row_boxes(w, h, m);
            REQUIRE(boxes.size() == m.item_count());
            for (size_t i = 0; i < boxes.size(); ++i) {
                CAPTURE(i);
                CHECK(boxes[i].x >= 0);
                CHECK(boxes[i].y >= 0);
                CHECK(boxes[i].w > 0);
                CHECK(boxes[i].h > 0);
                CHECK(boxes[i].x + boxes[i].w <= w);
                CHECK(boxes[i].y + boxes[i].h <= h);
                if (i > 0) {
                    // Strictly below its predecessor, and not touching it.
                    CHECK(boxes[i].y >= boxes[i - 1].y + boxes[i - 1].h);
                }
            }
        }
    }
}

TEST_CASE("the pointer selects the row it is on, and nothing when it is not on one") {
    MenuModel m;
    m.set_settings(MenuSettings{});
    m.open(MenuPage::Root);
    const int w = 1920;
    const int h = 1080;
    const auto boxes = dfn::app::menu_row_boxes(w, h, m);
    REQUIRE(boxes.size() == m.item_count());
    for (size_t i = 0; i < boxes.size(); ++i) {
        CAPTURE(i);
        const int cx = boxes[i].x + boxes[i].w / 2;
        const int cy = boxes[i].y + boxes[i].h / 2;
        CHECK(dfn::app::menu_row_at(w, h, m, cx, cy) == i);
    }

    // THE CONTROL, and it is the half that matters: hovering the emblem, the
    // corner or the space above the column must select NOTHING. A hit test that
    // answered "the nearest row" would pass every check above and would drag
    // the selection around the screen with the pointer.
    CHECK(dfn::app::menu_row_at(w, h, m, 0, 0) == m.item_count());
    CHECK(dfn::app::menu_row_at(w, h, m, w / 4, h / 2) == m.item_count());
    CHECK(dfn::app::menu_row_at(w, h, m, -5, -5) == m.item_count());
    CHECK(dfn::app::menu_row_at(w, h, m, w - 1, 0) == m.item_count());
}
