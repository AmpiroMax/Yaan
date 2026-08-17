/*
Created: 14:08:2026 - 18:57:03
Last updated: 18:08:2026 - 00:24:58
Module: engine/app
File: engine/app/sources/EditorHud.cpp

Responsibility:
- Composition and layout of the editor viewer's overlay block. See the header
  for why it knows nothing about the renderer.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 14:08:2026 - 18:57:03: Создан вместе с заголовком — вынос блока из App.cpp и
  разведение с отладочным выводом. Тексты строк перенесены БЕЗ ИЗМЕНЕНИЙ:
  правка геометрическая, и смешивать её с правкой формулировок значило бы
  сдавать два изменения под одним доказательством.
- 14:08:2026 - 19:05:56: Формулировки. «трис 1758233   дро 122» стало «В кадре:
  треугольников 1758233   вызовов отрисовки 122», а «прицел: 1476 трис 0.8 м» —
  «Под прицелом: треугольников 1476   до него 0.8 м   объект 42»: пользователь
  спрашивал, что это такое, и теперь строка сама говорит, ЧТО именно считается —
  весь кадр или один объект под прицелом — и где дистанция. Числительное после
  существительного не по вкусу, а из-за русской счётной формы: «%d треугольник»
  сломался бы на 2 и на 5.
- 14:08:2026 - 19:39:08: Инверсия штампа pick_id на строке прицела. Сентинел 0
  (рельеф, небо, узлы LOD сабмитятся без имени) не показывается ВОВСЕ — это
  обычный случай, прицел большую часть времени стоит на земле, и «объект 0»
  назвал бы настоящий слот, на который никто не смотрит.
- 18:08:2026 - 00:24:58: ЧЕТВЁРТАЯ СТРОКА — мышь и угол (заказ 18.08). Намеренно
  почти вся из цифр и латиницы: её читают, сравнивая ДВА числа между собой, а не
  переводя. Слева пришедшее за кадр смещение, справа — куда смотрит камера; живое
  левое при стоящем правом винит камеру, два стоящих — ввод. Состояние курсора
  тут же (cap/free), потому что при отданном интерфейсу курсоре нулевое смещение
  это НОРМА, и без пометки строка врала бы каждый раз, когда открыто меню.
*/

#include "engine/app/sources/EditorHud.h"

#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace dfn::app {
namespace {

// THE BLOCK'S CORNER. Left-aligned like the readout it stacks under: the
// alternative -- right-aligning it against the far edge -- was rejected because
// line 3 changes width every time the crosshair moves, and a right-aligned line
// that changes width makes the whole block slide sideways while the player is
// trying to read it.
constexpr int BLOCK_X = 4;
// A row of air between the readout's plate and this block's first line, so the
// two read as two panels rather than as one panel with a seam.
constexpr int BLOCK_GAP = 4;
constexpr int PLATE_PAD = 3; // draw_text_plate's default

[[nodiscard]] std::string loc_str(const char* key) {
    return std::string(localized(serialization::fnv1a64(key)));
}

// THE DOSE DOOR (Rule 47's one-binary clause): DFN_EDITOR_HUD_PINNED=1 puts the
// block back where it was -- nailed to (4, 4) on top of the readout -- so the
// before frame and the after frame come out of ONE build rather than two builds
// an hour apart in a tree six zones are compiling. Without it the "before" of
// this fix is unphotographable the moment the fix lands, and a layout claim
// with no before is a claim nobody can check.
//
// Read ONCE per process: a door polled every frame is a switch, and a switch
// inside an instrument lets two frames of one run disagree about what was
// tested. Same reasoning, same shape, as DFN_UI_PLATE next door.
[[nodiscard]] bool pinned_to_corner() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_EDITOR_HUD_PINNED");
        return e != nullptr && e[0] == '1';
    }();
    return on;
}

/// САМОЕ ВЫСОКОЕ СОСТОЯНИЕ ОТЛАДОЧНОГО ВЫВОДА: в воде он на строку выше, чем на
/// суше. Раскладка обязана держаться в ХУДШЕМ случае, иначе блок разъезжается
/// ровно тогда, когда человек заходит в воду, — а это и есть тот кадр, на
/// котором наложение никто не искал.
[[nodiscard]] DebugSnapshot worst_case_readout() {
    DebugSnapshot s;
    s.water_depth = 1.0f;
    return s;
}

} // namespace

