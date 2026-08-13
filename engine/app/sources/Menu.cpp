/*
Created: 10:08:2026 - 10:27:20
Last updated: 13:08:2026 - 19:45:00
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
- 13:08:2026 - 19:45:00: СТРАНИЦА НАСТРОЕК (просьба пользователя): разрешение,
  сглаживание, палитра, покачивание камеры и переход на калибровку — пять строк
  settings.cfg, которые до сих пор менялись только текстовым редактором. Каждая
  строка — ЛЕСТНИЦА ЗНАЧЕНИЙ, а не поле ввода: 640x360 движок принимает, а 641x361
  игрок мог вписать в файл руками. Вторая строка корня стала «Настройки», яркость
  живёт внутри них, и выход со страницы калибровки возвращается туда, откуда пришёл.
  Enter на строке значения делает то же, что стрелка вправо, — страница работает на
  клавишах, которые приложение уже шлёт.
*/

#include "engine/app/sources/Menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

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

// THE SETTINGS PAGE IS A LIST OF LADDERS, not a set of fields, and that is what
// lets it replace the text editor safely: every rung is a value the engine is
// known to accept. 640x360 is a resolution the upscale is exact for; 641x361 is
// a resolution the player could type into settings.cfg today and get a picture
// nobody has ever looked at. A menu that cannot express the broken state is
// worth more than a menu that validates it afterwards.
constexpr uint32_t RES_W[] = {320, 640, 960, 1280};
constexpr uint32_t RES_H[] = {180, 360, 540, 720};
constexpr uint32_t MSAA_STEPS[] = {0, 2, 4, 8};
// 0 is the motion-sickness setting the research mandated and it is a FULL stop
// of the motion, not a small one -- so it is a rung of its own, not the bottom
// of a slider the player has to hunt for.
constexpr float BOB_STEPS[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};

// The rows of the settings page, named once. Their ORDER is the order of the
// picture: what the frame is drawn on, then what is done to it, then how it
// moves, then how dark it is allowed to get.
enum SettingsRow : size_t {
    RowResolution = 0,
    RowMsaa,
    RowPalette,
    RowHeadBob,
    RowBrightness, // opens the calibration page, which is where an EYE decides
    RowBack,
    RowCount,
};

