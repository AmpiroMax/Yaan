/*
Module: engine/app
File: engine/app/sources/TrajectoryRecord.h

Responsibility:
- Record a walk/look as a per-frame trajectory and play it back
  DETERMINISTICALLY so two playbacks render bit-for-bit the same frame (O3, the
  key item of В28). The user watches with his own eyes; an agent sees LITERALLY
  the same, which is what makes a between-frames bug (ripple, shimmer, judder)
  catchable at all -- the still-frame tour freezes the tick and cannot show one
  (Rule 27).

Key items:
- TrajectoryFrame: one presented frame's EYE pose + counted clock + fov.
- Trajectory: the frames plus the identity (stand/seed) a replay is checked
  against, so a replay into the wrong world is refused, not silently walked into.
- TrajectoryRecorder: accumulates frames while recording; writes on stop.
- TrajectoryPlayer: steps one recorded frame per presented frame.

WHY POSE-PER-FRAME, NOT INPUT-REPLAY. The brief allows either ("поза по кадрам
минимум, вход если нужен"). Pose replay is STRONGER for bit-for-bit than
input+physics re-simulation: it bypasses the character controller and Jolt
entirely, so it cannot diverge on any nondeterminism there, and everything the
frame's image depends on -- sky, sun, cloud drift, the wind the foliage bends to
-- is already a pure function of game_seconds (the sky's clocks are pinned).
Drive the camera from the file and set game_seconds from the file, and two
replays are identical by construction. Input is therefore NOT recorded in this
first cut; the seam is here (a future INPUT section) if body animation during
replay ever needs it.

WHY BINARY, SECTION-BASED (Rule 7), NOT the chat's plain text. A trajectory is
per-frame BULK data (thousands of frames), and the property it exists to serve
is EXACT reproduction -- float values must round-trip bit-identically, which a
decimal text form does not guarantee. So it uses core's BinaryWriter/Reader
container (magic + version, tagged length-prefixed little-endian sections),
reused rather than re-implemented (Rule 35). The path to a trajectory is what a
chat line's `trajectory` field points at.

Dependencies:
- Uses: engine/core/serialization (BinaryWriter/Reader), glm.
- Used by: App only.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned zone (Rule 25), app cut.
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace dfn::app {

// One presented frame. The EYE pose (what the frame was rendered from), the
// counted clock (which fixes every time-driven visual), and the fov (the
// running fov-kick changes the projection and therefore the image, so it is
// part of the reproduced state, not derived).
struct TrajectoryFrame {
    double game_seconds = 0.0;
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fov_y = 0.0f;
};

// A recording plus the identity a replay is checked against. Without stand/seed
// a replay is a coincidence in whatever world happens to be loaded, not a
// reproduction (the DebugSnapshot rule, applied here).
struct Trajectory {
    uint32_t stand = 0;
    uint64_t seed = 0;
    std::vector<TrajectoryFrame> frames;
};

// Accumulates frames while active; writes them with core's section container.
class TrajectoryRecorder {
public:
    void begin(uint32_t stand, uint64_t seed);
    void push(const TrajectoryFrame& f); // no-op when not active
    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] size_t size() const { return traj_.frames.size(); }

    // Writes what has been recorded and stops. Returns the path on success, ""
    // on failure (reported). A recording of zero frames is refused rather than
    // written -- an empty trajectory is the silent-zero this project distrusts.
    std::string stop_and_write(const std::string& path);

private:
    Trajectory traj_;
    bool active_ = false;
};

// Reads a trajectory file. nullopt on bad magic / truncation (reported by the
// reader's ok() latch). Unknown sections are skipped, so a file from a newer
// build still loads the frames it can.
[[nodiscard]] std::optional<Trajectory> read_trajectory(const std::string& path);

// Steps through a loaded trajectory one frame per call.
class TrajectoryPlayer {
public:
    [[nodiscard]] bool load(const std::string& path); // false if unreadable/empty
    [[nodiscard]] bool active() const { return traj_.has_value() && index_ < count(); }
    [[nodiscard]] size_t index() const { return index_; }
    [[nodiscard]] size_t count() const {
        return traj_ ? traj_->frames.size() : 0;
    }
    [[nodiscard]] uint32_t stand() const { return traj_ ? traj_->stand : 0; }
    [[nodiscard]] uint64_t seed() const { return traj_ ? traj_->seed : 0; }

    // Returns the current frame and advances. nullptr when the trajectory is
    // spent (active() is then false).
    [[nodiscard]] const TrajectoryFrame* advance();

private:
    std::optional<Trajectory> traj_;
    size_t index_ = 0;
};

} // namespace dfn::app
