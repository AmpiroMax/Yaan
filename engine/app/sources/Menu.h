/*
Created: 10:08:2026 - 10:26:39
Last updated: 27:08:2026 - 20:10:06
Module: engine/app
File: engine/app/sources/Menu.h

Responsibility:
- The start screen and the in-game pause screen: what they contain, which item
  is selected, and how they draw. No world knowledge, no input polling -- the
  app feeds key edges in and reads an Action out, so the menu is testable
  without a window.

Key items:
- MenuSettings: the settings.cfg rows the player can turn on the settings page.
- MenuModel: page + selection + the map BROWSER (categories -> maps); the app
  hands in a MapCatalog and reads an Action + the chosen map out.
- draw_menu(): renders the current page into a PixelCanvas through BitmapFont.

Dependencies:
- Uses: engine/render (PixelCanvas, BitmapFont), engine/app Localization,
  engine/app MapCatalog.
- Used by: App only.

Notes:
- Every visible string is a LOCALIZATION KEY, never a literal (Rule 5). The
  menu cannot contain text by construction: it stores hashes and asks
  localized() at draw time.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 10:08:2026 - 10:26:39: Created -- start screen, map picker, pause screen (user request: launch
                          with and without the menu, check demo maps).
- 13:08:2026 - 18:45:00: Страница калибровки яркости (просьба пользователя: «минимальную
  яркость в настройках, как в скайриме/думе при старте просят настроить, чтобы вещи почти
  сливались»). Модель держит значение, пока страница открыта; пишет его приложение.
- 13:08:2026 - 19:40:00: ЭКРАН НАСТРОЕК (просьба пользователя). До него settings.cfg
  можно было изменить ТОЛЬКО текстовым редактором: разрешение, сглаживание, палитра,
  покачивание камеры и яркость существовали как настройки и не существовали как экран.
  Страница держит черновик и то, с чем игра ЗАПУЩЕНА, — вторая копия нужна, чтобы
  сказать вслух, какая строка применится лишь после перезапуска.
- 13:08:2026 - 19:50:00: Вторая точка входа на страницу настроек — пауза; модель
  помнит, куда возвращаться, тем же способом, что и страница калибровки.
- 13:08:2026 - 20:05:00: Метки времени приведены к часам — были написаны вперёд.
- 14:08:2026 - 16:11:00: Кнопка «Редактор» на корневом экране (запрос В39: две кнопки,
  игра и редактор) → MenuAction::EnterEditor. Корень стал четырёхстрочным: Играть,
  Редактор, Настройки, Выход.
- 14:08:2026 - 16:50:36: БРАУЗЕР КАРТ (контракт docs/MAP_LAYOUT.md). Вход в Играть и в
  Редактор открывает не карту, а браузер: категории (папки) → карты (.map) → открыть.
  MenuPage::Maps заменён на Categories + CategoryMaps; MenuAction::EnterWorld/EnterEditor
  свёрнуты в один OpenMap (режим решает browse_target). MapEntry/set_maps/chosen_stand
  сняты — их место занял MapCatalog. Пустые категории показываются пустыми.
- 14:08:2026 - 17:51:15: open_category() — прямой спуск во второй уровень браузера
  (дверь снимка DFN_MENU_PAGE=category_maps, чтобы список карт тоже снимался, правило 27).
- 14:08:2026 - 19:37:40: MenuPage::Controls — страница управления (просьба
  пользователя). Read-only: просили ПОСМОТРЕТЬ, а список, который выглядит
  редактируемым и не редактируется, хуже списка. Живёт внутри настроек, значит
  достижима и с паузы — не выходя из мира.
- 17:08:2026 - 16:35:20: SaveMap/DiscardToRoot и set_editing — три выхода редактора по Esc (заказ 17.08).
- 27:08:2026 - 01:05:40: ГЛАВНОЕ МЕНЮ ПО ОБРАЗЦУ SKYRIM И РАЗВЯЗКА ПАУЗЫ С РЕДАКТОРОМ
  (заказ владельца 26.08, дословно: «сделать меню 1в1 как в скайриме… сделать
  нормальное меню паузы — сейчас оно зависит от режима редактирования… меню должно
  все свои кнопки одинаково всегда отображать в соответствующих режимах игры, вне
  зависимости от состояния игрока»).
  1. set_editing/editing() СНЯТЫ, а не выключены. Пока флаг существует, состав
     страницы паузы остаётся представимым как «зависит от того, что в руке»: строки
     появлялись и исчезали по одному вызову, и приложение решало, что показать. Теперь
     страница паузы — ОДИН набор из шести строк, и вопроса «а в этом ли я режиме»
     у неё нет вовсе (правило 32: чинится механизм, а не случай). «Сохранить карту»
     вне редактора не прячется: она отвечает честной строкой статуса, а строка,
     которая то есть, то нет, учит игрока, что меню врёт.
  2. Корень стал списком образца: Продолжить / Новая игра / Загрузить / Настройки /
     Редактор / Титры / Выход. Систем сохранений и титров у нас нет — пункт всё равно
     рисуется и ведёт на ЗАГЛУШКУ с честной надписью (MenuPage::Stub), потому что
     спрятанный пункт неотличим от несуществующего.
  3. MenuPage::Splash — кадр студии при запуске; MenuPage::Credits — титры с
     обязательной строкой лицензии герба.
  4. RootRow/PauseRow названы в заголовке: до сих пор строки корня считались
     нажатиями в тестах, и рост корня на одну строку молча уводил проверку на
     соседний пункт (запись 14.08 в MenuTests об этом же).
  5. menu_row_boxes()/menu_row_at() — раскладка строк ОДНА на отрисовку и на мышь.
     Выбор мышью и стрелками нельзя было сделать двумя арифметиками: вторая копия
     разъезжается с первой в первый же день, и попадание по пункту становится
     «иногда».
- 27:08:2026 - 14:00:00: ТРИ ЗАКАЗА ВЛАДЕЛЬЦА 27.08 В ОДНОМ ЗАГОЛОВКЕ.
  1. SettingsRow переехал СЮДА из безымянного пространства Menu.cpp — по тому
     же уроку, что записан над RootRow 14.08, и он повторился день в день:
     страница выросла на две строки, и рукав, ходивший к «яркости» счётом
     нажатий, начал жать «палитру» и рапортовать об этом как «страница
     калибровки не открывается».
  2. MenuSettings получил window_w/window_h/fullscreen — страница настроек
     теперь умеет графику («в меню настроек добавь возможность менять
     графику»). Окно и сетка рендера — РАЗНЫЕ строки: у нас это два разных
     числа, и страница, называющая их одним словом, врала бы про то, которое
     игрок не выбирал. Умолчание сетки поднято до 1920×1080.
  3. set_live_fullscreen() — узкий вход для F11: полный экран единственная
     настройка с двумя органами управления, и страница обязана показывать
     экран. Через set_settings этого делать нельзя — он глушит needs_restart.
- 27:08:2026 - 20:10:06: ЗВУК НА СТРАНИЦЕ НАСТРОЕК И ВОПРОС «ЭТО ПАУЗА ИЛИ
  ЗАГЛАВНЫЙ ЭКРАН» (заказ владельца через музыкальную сессию: заглавная тема
  играет в главном меню на репите, громкость музыки и эффектов регулируется).
  1. SettingsRow вырос на MusicVolume и SfxVolume, MenuSettings — на два
     множителя. Они стоят ПОСЛЕ картинки и ДО глаголов (управление, назад):
     страница читается как «что видно, что слышно, куда уйти». Это третий раз,
     когда страница растёт, и третий раз имена спасают от молчаливого сдвига
     номеров — см. запись 27.08 выше и запись над RootRow.
  2. Обе строки применяются ЖИВЬЁМ, и это не украшение: громкость — та
     единственная настройка, которую нельзя выбрать глазами. Ползунок, чей
     результат слышен только после выхода со страницы, заставляет игрока
     входить и выходить, пока не угадает.
  3. over_world() — страница отвечает, стоит ли она НАД ЖИВЫМ МИРОМ. Нужна
     музыке: тема принадлежит заглавному экрану, а не паузе, и «настройки,
     открытые из паузы» — это по-прежнему пауза. Ответ ВЫЧИСЛЯЕТСЯ по
     цепочке возвратов (settings_return_ / calibrate_return_), а не хранится
     вторым флагом: второй флаг — это shadow copy цепочки, и он разойдётся с
     ней на первой же новой странице (правило 39).
*/

