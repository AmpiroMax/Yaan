/*
Created: 10:08:2026 - 10:27:20
Last updated: 10:08:2026 - 10:27:20
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
*/

#include "engine/app/sources/Menu.h"

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

    canvas.resize(canvas.width(), canvas.height());
    if (model.page() == MenuPage::Pause) {
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

    const int title_y = model.page() == MenuPage::Pause ? h / 4 : h / 5;
    draw_centered(canvas, title_y,
                  model.page() == MenuPage::Pause ? loc("menu.paused") : loc("app.title"),
                  TITLE);
    canvas.fill_rect(w / 4, title_y + render::FONT_CELL_H + 4, w / 2, 1, RULE_LINE);

    // Items start below the rule; the blurb line under each map costs one row,
    // so map rows are spaced two rows apart and plain rows one.
    const bool maps_page = model.page() == MenuPage::Maps;
    const int row = render::FONT_CELL_H + (maps_page ? 6 : 4);
    int y = title_y + render::FONT_CELL_H + 16;

    const size_t n = model.item_count();
    for (size_t i = 0; i < n; ++i) {
        const bool sel = (i == model.selection());
        const render::Color color = sel ? ITEM_SELECTED : ITEM;
        std::string_view label;
        std::string_view blurb;
        switch (model.page()) {
        case MenuPage::Root:
            label = (i == 0) ? loc("menu.play") : loc("menu.quit");
            break;
        case MenuPage::Maps:
            if (i < model.maps().size()) {
                label = loc(model.maps()[i].name_key);
                blurb = loc(model.maps()[i].blurb_key);
            } else {
                label = loc("menu.back");
            }
            break;
        case MenuPage::Pause:
            label = (i == 0) ? loc("menu.resume") : loc("menu.quit");
            break;
        }

        // The caret is what makes the selection readable at 640x360 -- colour
        // alone is a shade step, and a shade step is the weakest signal we have.
        const int label_w = render::text_width_px(label);
        const int x = (w - label_w) / 2;
        if (sel) {
            render::draw_text(canvas, x - render::FONT_CELL_W * 2, y, ">", ITEM_SELECTED,
                              true);
        }
        render::draw_text(canvas, x, y, label, color, /*shadow=*/true);
        y += render::FONT_CELL_H + 2;
        if (!blurb.empty()) {
            draw_centered(canvas, y, blurb, BLURB);
            y += render::FONT_CELL_H;
        }
        y += row - render::FONT_CELL_H - 2;
    }

    draw_centered(canvas, h - render::FONT_CELL_H * 2 - 4, loc("menu.hint"), BLURB);
}

} // namespace dfn::app
