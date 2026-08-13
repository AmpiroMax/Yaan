/*
Created: 10:08:2026 - 19:11:04
Last updated: 13:08:2026 - 16:48:00
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
*/

#include "engine/app/sources/DebugOverlay.h"

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

// THE DOSE DOOR (Rule 47, the one-binary clause). `DFN_UI_PLATE=0` draws the
// interface text with NO plate under it -- the state this project shipped
// before 22a603b. It exists so the before arm and the after arm come out of the
// SAME binary: in a shared tree with six zones building all day, a before/after
// across two builds measures the week rather than the change (render lost a
// whole reading to exactly that, an hour before this was written). Read once,
// not per frame: an instrument that can change mid-run is not an instrument.
// The same name is read by Menu.cpp -- one door, one meaning: "text without its
// ground", wherever the interface draws text.
[[nodiscard]] bool plates_enabled() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_UI_PLATE");
        return !(e != nullptr && e[0] == '0');
    }();
    return on;
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
// Readout
// ---------------------------------------------------------------------------

void draw_debug_overlay(render::PixelCanvas& canvas, const DebugSnapshot& snap) {
    const int w = static_cast<int>(canvas.width());
    const int line_h = render::FONT_CELL_H + 1;
    const render::Color ink{232, 228, 214};
    const render::Color dim{168, 164, 152};
    const render::Color warn{232, 168, 96};
    // THE PLATE'S TWO COLOURS ARE THE MENU'S TWO COLOURS on purpose: one
    // interface, one ground. See the plate note below the line table.
    const render::Color plate{18, 20, 26};
    const render::Color plate_edge{54, 56, 64};

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
    const auto plate_for = [&](int x0, int y0, int text_w, int lines) {
        if (!plates_enabled()) {
            return;
        }
        const int pw = text_w + 4;
        const int ph = lines * line_h + 3;
        canvas.fill_rect(x0, y0, pw, ph, plate);
        // One lit edge on the two sides that face the world, so the plate reads
        // as a panel rather than as a hole punched in the frame.
        canvas.hline(x0, y0 + ph, pw + 1, plate_edge);
        canvas.vline(x0 + pw, y0, ph + 1, plate_edge);
    };

    int widest = 0;
    int line_count = 0;
    for (const auto& l : left) {
        widest = std::max(widest, render::text_width_px(l.text));
        ++line_count;
    }
    widest = std::max(widest, render::text_width_px(words.text));
    ++line_count;
    if (snap.water_depth > 0.0f) {
        widest = std::max(widest, render::text_width_px(water.text));
        ++line_count;
    }
    plate_for(0, 0, widest + 3, line_count);

    int y = 3;
    for (const auto& l : left) {
        render::draw_text(canvas, 3, y, l.text, ink, /*shadow=*/true);
        y += line_h;
    }
    render::draw_text(canvas, 3, y, words.text, snap.grounded ? ink : warn, true);
    y += line_h;
    if (snap.water_depth > 0.0f) {
        render::draw_text(canvas, 3, y, water.text, warn, true);
        y += line_h;
    }

    // The capture hint sits bottom-right so it never overlaps the readout,
    // and it is drawn dim: it is an instruction, not a measurement. It gets the
    // same plate for the same reason -- the bottom of the frame is ground
    // today, but ground is snow, sand and water elsewhere.
    const std::string_view hint = localized(serialization::fnv1a64("debug.hint.capture"));
    const int hint_w = render::text_width_px(hint);
    const int hint_y = static_cast<int>(canvas.height()) - line_h - 2;
    if (plates_enabled()) {
        canvas.fill_rect(w - hint_w - 5, hint_y - 2, hint_w + 5, line_h + 3, plate);
        canvas.hline(w - hint_w - 6, hint_y - 2, 1, plate_edge);
        canvas.vline(w - hint_w - 6, hint_y - 2, line_h + 3, plate_edge);
    }
    render::draw_text(canvas, w - hint_w - 3, hint_y, hint, dim, true);
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