#pragma once

#include "engine/app/sources/MapCatalog.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

// Whether the browser was opened by "Играть" or "Редактор". OpenMap carries no
// mode of its own -- both buttons run the SAME browser (В39: play changes map
// through the same picker, only without the debug tools), and this is what the
// app reads back to decide whether to possess the body or fly the free camera.
enum class BrowseTarget : uint8_t { Play, Editor };

enum class MenuPage : uint8_t {
    Root = 0,          // start screen
    Categories = 1,    // the browser's first level: category folders
    CategoryMaps = 2,  // the browser's second level: .map files in one category
    Pause = 3,         // in-game
    Calibrate = 4,     // brightness calibration (Skyrim/Doom's first-run screen)
    Settings = 5,      // the settings.cfg rows, turnable without a text editor
    Controls = 6,      // the key list -- READ ONLY, drawn from the binding table
    Splash = 7,        // the studio's launch frame; any key or a timer leaves it
    Credits = 8,       // titles, and the emblem's licence line lives THERE
    Stub = 9,          // "this does not exist yet", named out loud (see open_stub)
};

// THE START SCREEN'S ROWS, NAMED WHERE THEY ARE DEFINED. Everything that reaches
// a row by pressing Down N times -- tests, doors, recipes -- has the root's shape
// as a premise, and a premise nobody states fails somewhere else: when the root
// grew "Редактор" in August the pause test's control silently started opening the
// map browser and reported "settings are unreachable".
enum class RootRow : size_t {
    Continue = 0, // no save system yet -> the honest stub, never a hidden row
    NewGame,      // the map browser, Play target
    Load,         // no save system yet -> stub
    Settings,
    Editor,       // the map browser, Editor target (ours, not the reference's)
    Credits,
    Quit,
    Count,
};

