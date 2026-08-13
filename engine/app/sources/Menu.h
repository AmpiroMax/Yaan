/*
Created: 10:08:2026 - 10:26:39
Last updated: 13:08:2026 - 18:45:00
Module: engine/app
File: engine/app/sources/Menu.h

Responsibility:
- The start screen and the in-game pause screen: what they contain, which item
  is selected, and how they draw. No world knowledge, no input polling -- the
  app feeds key edges in and reads an Action out, so the menu is testable
  without a window.

Key items:
- MapEntry: one selectable demo map (loc key + the stand it opens).
- MenuModel: page + selection + the map list; move()/activate() return Actions.
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

private:
    std::vector<MapEntry> maps_;
    MenuPage page_ = MenuPage::Root;
    size_t selection_ = 0;
    uint32_t chosen_stand_ = 0;
    float black_floor_ = 0.0f;
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
