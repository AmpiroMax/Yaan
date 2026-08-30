/*
Module: engine/app
File: engine/app/sources/AppStand.cpp

Responsibility:
- The stand's four camera poses and its clip queue, as data.

Dependencies:
- Uses: AppStand.h. Nothing else.
- Used by: dfn_app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- CONTENT, not constants (Rule 5/14): these timings and azimuths are a shot
  list. They belong beside the stand they photograph, not in NUMBERS.md, and
  the moment a second consumer needs one of them it stops being a shot list.
*/

#include "engine/app/sources/AppStand.h"

namespace dfn::app {
namespace {

/// THE AZIMUTH CONVENTION, measured and not guessed: `orbit_yaw` 0 puts the
/// camera IN FRONT of the figure, 180 directly BEHIND it. The first version of
/// this table had it the other way round and shot a "front" pose of a man's
/// back — visible only because the frame was looked at.
constexpr StandCamera CAMERAS[STAND_CAMERA_COUNT] = {
    // 1. FRONT. Reads proportions and where the arms hang; the pose a
    //    character sheet is drawn from.
    {0.0f, -6.0f, 3.4f, 0.75f, "front"},
    // 2. PROFILE. THE GAIT CAMERA: stride length, knee flexion and the trunk's
    //    lean are all fore-and-aft quantities and are invisible head-on.
    {90.0f, -4.0f, 3.6f, 0.80f, "profile"},
    // 3. THREE-QUARTER. The compromise the eye is used to: silhouette plus
    //    depth. It is what the wave's before/after pair is shot on, because a
    //    procedural gait and a bought clip differ in BOTH.
    {45.0f, -8.0f, 3.4f, 0.80f, "three-quarter"},
    // 4. CLOSE. Head and shoulders: the camera that shows the skinning seam at
    //    the neck and what the clothing palette actually looks like.
    {25.0f, 6.0f, 2.4f, 0.90f, "close"},
};

/// One phase of the queue: when it starts, what the hands are doing.
struct Phase {
    float from_s;
    StandStep step;
};

/// THE QUEUE THE OWNER ORDERED (31.08): Idle, Walk, Jog, Sprint, Jump,
/// Crouch, Sit. Every travelling gear runs OUT AND BACK in equal halves — the
/// turn is a yaw of pi, written directly (see StandStep::face_yaw) — so the
/// figure ends the queue where it began, beside the bench it is then told to
/// sit on. Three seconds out is roughly three walk cycles, two jog cycles and
/// four sprint cycles: the least that shows a cycle REPEATING rather than a
/// pose held.
///
/// THE TURNS ARE SNAPS, and the acceptance frames are taken inside the phases
/// rather than at their boundaries. A turn eased over half a second would be
/// prettier and would make the queue's timing depend on the easing, which is
/// the one thing a fixed shot list may not have.
constexpr float PI = 3.14159265358979f;

constexpr Phase PHASES[] = {
    {0.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.0f, "idle"}},
    {4.0f, {{0.0f, 1.0f}, false, false, false, false, false, 0.0f, "walk-out"}},
    {7.0f, {{0.0f, 1.0f}, false, false, false, false, false, PI, "walk-back"}},
    {10.0f, {{0.0f, 1.0f}, true, false, false, false, false, 0.0f, "jog-out"}},
    {13.0f, {{0.0f, 1.0f}, true, false, false, false, false, PI, "jog-back"}},
    {16.0f, {{0.0f, 1.0f}, false, true, false, false, false, 0.0f, "sprint-out"}},
    {18.0f, {{0.0f, 1.0f}, false, true, false, false, false, PI, "sprint-back"}},
    {20.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.0f, "settle"}},
    // The jump is an EDGE and the phase after it is the arc: the queue presses
    // once and then holds still, so the frames show take-off, flight and
    // landing rather than a man bouncing.
    {20.6f, {{0.0f, 0.5f}, false, false, true, false, false, 0.0f, "jump"}},
    {21.0f, {{0.0f, 0.5f}, false, false, false, false, false, 0.0f, "jump-arc"}},
    {23.0f, {{0.0f, 0.0f}, false, false, false, true, false, 0.0f, "crouch"}},
    {25.0f, {{0.0f, 0.7f}, false, false, false, true, false, 0.0f, "crouch-out"}},
    {27.0f, {{0.0f, 0.7f}, false, false, false, true, false, PI, "crouch-back"}},
    // FACING THE BENCH is what makes the sit possible at all: the seat is
    // taken by AIM, not by proximity (AppSeats::seat_aim), so a figure with
    // its back to the bench presses E at the grass.
    {29.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.5f * PI,
             "face-bench"}},
    {29.6f, {{0.0f, 0.0f}, false, false, false, false, true, 0.5f * PI,
             "sit-down"}},
    {30.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.5f * PI,
             "sitting"}},
};

} // namespace

StandCamera stand_camera(uint32_t n) {
    if (n < 1 || n > STAND_CAMERA_COUNT) {
        return CAMERAS[1]; // profile: the pose a gait is read from
    }
    return CAMERAS[n - 1];
}

StandStep stand_sequence_at(float prev_t, float t) {
    StandStep out;
    for (const Phase& p : PHASES) {
        if (t >= p.from_s) {
            out = p.step;
        }
    }
    // EDGES ARE EDGES. `jump` and `interact` are latches the movement code
    // consumes once; held down, the first would re-jump on every landing and
    // the second would sit and stand on alternate ticks. True only on the tick
    // that CROSSED the phase boundary.
    if (out.jump || out.interact) {
        bool crossed = false;
        for (const Phase& p : PHASES) {
            if ((p.step.jump || p.step.interact) && prev_t < p.from_s
                && t >= p.from_s) {
                crossed = true;
            }
        }
        if (!crossed) {
            out.jump = false;
            out.interact = false;
        }
    }
    return out;
}

} // namespace dfn::app