bool aim_entity_index(uint32_t pick_id, uint32_t& out_index) {
    if (pick_id == 0) {
        return false; // the contract's "unnamed": not an entity at all
    }
    out_index = pick_id - 1u;
    return true;
}

int editor_hud_row_h() { return render::FONT_CELL_H + 2; }

int editor_hud_x() { return BLOCK_X; }

int editor_hud_block_height_px(size_t line_count) {
    if (line_count == 0) {
        return 0;
    }
    // Each line carries its own plate, so the block's extent runs from the top
    // of the first plate to the bottom of the last one.
    return static_cast<int>(line_count - 1) * editor_hud_row_h()
         + render::FONT_INK_H + 2 * PLATE_PAD;
}

std::vector<std::string> editor_hud_lines(const EditorHudSnapshot& snap,
                                          int width_px, int height_px) {
    // The width every line has to live inside: the frame, less the block's own
    // left margin and the plate's margin on the right.
    const int budget = width_px - BLOCK_X - PLATE_PAD;

    std::vector<std::string> lines;
    lines.reserve(3);

    char num[32];
    const std::string tri_short = loc_str("editor.hud.tris.short");

    // EACH LINE IS BUILT TWICE, FULL AND SHORT, AND CHOSEN BY MEASURING. The
    // full form is a sentence -- it exists because the user asked what "трис"
    // meant, and an abbreviation cannot answer that. The short form exists
    // because 320x180 is a rung the settings page offers one keypress away,
    // and a sentence does not fit there. Which one is drawn is decided by
    // fits_width on the ASSEMBLED string, so the answer accounts for the
    // numbers too, not just the words.

    // LINE 1 -- the banner: which mode this is and how fast the camera flies.
    std::snprintf(num, sizeof(num), "%.1f", static_cast<double>(snap.fly_speed_mps));
    const std::string speed = std::string(" ") + num + " " + loc_str("editor.speed_unit");
    lines.push_back(std::string(
        fits_width(budget, loc_str("editor.banner") + speed,
                   loc_str("editor.banner.short") + speed)));

    // LINE 2 -- what the WHOLE FRAME costs.
    const std::string tag =
        snap.wireframe ? "   [" + loc_str("editor.hud.wire") + "]" : std::string{};
    const std::string tris = std::to_string(snap.frame_triangles);
    const std::string draws = std::to_string(snap.frame_draws);
    lines.push_back(std::string(fits_width(
        budget,
        loc_str("editor.hud.frame") + " " + tris + "   " + loc_str("editor.hud.draws")
            + " " + draws + tag,
        loc_str("editor.hud.frame.short") + " " + tris + " " + tri_short + "   " + draws
            + " " + loc_str("editor.hud.draws.short") + tag)));

    // LINE 3 -- what the ONE OBJECT under the crosshair costs, and how far off
    // it is. These are the two halves the user asked to be told apart.
    if (snap.aim_hit) {
        std::snprintf(num, sizeof(num), "%.1f", static_cast<double>(snap.aim_distance_m));
        const std::string dist = num;
        const std::string m = loc_str("editor.hud.m");
        const std::string at = std::to_string(snap.aim_triangles);
        // The id run is present only when the draw carried one: terrain, sky
        // and the LOD nodes submit unnamed, and "объект 0" would name a slot
        // that is not the one being looked at.
        std::string id_full;
        std::string id_short;
        uint32_t index = 0;
        if (aim_entity_index(snap.aim_pick_id, index)) {
            const std::string id = std::to_string(index);
            id_full = "   " + loc_str("editor.hud.object") + " " + id;
            id_short = "   #" + id;
        }
        lines.push_back(std::string(fits_width(
            budget,
            loc_str("editor.hud.aim") + " " + at + "   " + loc_str("editor.hud.distance")
                + " " + dist + " " + m + id_full,
            loc_str("editor.hud.aim.short") + " " + at + " " + tri_short + "   " + dist
                + " " + m + id_short)));
    } else {
        lines.push_back(std::string(fits_width(budget, loc_str("editor.hud.aim.none"),
                                               loc_str("editor.hud.aim.none.short"))));
    }

    // СТРОКА 4 -- МЫШЬ И УГОЛ. Заказ пользователя 18.08. Она намеренно почти
    // вся из цифр и латиницы: её читают, сравнивая ДВА числа между собой, а не
    // переводя. Слева пришедшее за кадр смещение мыши, справа — куда смотрит
    // камера. Если левое живое, а правое стоит — виновата камера; если стоят
    // оба — мышь до нас не дошла. Разделить это на глаз было нечем, и потому
    // «в режиме редактора не работает камера» три захода подряд разбирал
    // человек за игрой.
    //
    // Состояние курсора здесь же, потому что поломка оказалась ровно в нём:
    // cap=1 значит, что окно держит указатель, free=1 — что он отдан панелям
    // клавишей R. При free=1 нулевое смещение — это НОРМА, а не отказ, и без
    // этой пометки строка врала бы каждый раз, когда человек открыл меню.
    // ...И ОНА ПОЯВЛЯЕТСЯ, ТОЛЬКО ЕСЛИ ВЛЕЗАЕТ. На 320x180 четвёртая строка
    // выталкивает блок на подсказку снимка внизу — то самое наложение, на
    // которое пользователь уже жаловался. Считается по самому высокому
    // состоянию вывода (в воде он на строку выше), потому что раскладка обязана
    // держаться в ХУДШЕМ случае, а не в среднем: иначе блок разъезжается ровно
    // тогда, когда человек заходит в воду.
    const int probe_top = editor_hud_top_y(debug_overlay_bottom_y(worst_case_readout()));
    const int probe_h = editor_hud_block_height_px(lines.size() + 1);
    const bool probe_fits = probe_top + probe_h <= height_px
                         && probe_top + probe_h <= debug_overlay_hint_top_y(height_px);
    if (probe_fits) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "mouse %+.1f %+.1f   yaw %.1f  pitch %.1f   cap%d free%d",
                      static_cast<double>(snap.mouse_dx), static_cast<double>(snap.mouse_dy),
                      static_cast<double>(snap.yaw_deg), static_cast<double>(snap.pitch_deg),
                      snap.cursor_captured ? 1 : 0, snap.cursor_free ? 1 : 0);
        char shortbuf[64];
        std::snprintf(shortbuf, sizeof(shortbuf), "m %+.0f %+.0f  y %.0f p %.0f  c%d f%d",
                      static_cast<double>(snap.mouse_dx), static_cast<double>(snap.mouse_dy),
                      static_cast<double>(snap.yaw_deg), static_cast<double>(snap.pitch_deg),
                      snap.cursor_captured ? 1 : 0, snap.cursor_free ? 1 : 0);
        lines.push_back(std::string(fits_width(budget, buf, shortbuf)));
    }

    return lines;
}

int draw_editor_hud(render::PixelCanvas& canvas, const EditorHudSnapshot& snap,
                    int top_y) {
    const render::Color ink{244, 226, 160};
    const std::vector<std::string> lines =
        editor_hud_lines(snap, static_cast<int>(canvas.width()),
                         static_cast<int>(canvas.height()));

    int y = top_y;
    for (const std::string& line : lines) {
        draw_text_plate(canvas, BLOCK_X, y, render::text_width_px(line),
                        render::FONT_INK_H);
        render::draw_text(canvas, BLOCK_X, y, line, ink, /*shadow=*/true);
        y += editor_hud_row_h();
    }
    return y - editor_hud_row_h() + render::FONT_INK_H + PLATE_PAD;
}

int editor_hud_top_y(int readout_bottom_y) {
    // The door's whole effect is here: the block either follows the readout or
    // ignores it, which is exactly the difference the two frames show.
    return pinned_to_corner() ? BLOCK_X : readout_bottom_y + BLOCK_GAP;
}

} // namespace dfn::app
