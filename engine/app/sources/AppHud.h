/*
Module: engine/app
File: engine/app/sources/AppHud.h

Responsibility:
- THE FRAME'S OVERLAY, COMPOSED IN ONE PLACE: facts in, canvas out. The pieces
  (compass, bars, crosshair, badge, prompt, readout, editor block, chat) each
  live in their own module already; what did NOT live anywhere was the
  ASSEMBLY -- the order they are drawn in, who stacks under whom, and whether
  anything was drawn at all.

Key items:
- ToolBadgeFacts / tool_badge(): what the badge under the crosshair says. Pure.
- HudFrame / compose_hud(): the assembly. Returns whether the layer is worth
  showing at all.

Dependencies:
- Uses: HudScreen, DebugOverlay, EditorHud, ChatOverlay, Localization, render's
  canvas and font. No App, no window, no ImGui.
- Used by: App.cpp (one call per frame), tests/app/HudScreenTests.cpp.

Notes:
- WHY THE ASSEMBLY IS THE PART WORTH MOVING. Every piece here was already in a
  module with its own suite, and the defects still landed in the frame: the
  readout and the editor banner printed through each other at (3,3) and (4,4)
  because each was pinned to a corner by its own arithmetic, and nothing could
  see the pair. The pair only exists in the assembly, and until today the
  assembly was 238 lines in the middle of App::run(), which owns a window.
- WHAT compose_hud() DELIBERATELY DOES NOT DO. It does not gather facts. Where
  the eye is looking, what the judge said about the ghost, what the renderer
  counted last frame -- all of that stays in App, because gathering needs the
  world and the world needs the window. The line between them is: a decision
  that can be wrong on its own comes here; a reading of live state stays there.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/

#pragma once

#include "engine/app/sources/ChatOverlay.h"
#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/EditorHud.h"
#include "engine/app/sources/HudScreen.h"

#include <string>
#include <string_view>

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

// WHAT THE BADGE UNDER THE CROSSHAIR IS TOLD. Deliberately plain types: the
// toolbox, the judge and the localization table all answer before this, so the
// decision below can be made -- and measured -- without any of them.
struct ToolBadgeFacts {
    bool editor = false;       // outside the editor there is no badge at all
    bool have_tool = false;    // is anything in the hand
    std::string_view title;    // the tool's name, already translated
    const char* status_key = nullptr;  // localization key for what it will do
    std::string_view status_text;      // ...or the judge's ready-made sentence
    bool ready = false;        // green: the click would do something
    bool wants_rotation = false;       // the hand that turns parts
    std::string_view group;    // the building being assembled, may be empty
    bool ui_wants_mouse = false;       // the pointer is over a panel
};

struct ToolBadge {
    bool shown = false;
    std::string name;
    std::string action;
    bool ready = false;
};

// WHAT THE HAND IS AND WHAT THE CLICK WOULD DO (заказ 17.08: «состояние на R
// меняется, но инструменты не рисуются, не понятно что сейчас я делаю и что»).
[[nodiscard]] ToolBadge tool_badge(const ToolBadgeFacts& f);

// EVERYTHING THE OVERLAY LAYER IS MADE OF THIS FRAME. A null pointer means
// "that piece is not up", which is why they are pointers and not flags beside
// values: a flag and a value can disagree.
struct HudFrame {
    HudFacts facts;                    // compass, bars, crosshair, world rect
    ToolBadgeFacts tool;               // the badge's inputs; decided in here
    std::string_view prompt;           // interaction verb, empty = none
    const DebugSnapshot* readout = nullptr;      // F3/2 readout, or nothing
    const EditorHudSnapshot* editor_block = nullptr; // editor banner, or none
    ChatOverlay* chat = nullptr;       // drawn last: it is what is being used
    bool probe = false;                // DFN_HUD_PROBE side-by-side hook
    // ЧИСТЫЙ КАДР ПО ТРЕБОВАНИЮ (DFN_HUD=0): ни компаса, ни полос, ни прицела,
    // ни отладочного блока. Нужен для кадров, которые СМОТРИТ ЧЕЛОВЕК — приёмка
    // и README, — где панель поверх картинки не информация, а мусор.
    //
    // ПОЛЕ, А НЕ ЧТЕНИЕ ДВЕРИ ВНУТРИ, и это разница между проверяемым и
    // непроверяемым: дверь читается один раз за прогон и защёлкивается, так что
    // рукав, который её не выставил ПЕРВЫМ, уже не сможет проверить эту ветку
    // вовсе. Дверь читает App (одной защёлкнутой строкой), а решение «рисовать
    // или нет» стоит здесь, где его видно.
    bool hud_off = false;
};

// Draws the whole layer onto `hud` (which the caller has cleared) and answers
// whether anything landed on it. False means the layer should not be shown --
// including the DFN_HUD=0 case, which is checked in here so that both hands of
// an acceptance pair come out of one binary.
[[nodiscard]] bool compose_hud(render::PixelCanvas& hud, const HudFrame& f);

} // namespace dfn::app
