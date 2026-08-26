/*
Created: 27:08:2026 - 03:06:00
Module: engine/app
File: engine/app/sources/UiFont.cpp

Responsibility:
- Чтение .fnt + атласов и выкладывание покрытия на холст. Устройство и доводы —
  в заголовке; формат актива — в tools/bake_ui_font.py.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
*/
/*
UPD:
- 27:08:2026 - 03:06:00: Создан вместе с заголовком.
*/

#include "engine/app/sources/UiFont.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/app/sources/PngImage.h"
#include "engine/render/sources/BitmapFont.h" // utf8_next: ОДИН декодер UTF-8 на дерево

namespace dfn::app {

namespace {

struct Glyph {
    int x = 0, y = 0, w = 0, h = 0; // прямоугольник в атласе
    int ox = 0, oy = 0;             // смещение от пера (верх строки) до него
    int advance = 0;
};

struct Face {
    int px = 0;
    int line = 0;
    int ascent = 0;
    int cap = 0; // высота прописной, измеренная по «Н» при загрузке
    std::string atlas;
    std::unordered_map<uint32_t, Glyph> glyphs;
};

struct FontTable {
    std::vector<Face> faces; // по возрастанию кегля
    bool loaded = false;
};

FontTable& table() {
    // ЧИТАЕТСЯ ОДИН РАЗ, ВКЛЮЧАЯ НЕУДАЧУ: экран рисуется каждый кадр, и
    // отсутствующий актив обязан жаловаться однажды, а не шестьдесят раз в
    // секунду (тот же приём, что у cached_png и у проигрывателя интро).
    static FontTable t = [] {
        FontTable out;
        std::ifstream in(UI_FONT_METRICS);
        if (!in.is_open()) {
            std::fprintf(stderr,
                         "[шрифт] нет \"%s\" — интерфейс останется на блочном "
                         "шрифте (пересобрать: python3 tools/bake_ui_font.py)\n",
                         UI_FONT_METRICS);
            return out;
        }
        std::string line;
        Face* face = nullptr;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::istringstream ls(line);
            std::string word;
            ls >> word;
            if (word == "size") {
                Face f;
                std::string key;
                ls >> f.px >> key >> f.atlas >> key >> f.line >> key >> f.ascent;
                out.faces.push_back(f);
                face = &out.faces.back();
            } else if (word == "g" && face != nullptr) {
                uint32_t cp = 0;
                Glyph g;
                ls >> cp >> g.x >> g.y >> g.w >> g.h >> g.ox >> g.oy >> g.advance;
                face->glyphs[cp] = g;
            }
        }
        std::sort(out.faces.begin(), out.faces.end(),
                  [](const Face& a, const Face& b) { return a.px < b.px; });
        for (Face& f : out.faces) {
            // ВЫСОТА ПРОПИСНОЙ ИЗМЕРЯЕТСЯ, А НЕ БЕРЁТСЯ ДОЛЕЙ КЕГЛЯ. Доля верна
            // для этого шрифта и молча неверна для следующего, а раскладка
            // страниц стоит именно на ней.
            const auto it = f.glyphs.find(0x041D); // «Н»
            f.cap = it != f.glyphs.end() ? it->second.h : f.px * 7 / 10;
        }
        out.loaded = !out.faces.empty();
        if (out.loaded) {
            std::fprintf(stderr, "[шрифт] %s: %zu ступеней (%d..%d px), %zu знаков\n",
                         UI_FONT_METRICS, out.faces.size(), out.faces.front().px,
                         out.faces.back().px, out.faces.front().glyphs.size());
        }
        return out;
    }();
    return t;
}

/// Ступень, ближайшая к запрошенному кеглю. НИКОГДА не масштабирует: растянутое
/// покрытие — это второе размытие поверх сглаживания, то самое «мыло».
const Face* pick(int px) {
    FontTable& t = table();
    if (t.faces.empty()) {
        return nullptr;
    }
    const Face* best = &t.faces.front();
    int best_d = std::abs(best->px - px);
    for (const Face& f : t.faces) {
        const int d = std::abs(f.px - px);
        if (d < best_d) {
            best_d = d;
            best = &f;
        }
    }
    return best;
}

const Image& atlas_of(const Face& f) {
    static const std::string dir = "assets/fonts/";
    return cached_png(dir + f.atlas);
}

// ДОЛИ ВЫСОТЫ ХОЛСТА НА РОЛЬ. Вывод чисел — в заголовке (высота прописной с
// принятых кадров, делённая на 0.70 — высоту прописной PT Serif в долях кегля).
float role_fraction(UiText role) {
    switch (role) {
    case UiText::Accent:  return 0.083f; // 1080 -> 90 px, прописная ~63 (5.8 %)
    case UiText::Title:   return 0.070f; // 1080 -> 76
    case UiText::Item:    return 0.056f; // 1080 -> 60 px, прописная ~42 (3.9 %)
    case UiText::Caption: return 0.038f; // 1080 -> 41
    case UiText::Small:   return 0.028f; // 1080 -> 30
    }
    return 0.056f;
}

} // namespace

bool ui_font_ready() { return table().loaded; }

int ui_px(int canvas_h, UiText role) {
    if (!table().loaded || canvas_h <= 0) {
        return 0;
    }
    const int want = std::max(1, static_cast<int>(std::lround(
                                     static_cast<float>(canvas_h) * role_fraction(role))));
    const Face* f = pick(want);
    return f != nullptr ? f->px : 0;
}

