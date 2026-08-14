/*
Created: 14:08:2026 - 17:44:36
Last updated: 14:08:2026 - 17:44:36
Module: engine/app
File: engine/app/sources/ChatOverlay.h

Responsibility:
- The in-game CHAT overlay (В28): an input line the player TYPES into (live,
  UTF-8, Cyrillic) plus a short history of recent messages, drawn over the HUD.
  This is the piece that turns the chat from "Enter drops a snapshot" into "type
  a remark and send it" -- the user's literal ask.

Key items:
- ChatOverlay: open/close, feed_text()/backspace() driven by IInput::text_input()
  and the Backspace key, take_input() on send, push_history() for the log, and
  draw() into the HUD canvas.

Dependencies:
- Uses: engine/render (PixelCanvas, BitmapFont draw_text), engine/app
  (draw_text_plate for the ground under the text). No IInput/App knowledge: it
  is handed the frame's codepoints and edit events, so it is testable without a
  window (the same discipline as HudScreen).
- Used by: App (the HUD block) -- the wiring is a second commit on top of the
  debug-overlay zone's App.cpp work (lead's sequencing).

Notes:
- WHY IT OWNS NO INPUT DEVICE. The codepoints come from IInput::text_input()
  (layout/IME aware -- the physical Key enum cannot express Cyrillic), and
  Backspace/Enter are physical keys the caller reads. Passing them IN keeps this
  drawable-and-testable and keeps the one place that reads devices (App) the one
  place that reads devices.
- WHY UTF-8 INTERNALLY. render::draw_text takes UTF-8 and the font already
  carries the Cyrillic glyphs (proved by the HUD's Russian). Codepoints are
  encoded to UTF-8 on entry so the buffer is exactly what draw_text and the chat
  file both consume -- no second representation to keep in sync.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned zone (Rule 25), app cut.
*/
/*
UPD:
- 14:08:2026 - 17:44:36: Created -- the chat input overlay (live Russian typing).
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

// The typed-chat overlay. Draws an input line and a short message history; the
// caller feeds it the frame's codepoints and edit keys and reads take_input()
// when the player sends.
class ChatOverlay {
public:
    [[nodiscard]] bool is_open() const { return open_; }
    void open() { open_ = true; }
    void close() { open_ = false; }

    // Appends this frame's entered codepoints (IInput::text_input()) to the
    // input line, encoded as UTF-8. Control codepoints (< 0x20) are ignored --
    // Enter/Backspace are edit events, not text.
    void feed_text(const std::vector<uint32_t>& codepoints);
    // Removes the last UTF-8 character (not the last byte) from the input line.
    void backspace();

    [[nodiscard]] const std::string& input() const { return input_; }
    [[nodiscard]] bool input_empty() const { return input_.empty(); }
    void clear_input() { input_.clear(); }
    // Returns the current input and clears it -- what the caller writes to chat
    // on send.
    [[nodiscard]] std::string take_input();

    // Adds a line to the visible history (most recent shown at the bottom). The
    // history is bounded; the oldest lines fall off.
    void push_history(const std::string& who, const std::string& text);

    // Draws the overlay into `canvas` (the HUD layer the caller has cleared).
    // A no-op when closed. Returns whether anything was drawn, so the caller can
    // keep the HUD layer honest about being non-empty.
    bool draw(render::PixelCanvas& canvas) const;

private:
    bool open_ = false;
    std::string input_;
    std::vector<std::string> history_; // formatted "who: text", oldest first
};

} // namespace dfn::app
