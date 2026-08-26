/*
Created: 10:08:2026 - 19:11:04
Last updated: 27:08:2026 - 14:00:00
Module: engine/app
File: engine/app/sources/DebugOverlay.cpp

Responsibility:
- Implementation of the F3 readout and the F2 capture/restore sidecar.

Dependencies:
- Uses: engine/render (PixelCanvas, BitmapFont), engine/app Localization.
- Used by: App only.

Notes:
- FIELD LABELS ARE ASCII MNEMONICS, NOT PROSE, and that is a deliberate reading
  of Rule 5 rather than an exemption from it. `pos`, `yaw`, `fps` are the same
  symbols in every language a developer reads; translating them would make the
  readout HARDER to compare against a bug report. The one thing here that is a
  WORD -- the compass direction -- goes through localization like everything
  else, because "north" is prose and "N" is not universally read as it.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 10:08:2026 - 19:11:04: Created (user request: debug readout + restorable state capture).
- 13:08:2026 - 16:30:00: Плашка под обеими надписями (зона ui). Не вкусовая правка:
  над небом 55.4 % чернил стояли ближе двух шагов квантователя к фону, над тёмной
  землёй — 0.0 %, а угол, в котором живёт вывод, — это угол, в котором живёт небо.
  Плашка непрозрачная и постоянная; обоснование отказа от дизеринга и от условной
  плашки — в комментарии у самого кода.
- 13:08:2026 - 16:48:00: Дверь дозы DFN_UI_PLATE=0 (правило 47, оговорка про один
  бинарник, заведена ведущим сегодня): обе руки приёмки обязаны выходить из ОДНОЙ
  сборки, иначе в общем дереве меряется чужая работа за день. Тот же ключ читает
  Menu.cpp.
- 13:08:2026 - 17:05:00: Плашка стала ОДНОЙ функцией на весь интерфейс (draw_text_plate)
  вместо копии на каждое место: земля под текстом — одно решение. Кромка панели
  запрашивается со всех четырёх сторон и сама отсекается на границе кадра, поэтому
  прижатая к углу плашка теряет ровно те две стороны, которые и должна.
- 14:08:2026 - 18:57:57: Угол вывода назван константами, число строк считает одна
  функция, и draw проверяет свой массив против неё static_assert'ом: добавить строку
  в вывод, не сдвинув то, что под ним, теперь ОШИБКА КОМПИЛЯЦИИ, а не наложение,
  которое кто-то должен заметить. Подсказка снимка берёт свой y из той же функции,
  что и запрос снаружи, — иначе это две арифметики про одну строку.
- 17:08:2026 - 22:01:29: Начало блока (origin_x/origin_y) — см. шапку DebugOverlay.h.
  Все координаты вывода отсчитываются от него, поэтому вызывающий двигает блок
  ОДНИМ числом, а не десятью, и число это — уже посчитанная кем-то полоса, а не
  подобранный отступ.
- 18:08:2026 - 17:32:10: чтение двери — через таблицу (door_value, AppDoors.h). Слой 2
  разбора App.cpp: имя без строки в таблице больше не открывается и говорит об
  этом вслух, поэтому «какие вообще есть двери» перестало быть вопросом к grep.
- 27:08:2026 - 14:00:00: Подсказка «3 — снимок состояния, 5 — снимок экрана»
  СНЯТА (заказ владельца 27.08: «весь вспомогательный текст подсказок ВООБЩЕ
  отовсюду удалить»). Она и была инструкцией по клавишам — о чём говорил её
  собственный прежний комментарий. Измерения остались все до одного.
*/

#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/AppDoors.h"

