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
- InputTick / capture_input / apply_input (§16.8, фаза 7): ВВОД ПО ТИКАМ
  (секция INPT) — оси, взгляд, передачи, защёлки, камера обвода; на прогоне
  ходок ведётся записанным вводом, и два прогона дают побитово ту же походку
  (прибор app_locomotion the_recorded_input_replays_bit_for_bit).

WHY POSE-PER-FRAME, NOT INPUT-REPLAY. The brief allows either ("поза по кадрам
минимум, вход если нужен"). Pose replay is STRONGER for bit-for-bit than
input+physics re-simulation: it bypasses the character controller and Jolt
entirely, so it cannot diverge on any nondeterminism there, and everything the
frame's image depends on -- sky, sun, cloud drift, the wind the foliage bends to
-- is already a pure function of game_seconds (the sky's clocks are pinned).
Drive the camera from the file and set game_seconds from the file, and two
replays are identical by construction. Body animation during
replay needs the walker to WALK, so the INPUT section is here too (11.09): one
InputTick per sim tick, applied to PlayerState before player_pre_step. In a
counted run one frame is one tick, so the eye (FRMS) and the walker (INPT)
stay in step by construction.

WHY BINARY, SECTION-BASED (Rule 7), NOT the chat's plain text. A trajectory is
per-frame BULK data (thousands of frames), and the property it exists to serve
is EXACT reproduction -- float values must round-trip bit-identically, which a
decimal text form does not guarantee. So it uses core's BinaryWriter/Reader
container (magic + version, tagged length-prefixed little-endian sections),
reused rather than re-implemented (Rule 35). The path to a trajectory is what a
chat line's `trajectory` field points at.

Dependencies:
- Uses: engine/core/serialization (BinaryWriter/Reader), gameplay/PlayerMovement.h
  (PlayerState — что ходок читает за тик), glm.
- Used by: App only.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned zone (Rule 25), app cut.
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "engine/gameplay/sources/PlayerMovement.h"

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
// ВВОД ОДНОГО ТИКА СИМА — ровно то, что player_pre_step прочтёт из PlayerState
// (оси, накопленный взгляд, передачи, защёлки, присед, оружие) и рыск/тангаж
// камеры обвода третьего лица. Записывается после grab_input/park_posture,
// применяется в той же точке на прогоне.
struct InputTick {
    glm::vec2 move_axes{0.0f};
    glm::vec2 look{0.0f};   ///< pending_look, пиксели за тик
    /// ПРИЦЕЛ ДО ШАГА СИМА: его ставит не только мышь — очередь стенда пишет
    /// face_yaw прямо, и без него прогон стендовой записи расходился с глазом
    /// (фигура уходила из кадра ленты, 11.09). Взгляд тика прибавляется в
    /// pre_step поверх, как и при записи.
    float yaw = 0.0f;
    float cam_yaw = 0.0f;
    float cam_pitch = 0.0f;
    bool jog = false;
    bool run = false;
    bool debug_sprint = false;
    bool jump = false;
    bool crouch = false;
    bool interact = false;
    bool weapon = false;
};

struct Trajectory {
    uint32_t stand = 0;
    uint64_t seed = 0;
    std::vector<TrajectoryFrame> frames;
    std::vector<InputTick> inputs; ///< секция INPT; пусто у записей до 11.09
};

/// Снимок ввода тика с ходока (и камеры обвода) — для записи.
[[nodiscard]] InputTick capture_input(const gameplay::PlayerState& ps, float cam_yaw,
                                      float cam_pitch);
/// Записанный ввод — в ходока (защёлки складываются ИЛИ, как у клавиатуры).
void apply_input(gameplay::PlayerState& ps, const InputTick& in);

// Accumulates frames while active; writes them with core's section container.
class TrajectoryRecorder {
public:
    void begin(uint32_t stand, uint64_t seed);
    void push(const TrajectoryFrame& f); // no-op when not active
    void push_input(const InputTick& in); // no-op when not active
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
    /// Ввод следующего тика; nullptr, когда записанный ввод кончился (или его
    /// в файле нет — запись до 11.09: глаз из файла, ходок стоит).
    [[nodiscard]] const InputTick* next_input();
    [[nodiscard]] bool has_inputs() const { return traj_ && !traj_->inputs.empty(); }
    [[nodiscard]] size_t input_index() const { return input_index_; }
    [[nodiscard]] const Trajectory* trajectory() const { return traj_ ? &*traj_ : nullptr; }
private:
    std::optional<Trajectory> traj_;
    size_t index_ = 0;
    size_t input_index_ = 0;
};

/// Запись траектории в файл (тот же контейнер, что пишет TrajectoryRecorder) —
/// для приборов, которые собирают траекторию вне приложения.
[[nodiscard]] bool write_trajectory(const Trajectory& t, const std::string& path);

} // namespace dfn::app
