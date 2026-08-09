/*
Created: 09:08:2026 - 17:16:40
Last updated: 09:08:2026 - 23:32:07
Module: engine/render
File: engine/render/sources/PixelCanvas.cpp

Responsibility:
- PixelCanvas implementation: clipped software rasterization of the primitives
  UI screens need (rects, frames, 1-bit stamps, triangles).

Key items:
- PixelCanvas::resize/clear/clear_transparent/put/fill_rect/frame_rect/
  draw_stamp/fill_triangle.

Dependencies:
- Uses: PixelCanvas.h, C++ stdlib, glm.
- Used by: MapScreen; future menu/HUD screens.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every primitive clips; never trust caller coordinates.
*/
/*
UPD:
- 09:08:2026 - 17:16:40: Created together with the map screen.
- 09:08:2026 - 23:32:07: clear_transparent() for the HUD layer.
*/

#include "engine/render/sources/PixelCanvas.h"

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

uint8_t clamp_channel(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
}

} // namespace

Color shade(Color base, float factor) {
    return Color{clamp_channel(static_cast<float>(base.r) * factor),
                 clamp_channel(static_cast<float>(base.g) * factor),
                 clamp_channel(static_cast<float>(base.b) * factor)};
}

void PixelCanvas::resize(uint32_t width, uint32_t height) {
    if (width == width_ && height == height_) {
        return;
    }
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<size_t>(width) * height * 4, 0);
}

void PixelCanvas::clear(Color color) {
    for (size_t i = 0; i + 3 < pixels_.size(); i += 4) {
        pixels_[i] = color.r;
        pixels_[i + 1] = color.g;
        pixels_[i + 2] = color.b;
        pixels_[i + 3] = 255;
    }
}

void PixelCanvas::clear_transparent() {
    std::fill(pixels_.begin(), pixels_.end(), static_cast<uint8_t>(0));
}

void PixelCanvas::put(int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= static_cast<int>(width_) || y >= static_cast<int>(height_)) {
        return;
    }
    const size_t i = (static_cast<size_t>(y) * width_ + static_cast<size_t>(x)) * 4;
    pixels_[i] = color.r;
    pixels_[i + 1] = color.g;
    pixels_[i + 2] = color.b;
    pixels_[i + 3] = 255;
}

void PixelCanvas::fill_rect(int x, int y, int w, int h, Color color) {
    const int x1 = std::min(x + w, static_cast<int>(width_));
    const int y1 = std::min(y + h, static_cast<int>(height_));
    for (int py = std::max(y, 0); py < y1; ++py) {
        for (int px = std::max(x, 0); px < x1; ++px) {
            put(px, py, color);
        }
    }
}

void PixelCanvas::hline(int x, int y, int length, Color color) {
    for (int i = 0; i < length; ++i) {
        put(x + i, y, color);
    }
}

void PixelCanvas::vline(int x, int y, int length, Color color) {
    for (int i = 0; i < length; ++i) {
        put(x, y + i, color);
    }
}

void PixelCanvas::frame_rect(int x, int y, int w, int h, Color color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    hline(x, y, w, color);
    hline(x, y + h - 1, w, color);
    vline(x, y, h, color);
    vline(x + w - 1, y, h, color);
}

void PixelCanvas::draw_stamp(int x, int y, const Stamp& stamp, Color color,
                             bool outline_first, Color outline) {
    if (stamp.rows == nullptr || stamp.width <= 0 || stamp.height <= 0) {
        return;
    }
    // Halo pass: the same mask smeared over the 8 neighbours. One dark ring
    // makes a 5-7 px silhouette readable over any terrain value underneath.
    if (outline_first) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                for (int sy = 0; sy < stamp.height; ++sy) {
                    const char* row = stamp.rows[sy];
                    for (int sx = 0; sx < stamp.width; ++sx) {
                        if (row[sx] != '\0' && row[sx] != ' ') {
                            put(x + sx + dx, y + sy + dy, outline);
                        }
                    }
                }
            }
        }
    }
    for (int sy = 0; sy < stamp.height; ++sy) {
        const char* row = stamp.rows[sy];
        for (int sx = 0; sx < stamp.width; ++sx) {
            if (row[sx] != '\0' && row[sx] != ' ') {
                put(x + sx, y + sy, color);
            }
        }
    }
}

void PixelCanvas::fill_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, Color color) {
    const int min_x = static_cast<int>(std::floor(std::min({a.x, b.x, c.x})));
    const int max_x = static_cast<int>(std::ceil(std::max({a.x, b.x, c.x})));
    const int min_y = static_cast<int>(std::floor(std::min({a.y, b.y, c.y})));
    const int max_y = static_cast<int>(std::ceil(std::max({a.y, b.y, c.y})));

    const auto edge = [](glm::vec2 p, glm::vec2 q, glm::vec2 r) {
        return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
    };
    const float area = edge(a, b, c);
    if (std::fabs(area) < 1e-6f) {
        return; // degenerate
    }
    const float sign = area > 0.0f ? 1.0f : -1.0f;
    for (int py = min_y; py <= max_y; ++py) {
        for (int px = min_x; px <= max_x; ++px) {
            const glm::vec2 p{static_cast<float>(px) + 0.5f, static_cast<float>(py) + 0.5f};
            const float w0 = edge(a, b, p) * sign;
            const float w1 = edge(b, c, p) * sign;
            const float w2 = edge(c, a, p) * sign;
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                put(px, py, color);
            }
        }
    }
}

} // namespace dfn::render
