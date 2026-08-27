/*
Created: 23:08:2026 - 22:40:00
Last updated: 27:08:2026 - 15:10:00
Module: engine/render
File: engine/render/sources/LoadingScreen.cpp

Responsibility:
- Модель и оформление экрана загрузки, объявленного в LoadingScreen.h.

Dependencies:
- Uses: PixelCanvas, BitmapFont, std.
- Used by: engine/app, tests/render.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона render владеет этим файлом.
- ФОРМАТ СТРОКИ ПРИБОРА ЗАМОРОЖЕН. «[load] %-28s %8.1f ms  (итого %8.1f)» —
  ровно то, что печатал App::enter_world до появления экрана. Рецепты и
  замеры зон уже разбирают эти строки; сдвиг колонки — это молчаливая порча
  чужих приборов.
*/
/*
UPD:
- 23:08:2026 - 22:40:00: Создан вместе с заголовком (И15 волна А, шаг 3).
- 24:08:2026 - 00:45:00: stage() называет СДЕЛАННУЮ работу; полоса по set_expected().
- 27:08:2026 - 14:30:00: полоса встаёт ПОД списком (было: прибита к h/2 + 64).
  Отметка подходила пяти этапам входа в дом и наехала на девятый этап загрузки
  города в тот же день, когда их стало девять.
- 27:08:2026 - 15:10:00: РАЗМЕР БУКВЫ — ДОЛЯ ЭКРАНА. Холст интерфейса вырос
  сегодня с 640×360 до FullHD, и экран, нарисованный 1:1, стал вчетверо мельче
  вчерашнего на том же мониторе. Множитель = h / 360 (высота холста, на которой
  вёрстка сочинялась): на ней он равен единице и кадр прежний бит-в-бит.
  Отступы и полоса умножаются вместе с буквой — иначе вёрстка расползлась бы.
*/

#include "engine/render/sources/LoadingScreen.h"

#include "engine/render/sources/BitmapFont.h"

#include <algorithm>
#include <cstdio>

