/*
Created: 14:08:2026 - 18:57:03
Last updated: 14:08:2026 - 19:39:08
Module: tests/app
File: tests/app/EditorHudTests.cpp

Responsibility:
- Proves the two overlays that share the top-left corner -- the debug readout
  and the editor's own block -- can both be read AT THE SAME TIME, which is the
  state the user actually works in, and that neither runs off a 640x360 frame.

Dependencies:
- Uses: engine/app EditorHud + DebugOverlay (layout only -- no canvas, no
  window, no renderer), engine/render BitmapFont for the ruler, doctest.
- Used by: ctest.

Notes:
- THE MEASUREMENT IS THE POINT. The defect these cases exist for was invisible
  to every test in the repository and perfectly visible to anyone who launched
  the game: the readout drew at (3, 3) and the editor banner at (4, 4), so with
  both switched on they printed through each other. Nothing was wrong with
  either block; what was wrong was that two files each believed they owned the
  corner, and no instrument compared them. So these cases compare EXTENTS, in
  pixels, against the same font the frame is drawn with.
- AND THE VANTAGE IS CHOSEN WHERE THE PROPERTY VARIES (Rule 27). The readout is
  not a fixed height -- it grows a row in water -- so every geometric case is
  run BOTH dry and wading. A separation checked only on dry land is a check
  that passes on the frame where the bug cannot appear.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 14:08:2026 - 18:57:03: Создан вместе с модулем EditorHud — разведение
  редакторского блока и отладочного вывода по жалобе пользователя.
- 14:08:2026 - 19:05:56: Случай про формулировки. Приёмка тут не «влезло» —
  старое «трис 1758233» влезало прекрасно, — а «строка называет, ЧТО посчитано».
  Контроль обязателен и он неочевиден: короткие формы существуют, поэтому набор,
  проверяющий только 640, зелен и для модуля, который сокращает ВСЕГДА. Поэтому
  те же числа спрашиваются ещё и на 320 и обязаны вернуться сокращёнными.
- 14:08:2026 - 19:39:08: Случай про номер объекта. Проверяется и сама инверсия, и
  СТРОКА, которую рисует кадр: верный обратный ход в отрыве ничего не значит,
  если строка по-прежнему форматирует сырое поле. Контроль — сентинел: объект
  не показывается вовсе, а не показывается неправильно.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/EditorHud.h"
#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"

#include <algorithm>
#include <string>
#include <vector>

using dfn::app::DebugSnapshot;
using dfn::app::EditorHudSnapshot;

namespace {

// THE REAL TABLE, NOT THE PLACEHOLDERS. A missing key resolves to "?<0x...>?",
// which is both longer than any word we ship and not the text the frame draws
// -- so a width suite run without the table measures the miss handler. Loaded
// once; ctest runs every test from the repo root, which is what makes the
// relative path the same for everyone.
struct LoadedStrings {
    LoadedStrings() {
        loaded = dfn::app::load_localization(
            "games/daggerfall_n/assets/localization/ru.txt");
    }
    bool loaded = false;
};
const LoadedStrings& strings() {
    static const LoadedStrings s;
    return s;
}

// THE INTERNAL RESOLUTIONS THE SETTINGS PAGE OFFERS. 640x360 is what the user
// runs and what the task named; 320x180 is the rung one keypress away, and it
// is the one that catches a line that is merely "short enough" rather than
// measured -- exactly how the settings page's own text ran off both edges.
constexpr int W_640 = 640;
constexpr int H_360 = 360;
constexpr int W_320 = 320;
constexpr int H_180 = 180;

// THE WORST CASE THE BLOCK CAN BE ASKED TO DRAW, not a typical one. Every
// optional part is present and every number is at its widest: a check run
// against a comfortable sample measures the sample.
EditorHudSnapshot widest_editor() {
    EditorHudSnapshot s;
    s.fly_speed_mps = 8888.8f;
    s.frame_triangles = 4294967295u; // uint32 max: ten digits
    s.frame_draws = 4294967295u;
    s.wireframe = true;              // the optional "[каркас]" tag
    s.aim_hit = true;                // the long branch of line 3
    s.aim_triangles = 4294967295u;
    s.aim_distance_m = 88888.8f;
    s.aim_pick_id = 4294967295u;     // the optional id run
    return s;
}

// The readout at its TALLEST: in water it carries one row more than on land.
DebugSnapshot wading() {
    DebugSnapshot s;
    s.water_depth = 1.0f;
    return s;
}

DebugSnapshot dry() {
    DebugSnapshot s;
    s.water_depth = 0.0f;
    return s;
}

int widest_line_px(const std::vector<std::string>& lines) {
    int w = 0;
    for (const std::string& l : lines) {
        w = std::max(w, dfn::render::text_width_px(l));
    }
    return w;
}

} // namespace

TEST_CASE("the editor block starts below the readout, wet or dry") {
    // The two blocks are laid out in sequence, so the editor's first line
    // begins strictly after the readout's plate ends -- with air between them,
    // not merely without overlap.
    for (const DebugSnapshot& snap : {dry(), wading()}) {
        const int readout_bottom = dfn::app::debug_overlay_bottom_y(snap);
        const int editor_top = dfn::app::editor_hud_top_y(readout_bottom);
        CHECK(editor_top > readout_bottom);
    }

    // ...AND THE WATER ROW MOVES IT. This is the control that the gap is
    // computed rather than pinned: a block placed at a constant y would pass
    // the check above on both snapshots and still land on the readout the
    // moment the player waded in, because the readout got taller and it did
    // not. The two tops must DIFFER, by exactly one row of the readout.
    const int dry_top = dfn::app::editor_hud_top_y(dfn::app::debug_overlay_bottom_y(dry()));
    const int wet_top =
        dfn::app::editor_hud_top_y(dfn::app::debug_overlay_bottom_y(wading()));
    CHECK(wet_top > dry_top);
    CHECK(wet_top - dry_top == dfn::render::FONT_CELL_H + 1);
}

TEST_CASE("both blocks fit inside the frame at every internal resolution") {
    REQUIRE(strings().loaded); // measuring "?<0x...>?" would measure nothing
    const EditorHudSnapshot ed = widest_editor();
    struct Res {
        int w;
        int h;
    };
    // THE EXTENT MEASURED IS THE PLATE'S, NOT THE INK'S. The plate is what is
    // actually painted, it is wider than the text it carries by its margin on
    // each side, and a suite that measures the ink signs off on a panel whose
    // edge is already outside the frame.
    const int pad = 3; // draw_text_plate's default margin

    for (const Res r : {Res{W_640, H_360}, Res{W_320, H_180}}) {
        CAPTURE(r.w);
        // NARROWED FOR THIS FRAME, then measured. Asking for the lines at one
        // width and checking them against another would test nothing.
        const std::vector<std::string> lines = dfn::app::editor_hud_lines(ed, r.w);
        REQUIRE_FALSE(lines.empty());
        const int block_w = widest_line_px(lines);
        const int block_h = dfn::app::editor_hud_block_height_px(lines.size());
        // HORIZONTALLY: measured with the same text_width_px the frame draws
        // with, on the same assembled strings -- not on a guess about how long
        // a translated word is.
        CHECK(dfn::app::editor_hud_x() + block_w + pad <= r.w);

        // VERTICALLY, IN THE STATE THAT IS TALLEST: the readout in water (one
        // row more than on land), the editor block under it, and the capture
        // hint the readout pins to the bottom row. All three coexist or the
        // corner is broken again in a different place.
        const int top = dfn::app::editor_hud_top_y(dfn::app::debug_overlay_bottom_y(wading()));
        CHECK(top + block_h <= r.h);
        CHECK(top + block_h <= dfn::app::debug_overlay_hint_top_y(r.h));
    }
}

TEST_CASE("at the resolution he plays, the lines are sentences and not mnemonics") {
    REQUIRE(strings().loaded);

    // THE COMPLAINT THIS ANSWERS was "с текстом трисс (что это такое)", so the
    // acceptance is not "it fits" -- the old "трис 1758233" fitted fine. It is
    // that at 640x360, the resolution he actually runs, the line SAYS which
    // count it is: the whole frame, or the one object under the crosshair.
    EditorHudSnapshot s;
    s.fly_speed_mps = 8.0f;
    s.frame_triangles = 1758233;
    s.frame_draws = 122;
    s.aim_hit = true;
    s.aim_triangles = 1476;
    s.aim_distance_m = 0.8f;
    s.aim_pick_id = 42;

    const std::vector<std::string> full = dfn::app::editor_hud_lines(s, W_640);
    REQUIRE(full.size() == 3);
    // The frame line names the frame; the aim line names the crosshair. Asserted
    // through localization rather than against a literal, because a literal here
    // would be the Rule 5 violation this module exists to avoid -- and it would
    // pass just as well if the table were never loaded.
    const auto has = [](const std::string& line, const char* key) {
        const std::string_view want =
            dfn::app::localized(dfn::serialization::fnv1a64(key));
        return line.find(std::string(want)) != std::string::npos;
    };
    CHECK(has(full[1], "editor.hud.frame"));
    CHECK(has(full[1], "editor.hud.draws"));
    CHECK(has(full[2], "editor.hud.aim"));
    CHECK(has(full[2], "editor.hud.distance")); // "where the distance", his words
    CHECK(has(full[2], "editor.hud.object"));

    // THE CONTROL, AND IT IS THE ONE THAT MATTERS: the short forms exist, so a
    // suite that only checks 640 would pass a module that ALWAYS abbreviates.
    // At 320x180 the same numbers must come back shortened -- proving the two
    // tiers are really two, and that the choice is made by measuring.
    const std::vector<std::string> narrow = dfn::app::editor_hud_lines(s, W_320);
    REQUIRE(narrow.size() == 3);
    CHECK(narrow[1] != full[1]);
    CHECK(narrow[2] != full[2]);
    CHECK_FALSE(has(narrow[1], "editor.hud.frame"));
    CHECK_FALSE(has(narrow[2], "editor.hud.aim"));

    // ...and the miss branch is a sentence too, not a bare "пусто".
    EditorHudSnapshot miss;
    miss.aim_hit = false;
    const std::vector<std::string> none = dfn::app::editor_hud_lines(miss, W_640);
    REQUIRE(none.size() == 3);
    CHECK(has(none[2], "editor.hud.aim.none"));
}

TEST_CASE("the block's reported height is the height it draws") {
    // THE CONTROL FOR EVERY GEOMETRIC CLAIM ABOVE. All of them are made against
    // editor_hud_block_height_px(), so if that number were not the number the
    // draw produces, every case here would be green about the wrong rectangle.
    // Compared against the pitch the draw loop actually advances by.
    const int row = dfn::app::editor_hud_row_h();
    CHECK(row > dfn::render::FONT_INK_H); // rows must not print into each other

    CHECK(dfn::app::editor_hud_block_height_px(0) == 0);
    for (size_t n = 1; n <= 5; ++n) {
        const int h = dfn::app::editor_hud_block_height_px(n);
        // n lines span (n-1) pitches plus one line of ink, plus the plate's
        // margin above the first and below the last.
        CHECK(h == static_cast<int>(n - 1) * row + dfn::render::FONT_INK_H + 6);
        if (n > 1) {
            CHECK(h > dfn::app::editor_hud_block_height_px(n - 1));
        }
    }
}

TEST_CASE("the readout's own height tracks its rows") {
    // The editor block is positioned from this number, so it is worth one case
    // of its own. The wading readout is exactly one row taller than the dry
    // one -- and the CONTROL is that two dry snapshots agree, which is what
    // fails if the height ever starts depending on something it should not.
    const int a = dfn::app::debug_overlay_bottom_y(dry());
    DebugSnapshot other = dry();
    other.fps = 999.0f;
    other.position = {1234.5f, -99.0f, 4321.0f};
    const int b = dfn::app::debug_overlay_bottom_y(other);
    CHECK(a == b); // the CONTENT of a row does not change how many rows there are

    CHECK(dfn::app::debug_overlay_bottom_y(wading()) - a
          == dfn::render::FONT_CELL_H + 1);
}

TEST_CASE("fits_width narrows by measuring, and only when it must") {
    const std::string_view long_line = "a string that is quite definitely too long";
    const std::string_view short_line = "short";

    // Wide enough: the full line survives.
    CHECK(dfn::app::fits_width(W_640, long_line, short_line) == long_line);
    // Narrow: it falls back.
    CHECK(dfn::app::fits_width(40, long_line, short_line) == short_line);

    // THE CONTROL, and it is the boundary rather than a comfortable pair: a
    // line whose ink ENDS on the last pixel column reads as clipped even when
    // it is whole, so the rule demands a cell of air on each side. At exactly
    // that width the full line must still be chosen; one pixel narrower it must
    // not. Without this, a rule that kept no margin at all would pass both
    // checks above.
    const int exact = dfn::render::text_width_px(long_line) + 2 * dfn::render::FONT_CELL_W;
    CHECK(dfn::app::fits_width(exact, long_line, short_line) == long_line);
    CHECK(dfn::app::fits_width(exact - 1, long_line, short_line) == short_line);
}

TEST_CASE("the object number under the crosshair is the entity, not the stamp") {
    REQUIRE(strings().loaded);

    // engine/render stamps pick_id = EntityId.index + 1 so that entity slot 0 --
    // a real slot -- cannot collide with the contract's "0 = unnamed" sentinel.
    // The overlay printed the stamp raw, so every object in the world was named
    // one higher than it is. Nothing about that looks wrong on screen, which is
    // exactly why it needed a test rather than a second pair of eyes.
    uint32_t index = 0;
    CHECK(dfn::app::aim_entity_index(1u, index));
    CHECK(index == 0u); // THE SLOT THE +1 EXISTS FOR: stamp 1 is entity 0
    CHECK(dfn::app::aim_entity_index(43u, index));
    CHECK(index == 42u);

    // THE SENTINEL IS NOT AN ENTITY. Terrain, sky and the LOD nodes all submit
    // unnamed, and this is the case that must not return a number at all --
    // under-flowing to 4294967295 or naming slot 0 would both be a confident
    // lie about something nobody is looking at.
    index = 12345u;
    CHECK_FALSE(dfn::app::aim_entity_index(0u, index));
    CHECK(index == 12345u); // untouched: the caller's value is not clobbered

    // ...AND IT REACHES THE DRAWN LINE. The inversion being right in isolation
    // proves nothing if the line still formats the raw field, so the assertion
    // is made against the string the frame draws.
    EditorHudSnapshot s;
    s.aim_hit = true;
    s.aim_triangles = 1476;
    s.aim_distance_m = 0.8f;
    s.aim_pick_id = 43u; // entity 42
    const std::vector<std::string> lines = dfn::app::editor_hud_lines(s, W_640);
    REQUIRE(lines.size() == 3);
    CHECK(lines[2].find(" 42") != std::string::npos);
    CHECK(lines[2].find(" 43") == std::string::npos);

    // The unnamed draw shows no object at all rather than a wrong one -- the
    // control for the line above, and the common case: the crosshair spends
    // most of its time on terrain.
    s.aim_pick_id = 0u;
    const std::vector<std::string> unnamed = dfn::app::editor_hud_lines(s, W_640);
    REQUIRE(unnamed.size() == 3);
    const std::string_view object_word =
        dfn::app::localized(dfn::serialization::fnv1a64("editor.hud.object"));
    CHECK(unnamed[2].find(std::string(object_word)) == std::string::npos);
}