// Nearest rung to a value that may have come from a hand-edited settings.cfg.
// Never fails: a file saying msaa=3 has to land somewhere, and landing on the
// nearest legal rung is the only answer that keeps the page honest about what
// the game will actually run with.
template <typename T, size_t N>
size_t nearest_rung(const T (&rungs)[N], T value) {
    size_t best = 0;
    double best_d = -1.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = std::abs(static_cast<double>(rungs[i]) - static_cast<double>(value));
        if (best_d < 0.0 || d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

size_t cycle(size_t index, size_t count, int delta) {
    const int n = static_cast<int>(count);
    int next = (static_cast<int>(index) + delta) % n;
    if (next < 0) {
        next += n;
    }
    return static_cast<size_t>(next);
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

void MenuModel::set_settings(const MenuSettings& value) {
    settings_ = value;
    launched_ = value;
}

bool MenuModel::needs_restart() const {
    // ONLY the three rows the renderer swallows at init. head_bob and the
    // brightness floor are handed to a live frame, so listing them here would
    // be a warning about nothing -- and a warning that fires when it need not
    // is how players learn to ignore the one that matters.
    return settings_.internal_w != launched_.internal_w
        || settings_.internal_h != launched_.internal_h
        || settings_.msaa != launched_.msaa
        || settings_.palette != launched_.palette;
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
    case MenuPage::Settings:
        return RowCount;
    }
    return 0;
}

void MenuModel::adjust(int delta) {
    if (page_ != MenuPage::Settings || delta == 0) {
        return;
    }
    switch (selection_) {
    case RowResolution: {
        const size_t i = cycle(nearest_rung(RES_W, settings_.internal_w),
                               std::size(RES_W), delta);
        settings_.internal_w = RES_W[i];
        settings_.internal_h = RES_H[i];
        // The two are ONE row on purpose: the grid is an aspect the whole
        // look is drawn for, and a page that lets width and height be turned
        // apart is a page that can produce a picture nobody has ever seen.
        break;
    }
    case RowMsaa:
        settings_.msaa = MSAA_STEPS[cycle(nearest_rung(MSAA_STEPS, settings_.msaa),
                                          std::size(MSAA_STEPS), delta)];
        break;
    case RowPalette:
        settings_.palette = !settings_.palette; // two rungs: either direction flips
        break;
    case RowHeadBob:
        settings_.head_bob = BOB_STEPS[cycle(nearest_rung(BOB_STEPS, settings_.head_bob),
                                             std::size(BOB_STEPS), delta)];
        break;
    default:
        break; // brightness and back are not values: Enter is their only verb
    }
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
            // THE ROOT'S SECOND ROW IS NOW SETTINGS, NOT BRIGHTNESS. The dial
            // did not move away from the player -- it is the settings page's
            // own row, one press further in, and it stopped being the only
            // setting in the game that had a screen while resolution,
            // antialiasing, palette and camera bob had none.
            open(MenuPage::Settings);
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
        open(calibrate_return_);
        return MenuAction::CalibrationDone;
    case MenuPage::Settings:
        if (selection_ == RowBrightness) {
            calibrate_return_ = MenuPage::Settings;
            open(MenuPage::Calibrate);
            return MenuAction::None;
        }
        if (selection_ == RowBack) {
            open(MenuPage::Root);
            return MenuAction::SettingsDone;
        }
        // ENTER IS THE SAME VERB AS RIGHT on a value row. The app routes up,
        // down, Enter and Escape today; left and right are a patch in its
        // file, and a page that needs a key nobody sends is a page that ships
        // dead. With this line it works on the keys that already exist and
        // gets nicer when the other two arrive.
        adjust(+1);
        return MenuAction::None;
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
        open(calibrate_return_);
        return MenuAction::CalibrationDone;
    case MenuPage::Settings:
        // And so does Escape here, for the same reason and with the same
        // guarantee: both exits from this page emit SettingsDone, so there is
        // no way out that silently discards what the player just turned.
        open(MenuPage::Root);
        return MenuAction::SettingsDone;
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

// THE SETTINGS PAGE (the user's request: settings.cfg had five rows describing
// the picture and no way to change any of them except a text editor).
//
// IT IS TWO COLUMNS, not the centred list the other pages use, and that is the
// one layout decision here worth defending: a settings row is a PAIR (what it
// is, what it is set to), and centring each pair separately makes the values
// jitter left and right as the player turns them, which reads as the page
// twitching rather than the value changing. Labels left, values right-aligned
// against one edge, so only the value that changed moves.
void draw_settings(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    const MenuSettings& s = model.settings();

    canvas.clear(BACKGROUND);

    // Values built once, into storage that outlives the two passes below --
    // the block is MEASURED before it is drawn, and measuring text that is not
    // the text drawn is the mistake the pause plate was fixed for.
    const std::string off(loc("menu.settings.off"));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%ux%u", s.internal_w, s.internal_h);
    const std::string res(buf);
    std::snprintf(buf, sizeof(buf), "%ux", s.msaa);
    const std::string msaa = (s.msaa <= 1) ? off : std::string(buf);
    std::snprintf(buf, sizeof(buf), "%.0f%%", static_cast<double>(s.head_bob) * 100.0);
    const std::string bob = (s.head_bob <= 0.0f) ? off : std::string(buf);
    // The floor in STEPS, the same ruler the calibration page is built on: the
    // number the player saw while turning it is the number he reads here.
    std::snprintf(buf, sizeof(buf), "%.2f",
                  static_cast<double>(model.black_floor() / SHADE_STEP));
    const std::string bright = (model.black_floor() <= 0.0f) ? off : std::string(buf);

    struct Row {
        std::string_view label;
        std::string_view value;
    };
    const Row rows[RowCount] = {
        {loc("menu.settings.resolution"), res},
        {loc("menu.settings.msaa"), msaa},
        {loc("menu.settings.palette"),
         s.palette ? loc("menu.settings.on") : loc("menu.settings.off")},
        {loc("menu.settings.bob"), bob},
        {loc("menu.settings.brightness"), bright},
        {loc("menu.back"), {}},
    };

    int label_w = 0;
    int value_w = 0;
    for (const Row& r : rows) {
        label_w = std::max(label_w, render::text_width_px(r.label));
        value_w = std::max(value_w, render::text_width_px(r.value));
    }
    const int gap = render::FONT_CELL_W * 3;
    const int block = label_w + gap + value_w;
    const int x0 = (w - block) / 2;

    const int title_y = h / 6;
    draw_centered(canvas, title_y, loc("menu.settings.title"), TITLE);
    canvas.fill_rect(w / 4, title_y + render::FONT_CELL_H + 4, w / 2, 1, RULE_LINE);

    int y = title_y + render::FONT_CELL_H + 16;
    for (size_t i = 0; i < RowCount; ++i) {
        const bool sel = (i == model.selection());
        const render::Color color = sel ? ITEM_SELECTED : ITEM;
        if (sel) {
            render::draw_text(canvas, x0 - render::FONT_CELL_W * 2, y, ">", ITEM_SELECTED,
                              true);
        }
        render::draw_text(canvas, x0, y, rows[i].label, color, /*shadow=*/true);
        if (!rows[i].value.empty()) {
            render::draw_text(canvas, x0 + block - render::text_width_px(rows[i].value), y,
                              rows[i].value, color, /*shadow=*/true);
        }
        y += render::FONT_CELL_H + 4;
        if (i == RowBrightness) {
            y += 4; // the two rows below are verbs, not values
        }
    }

    // SAID OUT LOUD, and only when it is true: three of these rows are
    // swallowed by the renderer at init, so turning them changes the file and
    // not the frame in front of the player. A page that lets that happen
    // silently teaches the player that the settings do nothing.
    if (model.needs_restart()) {
        draw_centered(canvas, h - render::FONT_CELL_H * 4, loc("menu.settings.restart"), BLURB);
    }
    draw_centered(canvas, h - render::FONT_CELL_H * 2 - 4, loc("menu.settings.keys"), BLURB);
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
            return {(i == 1) ? loc("menu.settings") : loc("menu.quit"), {}};
        case MenuPage::Maps:
            if (i < model.maps().size()) {
                return {loc(model.maps()[i].name_key), loc(model.maps()[i].blurb_key)};
            }
            return {loc("menu.back"), {}};
        case MenuPage::Pause:
            return {(i == 0) ? loc("menu.resume") : loc("menu.quit"), {}};
        case MenuPage::Calibrate:
        case MenuPage::Settings:
            return {}; // drawn by their own functions, which have their own layout
        }
        return {};
    };

    canvas.resize(canvas.width(), canvas.height());
    if (model.page() == MenuPage::Calibrate) {
        draw_calibration(canvas, model);
        return;
    }
    if (model.page() == MenuPage::Settings) {
        draw_settings(canvas, model);
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
