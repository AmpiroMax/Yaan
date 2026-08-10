/*
Created: 10:08:2026 - 19:11:04
Last updated: 10:08:2026 - 19:11:04
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
/*
UPD:
- 10:08:2026 - 19:11:04: Created -- user request: a debug key showing where I look, fps,
                          speed and coordinates, plus a screenshot that carries the state
                          so the world can be rebuilt from it.
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
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
void draw_debug_overlay(render::PixelCanvas& canvas, const DebugSnapshot& snap);

// The sidecar text written next to the screenshot.
[[nodiscard]] std::string format_snapshot(const DebugSnapshot& snap);

// Reads a sidecar back. Returns nullopt only when the text is not a snapshot
// at all (no `stand` key); unknown keys are IGNORED rather than rejected, so a
// capture from a newer build still restores what it can, and missing keys keep
// their defaults. A capture is evidence, and evidence that a version bump can
// invalidate stops being collected.
[[nodiscard]] std::optional<DebugSnapshot> parse_snapshot(std::string_view text);

} // namespace dfn::app