// THE PAUSE PAGE'S ROWS, and there is ONE set of them (owner, 26.08). Not one set
// per mode, not one set per what the player is holding: six rows, always, in this
// order. The two rows that cannot be taken back sit last, furthest from where the
// cursor starts.
enum class PauseRow : size_t {
    Resume = 0,
    SaveMap,   // answers with a status line when there is nothing to save
    Settings,
    ToRoot,
    Discard,
    Quit,
    Count,
};

// СТРОКИ СТРАНИЦЫ НАСТРОЕК, НАЗВАННЫЕ ТАМ ЖЕ, ГДЕ ИХ ИНДЕКСИРУЮТ. Ровно тот же
// урок, что записан над RootRow, и он повторился в тот же день: страница выросла
// на две строки (окно и полный экран, заказ владельца 27.08), а рукав ходил к
// «яркости» счётом нажатий — и молча начал жать «палитру», сообщая об этом как
// «страница калибровки не открывается». Имя одно на код и на прибор: теперь
// перестановка строк — ошибка компиляции, а не тихий зелёный.
//
// ПОРЯДОК — ЭТО ПОРЯДОК КАРТИНКИ: сначала то, во что её показывают (окно, экран),
// потом в чём её считают (сетка, сглаживание, палитра), потом как она движется, и
// последним — насколько тёмной ей позволено стать.
enum class SettingsRow : size_t {
    Window = 0,   // размер окна — живьём
    Fullscreen,   // окно или весь экран — живьём
    Resolution,   // сетка рендера — со следующего запуска
    Msaa,
    Palette,
    HeadBob,
    Brightness,   // opens the calibration page, which is where an EYE decides
    // ЗВУК. Две шины, две строки — ровно потому, что это ДВЕ разные жалобы:
    // «музыка мешает слушать шаги» и «в игре слишком громко». Один общий
    // ползунок не может ответить ни на одну из них.
    MusicVolume,
    SfxVolume,
    Controls,     // opens the key list (read-only; rebinding is not a thing yet)
    Back,
    Count,
};

enum class MenuAction : uint8_t {
    None = 0,
    // A map was chosen in the browser. chosen_map() is the manifest; the app
    // resolves its source and enters browse_target()'s mode (Play or Editor).
    // One action for both buttons: the browser is shared (В39).
    OpenMap,
    Resume,
    ToRoot,
    Quit,
    // The player is done calibrating: the app persists black_floor() to
    // settings.cfg and returns wherever it came from. Deliberately not folded
    // into ToRoot -- "go back" and "save my brightness" are different events,
    // and a save that only happens on one exit path is a setting that silently
    // forgets itself.
    CalibrationDone,
    // The player left the settings page: the app copies settings() into its
    // config, applies what can be applied live and persists the file. Same
    // reasoning as CalibrationDone, and the same guarantee -- EVERY exit path
    // from the page emits it, so there is no way to leave and lose the change.
    SettingsDone,
    // THE EDITOR'S THREE EXITS (user, 17.08: «в редактуре при esc сохранять
    // карту, уходить без сохранения и выходить в главное меню а не закрывать
    // всю игру»). Three actions and not one with a flag, because they are three
    // different promises: SaveMap writes and STAYS (so you can save twice),
    // DiscardToRoot throws the session away, ToRoot is the plain way out that
    // makes no claim about saving. A single "exit" that guessed which one you
    // meant would guess wrong on the only one that cannot be undone.
    SaveMap,
    DiscardToRoot,
};

