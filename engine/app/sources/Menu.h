/*
Created: 10:08:2026 - 10:26:39
Last updated: 14:08:2026 - 17:51:15
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
*/

#pragma once

#include "engine/app/sources/MapCatalog.h"

#include <cstdint>
#include <string>
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
    uint32_t internal_w = 640;
    uint32_t internal_h = 360;
    uint32_t msaa = 4;       // 0/2/4/8 coverage samples on the internal grid
    bool palette = false;    // 64-colour quantization + dithering
    float head_bob = 1.0f;   // bob/dip/settle motion scale, 0..2
};

class MenuModel {
public:
    // THE MAP BROWSER'S DATA. Handed in by the app (which scanned the disk) and
    // only read here, so the menu stays testable without a filesystem: a test
    // builds a MapCatalog in memory and drives the pages. The pointer must
    // outlive the model (App owns both).
    void set_catalog(const MapCatalog* catalog) { catalog_ = catalog; }

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
    [[nodiscard]] size_t selection() const { return selection_; }
    [[nodiscard]] size_t item_count() const;

    // Selection wraps: at the bottom, down goes to the top. A menu that dead-ends
    // reads as broken input.
    void move(int delta);
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

// Draws `model` into `canvas` (which the caller sized to the internal
// resolution). Opaque for Root/Maps, dimmed-world overlay for Pause.
void draw_menu(render::PixelCanvas& canvas, const MenuModel& model);

} // namespace dfn::app
