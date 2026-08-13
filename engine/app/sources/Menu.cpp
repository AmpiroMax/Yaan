/*
Created: 10:08:2026 - 10:27:20
Last updated: 13:08:2026 - 18:52:00
Module: engine/app
File: engine/app/sources/Menu.cpp

Responsibility:
- MenuModel navigation and draw_menu's layout. See the header for why no
  literal user-facing string appears here.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 10:08:2026 - 10:27:20: Created.
- 13:08:2026 - 16:40:00: Плашка под текстом ЭКРАНА ПАУЗЫ (зона ui). Вуаль кроет
  ровно половину строк по построению, поэтому на незакрытых строках слова стоят на
  живом мире: замерено по архивному игровому кадру — 81.0 % чернил над светлым фоном
  ближе двух шагов квантователя к тому, что они кроют, и 80 кромок букв потеряно.
  Вуаль своей работы не теряет, она просто перестаёт быть тем, на чём стоит ТЕКСТ.
  Заодно один читатель строк вместо двух копий switch: плашку нельзя мерить по
  тексту, отличному от нарисованного.
- 13:08:2026 - 16:50:00: Дверь дозы DFN_UI_PLATE=0 — обе руки приёмки из ОДНОЙ сборки
  (правило 47, оговорка, заведённая ведущим сегодня). Тот же ключ читает DebugOverlay.cpp.
- 13:08:2026 - 17:06:00: Своя копия двери и своя плашка сняты — обе взяты из общей
  draw_text_plate(). Земля под текстом одна на весь интерфейс, и вторая копия правила
  была бы теневой (правило 39).
- 13:08:2026 - 18:52:00: СТРАНИЦА КАЛИБРОВКИ ЯРКОСТИ. Поле — настоящий чёрный, три
  квадрата в 0/1/2 шага квантователя НАД полем «как видно на стекле» (значения прогнаны
  через обратную кривую подъёма, иначе мишень занижена до 0.56 шага). Левый квадрат —
  КОНТРОЛЬ в ноль шагов: он неотличим от поля при любой настройке, и игрок, «видящий»
  все три, читает своё ожидание, а не экран.
*/

#include "engine/app/sources/Menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "engine/core/config/sources/Constants.h"

// For the shared text plate and its dose door. The pause page stands on the
// SAME ground as the readout because it is the same decision, and a second copy
// of a six-line getenv would be a shadow copy of a rule (Rule 39).
#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

