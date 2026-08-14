/*
Created: 14:08:2026 - 16:43:03
Last updated: 14:08:2026 - 16:48:45
Module: engine/app
File: engine/app/sources/ChatLog.h

Responsibility:
- The two-way chat that lives BESIDE a map (contract: docs/MAP_LAYOUT.md). It is
  an append-log `<map>.chat.jsonl`, one JSON object per line, so a remark or a
  demo note "knows its own map" and therefore the zone that owns it. A HUMAN
  drops a remark with the frame's capture (and optionally a trajectory)
  attached; an AGENT drops a demo's self-documentation (why the demo exists,
  what it fixes, what to look at, O1). Both roles land in the same file.
- The telemetry RING: the world's pose/look/load sampled a few times a second
  into a fixed buffer (config::TELEMETRY_LOG_HZ / _RING_SAMPLES), flushed to a
  log beside the map on stop (item 3). Render-side counts (triangles, what is
  under the crosshair) are read IF present and written 0 / empty otherwise --
  the hook a peer fills later, never a block here.

Key items:
- ChatEntry: one JSONL record (the MAP_LAYOUT fields t/who/text/capture/trajectory).
- append_chat_entry(): appends one line to <map>.chat.jsonl.
- chat_path_for_map() / map_file_for_stand(): where that file lives.
- TelemetrySample / TelemetryRing: the ring and its columnar flush.

Dependencies:
- Uses: std, glm. NOT DebugSnapshot -- a chat line references a capture by PATH
  (the capture is the existing DFN_CAPTURE artifact, reused not duplicated).
- Used by: App only.

Notes:
- WHY JSONL, NOT the F2 sidecar's key=value. The contract in MAP_LAYOUT.md fixes
  it: one JSON object per line, appended, two roles in one file. Kept minimal
  and hand-written (no JSON lib in this layer): only the five contract fields,
  strings escaped for JSON, UTF-8 bytes passed through so Cyrillic survives.
- WHY `t` IS game_seconds. Deterministic code has no wall clock (Rule 12); the
  counted clock is the only monotone quantity every run shares, so it orders the
  log. The real date is a best-effort EXTRA (`date`) the tool adds when it can
  reach a wall clock outside the deterministic path -- never the sort key.
- WHY THE STAND->MAP BRIDGE EXISTS HERE. The editor's map browser (editor zone)
  will hand the app the open `.map` path; until it does, the app only knows its
  stand id, so map_file_for_stand() scans assets/maps for the manifest whose
  `source = stand:<name>` matches. This is the transitional bridge MAP_LAYOUT.md
  documents, and the seam to hand over to the editor zone.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned zone (Rule 25), app cut.
*/
/*
UPD:
- 14:08:2026 - 16:43:03: Created -- the chat box, its snapshot attachment, the
  agent-self-doc format (O1) and the telemetry ring (item 3).
- 14:08:2026 - 16:48:45: Realigned to docs/MAP_LAYOUT.md (lead's correction): the
  chat is `<map>.chat.jsonl` beside the map, JSONL with the fixed fields
  t/who/text/capture/trajectory -- NOT an invented `chat/` folder or a key=value
  block. Added the stand->map bridge and JSON string escaping.
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace dfn::app {

// One line of `<map>.chat.jsonl`. The fields are exactly MAP_LAYOUT.md's:
//   {"t":<num>, "who":"human"|"<zone>", "text":"...", "capture":"...", "trajectory":"..."}
// `who` is "human" for a player's remark, or a zone name for a demo self-doc
// (O1). `capture` / `trajectory` are paths, omitted from the line when empty.
struct ChatEntry {
    std::string who;         // "human", or an agent zone name (e.g. "flora")
    std::string text;        // the message, UTF-8 (empty allowed)
    std::string capture;     // path to the attached DFN_CAPTURE png/state, or ""
    std::string trajectory;  // path to an attached trajectory/telemetry log, or ""
};

// Appends one JSONL line to `chat_path` (created if absent; parent dirs must
// already exist, like the map beside it). `t` is the counted-clock order key;
// `date` is the best-effort wall date, added as an extra field when present.
// Returns true on success (failure is reported by the caller).
bool append_chat_entry(const std::string& chat_path, double t,
                       const ChatEntry& entry,
                       const std::optional<std::string>& date);

// `<...>.map` -> `<...>.chat.jsonl`. Any other suffix simply gets `.chat.jsonl`
// appended, so a caller that already holds the chat path passes it through.
std::string chat_path_for_map(const std::string& map_file);

// Finds the `.map` manifest under `maps_root` whose `source = stand:<name>`
// names the given stand id (Testbed=0, Forest=1 -- the transitional bridge, see
// the header note). Returns "" when none matches, which the caller reports
// rather than guessing a path.
std::string map_file_for_stand(const std::string& maps_root, uint32_t stand);

// ---------------------------------------------------------------------------
// THE TELEMETRY RING (item 3). While the player walks/looks in the EDITOR, the
// app pushes a sample a few times a second (config::TELEMETRY_LOG_HZ). On stop
// the ring is flushed to a log beside the map. It is a RING so a long session
// costs a bounded amount of memory and the flush always carries the most recent
// window -- the part a "it just did something weird" report is about. In-game
// (Playing) there is NO continuous log (В39: keep play light).
//
// The render-side columns (tris, aim_*) are read from whatever the app already
// holds and default to 0 / "" -- the seam a render hook fills later. Writing the
// column now, empty, means the log's shape does not change the day it arrives.
// ---------------------------------------------------------------------------
struct TelemetrySample {
    double game_seconds = 0.0; // counted clock: the reproducible order key
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fps = 0.0f;
    float frame_ms = 0.0f;
    uint32_t chunks_resident = 0;
    uint32_t lod_nodes = 0;
    // Render seam, 0 / empty until a peer fills it (read-if-present, never a
    // block): triangles submitted this frame, and what the crosshair ray hit.
    uint32_t triangles = 0;
    std::string aim_target; // "" when render offers no pick
};

class TelemetryRing {
public:
    explicit TelemetryRing(size_t capacity);

    void push(const TelemetrySample& s);
    [[nodiscard]] bool empty() const { return count_ == 0; }
    [[nodiscard]] size_t size() const { return count_; }

    // Writes the ring oldest-first as a columnar log with a header, the same
    // shape as DFN_FRAME_LOG so one reader serves both. Returns true on success.
    bool flush(const std::string& path) const;

private:
    std::vector<TelemetrySample> buf_;
    size_t capacity_ = 0;
    size_t next_ = 0;   // write cursor
    size_t count_ = 0;  // valid samples (<= capacity_)
};

} // namespace dfn::app
