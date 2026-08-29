/*
Module: engine/editor
File: engine/editor/sources/EditorToolIcons.cpp

Responsibility:
- bake_tool_icon(): a tiny painter (dot, line, disc, triangle) and six drawings
  made of it. Everything is expressed in FRACTIONS of the icon, so the same code
  draws a 16 px icon and a 64 px one and neither is a special case.

Dependencies:
- Uses: EditorToolIcons.h, std.
- Used by: EditorToolbar, tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- No sizes in pixels inside a drawing. A number like "x = 7" is right for one
  icon size and silently wrong for the next one somebody asks for.
*/

#include "engine/editor/sources/EditorToolIcons.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dfn::app {
namespace {

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

/// THE INK. One light tone for the shapes and one accent, so six icons read as
/// one set — the interface's style already says the world is the subject and
/// the panels are not (EditorUi.cpp: apply_style).
constexpr Rgba INK{225, 228, 232, 255};
constexpr Rgba ACCENT{124, 196, 140, 255}; ///< the verdict's green, again
constexpr Rgba EARTH{176, 148, 112, 255};

/// A canvas that thinks in fractions of itself. `px` is the only place the size
/// enters, which is what makes every drawing below resolution-independent.
class Canvas {
public:
    Canvas(std::vector<std::uint8_t>& rgba, int px) : rgba_(&rgba), px_(px) {
        rgba_->assign(static_cast<std::size_t>(px) * static_cast<std::size_t>(px) * 4u,
                      0u); // transparent: the button's own colour shows through
    }

    void dot(float u, float v, Rgba c) {
        const int x = static_cast<int>(std::lround(u * static_cast<float>(px_ - 1)));
        const int y = static_cast<int>(std::lround(v * static_cast<float>(px_ - 1)));
        put(x, y, c);
    }

    /// A stroke of `thick` (also a fraction) from (u0,v0) to (u1,v1).
    void line(float u0, float v0, float u1, float v1, float thick, Rgba c) {
        const float len = std::max(std::abs(u1 - u0), std::abs(v1 - v0));
        const int steps = std::max(2, static_cast<int>(len * static_cast<float>(px_) * 2.0f));
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            disc(u0 + (u1 - u0) * t, v0 + (v1 - v0) * t, thick * 0.5f, c);
        }
    }

    void disc(float u, float v, float radius, Rgba c) {
        const float cx = u * static_cast<float>(px_ - 1);
        const float cy = v * static_cast<float>(px_ - 1);
        const float r = radius * static_cast<float>(px_ - 1);
        const int lo_x = static_cast<int>(std::floor(cx - r));
        const int hi_x = static_cast<int>(std::ceil(cx + r));
        const int lo_y = static_cast<int>(std::floor(cy - r));
        const int hi_y = static_cast<int>(std::ceil(cy + r));
        for (int y = lo_y; y <= hi_y; ++y) {
            for (int x = lo_x; x <= hi_x; ++x) {
                const float dx = static_cast<float>(x) - cx;
                const float dy = static_cast<float>(y) - cy;
                if (dx * dx + dy * dy <= r * r) {
                    put(x, y, c);
                }
            }
        }
    }

