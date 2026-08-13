/*
Created: 10:08:2026 - 10:26:39
Last updated: 13:08:2026 - 19:40:00
Module: engine/app
File: engine/app/sources/Menu.h

Responsibility:
- The start screen and the in-game pause screen: what they contain, which item
  is selected, and how they draw. No world knowledge, no input polling -- the
  app feeds key edges in and reads an Action out, so the menu is testable
  without a window.

Key items:
- MapEntry: one selectable demo map (loc key + the stand it opens).
- MenuSettings: the settings.cfg rows the player can turn on the settings page.
- MenuModel: page + selection + the map list; move()/adjust()/activate() return
  Actions.
- draw_menu(): renders the current page into a PixelCanvas through BitmapFont.

Dependencies:
- Uses: engine/render (PixelCanvas, BitmapFont), engine/app Localization.
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
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

// One selectable demo map. `stand` is the worldgen stand id the app passes to
// core; 0 is the testbed valley that exists today. New stands (forest, river,
// sea, town, mirror) append here as core lands them -- the menu needs no code
// change, which is the point of the table.
struct MapEntry {
    uint32_t stand = 0;
    std::string name_key;  // localization key, never a literal
    std::string blurb_key; // one line under the title
};

enum class MenuPage : uint8_t {
    Root = 0,        // start screen
    Maps = 1,        // map picker
    Pause = 2,       // in-game
    Calibrate = 3,   // brightness calibration (Skyrim/Doom's first-run screen)
    Settings = 4,    // the settings.cfg rows, turnable without a text editor
};

enum class MenuAction : uint8_t {
    None = 0,
    EnterWorld, // `chosen_stand` carries which
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
    void set_maps(std::vector<MapEntry> maps);
    [[nodiscard]] const std::vector<MapEntry>& maps() const { return maps_; }

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

    [[nodiscard]] uint32_t chosen_stand() const { return chosen_stand_; }

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
    std::vector<MapEntry> maps_;
    MenuPage page_ = MenuPage::Root;
    // Where Escape/Enter returns from the calibration page. It is reachable
    // from the root AND from settings, and a page that always returns to one
    // of its two callers loses the player's place in the other.
    MenuPage calibrate_return_ = MenuPage::Root;
    size_t selection_ = 0;
    uint32_t chosen_stand_ = 0;
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
