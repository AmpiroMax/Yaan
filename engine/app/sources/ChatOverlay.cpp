/*
Created: 14:08:2026 - 17:44:36
Last updated: 14:08:2026 - 17:44:36
Module: engine/app
File: engine/app/sources/ChatOverlay.cpp

Responsibility:
- The chat input overlay's text handling (UTF-8 encode/edit) and drawing
  (see ChatOverlay.h).

Dependencies:
- Uses: engine/render (PixelCanvas, BitmapFont), engine/app (draw_text_plate).
- Used by: App only.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned zone (Rule 25), app cut.
*/
/*
UPD:
- 14:08:2026 - 17:44:36: Created.
*/

#include "engine/app/sources/ChatOverlay.h"

#include "engine/app/sources/DebugOverlay.h" // draw_text_plate
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

#include <algorithm>

namespace dfn::app {

namespace {

// Pure LAYOUT of the overlay, in internal pixels -- the same class of number as
// the debug readout's own layout (local, cosmetic, single-zone), not a gameplay
// constant, so it lives here rather than in NUMBERS.md (Rule 14 is about
// gameplay/sim constants).
constexpr int MARGIN = 4;                 // gap from the canvas edge
constexpr int LINE_PITCH = render::FONT_INK_H + 2; // row-to-row step
constexpr size_t MAX_HISTORY_SHOWN = 6;   // recent lines drawn above the input
constexpr size_t MAX_HISTORY_KEPT = 64;   // ring bound on the stored log

// Appends one Unicode codepoint to `out` as UTF-8.
void encode_utf8(uint32_t cp, std::string& out) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// True for a UTF-8 continuation byte (0b10xxxxxx).
bool is_cont(unsigned char c) { return (c & 0xC0) == 0x80; }

// Number of UTF-8 characters (lead bytes) in the string.
size_t utf8_glyphs(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) {
        if (!is_cont(c)) {
            ++n;
        }
    }
    return n;
}

// The last `max_glyphs` UTF-8 characters of `s` (tail-scroll for the input line
// so the cursor stays visible on a long entry).
std::string utf8_tail(const std::string& s, size_t max_glyphs) {
    if (utf8_glyphs(s) <= max_glyphs) {
        return s;
    }
    // Walk back from the end counting lead bytes until we have max_glyphs.
    size_t glyphs = 0;
    size_t i = s.size();
    while (i > 0) {
        --i;
        if (!is_cont(static_cast<unsigned char>(s[i]))) {
            ++glyphs;
            if (glyphs == max_glyphs) {
                break;
            }
        }
    }
    return s.substr(i);
}

} // namespace

void ChatOverlay::feed_text(const std::vector<uint32_t>& codepoints) {
    if (!open_) {
        return;
    }
    for (uint32_t cp : codepoints) {
        if (cp >= 0x20) { // control codepoints are edit events, not text
            encode_utf8(cp, input_);
        }
    }
}

void ChatOverlay::backspace() {
    if (!open_ || input_.empty()) {
        return;
    }
    // Drop trailing continuation bytes, then the lead byte -- one whole UTF-8
    // character, never half of a Cyrillic letter.
    size_t i = input_.size();
    do {
        --i;
    } while (i > 0 && is_cont(static_cast<unsigned char>(input_[i])));
    input_.erase(i);
}

std::string ChatOverlay::take_input() {
    std::string out;
    out.swap(input_);
    return out;
}

void ChatOverlay::push_history(const std::string& who, const std::string& text) {
    history_.push_back(who + ": " + text);
    if (history_.size() > MAX_HISTORY_KEPT) {
        history_.erase(history_.begin(),
                       history_.begin()
                           + static_cast<std::ptrdiff_t>(history_.size() - MAX_HISTORY_KEPT));
    }
}

bool ChatOverlay::draw(render::PixelCanvas& canvas) const {
    if (!open_) {
        return false;
    }
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    const int block_x = MARGIN;
    const int block_w = w - 2 * MARGIN;
    if (block_w <= render::FONT_CELL_W) {
        return false; // canvas too narrow to say anything
    }
    const size_t shown = std::min(history_.size(), MAX_HISTORY_SHOWN);
    const int total_lines = static_cast<int>(shown) + 1; // + the input line

    // The input line sits just above the bottom margin; history stacks upward.
    const int input_y = h - MARGIN - render::FONT_INK_H;
    const int top_y = input_y - static_cast<int>(shown) * LINE_PITCH;

    // One plate under the whole block (the ground every UI string stands on).
    draw_text_plate(canvas, block_x, top_y, block_w,
                    total_lines * LINE_PITCH, /*pad=*/4);

    // History, oldest of the shown window first.
    const render::Color hist_ink{206, 200, 186};
    for (size_t i = 0; i < shown; ++i) {
        const std::string& line = history_[history_.size() - shown + i];
        const int y = top_y + static_cast<int>(i) * LINE_PITCH;
        (void)render::draw_text(canvas, block_x, y, line, hist_ink, /*shadow=*/true);
    }

    // The input line, with a prompt and a block cursor. Tail-scrolled so the
    // cursor is always visible on a long entry.
    const int max_glyphs = block_w / render::FONT_CELL_W;
    std::string shown_input = utf8_tail("> " + input_ + "_",
                                        static_cast<size_t>(std::max(1, max_glyphs)));
    const render::Color input_ink{244, 240, 226};
    (void)render::draw_text(canvas, block_x, input_y, shown_input, input_ink,
                            /*shadow=*/true);
    return true;
}

} // namespace dfn::app
