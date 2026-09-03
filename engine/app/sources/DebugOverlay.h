/*
Module: engine/app
File: engine/app/sources/DebugOverlay.h

Responsibility:
- The player-facing debug readout (F3) and the state CAPTURE/RESTORE pair (F2)
  that turns "I saw a visual bug" into a world someone else can stand in.

Key items:
- DebugSnapshot: everything needed to READ the moment and everything needed to
  REBUILD it. Deliberately one struct, not two -- see the note below.
- FrameClock: rolling frame time; fps is a distribution, not an instant.
- draw_debug_overlay(): the F3 readout.
- format_snapshot() / parse_snapshot(): the sidecar written next to the .png.

Dependencies:
- Uses: engine/render (PixelCanvas, BitmapFont), glm.
- Used by: App only.

Notes:
- WHY ONE STRUCT AND NOT TWO. The readout and the save file carry the same
  fields on purpose. If they were separate, the number the user is LOOKING AT
  when he decides "this is wrong" could differ from the number recorded in the
  file he sends -- and the whole point of the pair is that the report and the
  reproduction agree. Anything worth showing is worth saving.
- WHY THE FILE IS PLAIN `key = value` TEXT. Its first consumer is a human
  pasting it into a chat, and its second is an agent reading it without a
  parser. A binary or nested format would be smaller and worse at both. This is
  a DEBUG ARTIFACT, not game content, so Rule 5 (content lives in data files)
  does not make it a localization matter; nothing here is ever shown as prose.
- WHY RESTORE CAN REFUSE. A snapshot restored into a different world is not a
  reproduction, it is a coincidence -- so seed and stand are recorded and
  checked, and a mismatch is reported rather than silently walked into
  (Rule 27: a vantage that cannot fail is not evidence).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <string_view>

#include <glm/vec3.hpp>

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

// A moment in the world, complete enough to be rebuilt from.
//
// The grouping is by WHO WRITES IT, not by what it looks like on screen,
// because that is the axis along which a field goes stale: when a subsystem
// stops populating one, the whole group goes quiet together and the gap is
// visible instead of being one wrong number among thirty right ones.
struct DebugSnapshot {
    // -- Identity. Without these the rest is a coincidence, not a repro. ------
    uint32_t stand = 0;        // which demo map
    uint64_t seed = 0;         // worldgen seed
    std::string build_commit;  // short hash, "unknown" when not a git tree
    std::string captured_at;   // wall clock, for ordering a folder of captures

    // -- Clock. The single biggest cause of "it looks different for me". -----
    double game_seconds = 0.0; // absolute in-game time; the restorable one
    float day_fraction = 0.0f; // derived, shown so the hour is readable
    float lunar_phase = 0.0f;  // derived

    // -- Player. The user's words: "углы мои наклонов, позиций". -------------
    glm::vec3 position{0.0f};
    float yaw = 0.0f;   // radians, sim's convention: 0 = -Z, + = clockwise
    float pitch = 0.0f; // radians, + = up
    glm::vec3 look_dir{0.0f, 0.0f, -1.0f}; // derived from yaw/pitch, shown raw
    float speed_mps = 0.0f;
    float vertical_velocity = 0.0f;
    float stride_phase = 0.0f;
    uint8_t gait = 0;       // gameplay::Gait ordinal
    uint8_t locomotion = 0; // gameplay::Locomotion ordinal
    bool grounded = true;
    bool crouched = false;
    float water_depth = 0.0f;

    // -- Presentation. A bug report against 640x360 is a different bug -------
    // than the same report against 1280x720, and the palette post changes
    // which colours can exist at all.
    uint32_t internal_w = 0;
    uint32_t internal_h = 0;
    float fov_y_rad = 0.0f;
    float head_bob = 1.0f;
    bool palette_post = false;

    // -- Environment. What the sky was doing when the frame was taken. -------
    float wind_strength = 0.0f;
    float cloud_cover = 0.0f;
    float ambient_darkness = 0.0f;

    // -- Load. Present so a stutter report carries its own cause. ------------
    float fps = 0.0f;
    float frame_ms = 0.0f;
    float frame_ms_worst = 0.0f; // worst frame in the window, see FrameClock
    uint32_t chunks_resident = 0;
    uint32_t lod_nodes = 0;
    /// Строки приборов локомоции (DFN_LOCO_HUD); пусто — прибор выключен.
    std::vector<std::string> loco_lines;
};

// Rolling frame time over a fixed window.
//
// It reports the AVERAGE and the WORST of the window rather than an
// instantaneous rate, because the complaint this exists to serve -- "всё
// дергает" -- is a claim about the worst frames, and an average hides exactly
// those. A readout that could not show a hitch would be decoration (Rule 27).
class FrameClock {
public:
    static constexpr int WINDOW = 60;

    void push(float dt_seconds);
    [[nodiscard]] float fps() const;
    [[nodiscard]] float mean_ms() const;
    [[nodiscard]] float worst_ms() const;

private:
    float samples_[WINDOW]{};
    int count_ = 0;
    int next_ = 0;
};

// Cardinal name for a yaw, in sim's convention (0 = -Z = north). Returns a
// localization KEY, never a literal (Rule 5) -- the caller resolves it.
[[nodiscard]] uint64_t compass_key_for_yaw(float yaw_radians);

// Draws the readout into the HUD canvas. Does not clear it: the caller owns
// the layer and may have drawn a prompt already.
//
// `origin_x` / `origin_y` MOVE THE WHOLE BLOCK OFF AN EDGE SOMEBODY ELSE TOOK,
// and they are parameters rather than a constant because who took what changes
// while the program runs. The editor's toolbar is docked along the top and its
// panels along the sides; EditorUi counts the strips and publishes them
// (insets() / world_rect_norm()), and the readout starts inside what is LEFT.
// Zero on both is the game, where nothing is docked and the corner is free.
//
// PICKING A NUMBER BY EYE IS THE DEFECT THIS ARGUMENT EXISTS TO END (user,
// 17.08.2026: «кнопки сверху пересекаются с дебаг текстом»). A hand-tuned 44
// is right for one font at one scale on one day, and silently wrong for the
// next -- which is the same lesson the editor's own toolbar wrote into
// EditorUi.h after being nudged down by hand once.
void draw_debug_overlay(render::PixelCanvas& canvas, const DebugSnapshot& snap,
                        int origin_x = 0, int origin_y = 0);

// THE FIRST FREE ROW UNDER THE READOUT, plate included. Published because the
// readout is not the only thing that wants the top-left corner, and until this
// existed the second tenant guessed: the editor banner was pinned at (4, 4)
// while the readout starts at (3, 3), so with both on -- the user's normal
// working state -- they printed through each other and neither could be read.
//
// A LITERAL OFFSET WOULD HAVE BEEN THE SAME BUG WITH A DELAY (Rule 39): the
// readout's height is not a constant, it grows a row whenever the player is in
// water, so anything that hardcodes "the readout is N pixels tall" is correct
// on dry land and wrong the moment he wades in. This is computed from the same
// line count the draw uses, and a row added to the readout moves both.
//
// `origin_y` is the one handed to draw_debug_overlay; pass the same number or
// the block below stacks against a readout that is not where it was drawn.
[[nodiscard]] int debug_overlay_bottom_y(const DebugSnapshot& snap, int origin_y = 0);

// The top of the capture hint's plate, which the readout pins to the BOTTOM of
// the frame. Published for the same reason as the function above: it is the
// other thing already occupying the frame's edges, so anything that grows
// downward has to be able to ask where it stops.
[[nodiscard]] int debug_overlay_hint_top_y(int canvas_height);

// THE LINE THAT FITS, CHOSEN BY MEASURING RATHER THAN BY BRANCHING ON A
// RESOLUTION. `full` when it fits inside `width_px` with a cell of air on each
// side, `brief` otherwise -- a line that ENDS on the last pixel column reads as
// clipped even when it is whole.
//
// SHARED BECAUSE THE DECISION IS SHARED, not because two callers happened to
// want the same three lines. The settings page learned this the expensive way:
// 320x180 is a rung the page itself OFFERS, and at 320 px its own instruction
// lines ran off both edges -- the screen that exists to be read stopped being
// readable one keypress away. A branch on the number 320 would have fixed that
// day's Russian and broken on the first translation wider than it. Every
// overlay in this zone now narrows through this one function, so a new panel
// cannot reintroduce the defect by not knowing about it.
[[nodiscard]] std::string_view fits_width(int width_px, std::string_view full,
                                          std::string_view brief);

// ---------------------------------------------------------------------------
// THE INTERFACE'S GROUND. Every string the app draws over the world stands on
// one of these, and they are one function rather than four copies because they
// are one decision: text on our palette does not separate from a bright sky.
// Measured on the readout, which is the worst case only because it is the
// biggest: without a plate, 18.1 % of its glyph edges sat closer than
// 2 * PALETTE_SHADE_STEP_REF to what they abutted, 56.1 % of its ink failed the
// same rule wherever the background was sky, and 0.0 % failed over dark ground.
// With it, inside the readout's own rectangle, the count of lost edges is ZERO
// at BOTH palette settings.
//
// `text_plate` takes the TEXT box (the same x, y and width you pass to
// draw_text) and grows it by the margin the readout uses, so no caller has to
// re-derive the padding and no two plates end up different sizes.
// ---------------------------------------------------------------------------

// The dose door, Rule 47's one-binary clause: DFN_UI_PLATE=0 draws every
// interface string with no ground under it -- the state that shipped before
// 22a603b -- so a before/after pair comes out of ONE binary instead of two
// builds an hour apart in a tree six zones are compiling. Read once per
// process: an instrument that can change mid-run is not an instrument.
[[nodiscard]] bool ui_plates_enabled();

// Fills the plate for one line of text, plus a lit edge on the sides that face
// the world. No-op when the door is closed. `line_h` is the row pitch, not the
// glyph height, so a multi-line block passes lines * pitch.
// `pad` is the margin around the text: the default suits a single line, and a
// block of lines wants more air (the pause page passes 6). Sides that fall
// outside the canvas simply clip, so a plate pinned to a corner loses the two
// edges facing the frame border -- which is what makes it read as pinned.
void draw_text_plate(render::PixelCanvas& canvas, int text_x, int text_y, int text_w,
                     int text_h, int pad = 3);

// The sidecar text written next to the screenshot.
[[nodiscard]] std::string format_snapshot(const DebugSnapshot& snap);

// Reads a sidecar back. Returns nullopt only when the text is not a snapshot
// at all (no `stand` key); unknown keys are IGNORED rather than rejected, so a
// capture from a newer build still restores what it can, and missing keys keep
// their defaults. A capture is evidence, and evidence that a version bump can
// invalidate stops being collected.
[[nodiscard]] std::optional<DebugSnapshot> parse_snapshot(std::string_view text);

} // namespace dfn::app
