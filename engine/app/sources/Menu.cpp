/*
Created: 10:08:2026 - 10:27:20
Last updated: 13:08:2026 - 16:50:00
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
*/

#include "engine/app/sources/Menu.h"

#include <algorithm>
#include <cstdlib>

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

// THE DOSE DOOR (Rule 47, the one-binary clause). `DFN_UI_PLATE=0` draws the
// pause page's words with NO plate under them -- what shipped before c4c63e2 --
// so the before arm and the after arm of the acceptance come out of the SAME
// binary. In a shared tree, a before/after across two builds measures the day's
// other zones. The same name is read by DebugOverlay.cpp: one door, one meaning
// ("text without its ground"), wherever the interface draws text. Read once,
// because an instrument that can change mid-run is not an instrument.
[[nodiscard]] bool plates_enabled() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_UI_PLATE");
        return !(e != nullptr && e[0] == '0');
    }();
    return on;
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
        return 2; // play, quit
    case MenuPage::Maps:
        return maps_.size() + 1; // maps + back
    case MenuPage::Pause:
        return 2; // resume, quit
    }
    return 0;
}

void MenuModel::move(int delta) {
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
    }
    return MenuAction::None;
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
            return {(i == 0) ? loc("menu.play") : loc("menu.quit"), {}};
        case MenuPage::Maps:
            if (i < model.maps().size()) {
                return {loc(model.maps()[i].name_key), loc(model.maps()[i].blurb_key)};
            }
            return {loc("menu.back"), {}};
        case MenuPage::Pause:
            return {(i == 0) ? loc("menu.resume") : loc("menu.quit"), {}};
        }
        return {};
    };

    canvas.resize(canvas.width(), canvas.height());
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
    if (pause && plates_enabled()) {
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
        const int px = (w - block_w) / 2 - 8;
        const int py = title_y - 6;
        const int pw = block_w + 16;
        const int ph = block_h + 8;
        canvas.fill_rect(px, py, pw, ph, BACKGROUND);
        canvas.frame_rect(px, py, pw, ph, RULE_LINE);
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
    if (pause && plates_enabled()) {
        const int hw = render::text_width_px(hint);
        canvas.fill_rect((w - hw) / 2 - 4, hint_y - 3, hw + 8, render::FONT_CELL_H + 5,
                         BACKGROUND);
    }
    draw_centered(canvas, hint_y, hint, BLURB);
}

} // namespace dfn::app
