/*
Created: 27:08:2026 - 00:36:20
Module: engine/app
File: engine/app/sources/MenuArt.cpp

Responsibility:
- Implementation of MenuArt.h: magnified text, image fitting, the dust field and
  the studio splash frame.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
*/
/*
UPD:
- 27:08:2026 - 00:36:20: Создан вместе с заголовком.
*/

#include "engine/app/sources/MenuArt.h"

#include "engine/app/sources/PngImage.h"
#include "engine/render/sources/BitmapFont.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dfn::app {

namespace {

int advance_px(int scale, int tracking) {
    return render::FONT_CELL_W * scale + tracking;
}

// A cheap, fixed integer hash. Two motes must not share a lane, and a real RNG
// would be state the caller has to carry for a decorative effect.
uint32_t mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float unit(uint32_t h) {
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

// Composites one source pixel over the canvas. PixelCanvas has no blend of its
// own on purpose (it is a raster surface, not a compositor), so the read is
// done here, once, next to the only two callers that need it.
void blend(render::PixelCanvas& canvas, int x, int y, int r, int g, int b, float a) {
    if (a <= 0.0f || x < 0 || y < 0 || x >= static_cast<int>(canvas.width())
        || y >= static_cast<int>(canvas.height())) {
        return;
    }
    a = std::min(a, 1.0f);
    const auto px = canvas.pixels();
    const size_t at = (static_cast<size_t>(y) * canvas.width() + static_cast<size_t>(x)) * 4u;
    const float inv = 1.0f - a;
    const auto mixc = [&](int src, uint8_t dst) {
        return static_cast<uint8_t>(std::lround(
            std::clamp(static_cast<float>(src) * a + static_cast<float>(dst) * inv,
                       0.0f, 255.0f)));
    };
    canvas.put(x, y,
               render::Color{mixc(r, px[at]), mixc(g, px[at + 1]), mixc(b, px[at + 2])});
}

} // namespace

int text_width_scaled(std::string_view utf8, int scale, int tracking) {
    const int n = render::text_glyph_count(utf8);
    if (n <= 0) {
        return 0;
    }
    // The trailing tracking is subtracted: the ink ends at the last glyph, and a
    // right-aligned column measured with a trailing gap sits a gap short of its
    // own edge -- which reads as the list being crooked, not as it being padded.
    return n * advance_px(scale, tracking) - tracking;
}

int text_height_scaled(int scale) { return render::FONT_INK_H * scale; }

int draw_text_scaled(render::PixelCanvas& canvas, int x, int y, std::string_view utf8,
                     render::Color color, int scale, int tracking, bool shadow) {
    scale = std::max(1, scale);
    if (shadow) {
        draw_text_scaled(canvas, x + scale, y + scale, utf8, render::Color{0, 0, 0},
                         scale, tracking, /*shadow=*/false);
    }
    const render::FontAtlas& atlas = render::font_atlas();
    int pen = x;
    size_t pos = 0;
    while (pos < utf8.size()) {
        const uint32_t cp = render::utf8_next(utf8, pos);
        const int slot = render::font_slot_for_codepoint(cp);
        for (int gy = 0; gy < render::FONT_INK_H; ++gy) {
            for (int gx = 0; gx < render::FONT_INK_W; ++gx) {
                if (atlas.ink(slot, gx, gy)) {
                    canvas.fill_rect(pen + gx * scale, y + gy * scale, scale, scale, color);
                }
            }
        }
        pen += advance_px(scale, tracking);
    }
    return pen - x - tracking;
}

void draw_image_fit(render::PixelCanvas& canvas, const Image& image, int box_x,
                    int box_y, int box_w, int box_h, float alpha) {
    if (image.empty() || box_w <= 0 || box_h <= 0 || alpha <= 0.0f) {
        return;
    }
    // ASPECT PRESERVED, and the box is a bound rather than a target: an emblem
    // stretched to a box is a different emblem.
    const double sx = static_cast<double>(box_w) / image.width;
    const double sy = static_cast<double>(box_h) / image.height;
    const double s = std::min(sx, sy);
    const int dw = std::max(1, static_cast<int>(std::lround(image.width * s)));
    const int dh = std::max(1, static_cast<int>(std::lround(image.height * s)));
    const int dx0 = box_x + (box_w - dw) / 2;
    const int dy0 = box_y + (box_h - dh) / 2;

    for (int dy = 0; dy < dh; ++dy) {
        // Source rows this destination row covers. On an upscale the rectangle
        // collapses to one pixel and the loop below is a nearest-neighbour
        // sample, which is the right answer for a magnified logo too.
        const int sy0 = static_cast<int>(static_cast<double>(dy) * image.height / dh);
        const int sy1 = std::max(sy0 + 1,
                                 static_cast<int>(static_cast<double>(dy + 1) * image.height / dh));
        for (int dx = 0; dx < dw; ++dx) {
            const int sx0 = static_cast<int>(static_cast<double>(dx) * image.width / dw);
            const int sx1 = std::max(sx0 + 1,
                                     static_cast<int>(static_cast<double>(dx + 1) * image.width / dw));
            // PREMULTIPLIED AVERAGE. Averaging colour without weighting by
            // alpha pulls the transparent pixels' (usually black) colour into
            // the edge, which is the classic dark halo around a logo.
            double ar = 0.0;
            double ag = 0.0;
            double ab = 0.0;
            double aa = 0.0;
            int n = 0;
            for (int sy_ = sy0; sy_ < sy1; ++sy_) {
                for (int sx_ = sx0; sx_ < sx1; ++sx_) {
                    const uint8_t* p = image.at(sx_, sy_);
                    const double a = p[3] / 255.0;
                    ar += p[0] * a;
                    ag += p[1] * a;
                    ab += p[2] * a;
                    aa += a;
                    ++n;
                }
            }
            if (n == 0 || aa <= 0.0) {
                continue;
            }
            blend(canvas, dx0 + dx, dy0 + dy, static_cast<int>(std::lround(ar / aa)),
                  static_cast<int>(std::lround(ag / aa)),
                  static_cast<int>(std::lround(ab / aa)),
                  static_cast<float>(aa / n) * alpha);
        }
    }
}

void draw_dust(render::PixelCanvas& canvas, float time_s, int count) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    if (w <= 0 || h <= 0 || count <= 0) {
        return;
    }
    // The mote is 1 px at the retro presets and grows with the frame, or it
    // would be invisible at 1920x1080 and a snowstorm at 320x180.
    const int size = std::max(1, h / 540);
    for (int i = 0; i < count; ++i) {
        const uint32_t hx = mix(static_cast<uint32_t>(i) * 2654435761u + 17u);
        const uint32_t hy = mix(hx ^ 0x9E3779B9u);
        const uint32_t hv = mix(hy ^ 0x85EBCA6Bu);
        // SLOW: a mote crosses the frame in 40 to 100 seconds. The reference's
        // specks are barely moving, and anything faster reads as falling snow.
        const float speed = 0.010f + 0.015f * unit(hv);
        const float drift = (unit(hx ^ 0x2545F491u) - 0.5f) * 0.004f;
        float fx = unit(hx) + drift * time_s;
        float fy = unit(hy) - speed * time_s;
        fx -= std::floor(fx);
        fy -= std::floor(fy);
        const int x = static_cast<int>(fx * static_cast<float>(w));
        const int y = static_cast<int>(fy * static_cast<float>(h));
        // Value varies mote to mote AND breathes slowly, so the field has depth
        // instead of reading as a fixed grid of identical dots.
        const float base = 0.25f + 0.55f * unit(hv ^ 0xC2B2AE35u);
        const float pulse = 0.75f + 0.25f * std::sin(time_s * (0.4f + unit(hy) * 0.5f)
                                                     + static_cast<float>(i));
        const auto v = static_cast<int>(std::lround(220.0f * base * pulse));
        for (int oy = 0; oy < size; ++oy) {
            for (int ox = 0; ox < size; ++ox) {
                blend(canvas, x + ox, y + oy, v, v, v, 0.85f);
            }
        }
    }
}

void draw_studio_splash(render::PixelCanvas& canvas, float t_s, float total_s) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    // The brand's own dark, not the menu's: the lock-up was drawn on #0c0e12
    // and its PNG carries that ground, so any other clear colour would show as
    // a rectangle around it.
    canvas.clear(render::Color{0x0c, 0x0e, 0x12});

    // A quarter in, a quarter out, held in between. The fade is what makes a
    // two-second frame read as a title card rather than as a stall.
    const float fade = std::max(0.15f, total_s * 0.25f);
    float alpha = 1.0f;
    if (t_s < fade) {
        alpha = t_s / fade;
    } else if (t_s > total_s - fade) {
        alpha = std::max(0.0f, (total_s - t_s) / fade);
    }
    alpha = std::clamp(alpha, 0.0f, 1.0f);

    const int box = std::min(w, h) * 3 / 5;
    draw_image_fit(canvas, cached_png(BRAND_SPIRAL_FULL_PNG), (w - box) / 2,
                   (h - box) / 2, box, box, alpha);
}

} // namespace dfn::app