// THE SETTINGS THE PLAYER CAN TURN, and it is exactly the settings.cfg rows
// that describe the PICTURE. It mirrors AppConfig rather than referencing it
// for the same reason the menu owns black_floor while its page is up: this
// header knows nothing about the app, so the menu stays testable without one.
//
// show_menu is DELIBERATELY ABSENT. It is the row that decides whether this
// screen exists at all, and a switch that removes the screen it is drawn on is
// a trap: the player turns it off, and the only way back is the text editor
// this page was built to replace.
struct MenuSettings {
    // ОКНО — то, что игрок называет «разрешением» (заказ владельца 27.08: «в
    // меню настроек добавь возможность менять графику… разрешение окна,
    // полный экран»). Отдельная строка от сетки рендера ниже, и это не
    // педантизм: у нас это ДВА разных числа, и страница, называющая их одним
    // словом, будет врать про то, которое игрок не выбирал.
    uint32_t window_w = 1280;
    uint32_t window_h = 720;
    bool fullscreen = false;
    // СЕТКА РЕНДЕРА: в скольких пикселях считается сам мир, прежде чем его
    // растянут на окно. Это качество картинки, а не размер окна.
    uint32_t internal_w = 1920;
    uint32_t internal_h = 1080;
    uint32_t msaa = 4;       // 0/2/4/8 coverage samples on the internal grid
    bool palette = false;    // 64-colour quantization + dithering
    float head_bob = 1.0f;   // bob/dip/settle motion scale, 0..2
    // ЗВУК: линейные множители ДВУХ шин, 0..1 (Rule 14 — громкость линейная,
    // 1 = как записано). Ноль — полноценная ступень, а не край ползунка: «без
    // музыки» это способ играть, а не поломка, и его надо уметь ВЫБРАТЬ.
    float music_volume = 0.7f;
    float sfx_volume = 1.0f;
};

class MenuModel {
public:
    // THE MAP BROWSER'S DATA. Handed in by the app (which scanned the disk) and
    // only read here, so the menu stays testable without a filesystem: a test
    // builds a MapCatalog in memory and drives the pages. The pointer must
    // outlive the model (App owns both).
    void set_catalog(const MapCatalog* catalog) { catalog_ = catalog; }

    // THE CLOCK OF THE SCREENS, in seconds since the app started. The dust field
    // and the splash fade are functions of it and of nothing else -- no particle
    // state anywhere -- so the same second draws the same frame and a menu
    // screenshot is reproducible (Rule 13). The app ticks it once per menu frame.
    void tick(float dt_s) { time_ += dt_s; }
    [[nodiscard]] float time() const { return time_; }

    /// A PAGE THAT SAYS "NOT YET", BY NAME. The owner's rule for the start
    /// screen (26.08): a row whose system does not exist is still DRAWN, and
    /// pressing it lands here rather than doing nothing. A row that quietly
    /// ignores Enter is indistinguishable from a broken menu; a row that is
    /// hidden is indistinguishable from a feature nobody planned.
    /// `message_key` is a localization key (Rule 5), stored as its hash.
    void open_stub(std::string_view message_key);
    [[nodiscard]] uint64_t stub_message() const { return stub_message_; }

    // HOW LONG THE STUDIO FRAME STAYS UP. The app owns the number (it reads the
    // DFN_SPLASH door and it owns the clock); the page owns the fade, which is
    // computed from this and from time(). Zero is a legal value and means the
    // frame is skipped -- every unattended run takes that path, so no recipe in
    // the tree gets two extra seconds prepended to it.
    void set_splash_seconds(float seconds) {
        splash_seconds_ = seconds > 0.0f ? seconds : 0.0f;
    }
    [[nodiscard]] float splash_seconds() const { return splash_seconds_; }