namespace dfn::app {

namespace {

// Palette of the screens. Deliberately few values: the menu is text on a
// ground, and every extra tone is a decision nobody made.
constexpr render::Color BACKGROUND{18, 20, 26};
constexpr render::Color TITLE{232, 228, 214};
constexpr render::Color ITEM{176, 172, 160};
constexpr render::Color ITEM_SELECTED{244, 226, 160};
constexpr render::Color BLURB{120, 118, 112};
constexpr render::Color RULE_LINE{54, 56, 64};

// THE RULER, and everything on this page is expressed in it: one step is the
// quantizer's own cell, which is why a one-step difference is the number behind
// the words "почти сливается" -- at one step a difference MAY round into the
// same palette entry, and two steps is the project's guaranteed-separation rule.
constexpr float SHADE_STEP = static_cast<float>(config::PALETTE_SHADE_STEP_REF);

// THE LIFT CURVE, AND WHY A COPY OF IT LIVES HERE. The floor is applied on the
// GPU, in fs_upscale.sc, before the dither and the palette lookup. This page
// has to know the same curve to do the OPPOSITE sum: given a value we want to
// SEE on the glass, what must be drawn into the canvas so the lift lands it
// there. Without the inverse, the patch drawn "one step above the field" would
// arrive compressed -- 0.56 of a step at the shipping numbers -- and the
// screen would be calibrating against a target it misstates.
//
// THE SHADER IS THE AUTHORITY, and nothing here can enforce that -- a CPU test
// cannot run a fragment shader, so no assertion in this repository can catch
// the two drifting apart. What keeps them together is the one thing that can:
// both sides read the SAME exponent from docs/NUMBERS.md
// (BLACK_FLOOR_FALLOFF), and the curve is a single line in each, so a change
// has one shape to copy rather than an algorithm to re-derive. Said out loud
// because an unenforced pair that nobody names is how a shadow copy is born.
[[nodiscard]] float lift(float c, float floor_value) {
    const float k = static_cast<float>(config::BLACK_FLOOR_FALLOFF);
    return std::min(1.0f, c + floor_value * std::pow(std::max(1.0f - c, 0.0f), k));
}

// Canvas value that the lift puts at `target` on the glass. Bisection rather
// than algebra: the curve has no closed-form inverse for a general exponent,
// and 24 halvings of [0,1] land inside a 255th of a level, which is finer than
// the framebuffer can hold anyway.
[[nodiscard]] float unlift(float target, float floor_value) {
    float lo = 0.0f;
    float hi = 1.0f;
    for (int i = 0; i < 24; ++i) {
        const float mid = 0.5f * (lo + hi);
        if (lift(mid, floor_value) < target) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return 0.5f * (lo + hi);
}

[[nodiscard]] render::Color grey(float v) {
    const auto c = static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    return render::Color{c, c, c};
}

std::string_view loc(std::string_view key) {
    return localized(serialization::fnv1a64(key));
}

void draw_centered(render::PixelCanvas& canvas, int y, std::string_view text,
                   render::Color color) {
    const int x = (static_cast<int>(canvas.width()) - render::text_width_px(text)) / 2;
    render::draw_text(canvas, x, y, text, color, /*shadow=*/true);
}

} // namespace

float black_floor_max() { return static_cast<float>(config::BLACK_FLOOR_MAX); }
float black_floor_adjust_step() { return SHADE_STEP / 8.0f; }

void MenuModel::set_black_floor(float value) {
    black_floor_ = std::clamp(value, 0.0f, black_floor_max());
}

void MenuModel::set_maps(std::vector<MapEntry> maps) {
    maps_ = std::move(maps);
    if (selection_ >= item_count()) {
        selection_ = 0;
    }
}

void MenuModel::open(MenuPage page) {
    page_ = page;
    selection_ = 0;
}

size_t MenuModel::item_count() const {
    switch (page_) {
    case MenuPage::Root:
        return 3; // play, brightness, quit
    case MenuPage::Maps:
        return maps_.size() + 1; // maps + back
    case MenuPage::Pause:
        return 2; // resume, quit
    case MenuPage::Calibrate:
        return 0; // no list: up/down turn the dial itself
    }
    return 0;
}

void MenuModel::move(int delta) {
    // ON THE CALIBRATION PAGE UP AND DOWN ARE THE DIAL, not a selection, and
    // that is deliberate rather than lazy: it is the only page with nothing to
    // select, and reusing the keys the app already sends means the app needs no
    // new input mapping to drive it. Up is brighter, which is the only reading
    // of "up" anyone offers.
    if (page_ == MenuPage::Calibrate) {
        set_black_floor(black_floor_ - static_cast<float>(delta) * black_floor_adjust_step());
        return;
    }
    const size_t n = item_count();
    if (n == 0) {
        return;
    }
    const int cur = static_cast<int>(selection_);
    int next = (cur + delta) % static_cast<int>(n);
    if (next < 0) {
        next += static_cast<int>(n);
    }
    selection_ = static_cast<size_t>(next);
}

MenuAction MenuModel::activate() {
    switch (page_) {
    case MenuPage::Root:
        if (selection_ == 0) {
            open(MenuPage::Maps);
            return MenuAction::None;
        }
        if (selection_ == 1) {
            open(MenuPage::Calibrate);
            return MenuAction::None;
        }
        return MenuAction::Quit;
    case MenuPage::Maps:
        if (selection_ < maps_.size()) {
            chosen_stand_ = maps_[selection_].stand;
            return MenuAction::EnterWorld;
        }
        open(MenuPage::Root);
        return MenuAction::None;
    case MenuPage::Pause:
        return selection_ == 0 ? MenuAction::Resume : MenuAction::Quit;
    case MenuPage::Calibrate:
        // THE PAGE CLOSES ITSELF, and the action only asks the app to SAVE. A
        // page whose exit depends on the app handling a new action is a page
        // that traps the player the moment the handler is missing -- and the
        // handler landing later than the page is exactly the order these two
        // halves shipped in.
        open(MenuPage::Root);
        return MenuAction::CalibrationDone;
    }
    return MenuAction::None;
}

MenuAction MenuModel::back() {
    switch (page_) {
    case MenuPage::Root:
        return MenuAction::Quit;
    case MenuPage::Maps:
        open(MenuPage::Root);
        return MenuAction::None;
    case MenuPage::Pause:
        return MenuAction::Resume;
    case MenuPage::Calibrate:
        // Escape SAVES too. A brightness the player turned until he could see
        // and then lost by leaving the wrong way is worse than no dial.
        open(MenuPage::Root);
        return MenuAction::CalibrationDone;
    }
    return MenuAction::None;
}

// THE CALIBRATION PAGE (the user's request, and Skyrim's and Doom's first-run
// screen): a field at the game's TRUE black with three patches on it, and the
// player raises the floor until the middle one is barely there.
//
// WHY THREE, AND WHY THESE THREE. The patches sit at 0, 1 and 2 quantizer steps
// ABOVE the field AS SEEN ON THE GLASS (the canvas values are run back through
// the inverse of the lift, so the lift does not compress the target it is being
// judged by). One step is the ruler's own cell -- a one-step difference may
// round into the same palette entry, which is exactly what "почти сливается"
// means in numbers. Two steps is the project's guaranteed-separation rule, the
// same one the readout's plate was measured against.
//
// THE LEFT PATCH IS A CONTROL AND IT IS DRAWN AT ZERO STEPS, i.e. it is painted
// in the field's own colour and cannot be seen at any setting. It is here for
// the same reason every measurement in this project carries a control arm: a
// player who "sees" all three is reading his own expectation, and without the
// zero patch neither he nor we could tell that apart from a working eye. The
// digit under each square is what says where to look, so the invisible one is
// still locatable.
void draw_calibration(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    const float floor_value = model.black_floor();

    // TRUE black, not the menu's ground: the whole page is a statement about
    // what black looks like on this monitor, so it must not be tinted by ours.
    canvas.clear(render::Color{0, 0, 0});

    draw_centered(canvas, h / 8, loc("menu.calibrate.title"), TITLE);
    draw_centered(canvas, h / 8 + render::FONT_CELL_H * 2, loc("menu.calibrate.line1"), ITEM);
    draw_centered(canvas, h / 8 + render::FONT_CELL_H * 3 + 2, loc("menu.calibrate.line2"), BLURB);

    // Squares big enough that the eye judges a TONE rather than a thin edge:
    // a one-step difference on a 4 px sliver is a different perceptual task
    // than the one the setting is for.
    const int patch = std::max(w / 10, 24);
    const int gap = patch / 2;
    const int total = 3 * patch + 2 * gap;
    const int x0 = (w - total) / 2;
    const int y0 = h / 2 - patch / 4;
    for (int i = 0; i < 3; ++i) {
        const float target = lift(0.0f, floor_value) + static_cast<float>(i) * SHADE_STEP;
        const int px = x0 + i * (patch + gap);
        canvas.fill_rect(px, y0, patch, patch, grey(unlift(target, floor_value)));
        char label[8];
        std::snprintf(label, sizeof(label), "%d", i);
        render::draw_text(canvas, px + (patch - render::text_width_px(label)) / 2,
                          y0 + patch + 4, label, BLURB, /*shadow=*/true);
    }

    // The number, in the ruler the page is built on: steps first, because that
    // is what the patches mean, and the raw value second for the settings file.
    char value[48];
    std::snprintf(value, sizeof(value), "%.2f  (%.3f)",
                  static_cast<double>(floor_value / SHADE_STEP),
                  static_cast<double>(floor_value));
    const std::string_view level = loc("menu.calibrate.level");
    const int vw = render::text_width_px(level) + render::FONT_CELL_W
                 + render::text_width_px(value);
    const int vx = (w - vw) / 2;
    const int vy = y0 + patch + render::FONT_CELL_H * 2 + 6;
    render::draw_text(canvas, vx, vy, level, BLURB, true);
    render::draw_text(canvas, vx + render::text_width_px(level) + render::FONT_CELL_W, vy,
                      value, ITEM_SELECTED, true);

    draw_centered(canvas, h - render::FONT_CELL_H * 2 - 4, loc("menu.calibrate.keys"), BLURB);
}

void draw_menu(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    const bool pause = model.page() == MenuPage::Pause;

    // One reader for the rows, used TWICE: once to measure the block and once
    // to draw it. Two copies of this switch would be two chances for the plate
    // to be sized for text that is not the text drawn.
    struct Row {
        std::string_view label;
        std::string_view blurb;
    };
    const auto row_at = [&](size_t i) -> Row {
        switch (model.page()) {
        case MenuPage::Root:
            if (i == 0) {
                return {loc("menu.play"), {}};
            }
            return {(i == 1) ? loc("menu.calibrate") : loc("menu.quit"), {}};
        case MenuPage::Maps:
            if (i < model.maps().size()) {
                return {loc(model.maps()[i].name_key), loc(model.maps()[i].blurb_key)};
            }
            return {loc("menu.back"), {}};
        case MenuPage::Pause:
            return {(i == 0) ? loc("menu.resume") : loc("menu.quit"), {}};
        case MenuPage::Calibrate:
            return {}; // drawn by draw_calibration, which has no list
        }
        return {};
    };

    canvas.resize(canvas.width(), canvas.height());
    if (model.page() == MenuPage::Calibrate) {
        draw_calibration(canvas, model);
        return;
    }
    if (pause) {
        // The pause screen sits OVER the world: a dim veil, not a wall, so the
        // player can see where they left off.
        canvas.clear_transparent();
        for (int y = 0; y < h; ++y) {
            if ((y & 1) == 0) {
                canvas.fill_rect(0, y, w, 1, BACKGROUND);
            }
        }
    } else {
        canvas.clear(BACKGROUND);
    }

    const int title_y = pause ? h / 4 : h / 5;
    const int rule_y = title_y + render::FONT_CELL_H + 4;

    // Items start below the rule; the blurb line under each map costs one row,
    // so map rows are spaced two rows apart and plain rows one.
    const bool maps_page = model.page() == MenuPage::Maps;
    const int row = render::FONT_CELL_H + (maps_page ? 6 : 4);
    const int first_item_y = title_y + render::FONT_CELL_H + 16;
    const size_t n = model.item_count();

    // THE PAUSE PLATE. The veil is a HALF cover by construction -- it fills
    // every other scanline -- so on the rows it does not fill, the words stand
    // on the live world. Measured on the archived playing frame: 81.0 % of the
    // ink over bright background sat closer than 2 * PALETTE_SHADE_STEP_REF to
    // what it covered, and 80 glyph edges were lost outright. The veil keeps
    // its job (you can see where you left off); it just stops being what the
    // TEXT stands on. Only this page gets a plate: root and maps already clear
    // opaque, so there the plate would be a frame drawn around nothing.
    if (pause) {
        int block_w = render::text_width_px(loc("menu.paused"));
        for (size_t i = 0; i < n; ++i) {
            // The caret hangs two cells to the left of the widest label, so it
            // is part of the block whether or not this row is the selected one.
            block_w = std::max(block_w,
                               render::text_width_px(row_at(i).label)
                                   + render::FONT_CELL_W * 4);
        }
        block_w = std::max(block_w, w / 2); // the rule under the title
        int block_h = first_item_y - title_y;
        for (size_t i = 0; i < n; ++i) {
            block_h += row;
        }
        // A block of lines wants more air than a single line, which is the one
        // thing the shared plate takes as an argument.
        draw_text_plate(canvas, (w - block_w) / 2, title_y, block_w, block_h, /*pad=*/6);
    }

    draw_centered(canvas, title_y, pause ? loc("menu.paused") : loc("app.title"), TITLE);
    canvas.fill_rect(w / 4, rule_y, w / 2, 1, RULE_LINE);

    int y = first_item_y;
    for (size_t i = 0; i < n; ++i) {
        const bool sel = (i == model.selection());
        const render::Color color = sel ? ITEM_SELECTED : ITEM;
        const Row r = row_at(i);

        // The caret is what makes the selection readable at 640x360 -- colour
        // alone is a shade step, and a shade step is the weakest signal we have.
        const int label_w = render::text_width_px(r.label);
        const int x = (w - label_w) / 2;
        if (sel) {
            render::draw_text(canvas, x - render::FONT_CELL_W * 2, y, ">", ITEM_SELECTED,
                              true);
        }
        render::draw_text(canvas, x, y, r.label, color, /*shadow=*/true);
        y += render::FONT_CELL_H + 2;
        if (!r.blurb.empty()) {
            draw_centered(canvas, y, r.blurb, BLURB);
            y += render::FONT_CELL_H;
        }
        y += row - render::FONT_CELL_H - 2;
    }

    // The control hint. On the pause page it stands on the world like the rest,
    // so it gets the same treatment as the block above -- a plate its own size.
    const std::string_view hint = loc("menu.hint");
    const int hint_y = h - render::FONT_CELL_H * 2 - 4;
    if (pause) {
        const int hw = render::text_width_px(hint);
        draw_text_plate(canvas, (w - hw) / 2, hint_y, hw, render::FONT_INK_H);
    }
    draw_centered(canvas, hint_y, hint, BLURB);
}

} // namespace dfn::app