    void rect(float u0, float v0, float u1, float v1, Rgba c) {
        const int x0 = static_cast<int>(std::lround(u0 * static_cast<float>(px_ - 1)));
        const int x1 = static_cast<int>(std::lround(u1 * static_cast<float>(px_ - 1)));
        const int y0 = static_cast<int>(std::lround(v0 * static_cast<float>(px_ - 1)));
        const int y1 = static_cast<int>(std::lround(v1 * static_cast<float>(px_ - 1)));
        for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y) {
            for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) {
                put(x, y, c);
            }
        }
    }

    /// A filled triangle, by barycentric sign test — the same three-point form
    /// the ghost's outline uses, and cheap enough for a 32 px square.
    void triangle(float ax, float ay, float bx, float by, float cx, float cy, Rgba c) {
        const float s = static_cast<float>(px_ - 1);
        const float x0 = ax * s;
        const float y0 = ay * s;
        const float x1 = bx * s;
        const float y1 = by * s;
        const float x2 = cx * s;
        const float y2 = cy * s;
        const int lo_x = static_cast<int>(std::floor(std::min({x0, x1, x2})));
        const int hi_x = static_cast<int>(std::ceil(std::max({x0, x1, x2})));
        const int lo_y = static_cast<int>(std::floor(std::min({y0, y1, y2})));
        const int hi_y = static_cast<int>(std::ceil(std::max({y0, y1, y2})));
        const auto edge = [](float px, float py, float qx, float qy, float rx, float ry) {
            return (qx - px) * (ry - py) - (qy - py) * (rx - px);
        };
        for (int y = lo_y; y <= hi_y; ++y) {
            for (int x = lo_x; x <= hi_x; ++x) {
                const float fx = static_cast<float>(x);
                const float fy = static_cast<float>(y);
                const float w0 = edge(x0, y0, x1, y1, fx, fy);
                const float w1 = edge(x1, y1, x2, y2, fx, fy);
                const float w2 = edge(x2, y2, x0, y0, fx, fy);
                if ((w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                    || (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f)) {
                    put(x, y, c);
                }
            }
        }
    }

    /// The ground line every ground-facing icon stands on: one shape shared, so
    /// the set looks like a set.
    void ground(Rgba c) { line(0.10f, 0.80f, 0.90f, 0.80f, 0.09f, c); }

private:
    void put(int x, int y, Rgba c) {
        if (x < 0 || y < 0 || x >= px_ || y >= px_) {
            return;
        }
        const std::size_t at =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(px_)
             + static_cast<std::size_t>(x)) * 4u;
        (*rgba_)[at + 0] = c.r;
        (*rgba_)[at + 1] = c.g;
        (*rgba_)[at + 2] = c.b;
        (*rgba_)[at + 3] = c.a;
    }

    std::vector<std::uint8_t>* rgba_ = nullptr;
    int px_ = 0;
};

} // namespace