#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace dfn::app {
namespace {

constexpr float PI_F = 3.14159265358979323846f;

// One line of the readout. `snprintf` rather than iostreams or std::format:
// the readout runs every frame, and a fixed buffer with a known width is the
// cheapest thing that cannot allocate in the frame loop.
struct Line {
    char text[96]{};
};

[[nodiscard]] Line fmt(const char* format, ...) {
    Line line{};
    va_list args;
    va_start(args, format);
    std::vsnprintf(line.text, sizeof(line.text), format, args);
    va_end(args);
    return line;
}

// The eight-point compass. Sim's yaw convention is 0 = -Z and positive turns
// clockwise seen from above, so -Z is north and +X is east.
constexpr const char* COMPASS_KEYS[8] = {
    "debug.compass.n",  "debug.compass.ne", "debug.compass.e",  "debug.compass.se",
    "debug.compass.s",  "debug.compass.sw", "debug.compass.w",  "debug.compass.nw",
};

[[nodiscard]] const char* gait_key(uint8_t gait) {
    switch (gait) {
    case 0: return "debug.gait.walk";
    case 1: return "debug.gait.jog";
    case 2: return "debug.gait.run";
    default: return "debug.gait.unknown";
    }
}

[[nodiscard]] const char* locomotion_key(uint8_t loco) {
    switch (loco) {
    case 0: return "debug.loco.ground";
    case 1: return "debug.loco.wade";
    case 2: return "debug.loco.swim";
    default: return "debug.loco.unknown";
    }
}

// The plate's two colours ARE THE MENU'S two colours: one interface, one ground.
constexpr render::Color PLATE{18, 20, 26};
constexpr render::Color PLATE_EDGE{54, 56, 64};

// THE READOUT'S CORNER, named once. It is the top-left one, it is shared with
// the editor's own block, and the two used to disagree about it by a pixel
// while overlapping completely -- see debug_overlay_bottom_y in the header.
constexpr int READOUT_X = 3;
constexpr int READOUT_Y = 3;
// draw_text_plate's default margin. Named here because the block BELOW the
// readout has to clear the plate, not the text.
constexpr int READOUT_PLATE_PAD = 3;

// The rows drawn unconditionally. Kept as a constant so the height query and
// the draw cannot drift apart: the draw static_asserts its array against it,
// so adding a line to the readout without moving what sits under it is a
// COMPILE error rather than an overlap somebody has to notice (Rule 39).
constexpr int READOUT_FIXED_LINES = 10;

[[nodiscard]] int readout_line_count(const DebugSnapshot& snap) {
    // The fixed rows, plus the words row, plus the water row when there is
    // water. The water row is the whole reason this is a function: it makes
    // the readout's height a property of the MOMENT, not of the build.
    return READOUT_FIXED_LINES + 1 + (snap.water_depth > 0.0f ? 1 : 0);
}

// Trims ASCII spaces and tabs from both ends.
[[nodiscard]] std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// FrameClock
// ---------------------------------------------------------------------------

void FrameClock::push(float dt_seconds) {
    // A zero or negative delta is not a fast frame, it is a broken clock --
    // admitting it would report an infinite fps and quietly poison the mean.
    if (!(dt_seconds > 0.0f)) {
        return;
    }
    samples_[next_] = dt_seconds;
    next_ = (next_ + 1) % WINDOW;
    count_ = std::min(count_ + 1, WINDOW);
}

float FrameClock::mean_ms() const {
    if (count_ == 0) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (int i = 0; i < count_; ++i) {
        sum += samples_[i];
    }
    return (sum / static_cast<float>(count_)) * 1000.0f;
}

float FrameClock::worst_ms() const {
    if (count_ == 0) {
        return 0.0f;
    }
    float worst = samples_[0];
    for (int i = 1; i < count_; ++i) {
        worst = std::max(worst, samples_[i]);
    }
    return worst * 1000.0f;
}

float FrameClock::fps() const {
    const float ms = mean_ms();
    return ms > 0.0f ? 1000.0f / ms : 0.0f;
}

// ---------------------------------------------------------------------------
// Compass
// ---------------------------------------------------------------------------

uint64_t compass_key_for_yaw(float yaw_radians) {
    // Wrap into [0, 2pi) BEFORE bucketing. std::fmod keeps the sign of the
    // dividend, so a negative yaw -- which the look code produces the moment
    // the player turns left past north -- would index out of the table.
    const float two_pi = 2.0f * PI_F;
    float y = std::fmod(yaw_radians, two_pi);
    if (y < 0.0f) {
        y += two_pi;
    }
    // Offset by half a sector so each name is CENTRED on its direction:
    // without it, "north" would mean 0..45 degrees instead of -22.5..+22.5,
    // and the readout would disagree with the player's own sense of facing.
    const int sector =
        static_cast<int>(std::floor((y + PI_F / 8.0f) / (PI_F / 4.0f))) % 8;
    return serialization::fnv1a64(COMPASS_KEYS[sector]);
}

// ---------------------------------------------------------------------------
// The interface's ground (see the header for the measurement it comes from)
// ---------------------------------------------------------------------------

bool ui_plates_enabled() {
    // Read ONCE. A door polled every frame is a switch, and a switch inside an
    // instrument means two frames of one run can disagree about what was tested.
    static const bool on = [] {
        const char* e = door_value("DFN_UI_PLATE");
        return !(e != nullptr && e[0] == '0');
    }();
    return on;
}

void draw_text_plate(render::PixelCanvas& canvas, int text_x, int text_y, int text_w,
                     int text_h, int pad) {
    if (!ui_plates_enabled() || text_w <= 0 || text_h <= 0) {
        return;
    }
    const int x = text_x - pad;
    const int y = text_y - pad;
    const int w = text_w + 2 * pad;
    const int h = text_h + 2 * pad;
    canvas.fill_rect(x, y, w, h, PLATE);
    // All four sides are asked for; the ones outside the canvas clip away in
    // PixelCanvas::put, which is exactly what makes a corner-pinned plate read
    // as pinned rather than as a box floating against the frame border.
    canvas.frame_rect(x, y, w, h, PLATE_EDGE);
}

// ---------------------------------------------------------------------------
// Readout
// ---------------------------------------------------------------------------

void draw_debug_overlay(render::PixelCanvas& canvas, const DebugSnapshot& snap,
                        int origin_x, int origin_y) {
    // WHERE THE BLOCK STARTS, not where it is pinned. Every coordinate below
    // is measured from here, so a caller that has been told the top strip is
    // taken moves the readout by one number instead of by ten.
    const int read_x = READOUT_X + origin_x;
    const int read_y = READOUT_Y + origin_y;
    const int w = static_cast<int>(canvas.width());
    const int line_h = render::FONT_CELL_H + 1;
    const render::Color ink{232, 228, 214};
    const render::Color dim{168, 164, 152};
    const render::Color warn{232, 168, 96};

    const std::string_view compass = localized(compass_key_for_yaw(snap.yaw));
    const std::string_view gait = localized(serialization::fnv1a64(gait_key(snap.gait)));
    const std::string_view loco =
        localized(serialization::fnv1a64(locomotion_key(snap.locomotion)));

    const float deg = 180.0f / PI_F;

    Line left[] = {
        fmt("fps %5.1f  %.1f ms  max %.1f ms", static_cast<double>(snap.fps),
            static_cast<double>(snap.frame_ms), static_cast<double>(snap.frame_ms_worst)),
        fmt("pos %8.2f %8.2f %8.2f", static_cast<double>(snap.position.x),
            static_cast<double>(snap.position.y), static_cast<double>(snap.position.z)),
        fmt("yaw %7.2f  pitch %7.2f", static_cast<double>(snap.yaw * deg),
            static_cast<double>(snap.pitch * deg)),
        fmt("dir %6.3f %6.3f %6.3f", static_cast<double>(snap.look_dir.x),
            static_cast<double>(snap.look_dir.y), static_cast<double>(snap.look_dir.z)),
        fmt("spd %5.2f m/s  vy %6.2f  phase %.2f", static_cast<double>(snap.speed_mps),
            static_cast<double>(snap.vertical_velocity),
            static_cast<double>(snap.stride_phase)),
        fmt("day %.4f  lunar %.3f", static_cast<double>(snap.day_fraction),
            static_cast<double>(snap.lunar_phase)),
        fmt("stand %u  seed %llu", snap.stand,
            static_cast<unsigned long long>(snap.seed)),
        fmt("res %ux%u  fov %.1f  bob %.2f%s", snap.internal_w, snap.internal_h,
            static_cast<double>(snap.fov_y_rad * deg), static_cast<double>(snap.head_bob),
            snap.palette_post ? "  pal" : ""),
        fmt("wind %.2f  cloud %.2f  dark %.2f", static_cast<double>(snap.wind_strength),
            static_cast<double>(snap.cloud_cover),
            static_cast<double>(snap.ambient_darkness)),
        fmt("chunks %u  lod %u", snap.chunks_resident, snap.lod_nodes),
    };

    // The words go on their own line in the readout's own colour: they are the
    // fields most likely to be WRONG (gait selection is a live seam), so they
    // are the ones that must not be scanned past.
    const Line words =
        fmt("%.*s  %.*s  %.*s%s", static_cast<int>(compass.size()), compass.data(),
            static_cast<int>(gait.size()), gait.data(),
            static_cast<int>(loco.size()), loco.data(), snap.grounded ? "" : " (air)");
    const Line water = fmt("water %.2f m", static_cast<double>(snap.water_depth));

    // THE PLATE. Measured, not chosen: with the readout drawn straight onto the
    // world, 18.7 % of its glyph EDGES separated from what they sat on by less
    // than 2 * PALETTE_SHADE_STEP_REF (0.157 in the quantizer's metric), and
    // 55.4 % of its ink failed the same rule wherever the background was sky --
    // against 0.0 % over dark ground. The corner the readout lives in is the
    // corner the sky lives in, so the failure is the NORMAL case, not an edge
    // one. A conditional plate (only over bright backgrounds) was rejected
    // before it was written: it would flicker as clouds drift past, which is a
    // worse defect than the one being fixed.
    //
    // Opaque, not a dither veil like the pause page: half-covered white cloud
    // is still white on every other pixel, so the ruler fails on half the
    // edges, and half a rule is what this fix exists to end. The plate is sized
    // to the text and pinned to the corner, so it hides 12 % of the frame and
    // no more.
    // The row count is READ from the shared counter, not recounted here, so
    // whatever sits under this block cannot be told a different height than the
    // one actually drawn. The assert is what keeps that true when a row is
    // added: it fails at COMPILE time, in this file, next to the array.
    static_assert(static_cast<int>(std::size(left)) == READOUT_FIXED_LINES,
                  "readout_line_count() and the left[] array must agree -- "
                  "whatever is laid out below the readout is positioned from "
                  "the count, and a silent disagreement is an overlap");
    int widest = 0;
    for (const auto& l : left) {
        widest = std::max(widest, render::text_width_px(l.text));
    }
    widest = std::max(widest, render::text_width_px(words.text));
    if (snap.water_depth > 0.0f) {
        widest = std::max(widest, render::text_width_px(water.text));
    }
    draw_text_plate(canvas, read_x, read_y, widest,
                    readout_line_count(snap) * line_h - 1);

    int y = read_y;
    for (const auto& l : left) {
        render::draw_text(canvas, read_x, y, l.text, ink, /*shadow=*/true);
        y += line_h;
    }
    render::draw_text(canvas, read_x, y, words.text, snap.grounded ? ink : warn, true);
    y += line_h;
    if (snap.water_depth > 0.0f) {
        render::draw_text(canvas, read_x, y, water.text, warn, true);
        y += line_h;
    }

    // ПОДСКАЗКА «3 — снимок состояния, 5 — снимок экрана» СНЯТА (заказ владельца
    // 27.08: «весь вспомогательный текст подсказок ВООБЩЕ отовсюду удалить»).
    // Она стояла внизу справа и была ровно тем, что заказ называет: ИНСТРУКЦИЕЙ
    // ПО КЛАВИШАМ — о чём говорил и её собственный прежний комментарий («it is
    // an instruction, not a measurement»). Отладочный вывод остаётся полностью:
    // сняты не измерения, а подпись под ними. Клавиши 3 и 5 работают как
    // работали и названы там, где за ними идут, — на экране управления.
    //
    // debug_overlay_hint_top_y() ОСТАВЛЕН: он всё ещё отвечает на вопрос «где
    // кончается место, которое оверлей за собой держит», и на него смотрит
    // рукав app_debug_overlay. Полоса стала полем.
    (void)w;
    (void)dim;
}

int debug_overlay_bottom_y(const DebugSnapshot& snap, int origin_y) {
    const int line_h = render::FONT_CELL_H + 1;
    // The text block's height is `lines * pitch - 1` (the last row carries no
    // trailing gap), which is exactly what the plate above is sized to. Add the
    // plate's own margin, because what sits below has to clear the PLATE -- a
    // block that clears only the ink lands on the plate's lit edge and reads as
    // one panel cut in half.
    const int text_h = readout_line_count(snap) * line_h - 1;
    return READOUT_Y + origin_y + text_h + READOUT_PLATE_PAD;
}

int debug_overlay_hint_top_y(int canvas_height) {
    const int line_h = render::FONT_CELL_H + 1;
    return canvas_height - line_h - 2 - READOUT_PLATE_PAD;
}

std::string_view fits_width(int width_px, std::string_view full,
                            std::string_view brief) {
    return render::text_width_px(full) <= width_px - 2 * render::FONT_CELL_W ? full
                                                                            : brief;
}

// ---------------------------------------------------------------------------
// Sidecar
// ---------------------------------------------------------------------------

std::string format_snapshot(const DebugSnapshot& snap) {
    char buf[2048];
    // The ORDER is identity first, then clock, then pose. Someone reading this
    // in a chat window sees "which world" before "where in it", which is the
    // order the questions actually get asked in.
    const int n = std::snprintf(
        buf, sizeof(buf),
        "# Daggerfall N state capture\n"
        "# Restore with: DFN_RESTORE=<this file> DFN_MENU=0 ./dfn_app\n"
        "captured_at = %s\n"
        "build_commit = %s\n"
        "stand = %u\n"
        "seed = %llu\n"
        "game_seconds = %.6f\n"
        "day_fraction = %.6f\n"
        "lunar_phase = %.6f\n"
        "pos_x = %.6f\n"
        "pos_y = %.6f\n"
        "pos_z = %.6f\n"
        "yaw = %.6f\n"
        "pitch = %.6f\n"
        "look_x = %.6f\n"
        "look_y = %.6f\n"
        "look_z = %.6f\n"
        "speed_mps = %.6f\n"
        "vertical_velocity = %.6f\n"
        "stride_phase = %.6f\n"
        "gait = %u\n"
        "locomotion = %u\n"
        "grounded = %d\n"
        "crouched = %d\n"
        "water_depth = %.6f\n"
        "internal_w = %u\n"
        "internal_h = %u\n"
        "fov_y_rad = %.6f\n"
        "head_bob = %.6f\n"
        "palette_post = %d\n"
        "wind_strength = %.6f\n"
        "cloud_cover = %.6f\n"
        "ambient_darkness = %.6f\n"
        "fps = %.3f\n"
        "frame_ms = %.3f\n"
        "frame_ms_worst = %.3f\n"
        "chunks_resident = %u\n"
        "lod_nodes = %u\n",
        snap.captured_at.empty() ? "unknown" : snap.captured_at.c_str(),
        snap.build_commit.empty() ? "unknown" : snap.build_commit.c_str(), snap.stand,
        static_cast<unsigned long long>(snap.seed), snap.game_seconds,
        static_cast<double>(snap.day_fraction), static_cast<double>(snap.lunar_phase),
        static_cast<double>(snap.position.x), static_cast<double>(snap.position.y),
        static_cast<double>(snap.position.z), static_cast<double>(snap.yaw),
        static_cast<double>(snap.pitch), static_cast<double>(snap.look_dir.x),
        static_cast<double>(snap.look_dir.y), static_cast<double>(snap.look_dir.z),
        static_cast<double>(snap.speed_mps), static_cast<double>(snap.vertical_velocity),
        static_cast<double>(snap.stride_phase), static_cast<unsigned>(snap.gait),
        static_cast<unsigned>(snap.locomotion), snap.grounded ? 1 : 0,
        snap.crouched ? 1 : 0, static_cast<double>(snap.water_depth), snap.internal_w,
        snap.internal_h, static_cast<double>(snap.fov_y_rad),
        static_cast<double>(snap.head_bob), snap.palette_post ? 1 : 0,
        static_cast<double>(snap.wind_strength), static_cast<double>(snap.cloud_cover),
        static_cast<double>(snap.ambient_darkness), static_cast<double>(snap.fps),
        static_cast<double>(snap.frame_ms), static_cast<double>(snap.frame_ms_worst),
        snap.chunks_resident, snap.lod_nodes);
    return n > 0 ? std::string(buf, static_cast<size_t>(n)) : std::string{};
}

std::optional<DebugSnapshot> parse_snapshot(std::string_view text) {
    DebugSnapshot snap{};
    bool saw_stand = false;

    size_t pos = 0;
    while (pos <= text.size()) {
        const size_t nl = text.find('\n', pos);
        std::string_view line = text.substr(pos, nl == std::string_view::npos
                                                     ? std::string_view::npos
                                                     : nl - pos);
        pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;

        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        const std::string_view key = trim(line.substr(0, eq));
        const std::string value(trim(line.substr(eq + 1)));

        const auto as_f = [&value] { return std::strtof(value.c_str(), nullptr); };
        const auto as_d = [&value] { return std::strtod(value.c_str(), nullptr); };
        const auto as_u = [&value] {
            return static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        };
        const auto as_b = [&value] { return value == "1" || value == "true"; };

        if (key == "stand") {
            snap.stand = as_u();
            saw_stand = true;
        } else if (key == "seed") {
            snap.seed = std::strtoull(value.c_str(), nullptr, 10);
        } else if (key == "build_commit") {
            snap.build_commit = value;
        } else if (key == "captured_at") {
            snap.captured_at = value;
        } else if (key == "game_seconds") {
            snap.game_seconds = as_d();
        } else if (key == "day_fraction") {
            snap.day_fraction = as_f();
        } else if (key == "lunar_phase") {
            snap.lunar_phase = as_f();
        } else if (key == "pos_x") {
            snap.position.x = as_f();
        } else if (key == "pos_y") {
            snap.position.y = as_f();
        } else if (key == "pos_z") {
            snap.position.z = as_f();
        } else if (key == "yaw") {
            snap.yaw = as_f();
        } else if (key == "pitch") {
            snap.pitch = as_f();
        } else if (key == "look_x") {
            snap.look_dir.x = as_f();
        } else if (key == "look_y") {
            snap.look_dir.y = as_f();
        } else if (key == "look_z") {
            snap.look_dir.z = as_f();
        } else if (key == "speed_mps") {
            snap.speed_mps = as_f();
        } else if (key == "vertical_velocity") {
            snap.vertical_velocity = as_f();
        } else if (key == "stride_phase") {
            snap.stride_phase = as_f();
        } else if (key == "gait") {
            snap.gait = static_cast<uint8_t>(as_u());
        } else if (key == "locomotion") {
            snap.locomotion = static_cast<uint8_t>(as_u());
        } else if (key == "grounded") {
            snap.grounded = as_b();
        } else if (key == "crouched") {
            snap.crouched = as_b();
        } else if (key == "water_depth") {
            snap.water_depth = as_f();
        } else if (key == "internal_w") {
            snap.internal_w = as_u();
        } else if (key == "internal_h") {
            snap.internal_h = as_u();
        } else if (key == "fov_y_rad") {
            snap.fov_y_rad = as_f();
        } else if (key == "head_bob") {
            snap.head_bob = as_f();
        } else if (key == "palette_post") {
            snap.palette_post = as_b();
        } else if (key == "wind_strength") {
            snap.wind_strength = as_f();
        } else if (key == "cloud_cover") {
            snap.cloud_cover = as_f();
        } else if (key == "ambient_darkness") {
            snap.ambient_darkness = as_f();
        } else if (key == "fps") {
            snap.fps = as_f();
        } else if (key == "frame_ms") {
            snap.frame_ms = as_f();
        } else if (key == "frame_ms_worst") {
            snap.frame_ms_worst = as_f();
        } else if (key == "chunks_resident") {
            snap.chunks_resident = as_u();
        } else if (key == "lod_nodes") {
            snap.lod_nodes = as_u();
        }
        // Unknown keys fall through on purpose: see the header.
    }

    if (!saw_stand) {
        return std::nullopt;
    }
    return snap;
}

} // namespace dfn::app
