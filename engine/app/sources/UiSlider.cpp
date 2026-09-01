/*
Module: engine/app
File: engine/app/sources/UiSlider.cpp

Responsibility:
- Арифметика и отрисовка непрерывного ползунка. Договор и все доли — в
  UiSlider.h.

Dependencies:
- Uses: engine/app UiFont, engine/render PixelCanvas.
- Used by: engine/app CharGen.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
*/

#include "engine/app/sources/UiSlider.h"

#include "engine/app/sources/UiFont.h"

#include <algorithm>
#include <cmath>

namespace dfn::app {

namespace {

/// ДОЛЯ ХОДА, НА КОТОРОЙ СТОИТ ЗНАЧЕНИЕ. Вырожденная полоса — ноль, а не
/// NaN: у цели без хода ручка стоит в начале и не двигается.
[[nodiscard]] float fraction_of(float value, float lo, float hi) {
    if (!(hi > lo)) {
        return 0.0f;
    }
    return std::clamp((value - lo) / (hi - lo), 0.0f, 1.0f);
}

} // namespace

float slider_value_at(const SliderTrack& track, int px, float lo, float hi) {
    if (!(hi > lo)) {
        return lo;
    }
    const int span = std::max(1, track.w);
    const float t = std::clamp(static_cast<float>(px - track.x)
                                   / static_cast<float>(span),
                               0.0f, 1.0f);
    return lo + t * (hi - lo);
}

int slider_handle_x(const SliderTrack& track, float value, float lo, float hi) {
    const float t = fraction_of(value, lo, hi);
    return track.x + static_cast<int>(std::lround(t * static_cast<double>(track.w)));
}

float slider_key_step(float lo, float hi, bool fine) {
    if (!(hi > lo)) {
        return 0.0f;
    }
    const int steps = fine ? SLIDER_FINE_STEPS : SLIDER_COARSE_STEPS;
    return (hi - lo) / static_cast<float>(steps);
}

bool slider_hit(const SliderTrack& track, int px, int py) {
    // ПОЛЯ СЧИТАЮТСЯ ОТ САМОЙ РУЧКИ, а не выбраны в пикселях: на ретро-сетке
    // ручка три пикселя шириной, и поле в «восемь» съело бы соседнюю строку.
    const int pad_x = std::max(2, track.handle);
    const int pad_y = std::max(2, track.height);
    return px >= track.x - pad_x && px <= track.x + track.w + pad_x
           && py >= track.y - pad_y && py <= track.y + pad_y;
}

void draw_slider(render::PixelCanvas& canvas, const SliderTrack& track,
                 std::string_view label, int label_x, std::string_view value_text,
                 int value_right_x, float value, float lo, float hi,
                 const SliderInk& ink, int label_px, bool selected, bool dragging) {
    const int cap = std::max(1, ui_cap_height(label_px));
    // ПОДПИСЬ И ЧИСЛО СТОЯТ ВЕРХОМ СТРОКИ НА ОДНОЙ ВЫСОТЕ С СЕРЕДИНОЙ ПОЛОСЫ,
    // и это единственная связка между текстом и жёлобом: ui_draw_text кладёт
    // ВЕРХ строки на y (см. UiFont.h), а полоса задана СЕРЕДИНОЙ.
    const int text_y = track.y - cap / 2;
    if (!label.empty()) {
        ui_draw_text(canvas, label_x, text_y, label, ink.label, label_px,
                     /*shadow=*/true);
    }
    if (!value_text.empty()) {
        const int vw = ui_text_width(value_text, label_px);
        ui_draw_text(canvas, value_right_x - vw, text_y, value_text, ink.value,
                     label_px, /*shadow=*/true);
    }

    const int thick = std::max(1, static_cast<int>(
                                      std::lround(SLIDER_TRACK_THICK_FRAC
                                                  * static_cast<double>(cap))));
    const int hx = slider_handle_x(track, value, lo, hi);
    // НАРИСОВАННАЯ ЛИНИЯ ШИРЕ ЖЁЛОБА НА ПОЛ-РУЧКИ С КАЖДОЙ СТОРОНЫ: жёлоб —
    // это ход ЦЕНТРА ручки, и линия, обрезанная по нему, кончалась бы под
    // ручкой, стоящей на краю.
    const int half = std::max(1, track.handle / 2);
    const int line_x = track.x - half;
    const int line_w = track.w + 2 * half;
    canvas.fill_rect(line_x, track.y - thick / 2, line_w, thick, ink.track);
    if (hx > track.x) {
        canvas.fill_rect(line_x, track.y - thick / 2, hx - line_x, thick, ink.fill);
    }

    // РУЧКА — ПРЯМОУГОЛЬНИК, А НЕ КРУЖОК. На 320x180 круг радиусом в два
    // пикселя неотличим от квадрата, а на 1920x1080 нарисованный кружок
    // потребовал бы сглаживания, которого у холста нет; вертикальная планка
    // читается на обоих концах лестницы одинаково.
    const int hw = std::max(1, track.handle + (dragging ? 2 : 0));
    const int hh = std::max(1, track.height);
    canvas.fill_rect(hx - hw / 2, track.y - hh / 2, hw, hh, ink.handle);
    if (selected) {
        // Обводка, а не второй цвет: холст живёт в четырёх тонах на страницу,
        // и пятый тон ради фокуса был бы решением, которого никто не принимал.
        canvas.frame_rect(hx - hw / 2 - 1, track.y - hh / 2 - 1, hw + 2, hh + 2,
                          ink.handle);
    }
}

} // namespace dfn::app