    // Open the browser at its first level (categories). `target` is remembered
    // and returned by browse_target(), which is how the app knows whether the
    // chosen map should be played or flown.
    void open_browser(BrowseTarget target);
    // Descend to a category's map list directly (the DFN_MENU_PAGE=category_maps
    // door, so the SECOND browser level is photographable without a keyboard,
    // Rule 27). The app passes a valid category index; out-of-range is clamped
    // to 0 so the door never lands on a page that cannot draw.
    void open_category(size_t category_index);
    [[nodiscard]] BrowseTarget browse_target() const { return target_; }
    // Valid immediately after activate() returns OpenMap: the manifest chosen.
    [[nodiscard]] const MapManifest* chosen_map() const { return chosen_map_; }
    // For draw_menu: the catalog it browses and which category is open.
    [[nodiscard]] const MapCatalog* catalog() const { return catalog_; }
    [[nodiscard]] size_t chosen_category() const { return chosen_category_; }

    // A non-fatal browser message (e.g. a .dfw source with no baked file yet).
    // The app composes it from localization and hands it in; the browser draws
    // it and any navigation clears it. Empty = nothing to say.
    void set_browser_status(std::string text) { browser_status_ = std::move(text); }
    [[nodiscard]] const std::string& browser_status() const { return browser_status_; }

    void open(MenuPage page);
    [[nodiscard]] MenuPage page() const { return page_; }

    /// СТОИТ ЛИ ЭТА СТРАНИЦА НАД ЖИВЫМ МИРОМ. Пауза — да; настройки, открытые
    /// ИЗ паузы, — тоже да; те же настройки с корня — нет. Один вопрос, один
    /// ответ, и его задаёт музыка: заглавная тема принадлежит стартовому
    /// экрану, и запеть её поверх мира, который игрок поставил на паузу,
    /// значило бы объявить паузу вторым главным меню.
    ///
    /// ОТВЕТ ВЫЧИСЛЯЕТСЯ, А НЕ ХРАНИТСЯ. Модель УЖЕ помнит, куда возвращается
    /// каждая вложенная страница (settings_return_, calibrate_return_) — это и
    /// есть «откуда я сюда пришёл». Второй флаг рядом с этой цепочкой был бы её
    /// теневой копией и разошёлся бы с ней на первой же новой странице, у
    /// которой окажется два входа (правило 39).
    [[nodiscard]] bool over_world() const;
    [[nodiscard]] size_t selection() const { return selection_; }
    [[nodiscard]] size_t item_count() const;

    // Selection wraps: at the bottom, down goes to the top. A menu that dead-ends
    // reads as broken input.
    void move(int delta);
    /// Point AT a row -- what the mouse does. Out-of-range is ignored rather
    /// than clamped: "the pointer is on nothing" is a real answer (see
    /// menu_row_at), and clamping it would drag the selection to the last row
    /// every time the hand left the column.
    void set_selection(size_t row) {
        if (row < item_count()) {
            selection_ = row;
        }
    }
    [[nodiscard]] MenuAction activate();
    // Escape: from a sub-page it goes back, from the root it quits, from pause
    // it resumes. One key, no dead ends.
    [[nodiscard]] MenuAction back();

    // BRIGHTNESS FLOOR (the user's "minimum brightness"), in quantizer luma.
    // The menu owns it only while the calibration page is up: the app hands the
    // current value in, the player turns it, and the app reads it back out and
    // writes settings.cfg. Clamped to [0, BLACK_FLOOR_MAX] on both paths, so no
    // caller has to remember the range.
    void set_black_floor(float value);
    [[nodiscard]] float black_floor() const { return black_floor_; }

    // THE SETTINGS DRAFT, on the same loan as black_floor above: the app hands
    // in what it is running with, the player turns rows, and the app reads the
    // draft back on SettingsDone. set_settings() also records the values as
    // LAUNCHED, which is what needs_restart() answers against -- a page that
    // cannot say "this one lands next launch" is a page that looks broken
    // whenever the player picks a resolution and the picture does not change.
    void set_settings(const MenuSettings& value);

    /// ОДНА СТРОКА, ИЗМЕНЁННАЯ СНАРУЖИ СТРАНИЦЫ. Полный экран — единственная
    /// настройка с двумя органами управления (клавиша F11 и строка страницы), и
    /// страница обязана показывать то, что на экране, а не то, что на ней в
    /// прошлый раз выбрали.
    ///
    /// ПОЧЕМУ НЕ set_settings(). Тот перезаписывает и «с чем ЗАПУЩЕНЫ», то есть
    /// глушит needs_restart(): нажатие F11 стирало бы предупреждение «сетка
    /// рендера применится при следующем запуске», не отменив самой причины. Узкий
    /// вход трогает ровно то поле, у которого правда снаружи.
    void set_live_fullscreen(bool on) {
        settings_.fullscreen = on;
        launched_.fullscreen = on;
    }
    [[nodiscard]] const MenuSettings& settings() const { return settings_; }
    [[nodiscard]] bool needs_restart() const;