namespace dfn::render {
namespace {

// ПАЛИТРА ЭКРАНА. Взята у меню (Menu.cpp): загрузка и меню — один и тот же
// «не-мир», и два разных тёмных фона читались бы как две разные программы.
constexpr Color BACKGROUND{18, 20, 26};
constexpr Color TITLE{232, 228, 214};
constexpr Color SUBTITLE{120, 118, 112};
constexpr Color STAGE_DONE{176, 172, 160};
constexpr Color STAGE_NOW{244, 226, 160};
constexpr Color RULE_LINE{54, 56, 64};
constexpr Color BAR_FRAME{70, 74, 82};
constexpr Color BAR_FILL{150, 190, 120};

double ms_between(std::chrono::steady_clock::time_point a,
                  std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

} // namespace

void LoadingScreen::begin(std::string title, std::string subtitle) {
    title_ = std::move(title);
    subtitle_ = std::move(subtitle);
    stages_.clear();
    begun_ = Clock::now();
    marked_ = begun_;
    total_ms_ = 0.0;
    progress_ = -1.0f;
    active_ = true;
    running_ = true;
}

void LoadingScreen::stage(std::string what) {
    const Clock::time_point now = Clock::now();
    const double ms = ms_between(marked_, now);
    marked_ = now;
    stages_.push_back(LoadStage{std::move(what), ms, true});
    if (log_) {
        // ФОРМАТ ЗАМОРОЖЕН (см. шапку): «что сделано / сколько заняло /
        // итого». Строка печатается в момент, когда работа кончилась.
        std::fprintf(stderr, "[load] %-28s %8.1f ms  (итого %8.1f)\n",
                     stages_.back().what.c_str(), stages_.back().ms,
                     ms_between(begun_, now));
    }
}

void LoadingScreen::finish() {
    total_ms_ = ms_between(begun_, Clock::now());
    running_ = false;
}

void LoadingScreen::hide() {
    active_ = false;
    running_ = false;
}

double LoadingScreen::elapsed_ms() const {
    if (!running_) {
        return total_ms_;
    }
    return ms_between(begun_, Clock::now());
}

float LoadingScreen::progress() const {
    if (progress_ >= 0.0f) {
        return std::clamp(progress_, 0.0f, 1.0f);
    }
    // ПО ЭТАПАМ, а не по времени. Доля времени требовала бы знать, сколько
    // загрузка займёт, — а это ровно то, чего никто не знает до её конца;
    // полоса, ползущая по выдуманному прогнозу, врёт дважды в секунду.
    if (!running_) {
        // Кончилась — полна; НЕ НАЧИНАЛАСЬ — пуста. Эти два состояния
        // различаются только по active_, и слить их значило бы показать
        // полную полосу на экране, который ещё ничего не грузил.
        return active_ ? 1.0f : 0.0f;
    }
    if (expected_ == 0) {
        return 0.0f; // сколько всего этапов, никто не сказал
    }
    return std::clamp(static_cast<float>(stages_.size())
                          / static_cast<float>(expected_), 0.0f, 1.0f);
}

void LoadingScreen::draw(PixelCanvas& canvas) const {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    if (w <= 0 || h <= 0) {
        return;
    }
    canvas.clear(BACKGROUND);

    // РАЗМЕР БУКВЫ — ДОЛЯ ЭКРАНА, А НЕ ЧИСЛО ПИКСЕЛЕЙ. Холст интерфейса вырос
    // 27.08 с 640 до 1920 (FullHD), и текст, нарисованный 1:1, стал вчетверо
    // мельче вчерашнего на том же мониторе: экран загрузки читался бы как
    // мелкий шрифт договора рядом с меню, набранным антиквой в 60 px.
    // Отсчёт от 360 — высоты прежнего холста, на которой вёрстка и
    // сочинялась: на ней множитель равен единице и кадр совпадает с прежним
    // бит-в-бит, а на любом большем холсте буква занимает ту же долю высоты.
    const int scale = std::max(1, h / 360);
    const int line = FONT_CELL_H * scale;
    const auto centered = [&](int y, const std::string& text, Color c) {
        const int x = (w - text_width_px(text, scale)) / 2;
        draw_text(canvas, x, y, text, c, /*shadow=*/true, Color{0, 0, 0}, scale);
    };

    int y = h / 2 - 70 * scale;
    centered(y, title_, TITLE);
    y += line + 4 * scale;
    if (!subtitle_.empty()) {
        centered(y, subtitle_, SUBTITLE);
    }
    y += line + 6 * scale;
    canvas.hline(w / 4, y, w / 2, RULE_LINE);
    y += 10 * scale;

    // ЭТАПЫ СЛЕВА, ЧИСЛА СПРАВА, в одной колонке — так глаз ловит, какой из
    // них дорог, не читая ни одной строки целиком.
    const int left = w / 4;
    const int right = w - w / 4;
    for (const LoadStage& st : stages_) {
        const Color c = st.done ? STAGE_DONE : STAGE_NOW;
        draw_text(canvas, left, y, st.what, c, /*shadow=*/true, Color{0, 0, 0}, scale);
        if (st.done) {
            char num[32] = {};
            std::snprintf(num, sizeof(num), "%.0f ms", st.ms);
            const std::string n(num);
            draw_text(canvas, right - text_width_px(n, scale), y, n, c, true,
                      Color{0, 0, 0}, scale);
        }
        y += line + 2 * scale;
    }

    // ПОЛОСА. Три прямоугольника — рамка, ложе, заливка: заливка никогда не
    // касается фона напрямую (та же выкройка, что у полос состояния HUD).
    const int bar_w = w / 2;
    const int bar_h = 8 * scale;
    const int bar_x = (w - bar_w) / 2;
    // ПОЛОСА СТОИТ ПОД СПИСКОМ, А НЕ НА ФИКСИРОВАННОЙ ОТМЕТКЕ. Прибитая к
    // h/2 + 64, она подходила при пяти этапах входа в дом и НАЕХАЛА на девятый
    // этап загрузки города в тот же день, когда их стало девять (кадр приёмки
    // 27.08: «мир готов» перечёркнут заливкой). Место под список — величина
    // переменная, и вычислять её надо, а не помнить.
    const int bar_y = std::max(h / 2 + 64 * scale, y + 10 * scale);
    canvas.fill_rect(bar_x - scale, bar_y - scale, bar_w + 2 * scale,
                     bar_h + 2 * scale, BAR_FRAME);
    canvas.fill_rect(bar_x, bar_y, bar_w, bar_h, BACKGROUND);
    const int filled =
        static_cast<int>(static_cast<float>(bar_w) * progress() + 0.5f);
    if (filled > 0) {
        canvas.fill_rect(bar_x, bar_y, std::min(filled, bar_w), bar_h, BAR_FILL);
    }

    // ИТОГ ПОД ПОЛОСОЙ. Он на экране и тогда, когда прибор закрыт: человек,
    // жалующийся «долго грузится», обязан иметь возможность назвать число.
    char total[48] = {};
    std::snprintf(total, sizeof(total), "%.0f ms", elapsed_ms());
    centered(bar_y + bar_h + 6 * scale, std::string(total), SUBTITLE);
}

std::string LoadingScreen::report() const {
    char head[64] = {};
    std::snprintf(head, sizeof(head), "%.1f мс", total_ms_ > 0.0 ? total_ms_
                                                                 : elapsed_ms());
    std::string out = title_ + ": " + head;
    const char* sep = " — ";
    for (const LoadStage& s : stages_) {
        char num[32] = {};
        std::snprintf(num, sizeof(num), " %.1f", s.ms);
        out += sep;
        out += s.what;
        out += num;
        sep = " | ";
    }
    return out;
}

} // namespace dfn::render
