/*
Module: engine/app
File: engine/app/sources/MenuArt.h

Responsibility:
- THE DRAWING PRIMITIVES THE START SCREEN NEEDED AND THE ENGINE DID NOT HAVE:
  bitmap text at a whole-number magnification, a decoded .png fitted into a box
  with alpha, and the slow drift of motes over black. Nothing here knows what a
  menu is -- pages live in Menu.cpp.

Key items:
- text_width_scaled / draw_text_scaled: the 5x8 font blown up N times, with
  tracking. At 1920x1080 the unscaled font is 8 px tall, i.e. a fifth of the
  height of a Skyrim menu row -- the reference the owner named is unreachable
  without this.
- draw_image_fit: an Image centred in a box, aspect preserved, box-filtered
  down, alpha-composited.
- draw_sparks: поле комет главного экрана — белые и голубые искры, всплывающие
  снизу вверх со шлейфом, каждая со своим циклом жизни.
- draw_studio_splash: the Spiral Game Studios frame shown at launch.
- BRAND_*: the paths of the approved branding files (assets/branding/README.txt).

Dependencies:
- Uses: engine/render (PixelCanvas, BitmapFont), engine/app PngImage.
- Used by: engine/app Menu.cpp.

Notes:
- MAGNIFICATION IS A WHOLE NUMBER AND THE GLYPH IS A BLOCK. No smoothing: the
  font is a 1-bit mask, and interpolating a 1-bit mask produces grey fringes
  that read as a blurry screenshot rather than as a bigger letter.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
*/

#pragma once

#include <string_view>

#include "engine/render/sources/PixelCanvas.h"

namespace dfn::app {

struct Image;

// THE APPROVED BRANDING (assets/branding/README.txt, owner 26.08). Named once
// here so a file that moves is one edit rather than a hunt: the emblem of the
// Yaan Empire in the middle of the start screen, the studio's mark at launch
// and small in the corner.
// --- BRAND_SEAL_PNG: СНЯТА ---------------------------------------------------
// Здесь стояла строка пути "assets/branding/oak_seal/oak_seal_1024.png" —
// плоский герб-круг в центре главного меню. Заказ владельца 27.08 заменил его
// объёмным мешем (MenuEmblem.h: HERALDRY_OAK_DFO), и константа осталась без
// единого читателя. СНЯТА, А НЕ ОСТАВЛЕНА «на всякий случай»: путь без
// читателя следующий агент примет за забытое и включит обратно.
// САМ ФАЙЛ НА МЕСТЕ и снимать его нельзя: из соседнего oak_silhouette_black.png
// печётся меш, размерный ряд числится в assets/branding/README.txt, а
// tests/app/PngImageTests.cpp сверяет по этим файлам свой декодер .png.
inline constexpr const char* BRAND_SPIRAL_ICON_PNG =
    "assets/branding/spiral_logo/spiral_icon_transparent_256.png";
inline constexpr const char* BRAND_SPIRAL_FULL_PNG =
    "assets/branding/spiral_logo/spiral_logo_full_1024.png";

/// Width in pixels of `utf8` drawn at `scale` with `tracking` extra pixels
/// between cells. There is no trailing tracking, so the value is the exact ink
/// extent a right-aligned column must be measured by.
[[nodiscard]] int text_width_scaled(std::string_view utf8, int scale, int tracking);

/// Height of one line at `scale` (the ink box, not the cell): what a caller
/// stacking rows adds its own gap to.
[[nodiscard]] int text_height_scaled(int scale);

/// Draws `utf8` with the first cell's top-left at (x, y), each font pixel a
/// scale x scale block. `shadow` offsets a black copy by `scale` pixels, which
/// is what keeps light letters readable over the dimmed world on the pause
/// page. Returns the advance in pixels.
int draw_text_scaled(render::PixelCanvas& canvas, int x, int y, std::string_view utf8,
                     render::Color color, int scale, int tracking, bool shadow = false);

/// Centres `image` in the box (box_x, box_y, box_w, box_h) with its aspect
/// preserved, and composites it at `alpha` (0..1) over what is already there.
/// A downscale averages the source rectangle (a box filter) rather than picking
/// one pixel: the emblem is 1024 px wide and lands at ~700, and point sampling
/// an antialiased edge at that ratio is what makes a logo look ragged.
void draw_image_fit(render::PixelCanvas& canvas, const Image& image, int box_x,
                    int box_y, int box_w, int box_h, float alpha);

/// ПОЛЕ ИСКР: `count` комет, всплывающих СНИЗУ ВВЕРХ над тем, что уже лежит на
/// холсте. У каждой — свой цвет (белый #f0f2f4 или голубой #4a90d9, цвета
/// студии Spiral), свой затухающий шлейф за головой и свой цикл жизни
/// (рождение -> полёт -> угасание -> тёмная пауза). Ни одной живой частицы в
/// памяти: всё выводится из номера искры и `time_s`, поэтому два прогона на
/// одной секунде рисуют одно поле и кадр приёмки воспроизводим (правило 13).
/// Дверь `DFN_MENU_SPARK_PHASE=<секунды>` прикалывает `time_s`: беспилотный
/// прогон снимает кадр в произвольной секунде часов меню, и без двери две руки
/// замера сравнивали бы разные поля.
void draw_sparks(render::PixelCanvas& canvas, float time_s, int count);

/// The launch frame: the studio's full lock-up on its own dark ground, faded in
/// and out over `total_s` seconds. `t_s` is how long the frame has been up.
void draw_studio_splash(render::PixelCanvas& canvas, float t_s, float total_s);

} // namespace dfn::app