    // Left/right on the settings page: one press moves the selected row to its
    // next value (wrapping through the preset list, so there is no dead end at
    // either end). Enter on a value row does adjust(+1), which is why the page
    // is fully usable with the keys the app already routes.
    void adjust(int delta);

private:
    // The browser's data and where it is in it. catalog_ is borrowed (App owns
    // it); the two indices are only meaningful on the browser pages.
    const MapCatalog* catalog_ = nullptr;
    // Seconds since the app started, ticked by the app on every menu frame. The
    // only animation state the screens have; see tick() for why it is the only.
    float time_ = 0.0f;
    // Which "not yet" line the stub page shows, as a localization key hash.
    uint64_t stub_message_ = 0;
    // Long enough to read three words and see the mark, short enough that a
    // player who has launched the game fifty times does not learn to hate it.
    float splash_seconds_ = 2.2f;
    BrowseTarget target_ = BrowseTarget::Play;
    size_t chosen_category_ = 0;          // which category CategoryMaps lists
    const MapManifest* chosen_map_ = nullptr; // set on OpenMap
    std::string browser_status_;          // non-fatal message, drawn then cleared
    MenuPage page_ = MenuPage::Root;
    // Where Escape/Enter returns from the calibration page. It is reachable
    // from the root AND from settings, and a page that always returns to one
    // of its two callers loses the player's place in the other.
    MenuPage calibrate_return_ = MenuPage::Root;
    // Same question for the settings page, and it has a second caller for a
    // reason: PAUSE. The setting the player most wants mid-game is the one he
    // discovers he needs while standing in the dark, and a page that always
    // returns to the start screen would answer that by leaving the world.
    MenuPage settings_return_ = MenuPage::Root;
    size_t selection_ = 0;
    float black_floor_ = 0.0f;
    MenuSettings settings_{};
    MenuSettings launched_{};
};

// THE CALIBRATION SCREEN'S OWN NUMBERS, all expressed in the quantizer's ruler
// (PALETTE_SHADE_STEP_REF) rather than in arbitrary fractions, because that is
// the ruler the patches are drawn with.
//
// The ceiling is docs/NUMBERS.md BLACK_FLOOR_MAX (two steps) and the reason is
// measured: the falloff exponent that keeps the daylight frame still was
// derived at a ONE-step floor, and the shift it allows scales with the floor.
// Measured on the archived day frame, share of already-lit pixels moving by
// more than half a step: 0.00 % at one step, 25.96 % at one and a half,
// 49.36 % at two. So the top of the dial is where the day HAS begun to pay,
// and the player can see that happen rather than be quietly stopped early.
[[nodiscard]] float black_floor_max();
// One press of up/down. An eighth of a step: fine enough that the patch fades
// rather than jumps, coarse enough to cross the whole range in sixteen presses.
[[nodiscard]] float black_floor_adjust_step();

/// Where one selectable row is on the canvas, in canvas pixels. The box is the
/// CLICK TARGET, not the ink: it is padded to half the row gap, so the pointer
/// never falls between two rows and the selection never flickers as it crosses.
struct MenuRowBox {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    [[nodiscard]] bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

/// The boxes of the current page's rows, in draw order; empty on pages that have
/// no list (splash, credits, the stub, the calibration dial, the key list).
///
/// ONE ARITHMETIC FOR THE EYE AND FOR THE POINTER. draw_menu() lays its rows out
/// by calling this, and the app hit-tests the mouse against the same call. Two
/// copies would agree on the day they were written and disagree on the first day
/// a row changes size -- and the symptom of that is "the menu sometimes does not
/// take my click", which reads as a broken mouse rather than as a layout bug
/// (Rule 39).
[[nodiscard]] std::vector<MenuRowBox> menu_row_boxes(int canvas_w, int canvas_h,
                                                     const MenuModel& model);

/// The row under a canvas point, or model.item_count() when the point is on no
/// row at all -- which is a real answer and not a failure: hovering the
/// background must not move the selection.
[[nodiscard]] size_t menu_row_at(int canvas_w, int canvas_h, const MenuModel& model,
                                 int x, int y);

// Draws `model` into `canvas` (which the caller sized to the internal
// resolution). Opaque for Root/Maps, dimmed-world overlay for Pause.
void draw_menu(render::PixelCanvas& canvas, const MenuModel& model);

} // namespace dfn::app
