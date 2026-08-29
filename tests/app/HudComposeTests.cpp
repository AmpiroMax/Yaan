/*
Module: tests/app
File: tests/app/HudComposeTests.cpp

Responsibility:
- Holds the ASSEMBLY of the overlay frame: what the badge under the crosshair
  says, that a clean frame is really clean, that the editor's block cannot
  print through the readout, and that the verification hook draws where the
  thing it verifies draws.

Dependencies:
- Uses: engine/app AppHud (+ HudScreen, DebugOverlay, EditorHud, Localization),
  doctest.
- Used by: ctest (app_hud_screen).

Notes:
- WHY THIS SITS ON app_hud_screen RATHER THAN A NEW TARGET. It is the same
  canvas and the same frame; the pieces' own suites (app_hud_screen for the
  ribbon, app_editor_hud for the block) each hold one piece, and this holds
  what happens when they are put together. A separate binary would let the
  halves of one claim be run apart, which is how a paired arm goes missing.
- THE OVERLAP CASE IS THE REASON THE LAYER WAS MOVED. The readout at (3,3) and
  the editor banner at (4,4) printed through each other for anyone running with
  both on. Both modules were correct alone and both suites were green; the
  defect existed only in the assembly, and the assembly was 238 lines inside
  App::run(), which owns a window. It is measured here by a stricter claim than
  "they do not overlap": ADDING THE BLOCK CHANGES NO PIXEL THE READOUT OWNS.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/AppHud.h"
#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

#include <string>
#include <string_view>
#include <vector>

using dfn::app::HudFrame;
using dfn::app::ToolBadge;
using dfn::app::ToolBadgeFacts;

namespace {

constexpr int CW = 640;
constexpr int CH = 360;

dfn::render::PixelCanvas blank() {
    dfn::render::PixelCanvas c;
    c.resize(CW, CH);
    c.clear_transparent();
    return c;
}

std::string_view say(const char* key) {
    return dfn::app::localized(dfn::serialization::fnv1a64(key));
}

struct LocalizationOnce {
    LocalizationOnce() {
        (void)dfn::app::load_localization(
            "games/daggerfall_n/assets/localization/ru.txt");
    }
};
const LocalizationOnce LOCALIZATION{};

std::size_t painted(const dfn::render::PixelCanvas& c) {
    const auto px = c.pixels();
    std::size_t n = 0;
    for (std::size_t i = 3; i < px.size(); i += 4) {
        n += px[i] != 0 ? 1 : 0;
    }
    return n;
}

// Painted pixels on one row. Used to compare two composites row by row, which
// is what makes "the block changed nothing above it" an exact claim rather than
// a bounding-box guess.
std::size_t painted_row(const dfn::render::PixelCanvas& c, int y) {
    const auto px = c.pixels();
    std::size_t n = 0;
    for (int x = 0; x < CW; ++x) {
        const std::size_t i = (static_cast<std::size_t>(y) * CW
                               + static_cast<std::size_t>(x)) * 4 + 3;
        n += px[i] != 0 ? 1 : 0;
    }
    return n;
}

bool rows_identical(const dfn::render::PixelCanvas& a,
                    const dfn::render::PixelCanvas& b, int y0, int y1) {
    const auto pa = a.pixels();
    const auto pb = b.pixels();
    for (int y = y0; y < y1; ++y) {
        const std::size_t at = static_cast<std::size_t>(y) * CW * 4;
        for (std::size_t i = 0; i < static_cast<std::size_t>(CW) * 4; ++i) {
            if (pa[at + i] != pb[at + i]) {
                return false;
            }
        }
    }
    return true;
}

dfn::app::DebugSnapshot demo_snapshot() {
    dfn::app::DebugSnapshot s;
    s.stand = 1;
    s.seed = 7;
    s.fps = 60.0f;
    s.frame_ms = 16.7f;
    s.position = {12.5f, 3.0f, -40.25f};
    s.yaw = 0.5f;
    s.pitch = -0.1f;
    s.chunks_resident = 9;
    s.lod_nodes = 46;
    return s;
}

} // namespace

TEST_CASE("the badge says what is in the hand, and says it in the editor only") {
    ToolBadgeFacts f;
    // OUTSIDE THE EDITOR THERE IS NO BADGE AT ALL. The flying eye has no reach,
    // and a verb under the crosshair of a body that is not there was reported
    // on the first cut as a ghost of the possessed player.
    CHECK_FALSE(dfn::app::tool_badge(f).shown);

    // РУКА ПУСТА — И ЭТО СОСТОЯНИЕ, А НЕ МОЛЧАНИЕ (заказ 18.08: «выбор
    // сбросится... я буду просто бегать по игре»).
    f.editor = true;
    const ToolBadge empty = dfn::app::tool_badge(f);
    CHECK(empty.shown);
    CHECK(empty.name == std::string(say("editor.tool.none")));
    CHECK(empty.action == std::string(say("tool.hint.empty")));
    CHECK_FALSE(empty.ready);

    // A tool in hand: the name is the tool's, the deed is its status KEY.
    f.have_tool = true;
    f.title = "Высота";
    f.status_key = "tool.hint.empty"; // any key that resolves
    f.ready = true;
    const ToolBadge held = dfn::app::tool_badge(f);
    CHECK(held.name == "Высота");
    CHECK(held.action == std::string(say("tool.hint.empty")));
    CHECK(held.ready);

    // THE JUDGE'S OWN SENTENCE BEATS THE KEY. A tool that has something
    // specific to say ("hovers above the ground") must not be flattened into
    // its generic hint -- that sentence is the whole answer to "why is it red".
    f.status_text = "висит над землёй";
    const ToolBadge judged = dfn::app::tool_badge(f);
    CHECK(judged.action == "висит над землёй");
}

TEST_CASE("the group is named only by the hand that builds, and only when ready") {
    ToolBadgeFacts f;
    f.editor = true;
    f.have_tool = true;
    f.title = "Постройка";
    f.status_key = "tool.hint.empty";
    f.group = "сарай";

    // THREE CONDITIONS, TRIED ONE AT A TIME. Написано разбором, а не одним
    // положительным случаем: три условия, проверенные вместе, зелены и для
    // кода, который смотрит только на одно из них.
    f.ready = false;
    f.wants_rotation = true;
    CHECK(dfn::app::tool_badge(f).action.find("сарай") == std::string::npos);

    f.ready = true;
    f.wants_rotation = false;
    CHECK(dfn::app::tool_badge(f).action.find("сарай") == std::string::npos);

    f.wants_rotation = true;
    f.group = {};
    CHECK(dfn::app::tool_badge(f).action.find("сарай") == std::string::npos);

    f.group = "сарай";
    CHECK(dfn::app::tool_badge(f).action.find("сарай") != std::string::npos);
}

TEST_CASE("a pointer over a panel overrides whatever the tool was saying") {
    // ПОКА УКАЗАТЕЛЬ НА ПАНЕЛИ, МИР НЕ ТРОГАЕТСЯ, и подпись говорит ровно это:
    // иначе щелчок по ползунку выглядит как проглоченный щелчок по земле.
    // Проверяется на ОБЕИХ руках -- с инструментом и без, -- потому что это
    // правда о следующем щелчке, а не о том, что в руке.
    ToolBadgeFacts f;
    f.editor = true;
    f.ui_wants_mouse = true;

    const ToolBadge bare = dfn::app::tool_badge(f);
    CHECK(bare.action == std::string(say("tool.hint.blocked")));
    CHECK_FALSE(bare.ready);

    f.have_tool = true;
    f.title = "Кисть";
    f.status_text = "подниму землю";
    f.ready = true;
    f.wants_rotation = true;
    f.group = "сарай";
    const ToolBadge held = dfn::app::tool_badge(f);
    CHECK(held.action == std::string(say("tool.hint.blocked")));
    CHECK_FALSE(held.ready);
    CHECK(held.name == "Кисть"); // ЧТО В РУКЕ он всё равно называет
}

TEST_CASE("a clean frame is clean: DFN_HUD=0 leaves the canvas untouched") {
    HudFrame f;
    f.hud_off = true;
    auto c = blank();
    CHECK_FALSE(dfn::app::compose_hud(c, f));
    // NOT ONE PIXEL. A canvas that was drawn on and then hidden differs from a
    // clean one by exactly the thing nobody can check, and this door exists for
    // frames a HUMAN looks at.
    CHECK(painted(c) == 0);

    // ...and the control, or "clean" would pass for a function that draws
    // nothing at all.
    f.hud_off = false;
    auto d = blank();
    CHECK(dfn::app::compose_hud(d, f));
    CHECK(painted(d) > 0);
}

TEST_CASE("the editor's block changes no pixel the readout owns") {
    // THE DEFECT: the readout at (3,3) and the editor banner at (4,4), pinned
    // to one corner by two separate arithmetics, printing through each other.
    // Both modules were right alone; only the assembly was wrong, and the
    // assembly lived where no instrument could reach it.
    const dfn::app::DebugSnapshot snap = demo_snapshot();
    dfn::app::EditorHudSnapshot ed;
    ed.fly_speed_mps = 12.0f;
    ed.frame_triangles = 1758233;
    ed.frame_draws = 412;
    ed.aim_hit = true;
    ed.aim_distance_m = 8.25f;

    HudFrame only_readout;
    only_readout.readout = &snap;
    auto a = blank();
    REQUIRE(dfn::app::compose_hud(a, only_readout));

    HudFrame both = only_readout;
    both.editor_block = &ed;
    auto b = blank();
    REQUIRE(dfn::app::compose_hud(b, both));

    // The readout says where it ended -- it publishes its own geometry
    // precisely so nobody has to guess -- and every row it owns must come out
    // BYTE FOR BYTE the same with the block added. Note the boundary: the
    // readout's own bottom, not the block's top. The block's plate starts
    // PLATE_PAD above its text, so comparing up to the text's top would leave
    // the three rows where an overlap would actually first appear unchecked.
    const int readout_bottom = dfn::app::debug_overlay_bottom_y(snap, 0);
    const int top = dfn::app::editor_hud_top_y(readout_bottom);
    CHECK(readout_bottom > 0);
    CHECK(top > readout_bottom);
    CHECK(rows_identical(a, b, 0, readout_bottom));

    // AND THE BLOCK REALLY DREW, or the case above would pass for a composer
    // that quietly ignored it.
    std::size_t block_ink = 0;
    for (int y = readout_bottom; y < CH; ++y) {
        block_ink += painted_row(b, y) - painted_row(a, y);
    }
    CHECK(block_ink > 0);
}

TEST_CASE("the verification hook draws where the thing it verifies draws") {
    // DFN_HUD_PROBE puts a real prompt and a deliberate MISS side by side, so
    // the placeholder is proved unmistakable rather than assumed to be. That
    // proof is worth nothing if the hook draws in a different place or a
    // different ink from the real prompt -- it would be verifying a path the
    // game does not take.
    HudFrame real;
    real.prompt = say("prompt.take");
    auto a = blank();
    REQUIRE(dfn::app::compose_hud(a, real));

    HudFrame probe;
    probe.probe = true;
    auto b = blank();
    REQUIRE(dfn::app::compose_hud(b, probe));

    // The real prompt's band carries ink in both composites.
    const int y = CH - 40;
    std::size_t real_band = 0;
    std::size_t probe_band = 0;
    for (int r = y; r < y + dfn::render::FONT_INK_H; ++r) {
        real_band += painted_row(a, r);
        probe_band += painted_row(b, r);
    }
    CHECK(real_band > 0);
    CHECK(probe_band > 0);

    // ...and the probe draws a SECOND line the real prompt does not, which is
    // the whole point of it: one hit and one miss to compare.
    std::size_t second_band = 0;
    for (int r = CH - 24; r < CH - 24 + dfn::render::FONT_INK_H; ++r) {
        second_band += painted_row(b, r);
    }
    CHECK(second_band > 0);
}