bool bake_tool_icon(ToolIcon icon, int size_px, std::vector<std::uint8_t>& rgba) {
    const int px = std::clamp(size_px, 8, 256);
    Canvas c(rgba, px);
    switch (icon) {
    case ToolIcon::Height:
        // ЗЕМЛЯ И СТРЕЛКА ВВЕРХ: кисть высоты тянет грунт.
        c.ground(EARTH);
        c.triangle(0.50f, 0.12f, 0.26f, 0.46f, 0.74f, 0.46f, ACCENT);
        c.rect(0.42f, 0.46f, 0.58f, 0.74f, ACCENT);
        return true;
    case ToolIcon::Surface:
        // ЗЕМЛЯ И МАЗОК: кисть поверхности красит, а не двигает — поэтому
        // стрелки нет вовсе, есть полоса краски с дитерингом.
        c.ground(EARTH);
        c.line(0.18f, 0.62f, 0.82f, 0.62f, 0.16f, INK);
        for (int i = 0; i < 6; ++i) {
            const float u = 0.20f + 0.12f * static_cast<float>(i);
            c.disc(u, 0.40f, 0.045f, ACCENT);
        }
        return true;
    case ToolIcon::Select:
        // РАМКА И УКАЗАТЕЛЬ: выбираю то, что уже стоит.
        c.line(0.14f, 0.16f, 0.86f, 0.16f, 0.07f, INK);
        c.line(0.14f, 0.84f, 0.86f, 0.84f, 0.07f, INK);
        c.line(0.14f, 0.16f, 0.14f, 0.84f, 0.07f, INK);
        c.line(0.86f, 0.16f, 0.86f, 0.84f, 0.07f, INK);
        c.triangle(0.42f, 0.34f, 0.42f, 0.72f, 0.68f, 0.58f, ACCENT);
        return true;
    case ToolIcon::Place:
        // КУБ НА ЗЕМЛЕ: постройка ставит деталь.
        c.ground(EARTH);
        c.rect(0.28f, 0.40f, 0.72f, 0.78f, INK);
        c.triangle(0.28f, 0.40f, 0.72f, 0.40f, 0.50f, 0.20f, ACCENT);
        return true;
    case ToolIcon::Plant:
        // ДЕРЕВО: посадка. Своя иконка, потому что теперь это СВОЙ инструмент,
        // а не Shift у чужого (docs/AUDIT_EDITOR_TOOLS.md).
        c.ground(EARTH);
        c.rect(0.46f, 0.52f, 0.54f, 0.80f, EARTH);
        c.disc(0.50f, 0.36f, 0.24f, ACCENT);
        c.disc(0.34f, 0.46f, 0.14f, ACCENT);
        c.disc(0.66f, 0.46f, 0.14f, ACCENT);
        return true;
    case ToolIcon::Path:
        // ЗЕМЛЯ, ИЗВИЛИСТАЯ ТРОПА И ЕЁ УЗЛЫ. Картинка обязана отличаться от
        // кисти поверхности НЕ ЦВЕТОМ, а формой: тропа — линия между точками,
        // и это ровно то, чем она отличается от мазка по клеткам.
        c.ground(EARTH);
        c.line(0.16f, 0.78f, 0.40f, 0.54f, 0.10f, ACCENT);
        c.line(0.40f, 0.54f, 0.62f, 0.52f, 0.10f, ACCENT);
        c.line(0.62f, 0.52f, 0.84f, 0.24f, 0.10f, ACCENT);
        c.disc(0.16f, 0.78f, 0.075f, INK);
        c.disc(0.40f, 0.54f, 0.075f, INK);
        c.disc(0.62f, 0.52f, 0.075f, INK);
        c.disc(0.84f, 0.24f, 0.075f, INK);
        return true;
    case ToolIcon::HouseVertex:
        // ШАРИК И ОТВЕС: якорь и пунктирная нить вниз, к земле — ровно то, что
        // инструмент рисует в мире, когда вершина стоит в воздухе.
        c.ground(EARTH);
        c.disc(0.50f, 0.28f, 0.16f, ACCENT);
        for (int i = 0; i < 4; ++i) {
            const float y = 0.46f + 0.09f * static_cast<float>(i);
            c.line(0.50f, y, 0.50f, y + 0.05f, 0.05f, INK);
        }
        return true;
    case ToolIcon::HouseLine:
        // ДВА ЯКОРЯ И БРУС МЕЖДУ НИМИ. Точки на концах обязательны: прямая
        // здесь тянется ОТ вершины К вершине, а не рисуется на пустом месте.
        c.ground(EARTH);
        c.line(0.24f, 0.68f, 0.76f, 0.30f, 0.12f, ACCENT);
        c.disc(0.24f, 0.68f, 0.10f, INK);
        c.disc(0.76f, 0.30f, 0.10f, INK);
        return true;
    case ToolIcon::HouseSurface:
        // ПОЛОТНО НА ЧЕТЫРЁХ ЯКОРЯХ: заливка и углы. От куба постройки её
        // отличает то же, что и в мире — у поверхности есть углы, которые
        // человек назвал сам, а у куба их нет.
        c.ground(EARTH);
        c.triangle(0.20f, 0.66f, 0.80f, 0.66f, 0.68f, 0.30f, ACCENT);
        c.triangle(0.20f, 0.66f, 0.68f, 0.30f, 0.32f, 0.30f, ACCENT);
        c.disc(0.20f, 0.66f, 0.085f, INK);
        c.disc(0.80f, 0.66f, 0.085f, INK);
        c.disc(0.68f, 0.30f, 0.085f, INK);
        c.disc(0.32f, 0.30f, 0.085f, INK);
        return true;
    case ToolIcon::Settings: {
        // ШЕСТЕРЁНКА: общие параметры, не инструмент — и по картинке видно, что
        // это не инструмент.
        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) * 3.14159265f / 4.0f;
            c.line(0.50f, 0.50f, 0.50f + 0.40f * std::cos(a),
                   0.50f + 0.40f * std::sin(a), 0.14f, INK);
        }
        c.disc(0.50f, 0.50f, 0.26f, INK);
        c.disc(0.50f, 0.50f, 0.11f, Rgba{40, 44, 48, 255});
        return true;
    }
    case ToolIcon::Count:
        break;
    }
    return false;
}

} // namespace dfn::app
