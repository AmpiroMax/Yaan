/*
Module: engine/app
File: engine/app/sources/TrajectoryRecord.cpp

Responsibility:
- Serialisation of a trajectory via core's section container, and the recorder /
  player state machines (see TrajectoryRecord.h).

Dependencies:
- Uses: engine/core/serialization (BinaryWriter/Reader).
- Used by: App only.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned zone (Rule 25), app cut.
*/

#include "engine/app/sources/TrajectoryRecord.h"

#include "engine/core/serialization/sources/BinaryReader.h"
#include "engine/core/serialization/sources/BinaryWriter.h"

#include <cstdio>

namespace dfn::app {

namespace {

// Container magic and section tags (Rule 7 format constants, never derived).
constexpr uint32_t TRAJ_MAGIC = serialization::make_tag('D', 'F', 'T', 'R');
constexpr uint32_t TRAJ_CONTAINER_VERSION = 1;
constexpr serialization::SectionTag TAG_INFO = serialization::make_tag('I', 'N', 'F', 'O');
constexpr serialization::SectionTag TAG_FRMS = serialization::make_tag('F', 'R', 'M', 'S');
constexpr serialization::SectionTag TAG_INPT = serialization::make_tag('I', 'N', 'P', 'T');
constexpr uint64_t INPT_RECORD_BYTES = 7 * 4 + 1; // 7 f32 + flags u8

void write_input(serialization::BinaryWriter& w, const InputTick& in) {
    w.write_f32(in.move_axes.x);
    w.write_f32(in.move_axes.y);
    w.write_f32(in.look.x);
    w.write_f32(in.look.y);
    w.write_f32(in.yaw);
    w.write_f32(in.cam_yaw);
    w.write_f32(in.cam_pitch);
    const uint8_t flags = static_cast<uint8_t>((in.jog ? 1u : 0u) | (in.run ? 2u : 0u)
                                               | (in.debug_sprint ? 4u : 0u) | (in.jump ? 8u : 0u)
                                               | (in.crouch ? 16u : 0u) | (in.interact ? 32u : 0u)
                                               | (in.weapon ? 64u : 0u));
    w.write_u8(flags);
}

InputTick read_input(serialization::BinaryReader& r) {
    InputTick in;
    in.move_axes.x = r.read_f32();
    in.move_axes.y = r.read_f32();
    in.look.x = r.read_f32();
    in.look.y = r.read_f32();
    in.yaw = r.read_f32();
    in.cam_yaw = r.read_f32();
    in.cam_pitch = r.read_f32();
    const uint8_t flags = r.read_u8();
    in.jog = (flags & 1u) != 0;
    in.run = (flags & 2u) != 0;
    in.debug_sprint = (flags & 4u) != 0;
    in.jump = (flags & 8u) != 0;
    in.crouch = (flags & 16u) != 0;
    in.interact = (flags & 32u) != 0;
    in.weapon = (flags & 64u) != 0;
    return in;
}

void write_frame(serialization::BinaryWriter& w, const TrajectoryFrame& f) {
    w.write_f64(f.game_seconds);
    w.write_f32(f.position.x);
    w.write_f32(f.position.y);
    w.write_f32(f.position.z);
    w.write_f32(f.yaw);
    w.write_f32(f.pitch);
    w.write_f32(f.fov_y);
}

TrajectoryFrame read_frame(serialization::BinaryReader& r) {
    TrajectoryFrame f;
    f.game_seconds = r.read_f64();
    f.position.x = r.read_f32();
    f.position.y = r.read_f32();
    f.position.z = r.read_f32();
    f.yaw = r.read_f32();
    f.pitch = r.read_f32();
    f.fov_y = r.read_f32();
    return f;
}

} // namespace

void TrajectoryRecorder::begin(uint32_t stand, uint64_t seed) {
    traj_ = Trajectory{};
    traj_.stand = stand;
    traj_.seed = seed;
    active_ = true;
}

void TrajectoryRecorder::push(const TrajectoryFrame& f) {
    if (active_) {
        traj_.frames.push_back(f);
    }
}

void TrajectoryRecorder::push_input(const InputTick& in) {
    if (active_) {
        traj_.inputs.push_back(in);
    }
}

InputTick capture_input(const gameplay::PlayerState& ps, float cam_yaw, float cam_pitch) {
    InputTick in;
    in.move_axes = ps.move_axes;
    in.look = ps.pending_look;
    in.yaw = ps.yaw;
    in.cam_yaw = cam_yaw;
    in.cam_pitch = cam_pitch;
    in.jog = ps.jog;
    in.run = ps.run;
    in.debug_sprint = ps.debug_sprint;
    in.jump = ps.jump_pressed;
    in.crouch = ps.crouch_held;
    in.interact = ps.interact_pressed;
    in.weapon = ps.weapon_drawn;
    return in;
}

void apply_input(gameplay::PlayerState& ps, const InputTick& in) {
    ps.move_axes = in.move_axes;
    ps.pending_look = in.look;
    ps.yaw = in.yaw;
    ps.jog = in.jog;
    ps.run = in.run;
    ps.debug_sprint = in.debug_sprint;
    ps.jump_pressed = ps.jump_pressed || in.jump;
    ps.crouch_held = in.crouch;
    ps.interact_pressed = ps.interact_pressed || in.interact;
    ps.weapon_drawn = in.weapon;
}

std::string TrajectoryRecorder::stop_and_write(const std::string& path) {
    active_ = false;
    if (traj_.frames.empty()) {
        std::fprintf(stderr,
                     "[traj] recording had 0 frames -- NOT written (an empty "
                     "trajectory is a silent zero, not a file)\n");
        return {};
    }
    if (!write_trajectory(traj_, path)) {
        return {};
    }
    std::fprintf(stderr, "[traj] wrote %zu frames, %zu input ticks to %s\n", traj_.frames.size(),
                 traj_.inputs.size(), path.c_str());
    return path;
}

bool write_trajectory(const Trajectory& t, const std::string& path) {
    serialization::BinaryWriter w;
    w.begin_file(TRAJ_MAGIC, TRAJ_CONTAINER_VERSION);
    w.begin_section(TAG_INFO, 1);
    w.write_u32(static_cast<uint32_t>(t.frames.size()));
    w.write_u32(t.stand);
    w.write_u64(t.seed);
    w.end_section();
    w.begin_section(TAG_FRMS, 1);
    for (const TrajectoryFrame& f : t.frames) {
        write_frame(w, f);
    }
    w.end_section();
    if (!t.inputs.empty()) {
        w.begin_section(TAG_INPT, 1);
        for (const InputTick& in : t.inputs) {
            write_input(w, in);
        }
        w.end_section();
    }
    if (!w.ok() || !w.save_to_file(path)) {
        std::fprintf(stderr, "[traj] failed to write %s\n", path.c_str());
        return false;
    }
    return true;
}

std::optional<Trajectory> read_trajectory(const std::string& path) {
    serialization::BinaryReader r;
    if (!r.open_file(path, TRAJ_MAGIC)) {
        std::fprintf(stderr, "[traj] %s is not a trajectory (bad magic / unreadable)\n",
                     path.c_str());
        return std::nullopt;
    }
    Trajectory t;
    uint32_t declared = 0;
    while (auto s = r.next_section()) {
        switch (s->tag) {
        case TAG_INFO:
            declared = r.read_u32();
            t.stand = r.read_u32();
            t.seed = r.read_u64();
            break;
        case TAG_FRMS: {
            // Frame count from the section byte length, so a FRMS that arrives
            // before INFO (or without one) still reads: 8 + 6*4 = 32 bytes each.
            const uint64_t per = 8 + 6 * 4;
            const uint64_t n = s->byte_length / per;
            t.frames.reserve(static_cast<size_t>(n));
            for (uint64_t i = 0; i < n; ++i) {
                t.frames.push_back(read_frame(r));
            }
            break;
        }
        case TAG_INPT: {
            const uint64_t n = s->byte_length / INPT_RECORD_BYTES;
            t.inputs.reserve(static_cast<size_t>(n));
            for (uint64_t i = 0; i < n; ++i) {
                t.inputs.push_back(read_input(r));
            }
            break;
        }
        default:
            break; // unknown section: next_section() skips it
        }
    }
    if (!r.ok()) {
        std::fprintf(stderr, "[traj] %s is truncated or malformed\n", path.c_str());
        return std::nullopt;
    }
    if (t.frames.empty() && t.inputs.empty()) {
        std::fprintf(stderr, "[traj] %s has no frames\n", path.c_str());
        return std::nullopt;
    }
    if (declared != 0 && declared != t.frames.size()) {
        // Reported, not fatal: the frames are the payload and they read; a
        // mismatched count means a partial write, and saying so is cheaper than
        // guessing which number is right.
        std::fprintf(stderr,
                     "[traj] %s: INFO says %u frames, FRMS holds %zu -- using FRMS\n",
                     path.c_str(), declared, t.frames.size());
    }
    return t;
}

bool TrajectoryPlayer::load(const std::string& path) {
    traj_ = read_trajectory(path);
    index_ = 0;
    input_index_ = 0;
    return traj_.has_value();
}

const InputTick* TrajectoryPlayer::next_input() {
    if (!traj_ || input_index_ >= traj_->inputs.size()) {
        return nullptr;
    }
    return &traj_->inputs[input_index_++];
}

const TrajectoryFrame* TrajectoryPlayer::advance() {
    if (!traj_ || index_ >= traj_->frames.size()) {
        return nullptr;
    }
    return &traj_->frames[index_++];
}

} // namespace dfn::app