int ui_line_height(int px) {
    const Face* f = pick(px);
    return f != nullptr ? f->line : 0;
}

int ui_cap_height(int px) {
    const Face* f = pick(px);
    return f != nullptr ? f->cap : 0;
}

int ui_text_width(std::string_view utf8, int px) {
    const Face* f = pick(px);
    if (f == nullptr) {
        return 0;
    }
    int w = 0;
    size_t pos = 0;
    while (pos < utf8.size()) {
        const uint32_t cp = render::utf8_next(utf8, pos);
        const auto it = f->glyphs.find(cp);
        // ОТСУТСТВУЮЩИЙ ЗНАК ЗАНИМАЕТ МЕСТО. Ширина, посчитанная без него, и
        // отрисовка, которая его всё-таки чем-то заполнит, разъехались бы —
        // а расходятся такие пары ровно на чужом переводе.
        w += it != f->glyphs.end() ? it->second.advance : f->px / 2;
    }
    return w;
}

int ui_draw_text(render::PixelCanvas& canvas, int x, int y, std::string_view utf8,
                 render::Color color, int px, bool shadow) {
    const Face* f = pick(px);
    if (f == nullptr) {
        return 0;
    }
    if (shadow) {
        // Смещение растёт с кеглем: тень в один пиксель под буквой в 90 px —
        // это грязь на кромке, а не подложка.
        const int off = std::max(1, f->px / 22);
        ui_draw_text(canvas, x + off, y + off, utf8, render::Color{0, 0, 0}, px, false);
    }
    const Image& atlas = atlas_of(*f);
    if (atlas.empty()) {
        return 0;
    }
    const int cw = static_cast<int>(canvas.width());
    const int chh = static_cast<int>(canvas.height());
    const auto px_at = canvas.pixels();

    // y — ВЕРХ ПРОПИСНОЙ, А НЕ ВЕРХ СТРОКИ, и это не мелочь раскладки, а
    // условие, при котором вся арифметика страниц осталась прежней. Блочный
    // шрифт клал чернила прямо в (x, y); у настоящего шрифта между верхом
    // строки и верхом буквы лежит подъём выносных элементов, которых в
    // «Продолжить» нет. Первый кадр после перехода поймал это дважды: черта под
    // выбранным пунктом (её кладут в y + высота_прописной) прошла ПО СЕРЕДИНЕ
    // слова, а линейка под заголовком настроек — сквозь заголовок. Сдвиг на
    // (подъём − высота прописной) ставит букву туда, где её ждёт раскладка, и
    // заодно делает ящик строки (menu_row_boxes) тем, что игрок видит, —
    // а по нему целится мышь.
    const int cap_top = f->ascent - f->cap;

    int pen = x;
    size_t pos = 0;
    while (pos < utf8.size()) {
        const uint32_t cp = render::utf8_next(utf8, pos);
        const auto it = f->glyphs.find(cp);
        if (it == f->glyphs.end()) {
            pen += f->px / 2;
            continue;
        }
        const Glyph& g = it->second;
        for (int gy = 0; gy < g.h; ++gy) {
            const int dy = y + g.oy - cap_top + gy;
            if (dy < 0 || dy >= chh) {
                continue;
            }
            for (int gx = 0; gx < g.w; ++gx) {
                const int dx = pen + g.ox + gx;
                if (dx < 0 || dx >= cw) {
                    continue;
                }
                // ПОКРЫТИЕ ЛЕЖИТ В ЯРКОСТИ. Атлас — серый PNG (тип 0), и наш
                // читатель раскладывает его как r=g=b=покрытие, a=255. Брать
                // альфу было бы неверно: она там всегда единица.
                const uint8_t cov = atlas.at(g.x + gx, g.y + gy)[0];
                if (cov == 0) {
                    continue;
                }
                if (cov == 255) {
                    canvas.put(dx, dy, color);
                    continue;
                }
                const size_t at = (static_cast<size_t>(dy) * canvas.width()
                                   + static_cast<size_t>(dx)) * 4u;
                // СГЛАЖИВАТЬ МОЖНО ТОЛЬКО ПО НЕПРОЗРАЧНОМУ, и это не
                // осторожность, а ограничение холста: PixelCanvas::put выставляет
                // альфу 255 всегда, поэтому полупокрытый пиксель над ПРОЗРАЧНЫМ
                // фоном стал бы непрозрачным пикселем цвета, смешанного с
                // прозрачной чернотой, — то есть тёмным ореолом вокруг каждой
                // буквы поверх мира. Слой HUD чистится прозрачным, и там
                // покрытие режется порогом: кромка выходит резкой, как у
                // блочного шрифта, но чистой. Меню и всё, что стоит на плашке,
                // непрозрачны и получают полное сглаживание.
                if (px_at[at + 3] == 0) {
                    if (cov >= 128) {
                        canvas.put(dx, dy, color);
                    }
                    continue;
                }
                const int a = cov;
                const int inv = 255 - a;
                canvas.put(dx, dy,
                           render::Color{
                               static_cast<uint8_t>((color.r * a + px_at[at] * inv) / 255),
                               static_cast<uint8_t>((color.g * a + px_at[at + 1] * inv) / 255),
                               static_cast<uint8_t>((color.b * a + px_at[at + 2] * inv) / 255)});
            }
        }
        pen += g.advance;
    }
    return pen - x;
}

} // namespace dfn::app
