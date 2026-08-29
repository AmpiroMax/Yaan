/*
Module: engine/app
File: engine/app/sources/HudScreen.cpp

Responsibility:
- The aiming mark's shape, and the reasoning for every number in it.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone ui owns this file.
*/

#include "engine/app/sources/HudScreen.h"
#include "engine/app/sources/AppDoors.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>

#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/app/sources/UiFont.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

namespace dfn::app {

namespace {

// THE INK is the interface's own, the same value the prompt and the readout
// are drawn in: the aiming mark is not a different kind of object from the
// words that appear next to it, and a second near-white would be a colour
// nobody chose.
constexpr render::Color INK{232, 228, 214};
// THE OUTLINE IS BLACK, one pixel all round. The mark is drawn over WHATEVER
// the player is facing -- sky, snow, torchlight -- and that is the same
// problem the text had, answered the way a LINE has to answer it. A plate
// cannot be the answer here: an opaque rectangle in the exact middle of the
// screen would cover the thing being aimed at, which is the one place on
// screen that must stay visible. The measurement is in
// artifacts/acceptance/README.md (ui-crosshair-*): ink-vs-covered fails the
// two-step rule over bright ground, ink-vs-what-it-abuts passes everywhere,
// because what it abuts is its own outline.
constexpr render::Color OUTLINE{0, 0, 0};

// THE HOLE IN THE MIDDLE, and it is the whole design. A dot marks the point
// and HIDES it: at 640x360 a distant door handle is a pixel, and a crosshair
// that covers the pixel you are aiming at answers "where am I pointing" by
// deleting the answer. Four ticks pointing inward name the same point and
// leave it visible. Morrowind's mark (reference frame 07) is this shape.
constexpr int GAP = 3;    // clear pixels between the centre and each tick
constexpr int TICK = 4;   // length of each tick, in internal pixels
/// Сколько пикселей между плашкой инструмента и нижним краем ИГРОВОГО ПОЛЯ.
/// Шесть, а не ноль: плашка со своей подложкой, прижатая вплотную, читается как
/// часть рамки интерфейса, а не как надпись поверх мира.
constexpr int BADGE_BOTTOM_GAP = 6;
constexpr int THICK = 1;  // one pixel: it is a mark, not a widget

// ---------------------------------------------------------------------------
// The direction ribbon
// ---------------------------------------------------------------------------

constexpr float PI_F = 3.14159265358979323846f;

// THE EIGHT MARKS, in the SAME ORDER as the readout's compass table
// (DebugOverlay.cpp, COMPASS_KEYS) and under the same convention: index i is
// the bearing i * 45 degrees, 0 = north = -Z, clockwise from above.
//
// WHY A SECOND TABLE OF KEYS AND NOT THE READOUT'S. The readout answers "which
// way am I facing" in PROSE -- "северо-восток" -- because it is read once, in a
// bug report. A ribbon is read at a glance and eight marks are on it at once,
// so it needs one and two letter marks; "северо-восток" on a 426 px strip would
// be a wall. The ORDER and the convention are shared and named in both files;
// the words are not, because they answer different questions.
constexpr const char* RIBBON_KEYS[8] = {
    "hud.compass.n",  "hud.compass.ne", "hud.compass.e",  "hud.compass.se",
    "hud.compass.s",  "hud.compass.sw", "hud.compass.w",  "hud.compass.nw",
};

// A tick every 15 degrees: three between neighbouring marks, which is enough
// for the eye to read the ribbon as MOVING when the player turns slowly. With
// marks alone a slow turn looks like a jump from letter to letter.
constexpr float TICK_STEP_RAD = PI_F / 12.0f;

constexpr render::Color RIBBON_TICK{150, 148, 140};
constexpr render::Color RIBBON_MARK{232, 228, 214};
// North is the one direction a compass exists to find, so it is the one mark
// drawn in the selected-item colour. Same value the menu's caret uses -- the
// interface has one "this one" colour, not two.
constexpr render::Color RIBBON_NORTH{244, 226, 160};

// ---------------------------------------------------------------------------
// The condition bars
// ---------------------------------------------------------------------------

// LENGTH, HEIGHT AND ORDER. 64x4 in internal pixels, stacked with a pixel of
// air, bottom left. The order is FIXED and it is the only thing that tells the
// three apart for a player who does not separate red from green: health on
// top, stamina under it, magicka last. Colour is a second signal, never the
// only one -- and the day a bar carries a number or an icon, it goes here.
constexpr int BAR_W = 64;
constexpr int BAR_H = 4;
constexpr int BAR_GAP = 2;
constexpr int BAR_MARGIN = 6;

// THE THREE COLOURS ARE SEPARATED IN BRIGHTNESS, NOT ONLY IN HUE, and the
// numbers are the project's own ruler: in quantizer luma (0.30/0.59/0.11) they
// sit at 0.289, 0.471 and 0.636 -- 2.3 and 2.1 steps apart, i.e. each pair
// clears the two-step rule WITHOUT any colour vision at all. The first pass had
// them at 0.330 / 0.400 / 0.496: red and blue were 0.9 of a step apart, which
// is a pair of bars that a red-green colour-blind player reads as one bar
// twice. Hue still says WHICH; brightness and order say it again.
constexpr render::Color BAR_HEALTH{152, 40, 40};
constexpr render::Color BAR_MAGICKA{88, 120, 208};
constexpr render::Color BAR_STAMINA{120, 196, 96};
// The unspent part of the bar, and the frame around it. The frame is the same
// black outline the crosshair uses, for the same reason: these sit over
// whatever the player is facing.
constexpr render::Color BAR_EMPTY{24, 24, 28};

[[nodiscard]] std::string_view loc(std::string_view key) {
    return localized(serialization::fnv1a64(key));
}

// Shortest signed angle from `from` to `to`, in (-pi, pi]. Written out rather
// than fmod'ed in place because the sign of std::fmod follows the DIVIDEND, and
// that is exactly the trap the readout's compass fell into once already.
[[nodiscard]] float angle_delta(float to, float from) {
    float d = to - from;
    while (d > PI_F) {
        d -= 2.0f * PI_F;
    }
    while (d <= -PI_F) {
        d += 2.0f * PI_F;
    }
    return d;
}

} // namespace

namespace {
// One reader for three doors: the rule "read ONCE, a door polled every frame is
// a switch" is the same for all of them, and three copies of it would be three
// chances to write it differently.
[[nodiscard]] bool door(const char* name) {
    const char* e = door_value(name);
    return !(e != nullptr && e[0] == '0');
}
} // namespace

bool crosshair_enabled() {
    static const bool on = door("DFN_CROSSHAIR");
    return on;
}

bool compass_ribbon_enabled() {
    static const bool on = door("DFN_HUD_RIBBON");
    return on;
}

bool condition_bars_enabled() {
    static const bool on = door("DFN_HUD_BARS");
    return on;
}

void hud_world_rect(const render::PixelCanvas& canvas, const HudFacts& facts, int& x,
                    int& y, int& w, int& h) {
    const int cw = static_cast<int>(canvas.width());
    const int ch = static_cast<int>(canvas.height());
    // NO RECT MEANS THE WHOLE CANVAS, and that default is the game: nothing is
    // docked in it, so every drawer here keeps behaving exactly as it did
    // before this field existed. A zero-sized rect is not a request for a
    // zero-sized world; it is the absence of a request.
    if (facts.world_w <= 0 || facts.world_h <= 0) {
        x = 0;
        y = 0;
        w = cw;
        h = ch;
        return;
    }
    x = std::clamp(facts.world_x, 0, cw);
    y = std::clamp(facts.world_y, 0, ch);
    w = std::clamp(facts.world_w, 0, cw - x);
    h = std::clamp(facts.world_h, 0, ch - y);
}

bool draw_crosshair(render::PixelCanvas& canvas, const HudFacts& facts) {
    if (!crosshair_enabled()) {
        return false;
    }
    // See the header for why this rule lives here and not at the call site.
    if (facts.third_person || facts.map_open) {
        return false;
    }
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    if (w <= 0 || h <= 0) {
        return false;
    }
    // THE CENTRE IS THE CENTRE OF THE CAMERA RAY, and on an even-sized grid
    // that falls BETWEEN pixels. Rounding down puts the mark half a pixel up
    // and left of true centre; the ticks are symmetric about the same rounded
    // point, so the shape stays symmetric even where the grid cannot be.
    //
    // AND IT IS THE CANVAS'S CENTRE, NOT THE WORLD RECT'S, on purpose — this
    // is the one element here that must NOT follow the editor's strips. The
    // strips take space from the PANELS' layout; they do not move the camera's
    // projection, which still fills the whole frame. A mark placed at the
    // centre of what is left would sit 216 px from the ray it names with the
    // parts column open (a 432.7-unit column on a 1920 canvas), and the ghost
    // would appear a wall's width away from the cross the builder aimed with.
    // A mark that lies about the ray is worse than a mark half covered.
    const int cx = w / 2;
    const int cy = h / 2;

    // Outline first, then ink over it: the outline is the ink's rectangle
    // grown by one pixel on every side, so no ink pixel is ever left abutting
    // the world directly. Written as two passes rather than a per-pixel
    // dilation because the shape is four rectangles and a dilation would be a
    // clever way to draw the same twelve rows.
    const struct {
        int x, y, w, h;
    } ticks[4] = {
        {cx - GAP - TICK, cy, TICK, THICK},          // left
        {cx + GAP + 1, cy, TICK, THICK},             // right
        {cx, cy - GAP - TICK, THICK, TICK},          // up
        {cx, cy + GAP + 1, THICK, TICK},             // down
    };
    for (const auto& t : ticks) {
        canvas.fill_rect(t.x - 1, t.y - 1, t.w + 2, t.h + 2, OUTLINE);
    }
    for (const auto& t : ticks) {
        canvas.fill_rect(t.x, t.y, t.w, t.h, INK);
    }
    return true;
}

bool draw_compass_ribbon(render::PixelCanvas& canvas, const HudFacts& facts) {
    if (!compass_ribbon_enabled()) {
        return false;
    }
    // The map carries north on its own plate, and the readout OWNS the top-left
    // and names the direction in words. In both cases the ribbon would be a
    // second answer drawn over the first.
    if (facts.map_open || facts.debug_readout) {
        return false;
    }
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    if (w <= 0 || h <= 0) {
        return false;
    }

    // THE STRIP SPANS THE CAMERA'S OWN HORIZONTAL FOV, derived from the vertical
    // one and the canvas's aspect -- the canvas IS the picture, so it cannot be
    // handed an aspect that disagrees with what is drawn on it.
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const float fov_x = 2.0f * std::atan(std::tan(facts.fov_y_rad * 0.5f) * aspect);
    // Two thirds of the width: wide enough that three or four marks are on it
    // at once (so it reads as a ribbon rather than as a label), narrow enough
    // to leave the top corners to the readout and to whatever comes next.
    int wx = 0;
    int wy = 0;
    int ww = 0;
    int wh = 0;
    hud_world_rect(canvas, facts, wx, wy, ww, wh);
    (void)wx;
    (void)ww;
    (void)wh;
    // THE STRIP KEEPS THE CANVAS'S HORIZONTAL GEOMETRY, for the same reason
    // the crosshair does: a mark's column is a claim about where that direction
    // is ON SCREEN, and the projection spans the whole frame. Only the vertical
    // position follows the interface — the ribbon has to step out from under a
    // docked toolbar, and nothing about that changes what a column means.
    const int ribbon_w = (w * 2) / 3;
    const int cx = w / 2;
    const int x_left = cx - ribbon_w / 2;
    const int y_top = wy + 4;
    // ЛЕНТА МЕРЯЕТСЯ ТЕМ ШРИФТОМ, КОТОРЫМ НАПИСАНЫ ЕЁ МЕТКИ. Холст интерфейса
    // стал 1920×1080 (UI_CANVAS_W, 27.08), и метка «СВ» блочным шрифтом
    // физически уменьшилась втрое на том же экране; ячейка 6×9 перестала
    // описывать высоту полосы.
    const int mark_px = ui_px(h, UiText::Small);
    const int mark_h = mark_px > 0 ? ui_cap_height(mark_px) : render::FONT_CELL_H;
    const int tick_y = y_top + mark_h + std::max(1, mark_h / 8);

    // The ground under the marks, on the same rule as every other text in this
    // interface: the ribbon lies across the SKY, which is the brightest thing
    // on screen, and pale letters on bright cloud is the defect this project
    // already measured once (artifacts/acceptance/README.md, the readout's plate).
    draw_text_plate(canvas, x_left, y_top, ribbon_w, tick_y - y_top + 3, /*pad=*/2);

    // Maps a world bearing to a column, and the mapping is the whole design:
    // the same fraction of the ribbon as of the screen.
    const auto column = [&](float bearing) {
        const float d = angle_delta(bearing, facts.yaw_rad);
        return cx + static_cast<int>(std::lround(
                        static_cast<double>(d / fov_x) * static_cast<double>(ribbon_w)));
    };

    // Ticks first, marks over them.
    const int steps = static_cast<int>(std::ceil(PI_F / TICK_STEP_RAD));
    for (int i = -steps; i <= steps; ++i) {
        const float bearing = facts.yaw_rad + static_cast<float>(i) * TICK_STEP_RAD;
        const int x = column(bearing);
        if (x <= x_left || x >= x_left + ribbon_w - 1) {
            continue;
        }
        canvas.fill_rect(x, tick_y, 1, 2, RIBBON_TICK);
    }
    for (int i = 0; i < 8; ++i) {
        const float bearing = static_cast<float>(i) * (PI_F / 4.0f);
        const std::string_view mark = loc(RIBBON_KEYS[i]);
        const int mw = mark_px > 0 ? ui_text_width(mark, mark_px)
                                   : render::text_width_px(mark);
        const int x = column(bearing) - mw / 2;
        // A mark is drawn only if it fits WHOLE: half a letter at the edge of a
        // compass reads as a different letter, and a compass that can be
        // misread is worse than one that shows one mark fewer.
        if (x < x_left + 1 || x + mw > x_left + ribbon_w - 1) {
            continue;
        }
        const render::Color ink = (i == 0) ? RIBBON_NORTH : RIBBON_MARK;
        if (mark_px > 0) {
            ui_draw_text(canvas, x, y_top, mark, ink, mark_px, /*shadow=*/true);
        } else {
            render::draw_text(canvas, x, y_top, mark, ink, /*shadow=*/true);
        }
    }
    return true;
}

bool draw_condition_bars(render::PixelCanvas& canvas, const HudFacts& facts) {
    if (!condition_bars_enabled()) {
        return false;
    }
    // Over the map the world is not what the player is acting in, and the bars
    // describe acting in it.
    if (facts.map_open) {
        return false;
    }
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    if (w < BAR_W + 2 * BAR_MARGIN || h <= 0) {
        return false;
    }
    const struct {
        float value;
        render::Color colour;
    } bars[3] = {
        {facts.health, BAR_HEALTH},
        {facts.stamina, BAR_STAMINA},
        {facts.magicka, BAR_MAGICKA},
    };
    const int block_h = 3 * BAR_H + 2 * BAR_GAP;
    int y = h - BAR_MARGIN - block_h;

    // THE SAME GROUND THE TEXT STANDS ON, and it is here because the frame
    // alone was measured and found wanting: over dark grass the black frame is
    // invisible, and then the fill abuts the world directly -- health came
    // within 0.86 of a step of what it covered, magicka within 1.42, both under
    // this project's two-step rule. The plate is the interface's one answer to
    // "this is drawn over whatever the player is facing" (Rule 39: one ground,
    // not a second copy of the rule), and unlike at the crosshair it costs
    // nothing here -- the bottom-left corner hides no target.
    draw_text_plate(canvas, BAR_MARGIN, y, BAR_W, block_h, /*pad=*/2);

    for (const auto& bar : bars) {
        // Frame first, then the empty channel, then the filled part: three
        // rectangles rather than a per-pixel loop, and the frame is what every
        // one of them abuts, so none of them ever touches the world directly.
        canvas.fill_rect(BAR_MARGIN - 1, y - 1, BAR_W + 2, BAR_H + 2, OUTLINE);
        canvas.fill_rect(BAR_MARGIN, y, BAR_W, BAR_H, BAR_EMPTY);
        const float v = std::clamp(bar.value, 0.0f, 1.0f);
        const int filled = static_cast<int>(std::lround(static_cast<double>(v) * BAR_W));
        if (filled > 0) {
            canvas.fill_rect(BAR_MARGIN, y, filled, BAR_H, bar.colour);
        }
        y += BAR_H + BAR_GAP;
    }
    return true;
}

/// ЗНАЧОК РЕЖИМА РЕДАКТОРА: жёлтый квадрат с карандашом, слева внизу игрового
/// поля. Размер и место выбраны так, чтобы он НЕ спорил с плашкой инструмента
/// (та по центру) и не залезал на полоски состояния (те в самом углу).
static void draw_editor_mode_badge(render::PixelCanvas& canvas, const HudFacts& facts) {
    if (!facts.editor_mode || facts.map_open) {
        return;
    }
    int wx = 0;
    int wy = 0;
    int ww = 0;
    int wh = 0;
    hud_world_rect(canvas, facts, wx, wy, ww, wh);
    constexpr int SIDE = 11;
    constexpr int GAP = 4;
    const int x0 = wx + ww - SIDE - GAP; // правый нижний угол игрового поля
    const int y0 = wy + wh - SIDE - GAP;
    const render::Color yellow{232, 196, 72};
    const render::Color ink{28, 26, 20};
    canvas.fill_rect(x0, y0, SIDE, SIDE, yellow);
    // КАРАНДАШ ПО ДИАГОНАЛИ: три пикселя жала, черта корпуса. Рисунок, а не
    // буква, потому что буква на 11 пикселях читается как грязь.
    for (int i = 2; i < SIDE - 2; ++i) {
        canvas.put(x0 + i, y0 + SIDE - 1 - i, ink);
        if (i + 1 < SIDE - 2) {
            canvas.put(x0 + i + 1, y0 + SIDE - 1 - i, ink);
        }
    }
    canvas.put(x0 + 2, y0 + SIDE - 3, ink);
    canvas.put(x0 + 3, y0 + SIDE - 2, ink);
}

bool draw_tool_badge(render::PixelCanvas& canvas, const HudFacts& facts) {
    draw_editor_mode_badge(canvas, facts);
    // NO NAME MEANS NO TOOL IN HAND, and that is every frame of the game. The
    // test is on the NAME rather than on a mode flag so this layer never has to
    // learn what an editor tool is: it is handed two sentences or nothing.
    if (facts.tool_name.empty() || facts.map_open) {
        return false;
    }
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    if (w <= 0 || h <= 0) {
        return false;
    }
    int wx = 0;
    int wy = 0;
    int ww = 0;
    int wh = 0;
    hud_world_rect(canvas, facts, wx, wy, ww, wh);

    // ВНИЗУ КАДРА, А НЕ У ПРИЦЕЛА (заказ 18.08: «текст поперёк экрана с описанием
    // тем, что я делаю — мусорно, пусть снизу этот текст будет нарисован»).
    //
    // ПРЕЖНИЙ ДОВОД БЫЛ НЕ ГЛУП И ВСЁ РАВНО НЕВЕРЕН, поэтому он записан, а не
    // стёрт: плашка отвечает на тот же вопрос, что и прицел, и потому стояла
    // рядом с ним. Не учтено было то, что она стоит там ВСЕГДА — а смотрят на
    // прицел, чтобы видеть ЗЕМЛЮ под ним, и две строки текста закрывали ровно
    // то место, ради которого туда и смотрят. Подсказка, которая читается один
    // раз, не должна занимать пиксели, которые нужны каждый кадр.
    //
    // Низ игрового поля, а не низ холста: с пристыкованной снизу полосой
    // интерфейса плашка иначе уехала бы под неё. Та же строка, на которой
    // стоит подсказка взаимодействия в теле, — одно место для «что сейчас
    // будет», а не два.
    const int cx = w / 2;
    // ПЛАШКА ИНСТРУМЕНТА — ТЕМ ЖЕ ШРИФТОМ И ТОЙ ЖЕ ЛЕСТНИЦЕЙ (27.08). Роль
    // Caption: это ответ на «что сейчас будет по щелчку», его читают в кадре и
    // мельче подписи меню он быть не должен.
    const int badge_px = ui_px(static_cast<int>(canvas.height()), UiText::Caption);
    const auto text_w = [&](std::string_view t) {
        return badge_px > 0 ? ui_text_width(t, badge_px) : render::text_width_px(t);
    };
    const int line_h = badge_px > 0 ? ui_line_height(badge_px) : render::FONT_CELL_H + 1;
    const int lines_ahead = facts.tool_action.empty() ? 1 : 2;
    int y = wy + wh - lines_ahead * line_h - BADGE_BOTTOM_GAP;

    const int name_w = text_w(facts.tool_name);
    const int act_w = facts.tool_action.empty() ? 0 : text_w(facts.tool_action);
    const int block_w = std::max(name_w, act_w);
    const int lines = lines_ahead;
    int x = cx - block_w / 2;
    // CLAMPED INTO WHAT IS LEFT, because this one CAN move without lying: the
    // badge names a state, it does not point at a place. With a column docked
    // on the right a centred block would run under it and lose its last word.
    x = std::clamp(x, wx + 2, std::max(wx + 2, wx + ww - block_w - 2));
    // Ниже игрового поля не уезжает и выше его верха не поднимается: на очень
    // низком кадре обе границы могут сойтись, и тогда верх выигрывает — текст,
    // срезанный сверху, ещё читается, срезанный снизу — нет.
    y = std::min(y, wy + wh - lines * line_h - 2);
    y = std::max(y, wy + 2);

    draw_text_plate(canvas, x, y, block_w, lines * line_h - 1);
    const auto put = [&](int tx, int ty, std::string_view t, render::Color c) {
        if (badge_px > 0) {
            ui_draw_text(canvas, tx, ty, t, c, badge_px, /*shadow=*/true);
        } else {
            render::draw_text(canvas, tx, ty, t, c, /*shadow=*/true);
        }
    };
    put(x, y, facts.tool_name, INK);
    if (!facts.tool_action.empty()) {
        // THE SAME TWO COLOURS THE GHOST'S EDGES USE, and they mean the same
        // thing: green is "this click lands", red is "it will not, and here is
        // why". One verdict said twice, never two verdicts — the sentence and
        // the outline in the world come from the one judge pass.
        const render::Color ok{120, 208, 120};
        const render::Color no{224, 108, 108};
        put(x, y + line_h, facts.tool_action, facts.tool_ready ? ok : no);
    }
    return true;
}

} // namespace dfn::app
