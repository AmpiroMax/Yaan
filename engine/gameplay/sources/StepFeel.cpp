/*
Module: engine/gameplay
File: engine/gameplay/sources/StepFeel.cpp

Responsibility:
- Implementation of the pure step-feel math (see StepFeel.h).

Key items:
- step_length / advance_stride / bob_* / punctuation_curve / settle_offset /
  fov_scale_target.

Dependencies:
- Uses: StepFeel.h, generated constants, <cmath>, <algorithm>.
- Used by: PlayerMovement.cpp, ViewModel.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every tuning number is a NUMBERS row (Rule 14) — no literals beyond curve
  shape fractions documented in the header contract.
*/

#include "engine/gameplay/sources/StepFeel.h"

#include <algorithm>
#include <cmath>

#include "engine/core/config/sources/Constants.h"

namespace dfn::gameplay {

namespace {
constexpr float TAU = 6.28318530717958647692f;
constexpr float PI = 3.14159265358979323846f;
constexpr float LEN_BASE = static_cast<float>(config::STEP_LENGTH_BASE);
constexpr float LEN_PER_MPS = static_cast<float>(config::STEP_LENGTH_PER_MPS);
constexpr float PHASE_LEFT = static_cast<float>(config::FOOTFALL_PHASE_LEFT);
constexpr float PHASE_RIGHT = static_cast<float>(config::FOOTFALL_PHASE_RIGHT);
constexpr float AMP_AT_WALK = static_cast<float>(config::HEADBOB_AMPLITUDE_AT_WALK);
constexpr float AMP_MAX = static_cast<float>(config::HEADBOB_AMPLITUDE_MAX);
constexpr float WALK = static_cast<float>(config::WALK_SPEED);
constexpr float RUN = static_cast<float>(config::RUN_SPEED);
constexpr float FOV_MAX = static_cast<float>(config::FOV_SPEED_SCALE_MAX);

// Was `p` crossed while moving from `from` (exclusive) to `to` (inclusive),
// where the interval may wrap past 1?
[[nodiscard]] bool crossed(float from, float to, float p) {
    if (to >= from) {
        return from < p && p <= to;
    }
    return p > from || p <= to; // wrapped
}
} // namespace

float step_length(float speed) {
    return LEN_BASE + LEN_PER_MPS * std::max(0.0f, speed);
}

StrideAdvance advance_stride(float phase, float speed, float dt) {
    StrideAdvance out;
    if (speed <= 0.0f || dt <= 0.0f) {
        out.new_phase = phase;
        return out; // the phase HOLDS: no marching in place
    }
    const float cycle = 2.0f * step_length(speed); // a cycle is two steps
    float advanced = phase + (speed * dt) / cycle;
    const float new_phase = advanced - std::floor(advanced);

    const bool left = crossed(phase, new_phase, PHASE_LEFT);
    const bool right = crossed(phase, new_phase, PHASE_RIGHT);
    out.new_phase = new_phase;
    out.footfalls = (left ? 1 : 0) + (right ? 1 : 0);
    if (left && right) {
        // Which came first along the direction of travel from `phase`?
        const float d_left = PHASE_LEFT - phase + (PHASE_LEFT > phase ? 0.0f : 1.0f);
        const float d_right = PHASE_RIGHT - phase + (PHASE_RIGHT > phase ? 0.0f : 1.0f);
        out.first_is_left = d_left < d_right;
    } else {
        out.first_is_left = left;
    }
    return out;
}

float bob_amplitude_target(float speed) {
    if (speed <= 0.0f) {
        return 0.0f; // stationary = flat camera; the old floating is the reject
    }
    return std::min(AMP_MAX, AMP_AT_WALK * (speed / WALK));
}

float bob_vertical(float phase, float amplitude) {
    // Two minima per cycle, exactly at the footfall phases; <= 0 everywhere.
    return -amplitude * 0.5f * (1.0f - std::cos(2.0f * TAU * phase));
}

float bob_lateral(float phase, float amplitude) {
    // Half frequency: one lateral period per full cycle, peaking toward the
    // planted foot at the plant phases. `amplitude` is the LATERAL amplitude
    // itself — the camera passes HEADBOB_LATERAL_FACTOR * vertical amplitude,
    // the view-model sway passes its own row, both share this waveform.
    return amplitude * std::sin(TAU * phase);
}

float punctuation_curve(float u) {
    u = std::clamp(u, 0.0f, 1.0f);
    constexpr float DOWN = 1.0f / 3.0f; // fast down over the first third
    if (u < DOWN) {
        const float t = u / DOWN;
        return t * t * (3.0f - 2.0f * t); // smoothstep 0 -> 1
    }
    const float t = (u - DOWN) / (1.0f - DOWN);
    return 0.5f * (1.0f + std::cos(PI * t)); // slow ease 1 -> 0
}

float settle_offset(float u, float start_offset, float depth) {
    u = std::clamp(u, 0.0f, 1.0f);
    constexpr float DOWN = 1.0f / 3.0f;
    if (u < DOWN) {
        const float t = u / DOWN;
        const float w = t * t * (3.0f - 2.0f * t);
        return start_offset + (-depth - start_offset) * w; // live offset -> -depth
    }
    const float t = (u - DOWN) / (1.0f - DOWN);
    return -depth * 0.5f * (1.0f + std::cos(PI * t)); // -depth -> 0
}

float fov_scale_target(float speed) {
    const float t = std::clamp((speed - WALK) / (RUN - WALK), 0.0f, 1.0f);
    return 1.0f + (FOV_MAX - 1.0f) * t;
}

} // namespace dfn::gameplay
